#include "png_rgba.h"
#include "sprite_rules.h"
#include "unity.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static bool is_lower_snake_png(const char *file)
{
    const size_t len = strlen(file);
    if (len < 5 || strcmp(file + len - 4, ".png") != 0) {
        return false;
    }
    for (size_t i = 0; i + 4 < len; ++i) {
        const unsigned char c = (unsigned char)file[i];
        if (!(islower(c) || isdigit(c) || c == '_')) {
            return false;
        }
    }
    return true;
}

static bool uv_in_safe_area(SpriteUv p)
{
    return p.u >= 0.08f && p.u <= 0.92f && p.v >= 0.08f && p.v <= 0.92f;
}

static void test_manifest_every_sprite_has_valid_canvas_file_and_safe_anchors(void)
{
    bool group_has_base[SPR_GROUP_COUNT] = {false};
    for (int32_t i = 0; i < SPR_COUNT; ++i) {
        const SpriteDef *d = sprite_def((SpriteId)i);
        TEST_ASSERT_NOT_NULL(d);
        TEST_ASSERT_TRUE(d->canvas == 256 || d->canvas == 512);
        TEST_ASSERT_TRUE_MESSAGE(is_lower_snake_png(d->file), d->file);
        for (int32_t j = 0; j < i; ++j) {
            TEST_ASSERT_NOT_EQUAL_INT(0, strcmp(d->file, sprite_def((SpriteId)j)->file));
        }
        TEST_ASSERT_TRUE(uv_in_safe_area(d->foot));
        for (int32_t a = 0; a < SPR_ANCHOR_COUNT; ++a) {
            SpriteUv p;
            if (sprite_anchor(d, (SpriteAnchorId)a, &p)) {
                TEST_ASSERT_TRUE_MESSAGE(uv_in_safe_area(p), d->file);
            }
        }
        if (d->group_required) {
            group_has_base[d->group] = true;
            TEST_ASSERT_EQUAL_INT(SPR_NONE, d->fallback);
            TEST_ASSERT_FALSE(d->optional);
        } else {
            const SpriteDef *base = sprite_def(d->fallback);
            TEST_ASSERT_NOT_NULL(base);
            TEST_ASSERT_TRUE(base->group_required);
            TEST_ASSERT_EQUAL_INT(d->group, base->group);
            TEST_ASSERT_EQUAL_INT(d->canvas, base->canvas);
        }
    }
    for (int32_t g = 0; g < SPR_GROUP_COUNT; ++g) {
        TEST_ASSERT_TRUE(group_has_base[g]);
    }
    TEST_ASSERT_EQUAL_INT(20, sprite_core_count());
    TEST_ASSERT_NULL(sprite_def(SPR_NONE));
    TEST_ASSERT_NULL(sprite_def(SPR_COUNT));
}

static void test_ryu_head_states_share_neck_and_eye(void)
{
    const SpriteDef *dormant = sprite_def(SPR_RYU_HEAD_DORMANT);
    const SpriteDef *awake = sprite_def(SPR_RYU_HEAD_AWAKENED);
    const SpriteAnchorId shared[] = {SPR_ANCHOR_NECK, SPR_ANCHOR_EYE_L};
    for (size_t i = 0; i < sizeof shared / sizeof shared[0]; ++i) {
        SpriteUv a;
        SpriteUv b;
        TEST_ASSERT_TRUE(sprite_anchor(dormant, shared[i], &a));
        TEST_ASSERT_TRUE(sprite_anchor(awake, shared[i], &b));
        TEST_ASSERT_FLOAT_WITHIN(0.01f, a.u, b.u);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, a.v, b.v);
    }
}

