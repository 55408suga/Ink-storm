#include "combat/damage.h"
#include "core.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"
#include "game.h"
#include "player/player_skills.h"
#include "roguelike/relics.h"
#include "roguelike/rewards.h"
#include "roguelike/stage_gimmicks.h"
#include "roguelike/upgrades.h"
#include "roguelike/waves.h"
#include "unity.h"

#include <math.h>
#include <stddef.h>

void setUp(void) {}

void tearDown(void) {}

static void clear_entities(GameState *g)
{
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX) {
            ecs_destroy(&g->ecs, id);
        }
    }
    ecs_flush_destroy(&g->ecs);
    g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};
}

static GameState *arena(BrushStyleId brush)
{
    const RunConfig cfg = meta_run_config_preset(brush, UINT64_C(77));
    GameState *g = game_create(&cfg);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    clear_entities(g);
    fluid_reset(&g->fluid);
    g->event_count = 0;
    return g;
}

static int count_events(const GameState *g, GameEventType type, uint32_t flags)
{
    int n = 0;
    for (int32_t i = 0; i < g->event_count; ++i) {
        if (g->events[i].type == type && (g->events[i].flags & flags) == flags) {
            ++n;
        }
    }
    return n;
}

static int count_with_mask(const GameState *g, uint64_t mask)
{
    int n = 0;
    for (uint32_t i = 0; i < g->ecs.high_water; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, id, mask)) {
            ++n;
        }
    }
    return n;
}

static EntityId sturdy_enemy(GameState *g, EnemyDefId def, float x, float y, int32_t hp)
{
    const EntityId id = game_debug_spawn_enemy(g, def, x, y);
    TEST_ASSERT_NOT_EQUAL_UINT32(ECS_INVALID_INDEX, id.index);
    HealthComp *h = game_health_mut(g, id);
    h->hp = hp;
    h->max_hp = hp;
    game_velocity_mut(g, id)->vx = 0.0f;
    game_velocity_mut(g, id)->vy = 0.0f;
    return id;
}

static void cast_gale_right(GameState *g, float x, float y)
{
    g->player.ink = g->player.ink_max;
    g->player.gale_charges = g->player.gale_max_charges;
    g->player.aim_x = 1.0f;
    g->player.aim_y = 0.0f;
    const BrushInput in = {.pmx = x, .pmy = y, .mx = x, .my = y, .gale = true};
    player_skills_step(g, &in);
}

static void test_maelstrom_bind_collision_damage_waits_for_hit_cooldown(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    for (int lv = 0; lv < 3; ++lv) {
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_MAELSTROM_BIND));
    }
    const EntityId a = sturdy_enemy(g, ENEMY_KAMAITACHI, 400.0f, 300.0f, 1000);
    const EntityId b = sturdy_enemy(g, ENEMY_KAMAITACHI, 410.0f, 300.0f, 1000);
    g->internal.cyclones[0] = (ActiveCyclone){.active = true,
                                              .x = 405.0f,
                                              .y = 300.0f,
                                              .rad_cells = 13.0f,
                                              .life = 9.0f,
                                              .max_life = 9.0f};

    for (int t = 0; t < 20; ++t) {
        upgrades_fire_on_tick(g, INK_FIXED_DT);
    }
    TEST_ASSERT_EQUAL_INT32(960, game_health(g, a)->hp);
    TEST_ASSERT_EQUAL_INT32(960, game_health(g, b)->hp);

    for (int t = 0; t < 20; ++t) {
        upgrades_fire_on_tick(g, INK_FIXED_DT);
    }
    TEST_ASSERT_EQUAL_INT32(920, game_health(g, a)->hp);
    TEST_ASSERT_EQUAL_INT32(920, game_health(g, b)->hp);
    game_destroy(g);
}

static void test_piercing_burning_shot_hits_each_target_once(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    const EntityId boss = game_debug_spawn_enemy(g, ENEMY_BOSS_TENGU, 640.0f, 300.0f);
    const int32_t hp0 = game_health(g, boss)->hp;
    const EntityId shot = projectiles_spawn(g, PROJ_ARROW, 560.0f, 300.0f, 600.0f, 0.0f);
    ProjComp *p = game_proj_mut(g, shot);
    p->deflected = true;
    p->burning = true;
    p->pierce = 3;

    for (int t = 0; t < 45 && ecs_is_alive(&g->ecs, shot); ++t) {
        projectiles_step(g);
        ecs_flush_destroy(&g->ecs);
    }
    TEST_ASSERT_EQUAL_INT32(hp0 - 40, game_health(g, boss)->hp);
    if (ecs_is_alive(&g->ecs, shot)) {
        TEST_ASSERT_EQUAL_INT32(2, game_proj(g, shot)->pierce);
    }
    game_destroy(g);
}

