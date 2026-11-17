// Super Odyssey PC Recomp's look (runtime/ui): one palette for the start page, the menu, its option rows, the Controls
// page, the on-screen keyboard. Since 0.4.1 (owner 2026-11-17: "same black and red, just more modern and easy to use"):
// solid charcoal panels and surfaces, white text with a calm grey for the secondary, and red as the colour of action -
// the primary button filled red, the selected row and sidebar entry tinted red with a red bar on their left.
// (The icon keeps its pipe green: tools/make_icon.py.)
#pragma once
#include "imgui.h"

#define THEME_RGB(r, g, b, a) ImVec4((r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a))
// the palette
static const ImVec4 kPanelV = THEME_RGB(14, 14, 17, 0.97f);         // the menu window, the start page's card
static const ImVec4 kSidebarV = THEME_RGB(23, 23, 28, 1);           // the menu's sidebar (categories), info boxes
static const ImVec4 kSurfaceV = THEME_RGB(29, 29, 34, 1);           // fields, buttons, rows
static const ImVec4 kSurfaceHoverV = THEME_RGB(40, 40, 47, 1);      // ... under the mouse
static const ImVec4 kAccentV = THEME_RGB(229, 37, 33, 1);           // red: the primary button, the selection's bar
static const ImVec4 kAccentHotV = THEME_RGB(244, 62, 56, 1);        // ... the primary button under the mouse / cursor
static const ImVec4 kAccentTintV = THEME_RGB(229, 37, 33, 0.17f);   // the selected row / sidebar entry
static const ImVec4 kAccentDeepV = THEME_RGB(176, 24, 22, 1);       // pressed
static const ImVec4 kAccentSoftV = THEME_RGB(255, 255, 255, 1);     // a value that isn't the default (white)
static const ImVec4 kTextV = THEME_RGB(255, 255, 255, 1);
static const ImVec4 kTextDimV = THEME_RGB(168, 168, 178, 1);        // secondary text, categories not shown
static const ImVec4 kTextOffV = THEME_RGB(98, 98, 108, 1);          // greyed out
static const ImVec4 kLineV = THEME_RGB(255, 255, 255, 0.08f);       // separators
static const ImVec4 kWarnV = THEME_RGB(224, 154, 43, 1);            // a conflict (one key bound twice)

static inline ImU32 theme_u32(ImVec4 c, float alpha = -1) {
    if (alpha >= 0) c.w = alpha;
    return ImGui::ColorConvertFloat4ToU32(c);
}
#define kAccent theme_u32(kAccentV)
#define kAccentDim theme_u32(kAccentV, 0.43f)
#define kText theme_u32(kTextV)
#define kTextDim theme_u32(kTextDimV)
#define kTextOff theme_u32(kTextOffV)

// The icon's pipe (tools/make_icon.py) in a square of `size` at p: a green pipe with its wider lip, a light stripe and
// a dark shade, and a small gold star above it. bg: the colour behind it (unused, kept for the callers).
static inline void theme_logo(ImDrawList *dl, ImVec2 p, float size, ImU32 col, ImU32 bg) {
    (void)bg;
    float u = size / 16;  // the icon's 16-pixel grid
    auto r = [&](float x, float y, float w, float h, ImU32 c) {
        dl->AddRectFilled(ImVec2(p.x + x * u, p.y + y * u), ImVec2(p.x + (x + w) * u, p.y + (y + h) * u), c);
    };
    ImU32 dark = IM_COL32(16, 80, 36, 255), light = IM_COL32(150, 236, 130, 255), edge = IM_COL32(8, 40, 18, 255);
    ImU32 gold = IM_COL32(245, 197, 66, 255);
    r(4, 9, 8, 7, edge), r(5, 9, 6, 7, col), r(6, 9, 1, 7, light), r(9, 9, 2, 7, dark);  // the body
    r(3, 6, 10, 4, edge), r(3.5f, 6.5f, 9, 3, col), r(4.5f, 6.5f, 1, 3, light), r(10, 6.5f, 2.5f, 3, dark);  // the lip
    r(7, 1, 2, 4, gold), r(6, 2, 4, 2, gold), r(5, 2.5f, 6, 1, gold);  // the star
}
