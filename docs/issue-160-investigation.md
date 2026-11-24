# Issue #160: long starts, black-screen travel, stutter (2026-11-24)

Reports: starts of up to 2 minutes, travel between kingdoms stuck on black (music playing) for up to 2 minutes,
"sometimes", since 0.8.0; stutter back in the opening cutscene; one player (RTX 5080, 3440x1440, DLSS Quality) gets a
black picture after Play on 1.0.0 / 1.0.1 while the game's own overlay shows 60 FPS.

## How it was measured

The same recorded ride on game 1.4.1 every time: a 100% save, Mushroom Kingdom, then Metro (`ride-mushroom-metro`,
~3 minutes, two kingdom loads), started by script, the log stamped with wall-clock time. Per run: time to the first
frame, when each kingdom started, the longest frame gaps around each load, hitches over 50 ms in play, draws that
waited for a shader build, and the game's shader census. Scripts: the session scratchpad's `bench.sh`, `coldplay.sh`,
`benchsum.py`. PC: RTX 4090, the owner's PC, displays kept on behind a black overlay.

| Case | First frame | Mushroom / Metro start | Hitches in play | Worst frame |
|---|---|---|---|---|
| 0.7.0, fresh install, default settings, 1080p | 6.1 s | 34.0 / 64.2 s | 0 | 24 ms |
| 1.0.1, fresh install, default settings, 1080p | 6.0 s | 34.0 / 64.2 s | 1 | 57 ms |
| 0.7.0, fresh install, owner's settings, 4K | 9.6 s | 37.4 / 67.2 s | 0 | 44 ms |
| 1.0.1, fresh install, owner's settings, 4K | 9.7 s | 37.5 / 67.3 s | 2 | 55 ms |
| 1.0.1, owner's settings, pipeline cache not loaded | 11.4 s | 39.2 / 69.0 s | 1 | 52 ms |
| 1.0.1, a copy of the owner's real install (its caches) | 15.4 s | 43.1 / 73.0 s | 1-2 | 58-86 ms |
| 1.0.1 started on an install 0.7.0 set up (update) | **136 s** | 164 / 194 s | 1 | 48 ms |
| the same, second start | 9.7 s | 37.5 / 68.6 s | 1 | 54 ms |

