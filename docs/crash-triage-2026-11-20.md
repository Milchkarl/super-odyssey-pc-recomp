# Crash triage, 2026-11-20 (after 0.5.2)

Open crash issues #115, #99 and #87, plus the report files in the owner's Downloads. #113 and #119 were already
handled and are not repeated here. Branch `claude/crash-triage-1cad20`; nothing pushed or posted.

## Issues

| Issue | Version | Cause | Status |
|---|---|---|---|
| #115 "closed with code -1073740791 after 77 s" | 0.5.0, RTX 5060 + AMD iGPU, 32 GB, started from RecompLauncher | Unknown. -1073740791 = 0xC0000409, the code every `FATAL` ends with (`abort()` -> fail-fast), so it says only that the game stopped on an error. The issue has the shader preparation's `cache/prepare-pipelines.log`, not the game's log: **started from a launcher (piped output), the game wrote no `SuperOdysseyRecomp.log` and no crash report at all.** | Logging fixed locally (18ec6f7), not released. Crash itself: needs the reporter's log on 0.5.2. Timing: a shader preparation started inside the game's process 34 s before the stop, and 0.5.1 changed that path (92f7737, the game restarts after it) - possibly related, not shown. |
| #99 "Game Crash" | 0.4.5, Pentium 4415U, Intel HD 610, 8 GB | Out of graphics memory creating a 1024x1024 texture at the first kingdom; 10.7 GB committed. | Partly fixed: 0.5.0 halved committed memory (2b6156a, thread stacks reserved, not committed). Still to fix: the 3200 MB guest heap import probably uses Intel's single ~3.2 GB heap (our allocator had 352 MB of it when the texture failed), which a bigger page file can't help. Untestable here (no Intel GPU); 4d3d8cd logs the import's heap and every heap's budget at the error, so a 0.5.2+ log confirms it. |
| #87 "Crash When pressing play" | 0.3.1, same PC as #99, game 1.0.0 | Illegal instruction in `odyssey_game.dll`: zig's runtime linked into the game code was built for AVX2 even for the SSE4.2 target. | Fixed in 0.4.4 (8f77a81, PR #92). The reporter confirmed it in #99 ("works without AVX and launches into the menu" on 0.4.5). Can be closed; what remains is #99. |

## Report files in Downloads

| File | Version / PC | What it is | Matches |
|---|---|---|---|
| `report-20261120-033159-9840-0.txt` | 0.5.0, GTX 1070, Ryzen 3 3200G, 65 min, Cascade | Access violation on the render thread: `backend_execute` -> command replay (nearest exports in the 0.5.0 exe: `renderer_interp_replay+0x4a9 / +0xf9d`; static functions aren't exported) | #113, fixed in 0.5.2 (1f3b015) |
| `report-20261119-030454-8920-0.txt` | 0.4.8, RTX 2060, 16 GB, Wooded | `FATAL: nvn: out of host memory for commands`, 57 GB committed | #75, the 0.4.8 leak near poison, fixed in 0.4.9 |
| `report-20261116-163437-10976-0.txt` | 0.3.1, Linux, RX 7800 XT | signal 11 at 0x20 in game code | old version; no open issue |
| `report-20261117-005758-21672-0.txt` | 0.3.1, RTX 3060 Ti | access violation | old version; no open issue |
| `super-odyssey-pc-recomp-report-145855-super-odyssey-pc-recomp.log` | 0.5.1, RTX 5070 Ti, DLSS-G, launcher | No crash: 14 min of play, ends with "window closed" | no issue (not #115: other PC and folder) |
| `super-odyssey-pc-recomp-report-145812/146730-prepare-pipelines.log` | 0.5.0, RTX 2080 Ti / RX 570 | Shader preparation logs only, no error; the same wrong-log pattern as #115 | not #115 (other PCs and folders) |
| `SuperOdysseyRecomp (1)-(4).log`, `SuperOdysseyRecomp.log` | 0.4.6-0.4.7 and older | No fatal line in any of them | - |

## Fixes on this branch

1. **18ec6f7 - a launcher start writes the log and crash reports (#115).** With stderr a pipe the game now logs to
   `SuperOdysseyRecomp.log` as without a console, and a thread passes the new lines on to the launcher every 50 ms
   (and at a fatal error, at exit and after a restart). `ODYSSEY_LOG_PIPE_ONLY=1` = the old way. Tested on the PC with a
   Python parent piping both outputs and `ODYSSEY_TEST_CRASH=vram`: old way 2 of 2 runs had no log file and no report;
   new 3 of 3 had both, the report leading with "Out of graphics memory (VRAM)", and the pipe got every line.
   A fatal error now also shows the game's own message box under a launcher (it already did without one).
2. **4d3d8cd - #99 diagnostics.** Log line at the import: "3120 MB (memory type 2 of types 0xc, heap 1 sys of
   76521 MB)" on the RTX 4090. The out-of-memory heap lines were compiled, not triggered.

## Next

- #99: if the log confirms the shared heap, import only the GPU pools' ranges or pick a system-memory heap when one
  exists (the low-memory session's note has the options). Needs an Intel iGPU to test.
- #115: read the reporter's 0.5.2 log; if it is the post-preparation path, test the start page's "Compile shaders now"
  under a launcher.

## Draft replies (for the owner to approve; nothing posted)

### #115

> Thanks. That code only says the game stopped on an error; the log that was attached is the shader preparation's,
> not the game's, so the reason isn't in it.
>
> - **Fixed (next version):** started from RecompLauncher, the game didn't write its own log or crash report. It will.
> - **Still to fix:** this crash; I can't tell the cause yet.
> - **What I need:** please try **0.5.2**. If it stops again, attach `SuperOdysseyRecomp.log` from the game's folder
>   (`C:\Recomp Library\super-odyssey-pc-recomp`), and the newest file in its `reports` folder if there is one. Starting
>   the game directly with `SuperOdysseyRecomp.exe` writes both on 0.5.2 already.

### #99

> - **Fixed in 0.5.0:** the game commits about half the memory it did (13 GB -> 6-7 GB in our tests).
> - **Still to fix:** on Intel graphics the game may still run out of *graphics* memory, which a bigger page file
>   doesn't help. I have no Intel GPU to test on.
> - **What I need:** please try **0.5.2** with Memory use on Low. Whether it starts or not, attach
>   `SuperOdysseyRecomp.log` (and the newest report from the `reports` folder if it crashes).

### #87

> Fixed in **0.4.4**: the illegal instruction came from a part of the game code built for newer CPUs. You confirmed in
> #99 that it now gets to the menu, so I'm closing this one; the memory crash after Play is tracked in #99.
