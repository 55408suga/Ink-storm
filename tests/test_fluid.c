#include "fluid.h"
#include "unity.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const float k_dt = 1.0f / 60.0f;
static FluidState *f;
static FluidState *replay;

void setUp(void)
{
    f = calloc(1, sizeof *f);
    TEST_ASSERT_NOT_NULL(f);
    fluid_init(f, NULL);
}

void tearDown(void)
{
    free(f);
    free(replay);
    replay = NULL;
}

static int cell(int x, int y) { return y * FLUID_W + x; }

static bool outer(int x, int y) { return x == 0 || y == 0 || x == FLUID_W - 1 || y == FLUID_H - 1; }

static void assert_config(FluidConfig expected, FluidConfig actual)
{
    TEST_ASSERT_EQUAL_INT(expected.project_iters, actual.project_iters);
    TEST_ASSERT_EQUAL_INT(expected.active_channels, actual.active_channels);
    TEST_ASSERT_EQUAL_FLOAT(expected.vorticity_eps, actual.vorticity_eps);
    TEST_ASSERT_EQUAL_FLOAT(expected.vorticity_thresh, actual.vorticity_thresh);
    TEST_ASSERT_EQUAL_FLOAT(expected.max_vel, actual.max_vel);
    TEST_ASSERT_EQUAL_FLOAT(expected.max_dye, actual.max_dye);
    TEST_ASSERT_EQUAL_FLOAT(expected.vel_damp, actual.vel_damp);
    TEST_ASSERT_EQUAL_FLOAT_ARRAY(expected.dye_damp, actual.dye_damp, FLUID_MAX_CHANNELS);
}

static void test_init_and_reset(void)
{
    const FluidConfig defaults = {.project_iters = 12,
                                  .active_channels = 3,
                                  .vorticity_eps = 9.5f,
                                  .vorticity_thresh = 0.35f,
                                  .max_vel = 125.0f,
                                  .max_dye = 2.5f,
                                  .vel_damp = 0.985f,
                                  .dye_damp = {0.985f, 0.935f, 0.905f, 0.950f}};
    assert_config(defaults, fluid_config_default());
    assert_config(defaults, f->cfg);
    for (int y = 0; y < FLUID_H; ++y) {
        for (int x = 0; x < FLUID_W; ++x) {
            int i = cell(x, y);
            TEST_ASSERT_EQUAL_UINT8(outer(x, y), f->solid[i]);
            TEST_ASSERT_TRUE(f->u[i] == 0 && f->v[i] == 0);
            for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
                TEST_ASSERT_TRUE(f->dye[ch][i] == 0);
            }
        }
    }
    fluid_step(f, k_dt);
    TEST_ASSERT_EQUAL_UINT64(1, f->steps);
    TEST_ASSERT_TRUE(f->stats.max_vel == 0 && f->stats.max_vort == 0);
    TEST_ASSERT_TRUE(f->stats.solve_ms >= 0.0 && isfinite(f->stats.solve_ms));

    FluidConfig cfg = {.project_iters = 1000,
                       .active_channels = 9,
                       .vorticity_eps = NAN,
                       .vorticity_thresh = -1,
                       .max_vel = -1,
                       .max_dye = INFINITY,
                       .vel_damp = 2,
                       .dye_damp = {-1, 3, NAN, 0.625f}};
    FluidConfig expected = defaults;
    expected.project_iters = 64;
    expected.active_channels = 4;
    expected.vel_damp = 1;
    expected.dye_damp[0] = 0;
    expected.dye_damp[1] = 1;
    expected.dye_damp[3] = 0.625f;
    fluid_init(f, &cfg);
    assert_config(expected, f->cfg);
    fluid_set_solid_rect(f, 640, 320, 16, 16, true);
    fluid_splat(f, 420, 420, 10, 30, 40, (float[]){1, 1, 1, 1});
    fluid_step(f, k_dt);
    fluid_reset(f);
    assert_config(expected, f->cfg);
    TEST_ASSERT_EQUAL_UINT64(0, f->steps);
    TEST_ASSERT_TRUE(f->stats.solve_ms == 0 && f->stats.max_vel == 0 && f->stats.max_vort == 0);
    for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
        TEST_ASSERT_TRUE(f->stats.max_dye[ch] == 0);
    }
    for (int y = 0; y < FLUID_H; ++y) {
        for (int x = 0; x < FLUID_W; ++x) {
            int i = cell(x, y);
            bool pillar = (x == 80 || x == 81) && (y == 40 || y == 41);
            TEST_ASSERT_EQUAL_UINT8(outer(x, y) || pillar, f->solid[i]);
            TEST_ASSERT_TRUE(f->u[i] == 0 && f->v[i] == 0 && f->u0[i] == 0 && f->v0[i] == 0);
            TEST_ASSERT_TRUE(f->div[i] == 0 && f->p[i] == 0 && f->curl[i] == 0);
            for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
                TEST_ASSERT_TRUE(f->dye[ch][i] == 0 && f->dye0[ch][i] == 0);
            }
        }
    }
}

