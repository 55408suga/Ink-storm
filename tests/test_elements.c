#include "elements/elements.h"
#include "fluid.h"
#include "game.h"
#include "roguelike/upgrades.h"
#include "unity.h"

#include <math.h>
#include <stddef.h>

void setUp(void) {}

void tearDown(void) {}

static void clear_all_entities(GameState *g)
{
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX) {
            ecs_destroy(&g->ecs, id);
        }
    }
    ecs_flush_destroy(&g->ecs);
    g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};
}

static int cell_idx(int x, int y) { return y * FLUID_W + x; }

static float cell_center_px(int c) { return ((float)c + 0.5f) * FLUID_CELL_PX; }

static int count_events(const GameState *g, GameEventType type)
{
    int count = 0;
    for (int32_t i = 0; i < g->event_count; ++i) {
        if (g->events[i].type == type) {
            ++count;
        }
    }
    return count;
}

static void test_blazing_inkstorm_cell_reaction(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    float *ink = fluid_channel_mut(&g->fluid, 0);
    float *fire = fluid_channel_mut(&g->fluid, 1);
    const int i_react = cell_idx(40, 40);
    const int i_below = cell_idx(45, 40);
    ink[i_react] = 0.30f;
    fire[i_react] = 0.20f;
    ink[i_below] = 0.30f;
    fire[i_below] = 0.13f;

    elements_step(g);

    const float expected_burn = 0.20f * 0.030f;
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.20f + expected_burn * 1.15f, fire[i_react]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.30f - expected_burn * 0.14f, ink[i_react]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.13f, fire[i_below]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.30f, ink[i_below]);

    /* Wildfire Lv1 multiplies burn rate by 1.25. */
    fluid_reset(&g->fluid);
    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_WILDFIRE));
    ink[i_react] = 0.30f;
    fire[i_react] = 0.20f;
    elements_step(g);
    const float burn_wf = 0.20f * 0.030f * 1.25f;
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.20f + burn_wf * 1.15f, fire[i_react]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.30f - burn_wf * 0.14f, ink[i_react]);

    /* Curl boosts burn, capped at 1 + 2.4 = 3.4. */
    fluid_reset(&g->fluid);
    const int i_vort = cell_idx(60, 40);
    const int i_cap = cell_idx(80, 40);
    ink[i_vort] = 0.30f;
    fire[i_vort] = 0.20f;
    g->fluid.v[i_vort + 1] = 4.0f;
    g->fluid.v[i_vort - 1] = -4.0f; /* curl = 0.5 * (8 - 0) = 4.0 -> boost = 1 + 1.12 = 2.12 */

    ink[i_cap] = 0.30f;
    fire[i_cap] = 0.20f;
    g->fluid.v[i_cap + 1] = 50.0f;
    g->fluid.v[i_cap - 1] = -50.0f; /* curl = 50 -> capped at 1 + 2.4 = 3.4 */

    elements_step(g);
    const float burn_vort = 0.20f * 0.030f * (1.0f + 4.0f * 0.28f) * 1.25f;
    const float burn_cap = 0.20f * 0.030f * 3.4f * 1.25f;
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.20f + burn_vort * 1.15f, fire[i_vort]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.20f + burn_cap * 1.15f, fire[i_cap]);
    TEST_ASSERT_TRUE(fire[i_vort] > 0.20f + burn_wf * 1.15f);

    game_destroy(g);
}

static void test_ignite_callout_threshold_and_cooldown(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    float *fire = fluid_channel_mut(&g->fluid, 1);
    for (int k = 0; k < 23; ++k) {
        fire[cell_idx(20 + k, 20)] = 0.40f;
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_IGNITE));
    TEST_ASSERT_EQUAL_INT32(0, g->run.ignitions);

    fire[cell_idx(20 + 23, 20)] = 0.40f;
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_IGNITE));
    TEST_ASSERT_EQUAL_INT32(1, g->run.ignitions);

    /* Next 191 ticks (< 3.2s) must not emit EV_IGNITE again. */
    for (int t = 0; t < 191; ++t) {
        g->event_count = 0;
        elements_step(g);
        TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_IGNITE));
    }
    TEST_ASSERT_EQUAL_INT32(1, g->run.ignitions);

    /* 192nd tick (== 3.2s) allows EV_IGNITE again. */
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_IGNITE));
    TEST_ASSERT_EQUAL_INT32(2, g->run.ignitions);

    game_destroy(g);
}

