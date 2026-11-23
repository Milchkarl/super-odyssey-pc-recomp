// Super Odyssey PC Recomp's look (runtime/ui): one palette for the start page, the menu, its option rows, the Controls
// page, the on-screen keyboard. Since 0.4.1 (owner 2026-11-17: "same black and red, just more modern and easy to use"):
// solid charcoal panels and surfaces, white text with a calm grey for the secondary, and red as the colour of action -
// the primary button filled red, the selected row and sidebar entry tinted red with a red bar on their left.
// (The icon keeps its pipe green: tools/make_icon.py.)
#pragma once
#include "imgui.h"

#define THEME_RGB(r, g, b, a) ImVec4((r) / 255.0f, (g) / 255.0f, (b) / 255.0f, (a))
// the palette
// (experiment, branch menu-odyssey-look: the game's own HUD colours - a deep navy for panels, Mario's red for the
// values and the primary button, the coins' gold for the cursor - in the layout of the settings menus the owner likes:
// tabs along the top, rows with a filled value box, the picture and the description on the right)
static const ImVec4 kPanelV = THEME_RGB(0, 0, 0, 1);              // pitch black (owner 2026-11-20)          // the menu window, the start page's card
static const ImVec4 kSidebarV = THEME_RGB(20, 20, 23, 1);           // the tab bar, the description card, info boxes
static const ImVec4 kSurfaceV = THEME_RGB(31, 31, 35, 1);           // fields, buttons, rows
static const ImVec4 kSurfaceHoverV = THEME_RGB(46, 46, 52, 1);     // ... under the mouse / the cursor
static const ImVec4 kAccentV = THEME_RGB(104, 104, 112, 0.95f);   // (owner 2026-11-23: black / grey / white - was Mario red)           // Mario red: value boxes, the primary button
static const ImVec4 kAccentRestV = THEME_RGB(80, 80, 88, 0.8f);       // value boxes of the rows the cursor isn't on
static const ImVec4 kBoxV = THEME_RGB(44, 44, 50, 1);              // value boxes: a lighter charcoal than the rows
static const ImVec4 kBoxHotV = THEME_RGB(58, 58, 66, 1);           // ... on the row the cursor is on
static const ImVec4 kAccentHotV = THEME_RGB(128, 128, 136, 0.95f);       // ... the selected row's / under the mouse
static const ImVec4 kAccentTintV = THEME_RGB(255, 255, 255, 0.10f);    // the selected tab
static const ImVec4 kAccentDeepV = THEME_RGB(60, 60, 66, 1);        // pressed; a value box's inactive half
static const ImVec4 kAccentSoftV = THEME_RGB(255, 255, 255, 1);     // a value that isn't the default (white)
static const ImVec4 kGoldV = THEME_RGB(228, 0, 25, 1);            // the cursor's outline: Mario red (owner 2026-11-23; was coin gold)            // coin gold: the cursor's outline, the selected tab
static const ImVec4 kTextV = THEME_RGB(255, 255, 255, 1);
static const ImVec4 kTextDimV = THEME_RGB(172, 172, 180, 1);        // secondary text, tabs not shown
static const ImVec4 kTextOffV = THEME_RGB(96, 96, 104, 1);         // greyed out
static const ImVec4 kLineV = THEME_RGB(255, 255, 255, 0.10f);       // separators
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
#define kGold theme_u32(kGoldV)

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
