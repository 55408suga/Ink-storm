#include "game.h"

#include "combat/damage.h"
#include "core.h"
#include "elements/elements.h"
#include "enemies/enemy_defs.h"
#include "enemies/projectiles.h"
#include "player/player_skills.h"
#include "roguelike/hints.h"
#include "roguelike/rewards.h"
#include "roguelike/stage_gimmicks.h"
#include "roguelike/upgrades.h"
#include "roguelike/waves.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint8_t clamp_meta_lv(const RunConfig *cfg, MetaUpgradeKind kind)
{
    const uint8_t max_lv = meta_upgrade_max_level(kind);
    const uint8_t lv = cfg->meta_upgrade_lv[kind];
    return lv > max_lv ? max_lv : lv;
}

static bool brush_input_is_finite(const BrushInput *in)
{
    if (in == NULL || !isfinite(in->mx) || !isfinite(in->my) || !isfinite(in->pmx) ||
        !isfinite(in->pmy)) {
        return false;
    }
    if (in->has_cyclone_pos && (!isfinite(in->cyclone_x) || !isfinite(in->cyclone_y))) {
        return false;
    }
    return true;
}

/* Finite but huge coordinates (e.g. +-FLT_MAX from the API) would overflow stroke lengths into
 * Inf/NaN downstream; one screen of slack around the arena keeps every real pointer position. */
static const BrushInput *bounded_brush_input(const BrushInput *in, BrushInput *out)
{
    if (!brush_input_is_finite(in)) {
        return NULL;
    }
    const float lo_x = -(float)INK_SCREEN_W;
    const float hi_x = 2.0f * (float)INK_SCREEN_W;
    const float lo_y = -(float)INK_SCREEN_H;
    const float hi_y = 2.0f * (float)INK_SCREEN_H;
    *out = *in;
    out->mx = ink_clampf(in->mx, lo_x, hi_x);
    out->my = ink_clampf(in->my, lo_y, hi_y);
    out->pmx = ink_clampf(in->pmx, lo_x, hi_x);
    out->pmy = ink_clampf(in->pmy, lo_y, hi_y);
    if (in->has_cyclone_pos) {
        out->cyclone_x = ink_clampf(in->cyclone_x, lo_x, hi_x);
        out->cyclone_y = ink_clampf(in->cyclone_y, lo_y, hi_y);
    }
    return out;
}

static void step_dissolving_entities(GameState *g)
{
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index == ECS_INVALID_INDEX) {
            continue;
        }
        TransformComp *tr = game_transform_mut(g, id);
        if (tr == NULL || !tr->dissolving) {
            continue;
        }
        tr->dissolve_t += INK_FIXED_DT;
        if (tr->dissolve_t + 0.5f * INK_FIXED_DT >= 0.6f) {
            ecs_destroy(&g->ecs, id);
        }
    }
}

static void handle_command(GameState *g, const GameCommand *cmd)
{
    if (cmd == NULL || (unsigned)cmd->type > (unsigned)CMD_ABORT_RUN) {
        return;
    }
    switch (cmd->type) {
    case CMD_PICK_REWARD:
        (void)rewards_pick_reward(g, cmd->arg);
        break;
    case CMD_REROLL_REWARD:
        (void)rewards_reroll(g);
        break;
    case CMD_REPAIR_SHRINE:
        (void)rewards_repair_shrine(g);
        break;
    case CMD_PICK_ENDLESS_CRYSTAL:
        (void)rewards_pick_endless_crystal(g);
        break;
    case CMD_START_ENDLESS:
        if (g->phase == RUN_PHASE_VICTORY && !g->run.run_settled && g->run.endless_loop == 0) {
            if (g->shrine_count > 0) {
                ShrineState *sh = &g->shrines[0];
                sh->hp += 3;
                if (sh->hp > sh->max_hp) {
                    sh->hp = sh->max_hp;
                }
            }
            g->run.endless_loop = 1;
            g->internal.wave_clear_is_boss = true;
            rewards_generate_relic_offer(g);
            g->phase = RUN_PHASE_REWARD;
            g->phase_t = 0.0f;
        }
        break;
    case CMD_ABORT_RUN:
        if (g->phase != RUN_PHASE_VICTORY && g->phase != RUN_PHASE_GAME_OVER) {
            g->phase = RUN_PHASE_GAME_OVER;
            g->phase_t = 0.0f;
            const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
            const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
            (void)game_push_event(g, (GameEvent){
                                         .type = EV_GAME_OVER,
                                         .x = sx,
                                         .y = sy,
                                         .value = 0,
                                         .flags = EVF_RUN_ABORTED,
                                     });
        }
        break;
    case CMD_NONE:
    default:
        break;
    }
}

