#include "gtao.h"

#ifndef __APPLE__
// Ambient occlusion (Graphics: "Ambient occlusion", settings ambient_occlusion; off by default). The Switch version
// has none in its levels (its engine's SSAO passes stay empty), so this is an addition: XeGTAO-style horizon-based
// occlusion (Jimenez et al. 2016, "Practical Real-Time Strategies for Accurate Indirect Occlusion"; Intel's XeGTAO)
// from the scene's depth and the game's camera projection (hle/odyssey_debug.c, the same copy DLSS uses).
//
// Where: once per frame, just before the glow's first draw reads the finished HDR scene (exec.c dlss_probe_scene):
// every opaque surface, the sky, water and effects are in it; the HUD is not. Two steps at the scene's own size
// (the internal size with DLSS / FSR, which come after):
//   1. compute: the visibility per pixel (and its distance, for step 2) into an RG16F image;
//   2. a fullscreen draw into the picture: a depth-aware 5x5 average of step 1 (removes the per-pixel noise),
//      faded out with distance, multiplied into the colour (blend: dst x src).
// Tests: ODYSSEY_GTAO=0/1 overrides the setting; ODYSSEY_GTAO_FLIP_AT=<frame>: the other state from that frame on (an
// A/B of one scene); ODYSSEY_GTAO_DEBUG=1: the picture replaced by the occlusion (white: open, black: occluded);
// ODYSSEY_GTAO_RADIUS / _STRENGTH / _POWER / _FADE: tuning.
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform.h"
#include "../../settings.h"

static struct {
    float proj[16];
    bool have_proj;
    uint64_t done_frame;
    VkImage ao_image;
    VkImageView ao_view;
    VmaAllocation ao_mem;
    uint32_t w, h;
    uint64_t seq;  // the submission that last used the image
    VkDescriptorSetLayout cset, gset;
    VkPipelineLayout clayout, glayout;
    VkPipeline cpipe;
    struct { VkFormat fmt; bool debug; VkPipeline p; } gpipe[4];
    bool failed;
} ao;

static float envf(const char *name, float def) {
    const char *e = getenv(name);
    return e && *e ? (float)atof(e) : def;
}

bool vk_gtao_enabled(void) {
    static int env = -2;
    static long flip_at = -1;
    if (env == -2) {
        env = getenv("ODYSSEY_GTAO") ? atoi(getenv("ODYSSEY_GTAO")) : -1;
        flip_at = getenv("ODYSSEY_GTAO_FLIP_AT") ? atol(getenv("ODYSSEY_GTAO_FLIP_AT")) : 0;
    }
    bool on = env >= 0 ? env > 0 : g_settings.ambient_occlusion > 0;
    extern uint64_t renderer_present_count(void);
    if (flip_at > 0 && renderer_present_count() >= (uint64_t)flip_at) on = !on;
    return on && !ao.failed;
}

void vk_gtao_set_projection(const float projection[16]) {
    memcpy(ao.proj, projection, sizeof ao.proj);
    ao.have_proj = true;
}

static void fail(const char *what, int r) {
    LOG("[gtao] %s failed (%d): ambient occlusion off\n", what, r);
    ao.failed = true;
}

