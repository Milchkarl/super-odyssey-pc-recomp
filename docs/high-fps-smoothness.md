# Frame rates above 60 "not smooth at all" (Windows) - 2026-11-20

Players reported that 72 / 90 / 100 / 120 / 144 / 165 / 180 / 240 FPS look "not smooth at all" on Windows.

## Findings

Above 60 FPS the game still steps its world 60 times a second; the frames in between can show Mario's pose and the
camera at their own moment (runtime/hle/odyssey_120.c, odyssey_pose.c).

- **Frame delivery is even** at every rate (ms between presents, standard deviation, VSync Off): 60: 0.48 /
  120: 0.32-0.46 / 144: 0.38-0.46 / 165: 0.51; VSync Fast: 120: 0.46, 144: 0.44. Timing is not the problem.
- **On 1.4.1 nothing is drawn between ticks.** Mario's in-between pose needs `nn::g3d::SkeletonObj::CalculateWorldMtx`,
  which 1.4.1 inlines, so the pose sampling is off there (odyssey_pose.c; no `[pose-120] ... enabled` line in the log).
  The in-between camera hook read 1.0.0's registers on 1.4.1 (copy at `x23`, camera `x25`; on 1.4.1 the copy is on the
  stack at `x29 - 0xf0`, the camera `x0`, and the eye's x/y are still in `x8` at the hook) and did nothing. So on 1.4.1
  every tick is shown for 2 frames at 120 and 2-3 at 144: motion looks like 60, and at rates that aren't a multiple of
  60 it judders (2/3/2/3 frames per tick).

## What was tried, and what is in place (2026-11-20)

1. **Commit 57e930a: the camera hook with 1.4.1's registers** - the camera moved between ticks (its heading step per
   frame went from sd 1.09-1.40x of the mean to ~0.3x at 120 / 144 / 165). **Wrong without Mario:** Mario stayed on the
   tick while the camera moved, so against the camera he went back and forth every frame. The owner saw it at 120:
   "he looks like he's doubling". Measured (Mario walking, the camera turning, `ODYSSEY_LOG_CAMERA=1`; Mario's on-screen
   angle, second difference per frame, and how often his on-screen motion reversed in 10 s):

   | | 120 FPS | 144 FPS |
   |---|---|---|
   | camera on the tick (as at 60) | 0.051 deg, 0 reversals | 0.055 deg, 0 reversals |
   | 57e930a (camera between ticks, Mario on the tick) | 0.173 deg, 854 reversals | 0.188 deg, 1,051 reversals |
   | camera orbit only (target on the tick) - tried | 0.144 deg, 913 | 0.153 deg, 1,136 |
   | **now: camera on the tick on 1.4.1** | **0.051 deg, 0** | **0.055 deg, 0** |

2. **Moving Mario's skeleton by its share of the tick's movement on 1.4.1** (the part of the pose sampling that doesn't
   need `CalculateWorldMtx`) - tried, not kept. The palettes were made and the renderer read them, but in consecutive
   window shots (`ODYSSEY_NATIVE_120_STRICT=1`, which alternates tick / in-between frames) Mario still moved only ~0-2 px
   on in-between frames and ~17-20 px on tick frames, even with the offset exaggerated 8x. On 1.4.1 his drawn skeleton
   evidently doesn't come (only) from the palette this path replaces. Needs its own investigation.
3. **Orbit-only camera** (look at the tick's target from an in-between direction) - tried, not kept: Mario still moved
   back and forth on screen (table above).

**In place now:** `camera_inbetween()` in odyssey_120.c - the camera moves between ticks only where Mario's pose is
sampled too (1.0.0); on 1.4.1 it stays on the tick, exactly as before 57e930a. `ODYSSEY_CAMERA_INBETWEEN=1` forces the
in-between camera (reproduces the doubling on 1.4.1); `=0` turns it off on 1.0.0. The 1.4.1 register fix stays (the
hook reads the right camera; `ODYSSEY_LOG_CAMERA` logs it).

Checked: 120 and 144 on 1.4.1, the first Cap Kingdom visit, walking, the camera turning, Cappy thrown (scripted Y):
Mario's on-screen motion identical to the tick camera; six consecutive 4K window shots at 120 show each tick twice, one
Mario, intact. Not checked: Cappy in flight in a still (the shots caught Mario with Cappy on his head), 1.0.0 (no 1.0.0
test copy here; its path is unchanged in behaviour), rates other than 120 / 144, the Steam Deck.

**So on 1.4.1, rates above 60 currently give 60 FPS motion** (with lower input latency and even frame delivery) -
smoother motion there needs Mario's (and Cappy's) pose drawn between ticks on 1.4.1 first, then the camera with it.

## What still limits smoothness above 60 (not fixed, by design or by the display)

- **Even where in-betweens work (1.0.0), only Mario and the camera are drawn between ticks.** Enemies, platforms, other characters, particles and HUD
  animations move 60 times a second. At 120 each of their positions shows for exactly 2 frames (even). At rates that
  are not a multiple of 60 (72, 90, 100, 144, 165) they show for 2 or 3 frames alternately (144: 2.4 frames per tick),
  a visible judder on moving objects. 120 and 180 / 240 are the even rates.
- **A frame rate that doesn't divide the display's refresh.** On a 165 Hz screen, 144 with VSync Off tears and with
  VSync Fast shows frames for 1 or 2 refreshes; when frames reach the screen: 144 FPS sd 0.86-1.10 ms, 165 FPS (Match
  display) sd 0.96 ms against 0.38 ms for the presents. "Match display" (the refresh itself, or a rate that divides it)
  is the smooth choice.

## Testing pitfalls found on this PC

- **RivaTuner Statistics Server** ran with a global 60 FPS limit (`Profiles/Global` `[Framerate] Limit=60`): every
  rate showed exactly 60.0 FPS, with `vkQueuePresentKHR` blocking ~12.6 ms a frame while the GPU needed ~5 ms
  (`ODYSSEY_VK_PROF=1 ODYSSEY_HITCH_MS=5`: "present call 12.5"). `DISABLE_RTSS_LAYER=1` does not avoid it (RTSS also
  injects via its hooks loader). The owner set the limit to 0 for these tests. Players with RTSS / NVIDIA frame caps
  see the same.
- Scripted runs start the game behind the current window; NVIDIA's background frame-rate cap can apply then.

## Diagnostics added

- `ODYSSEY_LOG_SHOWN=1` (pacing.c): each image the display showed - when, spacing from the previous, time since its
  present call (`[shown] t dt latency`, ms; needs present wait, on by default).
- `ODYSSEY_LOG_CAMERA=1` (odyssey_120.c): each stage draw's camera as drawn and as the game made it, and Mario's
  position at the last tick.
