#include "render_rules.h"
#include "unity.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

static FluidState *f;
static unsigned char field[RENDER_FLUID_FIELD_BYTES];

void setUp(void)
{
    f = calloc(1, sizeof *f);
    TEST_ASSERT_NOT_NULL(f);
    fluid_init(f, NULL);
}

void tearDown(void)
{
    free(f);
    f = NULL;
}

static RenderFluidPalette test_palette(void)
{
    return (RenderFluidPalette){
        .sumi_clear = {0.79f, 0.76f, 0.70f, 1.0f},
        .sumi_light = {0.55f, 0.52f, 0.48f, 1.0f},
        .sumi_heavy = {0.29f, 0.27f, 0.24f, 1.0f},
        .sumi_dark = {0.17f, 0.15f, 0.14f, 1.0f},
        .sumi_scorch = {0.10f, 0.09f, 0.08f, 1.0f},
        .shu = {0.78f, 0.27f, 0.17f, 1.0f},
        .shu_hot = {0.94f, 0.54f, 0.24f, 1.0f},
        .ao = {0.18f, 0.44f, 0.70f, 1.0f},
        .ao_hot = {0.62f, 0.83f, 0.95f, 1.0f},
        .kin_hot = {0.95f, 0.82f, 0.48f, 1.0f},
    };
}

static float luminance(RenderRgba c) { return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b; }

static void test_lobby_and_acts_use_distinct_background_keys(void)
{
    const BgKey lobby = render_bg_key(false, 0);
    TEST_ASSERT_EQUAL_INT(BG_LOBBY, lobby.kind);
    TEST_ASSERT_TRUE(render_bg_key_equal(lobby, render_bg_key(false, 4)));
    TEST_ASSERT_FALSE(render_bg_key_equal(lobby, render_bg_key(true, 0)));
    TEST_ASSERT_FALSE(render_bg_key_equal(render_bg_key(true, 0), render_bg_key(true, 3)));
    TEST_ASSERT_TRUE(render_bg_key_equal(render_bg_key(true, -2), render_bg_key(true, 0)));
}

static void test_gpu_fluid_needs_post_shader_and_washi(void)
{
    TEST_ASSERT_TRUE(render_fluid_use_gpu(true, 7u, 3u));
    TEST_ASSERT_FALSE(render_fluid_use_gpu(false, 7u, 3u));
    TEST_ASSERT_FALSE(render_fluid_use_gpu(true, 0u, 3u));
    TEST_ASSERT_FALSE(render_fluid_use_gpu(true, 7u, 0u));
}

static void test_field_packs_ink_fire_storm_and_solid_per_texel(void)
{
    const int i = 45 * FLUID_W + 80;
    f->dye[0][i] = f->cfg.max_dye;
    f->dye[1][i + 1] = f->cfg.max_dye * 0.25f;
    f->dye[2][i + 2] = NAN;
    f->dye[0][i + 3] = -1.0f;
    render_fluid_encode_field(f, field);

    TEST_ASSERT_EQUAL_UINT8(255, field[i * 4 + 0]);
    TEST_ASSERT_EQUAL_UINT8(0, field[i * 4 + 1]);
    TEST_ASSERT_EQUAL_UINT8(128, field[(i + 1) * 4 + 1]);
    TEST_ASSERT_EQUAL_UINT8(0, field[(i + 2) * 4 + 2]);
    TEST_ASSERT_EQUAL_UINT8(0, field[(i + 3) * 4 + 0]);
    TEST_ASSERT_EQUAL_UINT8(255, field[3]);
    TEST_ASSERT_EQUAL_UINT8(0, field[i * 4 + 3]);
}

static void test_extreme_max_dye_keeps_density_ratios(void)
{
    const int i = 30 * FLUID_W + 30;
    f->cfg.max_dye = 1e-39f; /* subnormal: 1 / max_dye overflows */
    f->dye[0][i] = 0.5e-39f;
    f->dye[1][i] = 1e-39f;
    f->dye[2][i] = 3.0e38f;
    render_fluid_encode_field(f, field);
    TEST_ASSERT_UINT8_WITHIN(1, 180, field[i * 4 + 0]);
    TEST_ASSERT_EQUAL_UINT8(255, field[i * 4 + 1]);
    TEST_ASSERT_EQUAL_UINT8(255, field[i * 4 + 2]);

    f->cfg.max_dye = FLT_MAX;
    f->dye[0][i] = FLT_MAX * 0.25f;
    render_fluid_encode_field(f, field);
    TEST_ASSERT_EQUAL_UINT8(128, field[i * 4 + 0]);
}

