# Work and validation after 0.2.9

Requested by Simon: work through all remaining items, keeping changes separately
reviewable and testing them properly. Base source: `1ac024d` (tag `v0.2.9`).
Private builds, save copies and captures stay under ignored `build/029-tests/`.
No production saves/install edits, pushes or release publication in this work.
Windows tests use logical processors 16-31 on the current 32-thread PC.

## Completed before this work

The old PRIORITY-TEST-QUEUE predates Claude's 0.2.9 work. Clean Windows dependency
packaging and Windows/Linux GitHub Actions builds are implemented; successful
builds and both release assets were checked. Crash reports, display selection,
the expanded overlay and fog readback are present. Poison readback now covers
1.4.1 as well as 1.0.0. Language, mouse, audio and Linux-font fixes shipped too.
An open issue can still require reporter confirmation after its fix ships.

## Ordered acceptance checks

| Item | Implementation / required evidence |
| --- | --- |
| 1.4.1 loading at 90/120 FPS | Deterministically reproduce `TextureDataEx::reset` before changing behavior. Test stock 60, normal 90/120, strict diagnostic extra draws, repeated stage changes, new-game opening and a matching 1.0.0 library. Verify requested cadence resumes after loading and gameplay remains 60 Hz. |
| Shadow distortion (#26) | Reporter case is RX 6600 XT / Cascade / 0.2.8. Obtain settings and 0.2.9 retest; local NVIDIA captures cannot establish an AMD fix. Test native, FSR and DLSS independently at 60 and experimental rates. |
| FSR jitter and distant bloom (#24) | Identify the exact FSR backend. Compare static and moving-camera sequences with native and DLAA controls, including lamps, hair, fences, particles and UI. Preserve each backend's sharpness/CAS state. |
| Full kingdom tour | All 17 actual destinations, adequate settling, fresh per-run save copies, logs and captures; compare with the same game version/save/settings/renderer inputs. Manually review differences. Separate fog/poison/combat/interior/cutscene tests remain required. |
| Graphics controls | Shadow resolution, draw/LOD/effect distances: stock defaults and bounded independent options; validate render correctness, culling, memory and frame time on both game builds. Do not claim that raising a render target alone increases shadow detail. |
| Ultrawide | 21:9 and 32:9 gameplay, HUD, menus, map, cutscene bars, transitions and restoring 16:9. A wide-window screenshot alone is insufficient. |
| VRR and display cap | Presentation cap independent of simulation; validate monitor selection, 60/90/120, 144 Hz output, fullscreen modes and VRR hardware pacing. A 144 Hz presentation cap does not imply 144 Hz game logic. |
| Deck controls | Importable Steam Input configuration, gyro and appropriate prompts; actual hardware, docking and controller reconnection. |
| Deck sleep/resume | Mid-game suspend/resume, audio, gyro/input, device/swapchain restoration, timing and saves. Requires the Deck. |
| Add to Steam | Explicit opt-in, correct quoted AppImage/data paths, duplicate detection, closed-Steam handling and rollback; fixtures plus real Steam verification. |
| FSR Lite | Establish a compatible Vulkan ML provider/model and actual Deck support before exposing a setting. Preserve analytical FSR fallback; do not label it ML. |
| Camera controls | Independent horizontal/vertical sensitivity and inversion for stick, mouse and gyro. Preserve legacy settings and avoid changing gyro shake detection/capture controls. |
| Two-player Cappy | Explicit player-two selection, player-one retention, correct HID IDs, disconnect/reconnect and game co-op flow; requires two-controller testing. |
| Vendor frame generation | Actual supported NVIDIA/AMD/Intel implementations, UI composition, pacing and latency; distinguish vendor generation from existing game interpolation. GPU-specific tests required. |
| Sharper HUD/menu | Preserve stock layout, clipping, font fallback, shadows and post-processing on both game versions; test native/temporal upscalers and 1080p/1440p/4K. |

## Current loading investigation

Normal timing controls reached/loaded Wooded at 120 FPS and exited normally.
Strict alternating extra draws reproduced the known 1.4.1 fault twice on the
first Wooded draw: access `0x20` in `0x70009917d0` (`TextureDataEx::reset`).
Preparing graphics on gameplay frames as well as extra frames did not fix it;
that candidate was discarded.

Keeping asynchronous scene setup and its initial warm-up at stock 60 Hz, then
restoring the requested experimental cadence, passed the strict reproduction.
A normal 90 FPS test also loaded and captured Wooded. Further checks are in
progress; this is a scene-loading safeguard, not a general allocator rewrite.
`ODYSSEY_NATIVE_120_SAFE_LOAD=0` disables it for diagnostic negative controls.
It applies only to 1.4.1; stock 60 and the 1.0.0 draw path are unchanged.

The final default-enabled strict 120 test reached frame 3000 and exited zero.
The normal 90 run captured frame 3800; a normal 120 run captured frame 6500.
The latter logged effective cadence 60 followed by 120 and approximately 120
gameplay/controller ticks per two seconds during play. These are bounded
behavior checks, not CPU performance measurements (private compilation also
ran on the test processors). A matching current 1.0.0 library was rebuilt from
the owner's files without bypassing the library ID; its 120 FPS Cap test reached
frame 3300 and exited zero. Artifacts have descriptive run names in
`build/029-tests/`. Opening, repeated-transition and full-tour checks continue.

The published 0.2.9 Windows ZIP was downloaded for the renderer baseline and
verified against its release digest:
`393882cebd9a51d40c58b2a7d3dcc979d06f95f4a819799d2aeb517d84fae20a`.
Only runtime files were extracted for the private tour; its completed-save
source is copied separately for every run.

The tour runner now supports 90 FPS, explicit capture dimensions, processor
affinity and strict diagnostics. Timeout cleanup must actually terminate its
synthetic parent/child tree promptly; waiting for natural exit is not a pass.
The four comparator/cleanup checks passed in 0.432 s with process permissions.

Only observed or tester-confirmed checks may be marked passed. Unavailable
hardware, missing reproductions and pending tests remain explicit open items.
