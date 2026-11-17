Crash fixes, a reworked settings menu and start screen, and much clearer crash reports.

## Fixes

- **Crash entering a painting above 60 FPS** (#72, #85), on game versions 1.0.0 and 1.4.1: the painting's transition now runs at the game's own 60 FPS, and the kingdom goes straight back to your frame rate. Tested 7 times at 165 and 240 FPS on both versions without a crash (it crashed every time before).
- **Black picture, then a crash, with FSR frame generation on NVIDIA** (#77): the game asked the graphics driver about a swapchain it doesn't own. Tested at 4K with DLSS + FSR frame generation: a correct picture at about 118 FPS.
- **Broken picture with FSR 4 on older AMD cards** (#66): FSR 4 is now offered only on the Radeon RX 9000 series it's made for; other cards use FSR 3.1 (a saved FSR 4 setting switches over by itself).

## A new settings menu and start screen

- **Start screen:** a home screen with a big **Play** button (selected from the start: A, Enter or Start plays), **Settings** and **Exit**, and your main settings at a glance.
- **Settings menu** (in game and from the start screen): the categories in a sidebar on the left, the options on the right. Move the cursor onto a category to see it, press A / Enter / Right (or click) to go into it, B / Esc to come back, L / R (Q / E) to switch from anywhere.
- **A hint bar** at the bottom always shows what your buttons do, in your controller's own names (Xbox, PlayStation, Nintendo) or the keyboard's.
- **A cleaner black and red look**, with bigger, easier targets for the mouse and the controller.
- **The mouse in the game's own menus** (#64): left click confirms, right click goes back, the wheel moves up and down, and the pointer works in the pause menu, Options and more (Controls > Mouse in menus). Thanks to galaxyhf (#86).

## Crash reports and the log

- A crash report now starts with a **summary in plain words**: what happened, whether it was in the game's code, the graphics driver or the PC port, the last stage, frame rate, upscaler, memory use, game version and what started the game - plus a one-line signature to match the same crash between players.
- The log starts with the version and session, and has a short **status line every minute** (stage, FPS, worst frame, memory in RAM and committed).
- The log now counts **sound gaps** and what held the sound up, to find the crackling some of you hear during loads (#83, #84). If you hear it, please send your SuperOdysseyRecomp.log.

## Also

- The settings file is now **settings.ini** (was powermoon.ini); yours is renamed automatically at the first start and keeps all your settings.

## Updating

Windows: download the zip below and extract it over your current folder (or use **Updates** in the menu). Linux / Steam Deck: download the AppImage below. Your game files, saves and settings stay. The first start after updating prepares the shaders again once (a few minutes).