static void test_pale_ink_survives_eight_bit_encoding(void)
{
    /* A linear 8-bit ramp over [0, max_dye] would be off by up to 0.0049 here. */
    const int i = 40 * FLUID_W + 40;
    for (int k = 1; k <= 30; ++k) {
        const float v = 0.01f * (float)k;
        f->dye[0][i] = v;
        render_fluid_encode_field(f, field);
        const float back = render_fluid_decode_channel(field[i * 4], f->cfg.max_dye);
        TEST_ASSERT_FLOAT_WITHIN(0.0035f, v, back);
    }
}

static void assert_rgb_within(float delta, RenderRgba expected, RenderRgba actual)
{
    TEST_ASSERT_FLOAT_WITHIN(delta, expected.r, actual.r);
    TEST_ASSERT_FLOAT_WITHIN(delta, expected.g, actual.g);
    TEST_ASSERT_FLOAT_WITHIN(delta, expected.b, actual.b);
}

static void test_ink_curve_hits_art_direction_tones_and_opacities(void)
{
    const RenderFluidPalette pal = test_palette();
    const struct {
        float ink;
        RenderRgba tone;
        float alpha;
    } cases[] = {
        {0.04f, {1.0f, 1.0f, 1.0f, 0.0f}, 0.0f}, /* paper untouched */
        {0.05f, pal.sumi_clear, 0.15f},          {0.25f, pal.sumi_light, 0.45f},
        {0.80f, pal.sumi_scorch, 0.92f},         {1.50f, pal.sumi_scorch, 0.92f},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        const RenderRgba c = render_fluid_ink_curve(&pal, cases[i].ink, 0.0f);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, cases[i].alpha, c.a);
        assert_rgb_within(1e-5f, cases[i].tone, c);
    }
}

static void test_ink_curve_darkens_monotonically_without_steps_above_paper(void)
{
    const RenderFluidPalette pal = test_palette();
    RenderRgba prev = render_fluid_ink_curve(&pal, 0.05f, 0.0f);
    for (int k = 1; k <= 1950; ++k) {
        const RenderRgba cur = render_fluid_ink_curve(&pal, 0.05f + 0.001f * (float)k, 0.0f);
        TEST_ASSERT_TRUE(cur.a >= prev.a - 1e-6f);
        TEST_ASSERT_FLOAT_WITHIN(0.003f, prev.a, cur.a);
        TEST_ASSERT_TRUE(luminance(cur) <= luminance(prev) + 1e-6f);
        TEST_ASSERT_FLOAT_WITHIN(0.003f, luminance(prev), luminance(cur));
        prev = cur;
    }
}

static void test_wet_edge_pooling_only_darkens_and_stays_within_scorch_opacity(void)
{
    const RenderFluidPalette pal = test_palette();
    const float grads[] = {0.1f, 0.4f, 0.85f};
    for (int k = 5; k <= 200; ++k) {
        const float ink = 0.01f * (float)k;
        const RenderRgba flat = render_fluid_ink_curve(&pal, ink, 0.0f);
        for (size_t i = 0; i < sizeof grads / sizeof grads[0]; ++i) {
            const RenderRgba pooled = render_fluid_ink_curve(&pal, ink, grads[i]);
            TEST_ASSERT_TRUE(pooled.a >= flat.a - 1e-6f);
            TEST_ASSERT_TRUE(pooled.a <= 0.92f + 1e-6f);
            TEST_ASSERT_TRUE(luminance(pooled) <= luminance(flat) + 1e-6f);
        }
    }
}

static void test_cpu_ink_darkens_steadily_without_pooling_steps(void)
{
    const RenderFluidPalette pal = test_palette();
    const RenderRgba none = render_fluid_cpu_ink(&pal, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, none.r);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, none.b);
    float prev = 1.0f;
    for (int k = 1; k <= 60; ++k) {
        const float lum = luminance(render_fluid_cpu_ink(&pal, 0.04f * (float)k, 0.0f));
        TEST_ASSERT_TRUE(lum <= prev + 1e-6f);
        prev = lum;
    }
    for (int k = 0; k < 400; ++k) {
        const float g = 0.001f * (float)k;
        const float a = luminance(render_fluid_cpu_ink(&pal, 0.2f, g));
        const float b = luminance(render_fluid_cpu_ink(&pal, 0.2f, g + 0.001f));
        TEST_ASSERT_FLOAT_WITHIN(0.004f, a, b);
    }
}

