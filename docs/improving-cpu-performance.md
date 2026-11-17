# Improving CPU performance (handoff, on hold since 2026-11-16)

Status: paused by the owner to work on v1.0. Everything below is committed locally (not pushed). Pick up from
"Next steps".

## Where it stands

Benchmark: `cpu240.sh` (session scratchpad; recreate from the recipe below): Windows, 1920x1080 window, frame rate
240, VSync off, upscaling off, internal resolution 1x, the `sopr-wintest` test copy with its save. The route is the
**New Donk City rooftop** (Odyssey landing pad) - not Mushroom Kingdom, as earlier notes said. A scripted walk in
circles turning the camera; "busy" = 2-second stats lines with more than 1,500 draws a frame (`ODYSSEY_VK_PROF=1`
log lines `[vk] t=... fps ... (N draws ...)`). Run each setup 2+ times: busy-view FPS varies about +/-5%.

| Busy views (240 FPS setting) | FPS | render thread per frame |
|---|---|---|
| Before (Windows default) | about 132 | about 5.3 ms |
| + Early frame hand-off (`early_fences=on`) | about 148 (4-6 runs) | |
| + Multithreaded rendering (`record_thread=1`) | about 179 (2 runs: 178.2, 179.3) | 5.7 -> 4.1 ms |

Commits:
- `e49e4ab` - BC5 `getenv` off the texture path (11% of the render thread); Performance > **Early frame hand-off**
  (Auto: on Linux/Deck, off Windows); `ODYSSEY_SAMPLE` sampling profiler.
- `41feb20` - Performance > **Multithreaded rendering** (off by default, read at start): a recording thread makes each
  draw's Vulkan calls (`exec.c` "Recording thread", `dq_*` / `D_*`). Everything else syncs first (`DQ_SYNCED` macros,
  `vk_cmd()`, `vk_alloc_set()`). Checked: six-kingdom tour (frames match on/off), pause/map at 75% (HUD layer),
  FSR 3.1, DLSS Quality. **Not** checked: the Deck, hours of hand play, frame interpolation (it's bypassed there).

Defaults since 2026-11-16 (owner): Early frame hand-off Auto = on everywhere; Multithreaded rendering Auto = on with an
NVIDIA GPU, off with AMD / Intel (Steam Deck: tested forced on, 60 FPS locked either way, 12.8 vs 12.9 W, no errors).

## What was learned

- **Why it was slow:** the game waits (`nvnSyncWait`) on the frame it just submitted. Without early hand-off the game
  thread, our translation and the GPU ran one after another. The game's main thread spent ~59% of busy views waiting:
  45% for the render thread to translate, 14% for the GPU.
- **With early hand-off on**, render-thread time maps ~1:1 to frame time: slowing it by 0.98 ms a frame (state filter
  off) cost 0.9 ms a frame. That is why the recording thread paid off.
- **Tried, no gain, reverted:** descriptor-set reuse (3.13 vs 3.15 us/draw); allocating descriptor sets 32 at a time
  per layout (descriptor time -4%, FPS unchanged: 147.4/147.6, 140.8/141.7); copying vertices to make early hand-off
  fully safe on Windows (96.6 FPS, much slower).
- The `libwinpthread` / `KERNEL32` shares in profiles are just thread start frames at the bottom of each stack.

## Tools

- `ODYSSEY_SAMPLE=<file>` (Windows): samples every thread ~1000x/s with up to 8 stack frames; `<file>.modules` lists
  module bases. Resolve with `addr2line -f -C -e <the same exe>` at `addr - exe_base + 0x140000000`. Note
  `GetTickCount64` steps 16 ms, so count samples per thread, not per timestamp.
- `ODYSSEY_SYNC_TRACE=1`: per 120 frames, where the game's fence waits fall and how long they take.
- `ODYSSEY_VK_PROF=1`: render-thread time by category in the stats line (pre / key / sets / tex / vtx).
- `ODYSSEY_VK_STATE_FILTER=0` slows the render thread on purpose - a quick test of whether it is the limit.

## Next steps (in order)

1. **Profile with both options on** (3-minute `cpu240.sh` run with `ODYSSEY_SAMPLE`): is the limit now the game's main
   thread, the render thread, or the hand-off between them? At 179 FPS a busy frame is ~5.6 ms, render ~4.1 ms.
2. **Move more per-draw work to the recording thread** if the render thread is still the limit: building descriptor
   writes (constant-data copies `ubo_span` / `copy_dedupe`, hashing), texture lookups. Estimate 10-20%.
3. **Smaller render-thread costs:** `plat_written_pages` (GetWriteWatch) ~4%; `pipe_recipe_hash` / `hash_words` a few
   percent (cache per draw state).
4. **The game's own thread** (~39% busy in busy views) becomes the ceiling after that: recompiler code quality, core
   gate overhead - slower, riskier.
5. **Steam Deck:** measure Multithreaded rendering there (15 W power budget: an extra busy core may cost GPU clocks).
6. Decide defaults: make both options Auto/on on Windows once the owner has played with them.
