// Control remapping editor (runtime/input_bind.h), shared by the in-game menu's Controls tab (menu.mm) and the start
// screen's Controls page (launcher.mm): a table of Switch inputs with the controller button and the key bound to
// each. Select a binding, then press the new button / key (Esc or 5 s without a press cancels; a button already in
// use swaps over). Drawn in whichever ImGui context is current.
#include <SDL3/SDL.h>
#include <stdio.h>

#include "imgui.h"
#include "theme.h"
#include "i18n.h"

extern "C" {
#include "../input_bind.h"
#include "../input_players.h"
#include "../settings.h"
}

static void cell(int dev, int slot, int fam, int cap_dev, int cap_slot, int ms_left) {
    char lbl[96];
    bool waiting = cap_dev == dev && cap_slot == slot;
    if (waiting) snprintf(lbl, sizeof lbl, "%s  %d##%d_%d", tr(dev == BIND_DEV_PAD ? "Press a button (or a key)..." : "Press a key or mouse button..."),
                          (ms_left + 999) / 1000, dev, slot);
    else snprintf(lbl, sizeof lbl, "%s##%d_%d", dev == BIND_DEV_PAD ? bind_pad_label(g_bind_pad[slot], fam) :
                                                 bind_key_label(g_bind_key[slot]), dev, slot);
    // changed from the default: light red text; waiting for the press: the accent; the same button / key on two inputs
    // (a hand-edited settings.ini): amber
    const int16_t *t = dev == BIND_DEV_PAD ? g_bind_pad : g_bind_key;
    int n = dev == BIND_DEV_PAD ? BIND_PAD_COUNT : BIND_KEY_COUNT, pushed = 0;
    bool dup = false;
    for (int i = 0; i < n; i++) dup |= i != slot && t[i] == t[slot] && t[slot] > (dev == BIND_DEV_PAD ? -1 : 0);
    if (waiting) { ImGui::PushStyleColor(ImGuiCol_Button, kAccentDeepV); pushed++; }
    else if (dup) { ImGui::PushStyleColor(ImGuiCol_Button, kWarnV); pushed++; }
    if (!bind_is_default_slot(dev, slot)) { ImGui::PushStyleColor(ImGuiCol_Text, kAccentSoftV); pushed++; }
    if (ImGui::Button(lbl, ImVec2(-FLT_MIN, 0))) {
        if (waiting) bind_capture_cancel();
        else bind_capture_begin(dev, slot);
    }
    ImGui::PopStyleColor(pushed);
    if (waiting && !ImGui::IsItemVisible()) ImGui::SetScrollHereY(0.5f);  // (a capture started by a test hook)
}

