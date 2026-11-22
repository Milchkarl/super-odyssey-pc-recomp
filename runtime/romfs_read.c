// Reading the game's files on the host (romfs_read.h): the RomFS image's tables (header: ten u64 - header size, directory
// hash table offset / size, directory table offset / size, file hash table offset / size, file table offset / size,
// data offset; a directory entry: parent, sibling, first child directory, first file, hash sibling, name length,
// name; a file entry: parent, sibling, data offset (u64), data size (u64), hash sibling, name length, name), Yaz0 and
// SARC as the game's own tools write them.
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/types.h>
#endif

#include "romfs_read.h"

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t le64(const uint8_t *p) { return le32(p) | (uint64_t)le32(p + 4) << 32; }

static bool read_at(FILE *f, uint64_t off, void *dst, size_t n) {
#ifdef _WIN32
    if (_fseeki64(f, (long long)off, SEEK_SET) != 0) return false;
#else
    if (fseeko(f, (off_t)off, SEEK_SET) != 0) return false;
#endif
    return fread(dst, 1, n, f) == n;
}

uint8_t *romfs_read_file(const char *image, const char *path, size_t *size) {
    FILE *f = fopen(image, "rb");
    if (!f) return NULL;
    uint8_t head[0x50], *dm = NULL, *fm = NULL, *out = NULL;
    uint64_t h[10];
    if (!read_at(f, 0, head, sizeof head)) goto done;
    for (int i = 0; i < 10; i++) h[i] = le64(head + 8 * i);
    if (h[4] > (64u << 20) || h[8] > (64u << 20)) goto done;
    dm = malloc(h[4] ? h[4] : 1), fm = malloc(h[8] ? h[8] : 1);
    if (!read_at(f, h[3], dm, h[4]) || !read_at(f, h[7], fm, h[8])) goto done;
    uint32_t cur = 0;  // (the root directory)
    for (const char *p = path; *p;) {
        const char *slash = strchr(p, '/');
        size_t len = slash ? (size_t)(slash - p) : strlen(p);
        if (slash) {  // a directory
            uint32_t child = le32(dm + cur + 8);
            while (child != 0xFFFFFFFFu && child + 0x18 <= h[4]) {
                uint32_t nl = le32(dm + child + 0x14);
                if (nl == len && child + 0x18 + nl <= h[4] && !memcmp(dm + child + 0x18, p, len)) break;
                child = le32(dm + child + 4);
            }
            if (child == 0xFFFFFFFFu || child + 0x18 > h[4]) goto done;
            cur = child, p = slash + 1;
            continue;
        }
        uint32_t fe = le32(dm + cur + 0xC);  // the file in this directory
        while (fe != 0xFFFFFFFFu && fe + 0x20 <= h[8]) {
            uint32_t nl = le32(fm + fe + 0x1C);
            if (nl == len && fe + 0x20 + nl <= h[8] && !memcmp(fm + fe + 0x20, p, len)) {
                uint64_t off = le64(fm + fe + 8), n = le64(fm + fe + 0x10);
                if (n > (1ull << 31)) goto done;
                out = malloc(n ? n : 1);
                if (!read_at(f, h[9] + off, out, n)) { free(out); out = NULL; goto done; }
                *size = n;
                goto done;
            }
            fe = le32(fm + fe + 4);
        }
        goto done;
    }
done:
    free(dm);
    free(fm);
    fclose(f);
    return out;
}

const char *romfs_visit_name = "";