static void test_display_size_matches_radius_over_body_fraction(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 80.0f, sprite_display_size(sprite_def(SPR_ARCHER_IDLE), 16.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 96.0f, sprite_display_size(sprite_def(SPR_ARCHER_IDLE), 19.2f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 128.0f, sprite_display_size(sprite_def(SPR_SHRINE), 42.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 128.0f, sprite_display_size(sprite_def(SPR_SHRINE), 99.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 293.3f, sprite_display_size(sprite_def(SPR_TENGU_IDLE), 0.0f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sprite_display_size(NULL, 16.0f));
}

static void test_halo_padding_stays_inside_the_transparent_gutter(void)
{
    for (int32_t i = 0; i < SPR_COUNT; ++i) {
        const SpriteDef *d = sprite_def((SpriteId)i);
        int32_t pad = -1;
        int32_t feather = -1;
        sprite_halo_params(d, &pad, &feather);
        TEST_ASSERT_TRUE(pad >= 3);
        TEST_ASSERT_TRUE(feather >= 1);
        const float gutter_half_res = 0.08f * (float)(d->canvas / 2);
        TEST_ASSERT_TRUE_MESSAGE((float)(pad + 2 * feather) <= gutter_half_res, d->file);
    }
}

typedef struct {
    SpriteStateIn in;
    SpriteId expect;
} SelectCase;

static void test_state_selection_table(void)
{
    /* phase 507 mrad gives h = |sin(1.572)| = 1.0; phase 0 gives h = 0 */
    const SelectCase cases[] = {
        {{.def_id = ENEMY_ARCHER, .windup = 0.0f}, SPR_ARCHER_IDLE},
        {{.def_id = ENEMY_ARCHER, .windup = 0.3f}, SPR_ARCHER_DRAW},
        {{.def_id = ENEMY_LANTERN_FIRE}, SPR_LANTERN_FIRE},
        {{.def_id = ENEMY_LANTERN_STORM}, SPR_LANTERN_STORM},
        {{.def_id = ENEMY_UMBRELLA, .phase_milli = 507}, SPR_UMBRELLA_HOP},
        {{.def_id = ENEMY_UMBRELLA, .phase_milli = 0}, SPR_UMBRELLA_CROUCH},
        {{.def_id = ENEMY_KAMAITACHI}, SPR_KAMAITACHI},
        {{.def_id = ENEMY_STONE_ONI, .ai_state = 1}, SPR_STONE_ONI},
        {{.def_id = ENEMY_INK_LEECH}, SPR_INK_LEECH},
        {{.def_id = ENEMY_OFUDA_FOX, .windup = 0.0f}, SPR_OFUDA_FOX},
        {{.def_id = ENEMY_OFUDA_FOX, .windup = 0.4f}, SPR_OFUDA_FOX_CAST},
        {{.def_id = ENEMY_BOSS_TENGU, .telegraph = 0.0f}, SPR_TENGU_IDLE},
        {{.def_id = ENEMY_BOSS_TENGU, .telegraph = 0.02f}, SPR_TENGU_WINDUP},
        {{.def_id = ENEMY_BOSS_NUE, .ai_state = 0}, SPR_NUE_PROWL},
        {{.def_id = ENEMY_BOSS_NUE, .ai_state = 1}, SPR_NUE_CROUCH},
        {{.def_id = ENEMY_BOSS_NUE, .ai_state = 2}, SPR_NUE_PROWL},
        {{.def_id = ENEMY_BOSS_RYU, .phase_idx = 0}, SPR_RYU_HEAD_DORMANT},
        {{.def_id = ENEMY_BOSS_RYU, .phase_idx = 1}, SPR_RYU_HEAD_AWAKENED},
        {{.def_id = ENEMY_BOSS_RYU, .phase_idx = 2}, SPR_RYU_HEAD_AWAKENED},
        {{.def_id = ENEMY_BOSS_RYU_ORB, .part_id = 0}, SPR_HOJU_FIRE},
        {{.def_id = ENEMY_BOSS_RYU_ORB, .part_id = 1}, SPR_HOJU_STORM},
        {{.def_id = ENEMY_BOSS_RYU_ORB, .part_id = 2}, SPR_HOJU_WIND},
        {{.def_id = ENEMY_DEF_COUNT}, SPR_NONE},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(
            cases[i].expect, sprite_select(&cases[i].in),
            sprite_def(cases[i].expect) != NULL ? sprite_def(cases[i].expect)->file : "SPR_NONE");
    }
    TEST_ASSERT_EQUAL_INT(SPR_NONE, sprite_select(NULL));
}

static void test_umbrella_switches_to_crouch_below_hop_height_threshold(void)
{
    int32_t crouch_edge = -1;
    for (int32_t p = 0; p < 507; ++p) {
        if (sprite_umbrella_hop(p) >= 0.22f) {
            crouch_edge = p;
            break;
        }
    }
    TEST_ASSERT_TRUE(crouch_edge > 0);
    const SpriteStateIn below = {.def_id = ENEMY_UMBRELLA, .phase_milli = crouch_edge - 1};
    const SpriteStateIn at = {.def_id = ENEMY_UMBRELLA, .phase_milli = crouch_edge};
    TEST_ASSERT_EQUAL_INT(SPR_UMBRELLA_CROUCH, sprite_select(&below));
    TEST_ASSERT_EQUAL_INT(SPR_UMBRELLA_HOP, sprite_select(&at));
}

static void test_missing_variant_falls_back_to_base_and_missing_base_disables_group(void)
{
    bool present[SPR_COUNT];
    bool group_ok[SPR_GROUP_COUNT];
    for (int32_t i = 0; i < SPR_COUNT; ++i) {
        present[i] = true;
    }
    present[SPR_OFUDA_FOX_CAST] = false;
    present[SPR_ARCHER_DRAW] = false;
    TEST_ASSERT_EQUAL_INT(SPR_GROUP_COUNT, sprite_groups_resolve(present, group_ok));
    TEST_ASSERT_EQUAL_INT(SPR_ARCHER_IDLE, sprite_resolve(SPR_ARCHER_DRAW, present, group_ok));
    TEST_ASSERT_EQUAL_INT(SPR_OFUDA_FOX, sprite_resolve(SPR_OFUDA_FOX_CAST, present, group_ok));
    TEST_ASSERT_EQUAL_INT(SPR_TENGU_WINDUP, sprite_resolve(SPR_TENGU_WINDUP, present, group_ok));

    present[SPR_NUE_PROWL] = false;
    present[SPR_HOJU_WIND] = false;
    TEST_ASSERT_EQUAL_INT(SPR_GROUP_COUNT - 2, sprite_groups_resolve(present, group_ok));
    TEST_ASSERT_FALSE(group_ok[SPR_GROUP_NUE]);
    TEST_ASSERT_EQUAL_INT(SPR_NONE, sprite_resolve(SPR_NUE_CROUCH, present, group_ok));
    TEST_ASSERT_EQUAL_INT(SPR_NONE, sprite_resolve(SPR_NUE_PROWL, present, group_ok));
    TEST_ASSERT_EQUAL_INT(SPR_NONE, sprite_resolve(SPR_HOJU_FIRE, present, group_ok));
    TEST_ASSERT_EQUAL_INT(SPR_NONE, sprite_resolve(SPR_HOJU_STORM, present, group_ok));
    TEST_ASSERT_EQUAL_INT(SPR_SHRINE, sprite_resolve(SPR_SHRINE, present, group_ok));

    TEST_ASSERT_EQUAL_INT(0, sprite_groups_resolve(NULL, group_ok));
    TEST_ASSERT_FALSE(group_ok[SPR_GROUP_SHRINE]);
    TEST_ASSERT_EQUAL_INT(SPR_NONE, sprite_resolve(SPR_NONE, present, group_ok));
}

static void test_face_hysteresis_does_not_chatter_near_vertical(void)
{
    int8_t face = sprite_face_update(0, 0.9f);
    TEST_ASSERT_EQUAL_INT(1, face);
    int flips = 0;
    const float wobble[] = {0.1f, -0.1f, 0.05f, -0.2f, 0.24f, -0.24f, 0.0f, -0.1f};
    for (size_t i = 0; i < sizeof wobble / sizeof wobble[0]; ++i) {
        const int8_t next = sprite_face_update(face, wobble[i]);
        flips += next != face ? 1 : 0;
        face = next;
    }
    TEST_ASSERT_EQUAL_INT(0, flips);

    const float sweep[] = {-0.1f, -0.26f, -0.3f, -0.1f, 0.1f, 0.2f};
    for (size_t i = 0; i < sizeof sweep / sizeof sweep[0]; ++i) {
        const int8_t next = sprite_face_update(face, sweep[i]);
        flips += next != face ? 1 : 0;
        face = next;
    }
    TEST_ASSERT_EQUAL_INT(1, flips);
    TEST_ASSERT_EQUAL_INT(-1, face);
    TEST_ASSERT_EQUAL_INT(1, sprite_face_update(face, 0.26f));
    TEST_ASSERT_EQUAL_INT(-1, sprite_face_update(0, -0.01f));
}

static void test_paper_turn_narrows_then_reopens_on_the_new_side(void)
{
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, sprite_turn_sx(1, 0u));
    float min_mag = 2.0f;
    float prev = sprite_turn_sx(1, 0u);
    for (uint32_t t = 1; t <= (uint32_t)SPRITE_TURN_TICKS; ++t) {
        const float cur = sprite_turn_sx(1, t);
        TEST_ASSERT_TRUE(cur >= prev);
        min_mag = fminf(min_mag, fabsf(cur));
        prev = cur;
    }
    TEST_ASSERT_TRUE(min_mag >= 0.149f);
    TEST_ASSERT_TRUE(min_mag < 0.5f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sprite_turn_sx(1, (uint32_t)SPRITE_TURN_TICKS));
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, sprite_turn_sx(-1, 100u));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sprite_turn_sx(0, 0u));
}

static SpriteXform base_xform(float size)
{
    return (SpriteXform){.x = 400.0f, .y = 300.0f, .size = size, .sx = 1.0f, .sy = 1.0f};
}

static void quad_centre(const SpriteQuad *q, float *cx, float *cy)
{
    *cx = 0.25f * (q->x[0] + q->x[1] + q->x[2] + q->x[3]);
    *cy = 0.25f * (q->y[0] + q->y[1] + q->y[2] + q->y[3]);
}

static float quad_cross(const SpriteQuad *q)
{
    return (q->x[1] - q->x[0]) * (q->y[2] - q->y[0]) - (q->y[1] - q->y[0]) * (q->x[2] - q->x[0]);
}

static void test_quad_mirror_keeps_collision_centre_fixed(void)
{
    const SpriteDef *d = sprite_def(SPR_UMBRELLA_HOP);
    SpriteXform xf = base_xform(90.0f);
    SpriteQuad q;
    sprite_quad(d, &xf, 0.0f, 0.0f, 1.0f, 1.0f, &q);
    float cx = 0.0f;
    float cy = 0.0f;
    quad_centre(&q, &cx, &cy);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 400.0f, cx);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 300.0f, cy);
    TEST_ASSERT_TRUE(quad_cross(&q) < 0.0f);

    xf.sx = -1.0f;
    sprite_quad(d, &xf, 0.0f, 0.0f, 1.0f, 1.0f, &q);
    quad_centre(&q, &cx, &cy);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 400.0f, cx);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 300.0f, cy);
    TEST_ASSERT_TRUE(quad_cross(&q) < 0.0f);

    xf = base_xform(90.0f);
    xf.flip_v = true;
    xf.rot = 2.5f;
    sprite_quad(sprite_def(SPR_KAMAITACHI), &xf, 0.0f, 0.0f, 1.0f, 1.0f, &q);
    quad_centre(&q, &cx, &cy);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 400.0f, cx);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 300.0f, cy);
    TEST_ASSERT_TRUE(quad_cross(&q) < 0.0f);

    float fx0 = 0.0f;
    float fy0 = 0.0f;
    float fx1 = 0.0f;
    float fy1 = 0.0f;
    xf = base_xform(90.0f);
    sprite_xform_point(d, &xf, d->foot, &fx0, &fy0);
    for (int k = 0; k < 2; ++k) {
        xf.sx = k == 0 ? 0.96f : -1.04f;
        xf.sy = k == 0 ? 1.05f : 0.94f;
        SpriteXform mirrored_rest = xf;
        mirrored_rest.sy = 1.0f;
        mirrored_rest.sx = xf.sx < 0.0f ? -1.0f : 1.0f;
        float rx = 0.0f;
        float ry = 0.0f;
        sprite_xform_point(d, &mirrored_rest, d->foot, &rx, &ry);
        sprite_xform_point(d, &xf, d->foot, &fx1, &fy1);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, rx, fx1);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, ry, fy1);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, fy0, fy1);
    }
}

