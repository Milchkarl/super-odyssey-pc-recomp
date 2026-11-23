// Two-pane settings pages (see option_rows.h): rows on the left, the selected / hovered row's description on the right.
#include <set>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"  // (SetKeyOwner, NavCursorVisible)

#include "option_rows.h"
#include "option_notes.h"
#include "../rt.h"  // (LOG)
#include "i18n.h"
#include "osk.h"
#include "theme.h"
extern "C" void menu_rec(const char *fmt, ...);  // (menu.mm: the menu recorder)

extern "C" float renderer_picture_aspect(void);  // (runtime/nvn/vulkan/device.c)
static float g_scale = 1;
static std::string g_desc_hover, g_desc_focus, g_desc_shown;  // this frame's hovered / selected row, and the last shown
static std::string g_desc_first;  // the page's first row (shown until a row is selected or hovered)
static std::string g_title_hover, g_title_focus, g_title_shown, g_title_first;  // their rows' labels (the card's title)
static int g_row_toggle = -1;  // (opt_toggle -> row: the toggle's state)
static float g_row_slider = -1;  // (opt_int_slider -> row: the value's place on its track, 0..1; -1 not a slider)
static bool g_next_dropdown;     // (opt_next_dropdown -> the next opt_choice / opt_choice_avail: a dropdown list)
static std::string g_row_note;   // (opt_next_note -> the next row: a line under its description, already in the menu's language)
void opt_next_note(const char *note) { g_row_note = note ? note : ""; }
static bool g_row_chevron;       // (-> row: a dropdown's chevron in the box instead of the arrows)
static const char *g_row_seg[2]; // (opt_choice with two choices -> row: drawn as a switch with their names, like On | Off)
static ImVec2 g_row_min, g_row_max;  // (the last row's rectangle, before the gap under it: dropdown_list)
static int g_dropdown_frame = -10;   // (the last frame a dropdown's list was open)
// (B / Esc closing a dropdown's list closes it before the menu looks: the menu leaves that B / Esc alone)
bool opt_dropdown_recent(void) { return ImGui::GetFrameCount() - g_dropdown_frame <= 2; }
static float g_slider_drag = -1;  // (row -> opt_int_slider: where the mouse holds the track, 0..1; -1 not dragged)
static std::string g_page, g_page_shown;  // this page's id, and the page g_desc_shown belongs to
static bool g_next_disabled;
void opt_disable_next(void) { g_next_disabled = true; }
static int g_indent;
static bool g_focus_req, g_take_first;  // (opt_focus_first: requested / this page's first row takes it)
void opt_focus_first(bool on) { g_focus_req = on; }
static bool g_held;  // the last row's left / right has been held a while (steppers go faster)
void opt_indent(int step) { g_indent = g_indent + step < 0 ? 0 : g_indent + step; }

static int g_lr_block_until = -1;  // (frame count) left / right don't change a row until then
static bool g_last_row_focused;  // (row) the row drawn last has the cursor: after a page's rows, its last one
void opt_begin(const char *id, float scale) {
    g_scale = scale;
    g_take_first = g_focus_req;
    g_focus_req = false;
    if (g_take_first) {
        ImGui::NavMoveRequestCancel();  // (this frame's Down goes to the first row instead)
        // (the Right / A that came into the page from the sidebar isn't also the first row's change - it flipped
        // "Early frame hand-off" when entering Performance)
        g_lr_block_until = ImGui::GetFrameCount() + 1;
    }
    g_indent = 0;
    g_desc_hover.clear(), g_desc_focus.clear(), g_title_hover.clear(), g_title_focus.clear();
    g_page = id;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::PushID(id);
    // (on an ultrawide screen the rows stop growing - about as wide as the page is tall - and the picture takes the rest)
    ImGui::BeginChild("##rows", ImVec2(fminf(avail.x * 0.52f, avail.y * 1.05f), avail.y), ImGuiChildFlags_None, ImGuiWindowFlags_NavFlattened);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 6 * scale));  // (rows close)
    g_desc_first.clear(), g_title_first.clear();
}

void opt_section(const char *title) {
    std::string t = tr(title);
    for (char &c : t) c = (char)toupper((unsigned char)c);
    (void)t;
    std::string n = tr(title);  // (as written, centred between two hairlines)
    if (ImGui::GetCursorPosY() > ImGui::GetStyle().WindowPadding.y + 1) ImGui::Dummy(ImVec2(0, 10 * g_scale));
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x, fs = ImGui::GetFontSize(), tw = ImGui::CalcTextSize(n.c_str()).x;
    float cy = p.y + fs * 0.55f, mid = p.x + w * 0.5f, g = fs * 0.6f;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(p.x, cy), ImVec2(mid - tw * 0.5f - g, cy), theme_u32(kLineV), fmaxf(1, g_scale));
    dl->AddLine(ImVec2(mid + tw * 0.5f + g, cy), ImVec2(p.x + w, cy), theme_u32(kLineV), fmaxf(1, g_scale));
    dl->AddText(ImVec2(mid - tw * 0.5f, p.y), kTextDim, n.c_str());
    ImGui::Dummy(ImVec2(w, fs * 1.15f));
}

