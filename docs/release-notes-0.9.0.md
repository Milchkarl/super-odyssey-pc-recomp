## Fixed / new

- **Low frame rate after the Torkdrift fight** in the Wooded Kingdom (#136): the falling petals made the game copy the whole picture about 230 times a frame. On an AMD integrated GPU in our tests: 14 FPS before, 49 now.
- **Bloom quality: Off** and **Motion blur: Off** now work in every kingdom; before, the glow came back while the camera turned (#135). The Bloom choices are now in the order Switch / Off / High.
- **Crash at start-up on game version 1.0.0 with the Portuguese (Brazil) translation**: the translation mod lacks a text file 1.0.0 needs; the game now takes that file from its own copy.
- **Crash in the Metro Kingdom when Sherm resets its camera** (#138): guarded. We couldn't reproduce the crash itself, so please report it if it happens again.
- **Crash on older Intel integrated graphics** when entering a kingdom (#122): depth clamping is now off on Intel's legacy Windows driver. Not tested on that hardware.
- **The mouse in the game's own menus** now works on game version 1.0.0 too (it was 1.4.1 only).
- **Shake button and key** (Controls): can do a homing throw instead of a spin throw (#137). Shake stays the default.
- **Faster start-up**: shader warm-up runs on several CPU threads.
- **Menu**: pressing Down on a page's last row no longer jumps to another tab.

## Still to fix

- Shadow distance has no effect in Snapshot Mode (#135).
- A graphics driver stop at the opening cutscene on one Linux AMD laptop (#139): not reproduced on our PC or Steam Deck.
- Running dust as black squares on some AMD graphics cards (#103).
- The tutorial videos stay black with FSR 3.1 (#66).

## What's next

Fixing leftover bugs and improving the settings. Please keep the reports coming.

## Updating

The game code is rebuilt once on this update (about 4 minutes on a PC, about 20 on a Steam Deck), and shaders are prepared again once.

Windows: download the zip below and extract it over your current folder (or use **Updates** in the menu). Linux / Steam Deck: download the AppImage below, or use **Updates** in the menu. Your game files, saves and settings stay.