static void test_continuous_strong_inputs_stay_bounded(void)
{
    const int points[][2] = {{640, 360}, {4, 360}, {1276, 360}, {640, 4},   {640, 716},
                             {4, 4},     {4, 716}, {1276, 4},   {1276, 716}};
    for (int t = 0; t < 300; ++t) {
        float force = t % 2 == 0 ? 1e6f : -1e6f;
        for (size_t p = 0; p < sizeof points / sizeof points[0]; ++p) {
            fluid_splat(f, points[p][0], points[p][1], 5, force, -force, (float[]){9, -2, 4, 0});
        }
        fluid_step(f, k_dt);
        TEST_ASSERT_TRUE(fluid_validate_finite(f));
        for (int i = 0; i < FLUID_N; ++i) {
            TEST_ASSERT_TRUE(fabsf(f->u[i]) <= f->cfg.max_vel && fabsf(f->v[i]) <= f->cfg.max_vel);
            for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
                TEST_ASSERT_TRUE(f->dye[ch][i] >= 0 && f->dye[ch][i] <= f->cfg.max_dye);
            }
        }
    }
}

typedef struct {
    double mean;
    float peak;
} Divergence;

static Divergence divergence(const FluidState *state)
{
    Divergence result = {0};
    int fluid_cells = 0;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = cell(x, y);
            if (state->solid[i]) {
                continue;
            }
            float d = 0.5f * fabsf(state->u[i + 1] - state->u[i - 1] + state->v[i + FLUID_W] -
                                   state->v[i - FLUID_W]);
            ++fluid_cells;
            result.mean += d;
            result.peak = fmaxf(result.peak, d);
        }
    }
    result.mean /= fluid_cells;
    return result;
}

static void test_projection_reduces_divergence(void)
{
    fluid_add_radial_impulse(f, 640, 360, 8, 100);
    Divergence before = divergence(f);
    TEST_ASSERT_TRUE(before.mean > 0 && before.peak > 0);
    fluid_step(f, k_dt);
    Divergence after = divergence(f);
    TEST_ASSERT_TRUE(after.mean <= before.mean * 0.5);
    TEST_ASSERT_TRUE(after.peak <= before.peak * 0.25f);
}

static void assert_walls_are_empty(void)
{
    for (int i = 0; i < FLUID_N; ++i) {
        if (f->solid[i]) {
            TEST_ASSERT_TRUE(f->u[i] == 0 && f->v[i] == 0);
            for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
                TEST_ASSERT_TRUE(f->dye[ch][i] == 0);
            }
        }
    }
}

static void test_solids_and_normals(void)
{
    const float dye[] = {1, 1, 1, 0};
    fluid_splat(f, 644, 324, 6, 15, -20, dye);
    f->dye[3][cell(80, 40)] = 2;
    fluid_set_solid_rect(f, 640, 320, 16, 16, true);
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            TEST_ASSERT_EQUAL_UINT8(x >= 80 && x <= 81 && y >= 40 && y <= 41, f->solid[cell(x, y)]);
        }
    }
    fluid_splat(f, 804, 404, 6, -30, 25, dye);
    fluid_set_solid_circle(f, 804, 404, 16, true);
    TEST_ASSERT_TRUE(fluid_is_solid(f, 820, 404));
    TEST_ASSERT_FALSE(fluid_is_solid(f, 820, 412));
    assert_walls_are_empty();
    fluid_splat(f, 644, 324, 6, 15, -20, dye);
    fluid_splat(f, 804, 404, 6, 15, -20, dye);
    assert_walls_are_empty();
    fluid_step(f, k_dt);
    assert_walls_are_empty();
    TEST_ASSERT_TRUE(fluid_is_solid(f, 644, 324));
    TEST_ASSERT_FALSE(fluid_is_solid(f, 636, 324));
    TEST_ASSERT_TRUE(fluid_is_solid(f, -1, 320));
    TEST_ASSERT_TRUE(fluid_is_solid(f, 640, 720));
    TEST_ASSERT_TRUE(fluid_is_solid(f, 640, NAN));

    const struct {
        float x, y, dx, dy, dot_min;
    } normals[] = {{636, 324, -1, 0, 0.7f},
                   {660, 332, 1, 0, 0.7f},
                   {644, 316, 0, -1, 0.7f},
                   {652, 340, 0, 1, 0.7f},
                   {636, 316, -0.707f, -0.707f, 0.99f},
                   {644, 324, -0.707f, -0.707f, 0.99f},
                   {12, 364, 1, 0, 0.99f},
                   {4, 364, 1, 0, 0.99f},
                   {12, 12, 0.707f, 0.707f, 0.99f}};
    float nx, ny;
    for (size_t i = 0; i < sizeof normals / sizeof normals[0]; ++i) {
        TEST_ASSERT_TRUE(fluid_solid_normal(f, normals[i].x, normals[i].y, &nx, &ny));
        TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, hypotf(nx, ny));
        TEST_ASSERT_TRUE(nx * normals[i].dx + ny * normals[i].dy >= normals[i].dot_min);
    }
    TEST_ASSERT_FALSE(fluid_solid_normal(f, 320, 200, &nx, &ny));
    TEST_ASSERT_TRUE(nx == 0 && ny == 0);
    fluid_set_solid_rect(f, 0, 0, 1280, 720, false);
    for (int y = 0; y < FLUID_H; ++y) {
        for (int x = 0; x < FLUID_W; ++x) {
            TEST_ASSERT_EQUAL_UINT8(outer(x, y), f->solid[cell(x, y)]);
        }
    }
}