static void test_gale_reaches_slam_speed_for_heavy_and_fanned_targets(void)
{
    GameState *g = arena(BRUSH_STYLE_QUILL);
    const EntityId oni = sturdy_enemy(g, ENEMY_STONE_ONI, 400.0f, 300.0f, 90);
    cast_gale_right(g, 300.0f, 300.0f);
    TEST_ASSERT_TRUE(game_velocity(g, oni)->vx > 215.0f);
    game_destroy(g);

    g = arena(BRUSH_STYLE_QUILL);
    TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_TENGU_FAN));
    const EntityId kama = sturdy_enemy(g, ENEMY_KAMAITACHI, 400.0f, 300.0f, 30);
    const EntityId nue = sturdy_enemy(g, ENEMY_BOSS_NUE, 400.0f, 420.0f, 700);
    game_enemy_mut(g, nue)->ai_state = 1;
    cast_gale_right(g, 300.0f, 300.0f);
    TEST_ASSERT_TRUE(game_velocity(g, kama)->vx > 215.0f);
    cast_gale_right(g, 300.0f, 420.0f);
    TEST_ASSERT_TRUE(game_velocity(g, nue)->vx > 215.0f);
    game_destroy(g);
}

static void test_undeflected_shot_passing_shrine_stays_hostile_without_fluid(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    const float sx = g->shrines[0].x;
    const float sy = g->shrines[0].y;
    const EntityId shot =
        projectiles_spawn(g, PROJ_FIREBALL, sx - 240.0f, sy - 70.0f, 205.0f, 0.0f);
    for (int t = 0; t < 150 && ecs_is_alive(&g->ecs, shot); ++t) {
        fluid_reset(&g->fluid);
        projectiles_step(g);
        ecs_flush_destroy(&g->ecs);
        TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_DEFLECT, 0));
    }
    TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, shot));
    TEST_ASSERT_FALSE(game_proj(g, shot)->deflected);
    TEST_ASSERT_TRUE(game_transform(g, shot)->x > sx + 40.0f);
    game_destroy(g);
}

static void test_barrier_ward_is_kept_while_shrine_is_invulnerable(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    for (int lv = 0; lv < 3; ++lv) {
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_BARRIER));
    }
    const float sx = g->shrines[0].x;
    const float sy = g->shrines[0].y;
    const int32_t hp0 = g->shrines[0].hp;
    g->internal.barrier_shield_ready = true;
    g->shrines[0].invuln_t = 0.5f;
    (void)projectiles_spawn(g, PROJ_ARROW, sx + 20.0f, sy, -200.0f, 0.0f);
    projectiles_step(g);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_TRUE(g->internal.barrier_shield_ready);
    TEST_ASSERT_EQUAL_INT32(hp0, g->shrines[0].hp);

    g->shrines[0].invuln_t = 0.0f;
    g->event_count = 0;
    (void)projectiles_spawn(g, PROJ_ARROW, sx + 20.0f, sy, -200.0f, 0.0f);
    projectiles_step(g);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_FALSE(g->internal.barrier_shield_ready);
    TEST_ASSERT_EQUAL_INT32(hp0, g->shrines[0].hp);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_SHRINE_HIT, EVF_SHRINE_BLOCKED));
    game_destroy(g);
}

static void test_rich_ink_lv3_drops_orb_on_secondary_kill(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    for (int lv = 0; lv < 3; ++lv) {
        TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_RICH_INK));
    }
    const EntityId e = game_debug_spawn_enemy(g, ENEMY_KAMAITACHI, 500.0f, 300.0f);
    (void)combat_apply_damage(g, e, 1000, DMG_PHYSICAL | DMG_SECONDARY, 0.0f, 0.0f);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_EQUAL_INT(enemy_def_get(ENEMY_KAMAITACHI)->orb_count + 1,
                          count_with_mask(g, COMP_INK_ORB));
    game_destroy(g);
}

static void max_all_upgrades_except(GameState *g, const UpgradeId *keep, int keep_n)
{
    for (int32_t i = 0; i < (int32_t)UPG_COUNT; ++i) {
        bool skip = false;
        for (int k = 0; k < keep_n; ++k) {
            skip = skip || keep[k] == (UpgradeId)i;
        }
        while (!skip && roguelike_grant_upgrade(g, (UpgradeId)i)) {
        }
    }
}

