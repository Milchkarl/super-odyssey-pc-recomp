# AI handoff: goals, state and how to work on Super Odyssey PC Recomp

This is the brief for any AI model (Claude Opus, Codex, others) working on this project. Read it fully before
changing anything. The owner provides the hardware (a Windows PC and a Steam Deck) and does the hands-on play testing;
your job is to make measurable progress toward the goals below and to prove it with numbers.

Last updated 2026-11-15, on branch `linux-deck` at commit 6734b81 plus the dev-loop scripts.

---

## 1. The goals (in the owner's words, turned into targets)

| # | Goal | Measurable target |
|---|------|-------------------|
| G1 | **Absolutely zero stutters**, Windows and Steam Deck | No frame over 2x the target frame time in a 10-minute play session (the `[hitch]` log line, `ODYSSEY_HITCH_MS`), including the first visit to every kingdom (shader compiles) and loading zones. |
| G2 | **A Steam Deck mode**: native 16:10 (1280x800), and higher-than-Switch settings, because the Deck is roughly 6-7x a Switch | Deck holds a locked 60 FPS (90 later) in Bonneton (Cap Kingdom town, ~1,500 draws a frame) and in the heaviest kingdoms (Metro, Seaside, Luncheon), at >= the Switch's docked resolution, with 16:10 filling the screen without stretching. |
| G3 | **Runs on very low-end hardware** | A defined floor (e.g. a 4-core CPU with integrated graphics, 8 GB RAM) runs 30 FPS stable. Measure memory and CPU use; don't add cost to the low end for high-end features. |
| G4 | **Less shader compile time, fewer resources**, with high FPS and stable frame times; **all vsync options** | Shorter "preparing shaders" step on first start; a vsync menu: off, on (FIFO), adaptive (FIFO relaxed), mailbox/fast, with a frame cap. Lower RAM / VRAM use. |
| G5 | **1:1 with the Switch version in every aspect** | Picture, physics, timing, audio and behaviour match the console. Any enhancement is optional and off by default unless it is invisible (e.g. resolution). |
| G6 | **No slowdown when frames drop; 30 is the baseline; up to 360 FPS with no animation speed-up** | Game speed stays real time from 30 to 360 FPS. Below 60, the game catches up (no slow motion). Above 60, more frames are drawn but animations and physics keep their speed (interpolation, not faster ticks). |

