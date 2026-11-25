#include "combat/damage.h"
#include "core.h"
#include "elements/elements.h"
#include "fluid.h"
#include "game.h"
#include "roguelike/relics.h"
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

static void test_thunderburst_trigger_impulse_decay_cd_and_cap(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    /* Block (5, 5): x = 40..47, y = 40..47. 5 cells must NOT trigger. */
    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);
    for (int k = 0; k < 5; ++k) {
        fire[cell_idx(40 + k, 40)] = 0.45f;
        storm[cell_idx(40 + k, 40)] = 0.45f;
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_THUNDERBURST));
    TEST_ASSERT_EQUAL_INT32(0, g->pending_impulse_count);
    TEST_ASSERT_EQUAL_INT32(0, g->run.thunderbursts);

    /* Spawn Stone Oni (MASS_HEAVY) and stopped Nue inside 96px, plus an enemy outside 96px. */
    const float cx = 43.0f * FLUID_CELL_PX; /* centroid of 40..45 cell centers is 43.0 -> 344px */
    const float cy = cell_center_px(40);
    const EntityId oni = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cx + 40.0f, cy);
    const EntityId nue = game_debug_spawn_enemy(g, ENEMY_BOSS_NUE, cx, cy + 40.0f);
    const EntityId far_e = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, cx + 150.0f, cy);
    game_enemy_mut(g, nue)->ai_state = 1; /* stopped */

    fluid_reset(&g->fluid);
    for (int k = 0; k < 6; ++k) {
        fire[cell_idx(40 + k, 40)] = 0.45f;
        storm[cell_idx(40 + k, 40)] = 0.45f;
    }
    g->event_count = 0;
    elements_step(g);

    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_THUNDERBURST));
    TEST_ASSERT_EQUAL_INT32(1, g->run.thunderbursts);
    TEST_ASSERT_EQUAL_INT32(1, g->pending_impulse_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 60.0f, g->pending_impulses[0].peak_speed);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 9.0f, g->pending_impulses[0].rad_cells);
    TEST_ASSERT_TRUE(g->hitstop_t >= 0.03f - 1e-5f);

    /* Stone Oni took 30 dmg and outward knockback (520 * 0.55 = 286 px/s); Stage 4 decays
     * knockback_t by 1 tick after Stage 3 sets it to 1.0s. */
    TEST_ASSERT_EQUAL_INT32(60, game_health(g, oni)->hp);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 286.0f, game_velocity(g, oni)->vx);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f - INK_FIXED_DT, game_status(g, oni)->knockback_t);

    /* Stopped Nue took dual-element (FIRE | STORM) lower-resistance dmg (30) and outward knockback
     * (286 px/s); per game-design §6.4 bosses apply fmaxf(m_fire=1.0, m_storm=0.5) = 1.0. */
    TEST_ASSERT_EQUAL_INT32(670, game_health(g, nue)->hp);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 286.0f, game_velocity(g, nue)->vy);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f - INK_FIXED_DT, game_status(g, nue)->knockback_t);

    /* Far enemy untouched. */
    TEST_ASSERT_EQUAL_INT32(30, game_health(g, far_e)->hp);

    /* Dye decayed within 9 cells: fire *= 0.3, storm *= 0.2. */
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.45f * 0.3f, fire[cell_idx(42, 40)]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.45f * 0.2f, storm[cell_idx(42, 40)]);

    /* Pending impulse applies outward velocity on the next non-hitstop game_step. */
    clear_all_entities(g);
    g->hitstop_t = 0.0f;
    game_step(g, NULL, NULL);
    TEST_ASSERT_EQUAL_INT32(0, g->pending_impulse_count);
    TEST_ASSERT_TRUE(g->fluid.u[cell_idx(45, 40)] > 0.0f);
    TEST_ASSERT_TRUE(g->fluid.u[cell_idx(40, 40)] < 0.0f);

    /* Block CD: 0.8s = 48 ticks. We already ran 1 game_step (1 elements_step), so 46 more ticks
     * must not re-fire, and the 47th (48th total after trigger) re-fires. */
    for (int t = 0; t < 46; ++t) {
        for (int k = 0; k < 6; ++k) {
            fire[cell_idx(40 + k, 40)] = 0.45f;
            storm[cell_idx(40 + k, 40)] = 0.45f;
        }
        g->event_count = 0;
        elements_step(g);
        TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_THUNDERBURST));
    }
    for (int k = 0; k < 6; ++k) {
        fire[cell_idx(40 + k, 40)] = 0.45f;
        storm[cell_idx(40 + k, 40)] = 0.45f;
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_THUNDERBURST));

    /* Per-tick cap of 3: set 5 distinct blocks simultaneously, only top 3 fire. */
    fluid_reset(&g->fluid);
    for (int b = 0; b < GAME_THUNDERBURST_BLOCKS; ++b) {
        g->internal.thunderburst_block_cd[b] = 0.0f;
    }
    g->pending_impulse_count = 0;
    for (int blk = 0; blk < 5; ++blk) {
        const int bx = (2 + blk * 3) * 8;
        const int by = 2 * 8;
        const int ncells = 6 + blk; /* 6, 7, 8, 9, 10 cells */
        for (int k = 0; k < ncells; ++k) {
            fire[cell_idx(bx + (k % 8), by + (k / 8))] = 0.45f;
            storm[cell_idx(bx + (k % 8), by + (k / 8))] = 0.45f;
        }
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(3, count_events(g, EV_THUNDERBURST));
    TEST_ASSERT_EQUAL_INT32(3, g->pending_impulse_count);

    game_destroy(g);
}