// Step 1. View space as XeGTAO's: x right, y up, z the distance in front of the camera. The game's projection is
// OpenGL's (row-major; depth stored as ndc z * 0.5 + 0.5: dlss_probe.c's motion pass reads it the same way).
static const char gtao_cs[] =
    "#version 450\n"
    "layout(local_size_x=8, local_size_y=8) in;\n"
    "layout(binding=0) uniform sampler2D depthTex;\n"
    "layout(binding=1, rg16f) uniform writeonly image2D aoOut;\n"
    "layout(push_constant) uniform P { vec4 proj; vec4 zp; vec4 misc; vec4 opt; } pc;\n"
    // proj: p0 p5 p2 p6; zp: p10 p11 radius falloff-range; misc: width height max-radius-px power; opt.x: depth is ndc z
    "const float PI = 3.14159265, HALF_PI = 1.57079633;\n"
    "float dist(float d) { return abs(pc.zp.y / ((pc.opt.x > 0.5 ? d : d * 2.0 - 1.0) + pc.zp.x)); }\n"
    "vec3 viewPos(vec2 uv, float z) {\n"
    "    vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);\n"
    "    return vec3((ndc.x + pc.proj.z) / pc.proj.x * z, (ndc.y + pc.proj.w) / pc.proj.y * z, z);\n"
    "}\n"
    "vec3 at(ivec2 q) {\n"
    "    ivec2 s = textureSize(depthTex, 0);\n"
    "    q = clamp(q, ivec2(0), s - 1);\n"
    "    return viewPos((vec2(q) + 0.5) / vec2(s), dist(texelFetch(depthTex, q, 0).r));\n"
    "}\n"
    "float fastAcos(float x) {\n"
    "    float r = -0.156583 * abs(x) + HALF_PI; r *= sqrt(1.0 - abs(x));\n"
    "    return x >= 0.0 ? r : PI - r;\n"
    "}\n"
    "void main() {\n"
    "    ivec2 p = ivec2(gl_GlobalInvocationID.xy), size = imageSize(aoOut);\n"
    "    if (any(greaterThanEqual(p, size))) return;\n"
    "    float d = texelFetch(depthTex, p, 0).r;\n"
    "    if (d >= 0.999999) { imageStore(aoOut, p, vec4(1, 0, 0, 0)); return; }\n"  // (the sky: g 0)
    "    vec2 px = 1.0 / vec2(size), uv = (vec2(p) + 0.5) * px;\n"
    "    vec3 P = viewPos(uv, dist(d));\n"
    // the normal from the nearer neighbours (no G-buffer: its encoding differs between the game's shaders)
    "    vec3 l = at(p - ivec2(1, 0)), r = at(p + ivec2(1, 0)), u = at(p - ivec2(0, 1)), b = at(p + ivec2(0, 1));\n"
    "    vec3 dx = abs(r.z - P.z) < abs(P.z - l.z) ? r - P : P - l;\n"
    "    vec3 dy = abs(b.z - P.z) < abs(P.z - u.z) ? b - P : P - u;\n"
    "    vec3 V = normalize(-P);\n"
    "    vec3 N = normalize(cross(dx, dy));\n"
    "    if (dot(N, V) < 0.0) N = -N;\n"
    "    float radius = pc.zp.z;\n"
    "    float screenR = radius * abs(pc.proj.y) * 0.5 * float(size.y) / P.z;\n"  // (pixels)
    "    if (screenR < 1.0) { imageStore(aoOut, p, vec4(1, P.z, 0, 0)); return; }\n"
    "    screenR = min(screenR, pc.misc.z);\n"
    "    float falloffRange = pc.zp.w * radius, falloffFrom = radius - falloffRange;\n"
    "    float falloffMul = -1.0 / falloffRange, falloffAdd = falloffFrom / falloffRange + 1.0;\n"
    // noise: interleaved gradient noise for the slice angle, a second for the step offsets (removed by step 2)
    "    float n0 = fract(52.9829189 * fract(dot(vec2(p), vec2(0.06711056, 0.00583715))));\n"
    "    float n1 = fract(n0 * 0.618034 + 0.5 * fract(dot(vec2(p), vec2(0.7548777, 0.5698403))));\n"
    "    const int SLICES = 3, STEPS = 4;\n"
    "    float vis = 0.0;\n"
    "    for (int s = 0; s < SLICES; s++) {\n"
    "        float phi = (float(s) + n0) / float(SLICES) * PI;\n"
    "        vec2 omega = vec2(cos(phi), -sin(phi));\n"  // (screen: y down)
    // the slice's direction in view space, from how a screen step moves a point there (any flip of the picture included)
    "        vec3 dirV = normalize(vec3(omega.x / (pc.proj.x * float(size.x)), -omega.y / (pc.proj.y * float(size.y)), 0.0));\n"
    "        vec3 ortho = dirV - dot(dirV, V) * V;\n"
    "        vec3 axis = normalize(cross(ortho, V));\n"
    "        vec3 projN = N - axis * dot(N, axis);\n"
    "        float projLen = length(projN);\n"
    "        float signN = sign(dot(ortho, projN));\n"
    "        float cosN = clamp(dot(projN, V) / max(projLen, 1e-4), 0.0, 1.0);\n"
    "        float n = signN * fastAcos(cosN);\n"
    "        float low0 = cos(n + HALF_PI), low1 = cos(n - HALF_PI);\n"
    "        float h0c = low0, h1c = low1;\n"
    "        for (int j = 0; j < STEPS; j++) {\n"
    "            float t = (float(j) + n1) / float(STEPS);\n"
    "            t = t * t;\n"
    "            vec2 off = round(omega * max(t * screenR, float(j) + 1.0)) * px;\n"
    "            vec2 uv0 = uv + off, uv1 = uv - off;\n"
    "            vec3 d0 = viewPos(uv0, dist(textureLod(depthTex, uv0, 0.0).r)) - P;\n"
    "            vec3 d1 = viewPos(uv1, dist(textureLod(depthTex, uv1, 0.0).r)) - P;\n"
    "            float l0 = length(d0), l1 = length(d1);\n"
    "            float w0 = clamp(l0 * falloffMul + falloffAdd, 0.0, 1.0), w1 = clamp(l1 * falloffMul + falloffAdd, 0.0, 1.0);\n"
    "            float c0 = mix(low0, dot(d0 / l0, V), w0), c1 = mix(low1, dot(d1 / l1, V), w1);\n"
    "            h0c = max(h0c, c0); h1c = max(h1c, c1);\n"
    "        }\n"
    "        projLen = mix(projLen, 1.0, 0.05);\n"
    "        float h0 = -fastAcos(h1c), h1 = fastAcos(h0c);\n"
    "        h0 = n + clamp(h0 - n, -HALF_PI, HALF_PI);\n"
    "        h1 = n + clamp(h1 - n, -HALF_PI, HALF_PI);\n"
    "        float a0 = (cosN + 2.0 * h0 * sin(n) - cos(2.0 * h0 - n)) / 4.0;\n"
    "        float a1 = (cosN + 2.0 * h1 * sin(n) - cos(2.0 * h1 - n)) / 4.0;\n"
    "        vis += projLen * (a0 + a1);\n"
    "    }\n"
    "    vis = clamp(pow(vis / float(SLICES), pc.misc.w), 0.03, 1.0);\n"
    "    imageStore(aoOut, p, vec4(vis, P.z, 0, 0));\n"
    "}\n";

