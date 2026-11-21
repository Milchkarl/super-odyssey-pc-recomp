#pragma once

#include "vk.h"

#ifndef __APPLE__
// Ambient occlusion (settings ambient_occlusion; off by default): GTAO from the scene's depth, applied to the game's
// picture where its glow reads it (gtao.c). Not in the Switch version: an addition.
bool vk_gtao_enabled(void);
void vk_gtao_set_projection(const float projection[16]);  // this frame's camera (the game's device projection)
// color: the HDR scene picture (B10G11R11), depth: its D32 depth, the same size. Once per pass key (the frame, and
// frame interpolation's in-between apart: exec.c).
void vk_gtao_apply(NvnTexture *color, NvnTexture *depth, uint64_t key);
#endif