bool romfs_visit_files(const char *image, RomfsFileVisitor visit, void *arg) {
    FILE *f = fopen(image, "rb");
    if (!f) return false;
    uint8_t head[0x50], *table = NULL, *data = NULL;
    bool ok = false;
    if (!read_at(f, 0, head, sizeof head)) goto done;
    uint64_t table_off = le64(head + 0x38), table_size = le64(head + 0x40), data_off = le64(head + 0x48);
    if (table_size > (64u << 20) || !table_size) goto done;
    table = malloc((size_t)table_size);
    if (!table || !read_at(f, table_off, table, (size_t)table_size)) goto done;
    size_t capacity = 0;
    for (uint64_t i = 0; i < table_size;) {
        if (table_size - i < 0x20) goto done;
        const uint8_t *entry = table + i;
        uint64_t name_size = le32(entry + 0x1c), next = i + 0x20 + ((name_size + 3) & ~3ull);
        if (next > table_size) goto done;
        i = next;
        uint64_t offset = le64(entry + 8), size = le64(entry + 0x10);
        if (size < 4) continue;
        if (size > (512u << 20) || offset > UINT64_MAX - data_off) goto done;
        if ((size_t)size > capacity) {
            uint8_t *larger = realloc(data, (size_t)size);
            if (!larger) goto done;
            data = larger;
            capacity = (size_t)size;
        }
        char name[256];
        size_t nl = name_size < sizeof name - 1 ? (size_t)name_size : sizeof name - 1;
        memcpy(name, entry + 0x20, nl);
        name[nl] = 0;
        romfs_visit_name = name;
        bool visited = read_at(f, data_off + offset, data, (size_t)size) && visit(data, (size_t)size, arg);
        romfs_visit_name = "";
        if (!visited) goto done;
    }
    ok = true;
done:
    free(table);
    free(data);
    fclose(f);
    return ok;
}

uint8_t *yaz0_decode(const uint8_t *in, size_t n, size_t *out_n) {
    if (n < 16 || memcmp(in, "Yaz0", 4)) return NULL;
    size_t size = (size_t)in[4] << 24 | (size_t)in[5] << 16 | (size_t)in[6] << 8 | in[7], i = 16, o = 0;
    uint8_t *out = malloc(size ? size : 1);
    if (!out) return NULL;
    while (o < size && i < n) {
        uint8_t code = in[i++];
        for (int b = 0; b < 8 && o < size; b++) {
            if (code & (0x80 >> b)) {
                if (i >= n) goto bad;
                out[o++] = in[i++];
                continue;
            }
            if (i + 1 >= n) goto bad;
            uint8_t b1 = in[i], b2 = in[i + 1];
            i += 2;
            size_t dist = ((size_t)(b1 & 0xF) << 8 | b2) + 1, len = b1 >> 4;
            if (!len) {
                if (i >= n) goto bad;
                len = (size_t)in[i++] + 0x12;
            } else len += 2;
            if (dist > o) goto bad;
            while (len-- && o < size) { out[o] = out[o - dist]; o++; }
        }
    }
    if (o != size) goto bad;
    *out_n = size;
    return out;
bad:
    free(out);
    return NULL;
}

// SARC: "SARC", header size, byte order mark, file size, data offset, version; SFAT: "SFAT", header size, node count,
// hash key, then per node: name hash, attributes (name offset / 4 in the low 16 bits), data start, data end; SFNT:
// "SFNT", header size, then the names. (The game's archives are little-endian.)
const uint8_t *sarc_find(const uint8_t *d, size_t n, const char *name, size_t *size) {
    if (n < 0x20 || memcmp(d, "SARC", 4) || d[6] != 0xFF || d[7] != 0xFE) return NULL;
    uint32_t data = le32(d + 0xC), o = 0x14;
    if (memcmp(d + o, "SFAT", 4)) return NULL;
    uint32_t count = (uint32_t)d[o + 6] | (uint32_t)d[o + 7] << 8, names = o + 0xC + count * 16;
    if ((size_t)names + 8 > n || memcmp(d + names, "SFNT", 4)) return NULL;
    names += 8;
    for (uint32_t k = 0; k < count; k++) {
        const uint8_t *e = d + o + 0xC + k * 16;
        uint32_t no = names + (le32(e + 4) & 0xFFFF) * 4, s = le32(e + 8), t = le32(e + 12);
        if (no >= n || (size_t)data + t > n || s > t) continue;
        if (!strncmp((const char *)d + no, name, n - no)) { *size = t - s; return d + data + s; }
    }
    return NULL;
}