bool game_aftermath_reaches_victory(const GameState *g)
{
    if (g == NULL || g->phase != RUN_PHASE_WAVE_AFTERMATH || !g->internal.wave_clear_is_boss) {
        return false;
    }
    const bool reached_main_victory =
        (g->run.endless_loop == 0 && g->run.act_index + 1 >= g->run.act_count);
    const bool reached_endless_cap = (g->run.act_index + 1 >= META_MAX_BEST_ACT);
    return reached_main_victory || reached_endless_cap;
}

static void step_pending_impulses(GameState *g)
{
    int32_t count = g->pending_impulse_count;
    if (count > GAME_MAX_PENDING_IMPULSE) {
        count = GAME_MAX_PENDING_IMPULSE;
    }
    for (int32_t i = 0; i < count; ++i) {
        const PendingImpulse *imp = &g->pending_impulses[i];
        fluid_add_radial_impulse(&g->fluid, imp->x, imp->y, imp->rad_cells, imp->peak_speed);
    }
    g->pending_impulse_count = 0;
}

/* Chain and Surge timers stay frozen here: the purification window is not player time. */
static void step_aftermath(GameState *g)
{
    g->player.painting = false;
    g->player.stroke_speed = 0.0f;

    step_pending_impulses(g);
    fluid_step(&g->fluid, INK_FIXED_DT);
    fluid_sanitize_channels(&g->fluid);
    step_dissolving_entities(g);
    ecs_flush_destroy(&g->ecs);

    g->phase_t += INK_FIXED_DT;
    if (g->phase_t + 0.5f * INK_FIXED_DT >= 1.0f) {
        if (game_aftermath_reaches_victory(g)) {
            g->phase = RUN_PHASE_VICTORY;
            g->phase_t = 0.0f;
            (void)game_push_event(g, (GameEvent){
                                         .type = EV_VICTORY,
                                         .value = g->run.act_index,
                                         .flags = 0,
                                     });
        } else if (g->internal.wave_clear_is_boss) {
            rewards_generate_relic_offer(g);
            g->phase = RUN_PHASE_REWARD;
            g->phase_t = 0.0f;
        } else {
            rewards_generate_offer(g);
            g->phase = RUN_PHASE_REWARD;
            g->phase_t = 0.0f;
        }
    }
    ++g->ticks;
}

/* Shrine break has already been ruled out by the caller. A winning tick that also started a
 * hitstop (e.g. a final wall slam) defers this until the freeze ends, so the clear banner,
 * purification and its sound play after the impact instead of during it. */
static void finish_wave_if_won(GameState *g)
{
    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;
    if (g->run.wave_index == GAME_BOSS_WAVE_INDEX && g->run.boss_id.index == ECS_INVALID_INDEX) {
        if (g->run.act_index < GAME_MAIN_ACT_COUNT) {
            ++g->run.acts_cleared;
        } else if (g->run.endless_acts_cleared < META_MAX_ENDLESS_ACTS) {
            ++g->run.endless_acts_cleared;
        }
        g->internal.wave_clear_is_boss = true;
        if ((g->run.act_index + 1 < g->run.act_count || g->run.endless_loop > 0) &&
            g->shrine_count > 0) {
            ShrineState *sh = &g->shrines[0];
            sh->hp += 3;
            if (sh->hp > sh->max_hp) {
                sh->hp = sh->max_hp;
            }
        }
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_ACT_CLEAR,
                                     .x = g->internal.last_boss_x,
                                     .y = g->internal.last_boss_y,
                                     .value = g->run.act_index,
                                     .flags = 0,
                                 });
        combat_begin_wave_aftermath(g);
        return;
    }
    if (g->run.wave_index < GAME_BOSS_WAVE_INDEX && g->run.wave_kills >= g->run.wave_quota) {
        g->internal.wave_clear_is_boss = false;
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_WAVE_CLEAR,
                                     .x = sx,
                                     .y = sy,
                                     .value = g->run.wave_index,
                                     .flags = 0,
                                 });
        combat_begin_wave_aftermath(g);
    }
}

