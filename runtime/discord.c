// Discord Rich Presence: "Playing Super Odyssey PC Recomp", the kingdom Mario is in and the time played, on the
// player's Discord profile (issue #76). No SDK: the Discord client listens on a local pipe / socket (discord-ipc-0..9)
// for frames of two little-endian 32-bit words (opcode, length) and a JSON body: opcode 0 the handshake
// ({"v":1,"client_id":...}), opcode 1 commands (SET_ACTIVITY), opcode 2 a close (e.g. an unknown client id).
//
// Everything runs on a thread of its own, which wakes once a second: the game never waits for Discord. Without a
// Discord client (or with the setting off) it only tries to connect every 15 s. The setting "Discord activity"
// (discord_presence) turns it off at once (closing the connection clears the activity); ODYSSEY_DISCORD=0 never
// starts the thread. ODYSSEY_DISCORD_ID=<id> uses another Discord application (its "logo" art asset is the picture).
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include "discord.h"
#include "platform.h"
#include "rt.h"
#include "settings.h"
#include "ui/i18n.h"

#define DISCORD_CLIENT_ID "1572040778073505872"  // the "Super Odyssey PC Recomp" application on discord.com/developers

// ---- the stage -> kingdom table (the game's internal stage names: <Kingdom>World<Part>Stage)
static const struct { const char *prefix, *name; } k_kingdoms[] = {
    {"CapWorld", "Cap Kingdom"},          {"WaterfallWorld", "Cascade Kingdom"}, {"SandWorld", "Sand Kingdom"},
    {"LakeWorld", "Lake Kingdom"},        {"ForestWorld", "Wooded Kingdom"},     {"CloudWorld", "Cloud Kingdom"},
    {"ClashWorld", "Lost Kingdom"},       {"CityWorld", "Metro Kingdom"},        {"SnowWorld", "Snow Kingdom"},
    {"SeaWorld", "Seaside Kingdom"},      {"LavaWorld", "Luncheon Kingdom"},     {"BossRaidWorld", "Ruined Kingdom"},
    {"SkyWorld", "Bowser's Kingdom"},     {"MoonWorld", "Moon Kingdom"},         {"PeachWorld", "Mushroom Kingdom"},
    {"Special1World", "Dark Side"},       {"Special2World", "Darker Side"},
};
const char *discord_kingdom_of(const char *stage) {
    if (!stage) return NULL;
    for (size_t i = 0; i < sizeof k_kingdoms / sizeof *k_kingdoms; i++)
        if (!strncmp(stage, k_kingdoms[i].prefix, strlen(k_kingdoms[i].prefix))) return k_kingdoms[i].name;
    return NULL;  // (a sub-area with a name of its own - a moon's challenge stage: still in the last kingdom)
}

// ---- what to show (any thread writes, the presence thread reads)
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static const char *g_kingdom;  // (g_mx) English; NULL: not in a kingdom yet (the title screen)

void discord_set_stage(const char *stage) {
    const char *k = discord_kingdom_of(stage);
    if (!k) return;
    pthread_mutex_lock(&g_mx);
    g_kingdom = k;
    pthread_mutex_unlock(&g_mx);
}

