# 0.4.0 test checklist

Both test builds are local (not released) and never touch the real installs.

- **PC:** `I:\Switch Games\Super Odyssey PC Recomp 0.4.0 test\Play 0.4.0 test.cmd` - the package as it would ship, on
  the test copy's game files, a copy of your save (`save-owner`) and fresh settings (`powermoon-040.ini` in
  `I:\Switch Games\sopr-wintest`; delete it to see the first-start defaults again). Starting `SuperOdysseyRecomp.exe`
  in that folder directly tests a fresh install instead (it asks for your game files).
- **Steam Deck:** always through `/run/media/deck/SN01T/sopr-0.4.0-test/play-0.4.0-test.sh` (add it to Steam as a
  non-Steam game). Not the AppImage itself: it would open the real Deck install.

The first start after the update prepares the shaders once (a few minutes) - as players will see.

## Check

1. **Start page:** untick "Show this screen at every start", quit, start again: it no longer shows.
2. **Menu with a controller:** A (Cross) selects, B (Circle) goes back. Every tab at 1280 x 800 (the Deck): nothing
   overlaps or is cut off.
3. **Defaults** (fresh settings): borderless, FXAA, 60 FPS, VSync Off; Performance > Optimizations: Early frame
   hand-off On, Multithreaded rendering On (Off on the Deck); Warm-up Auto. No Battery saver, no 120 FPS
   interpolation.
4. **Play an hour on the Deck** through several kingdoms and loading zones: the frame rate should not sink over time
   (Performance overlay: memory).
5. **Lake Kingdom:** continue a save that is in Lake Kingdom.
6. **Quit** from the menu and with the window's close button (PC with DLSS frame generation on; Deck in Gaming Mode):
   no crash.
7. **Deck, docked, FSR 2 Quality** (Fill): no dark smears, black buildings or white mist dome in Cap Kingdom.
8. **Discord:** with Discord open, your profile shows the kingdom; Performance > Discord activity Off clears it.
9. **Wardrobe** in the Odyssey, the map, the pause menu.

## Known

- Scripted test tours that warp between kingdoms sometimes stop the game with its own "sead halt" (its 512 KB scene
  heap) right after a warp; whether normal play can hit it is being checked.
- The Steam Deck's GPU reset with Multithreaded rendering on: not seen again, but it stays off there.