static void test_anchor_transform_matches_quad_corners(void)
{
    const SpriteDef *d = sprite_def(SPR_ARCHER_DRAW);
    const SpriteXform variants[] = {
        {.x = 100.0f, .y = 120.0f, .size = 80.0f, .sx = 1.0f, .sy = 1.0f},
        {.x = 640.0f, .y = 360.0f, .size = 96.0f, .sx = -0.9f, .sy = 1.02f, .rot = 0.1f},
        {.x = 10.0f,
         .y = 700.0f,
         .size = 80.0f,
         .sx = 1.0f,
         .sy = 0.94f,
         .rot = -2.0f,
         .flip_v = true,
         .dx = 0.3f,
         .dy = -4.0f},
    };
    for (size_t i = 0; i < sizeof variants / sizeof variants[0]; ++i) {
        SpriteQuad q;
        sprite_quad(d, &variants[i], 0.0f, 0.0f, 1.0f, 1.0f, &q);
        for (int k = 0; k < 4; ++k) {
            float wx = 0.0f;
            float wy = 0.0f;
            sprite_xform_point(d, &variants[i], (SpriteUv){q.u[k], q.v[k]}, &wx, &wy);
            TEST_ASSERT_FLOAT_WITHIN(1e-3f, wx, q.x[k]);
            TEST_ASSERT_FLOAT_WITHIN(1e-3f, wy, q.y[k]);
        }
    }

    SpriteQuad q;
    const SpriteXform xf = base_xform(80.0f);
    sprite_quad(d, &xf, 0.25f, 0.5f, 0.375f, 0.625f, &q);
    for (int k = 0; k < 4; ++k) {
        TEST_ASSERT_TRUE(q.u[k] == 0.25f || q.u[k] == 0.375f);
        TEST_ASSERT_TRUE(q.v[k] == 0.5f || q.v[k] == 0.625f);
    }
}

static void test_quad_without_transform_is_zeroed(void)
{
    SpriteQuad q;
    memset(&q, 0x7f, sizeof q);
    sprite_quad(sprite_def(SPR_SHRINE), NULL, 0.0f, 0.0f, 1.0f, 1.0f, &q);
    for (int k = 0; k < 4; ++k) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, q.x[k]);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, q.y[k]);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, q.u[k]);
        TEST_ASSERT_EQUAL_FLOAT(0.0f, q.v[k]);
    }
    sprite_quad(sprite_def(SPR_SHRINE), NULL, 0.0f, 0.0f, 1.0f, 1.0f, NULL);
}

static void test_breathing_keeps_volume_feel_and_jelly_moves_axes_oppositely(void)
{
    float sx = 0.0f;
    float sy = 0.0f;
    sprite_breath(0.018f, 2.8f, 0.7f, 0.0f, false, &sx, &sy);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.018f, sy);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f - 0.4f * 0.018f, sx);
    sprite_breath(0.04f, 0.9f, 0.225f, 0.0f, true, &sx, &sy);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.04f, sy);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.96f, sx);
    sprite_breath(0.0f, 0.0f, 3.0f, 1.0f, false, &sx, &sy);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sx);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, sy);
}

static void test_boil_moves_silhouette_by_at_most_point_eight_pixels(void)
{
    const SpriteDef *d = sprite_def(SPR_STONE_ONI);
    const float sizes[] = {80.0f, 128.0f, 293.0f};
    const SpriteUv probes[] = {{0.08f, 0.08f}, {0.92f, 0.08f}, {0.5f, 0.08f}, {0.92f, 0.92f}};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; ++s) {
        for (uint32_t seed = 0; seed < 4096u; seed += 37u) {
            const uint32_t mixed = seed * 2654435761u;
            const SpriteBoil b = sprite_boil(mixed, sizes[s]);
            const SpriteXform rest = base_xform(sizes[s]);
            const SpriteXform boiled = {.x = 400.0f,
                                        .y = 300.0f,
                                        .size = sizes[s],
                                        .sx = b.scale,
                                        .sy = b.scale,
                                        .rot = b.rot,
                                        .dx = b.dx,
                                        .dy = b.dy};
            for (size_t p = 0; p < sizeof probes / sizeof probes[0]; ++p) {
                float x0 = 0.0f;
                float y0 = 0.0f;
                float x1 = 0.0f;
                float y1 = 0.0f;
                sprite_xform_point(d, &rest, probes[p], &x0, &y0);
                sprite_xform_point(d, &boiled, probes[p], &x1, &y1);
                TEST_ASSERT_TRUE(hypotf(x1 - x0, y1 - y0) <= 0.8f + 1e-3f);
            }
        }
    }
}