// One row: draws it, handles A / click and left / right while selected. Returns 2 (A / click), -1 / +1 (left / right) or 0.
// arrows: ◀ value ▶ (a choice of several, or a stepper): left / right step it, and the arrows click too
// Look (theme.h): a flat dark row, the label as written; selected / hovered: lighter, with a red bar on its left.
static int row(const char *label_en, const std::string &value_en, bool changed, const char *desc_en, bool upper = false,
               bool arrows = false, bool repeat = false) {
    ImGui::PushID(label_en);  // (the English one: the same row whatever the menu's language)
    // (the menu's language: runtime/ui/i18n.h - label, value and description as shown)
    const char *label = tr(label_en), *desc = tr(desc_en);
    static std::string with_note;  // (opt_next_note: a live line under the description - the memory a choice takes)
    if (!g_row_note.empty()) {
        with_note = g_row_note + (desc && *desc ? std::string("\n\n") + desc : std::string());  // (first: long ones scroll)
        desc = with_note.c_str();
        g_row_note.clear();
    }
    // (the option's plain-English note on top: what it does, what is recommended - option_notes.h)
    std::string noted;
    if (const OptionNote *n = option_note(label_en)) {
        const char *sum = tr(n->summary);  // (left out when the description already starts with it: "The game's volume.")
        if (sum[0] && !(desc && !strncmp(desc, sum, strlen(sum)))) noted = std::string(sum) + "\n";
        noted += std::string(tr("Recommended:")) + " " + tr(n->recommended);
        if (desc && *desc) noted += std::string("\n\n") + desc;
        desc = noted.c_str();
    }
    std::string value = tr(value_en.c_str());
    // "(Recommended)" beside the recommended value (option_notes.h tagged): smaller and dimmer than the value, and
    // dropped - never squeezing the label - where the row has no room for it
    const char *tag = NULL;
    if (const OptionNote *n = option_note(label_en); n && n->tagged && value_en == n->tagged) tag = tr("(Recommended)");
    if (g_indent) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + g_indent * 28 * g_scale);  // (an option of the row above)
    ImVec2 p = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x, h = ImGui::GetFontSize() * 1.9f;
    ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));  // (the row's own border shows the selection)
    bool clicked = ImGui::InvisibleButton("##row", ImVec2(w, h), ImGuiButtonFlags_EnableNav);
    ImGui::PopStyleColor();
    if (g_take_first) {  // (opt_focus_first)
        g_take_first = false;
        ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
        ImGui::SetNavCursorVisible(true);
        ImGui::SetScrollY(0);  // (the page's top: its first section's heading stays in view)
    }
    bool off = g_next_disabled;  // (greyed out: still selectable, so its description shows, but it can't change)
    g_next_disabled = false;
    bool hov = ImGui::IsItemHovered(), foc = ImGui::IsItemFocused() && ImGui::GetCurrentContext()->NavCursorVisible;
    g_last_row_focused = ImGui::IsItemFocused();  // (opt_end: the page's last row - Down stops there)
    bool blocked = ImGui::GetFrameCount() <= g_lr_block_until;
    int step = clicked && !off && !blocked ? 2 : 0;
    g_held = false;
    if (foc && !off && !blocked) {
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft, repeat) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickLeft, repeat) ||
            ImGui::IsKeyPressed(ImGuiKey_LeftArrow, repeat))
            step = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight, repeat) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickRight, repeat) ||
            ImGui::IsKeyPressed(ImGuiKey_RightArrow, repeat))
            step = 1;
        for (ImGuiKey k : {ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickLeft,
                           ImGuiKey_GamepadLStickRight, ImGuiKey_LeftArrow, ImGuiKey_RightArrow})
            if (ImGui::GetKeyData(k)->DownDuration > 1.0f) g_held = true;
    }
    // left / right belong to the selected row - every row: greyed-out ones, buttons, and on the frame it is entered
    // (navigation would otherwise move sideways: with the tabs on top that jumped to a tab or to the far side of the
    // page, owner 2026-11-20)
    if (foc)
        for (ImGuiKey k : {ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickLeft,
                           ImGuiKey_GamepadLStickRight, ImGuiKey_LeftArrow, ImGuiKey_RightArrow})
            ImGui::SetKeyOwner(k, ImGui::GetItemID());
    if (foc) g_desc_focus = desc ? desc : "", g_title_focus = label;
    if (foc) {  // (diagnosis 2026-11-21: the menu opened on its last option on a Steam Deck - which row the cursor lands on)
        static std::string last;
        static int nlog;
        if (last != label && nlog++ < 300) LOG("[menufocus] %s (ImGui frame %d)\n", label, ImGui::GetFrameCount());
        if (last != label) menu_rec("cursor on \"%s\"", label_en);
        last = label;
    }
    if (hov) g_desc_hover = desc ? desc : "", g_title_hover = label;
    if (g_desc_first.empty() && desc) g_desc_first = desc, g_title_first = label;

    ImDrawList *dl = ImGui::GetWindowDrawList();
    // The look (branch menu-odyssey-look): the label on a navy band on the left; the value in a red box on the right,
    // with arrows inside its ends (a choice / stepper) or as an On | Off switch (a toggle); the selected row outlined in
    // coin gold; a small gold corner on the band when the value isn't the default. Rows without a value (buttons) are
    // one band across.
    int tog = g_row_toggle;  // (opt_toggle: 1 On, 0 Off; -1 not a toggle)
    g_row_toggle = -1;
    bool chev = g_row_chevron;  // (a dropdown: a chevron at the box's right end)
    g_row_chevron = false;
    float sl = g_row_slider;  // (opt_int_slider: a track filled to the value, dragged with the mouse)
    g_row_slider = -1;
    g_slider_drag = -1;
    bool has_box = tog >= 0 || !value.empty();
    float r = 5 * g_scale, pad = 18 * g_scale, fs = ImGui::GetFontSize(), gapx = 6 * g_scale;
    float split = has_box ? floorf(w * 0.54f) : w;  // (the band's width; the box takes the rest)
    bool lit = hov || foc;
    ImVec2 b0(p.x + split + (has_box ? gapx : 0), p.y), b1(p.x + w, p.y + h);  // the value box
    float bw = b1.x - b0.x;
    dl->AddRectFilled(p, ImVec2(p.x + split, p.y + h), theme_u32(lit ? kSurfaceHoverV : kSurfaceV), r);
    if (changed && !off) {  // (not the default)
        float t = fs * 0.42f;
        dl->AddTriangleFilled(ImVec2(p.x + 1, p.y + 1), ImVec2(p.x + 1 + t, p.y + 1), ImVec2(p.x + 1, p.y + 1 + t), theme_u32(kTextV, 0.55f));
    }
    std::string t = label;
    if (upper)
        for (char &c : t) c = (char)toupper((unsigned char)c);
    ImU32 label_col = off ? kTextOff : kText;  // (greyed out: clearly dim)
    ImFont *font = ImGui::GetFont();
    // the label fitted into its band: drawn smaller where it doesn't fit (down to 70%), as before (owner 2026-11-16)
    float lw = ImGui::CalcTextSize(t.c_str()).x, lroom = split - 2 * pad, ls = 1;
    if (lw > lroom && lw > 0) {
        ls = fmaxf(0.7f, lroom / lw);
        static std::set<std::string> logged;
        if (logged.insert(t).second)
            LOG("[menu] fitted: \"%s\" (%.0f%%)%s\n", t.c_str(), ls * 100, lw * ls > lroom + 0.5f ? " - STILL OVERLAPS" : "");
    }
    dl->PushClipRect(p, ImVec2(p.x + split - pad * 0.5f, p.y + h), true);
    dl->AddText(font, fs * ls, ImVec2(p.x + pad, p.y + (h - fs * ls) * 0.5f), label_col, t.c_str());
    dl->PopClipRect();
    if (tog >= 0) {  // On | Off: a segmented control - the chosen half lighter with a short red line under its word
        float hw = bw * 0.5f;
        const char *names[2] = {tr("On"), tr("Off")};
        if (g_row_seg[0]) names[0] = tr(g_row_seg[0]), names[1] = tr(g_row_seg[1]);  // (a two-choice row: its names)
        g_row_seg[0] = g_row_seg[1] = NULL;
        dl->AddRectFilled(b0, b1, theme_u32(kSidebarV), r);
        for (int k = 0; k < 2; k++) {
            ImVec2 a(b0.x + k * hw, b0.y), z(b0.x + (k + 1) * hw, b1.y);
            bool chosen = (k == 0) == (tog == 1);
            if (chosen) dl->AddRectFilled(a, z, theme_u32(off ? kSurfaceV : lit ? kBoxHotV : kBoxV), r,
                                          k == 0 ? ImDrawFlags_RoundCornersLeft : ImDrawFlags_RoundCornersRight);
            ImVec2 ts = ImGui::CalcTextSize(names[k]);
            float tx = (a.x + z.x - ts.x) * 0.5f, ty = p.y + (h - fs) * 0.5f;
            dl->AddText(ImVec2(tx, ty), off ? kTextOff : chosen ? kText : theme_u32(kTextDimV, 0.55f), names[k]);
            if (chosen && !off) {
                float lw2 = fmaxf(ts.x, fs * 1.2f), ly = b1.y - fmaxf(3, 4 * g_scale);
                float lx0 = (a.x + z.x - lw2) * 0.5f;
                dl->AddRectFilled(ImVec2(lx0, ly), ImVec2(lx0 + lw2, ly + fmaxf(2, 2.5f * g_scale)), theme_u32(kGoldV), 2 * g_scale);
            }
        }
        if (clicked && !off && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {  // (a click on the chosen half: nothing)
            float mx = ImGui::GetIO().MousePos.x;
            if (mx >= b0.x && ((mx < b0.x + hw) == (tog == 1))) step = 0;
        }
    } else if (has_box && sl >= 0) {  // a slider: the box a track, filled up to the value, a handle there, the value on it
        dl->AddRectFilled(b0, b1, off ? theme_u32(kSurfaceV) : theme_u32(lit ? kBoxHotV : kBoxV), r);
        float m = fs * 0.5f, x0 = b0.x + m, x1 = b1.x - m, f = fminf(fmaxf(sl, 0), 1), hx = x0 + (x1 - x0) * f;
        float ty = p.y + h * 0.74f, th = fmaxf(3, 4 * g_scale);
        dl->AddRectFilled(ImVec2(x0, ty - th * 0.5f), ImVec2(x1, ty + th * 0.5f), theme_u32(kSidebarV), th);
        if (!off) dl->AddRectFilled(ImVec2(x0, ty - th * 0.5f), ImVec2(hx, ty + th * 0.5f), theme_u32(kGoldV, lit ? 1.0f : 0.85f), th);
        dl->AddCircleFilled(ImVec2(hx, ty), fs * 0.32f, off ? kTextOff : kText);
        ImVec2 vs = ImGui::CalcTextSize(value.c_str());
        float vx = (b0.x + b1.x - vs.x) * 0.5f, vy = p.y + h * 0.08f;
        dl->AddText(font, fs * 0.8f, ImVec2(vx + vs.x * 0.1f, vy), off ? kTextOff : kText, value.c_str());
        dl->AddCircle(ImVec2(hx, ty), fs * 0.32f, theme_u32(kSidebarV), 0, fmaxf(1, 1.5f * g_scale));
        // the mouse: held on the track, the value follows it (a press or a drag; the keys and the controller step)
        if (!off && ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            float mx = ImGui::GetIO().MousePos.x, cx = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left].x;
            if (cx >= b0.x) g_slider_drag = fminf(fmaxf((mx - x0) / fmaxf(x1 - x0, 1), 0), 1);
        }
        if (clicked && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) step = 0;  // (the mouse set it already)
    } else if (has_box) {
        dl->AddRectFilled(b0, b1, off ? theme_u32(kSurfaceV) : theme_u32(lit ? kBoxHotV : kBoxV), r);  // (charcoal; red only for the arrows)
        float aw = fs * 0.36f, agap = fs * 0.5f, cy = p.y + h * 0.5f;
        float inner = bw - (arrows ? 2 * (aw + agap * 1.4f) : agap * 2);  // (room for the value and its tag)
        float vw0 = ImGui::CalcTextSize(value.c_str()).x, vs = 1;
        const float tag_s = 0.78f;
        float tagw = tag ? ImGui::CalcTextSize(tag).x * tag_s + fs * 0.35f : 0;
        if (tag && vw0 + tagw > inner) tag = NULL, tagw = 0;  // (no room: the value keeps its size)
        if (vw0 > inner && vw0 > 0) vs = fmaxf(0.7f, inner / vw0);
        if (chev) {  // (a dropdown: the list opens with a click or A; left / right still step)
            ImU32 ac = off ? kTextOff : theme_u32(kTextV, lit ? 0.95f : 0.7f);
            float cx = b1.x - agap - aw;
            dl->AddTriangleFilled(ImVec2(cx - aw * 0.9f, cy - aw * 0.45f), ImVec2(cx + aw * 0.9f, cy - aw * 0.45f),
                                  ImVec2(cx, cy + aw * 0.6f), ac);
        }
        if (arrows) {
            ImU32 ac = off ? kTextOff : theme_u32(kTextV, lit ? 0.95f : 0.7f);  // (the red accent)
            float lx = b0.x + agap, rx = b1.x - agap - aw;
            dl->AddTriangleFilled(ImVec2(lx, cy), ImVec2(lx + aw, cy - aw * 0.8f), ImVec2(lx + aw, cy + aw * 0.8f), ac);
            dl->AddTriangleFilled(ImVec2(rx + aw, cy), ImVec2(rx, cy - aw * 0.8f), ImVec2(rx, cy + aw * 0.8f), ac);
            if (clicked && !off && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
                ImGui::IsItemHovered(ImGuiHoveredFlags_NoNavOverride)) {
                float mx = ImGui::GetIO().MousePos.x;  // (the box's left quarter steps back, its right quarter on)
                if (mx >= b0.x && mx < b0.x + bw * 0.25f) step = -1;
                else if (mx > b1.x - bw * 0.25f) step = 1;
            }
        }
        float tot = vw0 * vs + tagw * vs, vx = (b0.x + b1.x - tot) * 0.5f;
        dl->PushClipRect(ImVec2(b0.x + agap, b0.y), ImVec2(b1.x - agap, b1.y), true);
        dl->AddText(font, fs * vs, ImVec2(vx, p.y + (h - fs * vs) * 0.5f), off ? kTextOff : kText, value.c_str());
        if (tag) {  // (after the value, on its baseline: smaller, a lighter red-white)
            float ts = fs * vs * tag_s, tx = vx + vw0 * vs + fs * 0.35f * vs;
            dl->AddText(font, ts, ImVec2(tx, p.y + (h - fs * vs) * 0.5f + (fs * vs - ts) * 0.8f),
                        off ? kTextOff : theme_u32(kTextV, 0.8f), tag);
        }
        dl->PopClipRect();
    }
    if (foc) {  // the cursor: a thin white outline around the box (or the band, for a button)
        float th = fmaxf(1, 1.5f * g_scale);
        if (has_box) dl->AddRect(ImVec2(b0.x - 1, b0.y - 1), ImVec2(b1.x + 1, b1.y + 1), off ? theme_u32(kGoldV, 0.4f) : kGold, r + 1, 0, th);
        else dl->AddRect(ImVec2(p.x - 1, p.y - 1), ImVec2(p.x + w + 1, p.y + h + 1), off ? theme_u32(kGoldV, 0.4f) : kGold, r + 1, 0, th);
    }
    g_row_min = p, g_row_max = ImVec2(p.x + w, p.y + h);  // (the row's rectangle: a dropdown opens under it)
    ImGui::Dummy(ImVec2(w, 4 * g_scale));  // (a gap between rows: their boxes read as separate, owner 2026-11-23)
    ImGui::PopID();
    if (step || (clicked && (off || blocked)))  // (the menu recorder: what a press did to this row)
        menu_rec("row \"%s\" (value \"%s\"): %s%s", label_en, value_en.c_str(),
                 step == 2 ? "pressed" : step > 0 ? "right" : step < 0 ? "left" : "click ignored",
                 off ? " (greyed out)" : blocked ? " (left/right blocked this frame)" : clicked ? " - by a click or A" : "");
    return step;
}

