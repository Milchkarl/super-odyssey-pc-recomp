# CPUs without AVX2: how it was checked (2026-11-20)

A player with an AVX-only CPU (`[cpu] game code target: AVX (no AVX2)`, Radeon RX 570, Windows, 0.5.0) reported a
crash. Their bundle had only the shader-preparation log, which ends at `[prep] recorded ...`; that is where every
preparation log ends, an AVX2 player's too, so it doesn't show a crash. The game's main log
(`SuperOdysseyRecomp.log`) or the crash report is needed to say where it stopped.

## What runs on such a CPU, and the result

| Part | How it is built for an AVX-only CPU | Checked | Result |
|------|-------------------------------------|---------|--------|
| SuperOdysseyRecomp.exe | No `-march` (baseline x86-64); no `target(...)` attributes except AES (checked by CPUID first); no AVX2 / FMA intrinsics in runtime/ | objdump of the 0.5.2 code (build/dev) | No FMA, F16C, AVX2 vector or AVX-512. Its BMI2 / LZCNT instructions are zstd's: the same count per instruction as `libzstd.a`'s `*_bmi2` functions, which zstd runs only after its own CPUID check |
| Game code (odyssey_game.dll) | Compiled on the player's PC with `-march=sandybridge` (runtime/gamelib.c, gamebuild.c) | Built with `ODYSSEY_CPU_LEVEL=avx` (154 s), objdump of all 509 objects | Nothing beyond AVX: no FMA (the Switch's fused multiply-adds go to the C library's `fma`), no AVX2, no BMI |
| zig's runtime linked into the game code | Built ahead for baseline x86-64 (tools/zig_slim.sh, since #87) | zig_slim.sh refuses a runtime with anything beyond baseline, and `make dist` runs it for every release | Clean in every release since 2026-11-17 (0.5.0 included). A zig from before then (a 2026-11-15 package) still has AVX2 / F16C / BMI2 in compiler_rt, and that is what a test with an old package picks up |
| Shipped DLLs (SDL3, FFmpeg, libstdc++, XeSS, FidelityFX, DLSS, Streamline) | Vendors' builds | objdump | Their AVX2 / FMA code sits behind a CPU check (SDL's and FFmpeg's CPU flags, the MSVC STL's `__isa_available` test before the AVX2 loops); DLSS isn't loaded on AMD |

0.5.0 and 0.5.2 were built with the same CPU flags (`git diff v0.5.0 -- Makefile tools/zig_slim.sh runtime/gamelib.c
runtime/gamebuild.c`: no change to `-march` or the runtime's CPU).

Played: the 0.5.2 code with `ODYSSEY_CPU_LEVEL=avx` on the PC (Zen 5), save-cap, to Cap Kingdom: 3+ minutes at
60 FPS, no error. (This PC has AVX2, so it shows the AVX build plays, not that no AVX2 instruction is left. The
objdump checks are what cover that; Intel SDE `-snb` would be the full emulated test, not installed here.)

A real illegal instruction would show in the crash report as "Illegal instruction: the processor lacks an
instruction the game uses (CPU features)" (runtime/diagnostics.c), with the CPU's AVX2 / FMA / BMI2 flags.

## The guard added

`tools/check_cpu_baseline.py <file> [--dispatched <static library>]` fails on any instruction an AVX-only CPU lacks
(FMA, F16C, AVX2, AVX-512, MOVBE, RDRAND, BMI beyond the `--dispatched` libraries' own counts; TZCNT allowed).
`make dist` now runs it on the release exe with `--dispatched libzstd.a`, so a `-mavx2` / `-march` flag or an AVX2
helper in the runtime stops the release instead of crashing AVX-only players. Before, only zig's runtime was checked.

It also checks a game code build: `ODYSSEY_KEEP_GAMEBUILD=1` keeps `extracted/.gamebuild/*.o`; run the tool on them.

## Forcing the AVX target on a newer PC

Use a data folder of its own, so the shared test copy's AVX2 library isn't rebuilt:

1. A package folder: the exe, the DLLs, `odysseygen/` (with the current tools/*.py), `gamelib-src/` (runtime/cpu.h,
   gamelib.h, simd.h, simd_fp.h, tools/gamelib_entry.c, runtime/hle/hle_symbols.txt), `SuperOdysseyRecomp.dll.a`,
   and a `zig/` from a release made after 2026-11-17 (an older one has the AVX2 runtime above).
2. A data root with `extracted/exefs` copied, `extracted/romfs` as a junction and `extracted/romfs.bin` as a hard link
   to the test copy's (read only), and a copy of a save.
3. `ODYSSEY_CPU_LEVEL=avx ODYSSEY_DATA_ROOT=<it> ODYSSEY_SETUP_EXIT_WHEN_DONE=1 ODYSSEY_KEEP_GAMEBUILD=1` (plus the
   usual test settings): setup builds the game code with `-march=sandybridge` (`[gamebuild] ... for a CPU with AVX
   (no AVX2): -march=sandybridge`) and the library id ends in `-avx`. Then run again with the Cap Kingdom inputs.

Another session's test that closes every running SuperOdysseyRecomp.exe stops such a build too (seen once here: the
build simply ends with no error); run it again.
