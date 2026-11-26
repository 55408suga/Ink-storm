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

#include <float.h>
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

static void test_recycled_slot_starts_without_maelstrom_state(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    const EntityId first = game_debug_spawn_enemy(g, ENEMY_KAMAITACHI, 400.0f, 300.0f);
    g->internal.maelstrom_bind_t[first.index] = 1.0f;
    g->internal.maelstrom_hit_cd[first.index] = 0.45f;
    ecs_destroy(&g->ecs, first);
    ecs_flush_destroy(&g->ecs);
    const EntityId next = game_debug_spawn_enemy(g, ENEMY_KAMAITACHI, 500.0f, 300.0f);
    TEST_ASSERT_EQUAL_UINT32(first.index, next.index);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g->internal.maelstrom_bind_t[next.index]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g->internal.maelstrom_hit_cd[next.index]);
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

static void test_piercing_shot_homes_on_fresh_targets_after_a_hit(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    const EntityId boss = game_debug_spawn_enemy(g, ENEMY_BOSS_TENGU, 640.0f, 300.0f);
    const EntityId fresh = sturdy_enemy(g, ENEMY_KAMAITACHI, 980.0f, 300.0f, 1000);
    const int32_t boss_hp0 = game_health(g, boss)->hp;
    const EntityId shot = projectiles_spawn(g, PROJ_ARROW, 560.0f, 300.0f, 600.0f, 0.0f);
    ProjComp *p = game_proj_mut(g, shot);
    p->deflected = true;
    p->burning = true;
    p->pierce = 3;
    for (int t = 0; t < 60 && ecs_is_alive(&g->ecs, shot); ++t) {
        game_transform_mut(g, fresh)->x = 980.0f;
        game_transform_mut(g, fresh)->y = 300.0f;
        projectiles_step(g);
        ecs_flush_destroy(&g->ecs);
    }
    TEST_ASSERT_EQUAL_INT32(boss_hp0 - 40, game_health(g, boss)->hp);
    TEST_ASSERT_EQUAL_INT32(1000 - 40, game_health(g, fresh)->hp);
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

    g->internal.barrier_shield_ready = true;
    g->run.wave_kills = g->run.wave_quota;
    (void)projectiles_spawn(g, PROJ_ARROW, sx + 20.0f, sy, -200.0f, 0.0f);
    projectiles_step(g);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_TRUE(g->internal.barrier_shield_ready);
    TEST_ASSERT_EQUAL_INT32(hp0, g->shrines[0].hp);
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
    TEST_ASSERT_FALSE(rewards_can_reroll(g));
    TEST_ASSERT_FALSE(rewards_reroll(g));
    TEST_ASSERT_EQUAL_INT32(1, g->run.rerolls_left);
    game_destroy(g);
}

static void test_autoplay_picks_when_reroll_would_be_refused(void)
{
    static const UpgradeId keep3[3] = {UPG_EMBER_TIP, UPG_WILDFIRE, UPG_TAILWIND};
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    max_all_upgrades_except(g, keep3, 3);
    g->shrines[0].hp = g->shrines[0].max_hp;
    rewards_generate_offer(g);
    g->phase = RUN_PHASE_REWARD;
    g->run.rerolls_left = 1;
    for (int t = 0; t < 5 && g->phase == RUN_PHASE_REWARD; ++t) {
        const GameCommand cmd = game_autoplay_command(g);
        TEST_ASSERT_NOT_EQUAL_INT(CMD_REROLL_REWARD, cmd.type);
        game_step(g, NULL, &cmd);
    }
    TEST_ASSERT_NOT_EQUAL_INT(RUN_PHASE_REWARD, g->phase);
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
    TEST_ASSERT_TRUE(rewards_can_reroll(g));
    TEST_ASSERT_TRUE(rewards_reroll(g));
    TEST_ASSERT_FALSE(rewards_can_reroll(g));
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

static void test_final_kill_hitstop_plays_before_the_clear(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    g->internal.spawn_cd = 999.0f;
    const EntityId last = game_debug_spawn_enemy(g, ENEMY_UMBRELLA, 52.0f, 400.0f);
    game_velocity_mut(g, last)->vx = -420.0f;
    game_velocity_mut(g, last)->vy = 0.0f;
    game_status_mut(g, last)->knockback_t = 1.0f;
    g->run.wave_kills = g->run.wave_quota - 1;
    g->run.chain = 3;
    g->run.chain_t = 1.0f;
    g->player.surge_t = 2.0f;

    game_step(g, NULL, NULL);
    TEST_ASSERT_FALSE(ecs_is_alive(&g->ecs, last));
    TEST_ASSERT_EQUAL_INT32(g->run.wave_quota, g->run.wave_kills);
    TEST_ASSERT_TRUE(g->hitstop_t > 0.0f);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_WAVE_CLEAR, 0));

    int ticks_to_clear = 0;
    for (int t = 1; t <= 10 && g->phase == RUN_PHASE_PLAYING; ++t) {
        game_step(g, NULL, NULL);
        ticks_to_clear = t;
    }
    TEST_ASSERT_EQUAL_INT(3, ticks_to_clear);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_WAVE_AFTERMATH, g->phase);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_WAVE_CLEAR, 0));

    const float chain_t = g->run.chain_t;
    const float surge_t = g->player.surge_t;
    for (int t = 0; t < 30; ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_TRUE(g->phase_t > 0.4f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, chain_t, g->run.chain_t);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, surge_t, g->player.surge_t);
    TEST_ASSERT_TRUE(g->run.chain_t > 0.0f);
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