enum { HALO_W = 64, HALO_H = 64, HALO_HW = HALO_W / 2, HALO_HH = HALO_H / 2 };

static void test_halo_dilates_single_texel_to_feathered_disc(void)
{
    uint8_t alpha[HALO_W * HALO_H] = {0};
    const int cx = 16;
    const int cy = 16;
    alpha[(2 * cy) * HALO_W + 2 * cx] = 255;
    alpha[(2 * cy) * HALO_W + 2 * cx + 1] = 255;
    alpha[(2 * cy + 1) * HALO_W + 2 * cx] = 255;
    alpha[(2 * cy + 1) * HALO_W + 2 * cx + 1] = 255;

    /* Guard bytes after each buffer catch out-of-bounds writes even without ASan. */
    uint8_t out[HALO_HW * HALO_HH + 16];
    uint8_t scratch[2 * HALO_HW * HALO_HH + 16];
    memset(out, 0xa5, sizeof out);
    memset(scratch, 0xa5, sizeof scratch);
    const int pad = 4;
    const int feather = 2;
    TEST_ASSERT_TRUE(sprite_halo_from_alpha(alpha, HALO_W, HALO_H, pad, feather, out, scratch));
    for (size_t i = HALO_HW * HALO_HH; i < sizeof out; ++i) {
        TEST_ASSERT_EQUAL_HEX8(0xa5, out[i]);
    }
    for (size_t i = 2 * HALO_HW * HALO_HH; i < sizeof scratch; ++i) {
        TEST_ASSERT_EQUAL_HEX8(0xa5, scratch[i]);
    }

    TEST_ASSERT_EQUAL_UINT8(255, out[cy * HALO_HW + cx]);
    int last_nonzero = 0;
    uint8_t prev = 255;
    for (int dx = 0; cx + dx < HALO_HW; ++dx) {
        const uint8_t v = out[cy * HALO_HW + cx + dx];
        TEST_ASSERT_TRUE(v <= prev);
        prev = v;
        if (v > 0) {
            last_nonzero = dx;
        }
    }
    TEST_ASSERT_TRUE(last_nonzero >= pad + feather - 1);
    TEST_ASSERT_TRUE(last_nonzero <= pad + 2 * feather + 1);
    TEST_ASSERT_TRUE(out[cy * HALO_HW + cx + pad] >= 128);

    /* A disc, not a square: the diagonal reaches less far than the axis. */
    int diag_nonzero = 0;
    for (int k = 0; cx + k < HALO_HW && cy + k < HALO_HH; ++k) {
        if (out[(cy + k) * HALO_HW + cx + k] > 0) {
            diag_nonzero = k;
        }
    }
    TEST_ASSERT_TRUE(diag_nonzero < last_nonzero);
    TEST_ASSERT_EQUAL_UINT8(0, out[0]);
}

static void test_halo_interior_is_solid_and_soft_alpha_is_binarised(void)
{
    uint8_t alpha[HALO_W * HALO_H];
    for (int y = 0; y < HALO_H; ++y) {
        for (int x = 0; x < HALO_W; ++x) {
            const bool inside = x >= 16 && x < 48 && y >= 16 && y < 48;
            alpha[y * HALO_W + x] = inside ? 100u : 0u;
        }
    }
    uint8_t out[HALO_HW * HALO_HH];
    uint8_t scratch[2 * HALO_HW * HALO_HH];
    TEST_ASSERT_TRUE(sprite_halo_from_alpha(alpha, HALO_W, HALO_H, 2, 1, out, scratch));
    TEST_ASSERT_EQUAL_UINT8(255, out[16 * HALO_HW + 16]);
    TEST_ASSERT_EQUAL_UINT8(255, out[9 * HALO_HW + 16]);
    TEST_ASSERT_EQUAL_UINT8(0, out[2 * HALO_HW + 2]);
}

static void test_halo_rejects_odd_or_oversized_inputs(void)
{
    uint8_t alpha[8 * 8] = {0};
    uint8_t out[16];
    uint8_t scratch[32];
    memset(out, 0x5a, sizeof out);
    TEST_ASSERT_FALSE(sprite_halo_from_alpha(alpha, 7, 8, 2, 1, out, scratch));
    TEST_ASSERT_FALSE(sprite_halo_from_alpha(alpha, 8, 7, 2, 1, out, scratch));
    TEST_ASSERT_FALSE(sprite_halo_from_alpha(alpha, 0, 8, 2, 1, out, scratch));
    TEST_ASSERT_FALSE(sprite_halo_from_alpha(alpha, 514, 8, 2, 1, out, scratch));
    TEST_ASSERT_FALSE(sprite_halo_from_alpha(alpha, 8, 8, -1, 1, out, scratch));
    TEST_ASSERT_FALSE(sprite_halo_from_alpha(alpha, 8, 8, 2, 17, out, scratch));
    TEST_ASSERT_FALSE(sprite_halo_from_alpha(NULL, 8, 8, 2, 1, out, scratch));
    TEST_ASSERT_FALSE(sprite_halo_from_alpha(alpha, 8, 8, 2, 1, out, NULL));
    for (size_t i = 0; i < sizeof out; ++i) {
        TEST_ASSERT_EQUAL_HEX8(0x5a, out[i]);
    }
}

/* The original O(w·k) dilation, kept as the byte-for-byte reference for the sliding maximum. */
static void reference_halo(const uint8_t *alpha, int w, int h, int pad, int feather, uint8_t *out)
{
    enum { REF_MAX = 128 * 128 };
    static uint8_t m[REF_MAX];
    static uint8_t d[REF_MAX];
    static uint8_t t[REF_MAX];
    const int hw = w / 2;
    const int hh = h / 2;
    for (int y = 0; y < hh; ++y) {
        for (int x = 0; x < hw; ++x) {
            const int i0 = (2 * y) * w + 2 * x;
            const float a =
                (float)(alpha[i0] + alpha[i0 + 1] + alpha[i0 + w] + alpha[i0 + w + 1]) / 1020.0f;
            const float v = a >= 0.35f ? 1.0f : a / 0.35f;
            m[y * hw + x] = (uint8_t)(v * 255.0f + 0.5f);
        }
    }
    const float rr = ((float)pad + 0.5f) * ((float)pad + 0.5f);
    for (int y = 0; y < hh; ++y) {
        for (int x = 0; x < hw; ++x) {
            uint8_t best = 0;
            for (int dy = -pad; dy <= pad; ++dy) {
                const int span = (int)floorf(sqrtf(rr - (float)(dy * dy)));
                const int yy = y + dy;
                if (yy < 0 || yy >= hh) {
                    continue;
                }
                for (int dx = -span; dx <= span; ++dx) {
                    const int xx = x + dx;
                    if (xx >= 0 && xx < hw && m[yy * hw + xx] > best) {
                        best = m[yy * hw + xx];
                    }
                }
            }
            d[y * hw + x] = best;
        }
    }
    for (int pass = 0; pass < 2 && feather > 0; ++pass) {
        const int span = 2 * feather + 1;
        for (int y = 0; y < hh; ++y) {
            for (int x = 0; x < hw; ++x) {
                unsigned sum = 0;
                for (int k = -feather; k <= feather; ++k) {
                    sum += (x + k >= 0 && x + k < hw) ? d[y * hw + x + k] : 0u;
                }
                t[y * hw + x] = (uint8_t)((sum + (unsigned)span / 2u) / (unsigned)span);
            }
        }
        for (int y = 0; y < hh; ++y) {
            for (int x = 0; x < hw; ++x) {
                unsigned sum = 0;
                for (int k = -feather; k <= feather; ++k) {
                    sum += (y + k >= 0 && y + k < hh) ? t[(y + k) * hw + x] : 0u;
                }
                d[y * hw + x] = (uint8_t)((sum + (unsigned)span / 2u) / (unsigned)span);
            }
        }
    }
    for (int i = 0; i < hw * hh; ++i) {
        const unsigned v = ((unsigned)d[i] * 115u + 50u) / 100u;
        out[i] = (uint8_t)(v > 255u ? 255u : v);
    }
}