// ---- the connection (the presence thread only)
#ifdef _WIN32
static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static bool conn_open(void) {
    for (int i = 0; i < 10; i++) {
        char name[64];
        snprintf(name, sizeof name, "\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { g_pipe = h; return true; }
    }
    return false;
}
static bool conn_is_open(void) { return g_pipe != INVALID_HANDLE_VALUE; }
static void conn_close(void) {
    if (g_pipe != INVALID_HANDLE_VALUE) CloseHandle(g_pipe);
    g_pipe = INVALID_HANDLE_VALUE;
}
static bool conn_write(const void *p, size_t n) {
    DWORD w = 0;
    return WriteFile(g_pipe, p, (DWORD)n, &w, NULL) && w == n;
}
// bytes waiting to be read (>= 0), or -1: the pipe broke (Discord quit)
static long conn_avail(void) {
    DWORD a = 0;
    return PeekNamedPipe(g_pipe, NULL, 0, NULL, &a, NULL) ? (long)a : -1;
}
static bool conn_read(void *p, size_t n) {
    DWORD r = 0;
    return ReadFile(g_pipe, p, (DWORD)n, &r, NULL) && r == n;
}
static unsigned long my_pid(void) { return (unsigned long)GetCurrentProcessId(); }
#else
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0  // (macOS: SO_NOSIGPIPE on the socket instead)
#endif
static int g_fd = -1;
static bool try_path(const char *path) {
    struct sockaddr_un a = {.sun_family = AF_UNIX};
    if (strlen(path) >= sizeof a.sun_path) return false;
    strcpy(a.sun_path, path);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
    if (connect(fd, (struct sockaddr *)&a, sizeof a) == 0) { g_fd = fd; return true; }
    close(fd);
    return false;
}
static bool conn_open(void) {
    // the client's runtime directory, and where Flatpak / Snap / Vesktop builds of Discord put their socket
    const char *bases[] = {getenv("XDG_RUNTIME_DIR"), getenv("TMPDIR"), getenv("TMP"), getenv("TEMP"), "/tmp"};
    const char *subs[] = {"", "app/com.discordapp.Discord/", "snap.discord/", ".flatpak/dev.vencord.Vesktop/xdg-run/"};
    for (size_t b = 0; b < sizeof bases / sizeof *bases; b++) {
        if (!bases[b] || !*bases[b]) continue;
        for (size_t s = 0; s < sizeof subs / sizeof *subs; s++)
            for (int i = 0; i < 10; i++) {
                char path[512];
                snprintf(path, sizeof path, "%s/%sdiscord-ipc-%d", bases[b], subs[s], i);
                if (try_path(path)) return true;
            }
    }
    return false;
}
static bool conn_is_open(void) { return g_fd >= 0; }
static void conn_close(void) {
    if (g_fd >= 0) close(g_fd);
    g_fd = -1;
}
static bool conn_write(const void *p, size_t n) {
    const char *c = (const char *)p;
    while (n) {
        ssize_t w = send(g_fd, c, n, MSG_NOSIGNAL);
        if (w < 0 && errno == EINTR) continue;
        if (w <= 0) return false;
        c += w, n -= (size_t)w;
    }
    return true;
}
static long conn_avail(void) {
    struct pollfd p = {.fd = g_fd, .events = POLLIN};
    int r = poll(&p, 1, 0);
    if (r < 0) return errno == EINTR ? 0 : -1;
    if (r == 0) return 0;
    if (p.revents & (POLLERR | POLLNVAL)) return -1;
    char b;
    ssize_t n = recv(g_fd, &b, 1, MSG_PEEK | MSG_DONTWAIT);
    return n > 0 ? 1 << 16 : -1;  // (readable with nothing to read: closed)
}
static bool conn_read(void *p, size_t n) {
    char *c = (char *)p;
    while (n) {
        ssize_t r = recv(g_fd, c, n, 0);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        c += r, n -= (size_t)r;
    }
    return true;
}
static unsigned long my_pid(void) { return (unsigned long)getpid(); }
#endif

static bool send_frame(uint32_t op, const char *json) {
    size_t n = strlen(json);
    unsigned char *f = (unsigned char *)malloc(8 + n);
    if (!f) return false;
    for (int i = 0; i < 4; i++) f[i] = (unsigned char)(op >> (8 * i)), f[4 + i] = (unsigned char)((uint32_t)n >> (8 * i));
    memcpy(f + 8, json, n);
    bool ok = conn_write(f, 8 + n);
    free(f);
    return ok;
}
// One frame from Discord (waiting up to wait_ms for it): its opcode, or -1 (nothing came, or the connection broke:
// *broken). The body is kept in body (cut to its size).
static int read_frame(int wait_ms, char *body, size_t size, bool *broken) {
    *broken = false;
    for (int t = 0;; t += 10) {
        long a = conn_avail();
        if (a < 0) { *broken = true; return -1; }
        if (a >= 8) break;
        if (t >= wait_ms) return -1;
        plat_sleep_until_ns(plat_now_ns() + 10000000ull);
    }
    unsigned char h[8];
    if (!conn_read(h, 8)) { *broken = true; return -1; }
    uint32_t op = h[0] | h[1] << 8 | h[2] << 16 | (uint32_t)h[3] << 24, n = h[4] | h[5] << 8 | h[6] << 16 | (uint32_t)h[7] << 24;
    if (n > (1u << 20)) { *broken = true; return -1; }  // (not Discord)
    size_t kept = 0;
    while (n) {
        char chunk[512];
        uint32_t k = n < sizeof chunk ? n : (uint32_t)sizeof chunk;
        if (!conn_read(chunk, k)) { *broken = true; return -1; }
        if (body && kept + 1 < size) {
            size_t c = k < size - 1 - kept ? k : size - 1 - kept;
            memcpy(body + kept, chunk, c);
            kept += c;
        }
        n -= k;
    }
    if (body && size) body[kept] = 0;
    return (int)op;
}

// s as a JSON string's contents (UTF-8 kept as is)
static void json_text(char *out, size_t size, const char *s) {
    size_t o = 0;
    for (; *s && o + 7 < size; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') out[o++] = '\\', out[o++] = (char)c;
        else if (c < 0x20) o += (size_t)snprintf(out + o, size - o, "\\u%04x", c);
        else out[o++] = (char)c;
    }
    out[o] = 0;
}

static const char *client_id(void) {
    const char *e = getenv("ODYSSEY_DISCORD_ID");
    return e && *e ? e : DISCORD_CLIENT_ID;
}

// Connects and says hello; false: no Discord (or it refused this client id: *refused)
static bool connect_discord(bool *refused) {
    *refused = false;
    if (!conn_open()) return false;
    char hello[128], reply[512];
    snprintf(hello, sizeof hello, "{\"v\":1,\"client_id\":\"%s\"}", client_id());
    bool broken;
    int op = send_frame(0, hello) ? read_frame(5000, reply, sizeof reply, &broken) : -1;
    if (op == 1 && strstr(reply, "\"READY\"")) {
        LOG("[discord] connected: activity shown on the Discord profile\n");
        return true;
    }
    if (op == 2) {
        *refused = true;
        LOG("[discord] Discord refused the connection: %.200s\n", reply);
    }
    conn_close();
    return false;
}

static bool send_activity(const char *details, uint64_t start_unix, unsigned nonce) {
    char d[256], json[1024];
    json_text(d, sizeof d, details);
    snprintf(json, sizeof json,
             "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"%u\",\"args\":{\"pid\":%lu,\"activity\":{\"details\":\"%s\","
             "\"timestamps\":{\"start\":%llu},\"assets\":{\"large_image\":\"logo\",\"large_text\":\"Super Odyssey PC Recomp\"}}}}",
             nonce, my_pid(), d, (unsigned long long)start_unix);
    if (!send_frame(1, json)) return false;
    char reply[512];
    bool broken;
    int op = read_frame(5000, reply, sizeof reply, &broken);
    if (broken || op == 2) return false;
    if (op == 1 && strstr(reply, "\"ERROR\"")) LOG("[discord] activity not accepted: %.200s\n", reply);
    else LOG("[discord] activity: %s\n", details);
    return true;
}

static void *presence_thread(void *arg) {
    (void)arg;
    uint64_t start_unix = (uint64_t)time(NULL);
    uint64_t next_try = 0;
    unsigned nonce = 0;
    char shown[256] = "";  // what Discord shows now ("": nothing yet on this connection)
    for (;;) {
        uint64_t now = plat_now_ns();
        if (!g_settings.discord_presence) {
            if (conn_is_open()) { conn_close(); LOG("[discord] off: activity cleared\n"); }
            next_try = 0;
        } else {
            if (!conn_is_open() && now >= next_try) {
                bool refused;
                next_try = now + (uint64_t)15 * 1000000000ull;
                if (connect_discord(&refused)) shown[0] = 0;
                else if (refused) next_try = now + (uint64_t)300 * 1000000000ull;  // (no use asking again soon)
            }
            if (conn_is_open()) {
                bool broken = false;
                while (read_frame(0, NULL, 0, &broken) >= 0) {}  // (replies nobody waits for)
                if (broken) {
                    conn_close();
                    LOG("[discord] disconnected (Discord closed?)\n");
                    next_try = now + (uint64_t)5 * 1000000000ull;
                } else {
                    pthread_mutex_lock(&g_mx);
                    const char *k = g_kingdom;
                    pthread_mutex_unlock(&g_mx);
                    const char *details = tr(k ? k : "Title screen");
                    if (strcmp(details, shown)) {
                        if (send_activity(details, start_unix, ++nonce)) snprintf(shown, sizeof shown, "%s", details);
                        else { conn_close(); next_try = now + (uint64_t)5 * 1000000000ull; }
                    }
                }
            }
        }
        plat_sleep_until_ns(plat_now_ns() + 1000000000ull);
    }
    return NULL;
}

static void start_once(void) {
    const char *e = getenv("ODYSSEY_DISCORD");
    if (e && *e && !atoi(e)) {
        LOG("[discord] off (ODYSSEY_DISCORD=0)\n");
        return;
    }
    pthread_t t;
    if (pthread_create(&t, NULL, presence_thread, NULL) == 0) pthread_detach(t);
}
void discord_start(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, start_once);
}
