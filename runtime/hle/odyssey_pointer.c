// The mouse pointer in the game's menus (#64, prototype; 1.0.0 and 1.4.1). The game's menus know no pointer: each one keeps
// its own selection and moves it on the D-pad. For a menu whose items we can find, the pointer over an item that
// isn't selected becomes the selection at once, set the way the game's own D-pad step sets it (its index, then the
// items' "Wait" / "Select" actions and the menu's update), and a left click is its A (the mouse in menus). Where the
// items are comes from the game itself: al::calcPaneTrans (the "Cursor" pane each item has, where the game puts its cursor), in the
// layouts' 1280 x 720 space, as the pointer (input_pointer_layout).
//
// Menus, by their state's nerve (RTTI: the nerve class's vtable, slot 0 = execute(NerveKeeper *), host at [keeper]):
//   SceneStateSelectMode, its select nerve (new game: Start / Assist Mode): items at +0x30 (selected while the byte
//   at +0x59 is 1) and +0x38; updateCursorParts(this, instant) updates their look (disassembly of 1.4.1).
// The objects' layouts are the same in 1.0.0 and 1.4.1 (each offset below checked in both versions' code); only the
// addresses differ (g_a). 1.0.0's from its symbols; its CommonVerticalList Active / MapLayout Wait nerves are inlined
// copies of exeActive / exeWait (as in 1.4.1), found by their code.
//   MenuSelectParts, its Select nerve (the title menu and the pause menu), CommonVerticalList (Options and its pages,
//   the data manager's file lists), WindowConfirmData / WindowConfirm (yes / no windows): below.
// Not yet: the cursor sound on a pointer move; shops, the wardrobe, dialogue choices, the Odyssey's world map, the
// collection lists and the map (click and wheel only there).
// ODYSSEY_POINTER_PROBE=1 logs the hooked menus and the pointer's choices.
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../hle.h"
#include "../platform.h"
#include "../rt.h"

bool input_pointer_layout(float *x, float *y);  // runtime/hle/input.c

typedef struct {
    uint64_t calc_pane_trans;     // al::calcPaneTrans(sead::Vector3f *out, const al::IUseLayout *, const char *pane)
    uint64_t start_action;        // al::startAction(IUseLayoutAction *, const char *action, const char *pane)
    uint64_t set_pane_local_trans;  // al::setPaneLocalTrans(IUseLayout *, const char *pane, const Vector3f &)
    uint64_t str_cursor, str_wait, str_select, str_par_cursor;  // "Cursor", "Wait", "Select", "ParCursor" (the game's)
    uint64_t menu_select_update;  // MenuSelectParts::startActionMarioSelectIndex: what exeSelect calls after a step
    uint64_t select_mode_update;  // SceneStateSelectMode::updateCursorParts(this, instant): the items' look from +0x59
    uint64_t order_off, order_on; // MenuSelectParts: the item order tables (the byte at +0x58 0 / not 0)
    uint64_t nrv_select_mode, nrv_menu_select[2], nrv_vertical_list, nrv_confirm_data, nrv_confirm, nrv_map_wait;
} PointerAddrs;
static const PointerAddrs k_v141 = {
    0x7000630b60ull, 0x700062f120ull, 0x7000630d50ull, 0x7000e846f5ull, 0x7000e75a89ull, 0x7000e846eeull, 0x7000e65417ull,
    0x70001d7290ull, 0x700046ae80ull, 0x7000ec8288ull, 0x7000ec8270ull,
    0x700046b580ull, {0x70001d7a20ull, 0}, 0x70001c6c90ull, 0x70001ea5a0ull, 0x700070ce20ull, 0x70001d4910ull,
};
static const PointerAddrs k_v100 = {  // (MenuSelectParts: two nerves run exeSelect in 1.0.0)
    0x70008b2218ull, 0x70008b0b0cull, 0x70008b26acull, 0x700182b99bull, 0x700181c5bcull, 0x700182b984ull, 0x700182b9bfull,
    0x70001f978cull, 0x70004c493cull, 0x70018af570ull, 0x70018af55cull,
    0x70004c510cull, {0x70001f9f3cull, 0x70001f9f50ull}, 0x70001e3a88ull, 0x70002117ccull, 0x700099c460ull, 0x70001f6c94ull,
};
static const PointerAddrs *g_a = &k_v141;
#define CALC_PANE_TRANS (g_a->calc_pane_trans)
#define STR_CURSOR (g_a->str_cursor)
#define START_ACTION (g_a->start_action)
#define STR_WAIT (g_a->str_wait)
#define STR_SELECT (g_a->str_select)
#define MENU_SELECT_UPDATE (g_a->menu_select_update)
#define SELECT_MODE_UPDATE (g_a->select_mode_update)
static int g_log = -1;