static void test_halo_matches_reference_dilation_byte_for_byte(void)
{
    enum { RW = 256, RH = 256, RN = (RW / 2) * (RH / 2) };
    static uint8_t alpha[RW * RH];
    static uint8_t got[RN];
    static uint8_t want[RN];
    static uint8_t scratch[2 * RN];
    uint32_t state = 0x12345678u;
    for (int i = 0; i < RW * RH; ++i) {
        state = state * 1664525u + 1013904223u;
        const int x = i % RW;
        const int y = i / RW;
        const bool inside = (x - 128) * (x - 128) / 3 + (y - 120) * (y - 120) < 70 * 70;
        alpha[i] = inside ? (uint8_t)(state >> 24) : ((state >> 28) == 0u ? 90u : 0u);
    }
    const int params[][2] = {{0, 0}, {1, 1}, {3, 1}, {4, 2}, {5, 2}, {9, 3}};
    for (size_t p = 0; p < sizeof params / sizeof params[0]; ++p) {
        TEST_ASSERT_TRUE(
            sprite_halo_from_alpha(alpha, RW, RH, params[p][0], params[p][1], got, scratch));
        reference_halo(alpha, RW, RH, params[p][0], params[p][1], want);
        TEST_ASSERT_EQUAL_MEMORY(want, got, RN);
    }
}