static const char apply_vs[] =
    "#version 450\n"
    "void main() {\n"
    "    vec2 q = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);\n"
    "    gl_Position = vec4(q * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";

// Step 2. strength / fade: push constants (x strength, y fade start, z fade end, w debug).
static const char apply_fs[] =
    "#version 450\n"
    "layout(binding=0) uniform sampler2D aoTex;\n"
    "layout(push_constant) uniform P { vec4 a; } pc;\n"
    "layout(location=0) out vec4 o;\n"
    "void main() {\n"
    "    ivec2 p = ivec2(gl_FragCoord.xy), s = textureSize(aoTex, 0) - 1;\n"
    "    vec2 c = texelFetch(aoTex, p, 0).rg;\n"
    "    if (c.g <= 0.0) { o = vec4(1); return; }\n"
    "    float sum = 0.0, wsum = 0.0;\n"
    "    for (int y = -2; y <= 2; y++) for (int x = -2; x <= 2; x++) {\n"
    "        vec2 t = texelFetch(aoTex, clamp(p + ivec2(x, y), ivec2(0), s), 0).rg;\n"
    "        float w = t.g > 0.0 ? max(0.0, 1.0 - abs(t.g - c.g) / (c.g * 0.04 + 1.0)) : 0.0;\n"
    "        sum += t.r * w; wsum += w;\n"
    "    }\n"
    "    float vis = wsum > 0.0 ? sum / wsum : c.r;\n"
    "    float fade = 1.0 - smoothstep(pc.a.y, pc.a.z, c.g);\n"
    "    float k = mix(1.0, vis, clamp(pc.a.x * fade, 0.0, 1.0));\n"
    "    o = vec4(k, k, k, 1);\n"
    "}\n";