static void set_uniform_velocity(float u, float v)
{
    for (int i = 0; i < FLUID_N; ++i) {
        if (!f->solid[i]) {
            f->u[i] = u;
            f->v[i] = v;
        }
    }
}

static void test_advection_stops_at_walls_and_box_corners(void)
{
    FluidConfig cfg = fluid_config_default();
    cfg.project_iters = 0;
    cfg.vorticity_eps = 0;
    fluid_init(f, &cfg);
    fluid_set_solid_rect(f, 640, 0, 16, 720, true);
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x <= 79; ++x) {
            f->dye[0][cell(x, y)] = 2;
        }
    }
    for (int t = 0; t < 10; ++t) {
        set_uniform_velocity(125, 0);
        fluid_step(f, 0.05f);
        for (int y = 0; y < FLUID_H; ++y) {
            for (int x = 82; x < FLUID_W; ++x) {
                TEST_ASSERT_TRUE(f->dye[0][cell(x, y)] == 0);
            }
        }
    }

    fluid_init(f, &cfg);
    fluid_set_solid_rect(f, 480, 240, 160, 16, true);
    fluid_set_solid_rect(f, 480, 384, 160, 16, true);
    fluid_set_solid_rect(f, 480, 240, 16, 160, true);
    fluid_set_solid_rect(f, 624, 240, 16, 160, true);
    for (int sign = -1; sign <= 1; sign += 2) {
        fluid_reset(f);
        for (int y = 1; y < FLUID_H - 1; ++y) {
            for (int x = 1; x < FLUID_W - 1; ++x) {
                if (x < 60 || x > 79 || y < 30 || y > 49) {
                    f->dye[0][cell(x, y)] = 2;
                }
            }
        }
        for (int t = 0; t < 10; ++t) {
            set_uniform_velocity(sign * 125, sign * 125);
            fluid_step(f, 0.05f);
            for (int y = 32; y <= 47; ++y) {
                for (int x = 62; x <= 77; ++x) {
                    TEST_ASSERT_TRUE(f->dye[0][cell(x, y)] == 0);
                }
            }
        }
    }
}

static void test_injection_directions(void)
{
    const int center = cell(80, 45);
    const float dye[] = {1, 0, 0, 0};
    fluid_set_solid_rect(f, 640, 368, 8, 8, true);
    fluid_splat(f, 644, 364, 4, 10, 0, dye);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 10, f->u[center]);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 1, f->dye[0][center]);
    fluid_reset(f);
    fluid_swirl(f, 644, 364, 4, 10, dye);
    TEST_ASSERT_TRUE(f->v[center + 1] > 0);
    assert_walls_are_empty();
    fluid_reset(f);
    fluid_add_radial_impulse(f, 644, 364, 4, 1e6f);
    TEST_ASSERT_TRUE(f->u[center + 1] > 0 && f->u[center - 1] < 0);
    for (int i = 0; i < FLUID_N; ++i) {
        TEST_ASSERT_TRUE(fabsf(f->u[i]) <= f->cfg.max_vel && fabsf(f->v[i]) <= f->cfg.max_vel);
    }
    assert_walls_are_empty();
}

