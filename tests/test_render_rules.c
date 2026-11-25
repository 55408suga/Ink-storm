#include "render_rules.h"
#include "unity.h"

#include <math.h>
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

static void test_gpu_fluid_needs_post_and_a_loaded_shader(void)
{
    TEST_ASSERT_TRUE(render_fluid_use_gpu(true, 7u));
    TEST_ASSERT_FALSE(render_fluid_use_gpu(false, 7u));
    TEST_ASSERT_FALSE(render_fluid_use_gpu(true, 0u));
    TEST_ASSERT_FALSE(render_fluid_use_gpu(false, 0u));
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

static void test_polyline_plan_keeps_every_segment_within_the_cap(void)
{
    float len[40];
    int steps[40];
    for (int s = 0; s < 40; ++s) {
        len[s] = 300.0f;
    }
    const int total = render_polyline_plan(len, 40, 256, steps);
    TEST_ASSERT_TRUE(total <= 256);
    int sum = 0;
    for (int s = 0; s < 40; ++s) {
        TEST_ASSERT_TRUE(steps[s] >= 1);
        TEST_ASSERT_EQUAL_INT(steps[0], steps[s]);
        sum += steps[s];
    }
    TEST_ASSERT_EQUAL_INT(total, sum + 1);

    const float short_len[3] = {2.0f, 40.0f, 1000.0f};
    TEST_ASSERT_EQUAL_INT(2 + 10 + 24 + 1, render_polyline_plan(short_len, 3, 256, steps));
    TEST_ASSERT_EQUAL_INT(2, steps[0]);
    TEST_ASSERT_EQUAL_INT(10, steps[1]);
    TEST_ASSERT_EQUAL_INT(24, steps[2]);

    TEST_ASSERT_EQUAL_INT(0, render_polyline_plan(len, 0, 256, steps));
    TEST_ASSERT_EQUAL_INT(4, render_polyline_plan(len, 5, 4, steps));
    TEST_ASSERT_EQUAL_INT(0, steps[4]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lobby_and_acts_use_distinct_background_keys);
    RUN_TEST(test_gpu_fluid_needs_post_and_a_loaded_shader);
    RUN_TEST(test_field_packs_ink_fire_storm_and_solid_per_texel);
    RUN_TEST(test_pale_ink_survives_eight_bit_encoding);
    RUN_TEST(test_cpu_ink_darkens_steadily_without_pooling_steps);
    RUN_TEST(test_cpu_glow_is_premultiplied_and_blends_fire_with_storm);
    RUN_TEST(test_polyline_plan_keeps_every_segment_within_the_cap);
    return UNITY_END();
}