static void test_long_ember_stroke_keeps_every_ember_and_huge_input_stays_finite(void)
{
    GameState *g = arena(BRUSH_STYLE_EMBER);
    g->player.ink = g->player.ink_max;
    const BrushInput sweep = {
        .pmx = 140.0f, .pmy = 500.0f, .mx = 1148.0f, .my = 500.0f, .paint = true};
    player_skills_step(g, &sweep);
    float dye[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(&g->fluid, 140.0f + 72.0f * 14.0f, 500.0f, NULL, NULL, dye);
    TEST_ASSERT_TRUE(dye[1] > 0.10f);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, g->internal.ember_tip_dist);

    const BrushInput huge = {
        .pmx = -FLT_MAX, .pmy = -FLT_MAX, .mx = FLT_MAX, .my = FLT_MAX, .paint = true};
    game_step(g, &huge, NULL);
    TEST_ASSERT_TRUE(isfinite(g->internal.ember_tip_dist));
    TEST_ASSERT_TRUE(isfinite(g->player.ink));
    TEST_ASSERT_TRUE(fluid_validate_finite(&g->fluid));
    game_destroy(g);
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

    RunConfig bought = meta_run_config_preset(BRUSH_STYLE_QUILL, UINT64_C(1));
    bought.meta_upgrade_lv[META_UPG_INK_MAX] = 2;
    const float ink_before =
        g->fluid.dye[0][(int)(300.0f / FLUID_CELL_PX) * FLUID_W + (int)(360.0f / FLUID_CELL_PX)];
    game_sandbox_apply_config(g, &bought);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 120.0f, g->player.ink_max);
    TEST_ASSERT_EQUAL_FLOAT(
        ink_before,
        g->fluid.dye[0][(int)(300.0f / FLUID_CELL_PX) * FLUID_W + (int)(360.0f / FLUID_CELL_PX)]);

    game_sandbox_set_brush(g, BRUSH_STYLE_EMBER);
    TEST_ASSERT_EQUAL_INT(BRUSH_STYLE_EMBER, g->config.brush_style);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 72.0f, g->internal.mods.stroke_ember_interval);
    game_destroy(g);
}

static void assert_ink_full(const GameState *g)
{
    TEST_ASSERT_EQUAL_FLOAT(g->player.ink_max, g->player.ink);
    TEST_ASSERT_FALSE(g->player.dry);
}