static bool make_pipelines(void) {
    if (ao.cpipe) return true;
    VkShaderModule m = vk_compile_glsl(gtao_cs, STAGE_COMPUTE, "gtao");
    if (!m) { fail("shader", 0); return false; }
    VkDescriptorSetLayoutBinding cb[] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, NULL}};
    VkDescriptorSetLayoutCreateInfo ci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 2,
                                          .pBindings = cb};
    VkResult r = vkCreateDescriptorSetLayout(g_vk.dev, &ci, NULL, &ao.cset);
    VkPushConstantRange push = {VK_SHADER_STAGE_COMPUTE_BIT, 0, 64};
    VkPipelineLayoutCreateInfo li = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1,
                                     .pSetLayouts = &ao.cset, .pushConstantRangeCount = 1, .pPushConstantRanges = &push};
    if (r == VK_SUCCESS) r = vkCreatePipelineLayout(g_vk.dev, &li, NULL, &ao.clayout);
    VkComputePipelineCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .layout = ao.clayout,
        .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                  .module = m, .pName = "main"}};
    if (r == VK_SUCCESS) r = vkCreateComputePipelines(g_vk.dev, VK_NULL_HANDLE, 1, &pi, NULL, &ao.cpipe);
    vkDestroyShaderModule(g_vk.dev, m, NULL);
    if (r != VK_SUCCESS) { ao.cpipe = VK_NULL_HANDLE; fail("compute pipeline", r); return false; }
    VkDescriptorSetLayoutBinding gb = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL};
    ci.bindingCount = 1, ci.pBindings = &gb;
    r = vkCreateDescriptorSetLayout(g_vk.dev, &ci, NULL, &ao.gset);
    VkPushConstantRange gpush = {VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16};
    li.pSetLayouts = &ao.gset, li.pPushConstantRanges = &gpush;
    if (r == VK_SUCCESS) r = vkCreatePipelineLayout(g_vk.dev, &li, NULL, &ao.glayout);
    if (r != VK_SUCCESS) { fail("draw layout", r); return false; }
    return true;
}

static VkPipeline apply_pipeline(VkFormat fmt, bool debug) {
    for (int i = 0; i < 4; i++) if (ao.gpipe[i].p && ao.gpipe[i].fmt == fmt && ao.gpipe[i].debug == debug) return ao.gpipe[i].p;
    VkShaderModule vs = vk_compile_glsl(apply_vs, STAGE_VERTEX, "gtao_vs"), fs = vk_compile_glsl(apply_fs, STAGE_FRAGMENT, "gtao_fs");
    VkPipeline p = VK_NULL_HANDLE;
    if (vs && fs) {
        VkPipelineShaderStageCreateInfo stages[2] = {
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vs, .pName = "main"},
            {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fs, .pName = "main"}};
        VkPipelineVertexInputStateCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo ia = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                     .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        VkPipelineViewportStateCreateInfo vps = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1,
                                                 .scissorCount = 1};
        VkPipelineRasterizationStateCreateInfo rs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                     .polygonMode = VK_POLYGON_MODE_FILL, .lineWidth = 1};
        VkPipelineMultisampleStateCreateInfo ms = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                   .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
        // the picture x the occlusion (debug: the occlusion alone); its alpha stays
        VkPipelineColorBlendAttachmentState cba = {.blendEnable = !debug, .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR, .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE, .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = 7};
        VkPipelineColorBlendStateCreateInfo cbs = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                   .attachmentCount = 1, .pAttachments = &cba};
        VkDynamicState dyn[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dy = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2,
                                               .pDynamicStates = dyn};
        VkPipelineRenderingCreateInfo ri = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1,
                                            .pColorAttachmentFormats = &fmt};
        VkPipelineDepthStencilStateCreateInfo ds = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        VkGraphicsPipelineCreateInfo pi = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, .pNext = &ri, .stageCount = 2,
            .pStages = stages, .pVertexInputState = &vi, .pInputAssemblyState = &ia, .pViewportState = &vps,
            .pRasterizationState = &rs, .pMultisampleState = &ms, .pDepthStencilState = &ds, .pColorBlendState = &cbs,
            .pDynamicState = &dy, .layout = ao.glayout};
        VkResult r = vkCreateGraphicsPipelines(g_vk.dev, g_vk.pcache, 1, &pi, NULL, &p);
        if (r != VK_SUCCESS) { p = VK_NULL_HANDLE; fail("draw pipeline", r); }
    } else fail("draw shaders", 0);
    if (vs) vkDestroyShaderModule(g_vk.dev, vs, NULL);
    if (fs) vkDestroyShaderModule(g_vk.dev, fs, NULL);
    for (int i = 0; p && i < 4; i++) if (!ao.gpipe[i].p) { ao.gpipe[i].fmt = fmt, ao.gpipe[i].debug = debug, ao.gpipe[i].p = p; break; }
    return p;
}