// A game function called from a hook (the game thread), three arguments; the caller's registers kept
static void guest_fn(Cpu *c, uint64_t fn, uint64_t a, uint64_t b, uint64_t d) {
    Cpu saved = *c;
    c->sp = (c->sp - 0x200) & ~0xFull;
    c->x[0] = a, c->x[1] = b, c->x[2] = d;
    guest_call(c, fn);
    *c = saved;
}

// An item's "Cursor" pane in layout coordinates (x, y), from the game; layout: the item's al::LayoutActor
static bool item_point(Cpu *c, uint64_t layout, float out[2]) {
    if (!layout) return false;
    Cpu saved = *c;
    c->sp = (c->sp - 0x200) & ~0xFull;
    uint64_t vec = c->sp;
    memset((void *)(uintptr_t)vec, 0, 16);
    c->x[0] = vec, c->x[1] = layout + 8, c->x[2] = STR_CURSOR;  // (IUseLayout: the actor + 8, as the game passes it)
    guest_call(c, CALC_PANE_TRANS);
    memcpy(out, (void *)(uintptr_t)vec, 8);
    *c = saved;
    return isfinite(out[0]) && isfinite(out[1]);
}

// A vertical list of n items: the one the pointer is over (nearest by height, within reach), or -1
static int pick(const float pts[][2], int n, float px, float py) {
    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < n; i++) {
        float d = fabsf(py - pts[i][1]);
        if (d < bd) bd = d, best = i;
    }
    return bd < 160 ? best : -1;
}

// ---- SceneStateSelectMode (Start / Assist Mode)
static GuestFn g_select_mode;
static void select_mode(Cpu *c) {
    uint64_t self = *(uint64_t *)(uintptr_t)c->x[1];
    float px, py;
    if (self && input_pointer_layout(&px, &py)) {
        float pts[2][2];
        uint64_t a = *(uint64_t *)(uintptr_t)(self + 0x30), b = *(uint64_t *)(uintptr_t)(self + 0x38);
        if (item_point(c, a, pts[0]) && item_point(c, b, pts[1])) {
            int want = pick((const float(*)[2])pts, 2, px, py);
            int sel = *(uint8_t *)(uintptr_t)(self + 0x59) ? 0 : 1;  // (+0x59 set: the item at +0x30)
            if (g_log) {
                static int last = -2;
                if (want != last) LOG("[pointer] select mode: pointer %.0f,%.0f over %d (items at y %.0f / %.0f), selected %d\n",
                                      px, py, want, pts[0][1], pts[1][1], sel);
                last = want;
            }
            if (want >= 0 && want != sel) {  // the byte the D-pad toggles, then the game's own look update
                *(uint8_t *)(uintptr_t)(self + 0x59) = want == 0;
                guest_fn(c, SELECT_MODE_UPDATE, self, 0, 0);
            }
        }
    }
    g_select_mode(c);
}

