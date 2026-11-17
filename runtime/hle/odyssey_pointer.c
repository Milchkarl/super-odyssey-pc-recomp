// The mouse pointer in the game's menus (#64, prototype; 1.4.1 only). The game's menus know no pointer: each one keeps
// its own selection and moves it on the D-pad. For a menu whose items we can find, the pointer over an item that
// isn't selected becomes the selection at once, set the way the game's own D-pad step sets it (its index, then the
// items' "Wait" / "Select" actions and the menu's update), and a left click is its A (the mouse in menus). Where the
// items are comes from the game itself: al::calcPaneTrans (the "Cursor" pane each item has, where the game puts its cursor), in the
// layouts' 1280 x 720 space, as the pointer (input_pointer_layout).
//
// Menus, by their state's nerve (RTTI: the nerve class's vtable, slot 0 = execute(NerveKeeper *), host at [keeper]):
//   SceneStateSelectMode, its select nerve (new game: Start / Assist Mode): items at +0x30 (selected while the byte
//   at +0x59 is 1) and +0x38; 0x700046ae80(this, instant) updates their look (disassembly of 1.4.1).
//   MenuSelectParts, its Select nerve (the pause menu): below.
// Not yet: the cursor sound on a pointer move, and every other menu (options, shops, the map: click and wheel only).
// ODYSSEY_POINTER_PROBE=1 logs the hooked menus and the pointer's choices.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../hle.h"
#include "../rt.h"

bool input_pointer_layout(float *x, float *y);  // runtime/hle/input.c

#define CALC_PANE_TRANS 0x7000630b60ull  // al::calcPaneTrans(sead::Vector3f *out, const al::IUseLayout *, const char *pane)
#define STR_CURSOR 0x7000e846f5ull       // "Cursor" (the game's own string)
#define START_ACTION 0x700062f120ull     // al::startAction(IUseLayoutAction *, const char *action, const char *pane)
#define STR_WAIT 0x7000e75a89ull         // "Wait"
#define STR_SELECT 0x7000e846eeull       // "Select"
#define MENU_SELECT_UPDATE 0x70001d7290ull  // MenuSelectParts: what exeSelect calls after a step (the cursor's place)
#define SELECT_MODE_UPDATE 0x700046ae80ull  // SceneStateSelectMode: the items' look from +0x59 (this, instant)
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
// at +0x28 [order[index]], the order table chosen by the byte at +0x58 (0: 0x7000ec8288, else 0x7000ec8270)
static GuestFn g_menu_select;
static void menu_select(Cpu *c) {
    uint64_t self = *(uint64_t *)(uintptr_t)c->x[1];
    float px, py;
    if (self && input_pointer_layout(&px, &py)) {
        int n = *(int32_t *)(uintptr_t)(self + 0x18), sel = *(int32_t *)(uintptr_t)(self + 0x1c);
        uint64_t parts = *(uint64_t *)(uintptr_t)(self + 0x28);
        const int32_t *order = (const int32_t *)(uintptr_t)(*(uint8_t *)(uintptr_t)(self + 0x58) ? 0x7000ec8270ull : 0x7000ec8288ull);
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

void odyssey_pointer_boot(void) {
    static bool done;
    if (done || !loader_main_is_v141()) return;
    done = true;
    if (g_log < 0) g_log = getenv("ODYSSEY_POINTER_PROBE") != NULL;
    const uint64_t select_mode_nerve = 0x700046b580ull;  // SceneStateSelectMode's select nerve: execute
    if ((g_select_mode = dispatch_lookup(select_mode_nerve))) hle_override(select_mode_nerve, select_mode, "menu pointer: select mode");
    const uint64_t menu_select_nerve = 0x70001d7a20ull;  // MenuSelectParts's Select nerve: execute
    if ((g_menu_select = dispatch_lookup(menu_select_nerve))) hle_override(menu_select_nerve, menu_select, "menu pointer: pause menu");
    if (g_log) LOG("[pointer] menus: select mode %s, pause menu %s\n", g_select_mode ? "hooked" : "not found",
                   g_menu_select ? "hooked" : "not found");
}
