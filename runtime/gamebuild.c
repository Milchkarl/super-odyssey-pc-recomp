// Builds the game code library on the player's machine (runtime/gamebuild.h).
#include "gamebuild.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gamelib.h"
#include "gamelib_cpu.h"
#include "paths.h"
#include "platform.h"
#include "rt.h"

// From the Makefile: the flags the game code is compiled and linked with (as `make` builds it in a source checkout)
#ifndef GAMELIB_CFLAGS
#define GAMELIB_CFLAGS "-O1 -fno-strict-aliasing -fvisibility=hidden"
#endif
#ifndef GAMELIB_SHARED
#define GAMELIB_SHARED "-shared"
#endif

#ifdef _WIN32
#define SEP "\\"
#else
#define SEP "/"
#endif

// ------------------------------------------------------------------ child processes
// Every one running, so that quitting mid-build ends them too (atexit) instead of leaving compilers behind.
#define MAX_PROCS 80
static pthread_mutex_t g_plock = PTHREAD_MUTEX_INITIALIZER;
static PlatProc g_procs[MAX_PROCS];
static int g_nprocs;
static bool g_quitting;

static void end_all(void) {
    pthread_mutex_lock(&g_plock);
    g_quitting = true;
    for (int i = 0; i < g_nprocs; i++) plat_proc_kill(g_procs[i]);
    g_nprocs = 0;
    pthread_mutex_unlock(&g_plock);
}

// generator: Windows starts it with Control Flow Guard off (platform/windows.c plat_spawn_no_cfg)
static PlatProc spawn_proc(char **argv, const char *log, bool generator) {
    pthread_mutex_lock(&g_plock);
    PlatProc p = 0;
    if (!g_quitting) {
#ifdef _WIN32
        if (generator) {
            unsigned long error = 0;
            p = plat_spawn_no_cfg((const char *const *)argv, log, false, &error);
            static bool told;
            if (p && !told) {
                told = true;
                if (!error) LOG("[gamebuild] generator started with Control Flow Guard off\n");
                else LOG("[gamebuild] generator started normally: Control Flow Guard could not be turned off for it (error %lu)\n", error);
            }
        } else
#endif
        p = plat_spawn((const char *const *)argv, log, false);
    }
    (void)generator;
    if (p && g_nprocs < MAX_PROCS) g_procs[g_nprocs++] = p;
    pthread_mutex_unlock(&g_plock);
    return p;
}
static PlatProc spawn(char **argv, const char *log) { return spawn_proc(argv, log, false); }

// plat_proc_wait without blocking, then forgotten once it has ended
static int finished(PlatProc p) {
    pthread_mutex_lock(&g_plock);
    int r = plat_proc_wait(p, false);
    if (r >= 0)
        for (int i = 0; i < g_nprocs; i++)
            if (g_procs[i] == p) { g_procs[i] = g_procs[--g_nprocs]; break; }
    pthread_mutex_unlock(&g_plock);
    return r;
}

static void stop(PlatProc p) {  // ends one that is still running, then forgotten
    pthread_mutex_lock(&g_plock);
    bool ours = false;
    for (int i = 0; i < g_nprocs; i++)
        if (g_procs[i] == p) { g_procs[i] = g_procs[--g_nprocs]; ours = true; break; }
    pthread_mutex_unlock(&g_plock);
    if (ours) plat_proc_kill(p);
}

static void sleep_ms(int ms) { plat_sleep_until_ns(plat_now_ns() + (uint64_t)ms * 1000000ull); }

static int wait_for(PlatProc p) {
    int r;
    while ((r = finished(p)) < 0) sleep_ms(20);
    return r;
}

// ------------------------------------------------------------------ argument lists
typedef struct { char **v; int n, cap; } Args;
static void add(Args *a, const char *s) {
    if (a->n + 2 > a->cap) {
        a->cap = a->cap ? a->cap * 2 : 32;
        a->v = (char **)realloc(a->v, sizeof *a->v * a->cap);
    }
    a->v[a->n++] = strdup(s);
    a->v[a->n] = NULL;
}
static void add_split(Args *a, const char *flags) {  // (space-separated, no quoting)
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", flags);
    char *save = NULL;
    for (char *t = strtok_r(buf, " ", &save); t; t = strtok_r(NULL, " ", &save)) add(a, t);
}
static void args_free(Args *a) {
    for (int i = 0; i < a->n; i++) free(a->v[i]);
    free(a->v);
    memset(a, 0, sizeof *a);
}

// ------------------------------------------------------------------ the tools
typedef struct {
    char gen[1100], gen_script[1100];  // generator: odysseygen, or (source checkout) the venv's python + tools/gen.py
    char cc[1100];                     // compiler: zig (as `zig cc`), or ODYSSEY_CC (a source checkout: MSYS2's clang)
    bool zig;
    char inc[1100], entry[1100], hle[1100];  // headers the game code includes, tools/gamelib_entry.c, HLE names
    char hot[1100];  // runtime/hle/hot_functions.txt (the game functions that run: below), "" = none
#ifdef _WIN32
    char implib[1100];  // the .exe's import library (the runtime functions the game code calls)
    char zig_rt[1100];  // the release's zig: its runtime built ahead of time (zig/rt, tools/zig_slim.sh), "" = none
#endif
} Tools;