The current priority is **G2 and G6 on the Steam Deck** (the Deck's frame rate is not good enough yet), with G1 as the
standing rule for every change.

---

## 2. What the project is

- A **static recompilation** of the Switch game's ARM64 code into C (tools/gen.py, tools/lifter.py), built into a
  native program, with the Switch OS (HLE services in runtime/services, runtime/kernel.c) and the **NVN graphics API
  emulated on Vulkan** (runtime/nvn, runtime/nvn/vulkan).
- Game versions: **1.4.1** is the one being tuned (some fixes are 1.4.1-only, e.g. the SZS decompressor override at
  0x97e420 in runtime/hle/odyssey_120.c). 1.0.0 runs too but is not the focus.
- Platforms: Windows (main), Linux / Steam Deck (AppImage), macOS (MoltenVK).
- Threads: the game's emulated cores (gated in kernel.c), a render thread (runtime/nvn/rqueue.c) that turns NVN
  commands into Vulkan, a submit thread and a presenter thread (Linux default), shader workers, a translator.

---

## 3. Where things stand (measured, not guessed)

### Released
- **0.2.10** (public): config page before the game, red-outline black theme, async shaders on by default, DLSS off by
  default, PC stutter fixes (decompressor off the core gate, pipeline-cache autosave only after real compiles).
  The release notes say the Deck isn't playable yet and recommend 60 FPS.

### Local only (not pushed), on `linux-deck`
- **Early fences** (Linux default): the game's `nvnSyncWait` waits only for the GPU work that wrote game memory, not
  for the whole frame. Submissions split after such writes, at most 8 a frame, and none when a submit costs > 1 ms.
- **Placed GPU-owned guest heap** (Linux default, `ODYSSEY_VK_PLACED_HEAP=0` turns it off): the game's heap is
  Vulkan memory mapped at the guest address (VK_EXT_map_memory_placed). This removed ~3 ms per submit that the
  amdgpu driver spent re-checking the old user-pointer import.
- **Catch-up clock** (Linux default, `ODYSSEY_GAMEPLAY_CATCHUP=0` off; runtime/gameplay_clock.h): below 60 FPS the
  game runs up to 3 game ticks per frame so game speed stays real time. At a forced ~35 FPS: 120-122 ticks per 2 s
  with it, 62-74 without.

### Steam Deck numbers
- Scripted Bonneton walk (bench below), defaults: **56-60 FPS** (was 42-46 before the fixes).
- The owner's manual play on the same build: **30-56 FPS** after reaching Cap Kingdom. ~1,100-1,400 draws a frame,
  GPU busy ~1,050-1,250 ms of every 2 s (i.e. GPU-bound at times), render thread ~45-52% of a core, main game thread
  ~50-63%, a decompressor thread up to 68%. **Why the manual run is slower than the scripted one is the open question.**
- **Reproducible case (2026-11-15):** `tools/deck_dev.sh bench <name>` with `FULL=1 USER_INI=... WARP=world:CapWorldHomeStage@1700`
  and *no* `INPUT` (Mario stands where the warp puts him) holds a steady **40 FPS**, ~1,485 draws a frame, while
  **nothing is saturated**: main game thread (p44) 66%, render thread 52%, GPU ~55% busy, splits 0. The stats show
  `SyncWait 82x 753 ms` per 2 s: the game waits ~9 ms of every ~24 ms frame in `nvnSyncWait` for the render thread to
  reach the fence (rq_wait on the ticket in runtime/nvn/objects.c). The game thread (~16 ms of work) and the render
  thread (~12.7 ms) run in series instead of overlapping; overlapped, this scene would be ~60 FPS.
  **This is the lead to follow first.** Find which fence the game waits on (current frame or previous) and why
  the render thread is behind it. Before returning earlier, consider what the game does after the wait: it may
  overwrite memory that the render thread still reads (uniforms, vertices). Fixes might be render-thread speed, a
  snapshot of the data the render thread reads, or pipelining.
- The Deck has a **15 W budget shared by CPU and GPU**: CPU time saved gives the GPU clock headroom. Cutting CPU work
  (render thread, lifter code quality) therefore raises GPU-bound frame rates too.

### What exists for the goals
- Frame rate: 60 / 90 / 120 (`frame_rate`, runtime/settings.h; 90/120 interpolate with gameplay at 60). **Nothing
  above 120, and no 30 baseline mode yet.**
- Aspect: 16:9 or fill (`ASPECT_16_9`, `ASPECT_FILL`). **No proper 16:10 mode yet** (fill stretches).
- Deck defaults: `settings_battery_saver()` when `plat_is_steam_deck()` (75% resolution + FSR 1).
- Present modes: only environment switches (`ODYSSEY_PRESENT`), **no vsync menu**.
- Handheld mode: `ODYSSEY_HANDHELD=1` makes the game run its handheld mode (lower resolution, as on a Switch in
  handheld), but **the picture is placed wrongly (top-left)**; unfinished.

---

## 4. Priorities (do them roughly in this order)

1. **Deck: why manual play is 30-56 FPS.** Start with the 40 FPS standing case in section 3: the game thread waits
   in `nvnSyncWait` for the render thread, so the two run in series. Then reproduce with the bench (section 6) in the places the owner plays,
   compare the stats lines (splits, skipped splits, submit ms, GPU ms, thread CPU). Decide CPU-bound vs GPU-bound per
   place before changing code.
2. **Deck CPU cost.** Biggest lever on a shared-power device:
   - Lifter output quality: keep guest registers in C locals across a function instead of memory, and compute
     condition flags lazily (only when read). Changes in tools/lifter.py need the full build.
   - Render-thread cost per draw (state hashing, descriptor updates); the decompressor's cost.
3. **Deck mode / 16:10**: a proper 1280x800 output (game rendered at a 16:10 viewport, or 16:9 with the HUD placed
   correctly, without stretching), and fix the handheld-mode placement. Profile at the Switch's docked resolution and
   above.
4. **Frame-rate range (G6)**: a 30 FPS cap that behaves exactly like the Switch at 30, catch-up for drops, and
   interpolated rates past 120 up to 360 with animation speed unchanged. Windows needs the catch-up clock enabled and
   tested too (it's Linux-only by default today).
5. **Vsync menu (G4)**: off / on / adaptive / fast (mailbox) plus a frame cap, in the config page and the in-game menu.
6. **Shader compile time (G4/G1)**: shorter warm-up, a better seed cache (data/pipeline-seed.bin), no compile
   stalls on first visits.
7. **Low-end floor (G3)**: memory mode, resolution scaling, and making sure each feature has an off switch.

Every change: G1 (no new stutters) and G5 (1:1) are acceptance criteria, not extras.

---

## 5. Things already tried that were wrong (don't repeat)

- Setting the Deck's GPU clock by hand: not the cause.
- `ODYSSEY_NO_GMEM`'s apparent speed-up: it made draws blank (lost text), not faster.
- Forcing 2x resolution to simulate a slow GPU: the game lowers its dynamic resolution instead. To force ~35 FPS use
  `ODYSSEY_TEST_COUNTER_DELAY_MS=22`.
- ASTC decoding and the watchdog were not behind the 200 ms frames (that was a GPU-memory read in kbprompts.c).
- "Game Mode vs Desktop Mode" on the Deck: not the cause.
- Renaming the test exe on Windows: the game library binds to `SuperOdysseyRecomp.exe` by name; keep the name.

---

## 6. How to work: build, test, measure

### Windows (PC)
- Build: docs/BUILD-windows.md. From Git Bash, put the msys tools on PATH **only for make, in a subshell**:
  `(M=<msys root>; PATH=$M/ucrt64/bin:$M/usr/bin:$PATH make ...)`. Exporting that PATH for the game's launch breaks
  its environment.
- Test with the copy in `sopr-wintest`, never the owner's real install or saves.
- Scripted runs need `ODYSSEY_NO_CONFIG=1` (otherwise the config page waits for input).

### Steam Deck: the fast loop (seconds, not a full rebuild)
From the PC (Git Bash), with `DECK=deck@<ip>` in the git-ignored tools/release.local.sh:

```bash
tools/deck_dev.sh                      # copy the changed sources to ~/sopr on the Deck, rebuild only the runtime
tools/deck_dev.sh bench <name>         # ... and run the scripted Bonneton walk, print FPS / worst frame
tools/deck_dev.sh full                 # full AppImage (needed once, and after any lifter/generator change)
```

- Only files whose content changed are copied, so make stays incremental. A runtime-only change rebuilds in seconds.
- The quick build runs the AppDir directly (`build/linux/x86_64/AppDir/AppRun`); no AppImage packing.
- Bench options (environment): `FULL=1`, `USER_INI=/home/deck/sopr/bench/user.ini`, `WARP=world:<Stage>@<frame>`,
  `INPUT=...`, `SHOT=<frame>`, `HANDHELD=1`, `HITCH_MS=`, `PRESENT=`, `EARLY=`, `NOGMEM=` and more (see the header of
  tools/linux/deck/deck-bench.sh). Results and the game log land in `~/sopr/bench/<name>/` on the Deck.
- tools/linux/deck/deck-threads.sh and deck-gpu.sh sample per-thread CPU and GPU busy/clock while it runs.
- The Deck test copy of the game is `/run/media/deck/SN01T/sopr-fonttest`. Never use the owner's real Deck install.
- On the Deck: no sudo, no installs, no system setting changes. Start long jobs with `setsid nohup ... < /dev/null &`
  or `systemd-run --user` (plain nohup over SSH gets killed).

### What to read in a log
- `[vk] t=... frame N  X fps max Y ms ...` every 2 s: FPS, worst frame, draws, GPU ms, heap binds,
  `splits N (skipped M, a submission X ms)`.
- `[hitch]` lines (`ODYSSEY_HITCH_MS=<ms>`): a slow frame broken into wait / acquire / present.
- Thread names (Linux): `g<id> c<core> p<prio>` for game threads, `render`, `shader worker`, `translator`.

### A Linux PC
The owner can provide a Linux environment on the PC. Useful for RADV / Mesa profiling with a real GPU and faster
builds than the Deck; ask for it when a problem needs it.

---

## 7. Rules (non-negotiable)

- **Everything stays local.** No git push, no GitHub release, no issue/PR comments unless the owner says so in that
  conversation. Releases go through `tools/release.sh` (it tests before publishing).
- Never touch the owner's real game install or saves on the PC or the Deck; use the test copies above.
- Never print or log secrets from the owner's files. Keep the owner's personal details out of commits and outside
  services.
- Don't change system or security settings on any machine.
- Don't edit another agent's checkout (e.g. Codex's); read it and bring the changes over.
- Defaults that differ by platform (Linux-only today) must stay switchable by an environment variable, so a
  regression can be confirmed or ruled out in one run.
- Report results honestly: numbers from a run, which build, which place in the game. "Should be faster" is not a
  result.

---

## 8. Definition of done for a performance change

1. Before/after numbers from the same bench run (same place, same settings), on the device that matters.
2. No new `[hitch]` frames and no visual difference from the Switch (screenshot the same spot: `SHOT=<frame>`).
3. Windows still builds and runs; the change is off or neutral where it wasn't measured.
4. Committed locally with a message saying what was measured.