static bool make_image(VkCommandBuffer cb, uint32_t w, uint32_t h) {
    if (ao.ao_image && ao.w == w && ao.h == h) return true;
    if (ao.ao_image) {  // (a new size: wait until no submitted work uses the old one)
        if (ao.seq >= vk_pending_seq()) return false;
        if (ao.seq) vk_wait_seq(ao.seq);
        vkDestroyImageView(g_vk.dev, ao.ao_view, NULL);
        vmaDestroyImage(g_vk.vma, ao.ao_image, ao.ao_mem);
        ao.ao_image = VK_NULL_HANDLE, ao.ao_view = VK_NULL_HANDLE;
    }
    VkImageCreateInfo info = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R16G16_SFLOAT, .extent = {w, h, 1}, .mipLevels = 1, .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT, .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    VmaAllocationCreateInfo alloc = {.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    VkResult r = vmaCreateImage(g_vk.vma, &info, &alloc, &ao.ao_image, &ao.ao_mem, NULL);
    if (r != VK_SUCCESS) { ao.ao_image = VK_NULL_HANDLE; fail("image", r); return false; }
    VkImageViewCreateInfo vi = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = ao.ao_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = info.format, .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    r = vkCreateImageView(g_vk.dev, &vi, NULL, &ao.ao_view);
    if (r != VK_SUCCESS) { fail("image view", r); return false; }
    vk_image_init_layout(cb, ao.ao_image, VK_IMAGE_ASPECT_COLOR_BIT);
    ao.w = w, ao.h = h;
    LOG("[gtao] on: %ux%u\n", w, h);
    return true;
}

// ODYSSEY_GTAO_TIMING=1: the GPU time of both steps, averaged over 120 frames (read back without waiting)
#define TSLOTS 8
static struct { VkQueryPool pool; uint64_t seq[TSLOTS]; unsigned next; bool tried; double sum; int n; } tm;
static int time_begin(VkCommandBuffer cb) {
    if (!tm.tried) {
        tm.tried = true;
        VkQueryPoolCreateInfo info = {.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2 * TSLOTS};
        if (getenv("ODYSSEY_GTAO_TIMING") && vkCreateQueryPool(g_vk.dev, &info, NULL, &tm.pool) != VK_SUCCESS)
            tm.pool = VK_NULL_HANDLE;
    }
    if (!tm.pool) return -1;
    unsigned slot = tm.next++ % TSLOTS;
    if (tm.seq[slot] > vk_completed_seq()) return -1;
    if (tm.seq[slot]) {
        uint64_t t[2];
        if (vkGetQueryPoolResults(g_vk.dev, tm.pool, 2 * slot, 2, sizeof t, t, sizeof(uint64_t),
                                  VK_QUERY_RESULT_64_BIT) == VK_SUCCESS) {
            tm.sum += (t[1] - t[0]) * g_vk.props.limits.timestampPeriod / 1e6;
            if (++tm.n == 120) { LOG("[gtao] GPU %.3f ms (mean of 120 frames)\n", tm.sum / tm.n); tm.sum = 0, tm.n = 0; }
        }
    }
    vkCmdResetQueryPool(cb, tm.pool, 2 * slot, 2);
    vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, tm.pool, 2 * slot);
    tm.seq[slot] = vk_pending_seq();
    return (int)slot;
}

