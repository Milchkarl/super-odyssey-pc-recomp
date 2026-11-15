# Open bugs (2026-11-15)

Every open GitHub issue with its attached logs read, plus what came up in the owner's own testing. Status: **fixed**
(committed locally, not released), **cause known**, **needs repro**. Replies and closing issues happen at the end, by
the owner.

## Crashes

| # | Reports | What the logs show | Status |
|---|---------|--------------------|--------|
| **#40, #25, #19** | 0.2.10 RTX 4060; 0.2.8 RTX 2070 Super; 0.2.8 RX 6700 XT (Windows) | **120 FPS frame interpolation**: all three had it on. NVIDIA: `VK_ERROR_DEVICE_LOST` after the GPU wrote to an invalid address (`device fault: address write invalid 0` / `0x1d4fb000`); AMD: a crash inside amdvlk64.dll. #40: "crashes whenever I change a key bind ... only with 120 FPS interpolation on". | **Likely fix in** (in-betweens no longer replay draws that store to game memory: blended uniforms gave them garbage addresses). Not reproduced on the owner's PC (RTX, Cap Kingdom and the title screen, keyboard input): needs the reporters' retest. Interpolation is off by default; the option is now labelled experimental with a crash warning. |
| #25 (2nd report) | 0.2.8 RTX 2070 Super, windowed, interpolation off | Crash inside NVIDIA's driver (nvoglv64.dll), address 0x108. | **Needs repro** on a current build. |
| **#21** | 0.2.8 / 0.2.9, Linux, RX 7800 XT, **game 1.0.0** | Null pointer (0x98) in `al::ResourceSystem::createResource` (the game's resource loader), as the first level starts. romfs indexed 4636 files, nothing logged missing. | **Needs repro** with 1.0.0 on Linux (case-sensitive file names are a suspect). |
| Owner, Deck | 0.2.10+ dev, Gaming Mode | Crash report at every quit: a frame presented after the window was hidden; gamescope aborted ("surface already destroyed"). | **Fixed** (50ed7f8). |

## Steam Deck (owner's testing)

| Problem | Status |
|---------|--------|
| Resolution far below 1280x800: the game's own dynamic resolution (`al::GraphicsQualityController`, a nerve machine Wait -> Reduce -> Recover). | **Fixed**: setting **Dynamic resolution** (off by default on a Steam Deck) - the Reduce nerve's execute does nothing (1.0.0 0x7000a57f58, 1.4.1 0x70005ef6d0, from the RTTI). Deck, manual GPU clock 1000 MHz: docked stays 1600x900 at 53-60 FPS ~8.7 W, handheld 1280x720 at 58-60 FPS ~8.9 W. |
| 60 FPS at 7 W not reachable; target now **10 W** (+ manual GPU clock 1000 MHz recommended on the setup page). | In progress. |
| Steam keyboard popping up at launch / in the settings. | **Fixed** (6f264b6). |
| Stutters when new effects appear: game-thread stalls of 12-26 ms. | **Fixed**: a loading thread (priority 45) decoding SZS files held the game's main thread's core - its SVCs (file reads, locks) took the core back mid-decode. Decompression now runs off the core gate for its whole duration (`kernel_ungated_begin/_end`; `sead::SZSDecompressor` / `ParallelSZSDecompressor::tryDecompFromDevice`, both builds). Deck: gate waits in play 11-15 -> 0, first Cap Kingdom 59.4-60 FPS. Left: first-visit spikes when a new area's shaders build (one 64 ms frame in town). |
| Blocky stair-step edges on silhouettes against the sky. | **Needs comparison** with the Switch. |
| Motion controls: issues mentioned by the owner, not detailed yet. | **Needs details.** |
| #43 no sound on Steam Deck (0.2.10). The current build plays sound in Gaming Mode on the owner's Deck (recorded and measured); audio code unchanged since 0.2.10. | **Needs the reporter's log** (`[audren] device` line). |

## Performance

| # | Report | Status |
|---|--------|--------|
| #28 | Linux PCs drop 60 -> 5 FPS every few seconds (0.2.9 RX 9070 XT; 0.2.10 RTX 3060 Ti and a Steam Machine) | The 0.2.9 log matched #32 (fixed in 0.2.10). 0.2.10 reports remain: the Deck work since (compute queue, guarded writes, counter fix) may help - **needs a test on a Linux PC**. |
| #42 | Lake Kingdom: 27 FPS on sand (60 on grass); another player: a forest kingdom in slow motion | **Needs repro** (save / warp to Lake Kingdom). |
| #38 | Fossil Falls waterfall frame drops | **Needs repro**. |
| #13 | ROG Ally Z1 Extreme: slow motion at 30 FPS in corners | 0.2.10 changed present modes; the catch-up clock (game speed below 60) is Linux-only so far - Windows handhelds need it. |
| PC stutters on new effects | Owner's PC log (0.2.10): 31 frames held for shader builds in ~42,000. | **Fix in** (6f264b6: a short wait before holding a frame) - needs the owner's PC test. |

## Graphics

| # | Report | Status |
|---|--------|--------|
| #26 | Mario's shadow breaks into black squares in Cascade Kingdom (RX 6600 XT, 0.2.8) | **Needs repro** on a current build. |
| #25 | Rendering wrong with FSR 3.1 (0.2.8) | 0.2.10 fixed FSR 3.1 jitter (#24) - **retest**. |

## Input

| # | Report | Status |
|---|--------|--------|
| #40 | Key binds can't be changed on the launcher (configuration) page: the "press a key" prompt ignores mouse and keyboard. Mouse camera feels slow. | **Fixed** (the prompt only got presses while the in-game menu was open; now also on the configuration page) - needs the owner's test on the PC launcher. Mouse speed: a setting (25-400 %, default 100 %); default tuning goes with Phase 2's Windows controls. |
| #37 | Map flags / checkpoints can't be selected (PS4 controller, 1.4.1) | **Needs repro** (similar to closed #22, Switch controller). |
| #39, #27 | Map motion shake to a button | Feature - together with the owner's motion-control work. |

## Requests (not bugs)

#44 Korean in the F1 menu, #41 V-Sync option, #35 5.1 surround, #29 Brazilian Portuguese fan translation.