static void assert_null_state_contract(void)
{
    fluid_init(NULL, NULL);
    fluid_reset(NULL);
    fluid_clear_solids(NULL);
    fluid_set_solid_rect(NULL, 100, 100, 8, 8, true);
    fluid_set_solid_circle(NULL, 100, 100, 8, true);
    TEST_ASSERT_FALSE(fluid_is_solid(NULL, 100, 100));
    TEST_ASSERT_FALSE(fluid_solid_normal(NULL, 100, 100, NULL, NULL));
    fluid_step(NULL, k_dt);
    fluid_splat(NULL, 100, 100, 2, 1, 1, NULL);
    fluid_swirl(NULL, 100, 100, 2, 1, NULL);
    fluid_add_radial_impulse(NULL, 100, 100, 2, 1);
    fluid_sample(NULL, 100, 100, NULL, NULL, NULL);
    TEST_ASSERT_NULL(fluid_channel_const(NULL, 0));
    TEST_ASSERT_NULL(fluid_channel_mut(NULL, 0));
    fluid_sanitize_channels(NULL);
    TEST_ASSERT_FALSE(fluid_validate_finite(NULL));
    TEST_ASSERT_EQUAL_UINT64(0, fluid_hash(NULL));
}

static void test_invalid_inputs_and_optional_outputs(void)
{
    assert_null_state_contract();
    fluid_splat(f, 640, 360, 3, 2, 7, NULL);
    fluid_step(f, k_dt);
    uint64_t hash = fluid_hash(f);
    double solve_ms = f->stats.solve_ms;
    const float invalid[] = {NAN, INFINITY, -INFINITY};
    for (size_t n = 0; n < sizeof invalid / sizeof invalid[0]; ++n) {
        for (int arg = 0; arg < 5; ++arg) {
            float p[] = {640, 360, 24, 24, 15};
            p[arg] = invalid[n];
            fluid_splat(f, p[0], p[1], p[2], p[3], p[4], NULL);
            TEST_ASSERT_EQUAL_HEX64(hash, fluid_hash(f));
            if (arg < 4) {
                fluid_swirl(f, p[0], p[1], p[2], p[3], NULL);
                fluid_add_radial_impulse(f, p[0], p[1], p[2], p[3]);
                fluid_set_solid_rect(f, p[0], p[1], p[2], p[3], true);
                TEST_ASSERT_EQUAL_HEX64(hash, fluid_hash(f));
            }
            if (arg < 3) {
                fluid_set_solid_circle(f, p[0], p[1], p[2], true);
                float dye[] = {1, 2, 1, 0};
                dye[arg] = invalid[n];
                fluid_splat(f, 640, 360, 5, 1, 1, dye);
                fluid_swirl(f, 640, 360, 5, 1, dye);
                TEST_ASSERT_EQUAL_HEX64(hash, fluid_hash(f));
            }
        }
        float u = 1, v = 1, dye[] = {1, 1, 1, 1};
        fluid_sample(f, invalid[n], 300, &u, &v, dye);
        TEST_ASSERT_TRUE(u == 0 && v == 0);
        for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
            TEST_ASSERT_TRUE(dye[ch] == 0);
        }
        float nx = 1, ny = 1;
        TEST_ASSERT_FALSE(fluid_solid_normal(f, 200, invalid[n], &nx, &ny));
        TEST_ASSERT_TRUE(nx == 0 && ny == 0);
    }
    for (int bad = -1; bad <= 0; ++bad) {
        fluid_splat(f, 640, 360, bad, 2, 3, NULL);
        fluid_swirl(f, 640, 360, bad, 4, NULL);
        fluid_add_radial_impulse(f, 640, 360, bad, 5);
        fluid_set_solid_circle(f, 640, 360, bad, true);
        fluid_set_solid_rect(f, 640, 360, bad, 16, true);
        fluid_set_solid_rect(f, 640, 360, 16, bad, true);
        TEST_ASSERT_EQUAL_HEX64(hash, fluid_hash(f));
    }
    const float bad_dt[] = {0, -1, NAN, INFINITY, -INFINITY};
    for (size_t i = 0; i < sizeof bad_dt / sizeof bad_dt[0]; ++i) {
        fluid_step(f, bad_dt[i]);
        TEST_ASSERT_EQUAL_UINT64(1, f->steps);
        TEST_ASSERT_TRUE(solve_ms == f->stats.solve_ms);
        TEST_ASSERT_EQUAL_HEX64(hash, fluid_hash(f));
    }
    TEST_ASSERT_NULL(fluid_channel_const(f, -1));
    TEST_ASSERT_NULL(fluid_channel_mut(f, -1));
    TEST_ASSERT_NULL(fluid_channel_const(f, 4));
    TEST_ASSERT_NULL(fluid_channel_mut(f, 4));

    for (int sign = -1; sign <= 1; sign += 2) {
        float far = sign * 1e30f;
        fluid_splat(f, far, far, 1e30f, 2, 3, NULL);
        fluid_swirl(f, far, far, 1e30f, 4, NULL);
        fluid_add_radial_impulse(f, far, far, 1e30f, 5);
        fluid_set_solid_rect(f, far, far, 16, 16, true);
        fluid_set_solid_circle(f, far, far, 24, true);
        TEST_ASSERT_EQUAL_HEX64(hash, fluid_hash(f));
        TEST_ASSERT_TRUE(fluid_is_solid(f, far, far));
        fluid_solid_normal(f, far, far, NULL, NULL);
        fluid_sample(f, far, far, NULL, NULL, NULL);
    }
    // The rightmost point of this circle is at x=0, even at a huge radius.
    fluid_set_solid_circle(f, -1e30f, 360, 1e30f, true);
    TEST_ASSERT_EQUAL_HEX64(hash, fluid_hash(f));
    fluid_splat(f, 640, 360, 1e30f, 2, 3, NULL);
    fluid_swirl(f, 640, 360, 1e30f, 4, NULL);
    fluid_add_radial_impulse(f, 640, 360, 1e30f, 5);
    TEST_ASSERT_TRUE(fluid_validate_finite(f));
    float out;
    fluid_sample(f, 640, 360, &out, NULL, NULL);
    TEST_ASSERT_TRUE(isfinite(out));
    fluid_sample(f, 640, 360, NULL, &out, NULL);
    TEST_ASSERT_TRUE(isfinite(out));
    TEST_ASSERT_TRUE(fluid_solid_normal(f, 12, 364, &out, NULL));
    TEST_ASSERT_TRUE(fluid_solid_normal(f, 12, 364, NULL, &out));
}

