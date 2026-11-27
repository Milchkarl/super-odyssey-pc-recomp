// amiibo (nn::nfp, Odyssey). The SDK's client talks to the nfp:user service through an interface object that its
// Initialize never got (no such service here): nn::nfp::Initialize crashed on a null pointer. Replaced at the API
// level.
//
// A virtual amiibo (settings amiibo, Controls: "amiibo"; #48): no NFC reader and no amiibo file - the game only asks
// which figure was touched (GetModelInfo: character, variant, model number, series), so a figure chosen in the menu is
// answered from a small table. With "None" the console has no NFC device, as before (ListDevices finds none).
// The game's side (al::NfpDirector / al::NfpControllerThread): ListDevices, GetNpadId, AttachActivateEvent /
// AttachDeactivateEvent, StartDetection; it then waits for the activate event with the device in TagFound, Mounts,
// reads GetTagInfo and GetModelInfo, and waits for the tag to leave (the deactivate event, TagRemoved) before it
// searches again. The game searches only while its "touch an amiibo" prompt is up (D-pad Right held in play, or an
// amiibo question from Uncle amiibo): the chosen figure is "touched" 0.7 s into each search, stays a moment after it
// was read, and is lifted. ODYSSEY_AMIIBO=<1..> overrides the setting, ODYSSEY_AMIIBO_ID=<16 hex digits> the figure's id (tests).
// (Waluigi's character id was 0x0018 until 2026-11-27: the game took him for an unknown figure, #164; 0x0014 gives the
// same reaction as the other Mario figures - tested in the Moon Kingdom.)
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "../hle.h"
#include "../kernel.h"
#include "../settings.h"

#define NFP_DEVICE_NOT_FOUND MAKE_RESULT(115, 64)
#define NFP_WRONG_STATE MAKE_RESULT(115, 73)

enum { ST_INIT, ST_SEARCHING, ST_FOUND, ST_REMOVED, ST_MOUNTED };

// The figures offered (amiibo id: character 2 bytes, variant, figure type | model number 2 bytes, series, 02)
typedef struct { const char *name; uint8_t id[8]; } Figure;
static const Figure k_figures[] = {
    {"None", {0}},
    {"Mario (Wedding)", {0x00, 0x00, 0x00, 0x00, 0x03, 0x71, 0x01, 0x02}},
    {"Peach (Wedding)", {0x00, 0x02, 0x00, 0x00, 0x03, 0x72, 0x01, 0x02}},
    {"Bowser (Wedding)", {0x00, 0x05, 0x00, 0x00, 0x03, 0x73, 0x01, 0x02}},
    {"Mario", {0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x01, 0x02}},
    {"Luigi", {0x00, 0x01, 0x00, 0x00, 0x00, 0x35, 0x01, 0x02}},
    {"Peach", {0x00, 0x02, 0x00, 0x00, 0x00, 0x36, 0x01, 0x02}},
    {"Bowser", {0x00, 0x05, 0x00, 0x00, 0x00, 0x39, 0x01, 0x02}},
    {"Yoshi", {0x00, 0x03, 0x00, 0x00, 0x00, 0x37, 0x01, 0x02}},
    {"Wario", {0x00, 0x07, 0x00, 0x00, 0x00, 0x3c, 0x01, 0x02}},
    {"Waluigi", {0x00, 0x14, 0x00, 0x00, 0x00, 0xf8, 0x01, 0x02}},
    {"Diddy Kong", {0x00, 0x09, 0x00, 0x00, 0x00, 0x15, 0x00, 0x02}},
    {"Dr. Mario", {0x00, 0x00, 0x01, 0x00, 0x00, 0x16, 0x00, 0x02}},
    {"Link (another series: a hint)", {0x01, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02}},
};
enum { NFIG = sizeof k_figures / sizeof *k_figures };
int nfp_figure_count(void) { return NFIG; }
const char *nfp_figure_name(int i) { return i >= 0 && i < NFIG ? k_figures[i].name : "?"; }

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int state = ST_INIT;
static KEvent *ev_on, *ev_off;
static uint32_t on_w, on_r, off_w, off_r;
static uint64_t search_ns, read_ns;   // when the search began / the figure was read (0: not)
static _Atomic bool touch_asked;
static int touched;                    // the figure on the reader (index)

static int figure(void) {
    static int env = -1;
    if (env < 0) { const char *e = getenv("ODYSSEY_AMIIBO"); env = e ? atoi(e) : 0; }
    int f = env > 0 ? env : g_settings.amiibo;
    return f > 0 && f < NFIG ? f : 0;
}
bool nfp_searching(void) { return figure() && state == ST_SEARCHING; }
// The player touches the chosen figure to the reader (the menu's button)
void nfp_touch(void) { if (figure()) atomic_store(&touch_asked, true); }

