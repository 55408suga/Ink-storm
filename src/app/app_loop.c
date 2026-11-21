#include "app_loop.h"

#include "audio_out.h"
#include "core.h"
#include "frame_clock.h"
#include "input_latch.h"
#include "input_map.h"
#include "paths.h"
#include "raylib.h"
#include "save.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MetaState meta;
    char save_path[PATH_MAX];
    bool save_enabled;
    UiView ui;
} AppRunContext;

static uint64_t next_run_seed(const MetaState *meta)
{
    uint64_t s = UINT64_C(0x494e4b53544f524d) ^ (uint64_t)meta->runs_played;
    return ink_rng_next(&s);
}

static void start_new_run(AppRunContext *ctx, GameState *game, RenderState *render)
{
    const uint64_t seed = next_run_seed(&ctx->meta);
    const RunConfig cfg = meta_build_run_config(&ctx->meta, BRUSH_STYLE_HEAVY, seed);
    game_init(game, &cfg);
    render_reset_vfx(render, cfg.seed);
    ctx->ui.settled = false;
    ctx->ui.settle = (MetaSettleBreakdown){0};
    ctx->ui.hover_choice = UI_HOVER_NONE;
}

static void persist_meta(const AppRunContext *ctx)
{
    if (!ctx->save_enabled || ctx->save_path[0] == '\0') {
        return;
    }
    if (!save_write(ctx->save_path, &ctx->meta, NULL)) {
        fprintf(stderr, "INKSTORM: failed to write save file '%s'\n", ctx->save_path);
    }
}

static void app_after_tick(AppRunContext *ctx, GameState *game)
{
    if ((game->phase == RUN_PHASE_VICTORY || game->phase == RUN_PHASE_GAME_OVER) &&
        !game->run.run_settled) {
        MetaSettleBreakdown br = {0};
        if (meta_settle_run(&ctx->meta, game, &br)) {
            ctx->ui.settled = true;
            ctx->ui.settle = br;
            ctx->ui.crystals_after = ctx->meta.ink_crystals;
            persist_meta(ctx);
        }
    }
}

static void app_finish_run_on_exit(AppRunContext *ctx, GameState *game)
{
    if (!ctx->save_enabled) {
        return;
    }
    if (game->phase == RUN_PHASE_VICTORY || game->phase == RUN_PHASE_GAME_OVER) {
        return;
    }
    if (game->run.act_index == 0 && game->phase == RUN_PHASE_ACT_TRANSITION) {
        return;
    }
    const GameCommand abort_cmd = {.type = CMD_ABORT_RUN, .arg = 0};
    game_step(game, NULL, &abort_cmd);
    app_after_tick(ctx, game);
}

static void init_save_context(AppRunContext *ctx, const CliOptions *opts)
{
    meta_init_default(&ctx->meta);
    ctx->save_path[0] = '\0';
    ctx->save_enabled = (opts->save_mode != SAVE_MODE_DISABLED);
    if (!ctx->save_enabled) {
        return;
    }

    char exe_dir[PATH_MAX];
    const char *exe_arg = app_executable_dir(exe_dir, sizeof exe_dir) ? exe_dir : NULL;
    const char *explicit_p = opts->save_mode == SAVE_MODE_EXPLICIT ? opts->save_path : NULL;
    if (!save_resolve_path(explicit_p, getenv("XDG_DATA_HOME"), getenv("HOME"), exe_arg,
                           ctx->save_path, sizeof ctx->save_path)) {
        fputs("INKSTORM: unable to resolve save path; running without save\n", stderr);
        ctx->save_enabled = false;
        return;
    }

    const SaveLoadResult res = save_load(ctx->save_path, &ctx->meta);
    if (res == SAVE_LOAD_CORRUPT || res == SAVE_LOAD_IO_ERROR) {
        fprintf(stderr, "INKSTORM: save file '%s' unreadable (%s); using defaults\n",
                ctx->save_path, res == SAVE_LOAD_CORRUPT ? "corrupt" : "io error");
    }
}

int app_run_window_loop(GameState *game, AudioState *audio, RenderState *render,
                        const CliOptions *opts, uint32_t fault_mask)
{
    AppRunContext ctx;
    memset(&ctx, 0, sizeof ctx);
    init_save_context(&ctx, opts);

    if (opts->smoke_frames > 0) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
    }
    InitWindow(INK_SCREEN_W, INK_SCREEN_H, "INKSTORM");
    if (!IsWindowReady()) {
        return EXIT_FAILURE;
    }
    SetTargetFPS(INK_TICK_HZ);

    char font_path[PATH_MAX];
    const bool has_font =
        app_resolve_asset_path("fonts/ui_mincho.ttf", font_path, sizeof font_path);
    const RenderOptions render_opts = {
        .use_post = opts->use_post && ctx.meta.settings.post_shader_on,
        .fault_mask = fault_mask,
        .font_path = has_font ? font_path : NULL,
    };
    render_init(render, &render_opts);

    ctx.ui.scene = UI_SCENE_RUN;
    ctx.ui.hover_choice = UI_HOVER_NONE;
    ctx.ui.shake_scale = ctx.meta.settings.shake_scale;

    start_new_run(&ctx, game, render);

    audio_init(audio, game->config.seed);
    audio_set_volumes(audio, ctx.meta.settings.master_vol, ctx.meta.settings.bgm_vol,
                      ctx.meta.settings.se_vol);

    AudioOut audio_out = {0};
    if (opts->smoke_frames == 0) {
        audio_out_init(&audio_out);
    }

    FrameClock clock;
    frame_clock_reset(&clock);

    const Vector2 init_mouse = GetMousePosition();
    InputLatch latch;
    input_latch_init(&latch, init_mouse.x, init_mouse.y);

    int remaining_frames = opts->smoke_frames;
    while (remaining_frames > 0 || !WindowShouldClose()) {
        if (remaining_frames > 0) {
            const BrushInput in = game_autoplay_input(game);
            const GameCommand cmd = game_autoplay_command(game);
            game_step(game, &in, &cmd);
            audio_consume_events(audio, game);
            render_consume_events(render, game);
            render_update_vfx(render, game, INK_FIXED_DT);
            app_after_tick(&ctx, game);
        } else {
            const RawInput raw = input_map_read(game, &ctx.ui);
            input_latch_feed(&latch, &raw);

            const int ticks = frame_clock_advance(&clock, (double)GetFrameTime());
            for (int t = 0; t < ticks; ++t) {
                const BrushInput in = input_latch_take_tick(&latch);
                const GameCommand cmd = input_latch_take_command(&latch, game->phase);
                game_step(game, &in, &cmd);
                audio_consume_events(audio, game);
                render_consume_events(render, game);
                render_update_vfx(render, game, INK_FIXED_DT);
                app_after_tick(&ctx, game);
            }

            if ((game->phase == RUN_PHASE_VICTORY || game->phase == RUN_PHASE_GAME_OVER) &&
                input_latch_take_confirm(&latch)) {
                start_new_run(&ctx, game, render);
            }
            audio_out_pump(&audio_out, audio);
        }

        render_draw(render, game, &ctx.ui);
        if (remaining_frames > 0 && --remaining_frames == 0) {
            break;
        }
    }

    if (opts->smoke_frames == 0) {
        app_finish_run_on_exit(&ctx, game);
        audio_out_close(&audio_out);
    }

    render_close(render);
    CloseWindow();
    return EXIT_SUCCESS;
}