static void test_thunderburst_in_bottom_right_block_uses_last_block(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    /* The bottom row of 64px blocks is only partly covered by the 720px screen. Painting the
     * fixed screen region x >= 1216, y >= 704 fills that last partial block on any grid. */
    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            if (cell_center_px(x) >= 1216.0f && cell_center_px(y) >= 704.0f) {
                fire[cell_idx(x, y)] = 0.45f;
                storm[cell_idx(x, y)] = 0.45f;
            }
        }
    }
    g->event_count = 0;
    elements_step(g);

    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_THUNDERBURST));
    TEST_ASSERT_TRUE(g->internal.thunderburst_block_cd[GAME_THUNDERBURST_BLOCKS - 1] > 0.0f);
    game_destroy(g);
}

static void test_field_without_fire_or_storm_leaves_ink_untouched(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    float *ink = fluid_channel_mut(&g->fluid, 0);
    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);
    for (int x = 20; x < 60; ++x) {
        ink[cell_idx(x, 30)] = 1.8f;
    }
    fire[cell_idx(30, 30)] = 0.14f;
    storm[cell_idx(31, 30)] = 0.14f;
    g->internal.blazing_active = true;
    const uint64_t fluid_before = fluid_hash(&g->fluid);
    g->event_count = 0;

    elements_step(g);

    TEST_ASSERT_FALSE(g->internal.blazing_active);
    TEST_ASSERT_EQUAL_HEX64(fluid_before, fluid_hash(&g->fluid));
    TEST_ASSERT_EQUAL_INT32(0, g->event_count);
    TEST_ASSERT_EQUAL_INT32(0, g->pending_impulse_count);
    game_destroy(g);
}

