// Draw distance (Graphics tab "Draw distance": Switch / High / Very high). The game hides and stops the actors (enemies,
// coins, moons, NPCs, many map parts) beyond a far clipping distance, switches models to their simpler level of detail
// by distance, and fades some far objects out (al::FarDistanceDitherAnimator). The recompiler multiplies those
// distances by odyssey_draw_distance() right where the game reads them (tools/odyssey_patches.py DRAW_SITES, 1.0.0 and
// 1.4.1):
//   - al::ClippingJudge::update: the far clipping distance (al::ClippingFarAreaObserver's, 7000 by default, a stage's
//     or a ClippingFarArea's own) handed to every view's al::FrustumRadar - the far limit of every actor's clipping;
//   - al::ModelLodCtrl::update: the camera distance the level of detail is chosen by (model, shadow and material
//     levels), divided by the factor: the detailed models reach that much farther;
//   - al::FarDistanceDitherAnimator::update: the distance its object fades out at.
// The camera's own far plane is left as it is. Read every game frame: a change applies at once.
// Not 1:1 with the Switch above 1: actors farther away keep moving (they are frozen while clipped on the Switch), and
// the CPU and GPU do more.
#include <stdlib.h>

#include "../hle.h"
#include "../settings.h"

float odyssey_draw_distance(void) {
    static float env = -1.0f;
    if (env < 0.0f) {
        const char *e = getenv("ODYSSEY_DRAW_DISTANCE");
        float v = e && *e ? (float)atof(e) : 0.0f;
        env = v >= 0.25f && v <= 64.0f ? v : 0.0f;
    }
    if (env > 0.0f) return env;
    static const float k[] = {1.0f, 2.0f, 4.0f};
    int q = g_settings.draw_distance;
    return k[q < 0 ? 0 : q > 2 ? 2 : q];
}

// Shadow distance (Graphics: "Shadow distance": Switch / High / Very high; #123): how far from the camera the sun's
// shadows are drawn (al::ShadowDirector::getCurrentFar, tools/odyssey_patches.py SHADOW_SITES), times 1 / 2 / 4. The
// shadow map then covers more ground with the same texels: softer shadows near Mario unless Shadow quality is raised
// with it. ODYSSEY_SHADOW_DISTANCE=<factor> for tests.
float odyssey_shadow_distance(void) {
    static float env = -1.0f;
    if (env < 0.0f) {
        const char *e = getenv("ODYSSEY_SHADOW_DISTANCE");
        float v = e && *e ? (float)atof(e) : 0.0f;
        env = v >= 0.25f && v <= 16.0f ? v : 0.0f;
    }
    if (env > 0.0f) return env;
    static const float k[] = {1.0f, 2.0f, 4.0f};
    int q = g_settings.shadow_distance;
    return k[q < 0 ? 0 : q > 2 ? 2 : q];
}

// al::ShadowDirector::update, before al::DepthShadowDrawer::setNearFar (tools/odyssey_patches.py SHADOW_SITES): s3, the
// far end of the sun's shadow map, times the factor; s0-s2 are the cascades' split distances (left as they are).
// ODYSSEY_LOG_SHADOW_FAR=1 logs them when they change.
#include <string.h>
static float sreg(Cpu *c, int n) { float f; memcpy(&f, &c->v[n], 4); return f; }
void odyssey_shadow_far(Cpu *c) {
    float f = odyssey_shadow_distance(), v[4] = {sreg(c, 0), sreg(c, 1), sreg(c, 2), sreg(c, 3)};
    static int lg = -1;
    static float last[4];
    if (lg < 0) lg = getenv("ODYSSEY_LOG_SHADOW_FAR") != NULL;
    if (lg && memcmp(v, last, sizeof v)) {
        memcpy(last, v, sizeof v);
        LOG("[shadow] splits %.0f %.0f %.0f, far %.0f -> %.0f (x%g)\n", v[0], v[1], v[2], v[3], v[3] * f, f);
    }
    v[3] *= f;
    memcpy(&c->v[3], &v[3], 4);  // (s3: an argument; its upper lanes aren't read)
}
