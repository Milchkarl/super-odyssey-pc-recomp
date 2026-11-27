## New

- **macOS (Apple Silicon), experimental**: a macOS version (`.dmg`) is now built with each release, thanks to @dos-ise (#170). It is a first version: the Metal renderer is still in progress, and we haven't been able to test it ourselves - please report what you see.

## Fixed

- **Missing particle effects (1.0.3)**: some particle effects weren't drawn in 1.0.3 on Windows and Linux; they are back.

## Still being looked into

- **Old saves freezing on "Continue" (#175, #179)**: the save we received loads normally on our PCs (1.0.2 and 1.0.3, game 1.0.0 and 1.4.1). If it happens to you, please attach the log from that session (`SuperOdysseyRecomp.log`, or `SuperOdysseyRecomp.previous.log` if you started the game again).
- **Spin throw key (#185)** and **Shake key after a load (#168)**: they work on our PCs; we've asked for details.
- **Intel UHD 620 flicker and memory errors (#183, #169)** and **a crash in the New Donk City sewer on Linux (#181)**.

After updating from 1.0.3 the shaders are prepared once at the first start.

Thank you for reporting all the issues and for the logs.
