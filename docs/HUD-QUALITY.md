# HUD and menu quality, HUD placement in narrow pictures, and the box under the life gauge

Work of 2026-11-21 for 0.5.5, on Windows with game 1.4.1 (RTX 4090). Nothing here was run on a Steam Deck, on
Linux, on macOS or with game 1.0.0.

## 1. What the HUD looked like (0.5.4)

The game's HUD pictures and font glyphs are made for 720 lines. At "Internal resolution 100%" the game draws its
layouts straight into its picture at the window's size, so every HUD texture is enlarged by one bilinear tap: 1.5x at
1080p, 2x at 1440p, 3x at 4K.

- **Sharpening off (the 0.5.4 default):** soft edges, and curves wobble (the bilinear "diamond" along the life
  gauge's ring). Icons with larger textures (the coin, the hat) are fine; the counters' digits and the small labels
  are the softest.
- **"Sharper HUD and menus" on:** an unsharp mask over the already enlarged picture. It steepens the edges but also
  the bilinear steps: the ring and the digits turn visibly stair-stepped at 4K. That is the aliased look.
- Straight edges that come from geometry (the counter's underline) are sharp either way.

Crops: `build/hud-quality/ring-{1080,1440,2160}.png` (Switch | old sharpening | High),
`coin-<size>-{switch,old-sharpen,high}.png`, `text-<size>.png` (Switch | High).

## 2. HUD and menu quality (Graphics tab; setting `hud_quality = switch | high`)

It replaces "Sharper HUD and menus" (`hud_sharpen`, which is read and ignored now). **Switch is the default**: the
game's own programs and sampling, nothing changed. The owner has not looked at High yet; making it the default is one
line (`.hud_quality` in runtime/settings.c).

High changes how a layout's draw reads an enlarged texture (runtime/nvn/maxwell/translate.c `hudSample`):

1. The 4x4 texels around the point are taken as a cubic B-spline, which gives smooth first and second derivatives.
   From them: the direction across the nearest edge, and how far the point is from the middle of that edge.
2. The texture is then read with a Catmull-Rom cubic (kept between the four nearest texels, so nothing rings) at a
   point moved away from the middle of the edge. The edge becomes up to 2.5 times narrower along a smooth line.

So round shapes stay round and edges get crisp without steps, outlines or halos. It does not add detail. The move
fades out in the middle of thin strokes and on weak gradients (shading, noise), where plain cubic filtering remains.
Colours are weighted by alpha, which removes the dark rim that transparent texels would otherwise leave on a narrow
edge. It applies only where a texture is enlarged (under 0.8 texels a pixel): at 1280x800 it does nothing, and scaled
render targets (the pause menu's blurred scene, Mario's preview) are read as before. Font glyph sheets (2D array
textures) go through the same code.

How it is wired:

- The layouts' draws are already marked (`CMD_UI_MARK`). With High they ask for a variant of their fragment shader
  (`ShaderKey.hud_filter`); the strength travels in the resolution table (`rscale[slot].z`). With Switch the variant
  is never requested, so nothing changes for anyone who leaves the option alone.
- It works the same with the HUD layer (DLSS / FSR / XeSS, lower internal resolutions) and without it.
- Tests: `ODYSSEY_HUD_QUALITY=0/1`, `ODYSSEY_HUD_FILTER=<strength>` (0.75), `ODYSSEY_HUD_FLIP_AT=<frame>`,
  `ODYSSEY_LOG_HUD_DRAWS=1` (each layout shader once, with its textures; every draw in a debug frame).

**Shader preparation:** translate.c, shader.h and shaders.c changed, so PREP_ID changes: players prepare shaders
again once on the update that ships this. The variants themselves are not part of the preparation: they are built
the first time the HUD is drawn at High (the HUD can be missing for a few frames then, once; later starts take them
from the caches).

Known limits:

- A glyph drawn very large from a small source (the coin counter's digits at 4K) gets crisp but slightly lumpy
  outlines: the source has too few texels for a perfect curve.
- Small round icons (the + / - buttons) can look faintly polygonal at 3x.

## 3. HUD placement in pictures narrower than 16:9

In a narrow picture (Aspect ratio 16:10 or 4:3, Fill on a 16:10 screen) the HUD keeps its 16:9 shape across the
picture's width, so it is shorter than the picture and was centred: the counters and the life gauge sat 135 lines
below the top of a 1080-line 4:3 picture (60 at 1920x1200, 40 on a Steam Deck's 1280x800), the button guides as far
above the bottom.

Now the HUD's own layouts (the same ones "HUD position: Edges" moves sideways in wide pictures: counters, life
gauge, guides, mini map, save message) move up or down to the picture's edge when they sit above or below the middle
(runtime/nvn/vulkan/exec.c `hud_edge_shift`). Menus, messages and anything across the middle stay. Bands across the
width (the tutorial's dark strip, the pause menu's footer) are still stretched to the edge as before; their text now
moves with them.

- It applies whatever "HUD position (wide)" says: that option is about wide pictures.
- 16:9 and wider pictures are not touched (same code path as before for k >= 1).
- `ODYSSEY_HUD_EDGES_Y=0` turns it off; `ODYSSEY_LOG_HUD_EDGES=1` logs each layout's extent and move.
- The "faint" small labels seen at 4:3 were white text over the bright moon, the same as at 16:9 in that spot: not a
  rendering fault. With an upscaler on they are drawn in the HUD layer at the window's resolution in these shapes too
  (the layer was only skipped for the old sharpening in handheld mode).

Found on the way, not changed: `hud_edges` has no value in the default settings (runtime/settings.c), so a new PC
install starts at "Centered (16:9)" although settings.h, the menu and the option note call Edges the default.

Shots: `build/hud-quality/placement-4x3-before-after.png`, `placement-16x10-before-after.png`,
`deck-1280x800-{docked,handheld}-before-after.png`, `deck-1280x800-pause-docked-handheld-after.png`,
`pause-4x3-after.png`.

## 4. The box under the life gauge after changing Aspect ratio

Reproduced: 2560x1080 window, DLSS Performance (also FSR 3 with frame generation), Aspect ratio changed in the
in-game menu from Fill to 16:10: a striped translucent box appears behind the "Menu / Map" guide under the life gauge
and stays. Without a temporal upscaler the same step usually just loses the labels' soft shadow.

Cause: the guide has a 70x40 backdrop that the game draws once and keeps. 70:40 is close enough to 16:9 that the port
treats it as one of the game's screen-sized targets, which are re-created whenever the internal resolution changes
(an aspect change, an upscaler or its mode, the resolution setting). The new image was empty or held whatever the
memory had before, and the game never draws that backdrop again.

Fix (runtime/nvn/vulkan/textures.c `tex_backend`): a re-created colour target gets its old picture, scaled into it.
Targets drawn every frame are simply overwritten as before. A CPU-filled texture of that kind is uploaded again.
`ODYSSEY_RESCALE_KEEP=0` gives the old behaviour.

Shots: `build/hud-quality/aspect-switch-box-ref_before2_after4.png` (a fresh 16:10 start for reference, two runs
without the fix, four with it: DLSS three times, FSR 3 + frame generation once).

## 5. Verified / not verified

Looked at, enlarged:

- 4K, High against Switch and the old sharpening: life gauge, coin counter, Menu / Map, tutorial text.
- 4:3 in a 1920x1080 window: the HUD at the top and bottom edges.
- The box: 2 of 3 runs without the fix show it, 4 of 4 with the fix are clean (DLSS x3, FSR 3 + frame generation).
- The option's row and note in the Graphics tab (English).

Shots taken and saved, but **not yet looked at** (the session ran out of time): 1080p and 1440p High, 16:10 at
1920x1200, 1280x800 Fill docked and handheld with the pause menu, the pause menu and the map at 4K with High, 21:9
Fill, DLSS Performance at 4K with High, the pause menu at 4:3. Look at them before relying on any of this.

Not done at all:

- GPU cost of High (expected small: up to 32 texel reads per enlarged HUD pixel; measure with `ODYSSEY_GPUPROF_AT`
  on the map screen at 4K).
- The blink when the option is changed while playing (`ODYSSEY_HUD_FLIP_AT`).
- Steam Deck: its 1280x800 (16:10, Fill, handheld mode) needs a check there, HUD placement above all.
- Game 1.0.0, Linux, macOS (the Metal renderer ignores the option), Korean / Portuguese text in the menu.
- README.md still describes "Sharper HUD and menus".