bool opt_toggle(const char *label, bool *v, bool def, const char *desc) {
    g_row_toggle = *v ? 1 : 0;  // (drawn as an On | Off switch)
    if (!row(label, *v ? "On" : "Off", *v != def, desc)) return false;  // (any step flips it)
    *v = !*v;
    return true;
}

void opt_next_dropdown(void) { g_next_dropdown = true; }

// A dropdown row's list, under its value box (the row is the last item): open = a click or A on the row this frame.
// The mouse or up / down and A choose, B / Esc closes it; avail (may be NULL) greys choices this PC can't use.
// Returns the chosen index, or -1.
static int dropdown_list(const char *label, int cur, const char *const *names, int n, const bool *avail, bool open) {
    ImVec2 r0 = g_row_min, r1 = g_row_max;
    float w = r1.x - r0.x, bx = r0.x + floorf(w * 0.54f) + 6 * g_scale, bw = r1.x - bx;
    ImGui::PushID(label);
    if (open) ImGui::OpenPopup("##dropdown");
    int pick = -1;
    // (its own look, owner 2026-11-23: a charcoal panel the value box's width just under it, a thin border, the
    // choices centred as in the box, the hovered one lighter, the current one marked with a red bar)
    float rowh = (r1.y - r0.y) * 0.9f, pad = 4 * g_scale, gap = 3 * g_scale;
    float avail_h = ImGui::GetIO().DisplaySize.y - r1.y - gap - 8 * g_scale;
    float list_h = fminf(rowh * n + 2 * pad, fmaxf(avail_h, rowh * 4 + 2 * pad));
    bool above = avail_h < rowh * fminf((float)n, 4.0f);  // (no room below: it opens upward)
    ImGui::SetNextWindowPos(above ? ImVec2(bx, r0.y - list_h - gap) : ImVec2(bx, r1.y + gap));
    ImGui::SetNextWindowSize(ImVec2(bw, list_h));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, theme_u32(kBoxV));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(255, 255, 255, 40));
    ImGui::PushStyleColor(ImGuiCol_Header, theme_u32(kBoxHotV));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme_u32(kSurfaceHoverV));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, theme_u32(kBoxHotV));
    ImGui::PushStyleColor(ImGuiCol_NavCursor, theme_u32(kGoldV));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 5 * g_scale);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, fmaxf(1, g_scale));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopup("##dropdown")) {
        g_dropdown_frame = ImGui::GetFrameCount();
        for (int i = 0; i < n; i++) {
            bool en = !avail || avail[i];
            if (!en) ImGui::BeginDisabled();
            ImGui::PushID(i);
            if (ImGui::Selectable(tr(names[i]), i == cur, 0, ImVec2(0, rowh))) pick = i;
            if (i == cur) {  // (the current choice: a short red bar at its left)
                ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
                float bh = (b.y - a.y) * 0.5f;
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(a.x + 2 * g_scale, a.y + bh * 0.5f),
                                                          ImVec2(a.x + 5 * g_scale, b.y - bh * 0.5f), theme_u32(kGoldV), 2 * g_scale);
            }
            if (i == cur && ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus(), ImGui::SetScrollHereY(0.5f);
            ImGui::PopID();
            if (!en) ImGui::EndDisabled();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(6);
    ImGui::PopID();
    if (pick >= 0) menu_rec("dropdown \"%s\": chose \"%s\"", label, names[pick]);
    return pick;
}

// (owner 2026-11-23: no left / right rows - two choices are a switch like On | Off, more are a dropdown)
bool opt_choice(const char *label, int *v, const char *const *names, int n, int def, const char *desc, bool wrap) {
    int cur = *v >= 0 && *v < n ? *v : 0;
    g_next_dropdown = false;
    if (n == 2) {  // (a switch: a click on the other half, A, left / right choose the other)
        g_row_toggle = cur == 0 ? 1 : 0;
        g_row_seg[0] = names[0], g_row_seg[1] = names[1];
        if (!row(label, names[cur], cur != def, desc)) return false;
        *v = 1 - cur;
        return true;
    }
    bool dd = n >= 3;
    g_row_chevron = dd;
    int step = row(label, names[cur], cur != def, desc, false, !dd);
    if (dd) {
        int pick = dropdown_list(label, cur, names, n, NULL, step == 2);
        if (step == 2) return false;  // (the list opened instead)
        if (pick >= 0) {
            if (pick == cur) return false;
            *v = pick;
            return true;
        }
    }
    if (!step) return false;
    int nv = cur + (step == 2 ? 1 : step);
    nv = wrap ? (nv + n) % n : nv < 0 ? 0 : nv >= n ? n - 1 : nv;
    if (nv == cur) return false;
    *v = nv;
    return true;
}

// opt_choice whose choices this machine may not support (avail[i] false): left / right / A skip those, and the
// description names them ("Not available on this PC: ...") so they read as greyed out rather than missing. A choice
// saved earlier that isn't available here stays selectable away from (the row isn't disabled).
bool opt_choice_avail(const char *label, int *v, const char *const *names, int n, int def, const char *desc, const bool *avail) {
    int cur = *v >= 0 && *v < n ? *v : 0;
    std::string d = desc ? tr(desc) : "";
    std::string missing;
    for (int i = 0; i < n; i++)
        if (!avail[i]) missing += (missing.empty() ? "" : ", ") + std::string(tr(names[i]));
    if (!missing.empty()) d += "\n\n" + std::string(tr("Not available on this PC (greyed out):")) + " " + missing + ".";
    g_next_dropdown = false;
    if (n == 2) {  // (a switch, as opt_choice: only to an available choice)
        g_row_toggle = cur == 0 ? 1 : 0;
        g_row_seg[0] = names[0], g_row_seg[1] = names[1];
        if (!row(label, names[cur], cur != def, d.c_str()) || !avail[1 - cur]) return false;
        *v = 1 - cur;
        return true;
    }
    bool dd = n >= 3;
    g_row_chevron = dd;
    int step = row(label, names[cur], cur != def, d.c_str(), false, !dd);
    if (dd) {
        int pick = dropdown_list(label, cur, names, n, avail, step == 2);
        if (step == 2) return false;  // (the list opened instead)
        if (pick >= 0) {
            if (pick == cur || !avail[pick]) return false;
            *v = pick;
            return true;
        }
    }
    if (!step) return false;
    int dir = step == 2 ? 1 : step, nv = cur;
    for (int k = 0; k < n; k++) {  // (the next available one that way, wrapping)
        nv = (nv + dir + n) % n;
        if (avail[nv]) break;
    }
    if (nv == cur || !avail[nv]) return false;
    *v = nv;
    return true;
}

bool opt_slider(const char *label, float *v, float lo, float hi, float step, float def, const char *fmt, const char *desc) {
    // (a slider, as every number row: the mouse drags the track, left / right step, A resets)
    char buf[32];
    snprintf(buf, sizeof buf, fmt, *v);
    g_row_slider = hi > lo ? (*v - lo) / (hi - lo) : 0;
    int s = row(label, buf, fabsf(*v - def) > step * 0.01f, desc, false, false, true);
    float n = *v;
    if (g_slider_drag >= 0) n = lo + roundf(g_slider_drag * (hi - lo) / step) * step;
    else if (s == 1 || s == -1) n = roundf(*v / step) * step + s * step;
    else if (s == 2) n = def;
    n = n < lo ? lo : n > hi ? hi : n;
    if (fabsf(n - *v) < step * 0.01f) return false;
    *v = n;
    return true;
}

bool opt_int(const char *label, int *v, int lo, int hi, int step, int def, const char *fmt, const char *desc,
             const char *lo_name) {
    // (a slider, as every number row: lo_name, when given, is shown at the lowest value)
    char buf[48];
    if (lo_name && *v == lo) snprintf(buf, sizeof buf, "%s", lo_name);
    else snprintf(buf, sizeof buf, fmt, *v);
    g_row_slider = hi > lo ? (float)(*v - lo) / (float)(hi - lo) : 0;
    int s = row(label, buf, *v != def, desc, false, false, true);
    int n = *v;
    if (g_slider_drag >= 0) n = lo + (int)lroundf(g_slider_drag * (hi - lo) / step) * step;
    else if (s == 1 || s == -1) n = *v + s * step * (g_held && hi - lo > 40 * step ? 10 : 1);
    else if (s == 2) n = def;
    n = n < lo ? lo : n > hi ? hi : n;
    if (n == *v) return false;
    *v = n;
    return true;
}

bool opt_int_slider(const char *label, int *v, int lo, int hi, int step, int def, const char *fmt, const char *desc) {
    char buf[48];
    snprintf(buf, sizeof buf, fmt, *v);
    g_row_slider = hi > lo ? (float)(*v - lo) / (float)(hi - lo) : 0;
    int s = row(label, buf, *v != def, desc, false, false, true);
    int n = *v;
    if (g_slider_drag >= 0) {  // (the mouse on the track: the nearest step there)
        n = lo + (int)lroundf(g_slider_drag * (hi - lo) / step) * step;
    } else if (s == 1 || s == -1) {
        n = *v + s * step * (g_held && hi - lo > 40 * step ? 10 : 1);
    } else if (s == 2) {  // (A on a controller: back to the default)
        n = def;
    }
    n = n < lo ? lo : n > hi ? hi : n;
    if (n == *v) return false;
    *v = n;
    return true;
}

bool opt_button(const char *label, const char *desc) { return row(label, "", false, desc) == 2; }

bool opt_radio(const char *label, bool selected, const char *desc) {
    bool off = g_next_disabled;
    bool chosen = row(label, "", false, desc) == 2;
    ImVec2 a = g_row_min, b = g_row_max;  // (the row, not the gap under it)
    float r = ImGui::GetFontSize() * 0.32f, cx = b.x - 16 * g_scale - r, cy = (a.y + b.y) * 0.5f;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImU32 col = off ? kTextOff : selected ? kText : theme_u32(kTextV, 0.78f);
    dl->AddCircle(ImVec2(cx, cy), r, col, 0, 2.0f);
    if (selected) dl->AddCircleFilled(ImVec2(cx, cy), r * 0.55f, col);
    return chosen && !selected;
}

bool opt_action(const char *label, const char *value, const char *desc, bool upper) {
    return row(label, value ? value : "", false, desc, upper) == 2;
}

static ImGuiID g_text_editing;  // the text row turned into a field (one at a time)
bool opt_text_editing(const char *label) { return g_text_editing && g_text_editing == ImGui::GetID(label); }

bool opt_text(const char *label, char *buf, size_t cap, const char *hint, const char *desc) {
    ImGuiID &editing = g_text_editing;
    static int focus;        // frames left to give it the keyboard focus
    ImGuiID id = ImGui::GetID(label);
    if (editing != id) {
        if (row(label, buf[0] ? buf : (hint ? hint : ""), false, desc) == 2) editing = id, focus = 2;
        return false;
    }
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(-1);
    if (focus > 0) ImGui::SetKeyboardFocusHere(), focus--;
    bool done = ui_input_text("##text", buf, cap, ImGuiInputTextFlags_EnterReturnsTrue, hint, label);
    if (ImGui::IsItemFocused()) g_desc_focus = desc ? desc : "";
    if (done || ui_input_text_cancelled() || (focus == 0 && !ImGui::IsItemActive() && !osk_is_open())) editing = 0;
    ImGui::PopID();
    return done;
}

void opt_end(void) {
    // Down on a page's last row stays there. With nothing below it, ImGui's navigation took the nearest item in any
    // direction - the first category tab - and held Down at the end of Performance switched to Display (owner's
    // recording, 2026-11-22)
    if (g_last_row_focused && ImGui::GetCurrentContext()->NavCursorVisible &&
        (ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) || ImGui::IsKeyPressed(ImGuiKey_GamepadLStickDown) ||
         ImGui::IsKeyPressed(ImGuiKey_DownArrow)))
        ImGui::NavMoveRequestCancel();
    g_last_row_focused = false;
    ImGui::PopStyleVar();
    if (const char *t = getenv("ODYSSEY_OPT_SCROLL")) ImGui::SetScrollY(ImGui::GetScrollMaxY() * (float)atof(t));  // (test)
    ImGui::EndChild();
    // the description: the hovered row (mouse) wins, else the selected one (controller), else the last shown on this
    // page (another page's stayed on every tab: the Display tab's Monitor text on Graphics, Audio, Mods...)
    if (g_page_shown != g_page) g_desc_shown.clear(), g_title_shown.clear(), g_page_shown = g_page;
    if (!g_desc_hover.empty()) g_desc_shown = g_desc_hover, g_title_shown = g_title_hover;
    else if (!g_desc_focus.empty()) g_desc_shown = g_desc_focus, g_title_shown = g_title_focus;
    else if (g_desc_shown.empty()) g_desc_shown = g_desc_first, g_title_shown = g_title_first;
    ImGui::SameLine(0, 0);
    ImVec2 p = ImGui::GetCursorScreenPos(), avail = ImGui::GetContentRegionAvail();
    float gap = 22 * g_scale, r = 8 * g_scale;
    ImDrawList *dl = ImGui::GetWindowDrawList();
    float x0 = p.x + gap, x1 = p.x + avail.x, y = p.y;
    // In game: the game itself, live, through a window in the menu (menu.mm leaves that rectangle undimmed and
    // unpainted) - a setting that changes the picture shows there at once, at its real pixels
    {
        extern bool menu_live_picture(float x0, float y0, float x1, float y1);
        // (in the game picture's own shape: 16:9, or as wide as the screen with Aspect ratio Fill - ultrawide too)
        ImVec2 ds = ImGui::GetIO().DisplaySize;
        float asp = renderer_picture_aspect();
        if (asp < 0.5f || asp > 6) asp = ds.x > 0 && ds.y > 0 ? ds.x / ds.y : 16.0f / 9;
        float pw = x1 - x0, ph = pw / asp, maxh = avail.y * 0.6f;
        if (ph > maxh) ph = maxh, pw = ph * asp;
        float ox0 = x0, ox1 = x1;
        x0 = ox0 + (ox1 - ox0 - pw) * 0.5f, x1 = x0 + pw;
        if (menu_live_picture(x0, y, x1, y + ph)) {
            dl->AddRect(ImVec2(x0 - 1, y - 1), ImVec2(x1 + 1, y + ph + 1), theme_u32(kSidebarV), r, 0, fmaxf(2, 3 * g_scale));
            float lp = 8 * g_scale;  // (no "Live picture" label on it: the picture speaks for itself - owner 2026-11-21)
            char fps[32];
            snprintf(fps, sizeof fps, "%.0f fps", ImGui::GetIO().Framerate);
            ImVec2 fsz = ImGui::CalcTextSize(fps);
            dl->AddRectFilled(ImVec2(x1 - fsz.x - 2 * lp, y + ph - fsz.y - lp), ImVec2(x1, y + ph), theme_u32(kSidebarV, 0.85f), r,
                              ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersBottomRight);
            dl->AddText(ImVec2(x1 - fsz.x - lp, y + ph - fsz.y - lp * 0.5f), kText, fps);
            y += ph + 14 * g_scale;
        }
        x0 = ox0, x1 = ox1;  // (the card: the pane's whole width)
    }
    // the card: the option's name, a hairline, what it does
    dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, p.y + avail.y), theme_u32(kSidebarV), r);
    ImGui::SetCursorScreenPos(ImVec2(x0, y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20 * g_scale, 14 * g_scale));
    ImGui::BeginChild("##desc", ImVec2(x1 - x0, p.y + avail.y - y), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoNav);
    ImGui::PopStyleVar();
    if (!g_title_shown.empty()) {
        ImGui::SetWindowFontScale(1.3f);
        ImGui::TextUnformatted(g_title_shown.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImVec2 lp = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(lp.x, lp.y + 2 * g_scale), ImVec2(lp.x + ImGui::GetContentRegionAvail().x, lp.y + 2 * g_scale),
                                            theme_u32(kLineV), fmaxf(1, g_scale));
        ImGui::Dummy(ImVec2(0, 10 * g_scale));
    }
    ImGui::SetWindowFontScale(y > p.y ? 1.05f : 1.15f);  // (the description reads at a glance; under the picture a little smaller)
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(g_desc_shown.c_str());
    ImGui::PopTextWrapPos();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::EndChild();
    ImGui::PopID();
}
