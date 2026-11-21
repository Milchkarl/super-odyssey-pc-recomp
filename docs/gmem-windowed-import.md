# Windowed guest heap import (#120, #99: "Out of graphics memory" on 8 GB PCs with integrated graphics)

2026-11-20, branch `claude/eager-khayyam-3386c0` on top of 0.5.3. Code: runtime/nvn/vulkan/exec.c, "Windowed import"
(gwin_*), cbuf_info, stage_set, gfast_prepare / gfast_finish, vcache_lookup / vc_flush, guest_direct.

## Cause

Shader global memory (Maxwell LDG / STG) is served by importing the game's whole heap as one GPU buffer
(VK_EXT_external_memory_host, `gmem_import`): 3120 MB on 1.4.1, 3200 MB on 1.0.0. The import is charged to the memory
heap of the host-pointer memory type. On an 8 GB laptop with integrated graphics that heap is Windows' shared GPU
memory, about half the RAM (3.6 GB in #120's report, 2.8 GB with a 2 GB UMA frame buffer). The import took almost all
of it, and the next allocation failed:

- #120 (Ryzen 5 7535HS, Radeon iGPU, 1.0.0): `vmaCreateBuffer ... failed: -2` (exec.c:1531 / :1565) ~12 s in, right
  after `[vk] guest heap imported for shader global memory: 3200 MB`; with the 2 GB UMA frame buffer `sr failed: -2`
  (exec.c:699) right after the import. Same in all four attached reports (0.5.1, 0.5.2; 25 % / 50 % resolution;
  arrival_precreate 0 / 1).
- #99 (Intel HD 610): VMA at 352 of its 3223 MB heap and a texture still failed right after the import.

`ODYSSEY_VK_NO_GMEM=1` (no import) is not a way out: the HUD loses its numbers (coins, health, Menu / Map labels) and
Metro drops from 120 to 60 FPS.

## What the shaders actually use

`ODYSSEY_LOG_GMEM_RANGES=1` (new, debug) logs every 2 s which heap MBs the storage buffers of gmem shaders cover, how
many gmem binds had no storage buffer, the widest draw and the MBs read in place as vertex data.
1.4.1, RTX 4090, title screen -> Wooded -> Metro -> Sand -> Seaside -> Lake:

- every gmem draw / dispatch had its addresses in bound storage buffers ("no sbuf 0", "outside heap 0");
- they cover **3-4 MB of the 3120 MB heap**, in two places (heap MB 1377-1379 and 1465-1467), in every kingdom;
- the widest single draw spans 284 KB; in-place vertex reads (outside the vertex cache) 1 MB.

The shader computes `address - HEAP_BASE` and indexes the gmem buffer with it. The addresses come from the driver
constant buffer (cbuf 0: c[0][0x110 + 16 i] vertex, 0x510 fragment, 0x310 compute), which this renderer writes itself.

## Approach

An option, **Settings > Performance > Low GPU memory mode** (settings.ini `low_gpu_memory = 0/1`), **off by default**
(owner, 2026-11-20: opt-in rather than chosen automatically). Off: the whole heap is imported, exactly as in 0.5.3.
On (applies at the next start; the row is on the start screen's Settings too, so it can be turned on before the crash):

- **Windows:** 16 MB-aligned pieces of the heap around the storage buffers in use are imported, each its own buffer, on
  first use. A new window that overlaps or touches existing ones absorbs them (the old ones are freed once the work
  recorded with them is done), so windows never overlap. Total capped at budget / 8 (256 MB - 1 GB).
- **Per draw / dispatch:** the window holding all its storage buffers is bound as its gmem buffer, and the addresses
  written into its cbuf 0 are rebased by the window's start. No shader changes (nothing to re-translate or re-prepare).
- The windows are the heap's own pages, so GPU stores still land in guest memory; guarded compute writes, the compute
  queue and the fast-copy table (gfast) work as before, with window-relative offsets.
- **Vertex / index data:** the vertex cache uploads changed pages from the CPU (the transient ring) instead of copying
  from the import. Ranges it doesn't take (GPU-written ones) are read in place from a window, made if needed.
- Not used with the Steam Deck's placed heap (VK_EXT_map_memory_placed): that path is untouched.

The log says which was chosen, with the import heap's size and budget (VMA's: VK_EXT_memory_budget where the driver
has it, otherwise 80 % of the heap's size), and one line per window:
`[vk] guest heap 3120 MB; import heap 1: size ... MB, budget ... MB, used ... MB -> ...`. With the option off and the
import over half of that budget, the line adds "Low GPU memory mode may help", and the "Out of graphics memory" crash
report's advice now names the option (runtime/diagnostics.c).

Test switches: `ODYSSEY_VK_GMEM_WINDOW=1` / `0` overrides the option; `ODYSSEY_VK_GMEM_BUDGET_MB=<MB>` stands in for the
heap's budget (the windows' cap and the log's hint). Menu row, option note, Korean and Brazilian Portuguese texts:
runtime/ui/menu.mm (tab_performance, under Memory use), option_notes.h, lang/ko.txt, lang/pt-BR.txt.

## Before / after (RTX 4090, 96 GB, 1.4.1 test copy, 1280x720 windowed)

The first runs below used the earlier automatic choice with `ODYSSEY_VK_GMEM_BUDGET_MB=3600` (#120's shared GPU
memory) to pick windows; the windows themselves are the same code the option turns on. Rechecked with the option
(Metro, 120 FPS cap, frames 4800-6500):

| | imported | FPS | upload per 2 s | HUD vs an earlier whole-heap run (coins / health+map, pixels) |
|---|---|---|---|---|
| option off (default) | 3120 MB, one import ("one import of the whole heap") | 118.6 | 325 MB | 0 / 22 |
| option on, budget 3600 MB | 32 MB, 2 windows | 118.7 | 340 MB | 0 / 51 |
| option on, real budget (61217 MB) | 32 MB, 2 windows | 118.6 | 339 MB | - |

Two whole-heap runs of the same spot differ by 0 / 63 pixels there (animated scenery behind the HUD). Earlier runs:

| | whole heap | windows (budget 3600 MB) |
|---|---|---|
| imported for shader global memory | 3120 MB | 32 MB (2 windows) |
| Wooded, 60 FPS cap | 60-61 FPS | 60-61 FPS |
| Metro, 120 FPS cap, frames 4800-6500 | 118.9 FPS | 118.6 FPS |
| Metro upload per 2 s | 326 MB | 346 MB (vertex cache pages now copied by the CPU) |
| 5-kingdom tour (Wooded, Metro, Sand, Seaside, Lake) | - | 60 FPS, still 32 MB in 2 windows, no failed window |

HUD: the screenshots match. Pixel differences (any channel > 16) in the coin / moon counter and in the health / Menu /
Map / compass rectangles are at the level of two whole-heap runs of the same spot (Metro: 0 and 51 pixels vs 0 and 63
between two whole-heap runs; Wooded: the only cluster near the counter is the animated plant behind it).

A/B against the unmodified 0.5.3 code (Metro, 120 FPS cap, frames 4800-6500, two runs each, interleaved):

| build | FPS | worst frame | upload per 2 s | GPU waits per 2 s |
|---|---|---|---|---|
| 0.5.3 unmodified | 117.4 / 118.6 | 48 / 17 ms | 321 / 325 MB | 31 / 15 ms |
| this branch, whole heap | 118.7 / 118.7 | 19 / 16 ms | 326 / 325 MB | 17 / 16 ms |
| this branch, windows (budget 3600 MB) | 118.7 / 118.7 | 23 / 16 ms | 340 / 340 MB | 20 / 15 ms |

The whole-heap path is unchanged; the windows cost ~15 MB more uploads per 2 s and nothing measurable here.
At a 120 FPS cap a CPU cost of a few percent would not show: on a slow iGPU the extra vertex-cache uploads are the
part to watch.

## Not tested

- A real 8 GB PC with integrated graphics (none here): the budget, the heap the import lands on and whether imports of
  16 MB pieces are accepted are what the reporter's test will show. The log lines above say it.
- 1.0.0 (the reporter's version, 3200 MB heap): the windows are made from what the game binds, wherever it is.
- The remaining memory on such a PC: the rest of the game's GPU memory (textures, render targets) still has to fit.

## Draft reply for #120 (for the owner to approve; not posted)

> Thanks for the detailed reports - your analysis was right. The game imported its whole 3.2 GB heap into GPU memory
> for shader access, which on an 8 GB laptop with integrated graphics takes almost all of the shared GPU memory.
>
> The test build below adds an option that imports only the parts of the heap the shaders use (about 32 MB in our
> tests). It is only tested on a desktop PC with the GPU memory limit simulated, not on integrated graphics yet.
>
> Could you try it? Unzip it and replace `SuperOdysseyRecomp.exe` in your 0.5.3 folder (keep a copy of the old one).
> Then, on the start screen, open Settings > Performance, turn on **Low GPU memory mode**, and press Play.
> Please tell us whether it gets past the first ~12 seconds, and if it still crashes, attach the crash report.
> The lines starting with `[vk] guest heap` in the log are the ones we need.
>
> Test build: <link>