static void test_cataclysm_trigger_excludes_thunderburst_and_cd(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    /* Block (5, 5): x = 40..47, y = 40 -> centroid at (43.5 * 8 = 348, 40.5 * 8 = 324). */
    const float cx = 348.0f;
    const float cy = 324.0f;

    const EntityId e_near = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cx + 40.0f, cy);
    const EntityId e_repel = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cx - 50.0f, cy);
    const EntityId e_bolt3 = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cx + 160.0f, cy);
    const EntityId e_bolt4 = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cx + 190.0f, cy);
    const EntityId e_bolt5 = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cx + 220.0f, cy);
    const EntityId e_bolt6 = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cx + 250.0f, cy);
    game_enemy_mut(g, e_repel)->elite_flags |= ELITE_INK_REPEL;
    game_health_mut(g, e_near)->hp = 200;
    game_health_mut(g, e_repel)->hp = 200;
    game_health_mut(g, e_bolt3)->hp = 200;
    game_health_mut(g, e_bolt4)->hp = 200;
    game_health_mut(g, e_bolt5)->hp = 200;
    game_health_mut(g, e_bolt6)->hp = 200;

    fluid_reset(&g->fluid);
    float *ink = fluid_channel_mut(&g->fluid, 0);
    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);
    for (int k = 0; k < 8; ++k) {
        ink[cell_idx(40 + k, 40)] = 0.45f;
        fire[cell_idx(40 + k, 40)] = 0.45f;
        storm[cell_idx(40 + k, 40)] = 0.45f;
    }

    g->event_count = 0;
    elements_step(g);

    /* Cataclysm fires and excludes same-block Thunderburst. */
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_CATACLYSM));
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_THUNDERBURST));
    TEST_ASSERT_EQUAL_INT32(1, g->run.cataclysms);
    TEST_ASSERT_EQUAL_INT32(1, g->pending_impulse_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 90.0f, g->pending_impulses[0].peak_speed);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 12.0f, g->pending_impulses[0].rad_cells);
    TEST_ASSERT_TRUE(g->hitstop_t >= 0.06f - 1e-5f);

    /* e_near: 80 AoE + ink mark applied -> 30*2 bolt = 140 dmg, knockback applied (Stage 4 decays
     * timers by 1 tick after Stage 2 sets them). */
    TEST_ASSERT_EQUAL_INT32(60, game_health(g, e_near)->hp);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 3.0f - INK_FIXED_DT, game_status(g, e_near)->ink_mark_t);
    TEST_ASSERT_TRUE(game_velocity(g, e_near)->vx > 300.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f - INK_FIXED_DT, game_status(g, e_near)->knockback_t);

    /* e_repel: 80 AoE + no ink mark (ELITE_INK_REPEL) + 30 bolt = 110 dmg. */
    TEST_ASSERT_EQUAL_INT32(90, game_health(g, e_repel)->hp);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, game_status(g, e_repel)->ink_mark_t);

    /* Wide lightning hits top 4 closest enemies only (e_near, e_repel, e_bolt3, e_bolt4). */
    TEST_ASSERT_EQUAL_INT(4, count_events(g, EV_STORM_ZAP));
    TEST_ASSERT_EQUAL_INT32(170, game_health(g, e_bolt3)->hp);
    TEST_ASSERT_EQUAL_INT32(170, game_health(g, e_bolt4)->hp);
    TEST_ASSERT_EQUAL_INT32(200, game_health(g, e_bolt5)->hp);
    TEST_ASSERT_EQUAL_INT32(200, game_health(g, e_bolt6)->hp);

    /* Global CD 2.0s (120 ticks): another block must not trigger Cataclysm for 119 ticks. */
    clear_all_entities(g);
    for (int t = 0; t < 119; ++t) {
        fluid_reset(&g->fluid);
        for (int k = 0; k < 8; ++k) {
            ink[cell_idx(80 + k, 40)] = 0.45f;
            fire[cell_idx(80 + k, 40)] = 0.45f;
            storm[cell_idx(80 + k, 40)] = 0.45f;
        }
        g->event_count = 0;
        elements_step(g);
        TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_CATACLYSM));
    }
    /* 120th tick (== 2.0s) triggers Cataclysm in the other block. */
    fluid_reset(&g->fluid);
    for (int k = 0; k < 8; ++k) {
        ink[cell_idx(80 + k, 40)] = 0.45f;
        fire[cell_idx(80 + k, 40)] = 0.45f;
        storm[cell_idx(80 + k, 40)] = 0.45f;
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_CATACLYSM));
    TEST_ASSERT_EQUAL_INT32(2, g->run.cataclysms);

    game_destroy(g);
}

