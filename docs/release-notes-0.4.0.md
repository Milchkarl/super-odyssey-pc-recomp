A big update: more FPS on busy scenes, fixes for crashes in Lake Kingdom, on quit and with FSR 2 / DLSS on the Steam Deck, steady memory over long sessions, a tidier settings menu, Joy-Con support and Discord activity.

## Performance

- **Faster on busy scenes.** Two new options at the top of Performance > Optimizations:
  - **Early frame hand-off**: on by default: the game starts its next frame while the graphics card finishes the last one (about 10-15% more FPS in busy places, 45 -> 58-60 FPS on a Steam Deck).
  - **Multithreaded rendering**: a second CPU thread hands the drawing commands to the driver (about 25% more FPS in busy places at high frame rates). Auto by default: on with NVIDIA graphics cards, off with AMD and Intel ones and on the Steam Deck, until it has been tested more there.
  - If you see flickering, missing objects or a frozen picture, turn them off.
- **Steady memory over long sessions.** Memory no longer grows with every kingdom and loading zone on Linux / Steam Deck (the cause of slowdowns after a while). On PCs with less than 32 GB of RAM the warm-up at start prepares fewer shaders ahead, which used several GB of graphics driver memory (#75, #61).

## Fixes

- **Crash entering or resuming in Lake Kingdom** on game version 1.0.0 above 60 FPS (#72).
- **Crash when closing the game** with NVIDIA DLSS frame generation (#70), and on the Steam Deck in Gaming Mode.
- **FSR 2, FSR 3.1 and DLSS below native at 16:10** (the Steam Deck's screen): dark smears over the ground, half-black buildings and a white mist are gone.
- **"Show this screen at every start"** can be turned off again (#74).
- **Korean and Brazilian Portuguese menus** fully translated (#63).

## Settings menu

- **Same defaults everywhere** (Windows, Linux, Steam Deck): borderless, FXAA, 60 FPS, VSync off, the game's own texture filtering. The Steam Deck keeps its 1280 x 800 picture filling the screen.
- **Every option says in plain words what it does and what is recommended**, at the top of its description (a line under each control on the Controls page).
- **VSync**: On, Fast (Mailbox), Adaptive (Relaxed), Off.
- **New**: Shadow quality (2x / 4x shadow maps), Texture detail, FXAA, AMD FSR 1 in the upscaler list.
- **Controllers**: A / Cross selects and B / Circle goes back in the menu (it was the other way round on Xbox, PlayStation and Steam Deck controllers).
- Nothing overlaps or gets cut off at 1280 x 800, in any language.
- **Removed**: 120 FPS interpolation (it crashed - use Performance > Frame rate 72-240 instead) and Battery saver.

## New

- **Joy-Con pair** support: the game sees them as Joy-Cons; anything else stays a Pro Controller.
- **Discord activity**: your Discord profile shows the kingdom you're in and the time played, while Discord is open (Performance > Discord). Thanks to galaxyhf (#76, #78).
- Windows: a game folder with accents in its path (e.g. "Área de Trabalho") no longer stops the game at start (galaxyhf).

## Updating

Windows: download the zip below and extract it over your current folder (or use **Updates > Check for updates** in the menu). Linux / Steam Deck: download the AppImage below. Your game files, saves and settings stay. The first start after updating prepares the shaders again once (a few minutes).