static void test_mutable_channels_are_sanitized(void)
{
    int i = cell(80, 45);
    for (int ch = f->cfg.active_channels; ch < FLUID_MAX_CHANNELS; ++ch) {
        TEST_ASSERT_NULL(fluid_channel_mut(f, ch));
        TEST_ASSERT_NOT_NULL(fluid_channel_const(f, ch));
    }
    for (int ch = 0; ch < f->cfg.active_channels; ++ch) {
        float *dye = fluid_channel_mut(f, ch);
        TEST_ASSERT_NOT_NULL(dye);
        TEST_ASSERT_EQUAL_PTR(dye, fluid_channel_const(f, ch));
        dye[i] = NAN;
        dye[i + 1] = -1;
        dye[i + 2] = 99;
        dye[0] = 1;
        TEST_ASSERT_FALSE(fluid_validate_finite(f));
        fluid_sanitize_channels(f);
        TEST_ASSERT_TRUE(fluid_validate_finite(f));
        TEST_ASSERT_TRUE(dye[i] == 0 && dye[i + 1] == 0 && dye[i + 2] == f->cfg.max_dye &&
                         dye[0] == 0);
    }
}

static void test_read_only_field_accessors_alias_state(void)
{
    TEST_ASSERT_EQUAL_PTR(f->u, fluid_u_const(f));
    TEST_ASSERT_EQUAL_PTR(f->v, fluid_v_const(f));
    TEST_ASSERT_EQUAL_PTR(f->solid, fluid_solid_const(f));
    TEST_ASSERT_NULL(fluid_u_const(NULL));
    TEST_ASSERT_NULL(fluid_v_const(NULL));
    TEST_ASSERT_NULL(fluid_solid_const(NULL));
    TEST_ASSERT_NULL(fluid_channel_mut(NULL, 0));
    TEST_ASSERT_NULL(fluid_channel_const(f, FLUID_MAX_CHANNELS));
}

static void test_cell_size_follows_screen_and_grid(void)
{
    TEST_ASSERT_EQUAL_FLOAT((float)INK_SCREEN_W / (float)FLUID_W, FLUID_CELL_PX);
    TEST_ASSERT_EQUAL_FLOAT((float)INK_SCREEN_H / (float)FLUID_H, FLUID_CELL_PX);
    TEST_ASSERT_EQUAL_FLOAT(8.0f, FLUID_CELL_PX);
}