static void test_storm_growth_on_ink(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    float *ink = fluid_channel_mut(&g->fluid, 0);
    float *storm = fluid_channel_mut(&g->fluid, 2);
    const int i_grow = cell_idx(40, 40);
    const int i_low = cell_idx(45, 40);
    storm[i_grow] = 0.15f;
    ink[i_grow] = 0.25f;
    storm[i_low] = 0.15f;
    ink[i_low] = 0.23f;

    elements_step(g);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.15f + 0.15f * 0.015f, storm[i_grow]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.15f, storm[i_low]);

    game_destroy(g);
}

static void test_tempest_lightning_chain(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    const EntityId e0 =
        game_debug_spawn_enemy(g, ENEMY_UMBRELLA, cell_center_px(40), cell_center_px(40) + 120.0f);
    const EntityId e1 = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, cell_center_px(40) + 120.0f,
                                               cell_center_px(40) + 120.0f);
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, e0.index);
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, e1.index);
    /* Reset fluid after spawning so spawn ink splatters do not mark test targets. */
    fluid_reset(&g->fluid);
    float *storm = fluid_channel_mut(&g->fluid, 2);
    for (int k = 0; k < 9; ++k) {
        storm[cell_idx(40 + k, 40)] = 0.50f;
    }
    game_status_mut(g, e0)->ink_mark_t = 3.0f;
    game_status_mut(g, e1)->charge_t = 2.0f;
    game_health_mut(g, e0)->hp = 100;
    game_health_mut(g, e1)->hp = 100;

    /* 9 storm cells: must not trigger. */
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_STORM_ZAP));
    TEST_ASSERT_EQUAL_INT32(100, game_health(g, e0)->hp);

    /* 10th storm cell: triggers hop 0 (20 dmg) + hop 1 (7 dmg), each enemy hit at most once. */
    storm[cell_idx(49, 40)] = 0.50f;
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(2, count_events(g, EV_STORM_ZAP));
    TEST_ASSERT_EQUAL_INT32(0, g->events[0].value);
    TEST_ASSERT_EQUAL_INT32(1, g->events[1].value);
    TEST_ASSERT_EQUAL_INT32(80, game_health(g, e0)->hp);
    TEST_ASSERT_EQUAL_INT32(93, game_health(g, e1)->hp);
    TEST_ASSERT_EQUAL_INT32(1, g->run.storm_zaps);

    /* Cooldown 0.54s (32 ticks < 0.54s must not zap; 33rd tick zaps). */
    for (int t = 0; t < 32; ++t) {
        g->event_count = 0;
        elements_step(g);
        TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_STORM_ZAP));
    }

    /* Add 2 more enemies and Chain Lightning Lv1 -> hops increase from 2 to 3 (4 zaps total). */
    const EntityId e2 = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, cell_center_px(40) + 240.0f,
                                               cell_center_px(40) + 120.0f);
    const EntityId e3 = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, cell_center_px(40) + 360.0f,
                                               cell_center_px(40) + 120.0f);
    game_status_mut(g, e0)->ink_mark_t = 3.0f;
    game_status_mut(g, e1)->charge_t = 2.0f;
    game_status_mut(g, e2)->charge_t = 2.0f;
    game_status_mut(g, e3)->charge_t = 2.0f;
    game_health_mut(g, e0)->hp = 100;
    game_health_mut(g, e1)->hp = 100;
    game_health_mut(g, e2)->hp = 100;
    game_health_mut(g, e3)->hp = 100;
    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_CHAIN_LIGHTNING));

    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(4, count_events(g, EV_STORM_ZAP));
    TEST_ASSERT_EQUAL_INT32(3, g->events[3].value);
    TEST_ASSERT_EQUAL_INT32(2, g->run.storm_zaps);

    game_destroy(g);
}

