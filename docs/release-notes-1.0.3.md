## Fixed

- **Stutter while moving (#177)**: 1.0.2 left part of the game code unoptimized; on CPUs with 8 threads that showed as stutter while moving through a kingdom. All of it is optimized again.
- **Black squares under Mario's dust on AMD cards (#103, #174)**: the dust's shadow is now drawn correctly on AMD's driver. Tested on an AMD Radeon 780M; not yet on a desktop Radeon.
- **Steam Deck not starting (#180)**: a damaged shader cache no longer crashes the game; the damaged shaders are made again.
- **Crash during the first start (#184)**: losing the picture in exclusive full screen while the game code builds no longer stops the game.
- **Choosing another graphics card**: on a PC that also has an NVIDIA card, picking the other one in Graphics card no longer stops the game at start.
- **Higher frame rates (90 / 120 / 240 FPS)**: Mario, his cap and the camera move smoothly between frames, without the doubled look.
- **RecompLauncher Easy Setup**: the launcher now sets the game up from your copy (thanks to the RecompLauncher team). When the shaders need preparing, it shows on the start page instead of the old small window.
- **Waluigi amiibo (#164)**: the game now recognizes it.
- **Menu translations (#162, #171)**: the texts that couldn't be translated now can, including the graphics card's description.
- **Logs**: the log of the last session is kept (`SuperOdysseyRecomp.previous.log`), and a long black screen writes what it is waiting for.
- **Chinese (Simplified) menu (#178)** and the **macOS build workflow (#170)**: thanks to the contributors.

## Still being looked into

- **Old saves freezing on "Continue" (#175, #179)**: not reproduced on our PCs yet. If it happens to you, please attach your save and the log from that session.
- **Spin throw key (#185)** and **Shake key after a load (#168)**: they work on our PCs; we've asked for details.
- **Intel UHD 620 flicker and memory errors (#183, #169)** and **a crash in the New Donk City sewer on Linux (#181)**.

After updating, the game code is rebuilt and the shaders are prepared once at the first start (a few minutes on a PC, longer on a Steam Deck).

Thank you for reporting all the issues and for the logs.
