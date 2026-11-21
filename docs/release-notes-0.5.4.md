## Fixed / new

- **RecompLauncher support**: the launcher can now set the game up from your extracted game folder, shows the main settings on the game's page and backs up your saves. If you already play through the launcher, nothing moves.
- **Low GPU memory mode** (Performance, off by default): for 8 GB laptops with integrated graphics that crash with "Out of graphics memory" a few seconds in (#120, #99). Turn it on in Settings before pressing Play. Not yet confirmed on such a laptop: please tell us how it goes.
- **Lower CPU use**: the game's main thread does about 14% less work.
- **Settings cleanup**: options that can misbehave (Multithreaded rendering, Faster kingdom arrival, Only set up shaders seen before) are now under Performance > Experimental. Sharper HUD and menus is off by default for new players, since it could make the HUD look jagged.
- **Mixed game files**: if the game code is 1.0.0 but the game data comes from a later update, the game now says so at start instead of crashing at the title screen.

## Still to fix

- Frame rates above 60 aren't fully smooth yet.
- Two menu problems seen on the Steam Deck (a tab skipped with L / R, the menu opening on its last option) are not found yet; this version logs them when they happen.

## Updating

No game code rebuild and no shader preparation this time.

Windows: download the zip below and extract it over your current folder (or use **Updates** in the menu). Linux / Steam Deck: download the AppImage below, or use **Updates** in the menu. Your game files, saves and settings stay.