static bool file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static bool find_tools(Tools *t) {
    memset(t, 0, sizeof *t);
    // The generator: Windows releases ship Python's own signed embeddable interpreter with tools/gen.py and its packages
    // (odysseygen/python.exe; tools/pyembed.py) - the PyInstaller-built odysseygen.exe was taken for a trojan by
    // Windows Defender's machine-learning check (Trojan:Win32/Bearfoos.B!ml, 2026-11-02) and removed; Linux and macOS
    // keep that one.
    const char *e = getenv("ODYSSEY_GEN");
    if (e && *e) snprintf(t->gen, sizeof t->gen, "%s", e);
    else if (!(rt_resource("odysseygen" SEP "python" PLAT_EXE_SUFFIX, NULL, t->gen, sizeof t->gen) &&
               rt_resource("odysseygen" SEP "tools" SEP "gen.py", NULL, t->gen_script, sizeof t->gen_script)) &&
             !rt_resource("odysseygen" SEP "odysseygen" PLAT_EXE_SUFFIX, NULL, t->gen, sizeof t->gen) &&
             !(rt_resource(NULL, "tools/" PLAT_VENV_PYTHON, t->gen, sizeof t->gen) &&
               rt_resource(NULL, "tools/gen.py", t->gen_script, sizeof t->gen_script)))
        return false;
    e = getenv("ODYSSEY_CC");
    if (e && *e) snprintf(t->cc, sizeof t->cc, "%s", e);
    else if (rt_resource("zig" SEP "zig" PLAT_EXE_SUFFIX, NULL, t->cc, sizeof t->cc)) t->zig = true;
#ifndef _WIN32
    else if (rt_dev_mode() && file_exists("/usr/bin/clang")) snprintf(t->cc, sizeof t->cc, "/usr/bin/clang");
#endif
    else return false;
    if (!rt_resource("gamelib-src" SEP "cpu.h", "runtime/cpu.h", t->inc, sizeof t->inc) ||
        !rt_resource("gamelib-src" SEP "gamelib_entry.c", "tools/gamelib_entry.c", t->entry, sizeof t->entry) ||
        !rt_resource("gamelib-src" SEP "hle_symbols.txt", "runtime/hle/hle_symbols.txt", t->hle, sizeof t->hle))
        return false;
    if (!rt_resource("gamelib-src" SEP "hot_functions.txt", "runtime/hle/hot_functions.txt", t->hot, sizeof t->hot)) t->hot[0] = 0;
    char *s = strrchr(t->inc, '/'), *b = strrchr(t->inc, '\\');
    if (b > s) s = b;
    if (s) *s = 0;
#ifdef _WIN32
    if (!rt_resource("SuperOdysseyRecomp.dll.a", "build/SuperOdysseyRecomp.dll.a", t->implib, sizeof t->implib)) return false;
    t->zig_rt[0] = 0;
    if (t->zig && rt_resource("zig" SEP "rt" SEP "link.rsp", NULL, t->zig_rt, sizeof t->zig_rt)) {
        char *s = strrchr(t->zig_rt, '/'), *b = strrchr(t->zig_rt, '\\');
        if (b > s) s = b;
        if (s) *s = 0;
        else t->zig_rt[0] = 0;
    }
#endif
    return true;
}

bool gamebuild_available(void) {
    Tools t;
    return find_tools(&t);
}

// zig cc on Linux: an explicit target, so it uses its own glibc headers and stubs instead of looking for the system's
// (SteamOS and other systems without development packages have none: "failed to find libc installation")
#if defined(__linux__) && defined(__x86_64__)
#define ZIG_TARGET "x86_64-linux-gnu"
#elif defined(__linux__) && defined(__aarch64__)
#define ZIG_TARGET "aarch64-linux-gnu"
#else
#define ZIG_TARGET ""
#endif

static void compiler(Args *a, const Tools *t) {
    add(a, t->cc);
    if (t->zig) add(a, "cc");
    if (t->zig && ZIG_TARGET[0]) add(a, "-target"), add(a, ZIG_TARGET);
}

// ------------------------------------------------------------------ the modules (as `make gen`)
// main is the game, sdk the system libraries (both with the HLE replacements: runtime/hle/hle_symbols.txt), subsdk0
// the video library (optional). Generated in this order; the library's tables list them as GEN_MODS does (main
// subsdk0 sdk). share: of the generator's work (functions: 74,823 / 27,352 / 13,379).
typedef struct {
    const char *name, *base;
    bool hle;
    double share;
} GenModule;
static const GenModule MODULES[] = {
    {"main", "0x7000000000", true, 0.65},
    {"sdk", "0x7004000000", true, 0.24},
    {"subsdk0", "0x7003000000", false, 0.11},
};
enum { NMOD = sizeof MODULES / sizeof MODULES[0] };

// ------------------------------------------------------------------ generator progress (its log)
typedef struct {
    const GenModule *m;
    char log[1100], out[1100];
    PlatProc p;
    int exit;                // -1 while running
    int files;               // chunk files written so far (complete)
    unsigned long long ci, n;  // functions written / all
} Gen;

static void read_gen_log(Gen *g) {
    FILE *f = fopen(g->log, "rb");
    if (!f) return;
    static char buf[1 << 16];
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    long from = size > (long)sizeof buf - 1 ? size - (long)sizeof buf + 1 : 0;
    fseek(f, from, SEEK_SET);
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = 0;
    const char *last = NULL;
    for (const char *p = strstr(buf, ": progress "); p; p = strstr(p + 1, ": progress ")) last = p;
    unsigned long long ci, all;
    int files;
    if (last && sscanf(last, ": progress %llu/%llu files %d", &ci, &all, &files) == 3) {
        g->ci = ci, g->n = all;
        if (files > g->files) g->files = files;
    }
}

static void log_tail(const char *path, char *out, size_t n) {  // the last line of a log (an error message)
    out[0] = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char buf[4096];
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, size > (long)sizeof buf - 1 ? size - (long)sizeof buf + 1 : 0, SEEK_SET);
    size_t k = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[k] = 0;
    while (k && (buf[k - 1] == '\n' || buf[k - 1] == '\r')) buf[--k] = 0;
    char *nl = strrchr(buf, '\n');
    snprintf(out, n, "%s", nl ? nl + 1 : buf);
}