// ---- MenuSelectParts (the pause menu's list): count at +0x18, the selected index at +0x1c, the items' layout actors
// at +0x28 [order[index]], the order table chosen by the byte at +0x58 (g_a: order_off / order_on)
static GuestFn g_menu_select;
static void menu_select(Cpu *c) {
    uint64_t self = *(uint64_t *)(uintptr_t)c->x[1];
    float px, py;
    if (self && input_pointer_layout(&px, &py)) {
        int n = *(int32_t *)(uintptr_t)(self + 0x18), sel = *(int32_t *)(uintptr_t)(self + 0x1c);
        uint64_t parts = *(uint64_t *)(uintptr_t)(self + 0x28);
        const int32_t *order = (const int32_t *)(uintptr_t)(*(uint8_t *)(uintptr_t)(self + 0x58) ? g_a->order_on : g_a->order_off);
        float pts[8][2];
        bool ok = n >= 1 && n <= 6 && parts && sel >= 0 && sel < n;
        for (int i = 0; ok && i < n; i++)
            ok = order[i] >= 0 && order[i] < 8 && item_point(c, *(uint64_t *)(uintptr_t)(parts + 8 * (uint64_t)order[i]), pts[i]);
        if (ok) {
            int want = pick((const float(*)[2])pts, n, px, py);
            if (g_log) {
                static int last = -2;
                if (want != last) LOG("[pointer] pause menu: pointer %.0f,%.0f over %d of %d (first at y %.0f, last %.0f), selected %d\n",
                                      px, py, want, n, pts[0][1], pts[n - 1][1], sel);
                last = want;
            }
            if (want >= 0 && want != sel) {  // as exeSelect does on a D-pad step, straight to the item
                uint64_t old = *(uint64_t *)(uintptr_t)(parts + 8 * (uint64_t)order[sel]);
                uint64_t now = *(uint64_t *)(uintptr_t)(parts + 8 * (uint64_t)order[want]);
                guest_fn(c, START_ACTION, old ? old + 0x10 : 0, STR_WAIT, 0);
                *(int32_t *)(uintptr_t)(self + 0x1c) = want;
                guest_fn(c, START_ACTION, now ? now + 0x10 : 0, STR_SELECT, 0);
                guest_fn(c, MENU_SELECT_UPDATE, self, 0, 0);
            }
        }
    }
    g_menu_select(c);
}

// ---- CommonVerticalList (Options and its pages, the data manager, other scrolling lists): the selected index at +0x3c,
// the first shown at +0x40, the previous pair at +0x44 / +0x48, the item count at +0x9c, the shown items' layout
// actors at +0x20 (+0x18 of them; slot = index - first + 1), a byte per item at +0x90 (0: can't be chosen - up()
// skips it), the cursor's move timer +0x50 (set to +0x54 on a step). As up() (0x70001c5e20) sets them, for an item
// already on screen (no scroll).
static GuestFn g_vertical_list;
static void vertical_list(Cpu *c) {
    uint64_t self = *(uint64_t *)(uintptr_t)c->x[1];
    float px, py;
    if (self && input_pointer_layout(&px, &py)) {
        int sel = *(int32_t *)(uintptr_t)(self + 0x3c), top = *(int32_t *)(uintptr_t)(self + 0x40);
        int total = *(int32_t *)(uintptr_t)(self + 0x9c), slots = *(int32_t *)(uintptr_t)(self + 0x18);
        uint64_t items = *(uint64_t *)(uintptr_t)(self + 0x20), enabled = *(uint64_t *)(uintptr_t)(self + 0x90);
        float pts[16][2];
        int idx[16], n = 0;
        if (items && total > 0 && total < 200 && slots > 0 && slots <= 16)
            for (int s = 0; s < slots; s++) {
                int i = top + s - 1;
                uint64_t item = *(uint64_t *)(uintptr_t)(items + 8 * (uint64_t)s);
                if (i < 0 || i >= total || !item || !item_point(c, item, pts[n])) continue;
                idx[n++] = i;
            }
        int k = n >= 2 ? pick((const float(*)[2])pts, n, px, py) : -1;
        // (within half the items' spacing of one: between or past them, nothing)
        float half = n >= 2 ? fabsf(pts[0][1] - pts[1][1]) / 2 : 0;
        if (k >= 0 && fabsf(py - pts[k][1]) > half) k = -1;
        int want = k >= 0 ? idx[k] : -1;
        if (g_log) {
            static int last = -2;
            if (want != last) {
                char at[160] = "";
                for (int j = 0, o = 0; j < n && o < (int)sizeof at - 16; j++) o += snprintf(at + o, sizeof at - o, " %.0f", pts[j][1]);
                LOG("[pointer] list: pointer %.0f,%.0f over item %d (%d shown of %d, first %d; at y%s), selected %d\n", px, py,
                    want, n, total, top, at, sel);
            }
            last = want;
        }
        if (want >= 0 && want != sel && (!enabled || *(uint8_t *)(uintptr_t)(enabled + (uint64_t)want))) {
            *(int32_t *)(uintptr_t)(self + 0x44) = sel;
            *(int32_t *)(uintptr_t)(self + 0x48) = top;
            *(int32_t *)(uintptr_t)(self + 0x3c) = want;
            *(uint8_t *)(uintptr_t)(self + 0x65) = 0;
            *(int32_t *)(uintptr_t)(self + 0x50) = *(int32_t *)(uintptr_t)(self + 0x54);
        }
    }
    g_vertical_list(c);
}