static void test_lobby_sandbox_never_runs_dry_but_keeps_skill_cooldowns(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(1));
    game_init_sandbox(g, &cfg);
    assert_ink_full(g);

    for (int t = 0; t < 600; ++t) {
        const float x = 200.0f + (float)(t % 30) * 30.0f;
        const BrushInput stroke = {
            .pmx = x, .pmy = 300.0f, .mx = x + 30.0f, .my = 340.0f, .paint = true};
        game_step_sandbox(g, &stroke);
        TEST_ASSERT_TRUE(g->player.painting);
        assert_ink_full(g);
    }

    const BrushInput cyclone = {
        .pmx = 640.0f, .pmy = 360.0f, .mx = 640.0f, .my = 360.0f, .cyclone = true};
    game_step_sandbox(g, &cyclone);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_CYCLONE, 0));
    assert_ink_full(g);
    game_step_sandbox(g, &cyclone);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_CYCLONE, 0));
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_SKILL_DENIED, 0));
    TEST_ASSERT_TRUE(g->player.cyclone_cd > 0.0f);
    assert_ink_full(g);
    const int wait_ticks = (int)ceilf(g->player.cyclone_cd * (float)INK_TICK_HZ) + 1;
    for (int t = 0; t < wait_ticks; ++t) {
        game_step_sandbox(g, NULL);
    }
    game_step_sandbox(g, &cyclone);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_CYCLONE, 0));

    const BrushInput gale = {
        .pmx = 640.0f, .pmy = 360.0f, .mx = 640.0f, .my = 360.0f, .gale = true};
    const int32_t charges = g->player.gale_charges;
    TEST_ASSERT_TRUE(charges > 0);
    for (int32_t i = 0; i < charges; ++i) {
        game_step_sandbox(g, &gale);
        TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_GALE, 0));
        assert_ink_full(g);
    }
    game_step_sandbox(g, &gale);
    TEST_ASSERT_EQUAL_INT(0, count_events(g, EV_GALE, 0));
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_SKILL_DENIED, 0));
    assert_ink_full(g);

    RunConfig bought = cfg;
    bought.meta_upgrade_lv[META_UPG_INK_MAX] = 2;
    game_sandbox_apply_config(g, &bought);
    game_sandbox_set_brush(g, BRUSH_STYLE_QUILL);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 120.0f, g->player.ink_max);
    assert_ink_full(g);

    game_init(g, &cfg);
    const BrushInput run_stroke = {
        .pmx = 300.0f, .pmy = 300.0f, .mx = 400.0f, .my = 300.0f, .paint = true};
    game_step_sandbox(g, &run_stroke);
    TEST_ASSERT_TRUE(g->player.ink < g->player.ink_max);
    game_destroy(g);
}

static void test_gale_straight_180_reversal_snaps_aim(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    g->player.ink = g->player.ink_max;
    g->player.gale_charges = g->player.gale_max_charges;
    g->player.aim_x = 1.0f;
    g->player.aim_y = 0.0f;
    g->event_count = 0;

    const BrushInput rev = {.pmx = 500.0f, .pmy = 300.0f, .mx = 460.0f, .my = 300.0f, .gale = true};
    player_skills_step(g, &rev);

    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -1.0f, g->player.aim_x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, g->player.aim_y);
    bool saw_left_gale = false;
    for (int32_t i = 0; i < g->event_count; ++i) {
        if (g->events[i].type == EV_GALE && g->events[i].x2 < -0.99f) {
            saw_left_gale = true;
        }
    }
    TEST_ASSERT_TRUE(saw_left_gale);
    game_destroy(g);
}

static void test_high_speed_knocked_enemy_slams_into_torii_pillar_without_tunneling(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    stage_gimmicks_setup_act(g, 1);
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 368.0f, 328.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 384.0f, 328.0f));
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 398.0f, 328.0f));

    /* Light enemy (Umbrella) at 1800 px/s must slam and die on the pillar instead of tunneling. */
    const EntityId umb = sturdy_enemy(g, ENEMY_UMBRELLA, 368.0f, 328.0f, 30);
    game_velocity_mut(g, umb)->vx = 1800.0f;
    game_velocity_mut(g, umb)->vy = 0.0f;
    game_status_mut(g, umb)->knockback_t = 1.0f;
    g->event_count = 0;

    combat_physics_and_slams_step(g);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_SLAM, 0));
    TEST_ASSERT_FALSE(ecs_is_alive(&g->ecs, umb));

    /* Heavy enemy (Stone Oni) at 1800 px/s must slam, take damage, and stay in front of pillar. */
    const EntityId oni = sturdy_enemy(g, ENEMY_STONE_ONI, 368.0f, 328.0f, 200);
    game_velocity_mut(g, oni)->vx = 1800.0f;
    game_velocity_mut(g, oni)->vy = 0.0f;
    game_status_mut(g, oni)->knockback_t = 1.0f;
    g->event_count = 0;

    combat_physics_and_slams_step(g);
    TEST_ASSERT_EQUAL_INT(1, count_events(g, EV_SLAM, 0));
    TEST_ASSERT_TRUE(ecs_is_alive(&g->ecs, oni));
    TEST_ASSERT_TRUE(game_health(g, oni)->hp < 200);
    TEST_ASSERT_TRUE(game_transform(g, oni)->x < 376.0f);
    game_destroy(g);
}