// ------------------------------------------------------------------ compiling
// The game functions that run (runtime/hle/hot_functions.txt, tools/hot_functions.py: every function entered while
// playing through every kingdom with the function profiler, per module and game version - addresses only, no game
// code). A generated file holding none of them is compiled without optimization: ~0.4 s instead of up to 100 s, the
// code there runs several times slower but isn't run in play. Most of the system libraries and nearly all the video
// library (our videos play through FFmpeg) are such code: a new player's build -46% (PC, 8 jobs; 2026-11-24). A
// module of a game version the list doesn't name is built optimized throughout, as before.
// OFF by default since 2026-11-27 (ODYSSEY_GAME_COLD_FILES=1 turns it on, tests): the list misses code that runs while
// moving through a kingdom, and with 8 logical processors the same 90 s run through Sand (1.4.1, 60 FPS) had 265 and 82
// frames over 25 ms (up to 709 ms; the worst tenth of the seconds at 18 / 33 FPS) against 8 with every file optimized -
// on 32 none either way. Players saw it as stutter while moving, new in 1.0.2 (#177). The list is part of GAMELIB_ID
// (Makefile): its header changed with this, so every 1.0.2 build is made again.
typedef struct { uint64_t *a; size_t n; bool known; } HotSet;
static HotSet g_hot[3];  // (MODULES order)

static int cmp_u64(const void *x, const void *y) {
    uint64_t a = *(const uint64_t *)x, b = *(const uint64_t *)y;
    return a < b ? -1 : a > b;
}
static bool hot_has(const HotSet *h, uint64_t a) { return h->n && bsearch(&a, h->a, h->n, sizeof a, cmp_u64); }

// The module's build id as the generator names it (tools/nso.py: header bytes 0x40.., the first 8 as hex)
static bool nso_build_id(const char *nso, char out[17]) {
    FILE *f = fopen(nso, "rb");
    unsigned char b[8];
    bool ok = f && fseek(f, 0x40, SEEK_SET) == 0 && fread(b, 1, 8, f) == 8;
    if (f) fclose(f);
    if (ok) for (int i = 0; i < 8; i++) snprintf(out + 2 * i, 3, "%02x", b[i]);
    return ok;
}

// "# <module> <build id>" starts a module's section; each line after it a function's address in hex
static void hot_load(HotSet *h, const char *list, const char *module, const char *build) {
    free(h->a);
    memset(h, 0, sizeof *h);
    const char *cold = getenv("ODYSSEY_GAME_COLD_FILES");
    if (!list[0] || !cold || !atoi(cold) || getenv("ODYSSEY_GAME_ALL_OPT")) return;
    FILE *f = fopen(list, "r");
    if (!f) return;
    char line[256], want[128];
    snprintf(want, sizeof want, "# %s %s", module, build);
    bool in = false;
    size_t cap = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (line[0] == '#') { in = !strcmp(line, want); h->known |= in; continue; }
        if (!in || !line[0]) continue;
        if (h->n == cap) { cap = cap ? cap * 2 : 4096; h->a = (uint64_t *)realloc(h->a, cap * sizeof *h->a); }
        h->a[h->n++] = strtoull(line, NULL, 16);
    }
    fclose(f);
    if (h->n) qsort(h->a, h->n, sizeof *h->a, cmp_u64);
}

// Whether a generated file defines none of the functions that run ("void f_<addr>(Cpu *c) {", gen.py's layout)
static bool file_cold(const char *path, const HotSet *h) {
    if (!h->known) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    char line[4096];
    bool hot = false;
    while (!hot && fgets(line, sizeof line, f)) {
        const char *p = strstr(line, " f_");
        if (!p || line[0] == ' ' || line[0] == '\t' || !strstr(line, ") {")) continue;
        hot = hot_has(h, strtoull(p + 3, NULL, 16));
    }
    fclose(f);
    return !hot;
}

typedef struct {
    char src[1100], obj[1100], log[1100];
    unsigned long long bytes;
    bool generated;  // (deleted once compiled; the entry file is the shipped one)
    bool started;
    bool cold;       // (defines no function that runs: compiled without optimization)
    bool hit;        // (its object reused from the last build: obj is in .gamecache)
    char key[40];    // (what its object is made from, hashed: the cache's name for it; "" = not cached)
    int longest;     // its longest function, in lines (generated files; 0 otherwise)
    uint64_t t_start;  // (ODYSSEY_LOG_GAMEBUILD: when its compile started)
} Unit;

typedef struct {
    Unit *u;
    int n, cap;
} Units;

static int longest_function(const char *path);
static void add_unit(Units *us, const char *src, const char *work, const char *stem, bool generated) {
    if (us->n == us->cap) {
        us->cap = us->cap ? us->cap * 2 : 64;
        us->u = (Unit *)realloc(us->u, sizeof *us->u * us->cap);
    }
    Unit *u = &us->u[us->n++];
    snprintf(u->src, sizeof u->src, "%s", src);
    snprintf(u->obj, sizeof u->obj, "%s" SEP "%s.o", work, stem);
    snprintf(u->log, sizeof u->log, "%s" SEP "%s.log", work, stem);
    u->generated = generated;
    u->started = false;
    struct stat st;
    u->bytes = stat(src, &st) == 0 ? (unsigned long long)st.st_size : 0;
    u->longest = generated ? longest_function(src) : 0;
    u->cold = u->hit = false;
    u->key[0] = 0;
    u->t_start = 0;
    for (int i = 0; generated && i < 3; i++) {
        size_t l = strlen(MODULES[i].name);
        if (!strncmp(stem, MODULES[i].name, l) && stem[l] == '_' && stem[l + 1] >= '0' && stem[l + 1] <= '9')
            u->cold = file_cold(src, &g_hot[i]);  // (not the tables: modname_table)
    }
    // ODYSSEY_GAME_COLD=<stem>[,<stem>...] (tests): those files without optimization too (e.g. main_092)
    const char *force = getenv("ODYSSEY_GAME_COLD");
    if (generated && force && *force) {
        size_t l = strlen(stem);
        for (const char *p = strstr(force, stem); p; p = strstr(p + 1, stem))
            if ((p == force || p[-1] == ',') && (p[l] == 0 || p[l] == ',')) u->cold = true;
    }
}

