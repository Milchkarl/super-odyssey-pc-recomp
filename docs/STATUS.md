# Status

[Back to the README](../README.md)

Where each part of Super Odyssey PC Recomp stands, as of version 0.8.0. The numbers are the maintainer's estimates,
not measurements: they say how finished a part is, with the open issues that hold it back.

The overall bar in the README is a weighted average. Each area has its share of the whole (the percentage in its
heading). Each part has a weight inside its area: x1 an extra, x2 important, x3 essential. An area's value is the
weighted mean of its parts, and the overall value the weighted mean of the areas. After changing a table, run
`python tools/status_bars.py` to redraw the bars.

✅ done · 🔧 in progress · 🗺️ roadmap

### Core game (45%)

| | Part | Progress | Weight | What it means |
|---|---|---|---|---|
| ✅ | **Recompiler** | ![100%](media/progress/100.svg) | x3 | The whole game (main, sdk and subsdk modules) is translated to native code on your PC |
| 🔧 | **Horizon OS services** | ![95%](media/progress/95.svg) | x3 | Threads, files, saves, controllers, applets and time: everything the game uses. Online services aren't provided |
| 🔧 | **Maxwell shader translator** | ![98%](media/progress/98.svg) | x3 | Every shader of the game's archives (about 32,800 programs) translates to Vulkan |
| 🔧 | **Graphics (NVN on Vulkan)** | ![95%](media/progress/95.svg) | x3 | Every kingdom draws on NVIDIA, AMD, Intel and the Steam Deck; the glow and glare match the emulator since 0.8.0. Open: black dust squares on some AMD cards ([#103](https://github.com/Simon3Sarkis/super-odyssey-pc-recomp/issues/103)), a crash on older Intel graphics ([#122](https://github.com/Simon3Sarkis/super-odyssey-pc-recomp/issues/122)), a tutorial video with FSR 3.1 ([#66](https://github.com/Simon3Sarkis/super-odyssey-pc-recomp/issues/66)) |
| 🔧 | **Tested in game** | ![95%](media/progress/95.svg) | x3 | Every kingdom is visited by scripted tours on both game versions before each release, and players have finished the game at 100% on the port ([#117](https://github.com/Simon3Sarkis/super-odyssey-pc-recomp/issues/117)). Open crash reports wait for their reporters |
| 🔧 | **Sound** | ![95%](media/progress/95.svg) | x2 | The game SDK's audio renderer with reverb and 5.1 surround. Open: crackling in cutscenes on a low-end laptop ([#126](https://github.com/Simon3Sarkis/super-odyssey-pc-recomp/issues/126)) |
| ✅ | **Game files setup** | ![100%](media/progress/100.svg) | x2 | The start screen copies your extracted game, builds the game code and prepares the shaders |
| 🔧 | **Game versions 1.0.0 and 1.4.1** | ![95%](media/progress/95.svg) | x1 | Both play, and the start page shows which one is installed. 1.4.1 ships without function names, so a few test tools reach it later |

### Platforms and input (15%)

| | Part | Progress | Weight | What it means |
|---|---|---|---|---|
| 🔧 | **Windows** | ![98%](media/progress/98.svg) | x3 | The main platform: every release is played on it before it goes out |
| 🔧 | **Steam Deck** | ![90%](media/progress/90.svg) | x3 | 1280 x 800 at 60 FPS in Gaming Mode, tested on a Deck before every release. Open: the Deck's own gyro ([#68](https://github.com/Simon3Sarkis/super-odyssey-pc-recomp/issues/68)) |
| 🔧 | **Linux** | ![85%](media/progress/85.svg) | x2 | An AppImage, checked by the AppImage catalog's own test before every release. Less tested than Windows |
| 🔧 | **Controllers** | ![95%](media/progress/95.svg) | x2 | Xbox, PlayStation, Switch Pro and a Joy-Con pair, with matching button icons and rebinding; shake on any button |
| 🔧 | **Keyboard and mouse** | ![95%](media/progress/95.svg) | x2 | Rebindable keys, shake and spin throw on any key, a mouse camera with smoothing, the mouse in the game's own menus and the wheel on the map |
| 🔧 | **Motion controls** | ![70%](media/progress/70.svg) | x1 | PlayStation and Switch Pro controllers. Not yet the Steam Deck's built-in gyro |

### Performance (15%)

| | Part | Progress | Weight | What it means |
|---|---|---|---|---|
| 🔧 | **No shader stutter** | ![95%](media/progress/95.svg) | x3 | Every shader is prepared before play and again after a driver update. A rare effect nobody has recorded can still hitch once |
| 🔧 | **Frame pacing** | ![95%](media/progress/95.svg) | x2 | The game's timer is locked to when the screen shows each frame |
| 🔧 | **Frame rates above 60** | ![90%](media/progress/90.svg) | x2 | 72 to 240 FPS with every frame really drawn. Scenes not prepared for it (painting transitions) play at 60 |
| 🔧 | **Memory** | ![95%](media/progress/95.svg) | x2 | Steady over long sessions, checked in every kingdom before each release |
| 🔧 | **Low-end PCs and handhelds** | ![80%](media/progress/80.svg) | x2 | A low-end PC preset, Handheld mode and lower resolutions. Open: older Intel integrated graphics ([#122](https://github.com/Simon3Sarkis/super-odyssey-pc-recomp/issues/122)) |

### Graphics options (12%)

| | Part | Progress | Weight | What it means |
|---|---|---|---|---|
| ✅ | **Resolution and screen shape** | ![100%](media/progress/100.svg) | x3 | Up to 4x internal resolution, 16:9, 16:10, 21:9, 32:9, 4:3 or Fill, the HUD at the edges |
| 🔧 | **Upscalers** | ![95%](media/progress/95.svg) | x3 | NVIDIA DLSS / DLAA, AMD FSR 1 / 2 / 3.1 / 4 (FSR 4 on Radeon RX 9000 only), Intel XeSS, with the HUD drawn at full resolution |
| 🔧 | **Frame generation** | ![85%](media/progress/85.svg) | x2 | AMD FSR (Windows) and NVIDIA DLSS (beta) |
| ✅ | **Anti-aliasing, shadows, textures** | ![100%](media/progress/100.svg) | x2 | FXAA, SMAA, 2x / 4x shadow maps, shadow distance, texture detail, anisotropic filtering, smooth gradients |
| 🔧 | **Lighting and effects** | ![95%](media/progress/95.svg) | x2 | Ambient occlusion (Off / Medium / High), bloom (Switch / High / Off), motion blur on or off |

### Player features (13%)

| | Part | Progress | Weight | What it means |
|---|---|---|---|---|
| 🔧 | **Start screen and settings menu** | ![98%](media/progress/98.svg) | x3 | Play first, categories as tabs, every option explained, the game's live picture beside it |
| ✅ | **Built-in updates** | ![100%](media/progress/100.svg) | x2 | Checks this page and installs the new version from the menu |
| 🔧 | **Crash reports and logs** | ![95%](media/progress/95.svg) | x2 | A report with a plain-words summary and a signature to match crashes between players, and a menu input recorder |
| 🔧 | **Mods** | ![85%](media/progress/85.svg) | x2 | RomFS mods, switched on and off in the menu. Code mods (exefs patches) can't work: the code is rebuilt from the original |
| 🔧 | **Languages** | ![85%](media/progress/85.svg) | x1 | The game's own languages; the port's menus in English, Korean and Brazilian Portuguese. Simplified Chinese offered ([#134](https://github.com/Simon3Sarkis/super-odyssey-pc-recomp/issues/134)) |
| ✅ | **Discord activity** | ![100%](media/progress/100.svg) | x1 | The kingdom you're in on your profile (off by default) |
| 🔧 | **amiibo and extras** | ![90%](media/progress/90.svg) | x1 | A virtual amiibo figure, a screenshot key, Snapshot Mode at the full frame rate, first-person facing Mario |