static void test_cell_reach_covers_pixel_radius(void)
{
    TEST_ASSERT_EQUAL_INT(25, fluid_cell_reach(195.0f));
    TEST_ASSERT_EQUAL_INT(8, fluid_cell_reach(64.0f));
    TEST_ASSERT_EQUAL_INT(4, fluid_cell_reach(28.0f));
    TEST_ASSERT_EQUAL_INT(1, fluid_cell_reach(0.01f));
    TEST_ASSERT_EQUAL_INT(0, fluid_cell_reach(0.0f));
    TEST_ASSERT_EQUAL_INT(0, fluid_cell_reach(-5.0f));
    TEST_ASSERT_EQUAL_INT(0, fluid_cell_reach(NAN));
    TEST_ASSERT_EQUAL_INT(FLUID_W, fluid_cell_reach(INFINITY));
    TEST_ASSERT_EQUAL_INT(FLUID_W, fluid_cell_reach(1e30f));
}

static void test_is_solid_at_far_domain_edge_stays_in_grid(void)
{
    const float right = nextafterf((float)FLUID_W * FLUID_CELL_PX, 0.0f);
    const float bottom = nextafterf((float)FLUID_H * FLUID_CELL_PX, 0.0f);
    TEST_ASSERT_TRUE(fluid_is_solid(f, right, 360.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(f, 640.0f, bottom));
    TEST_ASSERT_TRUE(fluid_is_solid(f, right, bottom));
    TEST_ASSERT_FALSE(fluid_is_solid(f, right - FLUID_CELL_PX, 360.0f));
}

static void test_sampling_units_and_channels(void)
{
    set_uniform_velocity(3, 0);
    int i = cell(80, 45);
    f->dye[0][i] = 2;
    f->dye[3][i] = 2;
    float u, v, dye[] = {-1, -1, -1, -1};
    fluid_sample(f, 640, 360, &u, &v, dye);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 3, u);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0, v);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.5f, dye[0]);
    TEST_ASSERT_TRUE(dye[1] == 0 && dye[2] == 0 && dye[3] == 0);
    // The wall contributes a zero value to queries at the domain edge.
    fluid_sample(f, -20, 364, &u, NULL, NULL);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 1.5f, u);
    fluid_step(f, k_dt);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 2, f->dye[3][i]);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0, f->stats.max_dye[3]);

    FluidConfig cfg = fluid_config_default();
    cfg.project_iters = 0;
    cfg.vorticity_eps = 0;
    cfg.vel_damp = 1;
    cfg.dye_damp[0] = 0.5f;
    cfg.max_dye = FLT_MAX;
    fluid_init(f, &cfg);
    set_uniform_velocity(30, 0);
    f->dye[0][i] = 2;
    fluid_step(f, k_dt);
    // A half-cell translation followed by 0.5 decay splits the pulse evenly.
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, f->dye[0][i]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, f->dye[0][i + 1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, f->stats.max_dye[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 30, f->stats.max_vel);

    for (int y = 1; y <= 2; ++y) {
        for (int x = 1; x <= 2; ++x) {
            f->dye[0][cell(x, y)] = FLT_MAX;
        }
    }
    // Individually rounded bilinear weights can sum to slightly more than one.
    fluid_sample(f, 12.3389826f, 15.8490562f, NULL, NULL, dye);
    TEST_ASSERT_TRUE(isfinite(dye[0]) && dye[0] > 0.99f * FLT_MAX);
}

static void place_obstacles(FluidState *state)
{
    for (int side = 0; side < 2; ++side) {
        for (int row = 0; row < 2; ++row) {
            fluid_set_solid_rect(state, 372 + side * 520, 320 + row * 64, 16, 16, true);
            fluid_set_solid_circle(state, 340 + side * 600, 190 + row * 340, 24, true);
        }
    }
}

static void apply_tick_input(FluidState *state, int t)
{
    float a = 0.05f * t;
    fluid_splat(state, 640 + 380 * cosf(a), 360 + 220 * sinf(1.3f * a), 4.5f, -110 * sinf(a),
                90 * cosf(1.3f * a), (float[]){1.2f, 0, 0, 0});
    if (t % 7 == 0) {
        fluid_splat(state, 640 + 300 * sinf(2 * a), 200, 3, 0, 60, (float[]){0.4f, 0.8f, 0, 0});
    }
    if (t % 20 == 0) {
        fluid_swirl(state, 640 + 200 * cosf(0.7f * a), 360, 9, 70, (float[]){0.3f, 0, 0.7f, 0});
    }
    if (t % 45 == 0) {
        fluid_add_radial_impulse(state, 640 - 250 * cosf(a), 360 + 150 * sinf(a), 10, 110);
    }
}