bool sarc_visit_files(const uint8_t *d, size_t n, RomfsFileVisitor visit, void *arg) {
    if (n < 0x20 || memcmp(d, "SARC", 4) || d[6] != 0xff || d[7] != 0xfe || memcmp(d + 0x14, "SFAT", 4)) return false;
    uint32_t count = (uint32_t)d[0x1a] | (uint32_t)d[0x1b] << 8;
    size_t data = le32(d + 0xc);
    if (data < 0x20 + (size_t)count * 16 || data > n) return false;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *entry = d + 0x20 + 16 * i;
        size_t start = le32(entry + 8), end = le32(entry + 12);
        if (start > end || end > n - data) return false;
        if (!visit(d + data + start, end - start, arg)) return false;
    }
    return true;
}

// ---- a mod's archive with the files it lacks taken from the game's own (fs.c, rom_index)
// A translation or texture mod's .szs made for one game version can miss files another version's code loads: the
// JumpManClub Brasil translation (made from a later version's archives) has no SelectBalloonListParts.msbt in its
// LayoutMessage.szs, which 1.0.0 reads at start-up - a null message file, a crash in LMSi_AnalyzeMessageBinary
// (exception at 0xc). sarc_fill_missing makes a new SARC: every file of the mod's, then each file only the game's
// archive has. The nodes are in name-hash order (the game looks files up by binary search on the hash) and each file
// keeps the alignment it had in its own archive (as far as its offset shows, at most 0x2000).
typedef struct { const char *name; const uint8_t *data; uint32_t size, align, hash; } SarcEntry;

static int sarc_entries(const uint8_t *d, size_t n, SarcEntry **out, uint32_t *key) {
    if (n < 0x20 || memcmp(d, "SARC", 4) || d[6] != 0xFF || d[7] != 0xFE || memcmp(d + 0x14, "SFAT", 4)) return -1;
    uint32_t count = (uint32_t)d[0x1a] | (uint32_t)d[0x1b] << 8, data = le32(d + 0xC), names = 0x20 + count * 16;
    *key = le32(d + 0x1c);
    if ((size_t)names + 8 > n || memcmp(d + names, "SFNT", 4) || data > n) return -1;
    names += 8;
    SarcEntry *v = calloc(count ? count : 1, sizeof *v);
    for (uint32_t k = 0; k < count; k++) {
        const uint8_t *e = d + 0x20 + k * 16;
        uint32_t attr = le32(e + 4), s = le32(e + 8), t = le32(e + 12);
        size_t no = names + (size_t)(attr & 0xFFFF) * 4;
        if (!(attr & 0x01000000) || no >= n || !memchr(d + no, 0, n - no) || s > t || t > n - data) { free(v); return -1; }
        uint32_t a = 4, at = data + s;
        while (a < 0x2000 && !(at & (a * 2 - 1))) a *= 2;
        v[k] = (SarcEntry){(const char *)d + no, d + data + s, t - s, a, le32(e)};
    }
    *out = v;
    return (int)count;
}

static uint32_t sarc_hash(const char *s, uint32_t key) {
    uint32_t h = 0;
    for (; *s; s++) h = h * key + (uint32_t)(int32_t)(signed char)*s;
    return h;
}

static int sarc_entry_cmp(const void *a, const void *b) {
    uint32_t x = ((const SarcEntry *)a)->hash, y = ((const SarcEntry *)b)->hash;
    return x < y ? -1 : x > y;
}

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v, p[1] = (uint8_t)(v >> 8), p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24); }

