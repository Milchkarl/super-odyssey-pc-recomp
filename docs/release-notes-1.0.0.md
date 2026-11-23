## New

- **Native HDR** with scene colour grading (peak and paper-white brightness, exposure, highlights, shadows, contrast, saturation and more), on the Display tab as sliders. HDR10 displays on Windows; the Steam Deck stays SDR.
- **Graphics card choice** on the Display tab, for PCs with more than one.
- **Field of view** slider (100-150%) on the Display tab, for every camera.
- **Bloom quality High** now draws the glow at the picture's resolution, keeping the game's glow shape (#102).
- **Shadow edges: Soft** (Graphics): shadow edges fade instead of stepping. Off by default.
- **Switch 2 controllers over Bluetooth** (Pro Controller 2, Joy-Con 2, with gyro and HD rumble), Windows only, off by default: Controls > Switch 2 controllers. Contributed by zigzagulouszagger (#150); we couldn't test it with a real controller.
- **Menu**: dropdowns for long lists, switches for two choices, the Controls page in the same style, memory bars showing what each graphics choice takes, and no "Auto" entries: each row shows the value in effect.

## Fixed

- **Shadow distance** now changes how far the sun's shadows reach (#135).
- **Crash in the Moon Kingdom** (and wherever a video plays) on installs set up without the game's videos (#124): the video shows black instead, setup now asks for the videos, and the start screen says how to get them back.
- **Mouse pointer over cutscenes** (#109): it now hides after 3 seconds without mouse use.
- **Mario shivering in the sun in the Moon Kingdom** (#124): the game's shade check read a map the GPU never handed back; it now does.
- **Menu navigation**: Down reaches everything on a page (on Controls it stopped above the button bindings), and Up goes back to the page's own tab instead of jumping to another one.
- **Spin throw key** (#154): Mario no longer turns and drifts, and the spin comes out every time.
- **Mouse buttons bound to A or B** now act as A or B in the menus and on the map, as the button prompts show (#151).
- **"can't load odyssey_game.dll (error 193)" at start** (#152): a damaged game code file is now built again.
- **Untranslated texts** in the quit dialog, the setup page and the on-screen keyboard can now be translated (#153).
- **RecompLauncher Easy Setup** shows its progress and errors (#144).
- **Out of memory on small integrated GPUs** (#147): textures go to system memory there.
- **Older Intel graphics** (#148): depth clamping is off on every Intel Windows driver. Not tested on that hardware.
- **Linux**: a Japanese font for the game's system text where one is installed (#146).

## Still to fix

- A graphics driver stop at the opening cutscene on one Linux AMD laptop (#139): waiting on the reporter's tests.
- Running dust as black squares on some AMD graphics cards (#103).
- The tutorial videos stay black with FSR 3.1 (#66).

## Updating

The game code is rebuilt once on this update (about 4 minutes on a PC, about 20 on a Steam Deck), and shaders are prepared again once.

Windows: download the zip below and extract it over your current folder (or use **Updates** in the menu). Linux / Steam Deck: download the AppImage below, or use **Updates** in the menu. Your game files, saves and settings stay.