static void test_arrow_diagonal_corner_hits_torii_pillar(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    stage_gimmicks_setup_act(g, 1);
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 374.0f, 323.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 376.5f, 320.5f));
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 379.0f, 318.0f));

    const EntityId arrow = projectiles_spawn(g, PROJ_ARROW, 374.0f, 323.0f, 300.0f, -300.0f);
    projectiles_step(g);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_FALSE(ecs_is_alive(&g->ecs, arrow));
    game_destroy(g);
}

static void test_arrow_shallow_diagonal_corner_hits_torii_pillar(void)
{
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    stage_gimmicks_setup_act(g, 1);
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 373.6f, 322.8f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 376.2f, 320.2f));
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 377.9f, 318.5f));

    const EntityId arrow = projectiles_spawn(g, PROJ_ARROW, 373.6f, 322.8f, 300.0f, -300.0f);
    projectiles_step(g);
    ecs_flush_destroy(&g->ecs);
    TEST_ASSERT_FALSE(ecs_is_alive(&g->ecs, arrow));
    game_destroy(g);
}

static void test_nue_dash_halts_at_solid_torii_pillar(void)
{
    /* 1. Solid-pillar dash stop: lightning trail endpoint advances with Nue and ends at impact. */
    GameState *g = arena(BRUSH_STYLE_HEAVY);
    stage_gimmicks_setup_act(g, 1);
    TEST_ASSERT_FALSE(fluid_is_solid(&g->fluid, 348.0f, 326.0f));
    TEST_ASSERT_TRUE(fluid_is_solid(&g->fluid, 384.0f, 328.0f));

    const EntityId nue = sturdy_enemy(g, ENEMY_BOSS_NUE, 348.0f, 326.0f, 700);
    EnemyComp *en = game_enemy_mut(g, nue);
    en->ai_state = 0;
    en->phase = 2; /* Waypoint (1080, 360) crosses the left Torii pillar at (376..392, 320..336) */
    en->windup = 0.0f;
    en->fire_cd = 0.0f;

    enemies_ai_step(g);
    stage_gimmicks_step(g);
    TEST_ASSERT_EQUAL_INT32(2, en->ai_state);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 348.0f, g->internal.nue_trail_x[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 326.0f, g->internal.nue_trail_y[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 348.0f, g->internal.nue_trail_x[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 326.0f, g->internal.nue_trail_y[0][1]);

    for (int t = 0; t < 16 && en->ai_state == 2; ++t) {
        enemies_ai_step(g);
        stage_gimmicks_step(g);
    }
    const TransformComp *tr = game_transform(g, nue);
    TEST_ASSERT_EQUAL_INT32(1, en->ai_state);
    TEST_ASSERT_TRUE(tr->x < 376.0f);
    TEST_ASSERT_TRUE(g->internal.nue_trail_t[0] > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, tr->x, g->internal.nue_trail_x[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, tr->y, g->internal.nue_trail_y[0][1]);
    float beyond_pillar_dye[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(&g->fluid, 600.0f, 340.0f, NULL, NULL, beyond_pillar_dye);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, beyond_pillar_dye[2]);
    game_destroy(g);

    /* 2. Slowed/timed-out dash: trail endpoint matches Nue's actual stop, never the far target. */
    g = arena(BRUSH_STYLE_HEAVY);
    const EntityId slow_nue = sturdy_enemy(g, ENEMY_BOSS_NUE, 200.0f, 360.0f, 700);
    EnemyComp *sen = game_enemy_mut(g, slow_nue);
    StatusComp *sst = game_status_mut(g, slow_nue);
    sen->ai_state = 0;
    sen->phase = 2; /* Waypoint (1080, 360), 880 px away: 600 * 0.66 * 1.5s = 594 px < 880 px */
    sen->windup = 0.0f;
    sen->fire_cd = 0.0f;
    sst->ink_mark_t = 3.0f;

    enemies_ai_step(g);
    stage_gimmicks_step(g);
    TEST_ASSERT_EQUAL_INT32(2, sen->ai_state);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 200.0f, g->internal.nue_trail_x[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 360.0f, g->internal.nue_trail_y[0][1]);

    for (int t = 0; t < 120 && sen->ai_state == 2; ++t) {
        sst->ink_mark_t = 3.0f;
        enemies_ai_step(g);
        stage_gimmicks_step(g);
    }
    const TransformComp *str = game_transform(g, slow_nue);
    TEST_ASSERT_EQUAL_INT32(1, sen->ai_state);
    TEST_ASSERT_TRUE(str->x > 700.0f && str->x < 900.0f);
    TEST_ASSERT_TRUE(g->internal.nue_trail_t[0] > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, str->x, g->internal.nue_trail_x[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, str->y, g->internal.nue_trail_y[0][1]);
    float unreached_dye[FLUID_MAX_CHANNELS] = {0};
    fluid_sample(&g->fluid, 1060.0f, 360.0f, NULL, NULL, unreached_dye);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, unreached_dye[2]);
    game_destroy(g);
}

static void test_act3_boss_hitstop_and_aftermath_reach_victory(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_act(g, 2);
    for (int t = 0; t < 240 && g->phase == RUN_PHASE_ACT_TRANSITION; ++t) {
        game_step(g, NULL, NULL);
    }
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    for (int s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
        (void)combat_apply_damage(g, g->ryu_body.orb_ids[s], 500, DMG_PHYSICAL, 0.0f, 0.0f);
    }
    ecs_flush_destroy(&g->ecs);

    const EntityId head = g->run.boss_id;
    game_health_mut(g, head)->invuln_t = 0.0f;
    game_health_mut(g, head)->hp = 10;
    (void)combat_apply_damage(g, head, 100, DMG_PHYSICAL, 0.0f, 0.0f);
    g->hitstop_t = 0.05f;
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    TEST_ASSERT_TRUE(game_wave_goal_met(g));
    TEST_ASSERT_FALSE(game_aftermath_reaches_victory(g));

    for (int t = 0; t < 12 && g->phase == RUN_PHASE_PLAYING && game_wave_goal_met(g); ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_WAVE_AFTERMATH, g->phase);
    TEST_ASSERT_TRUE(game_aftermath_reaches_victory(g));

    for (int t = 0; t < 90 && game_aftermath_reaches_victory(g); ++t) {
        game_step(g, NULL, NULL);
    }
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_VICTORY, g->phase);
    TEST_ASSERT_FALSE(game_aftermath_reaches_victory(g));
    game_destroy(g);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_maelstrom_bind_collision_damage_waits_for_hit_cooldown);
    RUN_TEST(test_recycled_slot_starts_without_maelstrom_state);
    RUN_TEST(test_piercing_burning_shot_hits_each_target_once);
    RUN_TEST(test_piercing_shot_homes_on_fresh_targets_after_a_hit);
    RUN_TEST(test_gale_reaches_slam_speed_for_heavy_and_fanned_targets);
    RUN_TEST(test_undeflected_shot_passing_shrine_stays_hostile_without_fluid);
    RUN_TEST(test_barrier_ward_is_kept_while_shrine_is_invulnerable);
    RUN_TEST(test_rich_ink_lv3_drops_orb_on_secondary_kill);
    RUN_TEST(test_reroll_refuses_when_no_new_choice_exists);
    RUN_TEST(test_autoplay_picks_when_reroll_would_be_refused);
    RUN_TEST(test_reroll_brings_in_the_unseen_choice);
    RUN_TEST(test_nue_knockback_holds_dash_and_stun_bounce_moves);
    RUN_TEST(test_deflected_fireball_deals_fire_damage);
    RUN_TEST(test_wind_return_repels_approaching_walker);
    RUN_TEST(test_met_quota_blocks_shrine_damage_and_spawns);
    RUN_TEST(test_wave_clear_purifies_each_remaining_enemy);
    RUN_TEST(test_final_kill_hitstop_plays_before_the_clear);
    RUN_TEST(test_gale_cooldown_clamps_to_shortened_total);
    RUN_TEST(test_wave_start_clears_lingering_field_state);
    RUN_TEST(test_ryu_orbs_start_on_screen_and_offscreen_parts_are_untargetable);
    RUN_TEST(test_stroke_force_keeps_direction_and_scales_cap_with_brush_force);
    RUN_TEST(test_long_ember_stroke_keeps_every_ember_and_huge_input_stays_finite);
    RUN_TEST(test_lobby_sandbox_paints_with_combat_brush_and_no_waves);
    RUN_TEST(test_lobby_sandbox_never_runs_dry_but_keeps_skill_cooldowns);
    RUN_TEST(test_gale_straight_180_reversal_snaps_aim);
    RUN_TEST(test_high_speed_knocked_enemy_slams_into_torii_pillar_without_tunneling);
    RUN_TEST(test_arrow_diagonal_corner_hits_torii_pillar);
    RUN_TEST(test_arrow_shallow_diagonal_corner_hits_torii_pillar);
    RUN_TEST(test_nue_dash_halts_at_solid_torii_pillar);
    RUN_TEST(test_act3_boss_hitstop_and_aftermath_reach_victory);
    return UNITY_END();
}