// ---- WindowConfirmData (save / load / delete: "Save" / "Cancel"): two buttons' layout actors at +0x20 / +0x28, the
// selected one at +0x38; a step starts "Select" on the new one and "Wait" on the other (exeWait 0x70001ea120), which
// then puts the cursor (+0x18) on the selected button's "Cursor" pane itself, every frame.
static GuestFn g_confirm_data;
static void confirm_data(Cpu *c) {
    uint64_t self = *(uint64_t *)(uintptr_t)c->x[1];
    float px, py, pts[2][2];
    if (self && input_pointer_layout(&px, &py)) {
        uint64_t b0 = *(uint64_t *)(uintptr_t)(self + 0x20), b1 = *(uint64_t *)(uintptr_t)(self + 0x28);
        int sel = *(int32_t *)(uintptr_t)(self + 0x38);
        if (b0 && b1 && (sel == 0 || sel == 1) && item_point(c, b0, pts[0]) && item_point(c, b1, pts[1])) {
            int want = pick((const float(*)[2])pts, 2, px, py);
            if (want >= 0 && fabsf(py - pts[want][1]) > fabsf(pts[0][1] - pts[1][1])) want = -1;
            if (g_log) {
                static int last = -2;
                if (want != last) LOG("[pointer] confirm: pointer %.0f,%.0f over %d (at y %.0f / %.0f), selected %d\n", px, py,
                                      want, pts[0][1], pts[1][1], sel);
                last = want;
            }
            if (want >= 0 && want != sel) {
                guest_fn(c, START_ACTION, b0 + 0x10, want == 0 ? STR_SELECT : STR_WAIT, 0);
                guest_fn(c, START_ACTION, b1 + 0x10, want == 1 ? STR_SELECT : STR_WAIT, 0);
                *(int32_t *)(uintptr_t)(self + 0x38) = want;
            }
        }
    }
    g_confirm_data(c);
}