GameState *game_create(const RunConfig *cfg)
{
    GameState *g = calloc(1, sizeof *g);
    if (g != NULL) {
        game_init(g, cfg);
    }
    return g;
}

void game_destroy(GameState *g) { free(g); }

void game_init(GameState *g, const RunConfig *cfg)
{
    if (g == NULL) {
        return;
    }
    /* Copy cfg first so game_init(g, &g->config) preserves the caller's config. */
    const RunConfig cfg_copy =
        cfg != NULL ? *cfg : meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(1));
    /* Zero with memset to avoid materializing a >1 MB temporary on the stack. */
    memset(g, 0, sizeof *g);
    g->config = cfg_copy;
    g->rng = g->config.seed;

    fluid_init(&g->fluid, NULL);
    ecs_init(&g->ecs);

    g->phase = RUN_PHASE_ACT_TRANSITION;
    g->phase_t = 0.0f;
    g->run.act_index = 0;
    g->run.act_count = GAME_MAIN_ACT_COUNT;
    g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};
    g->ryu_body.head_id = (EntityId){ECS_INVALID_INDEX, 0};
    for (int i = 0; i < GAME_RYU_ORB_COUNT; ++i) {
        g->ryu_body.orb_ids[i] = (EntityId){ECS_INVALID_INDEX, 0};
    }
    g->ryu_body.active_orb_slot = -1;

    int32_t loop = g->config.endless_loop;
    if (loop < 0) {
        loop = 0;
    } else if (loop > ENDLESS_MAX_LOOP_N) {
        loop = ENDLESS_MAX_LOOP_N;
    }
    if (loop > 0) {
        g->run.endless_loop = loop;
        g->run.act_index = loop * GAME_MAIN_ACT_COUNT;
        g->run.acts_cleared = GAME_MAIN_ACT_COUNT;
    }

    const uint8_t hp_lv = clamp_meta_lv(&g->config, META_UPG_SHRINE_HP);
    g->shrine_count = 1;
    g->shrines[0] = (ShrineState){
        .x = (float)INK_SCREEN_W * 0.5f,
        .y = (float)INK_SCREEN_H * 0.5f,
        .hit_radius_proj = 32.0f,
        .hit_radius_contact = 42.0f,
        .hp = 8 + (int32_t)hp_lv,
        .max_hp = 8 + (int32_t)hp_lv,
        .invuln_t = 0.0f,
    };

    const uint8_t reroll_lv = clamp_meta_lv(&g->config, META_UPG_REROLLS);
    g->run.rerolls_left = 1 + (int32_t)reroll_lv;

    g->player.cursor_x = g->shrines[0].x;
    g->player.cursor_y = g->shrines[0].y;
    roguelike_recompute_modifiers(g);
    g->player.gale_max_charges = g->internal.mods.gale_max_charges;
    g->player.gale_charges = g->player.gale_max_charges;
    g->player.ink = g->player.ink_max;
}

void game_init_sandbox(GameState *g, const RunConfig *cfg)
{
    if (g == NULL) {
        return;
    }
    game_init(g, cfg);
    g->phase = RUN_PHASE_PLAYING;
    g->phase_t = 0.0f;
    g->internal.act_prepared = true;
    g->internal.sandbox_infinite_ink = true;
    player_skills_refill_sandbox_ink(g);
}

void game_sandbox_set_brush(GameState *g, BrushStyleId brush)
{
    if (g == NULL || (unsigned)brush >= (unsigned)BRUSH_STYLE_COUNT ||
        g->config.brush_style == brush) {
        return;
    }
    g->config.brush_style = brush;
    g->internal.ember_tip_dist = 0.0f;
    roguelike_recompute_modifiers(g);
    player_skills_refill_sandbox_ink(g);
}

void game_sandbox_apply_config(GameState *g, const RunConfig *cfg)
{
    if (g == NULL || cfg == NULL) {
        return;
    }
    g->config = *cfg;
    g->internal.ember_tip_dist = 0.0f;
    roguelike_recompute_modifiers(g);
    player_skills_refill_sandbox_ink(g);
}