static void test_deterministic_replay_and_hash_fields(void)
{
    replay = calloc(1, sizeof *replay);
    TEST_ASSERT_NOT_NULL(replay);
    fluid_init(replay, NULL);
    place_obstacles(f);
    place_obstacles(replay);
    for (int t = 0; t < 120; ++t) {
        apply_tick_input(f, t);
        apply_tick_input(replay, t);
        fluid_step(f, k_dt);
        fluid_step(replay, k_dt);
        uint64_t expected = fluid_hash(f);
        uint64_t actual = fluid_hash(replay);
        if (expected != actual) {
            char message[64];
            snprintf(message, sizeof message, "Replay first diverged after tick %d", t);
            TEST_ASSERT_EQUAL_HEX64_MESSAGE(expected, actual, message);
        }
    }
    uint64_t hash = fluid_hash(f);
    f->stats.solve_ms += 100;
    f->u0[0] = 1;
    f->dye[3][0] = -0.0f;
    TEST_ASSERT_EQUAL_HEX64(hash, fluid_hash(f));
    int i = cell(80, 45);
    f->dye[3][i] = 1;
    TEST_ASSERT_TRUE(hash != fluid_hash(f));
    f->dye[3][i] = 0;
    f->u[i] += 1e-3f;
    TEST_ASSERT_TRUE(hash != fluid_hash(f));
}

static void test_geometry_cache_invalidation_on_solid_changes(void)
{
    FluidConfig cfg = fluid_config_default();
    cfg.project_iters = 0;
    cfg.vorticity_eps = 0;
    fluid_init(f, &cfg);

    /* 1. Add a vertical wall and step once to populate cached geometry. */
    fluid_set_solid_rect(f, 640, 0, 16, 720, true);
    TEST_ASSERT_EQUAL_UINT8(0, f->geometry_valid);
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x <= 79; ++x) {
            f->dye[0][cell(x, y)] = 2.0f;
        }
    }
    set_uniform_velocity(125.0f, 0.0f);
    fluid_step(f, 0.05f);
    TEST_ASSERT_EQUAL_UINT8(1, f->geometry_valid);
    for (int y = 1; y < FLUID_H - 1; ++y) {
        TEST_ASSERT_TRUE(f->dye[0][cell(82, y)] == 0.0f);
    }

    /* 2. Remove the wall via fluid_set_solid_rect(..., false) and verify immediate passage. */
    fluid_set_solid_rect(f, 640, 0, 16, 720, false);
    TEST_ASSERT_EQUAL_UINT8(0, f->geometry_valid);
    for (int t = 0; t < 3; ++t) {
        set_uniform_velocity(125.0f, 0.0f);
        fluid_step(f, 0.05f);
    }
    TEST_ASSERT_EQUAL_UINT8(1, f->geometry_valid);
    TEST_ASSERT_TRUE(f->dye[0][cell(82, 45)] > 0.1f);

    /* 3. Add a circular solid obstacle and verify interior remains blocked after step. */
    fluid_set_solid_circle(f, 804, 364, 24, true);
    TEST_ASSERT_EQUAL_UINT8(0, f->geometry_valid);
    for (int t = 0; t < 3; ++t) {
        set_uniform_velocity(125.0f, 0.0f);
        fluid_step(f, 0.05f);
    }
    TEST_ASSERT_EQUAL_UINT8(1, f->geometry_valid);
    assert_walls_are_empty();
    TEST_ASSERT_TRUE(fluid_is_solid(f, 804, 364));

    /* 4. Clear solids via fluid_clear_solids and verify dye crosses the former obstacle center. */
    fluid_clear_solids(f);
    TEST_ASSERT_EQUAL_UINT8(0, f->geometry_valid);
    for (int y = 40; y <= 50; ++y) {
        for (int x = 90; x <= 97; ++x) {
            f->dye[0][cell(x, y)] = 2.0f;
        }
    }
    for (int t = 0; t < 3; ++t) {
        set_uniform_velocity(125.0f, 0.0f);
        fluid_step(f, 0.05f);
    }
    TEST_ASSERT_EQUAL_UINT8(1, f->geometry_valid);
    TEST_ASSERT_FALSE(fluid_is_solid(f, 804, 364));
    TEST_ASSERT_TRUE(f->dye[0][cell(100, 45)] > 0.1f);

    /* 5. Pressure-enabled (project_iters = 12) + short-backtrace differential check:
     * compare dynamically mutated solid caches against freshly rebuilt caches bit-for-bit. */
    replay = calloc(1, sizeof *replay);
    TEST_ASSERT_NOT_NULL(replay);

    fluid_init(f, NULL);
    fluid_set_solid_rect(f, 520, 280, 32, 160, true);
    fluid_splat(f, 480, 360, 4.0f, 18.0f, 4.0f, (float[]){1.2f, 0.4f, 0.0f, 0.0f});
    fluid_step(f, k_dt);
    TEST_ASSERT_EQUAL_UINT8(1, f->geometry_valid);

    /* Mutate solids dynamically: remove rect, add circle, then reset fluid fields only. */
    fluid_set_solid_rect(f, 520, 280, 32, 160, false);
    TEST_ASSERT_EQUAL_UINT8(0, f->geometry_valid);
    fluid_set_solid_circle(f, 640, 360, 24, true);
    TEST_ASSERT_EQUAL_UINT8(0, f->geometry_valid);
    fluid_reset(f);

    /* Fresh instance with the exact same final obstacle and never-mutated cache. */
    fluid_init(replay, NULL);
    fluid_set_solid_circle(replay, 640, 360, 24, true);

    for (int t = 0; t < 10; ++t) {
        /* Short backtrace (speed <= 20 cells/s -> dt*speed <= 0.34 <= 2) exercises !near_wall. */
        fluid_splat(f, 592, 360, 4.0f, 18.0f, 3.0f, (float[]){0.9f, 0.3f, 0.2f, 0.0f});
        fluid_splat(replay, 592, 360, 4.0f, 18.0f, 3.0f, (float[]){0.9f, 0.3f, 0.2f, 0.0f});
        fluid_step(f, k_dt);
        fluid_step(replay, k_dt);
        TEST_ASSERT_EQUAL_HEX64(fluid_hash(replay), fluid_hash(f));
    }

    /* Now clear solids on mutated instance and compare against fresh obstacle-free state. */
    fluid_clear_solids(f);
    TEST_ASSERT_EQUAL_UINT8(0, f->geometry_valid);
    fluid_reset(f);
    fluid_init(replay, NULL);
    for (int t = 0; t < 10; ++t) {
        fluid_splat(f, 608, 360, 4.0f, 18.0f, -2.0f, (float[]){0.8f, 0.0f, 0.5f, 0.0f});
        fluid_splat(replay, 608, 360, 4.0f, 18.0f, -2.0f, (float[]){0.8f, 0.0f, 0.5f, 0.0f});
        fluid_step(f, k_dt);
        fluid_step(replay, k_dt);
        TEST_ASSERT_EQUAL_HEX64(fluid_hash(replay), fluid_hash(f));
    }
}