// The flags the game code was built with (GAMELIB_CFLAGS, part of its id). zig cc reads -march=<cpu> with its own CPU
// names (x86_64_v3) and takes "-<feature>" off the end, so x86-64-v3 would be CPU "x86": the same CPU as -mcpu there.
static void add_cflags(Args *a, const Tools *t, bool cold) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", GAMELIB_CFLAGS);
    char *save = NULL;
    const char *march = gamelib_cpu_march();  // (an older CPU than the build's target: what it has - gamelib.c)
    char own[128];
    for (char *f = strtok_r(buf, " ", &save); f; f = strtok_r(NULL, " ", &save)) {
        if (march && !strncmp(f, "-march=", 7)) snprintf(own, sizeof own, "-march=%s", march), f = own;
        if (!strcmp(f, "-O1") && getenv("ODYSSEY_GAME_OPT") && (!strcmp(getenv("ODYSSEY_GAME_OPT"), "2") || !strcmp(getenv("ODYSSEY_GAME_OPT"), "3")))
            f = !strcmp(getenv("ODYSSEY_GAME_OPT"), "2") ? "-O2" : "-O3";
        if (t->zig && !strncmp(f, "-march=", 7)) {
            char cpu[128];
            snprintf(cpu, sizeof cpu, "-mcpu=%s", f + 7);
            for (char *c = cpu + 6; *c; c++) if (*c == '-') *c = '_';
            add(a, cpu);
        } else {
            add(a, f);
        }
    }
    // No debug info: zig cc emits it unless told not to, and for the few huge lifted functions (sdk_053, main_160:
    // 7,000-10,000 lines each) it made the build crawl - on a Steam Deck one file took 57+ minutes with it and 6.7
    // without. Crash reports still name functions (the symbol table). zig cc ignores -g0 (it adds -gdwarf-4 after it,
    // and the last one wins); -s, its "strip", is what leaves the debug info out.
    if (t->zig) add(a, "-s");
    // The optimization level the flags ask for: zig cc turns -O1 into clang's -O2 (its own "release" level). -O2 adds
    // loop-load elimination, which with LICM took most of the time on the biggest files (clang -ftime-report on a
    // Steam Deck); -O1 is what the project's own build (make, clang -O1) uses and was tested with.
    // (ODYSSEY_GAME_OPT=2 / 3, tests: the game code at that level instead - a run-time comparison)
    static const char *opt;
    if (!opt) { const char *e = getenv("ODYSSEY_GAME_OPT"); opt = e && (!strcmp(e, "2") || !strcmp(e, "3")) ? (!strcmp(e, "2") ? "-O2" : "-O3") : "-O1"; }
    if (t->zig) add(a, "-Xclang"), add(a, cold ? "-O0" : opt);
}

// The longest function in a generated C file, in lines: a function starts at a line that begins with a name and ends
// in ") {", ends at a "}" alone at the start of a line (gen.py's layout). 0 if unreadable.
static int longest_function(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char line[4096];
    int n = 0, start = -1, best = 0;
    while (fgets(line, sizeof line, f)) {
        n++;
        size_t l = strcspn(line, "\r\n");
        if (l >= 3 && ((line[0] >= 'a' && line[0] <= 'z') || (line[0] >= 'A' && line[0] <= 'Z') || line[0] == '_') &&
            !strncmp(line + l - 3, ") {", 3))
            start = n;
        else if (line[0] == '}' && start >= 0) {
            if (n - start > best) best = n - start;
            start = -1;
        }
    }
    fclose(f);
    return best;
}

static char g_modules_def[200];  // -DGAMELIB_MODULES=X(main) X(subsdk0) X(sdk): the modules this build has

static void compile_args(const Tools *t, const Unit *u, bool entry, Args *out);
static PlatProc start_compile(const Tools *t, const Unit *u, bool entry) {
    Args a = {0};
    compile_args(t, u, entry, &a);
    PlatProc p = spawn(a.v, u->log);
    args_free(&a);
    return p;
}

// ------------------------------------------------------------------ compiled files kept between builds
// An update that changes the game code (its generator, the patches) leaves almost every generated file as it was:
// 0.9.0 -> 1.0.1, 506 of the 509 objects came out byte for byte the same, and the whole build ran again (~2.5 min on a
// fast PC, ~16 on a Steam Deck). The objects of the last build are kept in <game files>/.gamecache, each named by a
// hash of what it is made from - its C source, the compile command (the build's id and the paths left out: generated
// files never use the id, and the objects didn't change with it), the headers it includes and the compiler's version -
// and a build takes an object from there instead of compiling a file whose hash it has. After a build that worked,
// .gamecache holds exactly that build's objects (~150 MB). ODYSSEY_GAME_NO_CACHE=1: everything compiled (tests).
static char g_cache[1100], g_cache_new[1100];
static uint64_t g_cache_salt[2];
static bool g_cache_on;