static void test_status_application_and_burn_intervals(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    const EntityId e_mark =
        game_debug_spawn_enemy(g, ENEMY_UMBRELLA, cell_center_px(30), cell_center_px(30));
    const EntityId e_nomark =
        game_debug_spawn_enemy(g, ENEMY_UMBRELLA, cell_center_px(50), cell_center_px(30));
    const EntityId e_burn =
        game_debug_spawn_enemy(g, ENEMY_UMBRELLA, cell_center_px(70), cell_center_px(30));
    const EntityId boss =
        game_debug_spawn_enemy(g, ENEMY_BOSS_TENGU, cell_center_px(90), cell_center_px(30));

    /* Reset fluid after spawning so spawn ink splatters do not affect exact threshold checks. */
    fluid_reset(&g->fluid);
    float *ink = fluid_channel_mut(&g->fluid, 0);
    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);

    /* Fill 3x3 patches so bilinear sampling equals the exact value at cell center. */
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            ink[cell_idx(30 + dx, 30 + dy)] = 0.29f;
            storm[cell_idx(30 + dx, 30 + dy)] = 0.26f;
            ink[cell_idx(50 + dx, 30 + dy)] = 0.27f;
            fire[cell_idx(70 + dx, 30 + dy)] = 0.43f;
            fire[cell_idx(90 + dx, 30 + dy)] = 0.43f;
        }
    }

    elements_step(g);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 3.0f, game_status(g, e_mark)->ink_mark_t);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, game_status(g, e_mark)->charge_t);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, game_status(g, e_nomark)->ink_mark_t);

    /* 21 ticks total (0.35s): neither burned yet. */
    for (int t = 1; t < 21; ++t) {
        elements_step(g);
    }
    TEST_ASSERT_EQUAL_INT32(30, game_health(g, e_burn)->hp);
    TEST_ASSERT_EQUAL_INT32(400, game_health(g, boss)->hp);

    /* 22nd tick (0.3667s >= 0.36s): normal enemy takes 10 burn dmg, boss still 400. */
    elements_step(g);
    TEST_ASSERT_EQUAL_INT32(20, game_health(g, e_burn)->hp);
    TEST_ASSERT_EQUAL_INT32(400, game_health(g, boss)->hp);

    /* Advance to tick 32 (0.5333s < 0.55s): boss still 400. */
    for (int t = 22; t < 32; ++t) {
        elements_step(g);
    }
    TEST_ASSERT_EQUAL_INT32(400, game_health(g, boss)->hp);

    /* 33rd tick (0.55s): boss takes 10 burn dmg. */
    elements_step(g);
    TEST_ASSERT_EQUAL_INT32(390, game_health(g, boss)->hp);

    game_destroy(g);
}

static void test_projectile_elemental_upgrades(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            fire[cell_idx(40 + dx, 40 + dy)] = 0.37f;
            storm[cell_idx(60 + dx, 40 + dy)] = 0.26f;
            storm[cell_idx(80 + dx, 40 + dy)] = 0.26f;
        }
    }

    const EntityId p_burn = game_debug_spawn_projectile(g, PROJ_ARROW, cell_center_px(40),
                                                        cell_center_px(40), 200.0f, 0.0f);
    const EntityId p_elec = game_debug_spawn_projectile(g, PROJ_ARROW, cell_center_px(60),
                                                        cell_center_px(40), 200.0f, 0.0f);
    const EntityId p_accel = game_debug_spawn_projectile(g, PROJ_ARROW, cell_center_px(80),
                                                         cell_center_px(40), 200.0f, 0.0f);
    game_proj_mut(g, p_burn)->deflected = true;
    game_proj_mut(g, p_elec)->deflected = true;

    elements_step(g);

    TEST_ASSERT_TRUE(game_proj(g, p_burn)->burning);
    TEST_ASSERT_EQUAL_INT32(2, game_proj(g, p_burn)->pierce);
    TEST_ASSERT_TRUE(game_proj(g, p_elec)->electrified);
    TEST_ASSERT_TRUE(game_proj(g, p_accel)->accelerated);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 260.0f, game_velocity(g, p_accel)->vx);

    /* Second tick over storm must not accelerate again. */
    elements_step(g);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 260.0f, game_velocity(g, p_accel)->vx);

    game_destroy(g);
}