static void test_cpu_glow_is_premultiplied_and_blends_fire_with_storm(void)
{
    const RenderFluidPalette pal = test_palette();
    const RenderRgba clear = render_fluid_cpu_elem(&pal, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, clear.a);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, clear.r);
    for (int fi = 0; fi <= 12; ++fi) {
        for (int si = 0; si <= 12; ++si) {
            const RenderRgba c = render_fluid_cpu_elem(&pal, 0.2f * (float)fi, 0.2f * (float)si);
            TEST_ASSERT_TRUE(c.r <= c.a + 1e-6f && c.g <= c.a + 1e-6f && c.b <= c.a + 1e-6f);
        }
    }
    const RenderRgba fire = render_fluid_cpu_elem(&pal, 0.8f, 0.0f);
    const RenderRgba storm = render_fluid_cpu_elem(&pal, 0.0f, 0.8f);
    const RenderRgba both = render_fluid_cpu_elem(&pal, 0.8f, 0.8f);
    TEST_ASSERT_TRUE(both.a >= fire.a && both.a >= storm.a);
    TEST_ASSERT_TRUE(both.b / both.a > fire.b / fire.a + 0.05f);
    TEST_ASSERT_TRUE(both.r / both.a > storm.r / storm.a + 0.05f);
}

static void test_polyline_plan_gives_each_segment_its_length_share(void)
{
    RenderPolySample plan[256];
    const float short_len[3] = {2.0f, 40.0f, 1000.0f};
    TEST_ASSERT_EQUAL_INT(2 + 10 + 24, render_polyline_plan(short_len, 3, 256, plan));
    TEST_ASSERT_EQUAL_INT32(0, plan[0].seg);
    TEST_ASSERT_EQUAL_INT32(1, plan[2].seg);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, plan[2].t);
    TEST_ASSERT_EQUAL_INT32(2, plan[12].seg);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, plan[24].t);

    float len[40];
    for (int s = 0; s < 40; ++s) {
        len[s] = 300.0f;
    }
    const int n = render_polyline_plan(len, 40, 256, plan);
    TEST_ASSERT_TRUE(n <= 255);
    int per_seg[40] = {0};
    for (int k = 0; k < n; ++k) {
        ++per_seg[plan[k].seg];
    }
    for (int s = 0; s < 40; ++s) {
        TEST_ASSERT_EQUAL_INT(per_seg[0], per_seg[s]);
        TEST_ASSERT_TRUE(per_seg[s] >= 1);
    }
    TEST_ASSERT_EQUAL_INT(0, render_polyline_plan(len, 0, 256, plan));
    TEST_ASSERT_EQUAL_INT(0, render_polyline_plan(len, 3, 1, plan));
}

static void test_polyline_plan_with_more_segments_than_samples_reaches_the_tail(void)
{
    /* 300 control points whose last 40 segments fold back sharply: every tail segment must be
     * sampled instead of being replaced by a chord to the end point. */
    float len[300];
    for (int s = 0; s < 300; ++s) {
        len[s] = s < 260 ? 10.0f : 30.0f;
    }
    RenderPolySample plan[256];
    const int n = render_polyline_plan(len, 300, 256, plan);
    TEST_ASSERT_EQUAL_INT(255, n);
    bool tail_hit[40] = {false};
    for (int k = 0; k < n; ++k) {
        TEST_ASSERT_TRUE(plan[k].t >= 0.0f && plan[k].t <= 1.0f);
        if (k > 0) {
            TEST_ASSERT_TRUE(plan[k].seg > plan[k - 1].seg ||
                             (plan[k].seg == plan[k - 1].seg && plan[k].t > plan[k - 1].t));
        }
        if (plan[k].seg >= 260) {
            tail_hit[plan[k].seg - 260] = true;
        }
    }
    for (int s = 0; s < 40; ++s) {
        TEST_ASSERT_TRUE(tail_hit[s]);
    }

    float zero[300] = {0};
    TEST_ASSERT_EQUAL_INT(255, render_polyline_plan(zero, 300, 256, plan));
    TEST_ASSERT_TRUE(plan[254].seg >= 295);
}

static void test_rain_phase_advances_continuously_across_long_duration(void)
{
    const float dt = 1.0f / 60.0f;
    float px = 0.0f;
    float py = 0.0f;
    float prev_x[44];
    float prev_y[44];
    for (int i = 0; i < 44; ++i) {
        render_rain_streak_pos(i, px, py, &prev_x[i], &prev_y[i]);
    }
    const float expect_dx = dt * 60.0f * 0.45f;
    const float expect_dy = dt * 60.0f;
    for (int t = 0; t < 600; ++t) {
        render_rain_advance(&px, &py, dt);
        int wrapped = 0;
        for (int i = 0; i < 44; ++i) {
            float x = 0.0f;
            float y = 0.0f;
            render_rain_streak_pos(i, px, py, &x, &y);
            const float dx = x - prev_x[i];
            const float dy = y - prev_y[i];
            const bool dx_ok = fabsf(dx - expect_dx) < 0.05f ||
                               fabsf(dx - (expect_dx - RENDER_RAIN_PERIOD_X)) < 0.05f;
            const bool dy_ok = fabsf(dy - expect_dy) < 0.05f ||
                               fabsf(dy - (expect_dy - RENDER_RAIN_PERIOD_Y)) < 0.05f;
            TEST_ASSERT_TRUE(dx_ok);
            TEST_ASSERT_TRUE(dy_ok);
            if (fabsf(dx - expect_dx) >= 0.05f || fabsf(dy - expect_dy) >= 0.05f) {
                ++wrapped;
            }
            prev_x[i] = x;
            prev_y[i] = y;
        }
        /* No synchronized reset of all 44 streaks on any tick (even past 5s). */
        TEST_ASSERT_TRUE(wrapped < 10);
    }
}