static void test_thunderburst_fuse_upgrade(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);

    /* Default thresh 0.30: fire=0.20, storm=0.20 must not trigger. */
    for (int k = 0; k < 6; ++k) {
        fire[cell_idx(40 + k, 40)] = 0.20f;
        storm[cell_idx(40 + k, 40)] = 0.20f;
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_THUNDERBURST));

    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_THUNDERBURST_FUSE));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.26f, g->internal.mods.thunderburst_thresh);
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_THUNDERBURST));

    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_THUNDERBURST_FUSE));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.22f, g->internal.mods.thunderburst_thresh);
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_THUNDERBURST));

    /* Lv3: thresh 0.18, radius_mul 1.30 (96 * 1.3 = 124.8px). */
    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_THUNDERBURST_FUSE));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.18f, g->internal.mods.thunderburst_thresh);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.30f, g->internal.mods.thunderburst_radius_mul);

    /* Centroid of x=40..45, y=40 is (42.5 + 0.5) * 8 = 344, 324. Place enemy 110px away. */
    const EntityId e = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 344.0f + 110.0f, 324.0f);
    fluid_reset(&g->fluid);
    for (int k = 0; k < 6; ++k) {
        fire[cell_idx(40 + k, 40)] = 0.20f;
        storm[cell_idx(40 + k, 40)] = 0.20f;
    }
    g->event_count = 0;
    elements_step(g);

    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_THUNDERBURST));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 72.0f * 1.30f, g->events[0].x2);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 9.0f * 1.30f, g->pending_impulses[0].rad_cells);
    TEST_ASSERT_EQUAL_INT32(60, game_health(g, e)->hp);

    game_destroy(g);
}

static int count_orbs(const GameState *g)
{
    int n = 0;
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, id, COMP_INK_ORB)) {
            ++n;
        }
    }
    return n;
}

static void test_relic_seal_of_trinity(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    float *ink = fluid_channel_mut(&g->fluid, 0);
    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);

    /* Without Seal of Trinity (thresh 0.30): 8 cells at 0.25 must not trigger Cataclysm. */
    for (int k = 0; k < 8; ++k) {
        ink[cell_idx(40 + k, 40)] = 0.25f;
        fire[cell_idx(40 + k, 40)] = 0.25f;
        storm[cell_idx(40 + k, 40)] = 0.25f;
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_CATACLYSM));
    TEST_ASSERT_EQUAL_INT32(0, g->run.cataclysms);

    /* With Seal of Trinity (thresh 0.22, CD 1.2s): triggers immediately. */
    TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_SEAL_OF_TRINITY));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.22f, g->internal.mods.cataclysm_thresh);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.2f, g->internal.mods.cataclysm_cd_base);

    fluid_reset(&g->fluid);
    for (int k = 0; k < 8; ++k) {
        ink[cell_idx(40 + k, 40)] = 0.25f;
        fire[cell_idx(40 + k, 40)] = 0.25f;
        storm[cell_idx(40 + k, 40)] = 0.25f;
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_CATACLYSM));
    TEST_ASSERT_EQUAL_INT32(1, g->run.cataclysms);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.2f, g->internal.cataclysm_cd);

    /* 71 ticks (< 1.2s) must not re-fire; 72nd tick (== 1.2s) re-fires. */
    for (int t = 0; t < 71; ++t) {
        fluid_reset(&g->fluid);
        for (int k = 0; k < 8; ++k) {
            ink[cell_idx(80 + k, 40)] = 0.25f;
            fire[cell_idx(80 + k, 40)] = 0.25f;
            storm[cell_idx(80 + k, 40)] = 0.25f;
        }
        g->event_count = 0;
        elements_step(g);
        TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_CATACLYSM));
    }
    fluid_reset(&g->fluid);
    for (int k = 0; k < 8; ++k) {
        ink[cell_idx(80 + k, 40)] = 0.25f;
        fire[cell_idx(80 + k, 40)] = 0.25f;
        storm[cell_idx(80 + k, 40)] = 0.25f;
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_CATACLYSM));
    TEST_ASSERT_EQUAL_INT32(2, g->run.cataclysms);

    game_destroy(g);
}