static void test_sanitize_and_finite(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    float *ink = fluid_channel_mut(&g->fluid, 0);
    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);

    const int i_nan = cell_idx(30, 30);
    const int i_neg = cell_idx(32, 30);
    const int i_big = cell_idx(34, 30);
    ink[i_nan] = NAN;
    fire[i_neg] = -1.0f;
    storm[i_big] = 99.0f;

    elements_step(g);
    fluid_sanitize_channels(&g->fluid);

    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.0f, ink[i_nan]);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.0f, fire[i_neg]);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, g->fluid.cfg.max_dye, storm[i_big]);
    TEST_ASSERT_TRUE(fluid_validate_finite(&g->fluid));

    game_destroy(g);
}

static void test_processing_order_ignite_before_storm_zap(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);
    for (int k = 0; k < 24; ++k) {
        fire[cell_idx(20 + k, 20)] = 0.40f;
    }
    for (int k = 0; k < 10; ++k) {
        storm[cell_idx(60 + k, 40)] = 0.50f;
    }

    const EntityId e =
        game_debug_spawn_enemy(g, ENEMY_UMBRELLA, cell_center_px(60), cell_center_px(40) + 80.0f);
    game_status_mut(g, e)->ink_mark_t = 3.0f;

    g->event_count = 0;
    elements_step(g);

    int ignite_idx = -1;
    int zap_idx = -1;
    for (int32_t i = 0; i < g->event_count; ++i) {
        if (g->events[i].type == EV_IGNITE && ignite_idx < 0) {
            ignite_idx = i;
        }
        if (g->events[i].type == EV_STORM_ZAP && zap_idx < 0) {
            zap_idx = i;
        }
    }
    TEST_ASSERT_TRUE(ignite_idx >= 0);
    TEST_ASSERT_TRUE(zap_idx >= 0);
    TEST_ASSERT_TRUE(ignite_idx < zap_idx);

    game_destroy(g);
}

static void test_pending_impulse_applied_next_tick(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    const float cx = cell_center_px(80);
    const float cy = cell_center_px(45);
    g->pending_impulses[0] = (PendingImpulse){
        .x = cx,
        .y = cy,
        .rad_cells = 8.0f,
        .peak_speed = 60.0f,
    };
    g->pending_impulse_count = 1;

    /* Before the next step's stage 3, velocity is still 0. */
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.0f, g->fluid.u[cell_idx(83, 45)]);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, 0.0f, g->fluid.u[cell_idx(77, 45)]);

    game_step(g, NULL, NULL);

    TEST_ASSERT_EQUAL_INT32(0, g->pending_impulse_count);
    TEST_ASSERT_TRUE(g->fluid.u[cell_idx(83, 45)] > 0.0f);
    TEST_ASSERT_TRUE(g->fluid.u[cell_idx(77, 45)] < 0.0f);
    TEST_ASSERT_TRUE(g->fluid.stats.max_vel <= g->fluid.cfg.max_vel + 1e-3f);
    TEST_ASSERT_TRUE(fluid_validate_finite(&g->fluid));

    game_destroy(g);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_blazing_inkstorm_cell_reaction);
    RUN_TEST(test_ignite_callout_threshold_and_cooldown);
    RUN_TEST(test_storm_growth_on_ink);
    RUN_TEST(test_tempest_lightning_chain);
    RUN_TEST(test_status_application_and_burn_intervals);
    RUN_TEST(test_projectile_elemental_upgrades);
    RUN_TEST(test_sanitize_and_finite);
    RUN_TEST(test_processing_order_ignite_before_storm_zap);
    RUN_TEST(test_pending_impulse_applied_next_tick);
    return UNITY_END();
}
