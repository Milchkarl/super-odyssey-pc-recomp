## Fixed / new

- **Fewer shader stutters for new players**: setup now prepares every shader in the game, not only the ones on its lists. On the test PC, a first run through Mushroom and Metro Kingdom went from about 2,000 shaders built while playing to under 10.
- **The shipped pipeline list is used again**: it had gone out of date in 0.5.0, and it was skipped when the game files were already set up.
- **"Compile shaders now" on the start page** no longer leaves the shaders it compiled unused: the game restarts once when it is done (the window closes and opens again).

## Still to fix

- Short hitches when arriving in a kingdom (loading textures) and on some loading screens.
- Gyro aiming through binoculars turns left/right the wrong way (#53).
- Intel integrated graphics may still run out of graphics memory (#99).

## Plan

Smoother kingdom arrivals next, then gyro aiming.

## Updating

This update prepares the shaders once more, now for the whole game: about 2 minutes on the test PC (RTX 4090), about 6 minutes on a Steam Deck. It also uses about 0.4 GB more disk in the game's cache folder. The game code is not rebuilt.

Windows: download the zip below and extract it over your current folder (or use **Updates** in the menu). Linux / Steam Deck: download the AppImage below, or use **Updates** in the menu. Your game files, saves and settings stay.