static void test_relic_inkstorm_secret(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_INKSTORM_SECRET));
    TEST_ASSERT_EQUAL_INT32(3, g->internal.mods.surge_min_chain);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.5f, g->internal.mods.surge_reaction_dmg_mul);

    /* Tradeoff: chain 2 does NOT start surge; chain 3 starts surge. */
    const EntityId a1 = game_debug_spawn_enemy(g, ENEMY_ARCHER, 300.0f, 300.0f);
    const EntityId a2 = game_debug_spawn_enemy(g, ENEMY_ARCHER, 360.0f, 300.0f);
    const EntityId a3 = game_debug_spawn_enemy(g, ENEMY_ARCHER, 420.0f, 300.0f);
    (void)combat_apply_damage(g, a1, 50, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(1, g->run.chain);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->player.surge_t);
    (void)combat_apply_damage(g, a2, 50, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(2, g->run.chain);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->player.surge_t);
    (void)combat_apply_damage(g, a3, 50, DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(3, g->run.chain);
    TEST_ASSERT_TRUE(g->player.surge_t > 0.0f);

    float *ink = fluid_channel_mut(&g->fluid, 0);
    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);

    /* 1. Blazing burn DoT during surge: 10 -> 15. */
    clear_all_entities(g);
    fluid_reset(&g->fluid);
    const EntityId e_burn =
        game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cell_center_px(70), cell_center_px(30));
    fluid_reset(&g->fluid);
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            fire[cell_idx(70 + dx, 30 + dy)] = 0.45f;
        }
    }
    g->player.surge_t = 3.0f;
    for (int t = 0; t < 22; ++t) {
        elements_step(g);
    }
    TEST_ASSERT_EQUAL_INT32(75, game_health(g, e_burn)->hp);

    /* 2. Tempest Lightning initial strike during surge: 10 -> 15 (no ink mark). */
    clear_all_entities(g);
    fluid_reset(&g->fluid);
    const EntityId e_zap =
        game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cell_center_px(40), cell_center_px(40) + 100.0f);
    fluid_reset(&g->fluid);
    for (int k = 0; k < 10; ++k) {
        storm[cell_idx(40 + k, 40)] = 0.50f;
    }
    game_status_mut(g, e_zap)->charge_t = 2.0f;
    g->internal.storm_zap_cd = 0.0f;
    g->player.surge_t = 3.0f;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT32(75, game_health(g, e_zap)->hp);

    /* 3. Thunderburst during surge: 30 -> 45. */
    clear_all_entities(g);
    fluid_reset(&g->fluid);
    const float tb_cx = 43.0f * FLUID_CELL_PX;
    const float tb_cy = cell_center_px(40);
    const EntityId e_tb = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, tb_cx + 40.0f, tb_cy);
    fluid_reset(&g->fluid);
    for (int b = 0; b < GAME_THUNDERBURST_BLOCKS; ++b) {
        g->internal.thunderburst_block_cd[b] = 0.0f;
    }
    for (int k = 0; k < 6; ++k) {
        fire[cell_idx(40 + k, 40)] = 0.45f;
        storm[cell_idx(40 + k, 40)] = 0.45f;
    }
    g->player.surge_t = 3.0f;
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_THUNDERBURST));
    TEST_ASSERT_EQUAL_INT32(45, game_health(g, e_tb)->hp);

    /* 4. Cataclysm body AoE during surge: 80 -> 120 (4 closer decoys take the 4 wide bolts). */
    clear_all_entities(g);
    fluid_reset(&g->fluid);
    const float cat_cx = 348.0f;
    const float cat_cy = 324.0f;
    for (int d = 1; d <= 4; ++d) {
        const EntityId decoy =
            game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cat_cx + (float)d * 10.0f, cat_cy);
        game_health_mut(g, decoy)->hp = 300;
    }
    const EntityId e_cat = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, cat_cx + 100.0f, cat_cy);
    game_health_mut(g, e_cat)->hp = 200;
    fluid_reset(&g->fluid);
    for (int k = 0; k < 8; ++k) {
        ink[cell_idx(40 + k, 40)] = 0.45f;
        fire[cell_idx(40 + k, 40)] = 0.45f;
        storm[cell_idx(40 + k, 40)] = 0.45f;
    }
    g->internal.cataclysm_cd = 0.0f;
    g->player.surge_t = 3.0f;
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_CATACLYSM));
    TEST_ASSERT_EQUAL_INT32(80, game_health(g, e_cat)->hp);

    /* 5. Normal deflected arrow (without DMG_REACTION) is NOT multiplied by 1.5 during surge. */
    clear_all_entities(g);
    fluid_reset(&g->fluid);
    const EntityId e_arrow = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 500.0f, 300.0f);
    g->player.surge_t = 3.0f;
    g->player.dry = false;
    const int32_t dealt =
        combat_apply_damage(g, e_arrow, 20, DMG_DEFLECTED | DMG_PHYSICAL, 0.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(20, dealt);

    game_destroy(g);
}

