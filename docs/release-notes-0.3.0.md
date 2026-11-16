A big update for high frame rates, upscaling, ultrawide screens and smoothness on high-refresh monitors.

## Frame rate

- **Any frame rate up to 240 FPS**: 60, 72, 90, 100, 120, 144, 165, 180 or 240 (Performance > Frame rate). Every frame is
  really drawn - Mario and the camera where they are at that moment, no generated in-between images. Gameplay itself
  (physics, enemies, timers) keeps the game's own 60 steps a second, as it was made.
- **Match display** picks the smoothest rate for your screen: the highest one up to 120 your refresh rate divides
  (a 144 Hz monitor: 72 FPS, every frame on screen equally long). 60 FPS on a 144 Hz screen can't be smooth - frames
  alternate 2 and 3 refreshes - which is the judder some of you saw even with VSync on.
- **Frame pacing on Windows**: the game's timer now follows when your screen really shows each frame (as on the Steam
  Deck already).

## Upscaling and the HUD

- **A sharp HUD with DLSS, FSR and XeSS**: the HUD is now drawn at your screen's full resolution after the upscale,
  instead of being upscaled with the 3D scene (blocky text and icons at Performance modes, smearing with temporal
  upscalers). Same for lower internal resolutions.
- **AMD FSR 2** upscaler on Linux and the Steam Deck (and Windows), run by the game's own code - no extra files.
  It now works at the Deck's defaults (100% resolution, 16:10) in Docked console mode.
- **Frame generation** (Windows): AMD FSR frame generation, and NVIDIA DLSS frame generation (beta) with **NVIDIA
  Reflex** low latency (Off / On / On + Boost). Off by default; needs an upscale method and 60 FPS.

## Ultrawide

- **HUD position (wide)**: Edges (default) puts the coin and moon counters, life gauge, button guides, mini map and
  "Saving..." in the corners of a 21:9 or 32:9 picture, as on a 16:9 screen; Centered keeps the HUD in a 16:9 box.
  Menus and messages stay in the middle.

## Controls and audio

- **The configuration page before the game works with a controller** (D-pad / stick to move, A to change, Start to play).
- **Mouse camera** turns the camera exactly as far as the mouse moves (#52).
- **Gyro**: invert X / Y and Calibrate (#53).
- **5.1 surround** audio output (Audio > Audio output) (#35).

## Fixes

- Blocky picture when turning the camera fast with the game's dynamic resolution (#51).
- Internal resolution shows the actual resolution instead of a cut-off "Match display".
- Fewer copies a frame in blur and bloom (up to ~6% faster on integrated GPUs).

## Still being worked on

- FSR 2 in Handheld console mode on the Steam Deck.
- Shadows that change with where the camera points (please send a screenshot and where it happened).
- DLSS frame generation is a beta: it needs testing on more displays.