// The figure's comings and goings, looked at whenever the game asks the device anything (its thread polls)
static void step(void) {
    uint64_t now = plat_now_ns();
    // (the game searches while its "touch an amiibo" prompt is up - D-pad Right held: the figure comes 0.7 s in)
    if (state == ST_SEARCHING && figure() &&
        (atomic_exchange(&touch_asked, false) || (search_ns && now - search_ns > 700000000ull))) {
        state = ST_FOUND;
        touched = figure();
        read_ns = 0;
        LOG("[amiibo] %s touched\n", k_figures[touched].name);
        if (ev_on) { pthread_mutex_lock(&g_klock); kernel_signal(ev_on); pthread_mutex_unlock(&g_klock); }
    } else if (state == ST_MOUNTED && read_ns && now - read_ns > 1500000000ull) {
        state = ST_REMOVED;  // (lifted off the reader a moment after it was read)
        LOG("[amiibo] lifted\n");
        if (ev_off) { pthread_mutex_lock(&g_klock); kernel_signal(ev_off); pthread_mutex_unlock(&g_klock); }
    }
    if (state != ST_SEARCHING) atomic_store(&touch_asked, false);
}

HLE_FUNC("_ZN2nn3nfp10InitializeEv", nfp_initialize) { c->x[0] = RES_OK; }
HLE_FUNC("_ZN2nn3nfp8FinalizeEv", nfp_finalize) { c->x[0] = RES_OK; }

// ListDevices(DeviceHandle *out, int *count, int max) -> Result
HLE_FUNC("_ZN2nn3nfp11ListDevicesEPNS0_12DeviceHandleEPii", nfp_list_devices) {
    int n = figure() && (int)c->x[2] > 0 ? 1 : 0;
    if (n && c->x[0]) *(uint64_t *)(uintptr_t)c->x[0] = 0;  // (the handle: player 1's controller)
    if (c->x[1]) *(int32_t *)(uintptr_t)c->x[1] = n;
    static int logged = -1;
    if (logged != n) LOG("[amiibo] ListDevices: %d device(s)\n", n), logged = n;
    c->x[0] = RES_OK;
}

// GetDeviceState(const DeviceHandle &) -> DeviceState
HLE_FUNC("_ZN2nn3nfp14GetDeviceStateERKNS0_12DeviceHandleE", nfp_get_device_state) {
    pthread_mutex_lock(&lock);
    step();
    c->x[0] = (uint64_t)state;
    pthread_mutex_unlock(&lock);
}