static void hash2(uint64_t h[2], const void *p, size_t n) {  // (two FNV-1a 64 lanes with different seeds: 128 bits)
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; i++) {
        h[0] = (h[0] ^ b[i]) * 0x100000001b3ull;
        h[1] = (h[1] ^ b[i]) * 0x100000001b3ull + 0x9e37;
    }
}
static bool hash2_file(uint64_t h[2], const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    static char buf[1 << 16];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) hash2(h, buf, n);
    fclose(f);
    return true;
}
// what every object depends on besides its own file and command: the compiler (its version) and the headers
static void cache_init(const Tools *t, const char *game_dir, const char *work) {
    g_cache_on = t->zig && !getenv("ODYSSEY_GAME_NO_CACHE");
    snprintf(g_cache, sizeof g_cache, "%s" SEP ".gamecache", game_dir);
    snprintf(g_cache_new, sizeof g_cache_new, "%s" SEP ".gamecache.new", game_dir);
    rt_remove_game_file(game_dir, ".gamecache.new");  // (a build that stopped half-way)
    g_cache_salt[0] = 0xcbf29ce484222325ull, g_cache_salt[1] = 0x84222325cbf29ce4ull;
    char p[1200];
    snprintf(p, sizeof p, "%s" SEP "zig-version.log", work);
    if (!hash2_file(g_cache_salt, p)) g_cache_on = false;
    static const char *const hdr[] = {"cpu.h", "simd.h", "simd_fp.h", "gamelib.h"};
    for (int i = 0; i < 4; i++) {
        snprintf(p, sizeof p, "%s" SEP "%s", t->inc, hdr[i]);
        if (!hash2_file(g_cache_salt, p)) g_cache_on = false;
    }
    hash2(g_cache_salt, ZIG_TARGET GAMELIB_CFLAGS, sizeof(ZIG_TARGET GAMELIB_CFLAGS));
}
static bool unit_key(const Tools *t, Unit *u) {
    u->key[0] = 0;
    if (!g_cache_on || !u->generated) return false;
    uint64_t h[2] = {g_cache_salt[0], g_cache_salt[1]};
    Args a = {0};
    compile_args(t, u, false, &a);
    for (int i = 1; i < a.n; i++) {  // (not the compiler's path: its version is in the salt)
        const char *s = a.v[i];
        if (!strncmp(s, "-I", 2) || !strncmp(s, "-DGAMELIB_ID=", 13) || !strcmp(s, u->src) || !strcmp(s, u->obj)) continue;
        hash2(h, s, strlen(s) + 1);
    }
    args_free(&a);
    if (!hash2_file(h, u->src)) return false;
    snprintf(u->key, sizeof u->key, "%016llx%016llx", (unsigned long long)h[0], (unsigned long long)h[1]);
    if (getenv("ODYSSEY_LOG_GAMEBUILD")) {  // (what went into the key: the cache's misses explained)
        Args b = {0};
        compile_args(t, u, false, &b);
        char cmd[2048] = "";
        size_t o = 0;
        for (int i = 1; i < b.n && o < sizeof cmd - 200; i++)
            o += snprintf(cmd + o, sizeof cmd - o, " %s", b.v[i]);
        args_free(&b);
        LOG("[gamebuild] key %s %s (salt %016llx):%s\n", u->key, u->src, (unsigned long long)g_cache_salt[0], cmd);
    }
    return true;
}
// After a build that worked: its objects become the cache (moved, not copied), the last build's go
static void cache_keep(Units *us, const char *game_dir) {
    if (!g_cache_on) return;
    mkdir(g_cache_new, 0755);
    int kept = 0;
    for (int k = 0; k < us->n; k++) {
        Unit *u = &us->u[k];
        if (!u->key[0]) continue;
        char to[1200];
        snprintf(to, sizeof to, "%s" SEP "%s.o", g_cache_new, u->key);
        remove(to);  // (Windows' rename doesn't replace: two files with the same source and command share one)
        if (rename(u->obj, to) == 0) kept++;
    }
    rt_remove_game_file(game_dir, ".gamecache");
    if (rename(g_cache_new, g_cache) != 0) rt_remove_game_file(game_dir, ".gamecache.new");
    LOG("[gamebuild] %d compiled files kept for the next build (%s)\n", kept, g_cache);
}

static void compile_args(const Tools *t, const Unit *u, bool entry, Args *out) {
    Args a = {0};
    compiler(&a, t);
    add_cflags(&a, t, u->cold);
    // A file holding a giant function (over 3,000 lines: about 60 of the ~500, mostly system codecs and the video
    // decoder) without LICM's promotion: on those it alone took many minutes per file (sdk_053: 13 min, 104 s
    // without it, on a Steam Deck). The rest keep it - it keeps guest registers in host ones in loops.
    // Two more for those files (2026-11-18, clang -ftime-report on the slowest, subsdk0_139 - one 15,000-line function of
    // the video decoder: 57 s alone on a Ryzen 9950X3D, 27 s of it in the register coalescer and 21 s in the SLP
    // vectorizer): global copies are not joined (the object came out 2.6% smaller, not larger) and no SLP
    // vectorization (the same object with and without). 57 s -> 14 s for that file, 58 -> 24 and 19 -> 8 for the next
    // two of the decoder; the whole build waited for these while every other file was done.
    // ODYSSEY_GAME_SLOW_FLAGS=1: without the two (tests).
    static int slow_flags = -1;
    if (slow_flags < 0) slow_flags = getenv("ODYSSEY_GAME_SLOW_FLAGS") != NULL;
    if (t->zig && u->generated && u->longest > 3000 && !u->cold) {
        add(&a, "-mllvm"), add(&a, "-disable-licm-promotion");
        if (!slow_flags) add(&a, "-mllvm"), add(&a, "-join-globalcopies=false"), add(&a, "-fno-slp-vectorize");
    }
    char inc[1200];
    snprintf(inc, sizeof inc, "-I%s", t->inc);
    add(&a, inc);
    char idd[160];
    snprintf(idd, sizeof idd, "-DGAMELIB_ID=\"%s\"", gamelib_id());  // (with the CPU level below the build's target)
    add(&a, idd);
    if (entry) add(&a, g_modules_def);
    add(&a, "-c");
    add(&a, u->src);
    add(&a, "-o");
    add(&a, u->obj);
    *out = a;
}