void vk_gtao_apply(NvnTexture *color_t, NvnTexture *depth_t, uint64_t key) {
    if (!vk_gtao_enabled() || !ao.have_proj || ao.done_frame == key + 1) return;
    TexBackend *color = vk_tex(color_t, true), *depth = vk_tex(depth_t, true);
    if (!color || !depth || !color->image || !depth->image || color->width != depth->width || color->height != depth->height)
        return;
    const float *pj = ao.proj;
    if (!(fabsf(pj[0]) > 1e-4f && fabsf(pj[5]) > 1e-4f && fabsf(pj[11]) > 1e-6f && isfinite(pj[10]))) return;
    ao.done_frame = key + 1;
    vk_end_pass();
    VkCommandBuffer cb = vk_cmd();
    if (!make_pipelines() || !make_image(cb, depth->width, depth->height)) return;
    static int debug = -1;
    static float radius, strength, power, fade, zmode;
    if (debug < 0) {
        debug = getenv("ODYSSEY_GTAO_DEBUG") && atoi(getenv("ODYSSEY_GTAO_DEBUG"));
        radius = envf("ODYSSEY_GTAO_RADIUS", 90.0f);      // (game units: Mario is about 160 tall)
        strength = envf("ODYSSEY_GTAO_STRENGTH", 0.85f);
        power = envf("ODYSSEY_GTAO_POWER", 1.6f);
        fade = envf("ODYSSEY_GTAO_FADE", 6000.0f);
        zmode = envf("ODYSSEY_GTAO_ZMODE", 0.0f);        // (fully faded at twice this distance)
    }
    VkPipeline gp = apply_pipeline(color->format, debug);
    if (!gp) return;
    VkImageView dview = vk_view(depth_t, NULL, false, false, TEXTYPE_2D);
    VkImageView cview = vk_view(color_t, NULL, false, true, TEXTYPE_2D);
    if (!dview || !cview) return;

    int slot = time_begin(cb);
    // step 1
    VkDescriptorSet set = vk_alloc_set(ao.cset);
    VkDescriptorImageInfo zi = {vk_default_sampler(false), dview, VK_IMAGE_LAYOUT_GENERAL};
    VkDescriptorImageInfo oi = {VK_NULL_HANDLE, ao.ao_view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet wr[] = {
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = 0, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &zi},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = 1, .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &oi}};
    vkUpdateDescriptorSets(g_vk.dev, 2, wr, 0, NULL);
    vk_full_barrier(cb);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ao.cpipe);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, ao.clayout, 0, 1, &set, 0, NULL);
    float maxr = 0.12f * (float)depth->height;  // (pixels: wide reaches cost cache misses and add little)
    float c[16] = {pj[0], pj[5], pj[2], pj[6], pj[10], pj[11], radius, 0.6f,
                   (float)depth->width, (float)depth->height, maxr, power, zmode};
    vkCmdPushConstants(cb, ao.clayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof c, c);
    vkCmdDispatch(cb, (depth->width + 7) / 8, (depth->height + 7) / 8, 1);
    vk_full_barrier(cb);

    // step 2
    VkDescriptorSet gset = vk_alloc_set(ao.gset);
    VkDescriptorImageInfo ai = {vk_default_sampler(false), ao.ao_view, VK_IMAGE_LAYOUT_GENERAL};
    VkWriteDescriptorSet gw = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = gset, .dstBinding = 0,
        .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &ai};
    vkUpdateDescriptorSets(g_vk.dev, 1, &gw, 0, NULL);
    VkRenderingAttachmentInfo att = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = cview,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL, .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD, .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
    VkRenderingInfo ri = {.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = {{0, 0}, {color->width, color->height}},
        .layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &att};
    vkCmdBeginRendering(cb, &ri);
    VkViewport vp = {0, 0, (float)color->width, (float)color->height, 0, 1};
    VkRect2D sc = {{0, 0}, {color->width, color->height}};
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor(cb, 0, 1, &sc);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, gp);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, ao.glayout, 0, 1, &gset, 0, NULL);
    float a[4] = {strength, fade, fade * 2.0f, (float)debug};
    vkCmdPushConstants(cb, ao.glayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof a, a);
    vkCmdDraw(cb, 3, 1, 0, 0);
    vkCmdEndRendering(cb);
    vk_full_barrier(cb);
    if (slot >= 0) vkCmdWriteTimestamp2(cb, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, tm.pool, 2 * slot + 1);
    ao.seq = vk_pending_seq();
    color->gfx_wseq = vk_pending_seq();
    static int logged;
    if (!logged++) {
        float n = fabsf(pj[11] / (pj[10] - 1)), f = fabsf(pj[11] / (pj[10] + 1));
        LOG("[gtao] first frame %llu: %ux%u, near %.2f far %.1f, fov %.1f deg, radius %.0f strength %.2f%s\n",
            (unsigned long long)key / 2, color->width, color->height, n, f, 2 * atanf(1 / fabsf(pj[5])) * 57.2958f,
            radius, strength, debug ? " (debug view)" : "");
    }
}
#endif