(Each "fresh install" was set up the way a player's is: setup from an extracted game, then both shader passes.)

## What this shows

- **A start of ~2 minutes happens once after an update that changes the game code**: the game code is recompiled
  before the game starts (127 s on this PC; much longer on slower CPUs - ~20 min on a Steam Deck). The window shows the
  setup page's "Recompiling the game code..." meanwhile. 0.8.0 -> 0.9.0 -> 1.0.0 each changed the game code
  (patches for shadows, field of view...), so every one of those updates cost this once. 1.0.0 -> 1.0.1 did not.
- **On this PC, 1.0.1 is as fast as 0.7.0** in start, travel and stutter, with default or the owner's settings, fresh
  or with the pipeline cache not loaded. The travel black screens and long starts reported could not be reproduced here.
- **A played install starts slower**: with the owner's caches the first frame comes ~6 s later (9.7 -> 15.4 s). That is
  the start-up warm-up building the pipelines recorded in earlier sessions (12,166 in the owner's install, 15,415 on the
  Deck); it stops at 15 s. Pipelines recorded with other settings (each graphics option makes its own versions of the
  shaders) are warmed too.
- **The black picture with the overlay at 60 FPS** (3440x1440, DLSS Quality) is the game running, not loading. It did
  not happen here at 3440x1440 with DLSS Quality or DLAA. Needs that player's SuperOdysseyRecomp.log and settings.ini.

## The game code build (a new player's setup, and updates that change the game code)

Measured on the PC with 8 compile jobs, as a Steam Deck has (`ODYSSEY_GAME_JOBS=8`, per-file times with
`ODYSSEY_LOG_GAMEBUILD=1`): the generator takes 14 s; compiling the 508 files takes 1,422 s of CPU time and keeps all
8 jobs busy to the end, so the build is bound by the total work, not by one slow file.

- **Updates**: between 0.9.0 and 1.0.1 (an update that made everyone rebuild) 506 of the 509 compiled files were
  byte-for-byte the same. The build now keeps its compiled files (`<game files>/.gamecache`, ~150 MB, each named by a
  hash of its C source, compile command, headers and compiler) and reuses those whose hash it has. Measured as a player
  updating would (RecompLauncher's setup step, 8 jobs): 0.9.0's generator, then 1.0.1's - **2 files compiled, 506
  reused, 15 s instead of 133 s**; the same build again: 14 s. The ride afterwards plays as usual (first frame 11.0 s,
  Mushroom 38.7 s, Metro 68.6 s, worst frame 36 ms). It helps from the second update on (the first has nothing kept).
- **New players**: most of the system libraries and nearly all the video library never run (every kingdom visited with
  the function profiler, plus the recorded ride; our videos play through FFmpeg). A file without one function that runs
  is now compiled without optimization (`runtime/hle/hot_functions.txt`, made by `tools/hot_functions.py`; addresses
  only): ~0.4 s instead of up to 40 s each.

| Build (PC, 8 jobs, 1.4.1) | Time | Ride: first frame / Mushroom / Metro | Worst frame |
|---|---|---|---|
| Everything optimized (as 1.0.1) | 210 s (186 s in another run) | 10.6 / 38.5 / 68.4 s | 68 ms |
| Files with no function that runs unoptimized (shipped) | 139 s | 10.75 / 38.5 / 68.4 s | 45-83 ms |
| ... and the two giant game functions that run rarely | 113 s | 11.3 / 39.2 / 69.0 s | 33-81 ms |

The last row was left out: `main_092`'s 3,900-line function runs while the game starts, and unoptimized it cost ~0.7 s
at every start (`ODYSSEY_GAME_COLD=main_092,main_160` reproduces it). The unoptimized functions' stack frames stay
under 47 KB (the game threads have 1 MB).

## Start-up warm-up in the background

Every start built every pipeline ever recorded on that PC before the first frame (12,312 on the owner's install,
15,415 on the Deck; the list only grows), up to 15 s. Start-up now waits only for the first scene's pipelines (~1,000,
already built first), and the warm-up goes on in the background on the same threads at low priority, within the same
budgets (time, and memory - as a number of pipelines, measured on the first scene's second half). While it runs the
draw path takes its lock to add a pipeline, and the shared "interface part" tables of the pipeline libraries have their
own lock now (the multithreaded warm-up of 0.9.0 could already race there). `ODYSSEY_PREWARM_BG=0`: as before.

| A copy of the owner's install, 1.4.1, the ride | First frame | Mushroom / Metro | Hitches in play | RAM |
|---|---|---|---|---|
| Waiting for the whole warm-up (as 1.0.1) | 15.3 s | 43.1 / 72.9 s | 0 | 6.2 GB (1.0.1: 6.3-6.4) |
| Background warm-up | **10.8-11.2 s** | 38.6-39.0 / 68.5-68.8 s | 0-1 | 6.0-6.4 GB |

(In the last run the background warm-up built all 12,277 pipelines in 9 s while the game was on screen.)

### A 4-thread CPU (the PC limited to 4 logical cores, 2 warm-up threads)

The first version of the background warm-up held the first frame until it ended there (42-58 s). Three causes, fixed:
the warm-up built shader libraries while holding the lock the game's draws take; its threads and the programs the game
sets up at start all ran at low priority, so the builds a warming-up scene waits for got almost no CPU beside the
game's busy loading threads (one saved translation, normally ~1 ms, took up to 40 s); and the warm-up competed with
those builds. Now the warm-up's libraries are built outside its lock, the background part waits while the game has
shader jobs or a scene is warming up (that time isn't counted in its budget), and shader jobs stay at normal priority
while a scene warms up (low priority is for play, as before).

| Owner's install copy, the ride, 4 threads | First frame | Worst frame in play | RAM at 3 min |
|---|---|---|---|
| Old warm-up, before these fixes | 19.9 s | - | - |
| Old warm-up (`ODYSSEY_PREWARM_BG=0`), with the priority fix | 14.2 s | 61 ms (arriving in Metro) | 6.3 GB |
| Background warm-up, all fixes (12,137 pipelines warmed in the background) | **11.9 s** | 67 ms (arriving in Metro) | 6.4-6.7 GB |
| The same, all threads (10,039 warmed before its budget ran out) | 10.3 s | 27 ms after the arrivals | 6.2 GB |

On the owner's install most libraries the warm-up asks for come from NVIDIA's own disk cache, not the game's pipeline
cache (~0.8 ms each instead of ~0.1): by design, the setup's shader pass compiles most programs for the driver's cache
only, so the pipeline cache the game loads whole at every start stays small.

## Shader compiles left in play and in the opening

Measured with the driver's own creation feedback (`ODYSSEY_LOG_LIBCACHE=1`): a new player's first start (fresh setup,
1.4.1, new game, the opening cutscene into Cap Kingdom) made 1,480 shader libraries from the game's pipeline cache and
compiled 6; in the opening itself each took ~0.1 ms. The census used to call those cache hits "compiled" (now fixed).
On later starts and in the recorded ride nothing is compiled. The opening's long frames on this PC (40-64 ms, the same in
0.7.0, 1.0.1 and now) come where the game loads: the cutscene starting, Cap Kingdom starting. What remains costly is
before the first frame: ~760 cache lookups by the warm-up's 8 threads at once (8.4 s of driver time, ~1 s of wall time).

## Steam Deck (1.0.1 AppImage, the 1.4.1 test copy, the same ride)

- The game code had to be rebuilt for 1.0.1 (the test copy's was from 2026-11-16): **961 s (16 min)** before the
  first frame, on the setup page.
- Start-up warm-up: 15,393 recorded pipelines in 2.0 s.
- Arriving in the Mushroom Kingdom: one 544 ms freeze and a few frames not shown while full-screen passes built
  (shaders translated in play, 50-200 ms each: the test copy's shader preparation was older than this build and was
  skipped in this run). Metro: 280 ms worst. Then 60 FPS, worst frame 17-29 ms.

## Found on the way: the release's Steam Deck test doesn't reach the game

`tools/release.sh` step 4 starts the AppImage on the Deck's test copy and stops it after 120 s with "errors in the
log: 0". When a release changed the game code, the Deck has to rebuild it first (~20 min), so those 120 s were the
setup page's "Recompiling the game code...", and stopping the window ended the rebuild ("window closed during setup",
the generator then reports exit 255). The test copy's game code is from 2026-11-16: the Deck tests of the releases since
then never played. The step should wait for the rebuild (or the test copy should be rebuilt first) and check that a
stage scene ran.

## Not done yet

- A slower PC: the Steam Deck run (below) is the only one.
- The opening cutscene from a new game (all rides here start from a save).