// ---- WindowConfirm (the game's other yes / no and choice windows): the button count at +0x130, the selected one at
// +0x134, the buttons' layout actors at +0x150 (+0x148 of them). A step (exeWait 0x700070c830): "Wait" on the old,
// the index, "Select" on the new, then the window's "ParCursor" pane moved to the new button's "Cursor" pane.
#define SET_PANE_LOCAL_TRANS (g_a->set_pane_local_trans)
#define STR_PAR_CURSOR (g_a->str_par_cursor)
static GuestFn g_confirm;
static void confirm(Cpu *c) {
    uint64_t self = *(uint64_t *)(uintptr_t)c->x[1];
    float px, py, pts[8][2];
    if (self && input_pointer_layout(&px, &py)) {
        int n = *(int32_t *)(uintptr_t)(self + 0x130), sel = *(int32_t *)(uintptr_t)(self + 0x134);
        int cap = *(int32_t *)(uintptr_t)(self + 0x148);
        uint64_t items = *(uint64_t *)(uintptr_t)(self + 0x150);
        bool ok = items && n >= 2 && n <= 8 && n <= cap && sel >= 0 && sel < n;
        for (int i = 0; ok && i < n; i++) ok = item_point(c, *(uint64_t *)(uintptr_t)(items + 8 * (uint64_t)i), pts[i]);
        if (ok) {
            int want = pick((const float(*)[2])pts, n, px, py);
            if (want >= 0 && fabsf(py - pts[want][1]) > fabsf(pts[0][1] - pts[1][1])) want = -1;
            if (g_log) {
                static int last = -2;
                if (want != last) LOG("[pointer] window: pointer %.0f,%.0f over %d of %d (first at y %.0f), selected %d\n", px, py,
                                      want, n, pts[0][1], sel);
                last = want;
            }
            if (want >= 0 && want != sel) {
                uint64_t old = *(uint64_t *)(uintptr_t)(items + 8 * (uint64_t)sel), now = *(uint64_t *)(uintptr_t)(items + 8 * (uint64_t)want);
                guest_fn(c, START_ACTION, old + 0x10, STR_WAIT, 0);
                *(int32_t *)(uintptr_t)(self + 0x134) = want;
                guest_fn(c, START_ACTION, now + 0x10, STR_SELECT, 0);
                // the cursor: the new button's "Cursor" pane, as a vector on the guest stack for setPaneLocalTrans
                Cpu saved = *c;
                c->sp = (c->sp - 0x200) & ~0xFull;
                uint64_t vec = c->sp;
                memset((void *)(uintptr_t)vec, 0, 16);
                c->x[0] = vec, c->x[1] = now + 8, c->x[2] = STR_CURSOR;
                guest_call(c, CALC_PANE_TRANS);
                c->x[0] = self + 8, c->x[1] = STR_PAR_CURSOR, c->x[2] = vec;
                guest_call(c, SET_PANE_LOCAL_TRANS);
                *c = saved;
            }
        }
    }
    g_confirm(c);
}

// ---- MapLayout (the map, Minus): its Wait nerve runs while the map is up (its lists too). A does nothing there, so a
// bound left click keeps its binding on the map (#90: a left click bound to Y opens the map's lists, as before the
// mouse in menus took bound buttons; hle/input.c). Nerve found through RTTI (MapLayoutNrvWait, 1.4.1), by its code in 1.0.0.
static GuestFn g_map_wait;
static _Atomic uint64_t g_map_seen_ns;
static void map_wait(Cpu *c) {
    atomic_store(&g_map_seen_ns, plat_now_ns());
    g_map_wait(c);
}
bool odyssey_map_open(void) {
    uint64_t t = atomic_load(&g_map_seen_ns);
    return t && plat_now_ns() - t < 200000000ull;
}