static bool rects_overlap(SpriteRect a, SpriteRect b)
{
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static void test_atlas_slots_do_not_overlap_and_fit(void)
{
    SpriteRect rects[2 * SPR_COUNT];
    int n = 0;
    for (int32_t i = 0; i < SPR_COUNT; ++i) {
        const SpriteDef *d = sprite_def((SpriteId)i);
        for (int halo = 0; halo < 2; ++halo) {
            SpriteRect rc;
            TEST_ASSERT_TRUE(sprite_atlas_slot((SpriteId)i, halo != 0, &rc));
            TEST_ASSERT_EQUAL_INT(halo ? d->canvas / 2 : d->canvas, rc.w);
            TEST_ASSERT_EQUAL_INT(rc.w, rc.h);
            TEST_ASSERT_TRUE(rc.x >= 0 && rc.y >= 0);
            TEST_ASSERT_TRUE(rc.x + rc.w <= SPRITE_ATLAS_PX && rc.y + rc.h <= SPRITE_ATLAS_PX);
            for (int j = 0; j < n; ++j) {
                TEST_ASSERT_FALSE(rects_overlap(rc, rects[j]));
            }
            rects[n++] = rc;
        }
    }
    SpriteRect rc;
    TEST_ASSERT_FALSE(sprite_atlas_slot(SPR_NONE, false, &rc));
    TEST_ASSERT_FALSE(sprite_atlas_slot(SPR_COUNT, true, &rc));
    TEST_ASSERT_FALSE(sprite_atlas_slot(SPR_SHRINE, false, NULL));
}

static void test_join_path_reports_overflow(void)
{
    char buf[32];
    TEST_ASSERT_TRUE(sprite_join_path("assets/sprites", "shrine.png", buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("assets/sprites/shrine.png", buf);
    char tiny[12];
    TEST_ASSERT_FALSE(sprite_join_path("assets/sprites", "shrine.png", tiny, sizeof tiny));
    TEST_ASSERT_EQUAL_STRING("", tiny);
    TEST_ASSERT_FALSE(sprite_join_path(NULL, "shrine.png", buf, sizeof buf));
    TEST_ASSERT_FALSE(sprite_join_path("", "shrine.png", buf, sizeof buf));
    TEST_ASSERT_FALSE(sprite_join_path("assets", "", buf, sizeof buf));
}

static size_t put_chunk(uint8_t *buf, size_t at, const char *type, const uint8_t *body,
                        uint32_t len)
{
    buf[at + 0] = (uint8_t)(len >> 24);
    buf[at + 1] = (uint8_t)(len >> 16);
    buf[at + 2] = (uint8_t)(len >> 8);
    buf[at + 3] = (uint8_t)len;
    memcpy(buf + at + 4, type, 4);
    if (len > 0u && body != NULL) {
        memcpy(buf + at + 8, body, len);
    }
    memset(buf + at + 8 + len, 0, 4);
    return at + 12u + len;
}

/* CRC is not part of the precheck, so the fixtures leave it zero. */
static size_t make_png(uint8_t *buf, uint32_t w, uint32_t h, uint8_t depth, uint8_t color,
                       uint8_t interlace, uint32_t idat_len, bool with_iend)
{
    static const uint8_t sig[8] = {0x89u, 'P', 'N', 'G', '\r', '\n', 0x1au, '\n'};
    memcpy(buf, sig, sizeof sig);
    const uint8_t ihdr[13] = {(uint8_t)(w >> 24),
                              (uint8_t)(w >> 16),
                              (uint8_t)(w >> 8),
                              (uint8_t)w,
                              (uint8_t)(h >> 24),
                              (uint8_t)(h >> 16),
                              (uint8_t)(h >> 8),
                              (uint8_t)h,
                              depth,
                              color,
                              0u,
                              0u,
                              interlace};
    size_t at = put_chunk(buf, 8u, "IHDR", ihdr, 13u);
    at = put_chunk(buf, at, "tEXt", (const uint8_t *)"k\0v", 3u);
    at = put_chunk(buf, at, "IDAT", NULL, idat_len);
    if (with_iend) {
        at = put_chunk(buf, at, "IEND", NULL, 0u);
    }
    return at;
}

static void test_png_dimensions_are_read_before_decoding(void)
{
    uint8_t png[128] = {0};
    const size_t len = make_png(png, 256u, 512u, 8u, 6u, 0u, 16u, true);
    int32_t w = 0;
    int32_t h = 0;
    TEST_ASSERT_TRUE(sprite_png_dimensions(png, len, &w, &h));
    TEST_ASSERT_EQUAL_INT(256, w);
    TEST_ASSERT_EQUAL_INT(512, h);
    TEST_ASSERT_FALSE(sprite_png_dimensions(png, 32, &w, &h));
    png[11] = 12u;
    TEST_ASSERT_FALSE(sprite_png_dimensions(png, len, &w, &h));
    png[11] = 13u;
    png[1] = 'J';
    TEST_ASSERT_FALSE(sprite_png_dimensions(png, len, &w, &h));
    png[1] = 'P';
    png[18] = 0;
    TEST_ASSERT_FALSE(sprite_png_dimensions(png, len, &w, &h));
}

static void test_png_validate_accepts_only_bounded_rgba8_streams(void)
{
    uint8_t png[128];
    size_t len = make_png(png, 256u, 256u, 8u, 6u, 0u, 16u, true);
    TEST_ASSERT_TRUE(sprite_png_validate(png, len, 256, 256));
    TEST_ASSERT_FALSE(sprite_png_validate(png, len, 512, 512));
    TEST_ASSERT_FALSE(sprite_png_validate(png, len - 1u, 256, 256));
    TEST_ASSERT_FALSE(sprite_png_validate(NULL, len, 256, 256));

    const struct {
        uint8_t depth, color, interlace;
    } bad_formats[] = {{16u, 6u, 0u}, {8u, 2u, 0u}, {8u, 4u, 0u}, {8u, 3u, 0u}, {8u, 6u, 1u}};
    for (size_t i = 0; i < sizeof bad_formats / sizeof bad_formats[0]; ++i) {
        len = make_png(png, 256u, 256u, bad_formats[i].depth, bad_formats[i].color,
                       bad_formats[i].interlace, 16u, true);
        TEST_ASSERT_FALSE(sprite_png_validate(png, len, 256, 256));
    }

    len = make_png(png, 256u, 256u, 8u, 6u, 0u, 16u, false);
    TEST_ASSERT_FALSE(sprite_png_validate(png, len, 256, 256));
    len = make_png(png, 256u, 256u, 8u, 6u, 0u, 0u, true);
    TEST_ASSERT_FALSE(sprite_png_validate(png, len, 256, 256));

    /* A chunk length that runs past the buffer must not be followed. */
    len = make_png(png, 256u, 256u, 8u, 6u, 0u, 16u, true);
    png[33 + 0] = 0x7fu;
    TEST_ASSERT_FALSE(sprite_png_validate(png, len, 256, 256));
    png[33 + 0] = 0xffu;
    TEST_ASSERT_FALSE(sprite_png_validate(png, len, 256, 256));
}

static void test_png_validate_caps_total_idat_payload(void)
{
    const uint32_t half = SPRITE_PNG_MAX_IDAT_BYTES / 2u;
    uint8_t *png = calloc(1, (size_t)SPRITE_PNG_MAX_IDAT_BYTES + 256u);
    TEST_ASSERT_NOT_NULL(png);
    size_t len = make_png(png, 256u, 256u, 8u, 6u, 0u, half, false);
    len = put_chunk(png, len, "IDAT", NULL, half);
    len = put_chunk(png, len, "IEND", NULL, 0u);
    TEST_ASSERT_TRUE(sprite_png_validate(png, len, 256, 256));

    len = make_png(png, 256u, 256u, 8u, 6u, 0u, half, false);
    len = put_chunk(png, len, "IDAT", NULL, half + 1u);
    len = put_chunk(png, len, "IEND", NULL, 0u);
    TEST_ASSERT_FALSE(sprite_png_validate(png, len, 256, 256));
    free(png);
}

static void test_alpha_usable_rejects_blank_and_solid_squares(void)
{
    enum { AW = 8, AH = 8 };
    uint8_t rgba[AW * AH * 4];
    memset(rgba, 0, sizeof rgba);
    TEST_ASSERT_FALSE(sprite_alpha_usable(rgba, AW, AH));
    rgba[(3 * AW + 3) * 4 + 3] = 200u;
    TEST_ASSERT_TRUE(sprite_alpha_usable(rgba, AW, AH));
    /* Any opaque pixel touching the outer border is rejected to protect atlas gutters. */
    rgba[3] = 1u;
    TEST_ASSERT_FALSE(sprite_alpha_usable(rgba, AW, AH));
    memset(rgba, 255, sizeof rgba);
    TEST_ASSERT_FALSE(sprite_alpha_usable(rgba, AW, AH));
    rgba[3] = 0u;
    TEST_ASSERT_FALSE(sprite_alpha_usable(rgba, AW, AH));
    TEST_ASSERT_FALSE(sprite_alpha_usable(NULL, AW, AH));
}

static size_t make_stored_zlib(uint8_t *out, const uint8_t *raw, size_t raw_len)
{
    size_t o = 0u;
    out[o++] = 0x78u;
    out[o++] = 0x01u;
    out[o++] = 0x01u; /* bfinal=1, btype=0 (stored) */
    out[o++] = (uint8_t)(raw_len & 0xffu);
    out[o++] = (uint8_t)((raw_len >> 8) & 0xffu);
    const uint16_t nlen = (uint16_t)~(uint16_t)raw_len;
    out[o++] = (uint8_t)(nlen & 0xffu);
    out[o++] = (uint8_t)((nlen >> 8) & 0xffu);
    memcpy(out + o, raw, raw_len);
    o += raw_len;
    uint32_t s1 = 1u;
    uint32_t s2 = 0u;
    for (size_t i = 0; i < raw_len; ++i) {
        s1 = (s1 + raw[i]) % 65521u;
        s2 = (s2 + s1) % 65521u;
    }
    const uint32_t adler = (s2 << 16) | s1;
    out[o++] = (uint8_t)(adler >> 24);
    out[o++] = (uint8_t)(adler >> 16);
    out[o++] = (uint8_t)(adler >> 8);
    out[o++] = (uint8_t)adler;
    return o;
}

static void test_png_rgba_decode_handles_filters_and_rejects_oversized_inflate(void)
{
    /* 2x2 RGBA8 image: row 0 = (10,20,30,255),(40,50,60,128); row 1 = (15,25,35,200),(70,80,90,64)
     */
    const uint8_t expect[16] = {10, 20, 30, 255, 40, 50, 60, 128, 15, 25, 35, 200, 70, 80, 90, 64};
    /* Row 0 filter 1 (Sub): pixel0 raw, pixel1 = (30,30,30,129) mod 256.
     * Row 1 filter 4 (Paeth): pixel0 has a=0,b=row0[0],c=0 -> pred=b, diff=(5,5,5,201);
     * pixel1 has a=(15,25,35,200),b=(40,50,60,128),c=(10,20,30,255) ->
     * p=(45,55,65,73), closest is b=(40,50,60) and for alpha closest is b=128 ->
     * diff=(30,30,30,192).
     */
    const uint8_t raw[18] = {1u, 10, 20, 30, 255, 30, 30, 30, 129,
                             4u, 5,  5,  5,  201, 30, 30, 30, 192};
    uint8_t zbuf[64];
    const size_t zlen = make_stored_zlib(zbuf, raw, sizeof raw);

    /* Split zlib stream across two IDAT chunks to exercise multi-IDAT streaming. */
    uint8_t png[256];
    size_t len = make_png(png, 2u, 2u, 8u, 6u, 0u, 0u, false);
    /* Rewind past the empty IDAT make_png wrote (12 bytes) */
    len -= 12u;
    len = put_chunk(png, len, "IDAT", zbuf, 9u);
    len = put_chunk(png, len, "IDAT", zbuf + 9u, (uint32_t)(zlen - 9u));
    len = put_chunk(png, len, "IEND", NULL, 0u);

    uint8_t rgba_out[16 + 8];
    uint8_t raw_scratch[18 + 8];
    memset(rgba_out, 0xa5, sizeof rgba_out);
    memset(raw_scratch, 0x5a, sizeof raw_scratch);
    TEST_ASSERT_TRUE(png_rgba_decode(png, len, 2, 2, rgba_out, 16u, raw_scratch, 18u));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, rgba_out, 16);
    for (size_t i = 16u; i < sizeof rgba_out; ++i) {
        TEST_ASSERT_EQUAL_HEX8(0xa5, rgba_out[i]);
    }
    for (size_t i = 18u; i < sizeof raw_scratch; ++i) {
        TEST_ASSERT_EQUAL_HEX8(0x5a, raw_scratch[i]);
    }

    /* Corrupt Adler-32 must fail. */
    uint8_t bad_z[64];
    memcpy(bad_z, zbuf, zlen);
    bad_z[zlen - 1u] ^= 1u;
    len = make_png(png, 2u, 2u, 8u, 6u, 0u, 0u, false) - 12u;
    len = put_chunk(png, len, "IDAT", bad_z, (uint32_t)zlen);
    len = put_chunk(png, len, "IEND", NULL, 0u);
    TEST_ASSERT_FALSE(png_rgba_decode(png, len, 2, 2, rgba_out, 16u, raw_scratch, 18u));

    /* Oversized decompressed output (zip bomb: 19 bytes for 18-byte 2x2 image) must abort before
     * touching guard bytes. */
    uint8_t bomb_raw[19] = {0};
    const size_t blen = make_stored_zlib(zbuf, bomb_raw, sizeof bomb_raw);
    len = make_png(png, 2u, 2u, 8u, 6u, 0u, 0u, false) - 12u;
    len = put_chunk(png, len, "IDAT", zbuf, (uint32_t)blen);
    len = put_chunk(png, len, "IEND", NULL, 0u);
    memset(raw_scratch + 18u, 0x5a, 8u);
    TEST_ASSERT_FALSE(png_rgba_decode(png, len, 2, 2, rgba_out, 16u, raw_scratch, 18u));
    for (size_t i = 18u; i < sizeof raw_scratch; ++i) {
        TEST_ASSERT_EQUAL_HEX8(0x5a, raw_scratch[i]);
    }
}

enum { SPINE_IN = 128 };

static void make_wavy_trail(GameVec2 *pts, int n)
{
    for (int i = 0; i < n; ++i) {
        const float u = (float)i / (float)(n - 1);
        pts[i] = (GameVec2){300.0f + 600.0f * u, 360.0f + 60.0f * sinf(u * 6.0f)};
    }
}

static void test_spine_resample_has_even_arc_spacing(void)
{
    GameVec2 in[SPINE_IN];
    make_wavy_trail(in, SPINE_IN);
    GameVec2 out[RENDER_RYU_SPINE_PTS];
    TEST_ASSERT_EQUAL_INT(RENDER_RYU_SPINE_PTS,
                          sprite_resample_polyline(in, SPINE_IN, out, RENDER_RYU_SPINE_PTS));
    const float total = sprite_polyline_length(in, SPINE_IN);
    const float step = total / (float)(RENDER_RYU_SPINE_PTS - 1);
    for (int i = 1; i < RENDER_RYU_SPINE_PTS; ++i) {
        const float d = hypotf(out[i].x - out[i - 1].x, out[i].y - out[i - 1].y);
        TEST_ASSERT_FLOAT_WITHIN(step * 0.01f, step, d);
    }
    TEST_ASSERT_EQUAL_FLOAT(in[0].x, out[0].x);
    TEST_ASSERT_EQUAL_FLOAT(in[SPINE_IN - 1].y, out[RENDER_RYU_SPINE_PTS - 1].y);

    const GameVec2 still[3] = {{5.0f, 5.0f}, {5.0f, 5.0f}, {5.0f, 5.0f}};
    TEST_ASSERT_EQUAL_INT(0, sprite_resample_polyline(still, 3, out, RENDER_RYU_SPINE_PTS));
    TEST_ASSERT_EQUAL_INT(0, sprite_resample_polyline(in, 1, out, RENDER_RYU_SPINE_PTS));
    TEST_ASSERT_EQUAL_INT(0, sprite_resample_polyline(in, SPINE_IN, out, 1));
}

static void test_spine_socket_replaces_the_neck_run_with_the_head_socket(void)
{
    const GameVec2 line[5] = {{0, 0}, {50, 0}, {100, 0}, {150, 0}, {200, 0}};
    GameVec2 out[8];
    const GameVec2 socket = {70.0f, 4.0f};
    const int32_t n = sprite_spine_socket(line, 5, socket, 77.0f, out, 8);
    TEST_ASSERT_EQUAL_INT(5, n);
    TEST_ASSERT_EQUAL_FLOAT(70.0f, out[0].x);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 77.0f, out[1].x);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, out[2].x);
    TEST_ASSERT_EQUAL_FLOAT(200.0f, out[4].x);
    TEST_ASSERT_EQUAL_INT(3, sprite_spine_socket(line, 5, socket, 77.0f, out, 3));
}