static void test_fluid_upload_cache_matches_and_invalidates(void)
{
    FluidUploadCache cache = {0};
    TEST_ASSERT_FALSE(render_fluid_cache_matches(&cache, f, true));
    cache = (FluidUploadCache){
        .valid = true,
        .field_ptr = f,
        .instance_id = f->instance_id,
        .steps = f->steps,
        .mutation_rev = f->mutation_rev,
        .max_dye = f->cfg.max_dye,
        .gpu_path = true,
    };
    TEST_ASSERT_TRUE(render_fluid_cache_matches(&cache, f, true));
    TEST_ASSERT_FALSE(render_fluid_cache_matches(&cache, f, false));

    f->steps += 1u;
    TEST_ASSERT_FALSE(render_fluid_cache_matches(&cache, f, true));
    f->steps -= 1u;
    TEST_ASSERT_TRUE(render_fluid_cache_matches(&cache, f, true));

    const float dye[FLUID_MAX_CHANNELS] = {0.5f, 0.0f, 0.0f, 0.0f};
    fluid_splat(f, 320.0f, 240.0f, 3.0f, 0.0f, 0.0f, dye);
    TEST_ASSERT_FALSE(render_fluid_cache_matches(&cache, f, true));

    cache.mutation_rev = f->mutation_rev;
    TEST_ASSERT_TRUE(render_fluid_cache_matches(&cache, f, true));

    /* An open fluid_channel_mut() borrow must bypass upload caching even after syncing
     * mutation_rev, so later writes through the retained pointer can never serve stale textures. */
    float *ink_mut = fluid_channel_mut(f, 0);
    TEST_ASSERT_NOT_NULL(ink_mut);
    cache.mutation_rev = f->mutation_rev;
    TEST_ASSERT_FALSE(render_fluid_cache_matches(&cache, f, true));
    ink_mut[40 * FLUID_W + 40] = 1.25f;
    TEST_ASSERT_FALSE(render_fluid_cache_matches(&cache, f, true));

    /* Stepping the fluid closes the mutable borrow and allows caching again. */
    fluid_step(f, 1.0f / 60.0f);
    TEST_ASSERT_FALSE(fluid_has_open_mut_borrow(f));
    cache.steps = f->steps;
    cache.mutation_rev = f->mutation_rev;
    TEST_ASSERT_TRUE(render_fluid_cache_matches(&cache, f, true));

    f->cfg.max_dye += 1.0f;
    TEST_ASSERT_FALSE(render_fluid_cache_matches(&cache, f, true));
    f->cfg.max_dye -= 1.0f;
    fluid_init(f, NULL);
    TEST_ASSERT_FALSE(render_fluid_cache_matches(&cache, f, true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lobby_and_acts_use_distinct_background_keys);
    RUN_TEST(test_gpu_fluid_needs_post_shader_and_washi);
    RUN_TEST(test_field_packs_ink_fire_storm_and_solid_per_texel);
    RUN_TEST(test_extreme_max_dye_keeps_density_ratios);
    RUN_TEST(test_pale_ink_survives_eight_bit_encoding);
    RUN_TEST(test_ink_curve_hits_art_direction_tones_and_opacities);
    RUN_TEST(test_ink_curve_darkens_monotonically_without_steps_above_paper);
    RUN_TEST(test_wet_edge_pooling_only_darkens_and_stays_within_scorch_opacity);
    RUN_TEST(test_cpu_ink_darkens_steadily_without_pooling_steps);
    RUN_TEST(test_cpu_glow_is_premultiplied_and_blends_fire_with_storm);
    RUN_TEST(test_polyline_plan_gives_each_segment_its_length_share);
    RUN_TEST(test_polyline_plan_with_more_segments_than_samples_reaches_the_tail);
    RUN_TEST(test_rain_phase_advances_continuously_across_long_duration);
    RUN_TEST(test_fluid_upload_cache_matches_and_invalidates);
    return UNITY_END();
}
