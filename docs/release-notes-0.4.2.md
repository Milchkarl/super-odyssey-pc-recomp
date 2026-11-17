A small update that fixes the sound crackling.

## Fixes

- **Sound crackling during loads** (#83, #84), mostly on Windows: short gaps in the music and sound while the title screen starts, when you continue a save, and between scenes. The sound mixer waited far too long for the game during a load (up to 90 ms instead of 4), and could be kept from running while the PC was busy. Both are fixed. Measured on the same route: about 5 gaps a run before, none in the last five runs.

## Also

- If sound still crackles for you, the log (`SuperOdysseyRecomp.log`) now says how often the sound ran dry and what held it up. Please attach it to an issue.

## Updating

Windows: download the zip below and extract it over your current folder (or use **Updates** in the menu). Linux / Steam Deck: download the AppImage below. Your game files, saves and settings stay.