static void test_spine_socket_keeps_an_exact_corner_once(void)
{
    const GameVec2 bend[4] = {{0, 0}, {50, 0}, {50, 50}, {50, 100}};
    GameVec2 out[8];
    const GameVec2 socket = {40.0f, -6.0f};
    const int32_t n = sprite_spine_socket(bend, 4, socket, 50.0f, out, 8);
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_FLOAT(40.0f, out[0].x);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, out[1].x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, out[1].y);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, out[2].y);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, out[3].y);
}

static void test_spine_socket_starts_short_or_still_trails_at_the_socket(void)
{
    const GameVec2 line[3] = {{0, 0}, {10, 0}, {20, 0}};
    const GameVec2 still[3] = {{5, 5}, {5, 5}, {5, 5}};
    GameVec2 out[4];
    const GameVec2 socket = {-30.0f, 2.0f};
    TEST_ASSERT_EQUAL_INT(1, sprite_spine_socket(line, 3, socket, 77.0f, out, 4));
    TEST_ASSERT_EQUAL_FLOAT(-30.0f, out[0].x);
    TEST_ASSERT_EQUAL_INT(1, sprite_spine_socket(still, 3, socket, 0.0f, out, 4));
    TEST_ASSERT_EQUAL_FLOAT(2.0f, out[0].y);
    TEST_ASSERT_EQUAL_INT(0, sprite_spine_socket(line, 1, socket, 0.0f, out, 4));
}

