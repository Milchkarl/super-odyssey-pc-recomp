## Fixed / new

- **Glow at high resolutions (#102)**: above the game's own 1600 x 900, the glow around the sun and lights could show a halo and small blocks. It now samples the picture the way the Switch does; in our tests at 1440p it matches the game's own picture and the emulator.
- **Ambient occlusion** (Graphics, Off by default): soft shading in corners, between steps and under characters. Not in the Switch version: an addition. About 1 ms a frame at 1440p on the test PC (RTX 4090); keep it off on a Steam Deck.
- **Settings menu**: L / R pressed on an option could switch tabs and then jump to Display. Fixed.
- **Settings menu**: the content now reaches the screen's edges, with a larger live picture.
- **Record menu input** (Quit, Diagnostics): records what the controller, mouse and keyboard send and what the menu did, to a file you can attach to a menu bug report.

## Still to fix

- Running dust as black squares on some AMD graphics cards (#103).
- A crash on older Intel integrated graphics when entering a kingdom (#122).
- The tutorial videos stay black with FSR 3.1 (#66).

## What's next

Fixing leftover bugs and improving the settings. The road to 1.0.0 follows players' experience: please keep the reports coming.

## Updating

Shaders are prepared again once on this update. No game code rebuild.

Windows: download the zip below and extract it over your current folder (or use **Updates** in the menu). Linux / Steam Deck: download the AppImage below, or use **Updates** in the menu. Your game files, saves and settings stay.