void game_step_sandbox(GameState *g, const BrushInput *in)
{
    if (g == NULL) {
        return;
    }
    g->event_count = 0;
    BrushInput bounded;
    const BrushInput *valid_in = bounded_brush_input(in, &bounded);
    step_pending_impulses(g);
    player_skills_step(g, valid_in);
    if (!fluid_is_pristine(&g->fluid)) {
        fluid_step(&g->fluid, INK_FIXED_DT);
        elements_step_post_solve(g);
        fluid_sanitize_channels(&g->fluid);
    } else {
        elements_step_idle_cooldowns(g);
    }
    ecs_flush_destroy(&g->ecs);
    g->phase_t += INK_FIXED_DT;
    ++g->ticks;
}

void game_step(GameState *g, const BrushInput *in, const GameCommand *cmd)
{
    if (g == NULL) {
        return;
    }

    /* Step 1: Event clear, input validation, command handling, stopping-phase branch. */
    g->event_count = 0;
    BrushInput bounded;
    const BrushInput *valid_in = bounded_brush_input(in, &bounded);
    handle_command(g, cmd);

    if (g->phase == RUN_PHASE_REWARD || g->phase == RUN_PHASE_VICTORY ||
        g->phase == RUN_PHASE_GAME_OVER) {
        return;
    }

    if (g->phase == RUN_PHASE_SHRINE_BREAK) {
        g->phase_t += INK_FIXED_DT;
        if (g->phase_t + 0.5f * INK_FIXED_DT >= 1.5f) {
            g->phase = RUN_PHASE_GAME_OVER;
            g->phase_t = 0.0f;
        }
        ++g->ticks;
        return;
    }

    if (g->phase == RUN_PHASE_ACT_TRANSITION) {
        if (!g->internal.act_prepared) {
            waves_prepare_act(g);
        }
        g->phase_t += INK_FIXED_DT;
        if (g->phase_t + 0.5f * INK_FIXED_DT >= 2.0f) {
            waves_start_wave(g, 0);
        }
        ecs_flush_destroy(&g->ecs);
        ++g->ticks;
        return;
    }

    if (g->phase == RUN_PHASE_WAVE_AFTERMATH) {
        step_aftermath(g);
        return;
    }

    /* Step 2: Hitstop (ticks and phase_t advance; steps 3..8 skipped while active). A clear that
     * was deferred by the hitstop resolves on the tick the freeze ends. */
    g->phase_t += INK_FIXED_DT;
    if (g->hitstop_t > 0.0f) {
        g->hitstop_t -= INK_FIXED_DT;
        if (g->hitstop_t <= 1e-4f) {
            g->hitstop_t = 0.0f;
            if (game_wave_goal_met(g)) {
                finish_wave_if_won(g);
            }
        }
        ++g->ticks;
        return;
    }

    /* Step 3: Pending impulses & player skills / ink economy. */
    step_pending_impulses(g);
    player_skills_step(g, valid_in);

    /* Step 4: Fluid simulation step. */
    fluid_step(&g->fluid, INK_FIXED_DT);

    /* Step 5: Elemental reactions & channel sanitization. */
    elements_step_post_solve(g);
    fluid_sanitize_channels(&g->fluid);

    /* Step 6: Stage gimmicks & wave reinforcement spawns. */
    stage_gimmicks_step(g);
    waves_step_spawns(g);

    /* Step 7: ECS combat systems (ascending index order). */
    enemies_ai_step(g);
    projectiles_step(g);
    combat_physics_and_slams_step(g);
    combat_orbs_step(g);
    upgrades_fire_on_tick(g, INK_FIXED_DT);
    hints_check_tick(g);

    /* Step 8: Flush deferred destroys and evaluate transition priority. */
    ecs_flush_destroy(&g->ecs);
    if (g->run.boss_id.index != ECS_INVALID_INDEX && !ecs_is_alive(&g->ecs, g->run.boss_id)) {
        g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};
    }

    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;

    if (g->shrine_count > 0 && g->shrines[0].hp <= 0) {
        g->phase = RUN_PHASE_SHRINE_BREAK;
        g->phase_t = 0.0f;
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_GAME_OVER,
                                     .x = sx,
                                     .y = sy,
                                     .value = 0,
                                     .flags = 0,
                                 });
    } else if (g->hitstop_t <= 0.0f) {
        finish_wave_if_won(g);
    }

    ++g->ticks;
}
