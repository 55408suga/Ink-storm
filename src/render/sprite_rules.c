#include "sprite_rules.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const float k_two_pi = 6.28318530718f;
static const float k_deg = 0.01745329252f;

#define ANCHOR(id, uu, vv) [id] = {.u = (uu), .v = (vv), .used = true}

/* アンカーは採用したスプライト画像から実測したキャンバス正規化座標 */
static const SpriteDef k_defs[SPR_COUNT] = {
    [SPR_SHRINE] = {.file = "shrine.png",
                    .canvas = 256,
                    .group = SPR_GROUP_SHRINE,
                    .group_required = true,
                    .fallback = SPR_NONE,
                    .body_frac = 0.66f,
                    .fixed_size_px = 152.0f,
                    .ref_radius = 42.0f,
                    .foot = {0.50f, 0.80f},
                    .orient = SPR_ORIENT_NONE,
                    .halo_pad_px = 4.0f,
                    .anchor = {ANCHOR(SPR_ANCHOR_FLAME, 0.50f, 0.66f)},
                    .extent = {0.055f, 0.070f}},
    [SPR_ARCHER_IDLE] = {.file = "archer_idle.png",
                         .canvas = 256,
                         .group = SPR_GROUP_ARCHER,
                         .group_required = true,
                         .fallback = SPR_NONE,
                         .body_frac = 0.40f,
                         .ref_radius = 16.0f,
                         .foot = {0.48f, 0.85f},
                         .orient = SPR_ORIENT_MIRROR_X,
                         .halo_pad_px = 3.0f,
                         .breath_amp = 0.018f,
                         .breath_period_s = 2.8f,
                         .anchor = {ANCHOR(SPR_ANCHOR_BOW_GRIP, 0.73f, 0.41f)}},
    [SPR_ARCHER_DRAW] = {.file = "archer_draw.png",
                         .canvas = 256,
                         .group = SPR_GROUP_ARCHER,
                         .fallback = SPR_ARCHER_IDLE,
                         .body_frac = 0.40f,
                         .ref_radius = 16.0f,
                         .foot = {0.50f, 0.85f},
                         .orient = SPR_ORIENT_MIRROR_X,
                         .halo_pad_px = 3.0f,
                         .breath_amp = 0.018f,
                         .breath_period_s = 2.8f,
                         .anchor = {ANCHOR(SPR_ANCHOR_BOW_GRIP, 0.82f, 0.40f),
                                    ANCHOR(SPR_ANCHOR_NOCK, 0.42f, 0.38f)}},
    [SPR_LANTERN_FIRE] = {.file = "lantern_fire.png",
                          .canvas = 256,
                          .group = SPR_GROUP_LANTERN_FIRE,
                          .group_required = true,
                          .fallback = SPR_NONE,
                          .body_frac = 0.40f,
                          .ref_radius = 17.0f,
                          .foot = {0.50f, 0.50f},
                          .orient = SPR_ORIENT_NONE,
                          .halo_pad_px = 3.0f,
                          .breath_amp = 0.030f,
                          .breath_period_s = 0.748f,
                          .anchor = {ANCHOR(SPR_ANCHOR_GLOW, 0.50f, 0.48f),
                                     ANCHOR(SPR_ANCHOR_MOUTH, 0.48f, 0.64f)},
                          .extent = {0.18f, 0.22f}},
    [SPR_LANTERN_STORM] = {.file = "lantern_storm.png",
                           .canvas = 256,
                           .group = SPR_GROUP_LANTERN_STORM,
                           .group_required = true,
                           .fallback = SPR_NONE,
                           .body_frac = 0.40f,
                           .ref_radius = 17.0f,
                           .foot = {0.50f, 0.50f},
                           .orient = SPR_ORIENT_NONE,
                           .halo_pad_px = 3.0f,
                           .breath_amp = 0.030f,
                           .breath_period_s = 0.748f,
                           .anchor = {ANCHOR(SPR_ANCHOR_GLOW, 0.50f, 0.46f),
                                      ANCHOR(SPR_ANCHOR_MOUTH, 0.57f, 0.47f)},
                           .extent = {0.10f, 0.24f}},
    [SPR_UMBRELLA_HOP] = {.file = "umbrella_hop.png",
                          .canvas = 256,
                          .group = SPR_GROUP_UMBRELLA,
                          .group_required = true,
                          .fallback = SPR_NONE,
                          .body_frac = 0.40f,
                          .ref_radius = 18.0f,
                          .foot = {0.47f, 0.87f},
                          .orient = SPR_ORIENT_MIRROR_X,
                          .halo_pad_px = 3.0f,
                          .breath_amp = 0.018f,
                          .breath_period_s = 3.0f},
    [SPR_UMBRELLA_CROUCH] = {.file = "umbrella_crouch.png",
                             .canvas = 256,
                             .group = SPR_GROUP_UMBRELLA,
                             .fallback = SPR_UMBRELLA_HOP,
                             .body_frac = 0.40f,
                             .ref_radius = 18.0f,
                             .foot = {0.47f, 0.78f},
                             .orient = SPR_ORIENT_MIRROR_X,
                             .halo_pad_px = 3.0f,
                             .breath_amp = 0.018f,
                             .breath_period_s = 3.0f},
    [SPR_KAMAITACHI] = {.file = "kamaitachi.png",
                        .canvas = 256,
                        .group = SPR_GROUP_KAMAITACHI,
                        .group_required = true,
                        .fallback = SPR_NONE,
                        .body_frac = 0.36f,
                        .ref_radius = 16.0f,
                        .foot = {0.50f, 0.50f},
                        .orient = SPR_ORIENT_ROTATE_MIRROR,
                        .halo_pad_px = 3.0f,
                        .breath_amp = 0.018f,
                        .breath_period_s = 2.4f,
                        .anchor = {ANCHOR(SPR_ANCHOR_TRAIL_ROOT, 0.18f, 0.50f)}},
    [SPR_STONE_ONI] = {.file = "stone_oni.png",
                       .canvas = 256,
                       .group = SPR_GROUP_STONE_ONI,
                       .group_required = true,
                       .fallback = SPR_NONE,
                       .body_frac = 0.44f,
                       .ref_radius = 24.0f,
                       .foot = {0.50f, 0.86f},
                       .orient = SPR_ORIENT_MIRROR_X,
                       .halo_pad_px = 4.0f,
                       .breath_amp = 0.018f,
                       .breath_period_s = 3.2f,
                       .anchor = {ANCHOR(SPR_ANCHOR_FOOT_L, 0.35f, 0.86f),
                                  ANCHOR(SPR_ANCHOR_FOOT_R, 0.66f, 0.83f)}},
    [SPR_INK_LEECH] = {.file = "ink_leech.png",
                       .canvas = 256,
                       .group = SPR_GROUP_INK_LEECH,
                       .group_required = true,
                       .fallback = SPR_NONE,
                       .body_frac = 0.40f,
                       .ref_radius = 19.0f,
                       .foot = {0.50f, 0.72f},
                       .orient = SPR_ORIENT_NONE,
                       .halo_pad_px = 4.0f,
                       .breath_amp = 0.040f,
                       .breath_period_s = 0.9f,
                       .jelly = true,
                       .anchor = {ANCHOR(SPR_ANCHOR_CORE, 0.50f, 0.52f)},
                       .extent = {0.26f, 0.26f}},
    [SPR_OFUDA_FOX] = {.file = "ofuda_fox.png",
                       .canvas = 256,
                       .group = SPR_GROUP_OFUDA_FOX,
                       .group_required = true,
                       .fallback = SPR_NONE,
                       .body_frac = 0.36f,
                       .ref_radius = 17.0f,
                       .foot = {0.48f, 0.87f},
                       .orient = SPR_ORIENT_MIRROR_X,
                       .halo_pad_px = 3.0f,
                       .breath_amp = 0.018f,
                       .breath_period_s = 2.6f,
                       .anchor = {ANCHOR(SPR_ANCHOR_TAIL_ROOT, 0.35f, 0.69f),
                                  ANCHOR(SPR_ANCHOR_TALISMAN, 0.66f, 0.50f)}},
    [SPR_OFUDA_FOX_CAST] = {.file = "ofuda_fox_cast.png",
                            .canvas = 256,
                            .group = SPR_GROUP_OFUDA_FOX,
                            .optional = true,
                            .fallback = SPR_OFUDA_FOX,
                            .body_frac = 0.36f,
                            .ref_radius = 17.0f,
                            .foot = {0.48f, 0.87f},
                            .orient = SPR_ORIENT_MIRROR_X,
                            .halo_pad_px = 3.0f,
                            .breath_amp = 0.018f,
                            .breath_period_s = 2.6f,
                            .anchor = {ANCHOR(SPR_ANCHOR_TAIL_ROOT, 0.35f, 0.69f),
                                       ANCHOR(SPR_ANCHOR_TALISMAN, 0.70f, 0.17f)}},
    [SPR_TENGU_IDLE] = {.file = "tengu_idle.png",
                        .canvas = 512,
                        .group = SPR_GROUP_TENGU,
                        .group_required = true,
                        .fallback = SPR_NONE,
                        .body_frac = 0.30f,
                        .ref_radius = 44.0f,
                        .foot = {0.48f, 0.87f},
                        .orient = SPR_ORIENT_FACE_SHRINE,
                        .halo_pad_px = 5.0f,
                        .breath_amp = 0.012f,
                        .breath_period_s = 3.6f,
                        .anchor = {ANCHOR(SPR_ANCHOR_FAN_TIP, 0.70f, 0.63f),
                                   ANCHOR(SPR_ANCHOR_FACE, 0.52f, 0.34f)}},
    [SPR_TENGU_WINDUP] = {.file = "tengu_windup.png",
                          .canvas = 512,
                          .group = SPR_GROUP_TENGU,
                          .fallback = SPR_TENGU_IDLE,
                          .body_frac = 0.30f,
                          .ref_radius = 44.0f,
                          .foot = {0.46f, 0.87f},
                          .orient = SPR_ORIENT_FACE_SHRINE,
                          .halo_pad_px = 5.0f,
                          .breath_amp = 0.012f,
                          .breath_period_s = 3.6f,
                          .anchor = {ANCHOR(SPR_ANCHOR_FAN_TIP, 0.71f, 0.19f),
                                     ANCHOR(SPR_ANCHOR_FACE, 0.56f, 0.44f)}},
    [SPR_NUE_PROWL] = {.file = "nue_prowl.png",
                       .canvas = 512,
                       .group = SPR_GROUP_NUE,
                       .group_required = true,
                       .fallback = SPR_NONE,
                       .body_frac = 0.30f,
                       .ref_radius = 40.0f,
                       .foot = {0.52f, 0.75f},
                       .orient = SPR_ORIENT_MIRROR_X,
                       .halo_pad_px = 5.0f,
                       .breath_amp = 0.012f,
                       .breath_period_s = 3.6f,
                       .anchor = {ANCHOR(SPR_ANCHOR_EYE_L, 0.73f, 0.46f),
                                  ANCHOR(SPR_ANCHOR_EYE_R, 0.79f, 0.46f),
                                  ANCHOR(SPR_ANCHOR_TAIL_ROOT, 0.20f, 0.44f)}},
    [SPR_NUE_CROUCH] = {.file = "nue_crouch.png",
                        .canvas = 512,
                        .group = SPR_GROUP_NUE,
                        .fallback = SPR_NUE_PROWL,
                        .body_frac = 0.30f,
                        .ref_radius = 40.0f,
                        .foot = {0.52f, 0.74f},
                        .orient = SPR_ORIENT_MIRROR_X,
                        .halo_pad_px = 5.0f,
                        .breath_amp = 0.012f,
                        .breath_period_s = 3.6f,
                        .anchor = {ANCHOR(SPR_ANCHOR_EYE_L, 0.74f, 0.57f),
                                   ANCHOR(SPR_ANCHOR_EYE_R, 0.80f, 0.57f),
                                   ANCHOR(SPR_ANCHOR_TAIL_ROOT, 0.18f, 0.45f)}},
    [SPR_RYU_HEAD_DORMANT] = {.file = "ryu_head_dormant.png",
                              .canvas = 512,
                              .group = SPR_GROUP_RYU,
                              .group_required = true,
                              .fallback = SPR_NONE,
                              .body_frac = 0.36f,
                              .ref_radius = 46.0f,
                              .foot = {0.50f, 0.50f},
                              .orient = SPR_ORIENT_ROTATE_MIRROR,
                              .halo_pad_px = 5.0f,
                              .breath_amp = 0.012f,
                              .breath_period_s = 3.6f,
                              .anchor = {ANCHOR(SPR_ANCHOR_NECK, 0.18f, 0.62f),
                                         ANCHOR(SPR_ANCHOR_EYE_L, 0.61f, 0.53f),
                                         ANCHOR(SPR_ANCHOR_WHISKER_A, 0.83f, 0.62f),
                                         ANCHOR(SPR_ANCHOR_WHISKER_B, 0.78f, 0.66f)},
                              .horn_a = {{0.50f, 0.43f}, {0.36f, 0.30f}, {0.22f, 0.20f}},
                              .horn_b = {{0.57f, 0.40f}, {0.50f, 0.31f}, {0.43f, 0.24f}}},
    [SPR_RYU_HEAD_AWAKENED] = {.file = "ryu_head_awakened.png",
                               .canvas = 512,
                               .group = SPR_GROUP_RYU,
                               .fallback = SPR_RYU_HEAD_DORMANT,
                               .body_frac = 0.36f,
                               .ref_radius = 46.0f,
                               .foot = {0.50f, 0.50f},
                               .orient = SPR_ORIENT_ROTATE_MIRROR,
                               .halo_pad_px = 5.0f,
                               .breath_amp = 0.012f,
                               .breath_period_s = 3.6f,
                               .anchor = {ANCHOR(SPR_ANCHOR_NECK, 0.18f, 0.62f),
                                          ANCHOR(SPR_ANCHOR_EYE_L, 0.61f, 0.53f),
                                          ANCHOR(SPR_ANCHOR_WHISKER_A, 0.83f, 0.62f),
                                          ANCHOR(SPR_ANCHOR_WHISKER_B, 0.78f, 0.66f)},
                               .horn_a = {{0.50f, 0.43f}, {0.36f, 0.30f}, {0.22f, 0.20f}},
                               .horn_b = {{0.57f, 0.40f}, {0.50f, 0.31f}, {0.43f, 0.24f}}},
    [SPR_RYU_BODY] = {.file = "ryu_body.png",
                      .canvas = 512,
                      .group = SPR_GROUP_RYU,
                      .group_required = true,
                      .fallback = SPR_NONE,
                      .body_frac = 0.36f,
                      .ref_radius = 46.0f,
                      .foot = {0.50f, 0.54f},
                      .orient = SPR_ORIENT_NONE,
                      .halo_pad_px = 5.0f,
                      .tile_x = true},
    [SPR_RYU_LIMB] = {.file = "ryu_limb.png",
                      .canvas = 256,
                      .group = SPR_GROUP_RYU,
                      .group_required = true,
                      .fallback = SPR_NONE,
                      .body_frac = 0.50f,
                      .ref_radius = 30.0f,
                      .foot = {0.35f, 0.28f},
                      .orient = SPR_ORIENT_ROTATE_MIRROR,
                      .halo_pad_px = 4.0f},
    [SPR_RYU_TAIL] = {.file = "ryu_tail.png",
                      .canvas = 256,
                      .group = SPR_GROUP_RYU,
                      .group_required = true,
                      .fallback = SPR_NONE,
                      .body_frac = 0.50f,
                      .ref_radius = 28.0f,
                      .foot = {0.16f, 0.54f},
                      .orient = SPR_ORIENT_ROTATE_MIRROR,
                      .halo_pad_px = 4.0f},
    [SPR_HOJU_FIRE] = {.file = "hoju_fire.png",
                       .canvas = 256,
                       .group = SPR_GROUP_HOJU,
                       .group_required = true,
                       .fallback = SPR_NONE,
                       .body_frac = 0.56f,
                       .ref_radius = 30.0f,
                       .foot = {0.50f, 0.50f},
                       .orient = SPR_ORIENT_NONE,
                       .halo_pad_px = 4.0f,
                       .anchor = {ANCHOR(SPR_ANCHOR_CORE, 0.50f, 0.63f)}},
    [SPR_HOJU_STORM] = {.file = "hoju_storm.png",
                        .canvas = 256,
                        .group = SPR_GROUP_HOJU,
                        .group_required = true,
                        .fallback = SPR_NONE,
                        .body_frac = 0.56f,
                        .ref_radius = 30.0f,
                        .foot = {0.50f, 0.50f},
                        .orient = SPR_ORIENT_NONE,
                        .halo_pad_px = 4.0f,
                        .anchor = {ANCHOR(SPR_ANCHOR_CORE, 0.50f, 0.63f)}},
    [SPR_HOJU_WIND] = {.file = "hoju_wind.png",
                       .canvas = 256,
                       .group = SPR_GROUP_HOJU,
                       .group_required = true,
                       .fallback = SPR_NONE,
                       .body_frac = 0.56f,
                       .ref_radius = 30.0f,
                       .foot = {0.50f, 0.50f},
                       .orient = SPR_ORIENT_NONE,
                       .halo_pad_px = 4.0f,
                       .anchor = {ANCHOR(SPR_ANCHOR_CORE, 0.50f, 0.63f)},
                       .extent = {0.16f, 0.16f}},
    [SPR_TERRAIN_ROCK] = {.file = "terrain_rock.png",
                          .canvas = 256,
                          .group = SPR_GROUP_TERRAIN,
                          .group_required = true,
                          .fallback = SPR_NONE,
                          .body_frac = 0.64f,
                          .ref_radius = 24.0f,
                          .foot = {0.50f, 0.50f},
                          .orient = SPR_ORIENT_NONE,
                          .halo_pad_px = 2.5f},
    [SPR_TERRAIN_TORII] = {.file = "terrain_torii.png",
                           .canvas = 256,
                           .group = SPR_GROUP_TERRAIN,
                           .group_required = true,
                           .fallback = SPR_NONE,
                           .body_frac = 0.48f,
                           .fixed_size_px = 168.0f,
                           .ref_radius = 40.0f,
                           .foot = {0.50f, 0.50f},
                           .orient = SPR_ORIENT_NONE,
                           .halo_pad_px = 4.0f},
};