static void test_reroll_refuses_when_no_new_choice_exists(void)
{
    static const UpgradeId keep3[3] = {UPG_EMBER_TIP, UPG_WILDFIRE, UPG_TAILWIND};
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    max_all_upgrades_except(g, keep3, 3);
    rewards_generate_offer(g);
    g->phase = RUN_PHASE_REWARD;
    g->run.rerolls_left = 1;
    TEST_ASSERT_EQUAL_INT32(3, g->run.reward.choice_count);
    TEST_ASSERT_FALSE(rewards_reroll(g));
    TEST_ASSERT_EQUAL_INT32(1, g->run.rerolls_left);
    game_destroy(g);
}

static void test_reroll_brings_in_the_unseen_choice(void)
{
    static const UpgradeId keep4[4] = {UPG_EMBER_TIP, UPG_WILDFIRE, UPG_TAILWIND, UPG_DEEP_INK};
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    max_all_upgrades_except(g, keep4, 4);
    rewards_generate_offer(g);
    g->phase = RUN_PHASE_REWARD;
    g->run.rerolls_left = 1;
    int32_t before[GAME_MAX_REWARD_CHOICES];
    for (int i = 0; i < GAME_MAX_REWARD_CHOICES; ++i) {
        before[i] = g->run.reward.choice_ids[i];
    }
    int32_t unseen = -1;
    for (int k = 0; k < 4; ++k) {
        bool shown = false;
        for (int i = 0; i < GAME_MAX_REWARD_CHOICES; ++i) {
            shown = shown || before[i] == (int32_t)keep4[k];
        }
        if (!shown) {
            unseen = (int32_t)keep4[k];
        }
    }
    TEST_ASSERT_TRUE(unseen >= 0);
    TEST_ASSERT_TRUE(rewards_reroll(g));
    TEST_ASSERT_EQUAL_INT32(0, g->run.rerolls_left);
    bool offered = false;
    for (int i = 0; i < g->run.reward.choice_count; ++i) {
        offered = offered || g->run.reward.choice_ids[i] == unseen;
    }
    TEST_ASSERT_TRUE(offered);
    game_destroy(g);
}

static void test_nue_knockback_holds_dash_and_stun_bounce_moves(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    const EntityId nue = sturdy_enemy(g, ENEMY_BOSS_NUE, 640.0f, 200.0f, 700);
    EnemyComp *en = game_enemy_mut(g, nue);
    StatusComp *st = game_status_mut(g, nue);
    en->ai_state = 1;
    en->windup = 0.0f;
    en->fire_cd = 0.05f;
    st->knockback_t = 0.6f;
    for (int t = 0; t < 6; ++t) {
        enemies_ai_step(g);
    }
    TEST_ASSERT_EQUAL_INT32(1, en->ai_state);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.05f, en->fire_cd);

    st->knockback_t = 0.0f;
    st->stun_t = 0.8f;
    game_velocity_mut(g, nue)->vx = 120.0f;
    const float x0 = game_transform(g, nue)->x;
    combat_physics_and_slams_step(g);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, x0 + 120.0f * INK_FIXED_DT, game_transform(g, nue)->x);
    game_destroy(g);
}

static void test_deflected_fireball_deals_fire_damage(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_PACT_OF_FIRE));
    const EntityId e = sturdy_enemy(g, ENEMY_KAMAITACHI, 500.0f, 300.0f, 1000);
    const EntityId shot = projectiles_spawn(g, PROJ_FIREBALL, 480.0f, 300.0f, 300.0f, 0.0f);
    game_proj_mut(g, shot)->deflected = true;
    projectiles_step(g);
    TEST_ASSERT_EQUAL_INT32(1000 - 60, game_health(g, e)->hp);
    game_destroy(g);
}

static void test_wind_return_repels_approaching_walker(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    TEST_ASSERT_TRUE(roguelike_grant_relic(g, RELIC_WIND_RETURN));
    const float sx = g->shrines[0].x;
    const float sy = g->shrines[0].y;
    const EntityId leaving = sturdy_enemy(g, ENEMY_KAMAITACHI, sx + 60.0f, sy, 30);
    game_velocity_mut(g, leaving)->vx = 100.0f;
    upgrades_fire_on_tick(g, INK_FIXED_DT);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_GALE, 0));

    game_velocity_mut(g, leaving)->vx = -100.0f;
    upgrades_fire_on_tick(g, INK_FIXED_DT);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_GALE, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 12.0f, g->internal.wind_return_cd);
    game_destroy(g);
}