// ---- (development) ODYSSEY_POINTER_NERVES=<file>: lines "<execute address hex>:<name>" - every listed nerve hooked,
// and each logged when it starts running (a state change) with its host: which object runs which screen.
#define NPROBE 320
static struct { uint64_t addr, host, frame; GuestFn orig; char name[80]; } g_probe[NPROBE];
static int g_nprobe;
static void probe_run(Cpu *c, int k) {
    extern uint64_t renderer_present_count(void);
    uint64_t host = c->x[1] ? *(uint64_t *)(uintptr_t)c->x[1] : 0, f = renderer_present_count();
    if (host != g_probe[k].host || f > g_probe[k].frame + 10)  // (a new host, or running again after a pause)
        LOG("[nerve] frame %llu %s host %llx\n", (unsigned long long)f, g_probe[k].name, (unsigned long long)host);
    g_probe[k].host = host, g_probe[k].frame = f;
    g_probe[k].orig(c);
}
#define PR(n) static void pr_##n(Cpu *c) { probe_run(c, 0##n - 01000); }  /* (the digits, as octal: 0 .. 319) */
#define PR8(n) PR(n##0) PR(n##1) PR(n##2) PR(n##3) PR(n##4) PR(n##5) PR(n##6) PR(n##7)
#define PR64(n) PR8(n##0) PR8(n##1) PR8(n##2) PR8(n##3) PR8(n##4) PR8(n##5) PR8(n##6) PR8(n##7)
PR64(10) PR64(11) PR64(12) PR64(13) PR64(14)
#define PF(n) pr_##n
#define PF8(n) PF(n##0), PF(n##1), PF(n##2), PF(n##3), PF(n##4), PF(n##5), PF(n##6), PF(n##7)
#define PF64(n) PF8(n##0), PF8(n##1), PF8(n##2), PF8(n##3), PF8(n##4), PF8(n##5), PF8(n##6), PF8(n##7)
static void (*const k_pr[NPROBE])(Cpu *) = {PF64(10), PF64(11), PF64(12), PF64(13), PF64(14)};
static void probe_boot(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { LOG("[nerve] %s: can't read\n", path); return; }
    char line[200];
    while (g_nprobe < NPROBE && fgets(line, sizeof line, f)) {
        unsigned long long a = 0;
        char name[80] = "";
        if (sscanf(line, "%llx:%79s", &a, name) != 2) continue;
        GuestFn o = dispatch_lookup(a);
        if (!o) continue;
        bool dup = false;
        for (int i = 0; i < g_nprobe; i++) dup |= g_probe[i].addr == a;
        if (dup) continue;  // (nerves sharing one execute)
        g_probe[g_nprobe].addr = a, g_probe[g_nprobe].orig = o;
        snprintf(g_probe[g_nprobe].name, sizeof g_probe[g_nprobe].name, "%s", name);
        hle_override(a, k_pr[g_nprobe], "nerve probe");
        g_nprobe++;
    }
    fclose(f);
    LOG("[nerve] probing %d nerves\n", g_nprobe);
}

void odyssey_pointer_boot(void) {
    static bool done;
    if (done) return;
    done = true;
    g_a = loader_main_is_v141() ? &k_v141 : &k_v100;
    if (g_log < 0) g_log = getenv("ODYSSEY_POINTER_PROBE") != NULL;
    if (getenv("ODYSSEY_POINTER_NERVES")) { probe_boot(getenv("ODYSSEY_POINTER_NERVES")); return; }
    if ((g_select_mode = dispatch_lookup(g_a->nrv_select_mode))) hle_override(g_a->nrv_select_mode, select_mode, "menu pointer: select mode");
    // (1.0.0: two nerves, each "ldr x0, [x1]; b exeSelect" - either's original runs exeSelect for the other too)
    for (int i = 0; i < 2 && g_a->nrv_menu_select[i]; i++) {
        GuestFn f = dispatch_lookup(g_a->nrv_menu_select[i]);
        if (!f) continue;
        if (!g_menu_select) g_menu_select = f;
        hle_override(g_a->nrv_menu_select[i], menu_select, "menu pointer: pause menu");
    }
    if ((g_vertical_list = dispatch_lookup(g_a->nrv_vertical_list)))
        hle_override(g_a->nrv_vertical_list, vertical_list, "menu pointer: vertical list");
    if ((g_confirm_data = dispatch_lookup(g_a->nrv_confirm_data))) hle_override(g_a->nrv_confirm_data, confirm_data, "menu pointer: data window");
    if ((g_confirm = dispatch_lookup(g_a->nrv_confirm))) hle_override(g_a->nrv_confirm, confirm, "menu pointer: confirm window");
    if ((g_map_wait = dispatch_lookup(g_a->nrv_map_wait))) hle_override(g_a->nrv_map_wait, map_wait, "menu mouse: map");
    if (g_log) LOG("[pointer] menus (%s): select mode %s, pause menu %s, vertical list %s, data window %s, window %s, map %s\n",
                   loader_main_is_v141() ? "1.4.1" : "1.0.0", g_select_mode ? "hooked" : "not found", g_menu_select ? "hooked" : "not found",
                   g_vertical_list ? "hooked" : "not found", g_confirm_data ? "hooked" : "not found",
                   g_confirm ? "hooked" : "not found", g_map_wait ? "hooked" : "not found");
}