HLE_FUNC("_ZN2nn3nfp14StartDetectionERKNS0_12DeviceHandleE", nfp_start_detection) {
    if (!figure()) { c->x[0] = NFP_DEVICE_NOT_FOUND; return; }
    pthread_mutex_lock(&lock);
    state = ST_SEARCHING;
    search_ns = plat_now_ns();
    atomic_store(&touch_asked, false);
    pthread_mutex_unlock(&lock);
    LOG("[amiibo] the game searches for a figure\n");
    c->x[0] = RES_OK;
}
HLE_FUNC("_ZN2nn3nfp13StopDetectionERKNS0_12DeviceHandleE", nfp_stop_detection) {
    pthread_mutex_lock(&lock);
    if (state != ST_INIT) LOG("[amiibo] the game stopped searching\n");
    state = ST_INIT;
    search_ns = 0;
    pthread_mutex_unlock(&lock);
    c->x[0] = RES_OK;
}
HLE_FUNC("_ZN2nn3nfp5MountERKNS0_12DeviceHandleENS0_9ModelTypeENS0_11MountTargetE", nfp_mount) {
    pthread_mutex_lock(&lock);
    bool ok = state == ST_FOUND || state == ST_MOUNTED;
    if (ok) state = ST_MOUNTED;
    pthread_mutex_unlock(&lock);
    c->x[0] = ok ? RES_OK : NFP_WRONG_STATE;
}
HLE_FUNC("_ZN2nn3nfp7UnmountERKNS0_12DeviceHandleE", nfp_unmount) {
    pthread_mutex_lock(&lock);
    if (state == ST_MOUNTED) state = ST_FOUND;
    pthread_mutex_unlock(&lock);
    c->x[0] = RES_OK;
}
// TagInfo (0x58): u8 uuid[10], u8 uuid length, 0x15 reserved, u32 protocol, u32 tag type, 0x30 reserved
HLE_FUNC("_ZN2nn3nfp10GetTagInfoEPNS0_7TagInfoERKNS0_12DeviceHandleE", nfp_get_tag_info) {
    pthread_mutex_lock(&lock);
    bool ok = state == ST_FOUND || state == ST_MOUNTED;
    if (ok) {
        uint8_t *t = (uint8_t *)(uintptr_t)c->x[0];
        memset(t, 0, 0x58);
        static const uint8_t uuid[7] = {0x04, 0x53, 0x4f, 0x50, 0x52, 0x00, 0x00};
        memcpy(t, uuid, 7);
        t[6] = (uint8_t)touched;  // (a different tag per figure: the game counts the figures it has seen)
        t[10] = 7;
        *(uint32_t *)(t + 0x20) = 1;  // protocol: type A
        *(uint32_t *)(t + 0x24) = 2;  // tag type 2 (NTAG215)
    }
    pthread_mutex_unlock(&lock);
    c->x[0] = ok ? RES_OK : NFP_WRONG_STATE;
}
// ModelInfo (0x40): u16 character id, u8 variant, u8 figure type, u16 model number, u8 series, u8 tag type, reserved
HLE_FUNC("_ZN2nn3nfp12GetModelInfoEPNS0_9ModelInfoERKNS0_12DeviceHandleE", nfp_get_model_info) {
    pthread_mutex_lock(&lock);
    bool ok = state == ST_MOUNTED || state == ST_FOUND;
    if (ok) {
        uint8_t *m = (uint8_t *)(uintptr_t)c->x[0];
        const uint8_t *id = k_figures[touched].id;
        uint8_t over[8];  // (ODYSSEY_AMIIBO_ID=<16 hex digits>, tests: that id instead of the table's)
        const char *oe = getenv("ODYSSEY_AMIIBO_ID");
        if (oe && strlen(oe) == 16) {
            for (int i = 0; i < 8; i++) { char b[3] = {oe[2 * i], oe[2 * i + 1], 0}; over[i] = (uint8_t)strtoul(b, NULL, 16); }
            id = over;
        }
        memset(m, 0, 0x40);
        m[0] = id[0], m[1] = id[1], m[2] = id[2], m[3] = id[3];
        m[4] = id[5], m[5] = id[4];  // (the model number as a number: little endian here)
        m[6] = id[6], m[7] = id[7];
        read_ns = plat_now_ns();
        LOG("[amiibo] the game read %s\n", k_figures[touched].name);
    }
    pthread_mutex_unlock(&lock);
    c->x[0] = ok ? RES_OK : NFP_WRONG_STATE;
}
HLE_FUNC("_ZN2nn3nfp9GetNpadIdEPjRKNS0_12DeviceHandleE", nfp_get_npad_id) {
    if (!figure()) { c->x[0] = NFP_DEVICE_NOT_FOUND; return; }
    *(uint32_t *)(uintptr_t)c->x[0] = 0;  // (player 1)
    c->x[0] = RES_OK;
}

// AttachActivateEvent / AttachDeactivateEvent(SystemEventType *, const DeviceHandle &): signalled when a figure comes / goes
static void attach(Cpu *c, KEvent **ev, uint32_t *w, uint32_t *r) {
    if (!figure()) { c->x[0] = NFP_DEVICE_NOT_FOUND; return; }
    pthread_mutex_lock(&lock);
    if (!*ev) *ev = kernel_create_event_pair(w, r);
    else *w = kernel_new_handle(*ev), *r = kernel_new_handle(*ev);
    uint32_t rd = *r;
    pthread_mutex_unlock(&lock);
    // nn::os::AttachReadableHandleToSystemEvent(event, handle, managed = false, clear mode = AutoClear)
    c->x[1] = rd;
    c->x[2] = 0;
    c->x[3] = 1;
    guest_call(c, guest_sym("_ZN2nn2os33AttachReadableHandleToSystemEventEPNS0_15SystemEventTypeEjbNS0_14EventClearModeE"));
    c->x[0] = RES_OK;
}
HLE_FUNC("_ZN2nn3nfp19AttachActivateEventEPNS_2os15SystemEventTypeERKNS0_12DeviceHandleE", nfp_attach_activate) {
    attach(c, &ev_on, &on_w, &on_r);
}
HLE_FUNC("_ZN2nn3nfp21AttachDeactivateEventEPNS_2os15SystemEventTypeERKNS0_12DeviceHandleE", nfp_attach_deactivate) {
    attach(c, &ev_off, &off_w, &off_r);
}

// Reboot: the kernel's handle table is new; the events are made again when the game attaches them
RT_ON_RESET(nfp_reset) {
    ev_on = ev_off = NULL;
    on_w = on_r = off_w = off_r = 0;
    state = ST_INIT;
    search_ns = read_ns = 0;
}