static void test_met_quota_blocks_shrine_damage_and_spawns(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    const int32_t hp0 = g->shrines[0].hp;
    g->run.wave_kills = g->run.wave_quota;
    TEST_ASSERT_TRUE(game_wave_goal_met(g));
    combat_damage_shrine(g, g->shrines[0].x + 10.0f, g->shrines[0].y, 1);
    TEST_ASSERT_EQUAL_INT32(hp0, g->shrines[0].hp);

    g->internal.spawn_cd = 0.0f;
    waves_step_spawns(g);
    TEST_ASSERT_EQUAL_INT(0, count_with_mask(g, COMP_ENEMY));

    g->run.wave_kills = g->run.wave_quota - 1;
    TEST_ASSERT_FALSE(game_wave_goal_met(g));
    combat_damage_shrine(g, g->shrines[0].x + 10.0f, g->shrines[0].y, 1);
    TEST_ASSERT_EQUAL_INT32(hp0 - 1, g->shrines[0].hp);
    game_destroy(g);
}

static void test_wave_clear_purifies_each_remaining_enemy(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    (void)game_debug_spawn_enemy(g, ENEMY_ARCHER, 120.0f, 120.0f);
    (void)game_debug_spawn_enemy(g, ENEMY_UMBRELLA, 1100.0f, 140.0f);
    (void)game_debug_spawn_enemy(g, ENEMY_KAMAITACHI, 1100.0f, 600.0f);
    g->run.wave_kills = g->run.wave_quota;
    game_step(g, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_WAVE_AFTERMATH, g->phase);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_WAVE_CLEAR, 0));
    TEST_ASSERT_EQUAL_INT(3, count_events(g, EV_PURIFY, 0));
    game_destroy(g);
}

static void test_aftermath_holds_hitstop_and_freezes_chain_and_surge(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    g->run.wave_kills = g->run.wave_quota;
    game_step(g, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_WAVE_AFTERMATH, g->phase);
    g->hitstop_t = 0.05f;
    g->run.chain = 3;
    g->run.chain_t = 1.0f;
    g->player.surge_t = 2.0f;
    for (int t = 0; t < 3; ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, g->phase_t);
    for (int t = 0; t < 30; ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_TRUE(g->phase_t > 0.4f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, g->run.chain_t);
    TEST_ASSERT_EQUAL_INT32(3, g->run.chain);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, g->player.surge_t);
    game_destroy(g);
}

static void test_gale_cooldown_clamps_to_shortened_total(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    g->player.gale_cd = 0.95f;
    TEST_ASSERT_TRUE(roguelike_grant_upgrade(g, UPG_DOUBLE_GALE));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.85f, g->player.gale_cd_total);
    TEST_ASSERT_TRUE(g->player.gale_cd <= g->player.gale_cd_total + 1e-6f);
    game_destroy(g);
}

static void test_wave_start_clears_lingering_field_state(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    g->run.act_index = 2;
    g->internal.cyclones[0].active = true;
    g->internal.maelstrom_bind_t[5] = 1.0f;
    g->internal.maelstrom_hit_cd[5] = 0.3f;
    g->internal.wind_telegraph_t = 0.0f;
    g->internal.wind_active_t = 2.0f;
    g->internal.wind_timer = 0.0f;
    waves_start_wave(g, 1);
    TEST_ASSERT_FALSE(g->internal.cyclones[0].active);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g->internal.maelstrom_bind_t[5]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g->internal.maelstrom_hit_cd[5]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g->internal.wind_active_t);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 10.5f, g->internal.wind_timer);

    g->internal.cyclones[1].active = true;
    g->internal.act_prepared = false;
    waves_prepare_act(g);
    TEST_ASSERT_FALSE(g->internal.cyclones[1].active);
    game_destroy(g);
}

static void test_ryu_orbs_start_on_screen_and_offscreen_parts_are_untargetable(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_act(g, 2);
    for (int t = 0; t < 240 && g->phase == RUN_PHASE_ACT_TRANSITION; ++t) {
        game_step(g, NULL, NULL);
    }
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    for (int s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
        const EntityId orb = g->ryu_body.orb_ids[s];
        const TransformComp *tr = game_transform(g, orb);
        TEST_ASSERT_NOT_NULL(tr);
        TEST_ASSERT_TRUE(tr->x >= 110.0f && tr->x <= 1170.0f);
        TEST_ASSERT_TRUE(tr->y >= 96.0f && tr->y <= 220.0f);
        TEST_ASSERT_TRUE(game_enemy_is_targetable(g, orb));
    }
    game_transform_mut(g, g->ryu_body.orb_ids[2])->x = -40.0f;
    TEST_ASSERT_FALSE(game_enemy_is_targetable(g, g->ryu_body.orb_ids[2]));
    game_destroy(g);
}