static void test_extreme_flt_max_config_stays_finite(void)
{
    FluidConfig cfg = fluid_config_default();
    cfg.max_vel = FLT_MAX;
    cfg.max_dye = FLT_MAX;
    cfg.project_iters = 12;
    cfg.vorticity_eps = 9.5f;
    fluid_init(f, &cfg);
    fluid_set_solid_rect(f, 640, 320, 16, 48, true);

    for (int y = 40; y <= 46; ++y) {
        for (int x = 74; x <= 78; ++x) {
            int i = cell(x, y);
            f->u[i] = (x % 2 == 0) ? 0.5f * FLT_MAX : -0.5f * FLT_MAX;
            f->v[i] = (y % 2 == 0) ? 0.5f * FLT_MAX : -0.5f * FLT_MAX;
            f->dye[0][i] = FLT_MAX;
            f->dye[1][i] = 0.75f * FLT_MAX;
            f->dye[2][i] = 0.5f * FLT_MAX;
        }
    }
    for (int t = 0; t < 5; ++t) {
        fluid_step(f, k_dt);
        TEST_ASSERT_TRUE(fluid_validate_finite(f));
    }
    float su = 0.0f, sv = 0.0f, sdye[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(f, 610.0f, 344.0f, &su, &sv, sdye);
    TEST_ASSERT_TRUE(isfinite(su) && isfinite(sv));
    for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
        TEST_ASSERT_TRUE(isfinite(sdye[ch]));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_and_reset);
    RUN_TEST(test_continuous_strong_inputs_stay_bounded);
    RUN_TEST(test_projection_reduces_divergence);
    RUN_TEST(test_solids_and_normals);
    RUN_TEST(test_advection_stops_at_walls_and_box_corners);
    RUN_TEST(test_injection_directions);
    RUN_TEST(test_invalid_inputs_and_optional_outputs);
    RUN_TEST(test_mutable_channels_are_sanitized);
    RUN_TEST(test_read_only_field_accessors_alias_state);
    RUN_TEST(test_cell_size_follows_screen_and_grid);
    RUN_TEST(test_cell_reach_covers_pixel_radius);
    RUN_TEST(test_is_solid_at_far_domain_edge_stays_in_grid);
    RUN_TEST(test_sampling_units_and_channels);
    RUN_TEST(test_deterministic_replay_and_hash_fields);
    RUN_TEST(test_geometry_cache_invalidation_on_solid_changes);
    RUN_TEST(test_extreme_flt_max_config_stays_finite);
    return UNITY_END();
}