static const char *const k_bg_files[SPR_BG_COUNT] = {
    [SPR_BG_LOBBY] = "bg_lobby.png",
    [SPR_BG_ACT1] = "bg_act1.png",
    [SPR_BG_ACT2] = "bg_act2.png",
    [SPR_BG_ACT3] = "bg_act3.png",
};

#undef ANCHOR

static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

static float smooth_step(float e0, float e1, float x)
{
    const float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

static float vec_dist(GameVec2 a, GameVec2 b) { return hypotf(b.x - a.x, b.y - a.y); }

static bool sprite_id_valid(SpriteId id) { return id >= 0 && id < SPR_COUNT; }

const SpriteDef *sprite_def(SpriteId id) { return sprite_id_valid(id) ? &k_defs[id] : NULL; }

bool sprite_anchor(const SpriteDef *d, SpriteAnchorId a, SpriteUv *out)
{
    if (d == NULL || a < 0 || a >= SPR_ANCHOR_COUNT || !d->anchor[a].used) {
        return false;
    }
    if (out != NULL) {
        *out = (SpriteUv){d->anchor[a].u, d->anchor[a].v};
    }
    return true;
}

int32_t sprite_core_count(void)
{
    int32_t n = 0;
    for (int32_t i = 0; i < SPR_COUNT; ++i) {
        n += k_defs[i].optional ? 0 : 1;
    }
    return n;
}

float sprite_display_size(const SpriteDef *d, float radius)
{
    if (d == NULL) {
        return 0.0f;
    }
    if (d->fixed_size_px > 0.0f) {
        return d->fixed_size_px;
    }
    const float r = radius > 0.0f ? radius : d->ref_radius;
    return d->body_frac > 0.0f ? 2.0f * r / d->body_frac : 0.0f;
}

void sprite_halo_params(const SpriteDef *d, int32_t *pad_tex, int32_t *feather_tex)
{
    int32_t pad = 0;
    int32_t feather = 0;
    const float size = sprite_display_size(d, 0.0f);
    if (d != NULL && size > 0.0f) {
        const float texel_ratio = (float)d->canvas / size;
        pad = (int32_t)lroundf(d->halo_pad_px * texel_ratio * 0.5f);
        feather = (int32_t)lroundf(1.5f * texel_ratio * 0.5f);
        feather = feather < 1 ? 1 : feather;
    }
    if (pad_tex != NULL) {
        *pad_tex = pad;
    }
    if (feather_tex != NULL) {
        *feather_tex = feather;
    }
}

int32_t sprite_groups_resolve(const bool present[SPR_COUNT], bool group_ok[SPR_GROUP_COUNT])
{
    if (group_ok == NULL) {
        return 0;
    }
    bool has_required[SPR_GROUP_COUNT] = {false};
    for (int32_t g = 0; g < SPR_GROUP_COUNT; ++g) {
        group_ok[g] = present != NULL;
    }
    for (int32_t i = 0; i < SPR_COUNT; ++i) {
        const SpriteDef *d = &k_defs[i];
        if (d->group_required) {
            has_required[d->group] = true;
            if (present == NULL || !present[i]) {
                group_ok[d->group] = false;
            }
        }
    }
    int32_t ok = 0;
    for (int32_t g = 0; g < SPR_GROUP_COUNT; ++g) {
        group_ok[g] = group_ok[g] && has_required[g];
        ok += group_ok[g] ? 1 : 0;
    }
    return ok;
}

SpriteId sprite_resolve(SpriteId id, const bool present[SPR_COUNT],
                        const bool group_ok[SPR_GROUP_COUNT])
{
    if (!sprite_id_valid(id) || present == NULL || group_ok == NULL) {
        return SPR_NONE;
    }
    const SpriteDef *d = &k_defs[id];
    if (!group_ok[d->group]) {
        return SPR_NONE;
    }
    if (present[id]) {
        return id;
    }
    if (sprite_id_valid(d->fallback) && present[d->fallback]) {
        return d->fallback;
    }
    return SPR_NONE;
}

float sprite_umbrella_hop(int32_t phase_milli)
{
    return fabsf(sinf((float)phase_milli * 0.001f * 3.1f));
}

SpriteId sprite_select(const SpriteStateIn *in)
{
    if (in == NULL) {
        return SPR_NONE;
    }
    switch (in->def_id) {
    case ENEMY_ARCHER:
        return in->windup > 0.05f ? SPR_ARCHER_DRAW : SPR_ARCHER_IDLE;
    case ENEMY_LANTERN_FIRE:
        return SPR_LANTERN_FIRE;
    case ENEMY_LANTERN_STORM:
        return SPR_LANTERN_STORM;
    case ENEMY_UMBRELLA:
        return sprite_umbrella_hop(in->phase_milli) >= 0.22f ? SPR_UMBRELLA_HOP
                                                             : SPR_UMBRELLA_CROUCH;
    case ENEMY_KAMAITACHI:
        return SPR_KAMAITACHI;
    case ENEMY_STONE_ONI:
        return SPR_STONE_ONI;
    case ENEMY_INK_LEECH:
        return SPR_INK_LEECH;
    case ENEMY_OFUDA_FOX:
        return in->windup > 0.05f ? SPR_OFUDA_FOX_CAST : SPR_OFUDA_FOX;
    case ENEMY_BOSS_TENGU:
        return in->telegraph > 0.01f ? SPR_TENGU_WINDUP : SPR_TENGU_IDLE;
    case ENEMY_BOSS_NUE:
        return in->ai_state == 1 ? SPR_NUE_CROUCH : SPR_NUE_PROWL;
    case ENEMY_BOSS_RYU:
        return in->phase_idx >= 1 ? SPR_RYU_HEAD_AWAKENED : SPR_RYU_HEAD_DORMANT;
    case ENEMY_BOSS_RYU_ORB:
        if (in->part_id == 0) {
            return SPR_HOJU_FIRE;
        }
        return in->part_id == 1 ? SPR_HOJU_STORM : SPR_HOJU_WIND;
    case ENEMY_DEF_COUNT:
    default:
        return SPR_NONE;
    }
}

void sprite_xform_point(const SpriteDef *d, const SpriteXform *xf, SpriteUv p, float *wx, float *wy)
{
    if (xf == NULL) {
        return;
    }
    float x = xf->x;
    float y = xf->y;
    if (d != NULL) {
        const float s = xf->size;
        /* 反転はキャンバス中心 u = 0.5 で行い、当たり判定の中心を動かさない */
        const float mx = xf->sx < 0.0f ? -1.0f : 1.0f;
        const float my = xf->flip_v ? -1.0f : 1.0f;
        const float px = xf->x + mx * (d->foot.u - 0.5f) * s;
        const float py = xf->y + my * (d->foot.v - 0.5f) * s;
        const float lx = (p.u - d->foot.u) * s * xf->sx;
        const float ly = (p.v - d->foot.v) * s * xf->sy * my;
        const float c = cosf(xf->rot);
        const float sn = sinf(xf->rot);
        x = px + lx * c - ly * sn + xf->dx;
        y = py + lx * sn + ly * c + xf->dy;
    }
    if (wx != NULL) {
        *wx = x;
    }
    if (wy != NULL) {
        *wy = y;
    }
}

void sprite_quad(const SpriteDef *d, const SpriteXform *xf, float u0, float v0, float u1, float v1,
                 SpriteQuad *out)
{
    if (out == NULL) {
        return;
    }
    if (xf == NULL) {
        *out = (SpriteQuad){{0.0f}, {0.0f}, {0.0f}, {0.0f}};
        return;
    }
    static const float cu[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    static const float cv[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    for (int k = 0; k < 4; ++k) {
        sprite_xform_point(d, xf, (SpriteUv){cu[k], cv[k]}, &out->x[k], &out->y[k]);
        out->u[k] = cu[k] > 0.5f ? u1 : u0;
        out->v[k] = cv[k] > 0.5f ? v1 : v0;
    }
    /* raylib の 2D は裏面を間引くため、鏡像で巻き順が反転したら頂点 1 と 3 を入れ替える */
    const float cross = (out->x[1] - out->x[0]) * (out->y[2] - out->y[0]) -
                        (out->y[1] - out->y[0]) * (out->x[2] - out->x[0]);
    if (cross > 0.0f) {
        const float tx = out->x[1];
        const float ty = out->y[1];
        const float tu = out->u[1];
        const float tv = out->v[1];
        out->x[1] = out->x[3];
        out->y[1] = out->y[3];
        out->u[1] = out->u[3];
        out->v[1] = out->v[3];
        out->x[3] = tx;
        out->y[3] = ty;
        out->u[3] = tu;
        out->v[3] = tv;
    }
}

void sprite_breath(float amp, float period_s, float t_s, float phase, bool jelly, float *sx,
                   float *sy)
{
    float wave = 0.0f;
    if (amp > 0.0f && period_s > 0.0f) {
        wave = amp * sinf(k_two_pi * t_s / period_s + phase);
    }
    if (sx != NULL) {
        *sx = 1.0f - (jelly ? 1.0f : 0.4f) * wave;
    }
    if (sy != NULL) {
        *sy = 1.0f + wave;
    }
}

SpriteBoil sprite_boil(uint32_t seed, float size_px)
{
    const float a = (float)(seed & 0xffu) / 127.5f - 1.0f;
    const float b = (float)((seed >> 8) & 0xffu) / 127.5f - 1.0f;
    const float c = (float)((seed >> 16) & 0xffu) / 127.5f - 1.0f;
    const float e = (float)((seed >> 24) & 0xffu) / 127.5f - 1.0f;
    /* 0.28·√2 の平行移動に回転と拡縮の 0.2px ずつを足して 0.8px に収める */
    const float reach = 0.8f * (size_px > 1.0f ? size_px : 1.0f);
    const float rot_amp = fminf(0.35f * k_deg, 0.2f / reach);
    const float scale_amp = fminf(0.006f, 0.2f / reach);
    return (SpriteBoil){
        .dx = 0.28f * a, .dy = 0.28f * b, .rot = rot_amp * c, .scale = 1.0f + scale_amp * e};
}

int8_t sprite_face_update(int8_t prev, float cos_facing)
{
    if (prev == 0) {
        return cos_facing < 0.0f ? (int8_t)-1 : (int8_t)1;
    }
    if (prev > 0) {
        return cos_facing < -0.25f ? (int8_t)-1 : (int8_t)1;
    }
    return cos_facing > 0.25f ? (int8_t)1 : (int8_t)-1;
}

float sprite_turn_sx(int8_t face, uint32_t elapsed_ticks)
{
    const float sign = face < 0 ? -1.0f : 1.0f;
    if (face == 0 || elapsed_ticks >= (uint32_t)SPRITE_TURN_TICKS) {
        return sign;
    }
    const float e = smooth_step(0.0f, 1.0f, (float)elapsed_ticks / (float)SPRITE_TURN_TICKS);
    if (e < 0.5f) {
        return -sign * (1.0f - 0.85f * (e / 0.5f));
    }
    return sign * (0.15f + 0.85f * ((e - 0.5f) / 0.5f));
}

/* 窓 [x-k, x+k] の最大値を単調な両端キューで O(w) に求める（wrap_x が true なら横方向は周期境界）
 */
static void row_max(const uint8_t *src, uint8_t *dst, int32_t w, int32_t h, int32_t k, bool wrap_x)
{
    if (wrap_x) {
        if (2 * k + 1 >= w) {
            for (int32_t y = 0; y < h; ++y) {
                const uint8_t *row = src + (size_t)y * (size_t)w;
                uint8_t *drow = dst + (size_t)y * (size_t)w;
                uint8_t best = 0u;
                for (int32_t x = 0; x < w; ++x) {
                    if (row[x] > best) {
                        best = row[x];
                    }
                }
                memset(drow, best, (size_t)w);
            }
            return;
        }
        int16_t dq_pos[SPRITE_CANVAS_MAX];
        uint8_t dq_val[SPRITE_CANVAS_MAX];
        const int32_t total = w + 2 * k;
        for (int32_t y = 0; y < h; ++y) {
            const uint8_t *row = src + (size_t)y * (size_t)w;
            uint8_t *drow = dst + (size_t)y * (size_t)w;
            int32_t head = 0;
            int32_t tail = 0;
            for (int32_t j = 0; j < total; ++j) {
                int32_t col = j - k;
                if (col < 0) {
                    col += w;
                } else if (col >= w) {
                    col -= w;
                }
                const uint8_t val = row[col];
                while (tail > head && dq_val[tail - 1] <= val) {
                    --tail;
                }
                dq_pos[tail] = (int16_t)j;
                dq_val[tail] = val;
                ++tail;
                const int32_t x = j - 2 * k;
                if (x < 0) {
                    continue;
                }
                while (dq_pos[head] < x) {
                    ++head;
                }
                drow[x] = dq_val[head];
            }
        }
        return;
    }

    int16_t dq[SPRITE_CANVAS_MAX / 2];
    for (int32_t y = 0; y < h; ++y) {
        const uint8_t *row = src + (size_t)y * (size_t)w;
        uint8_t *drow = dst + (size_t)y * (size_t)w;
        int32_t head = 0;
        int32_t tail = 0;
        for (int32_t j = 0; j < w + k; ++j) {
            if (j < w) {
                while (tail > head && row[dq[tail - 1]] <= row[j]) {
                    --tail;
                }
                dq[tail++] = (int16_t)j;
            }
            const int32_t x = j - k;
            if (x < 0) {
                continue;
            }
            while (dq[head] < x - k) {
                ++head;
            }
            drow[x] = row[dq[head]];
        }
    }
}

static int32_t disc_span(int32_t pad, int32_t dy)
{
    const float rr = ((float)pad + 0.5f) * ((float)pad + 0.5f);
    return (int32_t)floorf(sqrtf(rr - (float)(dy * dy)));
}

/* 範囲外は透明として数え、縁へ向かって自然に薄くする（横方向かつ wrap_x のときのみ周期境界） */
static void box_blur(const uint8_t *src, uint8_t *dst, int32_t w, int32_t h, int32_t f,
                     bool vertical, bool wrap_x)
{
    const int32_t len = vertical ? h : w;
    const int32_t lines = vertical ? w : h;
    const size_t step = vertical ? (size_t)w : 1u;
    const size_t line_step = vertical ? 1u : (size_t)w;
    const uint32_t span = (uint32_t)(2 * f + 1);
    if (!vertical && wrap_x) {
        for (int32_t line = 0; line < lines; ++line) {
            const uint8_t *s = src + (size_t)line * line_step;
            uint8_t *d = dst + (size_t)line * line_step;
            uint32_t sum = 0u;
            for (int32_t dx = -f; dx <= f; ++dx) {
                int32_t col = dx % len;
                if (col < 0) {
                    col += len;
                }
                sum += s[col];
            }
            for (int32_t i = 0; i < len; ++i) {
                d[i] = (uint8_t)((sum + span / 2u) / span);
                const int32_t col_add = (i + f + 1) % len;
                int32_t col_sub = (i - f) % len;
                if (col_sub < 0) {
                    col_sub += len;
                }
                sum = sum + s[col_add] - s[col_sub];
            }
        }
        return;
    }
    for (int32_t line = 0; line < lines; ++line) {
        const uint8_t *s = src + (size_t)line * line_step;
        uint8_t *d = dst + (size_t)line * line_step;
        uint32_t sum = 0;
        for (int32_t i = 0; i <= f && i < len; ++i) {
            sum += s[(size_t)i * step];
        }
        for (int32_t i = 0; i < len; ++i) {
            d[(size_t)i * step] = (uint8_t)((sum + span / 2u) / span);
            if (i + f + 1 < len) {
                sum += s[(size_t)(i + f + 1) * step];
            }
            if (i - f >= 0) {
                sum -= s[(size_t)(i - f) * step];
            }
        }
    }
}

bool sprite_halo_from_alpha_ex(const uint8_t *alpha, int32_t w, int32_t h, int32_t pad_tex,
                               int32_t feather_tex, bool wrap_x, uint8_t *out, uint8_t *scratch)
{
    if (alpha == NULL || out == NULL || scratch == NULL || w <= 0 || h <= 0 || (w & 1) != 0 ||
        (h & 1) != 0 || w > SPRITE_CANVAS_MAX || h > SPRITE_CANVAS_MAX || pad_tex < 0 ||
        pad_tex > 32 || feather_tex < 0 || feather_tex > 16) {
        return false;
    }
    const int32_t hw = w / 2;
    const int32_t hh = h / 2;
    const size_t n = (size_t)hw * (size_t)hh;
    uint8_t *p0 = scratch;
    uint8_t *p1 = scratch + n;

    for (int32_t y = 0; y < hh; ++y) {
        for (int32_t x = 0; x < hw; ++x) {
            const size_t i0 = (size_t)(2 * y) * (size_t)w + (size_t)(2 * x);
            const uint32_t sum = (uint32_t)alpha[i0] + alpha[i0 + 1] + alpha[i0 + (size_t)w] +
                                 alpha[i0 + (size_t)w + 1];
            const float a = (float)sum / 1020.0f;
            const float m = a >= 0.35f ? 1.0f : a / 0.35f;
            out[(size_t)y * (size_t)hw + (size_t)x] = (uint8_t)(m * 255.0f + 0.5f);
        }
    }

    /* 円盤での膨張。行ごとの最大値を半幅ごとに一度だけ求め、各行の幅へ配る */
    int32_t spans[2 * 32 + 1];
    bool span_used[32 + 1] = {false};
    for (int32_t dy = -pad_tex; dy <= pad_tex; ++dy) {
        spans[dy + pad_tex] = disc_span(pad_tex, dy);
        span_used[spans[dy + pad_tex]] = true;
    }
    memset(p1, 0, n);
    for (int32_t k = 0; k <= pad_tex; ++k) {
        if (!span_used[k]) {
            continue;
        }
        row_max(out, p0, hw, hh, k, wrap_x);
        for (int32_t dy = -pad_tex; dy <= pad_tex; ++dy) {
            if (spans[dy + pad_tex] != k) {
                continue;
            }
            for (int32_t y = 0; y < hh; ++y) {
                const int32_t yy = y + dy;
                if (yy < 0 || yy >= hh) {
                    continue;
                }
                uint8_t *dst = p1 + (size_t)y * (size_t)hw;
                const uint8_t *src = p0 + (size_t)yy * (size_t)hw;
                for (int32_t x = 0; x < hw; ++x) {
                    dst[x] = src[x] > dst[x] ? src[x] : dst[x];
                }
            }
        }
    }

    if (feather_tex > 0) {
        box_blur(p1, p0, hw, hh, feather_tex, false, wrap_x);
        box_blur(p0, out, hw, hh, feather_tex, true, false);
        box_blur(out, p0, hw, hh, feather_tex, false, wrap_x);
        box_blur(p0, p1, hw, hh, feather_tex, true, false);
    }
    for (size_t i = 0; i < n; ++i) {
        const uint32_t v = ((uint32_t)p1[i] * 115u + 50u) / 100u;
        out[i] = (uint8_t)(v > 255u ? 255u : v);
    }
    return true;
}

bool sprite_halo_from_alpha(const uint8_t *alpha, int32_t w, int32_t h, int32_t pad_tex,
                            int32_t feather_tex, uint8_t *out, uint8_t *scratch)
{
    return sprite_halo_from_alpha_ex(alpha, w, h, pad_tex, feather_tex, false, out, scratch);
}

bool sprite_atlas_slot(SpriteId id, bool halo, SpriteRect *out)
{
    if (out == NULL || !sprite_id_valid(id)) {
        return false;
    }
    const int32_t canvas = k_defs[id].canvas;
    int32_t ordinal = 0;
    for (int32_t i = 0; i < id; ++i) {
        ordinal += k_defs[i].canvas == canvas ? 1 : 0;
    }
    const int32_t slot_px = SPRITE_ATLAS_PX / 4;
    int32_t slot = 0;
    int32_t cell = 0;
    int32_t cell_px = 0;
    int32_t limit = 0;
    if (canvas == 512) {
        if (halo) {
            slot = 6 + ordinal / 4;
            cell = ordinal % 4;
            cell_px = 256;
            limit = 8;
        } else {
            slot = ordinal < 6 ? ordinal : 13 + (ordinal - 6);
            cell = 0;
            cell_px = 512;
            limit = 7;
        }
    } else if (canvas == 256) {
        if (halo) {
            slot = ordinal < 16 ? 12 : 15;
            cell = ordinal % 16;
            cell_px = 128;
            limit = 20;
        } else {
            slot = ordinal < 16 ? 8 + ordinal / 4 : 14 + (ordinal - 16) / 4;
            cell = ordinal % 4;
            cell_px = 256;
            limit = 20;
        }
    } else {
        return false;
    }
    if (ordinal >= limit) {
        return false;
    }
    const int32_t per_row = slot_px / cell_px;
    out->x = (slot % 4) * slot_px + (cell % per_row) * cell_px;
    out->y = (slot / 4) * slot_px + (cell / per_row) * cell_px;
    out->w = cell_px;
    out->h = cell_px;
    return true;
}

bool sprite_join_path(const char *dir, const char *file, char *out, size_t size)
{
    if (dir == NULL || file == NULL || out == NULL || size == 0 || dir[0] == '\0' ||
        file[0] == '\0') {
        return false;
    }
    const int written = snprintf(out, size, "%s/%s", dir, file);
    if (written < 0 || (size_t)written >= size) {
        out[0] = '\0';
        return false;
    }
    return true;
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

bool sprite_png_dimensions(const uint8_t *data, size_t size, int32_t *w, int32_t *h)
{
    static const uint8_t sig[8] = {0x89u, 'P', 'N', 'G', '\r', '\n', 0x1au, '\n'};
    /* IHDR は長さ 13 の本体と CRC を含めて 8 + 25 バイト */
    if (data == NULL || size < 33u || memcmp(data, sig, sizeof sig) != 0 ||
        read_be32(data + 8) != 13u || memcmp(data + 12, "IHDR", 4) != 0) {
        return false;
    }
    const uint32_t pw = read_be32(data + 16);
    const uint32_t ph = read_be32(data + 20);
    if (pw == 0u || ph == 0u || pw > (uint32_t)INT32_MAX || ph > (uint32_t)INT32_MAX) {
        return false;
    }
    if (w != NULL) {
        *w = (int32_t)pw;
    }
    if (h != NULL) {
        *h = (int32_t)ph;
    }
    return true;
}

bool sprite_png_validate(const uint8_t *data, size_t size, int32_t expect_w, int32_t expect_h)
{
    int32_t w = 0;
    int32_t h = 0;
    if (!sprite_png_dimensions(data, size, &w, &h) || w != expect_w || h != expect_h) {
        return false;
    }
    /* IHDR: 深度 8・RGBA・deflate・標準フィルタ・非インターレース */
    if (data[24] != 8u || data[25] != 6u || data[26] != 0u || data[27] != 0u || data[28] != 0u) {
        return false;
    }
    size_t off = 8u;
    size_t idat_total = 0u;
    bool has_idat = false;
    while (size - off >= 12u) {
        const uint32_t len = read_be32(data + off);
        if (len > UINT32_C(0x7fffffff) || (size_t)len > size - off - 12u) {
            return false;
        }
        const uint8_t *type = data + off + 4u;
        if (off == 8u && memcmp(type, "IHDR", 4) != 0) {
            return false;
        }
        if (memcmp(type, "IDAT", 4) == 0) {
            idat_total += len;
            has_idat = has_idat || len > 0u;
            if (idat_total > SPRITE_PNG_MAX_IDAT_BYTES) {
                return false;
            }
        } else if (memcmp(type, "IEND", 4) == 0) {
            return len == 0u && has_idat;
        }
        off += 12u + (size_t)len;
    }
    return false;
}

bool sprite_alpha_usable_ex(const uint8_t *rgba, int32_t w, int32_t h, bool tile_x)
{
    if (rgba == NULL || w <= 0 || h <= 0) {
        return false;
    }
    const int32_t min_dim = w < h ? w : h;
    int32_t margin = (min_dim * 8) / 100;
    if (margin < 1) {
        margin = 1;
    }
    bool opaque = false;
    for (int32_t y = 0; y < h; ++y) {
        const bool margin_row = y < margin || y >= h - margin;
        for (int32_t x = 0; x < w; ++x) {
            const uint8_t a = rgba[((size_t)y * (size_t)w + (size_t)x) * 4u + 3u];
            const bool margin_col = !tile_x && (x < margin || x >= w - margin);
            if ((margin_row || margin_col) && a != 0u) {
                return false;
            }
            opaque = opaque || a >= 128u;
        }
    }
    return opaque;
}

bool sprite_alpha_usable(const uint8_t *rgba, int32_t w, int32_t h)
{
    return sprite_alpha_usable_ex(rgba, w, h, false);
}

const char *sprite_bg_file(SpriteBgId id)
{
    return (id >= 0 && id < SPR_BG_COUNT) ? k_bg_files[id] : NULL;
}

bool sprite_bg_alpha_usable(const uint8_t *rgba, int32_t w, int32_t h)
{
    if (rgba == NULL || w <= 0 || h <= 0) {
        return false;
    }
    const size_t uw = (size_t)w;
    const size_t uh = (size_t)h;
    const size_t cx0 = uw / 4u;
    const size_t cx1 = (uw * 3u) / 4u;
    const size_t cy0 = uh / 4u;
    const size_t cy1 = (uh * 3u) / 4u;
    if (cx1 <= cx0 || cy1 <= cy0) {
        return false;
    }
    bool visible = false;
    size_t center_calm = 0u;
    const size_t center_total = (cx1 - cx0) * (cy1 - cy0);
    for (size_t y = 0u; y < uh; ++y) {
        const bool in_cy = y >= cy0 && y < cy1;
        for (size_t x = 0u; x < uw; ++x) {
            const uint8_t a = rgba[(y * uw + x) * 4u + 3u];
            visible = visible || a >= 64u;
            if (in_cy && x >= cx0 && x < cx1 && a <= 32u) {
                ++center_calm;
            }
        }
    }
    return visible && center_calm * 5u >= center_total * 2u;
}

float sprite_polyline_length(const GameVec2 *pts, int32_t n)
{
    float total = 0.0f;
    for (int32_t i = 1; pts != NULL && i < n; ++i) {
        total += vec_dist(pts[i - 1], pts[i]);
    }
    return total;
}

int32_t sprite_resample_polyline(const GameVec2 *in, int32_t n, GameVec2 *out, int32_t out_n)
{
    if (in == NULL || out == NULL || n < 2 || out_n < 2) {
        return 0;
    }
    const float total = sprite_polyline_length(in, n);
    if (!(total > 1e-3f) || !isfinite(total)) {
        return 0;
    }
    int32_t seg = 0;
    float seg_start = 0.0f;
    float seg_len = vec_dist(in[0], in[1]);
    out[0] = in[0];
    for (int32_t i = 1; i < out_n - 1; ++i) {
        const float target = total * ((float)i / (float)(out_n - 1));
        while (seg < n - 2 && seg_start + seg_len < target) {
            seg_start += seg_len;
            ++seg;
            seg_len = vec_dist(in[seg], in[seg + 1]);
        }
        const float t = seg_len > 1e-6f ? clamp01((target - seg_start) / seg_len) : 0.0f;
        out[i] = (GameVec2){in[seg].x + (in[seg + 1].x - in[seg].x) * t,
                            in[seg].y + (in[seg + 1].y - in[seg].y) * t};
    }
    out[out_n - 1] = in[n - 1];
    return out_n;
}

int32_t sprite_spine_socket(const GameVec2 *in, int32_t n, GameVec2 socket, float d_neck,
                            GameVec2 *out, int32_t cap)
{
    if (in == NULL || out == NULL || n < 2 || cap < 2) {
        return 0;
    }
    out[0] = socket;
    int32_t count = 1;
    float acc = 0.0f;
    for (int32_t i = 1; i < n && count < cap; ++i) {
        const float len = vec_dist(in[i - 1], in[i]);
        const float next = acc + len;
        if (next >= d_neck && len > 1e-6f) {
            const float t = (d_neck - acc) / len;
            /* 角でちょうど切れるときは角の点そのものを残し、同じ点を二重にしない */
            if (t > 0.0f && t < 1.0f) {
                out[count++] = (GameVec2){in[i - 1].x + (in[i].x - in[i - 1].x) * t,
                                          in[i - 1].y + (in[i].y - in[i - 1].y) * t};
            }
            if (count < cap) {
                out[count++] = in[i];
            }
        }
        acc = next;
    }
    return count;
}

float sprite_polyline_nearest_arc(const GameVec2 *pts, int32_t n, GameVec2 q)
{
    if (pts == NULL || n < 1) {
        return 0.0f;
    }
    float best_d2 = (q.x - pts[0].x) * (q.x - pts[0].x) + (q.y - pts[0].y) * (q.y - pts[0].y);
    float best_s = 0.0f;
    float acc = 0.0f;
    for (int32_t i = 1; i < n; ++i) {
        const float ex = pts[i].x - pts[i - 1].x;
        const float ey = pts[i].y - pts[i - 1].y;
        const float len2 = ex * ex + ey * ey;
        const float len = sqrtf(len2);
        const float t =
            len2 > 1e-12f ? clamp01(((q.x - pts[i - 1].x) * ex + (q.y - pts[i - 1].y) * ey) / len2)
                          : 0.0f;
        const float px = pts[i - 1].x + ex * t - q.x;
        const float py = pts[i - 1].y + ey * t - q.y;
        const float d2 = px * px + py * py;
        if (d2 < best_d2) {
            best_d2 = d2;
            best_s = acc + len * t;
        }
        acc += len;
    }
    return best_s;
}

SpriteSpineWave sprite_ryu_wave(bool awakened)
{
    return awakened ? (SpriteSpineWave){.amp = 6.5f, .wavelength = 220.0f, .period_s = 1.0f}
                    : (SpriteSpineWave){.amp = 5.0f, .wavelength = 220.0f, .period_s = 1.4f};
}

float sprite_ryu_spine_width(bool awakened, float t_s)
{
    return (awakened ? 28.0f : 26.0f) * (1.0f + 0.04f * sinf(k_two_pi * 0.5f * t_s));
}

float sprite_ryu_body_width(bool awakened, float time_s, float s)
{
    const float u = isfinite(s) ? clamp01(s) : 0.0f;
    const float t = isfinite(time_s) ? time_s : 0.0f;
    const float taper = 0.28f * u + 0.32f * u * u + 0.40f * u * u * u;
    const float base = (86.0f - 54.0f * taper) * (awakened ? 1.09f : 1.0f);
    const float breath = 1.0f + 0.035f * sinf(k_two_pi * 0.55f * t - u * 4.5f);
    return base * breath;
}

float sprite_spine_envelope(float s, float total_len, const float *orb_s, int32_t orb_n)
{
    if (!(total_len > 0.0f)) {
        return 0.0f;
    }
    float a = smooth_step(0.0f, 60.0f, s) * (1.0f + 0.6f * clamp01(s / total_len));
    for (int32_t k = 0; orb_s != NULL && k < orb_n; ++k) {
        a *= smooth_step(0.0f, 36.0f, fabsf(s - orb_s[k]));
    }
    return a;
}

void sprite_spine_undulate(const GameVec2 *in, int32_t n, const SpriteSpineWave *w, float t_s,
                           const float *orb_s, int32_t orb_n, GameVec2 *out)
{
    if (in == NULL || out == NULL || n <= 0) {
        return;
    }
    const float total = sprite_polyline_length(in, n);
    float s = 0.0f;
    /* out が in と同じ配列でも、書き換える前の 1 つ前の点を保持して接線と弧長を求める */
    GameVec2 prev = in[0];
    for (int32_t i = 0; i < n; ++i) {
        const GameVec2 cur = in[i];
        if (i > 0) {
            s += vec_dist(prev, cur);
        }
        const GameVec2 a = prev;
        const GameVec2 b = in[i + 1 < n ? i + 1 : n - 1];
        prev = cur;
        out[i] = cur;
        if (w == NULL || n < 2 || w->wavelength <= 0.0f || w->period_s <= 0.0f) {
            continue;
        }
        const float tx = b.x - a.x;
        const float ty = b.y - a.y;
        const float tl = hypotf(tx, ty);
        if (tl <= 1e-6f) {
            continue;
        }
        const float off = w->amp * sprite_spine_envelope(s, total, orb_s, orb_n) *
                          sinf(k_two_pi * (s / w->wavelength - t_s / w->period_s));
        out[i].x += -ty / tl * off;
        out[i].y += tx / tl * off;
    }
}