static void test_stroke_force_keeps_direction_and_scales_cap_with_brush_force(void)
{
    const float max_vel = 125.0f;
    const GameVec2 heavy = player_skills_stroke_force(100.0, 50.0, 1.2f, max_vel);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, heavy.y / heavy.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 46.0f * 1.2f, hypotf(heavy.x, heavy.y));

    const GameVec2 plain = player_skills_stroke_force(100.0, 50.0, 1.0f, max_vel);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 46.0f, hypotf(plain.x, plain.y));

    const GameVec2 mighty = player_skills_stroke_force(300.0, -10.0, 2.4f, max_vel);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, max_vel * 0.72f, hypotf(mighty.x, mighty.y));

    const GameVec2 small = player_skills_stroke_force(10.0, 0.0, 1.2f, max_vel);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 10.0f * 0.58f * 1.2f, small.x);
}

static void test_lobby_sandbox_paints_with_combat_brush_and_no_waves(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    stage_gimmicks_setup_act(g, 1);
    g->fluid.cfg.dye_damp[0] = 0.95f;
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 376.0f, 320.0f));

    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_QUILL, UINT64_C(1));
    game_init_sandbox(g, &cfg);
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 376.0f, 320.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.985f, g->fluid.cfg.dye_damp[0]);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);

    const BrushInput stroke = {
        .pmx = 300.0f, .pmy = 300.0f, .mx = 400.0f, .my = 300.0f, .paint = true};
    game_step_sandbox(g, &stroke);
    TEST_ASSERT_TRUE(g->player.painting);
    float dye[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(&g->fluid, 360.0f, 300.0f, NULL, NULL, dye);
    TEST_ASSERT_TRUE(dye[0] > 0.05f);

    const BrushInput cyclone = {
        .pmx = 400.0f, .pmy = 300.0f, .mx = 400.0f, .my = 300.0f, .cyclone = true};
    game_step_sandbox(g, &cyclone);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_CYCLONE, 0));

    for (int t = 0; t < 600; ++t) {
        game_step_sandbox(g, NULL);
    }
    TEST_ASSERT_EQUAL_INT(0, count_with_mask(g, COMP_ENEMY));
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    TEST_ASSERT_TRUE(fluid_validate_finite(&g->fluid));

    game_sandbox_set_brush(g, BRUSH_STYLE_EMBER);
    TEST_ASSERT_EQUAL_INT(BRUSH_STYLE_EMBER, g->config.brush_style);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 72.0f, g->internal.mods.stroke_ember_interval);
    game_destroy(g);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_maelstrom_bind_collision_damage_waits_for_hit_cooldown);
    RUN_TEST(test_piercing_burning_shot_hits_each_target_once);
    RUN_TEST(test_gale_reaches_slam_speed_for_heavy_and_fanned_targets);
    RUN_TEST(test_undeflected_shot_passing_shrine_stays_hostile_without_fluid);
    RUN_TEST(test_barrier_ward_is_kept_while_shrine_is_invulnerable);
    RUN_TEST(test_rich_ink_lv3_drops_orb_on_secondary_kill);
    RUN_TEST(test_reroll_refuses_when_no_new_choice_exists);
    RUN_TEST(test_reroll_brings_in_the_unseen_choice);
    RUN_TEST(test_nue_knockback_holds_dash_and_stun_bounce_moves);
    RUN_TEST(test_deflected_fireball_deals_fire_damage);
    RUN_TEST(test_wind_return_repels_approaching_walker);
    RUN_TEST(test_met_quota_blocks_shrine_damage_and_spawns);
    RUN_TEST(test_wave_clear_purifies_each_remaining_enemy);
    RUN_TEST(test_aftermath_holds_hitstop_and_freezes_chain_and_surge);
    RUN_TEST(test_gale_cooldown_clamps_to_shortened_total);
    RUN_TEST(test_wave_start_clears_lingering_field_state);
    RUN_TEST(test_ryu_orbs_start_on_screen_and_offscreen_parts_are_untargetable);
    RUN_TEST(test_stroke_force_keeps_direction_and_scales_cap_with_brush_force);
    RUN_TEST(test_lobby_sandbox_paints_with_combat_brush_and_no_waves);
    return UNITY_END();
}