static void test_nearest_arc_projects_onto_the_polyline(void)
{
    const GameVec2 line[3] = {{0, 0}, {100, 0}, {100, 100}};
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 40.0f, sprite_polyline_nearest_arc(line, 3, (GameVec2){40, 9}));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 150.0f,
                             sprite_polyline_nearest_arc(line, 3, (GameVec2){120, 50}));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, sprite_polyline_nearest_arc(line, 3, (GameVec2){-9, -9}));
}

static void test_spine_envelope_is_zero_at_neck_and_orbs(void)
{
    const float orbs[3] = {180.0f, 360.0f, 540.0f};
    const float total = 700.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sprite_spine_envelope(0.0f, total, orbs, 3));
    for (int k = 0; k < 3; ++k) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, sprite_spine_envelope(orbs[k], total, orbs, 3));
        TEST_ASSERT_TRUE(sprite_spine_envelope(orbs[k] + 10.0f, total, orbs, 3) > 0.0f);
        TEST_ASSERT_TRUE(sprite_spine_envelope(orbs[k] + 10.0f, total, orbs, 3) < 0.5f);
    }
    const float mid_early = sprite_spine_envelope(100.0f, total, NULL, 0);
    const float mid_late = sprite_spine_envelope(650.0f, total, NULL, 0);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f + 0.6f * 100.0f / total, mid_early);
    TEST_ASSERT_TRUE(mid_late > mid_early);
    TEST_ASSERT_TRUE(mid_late <= 1.6f + 1e-4f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sprite_spine_envelope(10.0f, 0.0f, orbs, 3));
}

static void test_spine_undulation_moves_body_but_holds_neck_and_orbs(void)
{
    GameVec2 straight[RENDER_RYU_SPINE_PTS];
    for (int i = 0; i < RENDER_RYU_SPINE_PTS; ++i) {
        straight[i] = (GameVec2){(float)i * 10.0f, 200.0f};
    }
    const float orbs[1] = {300.0f};
    const SpriteSpineWave wave = sprite_ryu_wave(true);
    GameVec2 moved[RENDER_RYU_SPINE_PTS];
    float max_off = 0.0f;
    for (int step = 0; step < 8; ++step) {
        const float t = (float)step * 0.13f;
        sprite_spine_undulate(straight, RENDER_RYU_SPINE_PTS, &wave, t, orbs, 1, moved);
        TEST_ASSERT_EQUAL_FLOAT(200.0f, moved[0].y);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 200.0f, moved[30].y);
        for (int i = 0; i < RENDER_RYU_SPINE_PTS; ++i) {
            TEST_ASSERT_EQUAL_FLOAT(straight[i].x, moved[i].x);
            const float off = fabsf(moved[i].y - 200.0f);
            TEST_ASSERT_TRUE(off <= wave.amp * 1.6f + 1e-3f);
            max_off = fmaxf(max_off, off);
        }
    }
    TEST_ASSERT_TRUE(max_off > 2.0f);
    TEST_ASSERT_TRUE(sprite_ryu_wave(false).period_s > sprite_ryu_wave(true).period_s);
    TEST_ASSERT_FLOAT_WITHIN(26.0f * 0.04f + 1e-3f, 26.0f, sprite_ryu_spine_width(false, 0.7f));
    TEST_ASSERT_FLOAT_WITHIN(28.0f * 0.04f + 1e-3f, 28.0f, sprite_ryu_spine_width(true, 1.3f));
}

static void test_spine_undulation_in_place_matches_separate_buffers(void)
{
    GameVec2 in[RENDER_RYU_SPINE_PTS];
    make_wavy_trail(in, RENDER_RYU_SPINE_PTS);
    const float orbs[2] = {120.0f, 400.0f};
    const SpriteSpineWave wave = sprite_ryu_wave(false);
    GameVec2 separate[RENDER_RYU_SPINE_PTS];
    sprite_spine_undulate(in, RENDER_RYU_SPINE_PTS, &wave, 0.37f, orbs, 2, separate);
    sprite_spine_undulate(in, RENDER_RYU_SPINE_PTS, &wave, 0.37f, orbs, 2, in);
    for (int i = 0; i < RENDER_RYU_SPINE_PTS; ++i) {
        TEST_ASSERT_EQUAL_FLOAT(separate[i].x, in[i].x);
        TEST_ASSERT_EQUAL_FLOAT(separate[i].y, in[i].y);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_manifest_every_sprite_has_valid_canvas_file_and_safe_anchors);
    RUN_TEST(test_ryu_head_states_share_neck_and_eye);
    RUN_TEST(test_display_size_matches_radius_over_body_fraction);
    RUN_TEST(test_halo_padding_stays_inside_the_transparent_gutter);
    RUN_TEST(test_state_selection_table);
    RUN_TEST(test_umbrella_switches_to_crouch_below_hop_height_threshold);
    RUN_TEST(test_missing_variant_falls_back_to_base_and_missing_base_disables_group);
    RUN_TEST(test_face_hysteresis_does_not_chatter_near_vertical);
    RUN_TEST(test_paper_turn_narrows_then_reopens_on_the_new_side);
    RUN_TEST(test_quad_mirror_keeps_collision_centre_fixed);
    RUN_TEST(test_anchor_transform_matches_quad_corners);
    RUN_TEST(test_quad_without_transform_is_zeroed);
    RUN_TEST(test_breathing_keeps_volume_feel_and_jelly_moves_axes_oppositely);
    RUN_TEST(test_boil_moves_silhouette_by_at_most_point_eight_pixels);
    RUN_TEST(test_halo_dilates_single_texel_to_feathered_disc);
    RUN_TEST(test_halo_interior_is_solid_and_soft_alpha_is_binarised);
    RUN_TEST(test_halo_rejects_odd_or_oversized_inputs);
    RUN_TEST(test_halo_matches_reference_dilation_byte_for_byte);
    RUN_TEST(test_atlas_slots_do_not_overlap_and_fit);
    RUN_TEST(test_join_path_reports_overflow);
    RUN_TEST(test_png_dimensions_are_read_before_decoding);
    RUN_TEST(test_png_validate_accepts_only_bounded_rgba8_streams);
    RUN_TEST(test_png_validate_caps_total_idat_payload);
    RUN_TEST(test_alpha_usable_rejects_blank_and_solid_squares);
    RUN_TEST(test_png_rgba_decode_handles_filters_and_rejects_oversized_inflate);
    RUN_TEST(test_spine_resample_has_even_arc_spacing);
    RUN_TEST(test_spine_socket_replaces_the_neck_run_with_the_head_socket);
    RUN_TEST(test_spine_socket_keeps_an_exact_corner_once);
    RUN_TEST(test_spine_socket_starts_short_or_still_trails_at_the_socket);
    RUN_TEST(test_nearest_arc_projects_onto_the_polyline);
    RUN_TEST(test_spine_envelope_is_zero_at_neck_and_orbs);
    RUN_TEST(test_spine_undulation_moves_body_but_holds_neck_and_orbs);
    RUN_TEST(test_spine_undulation_in_place_matches_separate_buffers);
    return UNITY_END();
}