// The extra actions (shake, spin throw, screenshot): a controller button and a key each, "click, then press" as above
// (Delete / Backspace clears one). Their buttons and keys no longer also press the game button they're bound to.
extern "C" void controls_actions_draw(void) {
    int fam = bind_label_family();
    int cd, cs, ms;
    bind_capture_active(&cd, &cs, &ms);
    static int seen_serial = -1;
    static char msg[160];
    int rdev, rslot, rother, serial;
    int kind = bind_last_result(&rdev, &rslot, &rother, &serial);
    if (seen_serial < 0) seen_serial = serial;
    if (serial != seen_serial) {
        seen_serial = serial;
        if (rdev >= BIND_DEV_ACT_PAD && rslot >= 0) {
            int pv = *bind_action_pad(rslot), kv = *bind_action_key(rslot);
            const char *now = rdev == BIND_DEV_ACT_PAD ? (pv >= 0 ? bind_pad_label(pv, fam) : tr("None")) : (kv > 0 ? bind_key_label(kv) : tr("None"));
            if (kind == 1) snprintf(msg, sizeof msg, tr("%s is now %s."), tr(bind_action_name(rslot)), now), settings_save();
            else if (kind == 3) snprintf(msg, sizeof msg, tr("Cancelled: %s unchanged."), tr(bind_action_name(rslot)));
            else if (kind == 4) snprintf(msg, sizeof msg, tr("Nothing pressed: %s unchanged."), tr(bind_action_name(rslot)));
        }
    }
    if (msg[0]) ImGui::TextColored(kAccentSoftV, "%s", msg);
    if (!ImGui::BeginTable("##actions", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) return;
    ImGui::TableSetupColumn(tr("Action"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn(tr("Controller"), ImGuiTableColumnFlags_WidthStretch, 1.3f);
    ImGui::TableSetupColumn(tr("Keyboard"), ImGuiTableColumnFlags_WidthStretch, 1.3f);
    ImGui::TableHeadersRow();
    for (int act = 0; act < BIND_ACT_COUNT; act++) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr(bind_action_name(act)));
        for (int dev = BIND_DEV_ACT_PAD; dev <= BIND_DEV_ACT_KEY; dev++) {
            ImGui::TableNextColumn();
            bool waiting = cd == dev && cs == act;
            char lbl[96];
            int pv = *bind_action_pad(act), kv = *bind_action_key(act);
            if (waiting) snprintf(lbl, sizeof lbl, "%s  %d##a%d_%d", tr("Press a button or key (Delete clears)..."), (ms + 999) / 1000, dev, act);
            else snprintf(lbl, sizeof lbl, "%s##a%d_%d", dev == BIND_DEV_ACT_PAD ? (pv >= 0 ? bind_pad_label(pv, fam) : tr("None"))
                                                                            : (kv > 0 ? bind_key_label(kv) : tr("None")), dev, act);
            if (waiting) ImGui::PushStyleColor(ImGuiCol_Button, kAccentDeepV);
            if (ImGui::Button(lbl, ImVec2(-FLT_MIN, 0))) {
                if (waiting) bind_capture_cancel();
                else bind_capture_begin(dev, act);
            }
            if (waiting) ImGui::PopStyleColor();
        }
    }
    ImGui::EndTable();
}

extern "C" void controls_bindings_draw(float scale) {
    (void)scale;
    int fam = bind_label_family();
    static const char *const fam_name[] = {"Nintendo", "Xbox", "PlayStation"};
    int n = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&n);
    const char *pad_name = SDL_GetGamepadNameForID(input_primary_id());
    char line[200];
    if (n > 0) snprintf(line, sizeof line, tr("Controller: %s  (%s button names)"), pad_name ? pad_name : tr("connected"), fam_name[fam % 3]);
    else snprintf(line, sizeof line, tr("No controller connected  (%s button names shown)"), fam_name[fam % 3]);
    SDL_free(ids);
    ImGui::TextUnformatted(line);
    {
        uint32_t selected = input_cappy_id();
        const char *name = selected ? SDL_GetGamepadNameForID(selected) : NULL;
        ImGui::BeginDisabled(!input_primary_id());
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr("Cappy controller"));
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.45f);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##cappy_controller", name ? name : tr("Not assigned"))) {
            if (ImGui::Selectable(tr("Not assigned"), !selected)) input_cappy_assign(0);
            int count = 0;
            SDL_JoystickID *pads = SDL_GetGamepads(&count);
            for (int i = 0; i < count; i++) {
                if (pads[i] == input_primary_id()) continue;
                const char *label = SDL_GetGamepadNameForID(pads[i]);
                char item[256];
                snprintf(item, sizeof item, "%s##cappy-%u", label ? label : "Controller", pads[i]);
                if (ImGui::Selectable(item, pads[i] == selected)) input_cappy_assign(pads[i]);
            }
            SDL_free(pads);
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s", tr("Assign a second controller here, then choose Two Players in the game's pause menu. "
                                   "Mario keeps the current controller and keyboard. Cappy uses this controller's buttons "
                                   "and sticks; secondary motion controls are not available yet. Assignment lasts for this session."));
    }
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("%s", tr("Select a binding, then press the new button, key, mouse button or wheel. Esc (or 5 seconds) cancels. A button "
                                 "that's already in use swaps with the old one."));
    ImGui::PopTextWrapPos();

    int cd, cs, ms;
    bind_capture_active(&cd, &cs, &ms);
    // a finished capture: say what happened, keep larecomp.ini current
    static int seen_serial = -1;
    static char msg[200];
    int rdev, rslot, rother, serial;
    int kind = bind_last_result(&rdev, &rslot, &rother, &serial);
    if (seen_serial < 0) seen_serial = serial;
    if (serial != seen_serial && rdev < BIND_DEV_ACT_PAD) {  // (the extra actions' results: controls_actions_draw)
        seen_serial = serial;
        const char *now = rslot < 0 ? "" : rdev == BIND_DEV_PAD ? bind_pad_label(g_bind_pad[rslot], fam) : bind_key_label(g_bind_key[rslot]);
        if (kind == 5) snprintf(msg, sizeof msg, "%s", tr(rdev == BIND_DEV_PAD ? "Controller bindings reset to the defaults." : "Keyboard bindings reset to the defaults."));
        else if (kind == 1) snprintf(msg, sizeof msg, tr("%s is now %s."), bind_input_name(rslot), now);
        else if (kind == 2)
            snprintf(msg, sizeof msg, tr("%s is now %s; %s moved to %s (swapped)."), bind_input_name(rslot), now,
                     bind_input_name(rother), rdev == BIND_DEV_PAD ? bind_pad_label(g_bind_pad[rother], fam) : bind_key_label(g_bind_key[rother]));
        else if (kind == 3) snprintf(msg, sizeof msg, tr("Cancelled: %s unchanged."), bind_input_name(rslot));
        else if (kind == 4) snprintf(msg, sizeof msg, tr("Nothing pressed: %s unchanged."), bind_input_name(rslot));
        if (kind == 1 || kind == 2 || kind == 5) settings_save();
    }
    if (msg[0]) ImGui::TextColored(kAccentSoftV, "%s", msg);

    if (ImGui::BeginTable("##binds", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn(tr("Switch"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn(tr("Controller"), ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableSetupColumn(tr("Keyboard"), ImGuiTableColumnFlags_WidthStretch, 1.3f);
        ImGui::TableHeadersRow();
        // in the order a player thinks of them: face buttons, shoulders, Plus / Minus, d-pad, sticks
        static const int order[] = {BIND_A, BIND_B, BIND_X, BIND_Y, BIND_L, BIND_R, BIND_ZL, BIND_ZR, BIND_PLUS,
                                    BIND_MINUS, BIND_UP, BIND_DOWN, BIND_LEFT, BIND_RIGHT, BIND_LSTICK, BIND_RSTICK,
                                    BIND_LS_UP, BIND_LS_DOWN, BIND_LS_LEFT, BIND_LS_RIGHT, BIND_RS_UP, BIND_RS_DOWN,
                                    BIND_RS_LEFT, BIND_RS_RIGHT};
        for (int slot : order) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(bind_input_name(slot));
            ImGui::TableNextColumn();
            if (slot < BIND_PAD_COUNT) cell(BIND_DEV_PAD, slot, fam, cd, cs, ms);
            else ImGui::TextDisabled("%s", tr(slot < BIND_RS_UP ? "Left stick" : "Right stick"));
            ImGui::TableNextColumn();
            cell(BIND_DEV_KEY, slot, fam, cd, cs, ms);
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::BeginDisabled(bind_is_default(BIND_DEV_PAD));
    if (ImGui::Button(tr("Reset controller to defaults"))) { bind_capture_cancel(); bind_reset(BIND_DEV_PAD); }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(bind_is_default(BIND_DEV_KEY));
    if (ImGui::Button(tr("Reset keyboard to defaults"))) { bind_capture_cancel(); bind_reset(BIND_DEV_KEY); }
    ImGui::EndDisabled();
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("%s", tr("Bindings apply to the game only. Menus keep a fixed layout (A / Cross or Enter selects, "
                                 "B / Circle or Esc goes back), and the in-game menu always opens with F1 or Esc, or "
                                 "the controller's Back + Start held together, or its Home / Guide button - so this page "
                                 "can't be locked out. The game's button icons follow the controller bindings."));
    ImGui::PopTextWrapPos();
}
