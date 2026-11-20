# Frame rates above 60 "not smooth at all" (Windows) - 2026-11-20

Players reported that 72 / 90 / 100 / 120 / 144 / 165 / 180 / 240 FPS look "not smooth at all" on Windows.

## Cause: on 1.4.1 the camera was never moved between gameplay ticks

Above 60 FPS the game still steps its world 60 times a second; the frames in between show Mario's pose and the camera
at their own moment (runtime/hle/odyssey_120.c, odyssey_pose.c). The camera part is a hook in `StageScene::drawMain`
where the scene makes its private copy of the `LookAtCamera`. It read **1.0.0's registers** (copy at `x23`, camera
`x25`) on **1.4.1** too, where the copy is on the stack at `x29 - 0xf0` and the camera is `x0` (the camera-yaw logger
and the DLSS camera code next to it already used the 1.4.1 address). On 1.4.1 it read stack garbage, its checks
rejected it, and it did nothing: **the camera stayed on each 60 Hz tick for 2-3 frames while Mario moved at the frame
rate.** Turning the camera or following Mario then judders at 60 under a smooth Mario, which is worse than plain 60.

1.4.1 also stores the copy's eye x/y one instruction after the hook (`stur x8, [x29, #-0xb8]`), so the fix takes
them from `x8` before reading the copy and hands the smoothed values back through `x8`.

Fix: `odyssey_camera_draw` picks the registers by version (1.0.0 unchanged).

## Measurements

PC: RTX 4090, one 3840x2160 display at 165 Hz, borderless, VSync Off, test copy (sopr-wintest, save-cap: first Cap
Kingdom visit), the camera turning (right stick held) from the same scripted start. "Camera step" = the change in the
camera's heading from one drawn frame to the next (`ODYSSEY_LOG_CAMERA=1`, which logs each frame's camera as drawn and
as the game made it, so before/after are from the same frames). Perfectly even motion = the same step every frame:
sd 0.

| Frame rate | Camera step sd, before | after | Frames with (almost) no camera movement per 10 s, before | after |
|---|---|---|---|---|
| 120 | 1.09x the mean step | 0.30x | 626 of 1,199 | 74 |
| 144 | 1.27x | 0.31x | 861 of 1,439 | 91 |
| 165 | 1.40x | 0.29x | 1,067 of 1,649 | 85 |

Before, about half of all frames repeated the previous camera (and the next one jumped double), which is exactly the
"not smooth" look. What is left (~7% of frames) are frames where the in-between camera is skipped on purpose (cuts,
big jumps, roll changes) and the game camera's own easing.

Frame delivery was already even and is unchanged (ms between presents, standard deviation, VSync Off):
60: 0.48 / 120: 0.32-0.46 / 144: 0.38-0.46 / 165: 0.51. VSync Fast: 120: 0.46, 144: 0.44.
Picture checked at 144 (two consecutive 4K window shots): intact, HUD in place.

## What still limits smoothness above 60 (not fixed, by design or by the display)

- **Only Mario and the camera are drawn in between ticks.** Enemies, platforms, other characters, particles and HUD
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
- `ODYSSEY_LOG_CAMERA=1` (odyssey_120.c): each stage draw's camera as drawn and as the game made it.