uint8_t *sarc_fill_missing(const uint8_t *mod, size_t mod_n, const uint8_t *orig, size_t orig_n, size_t *out_n, int *added) {
    *added = 0;
    SarcEntry *m = NULL, *o = NULL;
    uint32_t key = 0x65, okey = 0x65;
    int nm = sarc_entries(mod, mod_n, &m, &key), no = sarc_entries(orig, orig_n, &o, &okey);
    if (nm < 0 || no < 0) { free(m); free(o); return NULL; }
    SarcEntry *all = malloc(sizeof *all * (size_t)(nm + no + 1));
    memcpy(all, m, sizeof *m * (size_t)nm);
    int n = nm;
    for (int i = 0; i < no; i++) {
        bool have = false;
        for (int k = 0; k < nm && !have; k++) have = !strcmp(m[k].name, o[i].name);
        if (!have) all[n++] = o[i], (*added)++;
    }
    uint8_t *res = NULL;
    if (*added) {
        for (int i = 0; i < n; i++) all[i].hash = sarc_hash(all[i].name, key);
        qsort(all, (size_t)n, sizeof *all, sarc_entry_cmp);
        size_t names = 0, align = 4;
        for (int i = 0; i < n; i++) {
            names += (strlen(all[i].name) + 4) & ~(size_t)3;
            if (all[i].align > align) align = all[i].align;
        }
        size_t head = 0x20 + (size_t)n * 16 + 8 + names, data = (head + align - 1) & ~(align - 1), end = 0;
        for (int i = 0; i < n; i++) end = ((end + all[i].align - 1) & ~((size_t)all[i].align - 1)) + all[i].size;
        size_t total = data + end;
        res = calloc(1, total);
        memcpy(res, "SARC", 4), res[4] = 0x14, res[6] = 0xFF, res[7] = 0xFE;
        put32(res + 8, (uint32_t)total), put32(res + 0xC, (uint32_t)data), res[0x10] = 0, res[0x11] = 1;
        memcpy(res + 0x14, "SFAT", 4), res[0x18] = 0xC, res[0x1a] = (uint8_t)n, res[0x1b] = (uint8_t)(n >> 8);
        put32(res + 0x1c, key);
        uint8_t *nt = res + 0x20 + (size_t)n * 16;
        memcpy(nt, "SFNT", 4), nt[4] = 8;
        size_t np = 0, dp = 0;
        for (int i = 0; i < n; i++) {
            uint8_t *e = res + 0x20 + (size_t)i * 16;
            dp = (dp + all[i].align - 1) & ~((size_t)all[i].align - 1);
            put32(e, all[i].hash), put32(e + 4, 0x01000000u | (uint32_t)(np / 4));
            put32(e + 8, (uint32_t)dp), put32(e + 12, (uint32_t)(dp + all[i].size));
            strcpy((char *)nt + 8 + np, all[i].name);
            np += (strlen(all[i].name) + 4) & ~(size_t)3;
            memcpy(res + data + dp, all[i].data, all[i].size);
            dp += all[i].size;
        }
        *out_n = total;
    }
    free(all), free(m), free(o);
    return res;
}

// Yaz0 holding d as literals (a flag byte 0xFF before every 8 bytes): what the game's decoder reads; header bytes 8..15
// (the alignment some tools write) copied from the mod's own file when given.
uint8_t *yaz0_store(const uint8_t *d, size_t n, const uint8_t *like, size_t *out_n) {
    size_t total = 16 + n + (n + 7) / 8;
    uint8_t *y = calloc(1, total), *p = y + 16;
    memcpy(y, "Yaz0", 4);
    y[4] = (uint8_t)(n >> 24), y[5] = (uint8_t)(n >> 16), y[6] = (uint8_t)(n >> 8), y[7] = (uint8_t)n;
    if (like) memcpy(y + 8, like + 8, 8);
    for (size_t i = 0; i < n; i += 8) {
        size_t k = n - i < 8 ? n - i : 8;
        *p++ = 0xFF;
        memcpy(p, d + i, k), p += k;
    }
    *out_n = total;
    return y;
}