static void test_relic_thunderfire_gem(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_all_entities(g);
    fluid_reset(&g->fluid);

    TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_THUNDERFIRE_GEM));
    TEST_ASSERT_TRUE(g->internal.mods.thunderfire_gem);

    float *fire = fluid_channel_mut(&g->fluid, 1);
    float *storm = fluid_channel_mut(&g->fluid, 2);

    /* Two Thunderburst blocks in the same tick drop at most 1 orb and start 1.5s CD. */
    for (int blk = 0; blk < 2; ++blk) {
        const int bx = (2 + blk * 3) * 8;
        const int by = 2 * 8;
        for (int k = 0; k < 6; ++k) {
            fire[cell_idx(bx + k, by)] = 0.45f;
            storm[cell_idx(bx + k, by)] = 0.45f;
        }
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(2, count_events(g, EV_THUNDERBURST));
    TEST_ASSERT_EQUAL_INT(1, count_orbs(g));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.5f, g->internal.thunderfire_gem_cd);

    /* 89 ticks (< 1.5s): re-triggering Thunderburst must NOT spawn another orb. */
    for (int t = 0; t < 89; ++t) {
        upgrades_fire_on_tick(g, INK_FIXED_DT);
    }
    TEST_ASSERT_TRUE(g->internal.thunderfire_gem_cd > 0.0f);
    fluid_reset(&g->fluid);
    for (int b = 0; b < GAME_THUNDERBURST_BLOCKS; ++b) {
        g->internal.thunderburst_block_cd[b] = 0.0f;
    }
    for (int k = 0; k < 6; ++k) {
        fire[cell_idx(16 + k, 16)] = 0.45f;
        storm[cell_idx(16 + k, 16)] = 0.45f;
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_THUNDERBURST));
    TEST_ASSERT_EQUAL_INT(1, count_orbs(g));

    /* 90th tick (== 1.5s): CD reaches 0 and next Thunderburst drops 1 orb again. */
    upgrades_fire_on_tick(g, INK_FIXED_DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->internal.thunderfire_gem_cd);
    fluid_reset(&g->fluid);
    for (int b = 0; b < GAME_THUNDERBURST_BLOCKS; ++b) {
        g->internal.thunderburst_block_cd[b] = 0.0f;
    }
    for (int k = 0; k < 6; ++k) {
        fire[cell_idx(16 + k, 16)] = 0.45f;
        storm[cell_idx(16 + k, 16)] = 0.45f;
    }
    g->event_count = 0;
    elements_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_THUNDERBURST));
    TEST_ASSERT_EQUAL_INT(2, count_orbs(g));

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
    RUN_TEST(test_thunderburst_trigger_impulse_decay_cd_and_cap);
    RUN_TEST(test_thunderburst_in_bottom_right_block_uses_last_block);
    RUN_TEST(test_field_without_fire_or_storm_leaves_ink_untouched);
    RUN_TEST(test_cataclysm_trigger_excludes_thunderburst_and_cd);
    RUN_TEST(test_thunderburst_fuse_upgrade);
    RUN_TEST(test_relic_seal_of_trinity);
    RUN_TEST(test_relic_inkstorm_secret);
    RUN_TEST(test_relic_thunderfire_gem);
    return UNITY_END();
}
