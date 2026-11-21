#include "game.h"

#include "core.h"

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
    return in != NULL && isfinite(in->mx) && isfinite(in->my) && isfinite(in->pmx) &&
           isfinite(in->pmy);
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
    /* Zero with memset to avoid materializing a >1 MB temporary on the stack. */
    memset(g, 0, sizeof *g);
    g->config = cfg != NULL ? *cfg : meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(1));
    g->rng = g->config.seed;

    fluid_init(&g->fluid, NULL);
    ecs_init(&g->ecs);

    g->phase = RUN_PHASE_ACT_TRANSITION;
    g->phase_t = 0.0f;
    g->run.act_index = 0;
    g->run.act_count = 1;
    g->run.boss_id = (EntityId){ECS_INVALID_INDEX, 0};

    const uint8_t hp_lv = clamp_meta_lv(&g->config, META_UPG_SHRINE_HP);
    g->shrine_count = 1;
    g->shrines[0] = (ShrineState){
        .x = 640.0f,
        .y = 360.0f,
        .hit_radius_proj = 32.0f,
        .hit_radius_contact = 42.0f,
        .hp = 8 + (int32_t)hp_lv,
        .max_hp = 8 + (int32_t)hp_lv,
        .invuln_t = 0.0f,
    };

    const uint8_t ink_lv = clamp_meta_lv(&g->config, META_UPG_INK_MAX);
    g->player.ink_max = 100.0f + 10.0f * (float)ink_lv;
    g->player.ink = g->player.ink_max;

    const uint8_t reroll_lv = clamp_meta_lv(&g->config, META_UPG_REROLLS);
    g->run.rerolls_left = 1 + (int32_t)reroll_lv;
}

void game_step(GameState *g, const BrushInput *in, const GameCommand *cmd)
{
    if (g == NULL) {
        return;
    }
    g->event_count = 0;

    const BrushInput *valid_in = brush_input_is_finite(in) ? in : NULL;
    const GameCommandType cmd_type =
        (cmd != NULL && (unsigned)cmd->type <= (unsigned)CMD_ABORT_RUN) ? cmd->type : CMD_NONE;

    if (cmd_type == CMD_ABORT_RUN && g->phase != RUN_PHASE_VICTORY &&
        g->phase != RUN_PHASE_GAME_OVER) {
        g->phase = RUN_PHASE_GAME_OVER;
        g->phase_t = 0.0f;
        const float sx = g->shrine_count > 0 ? g->shrines[0].x : 640.0f;
        const float sy = g->shrine_count > 0 ? g->shrines[0].y : 360.0f;
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_GAME_OVER,
                                     .x = sx,
                                     .y = sy,
                                     .value = 0,
                                     .flags = EVF_RUN_ABORTED,
                                 });
    }

    if (g->phase == RUN_PHASE_REWARD || g->phase == RUN_PHASE_VICTORY ||
        g->phase == RUN_PHASE_GAME_OVER) {
        return;
    }

    if (g->phase == RUN_PHASE_ACT_TRANSITION) {
        g->phase_t += INK_FIXED_DT;
        if (g->phase_t + 0.5f * INK_FIXED_DT >= 2.0f) {
            g->phase = RUN_PHASE_PLAYING;
            g->phase_t = 0.0f;
        }
    } else if (g->phase == RUN_PHASE_PLAYING) {
        g->phase_t += INK_FIXED_DT;
        g->player.painting = false;
        if (valid_in != NULL) {
            g->player.cursor_x = valid_in->mx;
            g->player.cursor_y = valid_in->my;
            if (valid_in->paint) {
                const float mid_x = 0.5f * (valid_in->pmx + valid_in->mx);
                const float mid_y = 0.5f * (valid_in->pmy + valid_in->my);
                const float fx = ink_clampf((valid_in->mx - valid_in->pmx) * 0.58f, -46.0f, 46.0f);
                const float fy = ink_clampf((valid_in->my - valid_in->pmy) * 0.58f, -46.0f, 46.0f);
                static const float ink_dye[FLUID_MAX_CHANNELS] = {0.4f, 0.0f, 0.0f, 0.0f};
                fluid_splat(&g->fluid, mid_x, mid_y, 3.7f, fx, fy, ink_dye);
                g->player.painting = true;
            }
        }
        fluid_step(&g->fluid, INK_FIXED_DT);
        fluid_sanitize_channels(&g->fluid);
    } else {
        g->phase_t += INK_FIXED_DT;
    }

    ecs_flush_destroy(&g->ecs);
    ++g->ticks;
}
