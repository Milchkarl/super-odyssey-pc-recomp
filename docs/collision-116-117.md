# Stew and sea collision (#116, #117): the height maps the game reads back

2026-11-21, game 1.4.1, Windows, RTX 4090, the reporter's save (File1.bin of `MarioSaves.zip`) on the test data.
Local only; nothing posted.

## What the game does

The surface of a fluid is not collision geometry. The game draws height maps on the GPU and reads single texels of
them on the CPU (`al::getF32FromLinearTextureF16`):

- **Luncheon's stew** (`LavaWave`, the big pot at -9350 16950 7800, radius 3500): `LavaWave::calcPos` = the actor's
  height + one texel of a 256x256 half-float sine-wave map (waves of about +-92 units). A captured Lava Bubble is held
  on that surface (`Bubble::updateLavaWave`); when no surface is found under it, it falls, touches the pot's floor,
  and the game ends the capture there (`Bubble::tryShiftLand`: on ground that is not "DamageFire" -> `forceEndHack`).
- **Seaside's sea** (`al::OceanWave`, type "SodaWater", height 470): `OceanWave::tryCalcWaveHeight` = 470 + (depth
  rate)^2 x wave. The depth rate is a texel of a top-down depth map (256x256), the wave a texel of a 64x64 wave map.
  A point is in the water when its y is at or below that height (`calcIsInArea`). A value that is no number (NaN) in
  the depth map passes the game's 0..1 clamp, makes the height NaN, and the comparison fails at any depth: no water
  in that patch of the map.

Both Koopa races of Seaside use the same placement (scenario 5, start `SeaWorldRacemanStart`, sub scenario 4); only
the rival level differs (0 / 1). The race is a new scene, so its maps are new textures.

## Cause

The port gives the game those texels from a GPU copy of the map (objects.c `nvnMemoryPoolMap`, exec.c
`renderer_cpu_texel`, the "fog readback" of Cap Kingdom). One copy is recorded per frame into one of three slots,
and the newest finished copy is handed to the game.

The frame's new copy was recorded **before** the finished one was taken, and it went into the first slot the GPU had
finished - the very copy about to be taken. Whenever the GPU finished each copy exactly by the next frame's ask (a
fast GPU in a light scene), every frame overwrote the previous copy unread and the game never got a map: it read its
own memory instead - zeros, or texels left from the few reads that did succeed earlier (a map's first frames, before
its first full draw, can hold NaN patterns). Whether a scene ran in that state was decided by its timing when it
started: one stage entry worked, the next did not.

It does not depend on the Frame rate setting: the same at 60 and at 120.

## Fix

exec.c `renderer_cpu_texel`: the finished copy is taken first, then the frame's copy is recorded (commit "Height
map readback: take the finished copy before recording the next"). `ODYSSEY_FOG_READBACK_ORDER=old` gives the old
order, for comparison.

## Evidence (same scripted runs, old order / fixed)

Reads of each map answered from a GPU copy, per 300 frames (`ODYSSEY_LOG_LINREAD=1`, "[fog-read]"):

| Place | Map | Old order | Fixed |
| --- | --- | --- | --- |
| Seaside, Koopa race scene (regular race) | wave 64x64 | 0 of 5,686 | 5,720 of 5,722 |
| Seaside, the stage before it (first scene) | wave / depth | 12,477 of 13,003 | 12,794 of 13,005 |
| Luncheon, Mario over the big stew pot | stew 256x256 | 0 of 7,818 | 7,667 of 7,667 |
| Seaside race at Frame rate 120 | wave / depth | - | 4,647 of 4,647 / 4,831 of 4,831 |
| Cap Kingdom (fog, two maps) | | - | 3,552 of 3,552 each |

The stew surface the game computes over the pot, 225 points (`ODYSSEY_LOG_SURFACE`, "[surface] lava wave"):

- old order: `-131008.0 .. +3.3, 195 of 225 points exactly 0` and unchanged from tick to tick - a flat, frozen
  surface with dead points (-131008 is the game's decoding of a 0xffff half float: no surface there);
- fixed: `-92.2 .. +91.4, 0 of 225 points exactly 0`, changing every tick - the waves that are drawn.

Screenshots of both places were checked (the race start under the sea with its air meter; Mario over the stew).

## Not verified

- **Neither symptom was played to the end.** Getting a captured Lava Bubble into the pot by script was not done: the
  dismount itself (#116) is explained by the game's code and the dead points measured, not seen before / after.
- **#117's "acts as on land" was not reproduced.** In the runs here the race scene's maps read zeros under the old
  order (a flat sea: still water). NaN texels in the depth map are how the game's code gives "no water in an area";
  that they were what the reporter had is an inference. In one longer chain (Seaside -> Luncheon -> Seaside -> race)
  the old order happened to work in the race scene - it is that dependent on timing. If the report persists after
  this fix, ask for a clip and the spot.
- The stew's map holds NaN patterns while Mario is far from the pot (the wave actor inactive). Nothing reads it then
  in play (only the test log did); whether the Switch's memory holds the same there is unknown.
- 1.0.0, Linux / Steam Deck, AMD and Intel GPUs: not run. The change is in shared Vulkan code; the Metal renderer has
  no such readback.

## Test tools added (off unless set)

- `ODYSSEY_LOG_SURFACE=<ticks>`, `ODYSSEY_LOG_SURFACE_MAP=<tick>[:<half size>[:<y>]]`: the fluid surface the game
  computes at Mario's position (OceanWave and LavaWave), and a text map of it (hle/odyssey_warp.c).
- `ODYSSEY_WARP=race<0|1>:<stage>:<start>:<scenario>`: starts a Koopa race as its Koopa does (1.4.1).
  Seaside: `race0:SeaWorldHomeStage:SeaWorldRacemanStart:5`.
- `ODYSSEY_TEST_PLACE=<frame>:<x>:<y>:<z>[;...]`: puts Mario at a point (after the arrival scene: frame 3800 worked
  for Luncheon after `world:LavaWorldHomeStage@1700`; over the pot: `3800:-9350:19500:7800`).
- `ODYSSEY_LOG_LINREAD=1` now also logs, per map, reads answered from a copy and reads without one, the first asks
  about each map, and reads that gave a NaN pattern; `ODYSSEY_LOG_RT_DRAWS=<address>` logs the draws into a target.
- The reporter's save shows a dozen "Update Notifications" pages at the title; tests used its File1.bin with the
  test data's Common.bin.

## Draft replies (not posted - for the owner to approve)

**#116**

> Fixed for the next release (0.5.5). The stew's surface is a height map the game reads back from the GPU; on fast
> GPUs the port could stop delivering it for a whole visit to the kingdom, so the fireball saw a flat surface with
> holes in it and fell through. I measured the fix at the big pot with your save, but I could not ride a fireball
> there in an automated test - if it still happens on 0.5.5, tell me and send the log.

**#117**

> Likely fixed for the next release (0.5.5), together with #116: the sea also uses height maps read back from the
> GPU, and that readback could fail for a whole scene - the race is its own scene, which is why the normal level
> looked fine. I could not reproduce the dry areas themselves, so please try the regular race again on 0.5.5. If it
> still happens, a short clip of one spot would help.