// ------------------------------------------------------------------ the build
bool gamebuild_run(const char *game_dir, const GameBuildOpts *o, char *why, size_t why_size) {
    uint64_t t0 = plat_now_ns();
    static bool registered;
    if (!registered) registered = true, atexit(end_all);
    Tools t;
    if (!find_tools(&t)) {
        snprintf(why, why_size, "the game code builder is missing from this copy of " RT_APP_NAME);
        return false;
    }
    char work[1100], lib[1100], tmp[1100];
    snprintf(work, sizeof work, "%s" SEP ".gamebuild", game_dir);
    snprintf(lib, sizeof lib, "%s" SEP "%s", game_dir, GAMELIB_NAME);
    snprintf(tmp, sizeof tmp, "%s" SEP "%s.tmp", work, GAMELIB_NAME);
    rt_remove_game_file(game_dir, ".gamebuild");
    rt_remove_game_file(game_dir, GAMELIB_NAME);
    rt_remove_game_file(game_dir, GAMELIB_NAME ".id");
    mkdir(work, 0755);
    if (t.zig) {
        // (#121) The compiler runs at all: a damaged zig.exe (a 190 MB file a download or an unpacking tool cut short,
        // or an antivirus changed) stopped with an illegal instruction on every file, and the setup said only "The game
        // code didn't build" - the release's own zig.exe is the official 0.17.0 one, byte for byte. Asked once
        // (`zig version`, a fraction of a second), and named when it fails.
        char vlog[1200];
        snprintf(vlog, sizeof vlog, "%s" SEP "zig-version.log", work);
        char *argv[] = {t.cc, (char *)"version", NULL};
        PlatProc vp = spawn(argv, vlog);
        int r = vp ? plat_proc_wait(vp, true) : -1;
        if (r != 0) {
            LOG("[gamebuild] the compiler %s doesn't run (zig version: exit %d, %#x)\n", t.cc, r, (unsigned)r);
            snprintf(why, why_size, "the compiler that comes with " RT_APP_NAME " (the zig folder) doesn't run on this PC "
                     "(code %#x): its zig.exe is probably damaged. Download the release again, extract it with "
                     "Windows' Extract All or 7-Zip, and check that your antivirus didn't quarantine or change it",
                     (unsigned)r);
            return false;
        }
    }
    if (t.zig) {  // its caches in the work folder too (deleted below), not ~/.cache: hundreds of MB, used once
        char zc[1200];
        snprintf(zc, sizeof zc, "%s" SEP "zig-cache", work);
#ifdef _WIN32
        _putenv_s("ZIG_GLOBAL_CACHE_DIR", zc);
        _putenv_s("ZIG_LOCAL_CACHE_DIR", zc);
#else
        setenv("ZIG_GLOBAL_CACHE_DIR", zc, 1);
        setenv("ZIG_LOCAL_CACHE_DIR", zc, 1);
#endif
    }
    cache_init(&t, game_dir, work);  // (after `zig version`: its answer is part of every object's key)
    int reused = 0;
    LOG("[gamebuild] generator %s%s%s, compiler %s%s, %d jobs, for a CPU with %s%s%s\n", t.gen, t.gen_script[0] ? " " : "",
        t.gen_script, t.cc, t.zig ? " cc" : "", o->jobs, gamelib_cpu_name(), gamelib_cpu_march() ? ": -march=" : "",
        gamelib_cpu_march() ? gamelib_cpu_march() : "");

    // 1. the generator, all modules at once; their finished files are compiled while it goes on
    Gen gens[NMOD];
    int ngen = 0;
    bool ok = true;
    for (int i = 0; i < NMOD && ok; i++) {
        char nso[1100];
        snprintf(nso, sizeof nso, "%s" SEP "exefs" SEP "%s", game_dir, MODULES[i].name);
        if (!file_exists(nso)) {
            if (!strcmp(MODULES[i].name, "subsdk0")) continue;  // (optional: no videos without it)
            snprintf(why, why_size, "the game files have no exefs/%s", MODULES[i].name);
            ok = false;
            break;
        }
        {   // the functions of this module and game version that run (above: cold files)
            char id[17] = "";
            if (nso_build_id(nso, id)) hot_load(&g_hot[i], t.hot, MODULES[i].name, id);
            LOG("[gamebuild] %s (build %s): %s\n", MODULES[i].name, id[0] ? id : "?", g_hot[i].known
                ? "files without a function that runs are compiled without optimization" : "every file optimized");
        }
        Gen *g = &gens[ngen++];
        memset(g, 0, sizeof *g);
        g->m = &MODULES[i];
        g->exit = -1;
        snprintf(g->log, sizeof g->log, "%s" SEP "gen_%s.log", work, g->m->name);
        snprintf(g->out, sizeof g->out, "%s" SEP "%s", work, g->m->name);
        Args a = {0};
        add(&a, t.gen);
        if (t.gen_script[0]) add(&a, t.gen_script);
        add(&a, g->m->name);
        add(&a, nso);
        add(&a, g->m->base);
        add(&a, g->out);
        if (g->m->hle) { add(&a, "--hle"); add(&a, t.hle); }
        g->p = spawn_proc(a.v, g->log, true);
        args_free(&a);
        if (!g->p) { snprintf(why, why_size, "the game code builder didn't start"); ok = false; }
    }
    {   // the library's module list in GEN_MODS order (main, subsdk0, sdk)
        static const char *const order[] = {"main", "subsdk0", "sdk"};
        int n = snprintf(g_modules_def, sizeof g_modules_def, "-DGAMELIB_MODULES=");
        for (int k = 0; k < 3; k++)
            for (int i = 0; i < ngen; i++)
                if (!strcmp(gens[i].m->name, order[k]))
                    n += snprintf(g_modules_def + n, sizeof g_modules_def - n, "%sX(%s)", n > 18 ? " " : "", order[k]);
    }

    Units units = {0};
    int queued[NMOD] = {0};  // chunk files of each module put in units so far
    bool tables[NMOD] = {false};
    int next = 0, jobs = o->jobs < 1 ? 1 : o->jobs > 64 ? 64 : o->jobs;
    if (getenv("ODYSSEY_GAME_JOBS")) jobs = atoi(getenv("ODYSSEY_GAME_JOBS")) < 1 ? 1 : atoi(getenv("ODYSSEY_GAME_JOBS")) > 64 ? 64 : atoi(getenv("ODYSSEY_GAME_JOBS"));  // (tests: a Steam Deck's 8)
    struct { PlatProc p; int unit; } run[64];
    int nrun = 0;
    unsigned long long done_bytes = 0;
    double shown = 0;
    add_unit(&units, t.entry, work, "gamelib_entry", false);
    while (ok) {
        // generator progress, newly finished files
        bool gens_done = true;
        double gen_frac = 0;
        unsigned long long written = 0;
        for (int i = 0; i < ngen; i++) {
            Gen *g = &gens[i];
            if (g->exit < 0) {
                read_gen_log(g);
                int r = finished(g->p);
                if (r >= 0) {
                    g->exit = r;
                    read_gen_log(g);
                    if (getenv("ODYSSEY_LOG_GAMEBUILD"))
                        LOG("[gamebuild] generator (%s) ended at %.1f s, %d files\n", g->m->name, (plat_now_ns() - t0) / 1e9, g->files);
                    if (r != 0) {
                        char last[300];
                        log_tail(g->log, last, sizeof last);
                        LOG("[gamebuild] generator (%s) failed (exit %d): %s\n", g->m->name, r, last);
                        snprintf(why, why_size, "the game code couldn't be prepared");
                        ok = false;
                    }
                }
            }
            if (g->exit != 0) gens_done = false;
            for (; queued[i] < g->files; queued[i]++) {
                char src[1100], stem[64];
                snprintf(stem, sizeof stem, "%s_%03d", g->m->name, queued[i]);
                snprintf(src, sizeof src, "%s" SEP "%s.c", g->out, stem);
                add_unit(&units, src, work, stem, true);
            }
            if (g->exit == 0 && !tables[i]) {
                char src[1100], stem[64];
                snprintf(stem, sizeof stem, "%s_table", g->m->name);
                snprintf(src, sizeof src, "%s" SEP "%s.c", g->out, stem);
                add_unit(&units, src, work, stem, true);
                tables[i] = true;
            }
            gen_frac += g->m->share * (g->exit == 0 ? 1 : g->n ? (double)g->ci / (double)g->n : 0);
        }
        if (!ok) break;
        for (int k = 0; k < units.n; k++) written += units.u[k].bytes;

        // compiles (unit 0, the entry, last: it lists the modules' tables, which come at the end). The biggest file
        // waiting goes first: a few files hold enormous functions (7,000-10,000 lines) that take many minutes alone,
        // and started late they were the whole build's tail (the generator itself takes seconds, so they're all
        // waiting soon). next: the files started so far.
        // Files with a giant function before all others, the longest function first: the file's size does not tell -
        // main_092 (1.4.1) is 224 KB, one 3,900-line function, and takes 90 s alone on a Ryzen 9950X3D and 264 s on
        // a Steam Deck (the loop passes); by size it started among the last and the whole build then waited for it
        // with every other core idle. ODYSSEY_GAME_ORDER_OLD=1: by size only, as before (tests).
        static int order_old = -1;
        if (order_old < 0) order_old = getenv("ODYSSEY_GAME_ORDER_OLD") != NULL;
        while (nrun < jobs) {
            int pick = -1;
            for (int k = 1; k < units.n; k++) {
                if (units.u[k].started) continue;
                if (pick < 0) { pick = k; continue; }
                int gk = !order_old && units.u[k].longest > 3000 ? units.u[k].longest : 0;
                int gp = !order_old && units.u[pick].longest > 3000 ? units.u[pick].longest : 0;
                if (gk != gp ? gk > gp : units.u[k].bytes > units.u[pick].bytes) pick = k;
            }
            if (pick < 0) break;
            {   // made before from the same source and command: the last build's object (above: .gamecache)
                Unit *pu = &units.u[pick];
                char c[1200];
                if (unit_key(&t, pu) && (snprintf(c, sizeof c, "%s" SEP "%s.o", g_cache, pu->key), file_exists(c))) {
                    snprintf(pu->obj, sizeof pu->obj, "%s", c);
                    pu->hit = pu->started = true;
                    next++, reused++;
                    done_bytes += pu->bytes;
                    if (!getenv("ODYSSEY_LOG_GAMEBUILD")) remove(pu->src);
                    continue;
                }
            }
            PlatProc p = start_compile(&t, &units.u[pick], false);
            if (!p) { snprintf(why, why_size, "the compiler didn't start"); ok = false; break; }
            units.u[pick].started = true;
            units.u[pick].t_start = plat_now_ns();
            run[nrun].p = p, run[nrun].unit = pick;
            nrun++, next++;
        }
        for (int k = 0; k < nrun; k++) {
            int r = finished(run[k].p);
            if (r < 0) continue;
            Unit *u = &units.u[run[k].unit];
            if (r != 0) {
                char last[300];
                log_tail(u->log, last, sizeof last);
                LOG("[gamebuild] compiling %s failed (exit %d): %s\n", u->src, r, last);
                snprintf(why, why_size, "the game code didn't compile");
                ok = false;
            }
            done_bytes += u->bytes;
            // ODYSSEY_LOG_GAMEBUILD=1 (measurements): each file's compile - when it started and ended in the build, its
            // size and its longest function - and the generated C kept (for reading what a slow file holds)
            static int log_units = -1;
            if (log_units < 0) log_units = getenv("ODYSSEY_LOG_GAMEBUILD") != NULL;
            if (log_units)
                LOG("[gamebuild] unit %s: %.1f s (from %.1f to %.1f s), %llu KB, longest function %d lines\n",
                    u->src, (plat_now_ns() - u->t_start) / 1e9, (u->t_start - t0) / 1e9, (plat_now_ns() - t0) / 1e9,
                    u->bytes / 1024, u->longest);
            if (u->generated && !log_units) remove(u->src);  // (generated C, done with: not kept)
            run[k--] = run[--nrun];
        }
        if (!ok) break;
        if (gens_done && next == units.n - 1 && nrun == 0) {  // (all but the entry, unit 0, done)
            int cold = 0;
            for (int k = 0; k < units.n; k++) cold += units.u[k].cold;
            LOG("[gamebuild] %d files: %d compiled (%d without optimization: no function that runs), %d reused from the "
                "last build; by %.0f s\n", units.n - 1, units.n - 1 - reused, cold, reused, (plat_now_ns() - t0) / 1e9);
            break;
        }

        // progress: the generator's share (its functions written), the compiler's (bytes of C compiled, of an
        // estimate of all of it while the generator still writes)
        double est_total = gens_done ? (double)written : gen_frac > 0.02 ? (double)written / gen_frac : 0;
        double comp_frac = est_total > 0 ? (double)done_bytes / est_total : 0;
        if (comp_frac > 1) comp_frac = 1;
        double f = 0.15 * gen_frac + 0.8 * comp_frac;
        double secs = (double)(plat_now_ns() - t0) / 1e9;
        if (gen_frac == 0) f = 0.03 * (secs / (secs + 20));  // (the generator's first pass: no count yet)
        if (f > shown) shown = f;
        if (o->progress) o->progress(shown, !gens_done && comp_frac < 0.02 ? "Reading the game code..." : "Recompiling the game code...");
        sleep_ms(50);
    }
    for (int k = 0; k < nrun; k++) stop(run[k].p);  // (a failure: the rest is of no use - the failure shows at once)
    for (int i = 0; i < ngen; i++)
        if (gens[i].p && gens[i].exit < 0) stop(gens[i].p);

    // 2. the entry (the modules' tables), then link
    if (ok) {
        if (o->progress) o->progress(shown > 0.95 ? shown : 0.95, "Finishing...");
        PlatProc p = start_compile(&t, &units.u[0], true);
        int r = p ? wait_for(p) : 255;
        if (r != 0) {
            char last[300];
            log_tail(units.u[0].log, last, sizeof last);
            LOG("[gamebuild] compiling %s failed (exit %d): %s\n", units.u[0].src, r, last);
            snprintf(why, why_size, "the game code didn't compile");
            ok = false;
        }
    }
    if (ok) {
        char rsp[1100], log[1100];
        snprintf(rsp, sizeof rsp, "%s" SEP "link.rsp", work);
        snprintf(log, sizeof log, "%s" SEP "link.log", work);
        FILE *f = fopen(rsp, "wb");
        if (f) {
            for (int k = 0; k < units.n; k++) {  // (forward slashes: the response file's backslashes are escapes)
                fputc('"', f);
                for (const char *c = units.u[k].obj; *c; c++) fputc(*c == '\\' ? '/' : *c, f);
                fputs("\"\n", f);
            }
            fclose(f);
        }
        Args a = {0};
        char at[1200];
        snprintf(at, sizeof at, "@%s", rsp);
#ifdef _WIN32
        if (t.zig_rt[0]) {  // the linker itself, with the runtime built ahead of time (its flags and libraries: link.rsp)
            char opt[1200];
            add(&a, t.cc);
            add(&a, "lld-link");
            snprintf(opt, sizeof opt, "@%s" SEP "link.rsp", t.zig_rt);
            add(&a, opt);
            snprintf(opt, sizeof opt, "-LIBPATH:%s", t.zig_rt);
            add(&a, opt);
            snprintf(opt, sizeof opt, "-OUT:%s", tmp);
            add(&a, opt);
            add(&a, at);
            add(&a, t.implib);
        } else
#endif
        {
            compiler(&a, &t);
            add_split(&a, GAMELIB_SHARED);
            add(&a, "-o");
            add(&a, tmp);
            add(&a, at);
#ifdef _WIN32
            add(&a, t.implib);
#endif
        }
        PlatProc p = f ? spawn(a.v, log) : 0;
        args_free(&a);
        int r = p ? wait_for(p) : 255;
        if (r != 0) {
            char last[300];
            log_tail(log, last, sizeof last);
            LOG("[gamebuild] linking failed (exit %d): %s\n", r, last);
            snprintf(why, why_size, "the game code didn't link");
            ok = false;
        }
    }
    if (ok) {
        remove(lib);
        char idp[1100];
        snprintf(idp, sizeof idp, "%s.id", lib);
        FILE *f = NULL;
        if (rename(tmp, lib) != 0 || !(f = fopen(idp, "wb")) || fprintf(f, "%s\n", gamelib_id()) < 0) {
            snprintf(why, why_size, "the game code couldn't be saved");
            ok = false;
        }
        if (f) fclose(f);
    }
    if (ok) cache_keep(&units, game_dir);
    else if (reused) rt_remove_game_file(game_dir, ".gamecache");  // (a failed build that used it: the next one compiles all)
    free(units.u);
    if (!getenv("ODYSSEY_KEEP_GAMEBUILD")) rt_remove_game_file(game_dir, ".gamebuild");  // the C, the objects, the logs
    LOG("[gamebuild] %s in %.0fs\n", ok ? "done" : "failed", (double)(plat_now_ns() - t0) / 1e9);
    if (ok && o->progress) o->progress(1, "Finishing...");
    return ok;
}
