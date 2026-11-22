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
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    MetaState meta;
    char save_path[PATH_MAX];
    bool save_enabled;
    bool cli_use_post;
    bool quit_requested;
    UiView ui;

    /* First-time hint FIFO queue (2.0s each, non-blocking) */
    HintId hint_queue[HINT_COUNT];
    int32_t hint_head;
    int32_t hint_count;

    /* Lobby ambient fluid demo state */
    float lobby_prev_x, lobby_prev_y;
    bool lobby_prev_painting;
    uint64_t lobby_ticks;
} AppRunContext;

static void sync_meta_view(AppRunContext *ctx)
{
    ctx->ui.meta_view = ctx->meta;
    ctx->ui.shake_scale = ctx->meta.settings.shake_scale;
}

static uint64_t next_run_seed(const MetaState *meta)
{
    uint64_t s = UINT64_C(0x494e4b53544f524d) ^ (uint64_t)meta->runs_played;
    return ink_rng_next(&s);
}

static void clear_hint_queue(AppRunContext *ctx)
{
    ctx->hint_head = 0;
    ctx->hint_count = 0;
    ctx->ui.active_hint = HINT_STROKE;
    ctx->ui.active_hint_t = 0.0f;
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

static void start_new_run(AppRunContext *ctx, GameState *game, RenderState *render,
                          AudioState *audio, BrushStyleId brush)
{
    const uint64_t seed = next_run_seed(&ctx->meta);
    const RunConfig cfg = meta_build_run_config(&ctx->meta, brush, seed);
    game_init(game, &cfg);
    render_reset_vfx(render, cfg.seed);
    ctx->ui.scene = UI_SCENE_RUN;
    ctx->ui.selected_brush = cfg.brush_style;
    ctx->ui.paused = false;
    ctx->ui.pause_confirm = PAUSE_CONFIRM_NONE;
    ctx->ui.settled = false;
    ctx->ui.settle = (MetaSettleBreakdown){0};
    ctx->ui.hover_choice = UI_HOVER_NONE;
    clear_hint_queue(ctx);
    sync_meta_view(ctx);
    if (audio != NULL) {
        audio_set_paused(audio, false);
        audio_set_lobby_mode(audio, false);
    }
}

static void enter_lobby_scene(AppRunContext *ctx, GameState *game, RenderState *render,
                              AudioState *audio)
{
    ctx->ui.scene = UI_SCENE_TITLE_LOBBY;
    ctx->ui.paused = false;
    ctx->ui.pause_confirm = PAUSE_CONFIRM_NONE;
    ctx->ui.hover_choice = UI_HOVER_NONE;
    clear_hint_queue(ctx);
    fluid_reset(&game->fluid);
    render_reset_vfx(render, UINT64_C(1));
    sync_meta_view(ctx);
    if (audio != NULL) {
        audio_set_paused(audio, false);
        audio_set_lobby_mode(audio, true);
    }
}

static void consume_hint_events(AppRunContext *ctx, const GameState *game)
{
    const int32_t count = game->event_count > GAME_MAX_EVENTS ? GAME_MAX_EVENTS : game->event_count;
    bool marked_any = false;
    for (int32_t i = 0; i < count; ++i) {
        const GameEvent *ev = &game->events[i];
        if (ev->type != EV_HINT_TRIGGER) {
            continue;
        }
        const int32_t raw_id = ev->value;
        if (raw_id < 0 || raw_id >= HINT_COUNT) {
            continue;
        }
        if (!ctx->meta.settings.hints_on || meta_is_hint_seen(&ctx->meta, (uint32_t)raw_id)) {
            continue;
        }
        meta_mark_hint_seen(&ctx->meta, (uint32_t)raw_id);
        marked_any = true;
        if (ctx->hint_count < HINT_COUNT) {
            const int32_t tail = (ctx->hint_head + ctx->hint_count) % HINT_COUNT;
            ctx->hint_queue[tail] = (HintId)raw_id;
            ctx->hint_count++;
        }
    }
    if (marked_any) {
        sync_meta_view(ctx);
        persist_meta(ctx);
    }
}

static void step_hint_queue(AppRunContext *ctx, float dt)
{
    if (ctx->ui.active_hint_t > 0.0f) {
        ctx->ui.active_hint_t = fmaxf(0.0f, ctx->ui.active_hint_t - dt);
    }
    if (ctx->ui.active_hint_t <= 0.0f && ctx->hint_count > 0) {
        ctx->ui.active_hint = ctx->hint_queue[ctx->hint_head];
        ctx->hint_head = (ctx->hint_head + 1) % HINT_COUNT;
        ctx->hint_count--;
        ctx->ui.active_hint_t = 2.0f;
    }
}

static void app_after_tick(AppRunContext *ctx, GameState *game)
{
    consume_hint_events(ctx, game);
    if ((game->phase == RUN_PHASE_VICTORY || game->phase == RUN_PHASE_GAME_OVER) &&
        !game->run.run_settled) {
        MetaSettleBreakdown br = {0};
        if (meta_settle_run(&ctx->meta, game, &br)) {
            ctx->ui.settled = true;
            ctx->ui.settle = br;
            ctx->ui.crystals_after = ctx->meta.ink_crystals;
            sync_meta_view(ctx);
            persist_meta(ctx);
        }
    }
}

static bool is_active_run_scene(const UiView *ui)
{
    return ui->scene == UI_SCENE_RUN ||
           (ui->scene == UI_SCENE_SETTINGS && ui->settings_return_scene == UI_SCENE_RUN);
}

static void abort_current_run_if_started(AppRunContext *ctx, GameState *game)
{
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

static void app_finish_run_on_exit(AppRunContext *ctx, GameState *game)
{
    if (!ctx->save_enabled || !is_active_run_scene(&ctx->ui)) {
        return;
    }
    abort_current_run_if_started(ctx, game);
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

static void apply_settings_change(AppRunContext *ctx, RenderState *render, AudioState *audio,
                                  int32_t row, int32_t delta)
{
    SettingsState *st = &ctx->meta.settings;
    if (row == 0) {
        const int v = (int)st->master_vol + delta * 10;
        st->master_vol = (uint8_t)(v < 0 ? 0 : (v > 100 ? 100 : v));
    } else if (row == 1) {
        const int v = (int)st->bgm_vol + delta * 10;
        st->bgm_vol = (uint8_t)(v < 0 ? 0 : (v > 100 ? 100 : v));
    } else if (row == 2) {
        const int v = (int)st->se_vol + delta * 10;
        st->se_vol = (uint8_t)(v < 0 ? 0 : (v > 100 ? 100 : v));
    } else if (row == 3) {
        st->post_shader_on = !st->post_shader_on;
    } else if (row == 4) {
        if (delta > 0) {
            st->shake_scale = st->shake_scale == 0 ? 50 : (st->shake_scale == 50 ? 100 : 0);
        } else {
            st->shake_scale = st->shake_scale == 100 ? 50 : (st->shake_scale == 50 ? 0 : 100);
        }
    } else if (row == 5) {
        st->hints_on = !st->hints_on;
    }

    audio_set_volumes(audio, st->master_vol, st->bgm_vol, st->se_vol);
    render_set_post_enabled(render, ctx->cli_use_post && st->post_shader_on);
    sync_meta_view(ctx);
}

static void step_lobby_fluid_demo(AppRunContext *ctx, GameState *game, RenderState *render,
                                  const MenuInput *menu)
{
    ctx->lobby_ticks++;
    render->vfx_time += INK_FIXED_DT;

    /* Gentle autonomous ink swirl across the open right side of the washi canvas */
    const float t = (float)ctx->lobby_ticks * INK_FIXED_DT;
    const float px = 784.0f + cosf(t * 0.42f) * 240.0f;
    const float py = 368.0f + sinf(t * 0.65f) * 160.0f;
    const float vx = -sinf(t * 0.42f) * 16.0f;
    const float vy = cosf(t * 0.65f) * 14.0f;
    const float amb_dye[FLUID_MAX_CHANNELS] = {(ctx->lobby_ticks % 6u) == 0u ? 0.24f : 0.0f, 0.0f,
                                               0.0f, 0.0f};
    fluid_splat(&game->fluid, px, py, 5.5f, vx, vy, amb_dye);

    /* Interactive mouse painting on the Lobby canvas */
    if (ctx->ui.scene == UI_SCENE_TITLE_LOBBY && menu->lobby_paint) {
        float dvx = 0.0f;
        float dvy = 0.0f;
        if (ctx->lobby_prev_painting) {
            dvx = ink_clampf((menu->mouse_x - ctx->lobby_prev_x) * 0.55f, -38.0f, 38.0f);
            dvy = ink_clampf((menu->mouse_y - ctx->lobby_prev_y) * 0.55f, -38.0f, 38.0f);
        }
        const float paint_dye[FLUID_MAX_CHANNELS] = {0.55f, 0.0f, 0.0f, 0.0f};
        fluid_splat(&game->fluid, menu->mouse_x, menu->mouse_y, 4.0f, dvx, dvy, paint_dye);
        ctx->lobby_prev_x = menu->mouse_x;
        ctx->lobby_prev_y = menu->mouse_y;
        ctx->lobby_prev_painting = true;
    } else {
        ctx->lobby_prev_painting = false;
    }

    fluid_step(&game->fluid, INK_FIXED_DT);
    fluid_sanitize_channels(&game->fluid);
}

static void handle_menu_action(AppRunContext *ctx, GameState *game, RenderState *render,
                               AudioState *audio, FrameClock *clock, InputLatch *latch,
                               const MenuInput *menu)
{
    switch (menu->action) {
    case MENU_ACT_LOBBY_SORTIE:
        audio_trigger_ui_stamp(audio);
        ctx->ui.scene = UI_SCENE_BRUSH_SELECT;
        ctx->ui.hover_choice = UI_HOVER_NONE;
        break;
    case MENU_ACT_LOBBY_BOKUAN:
        audio_trigger_ui_stamp(audio);
        ctx->ui.scene = UI_SCENE_BOKUAN;
        ctx->ui.shop_cursor = 0;
        ctx->ui.hover_choice = UI_HOVER_NONE;
        sync_meta_view(ctx);
        break;
    case MENU_ACT_LOBBY_SETTINGS:
        audio_trigger_ui_stamp(audio);
        ctx->ui.settings_return_scene = UI_SCENE_TITLE_LOBBY;
        ctx->ui.scene = UI_SCENE_SETTINGS;
        ctx->ui.settings_cursor = 0;
        ctx->ui.hover_choice = UI_HOVER_NONE;
        sync_meta_view(ctx);
        break;
    case MENU_ACT_LOBBY_QUIT:
        ctx->quit_requested = true;
        break;
    case MENU_ACT_BRUSH_PICK:
        audio_trigger_ui_stamp(audio);
        start_new_run(ctx, game, render, audio, (BrushStyleId)menu->arg);
        frame_clock_reset(clock);
        input_latch_reset_for_ui_transition(latch, menu->mouse_x, menu->mouse_y, menu->left_down);
        break;
    case MENU_ACT_BACK:
        if (ctx->ui.scene == UI_SCENE_SETTINGS) {
            persist_meta(ctx);
            ctx->ui.scene = ctx->ui.settings_return_scene;
            ctx->ui.hover_choice = UI_HOVER_NONE;
        } else {
            ctx->ui.scene = UI_SCENE_TITLE_LOBBY;
            ctx->ui.hover_choice = UI_HOVER_NONE;
            sync_meta_view(ctx);
        }
        break;
    case MENU_ACT_SHOP_BUY_UPG:
        if (meta_buy_upgrade(&ctx->meta, (MetaUpgradeKind)menu->arg)) {
            audio_trigger_ui_stamp(audio);
            sync_meta_view(ctx);
            persist_meta(ctx);
        }
        break;
    case MENU_ACT_SHOP_BUY_RELIC:
        if (meta_buy_relic_unlock(&ctx->meta, (uint32_t)menu->arg)) {
            audio_trigger_ui_stamp(audio);
            sync_meta_view(ctx);
            persist_meta(ctx);
        }
        break;
    case MENU_ACT_SETTINGS_CHANGE:
        apply_settings_change(ctx, render, audio, menu->arg, menu->delta);
        break;
    case MENU_ACT_TOGGLE_PAUSE:
        ctx->ui.paused = true;
        ctx->ui.pause_cursor = 0;
        ctx->ui.pause_confirm = PAUSE_CONFIRM_NONE;
        ctx->ui.hover_choice = UI_HOVER_NONE;
        audio_set_paused(audio, true);
        break;
    case MENU_ACT_PAUSE_RESUME:
        ctx->ui.paused = false;
        ctx->ui.pause_confirm = PAUSE_CONFIRM_NONE;
        ctx->ui.hover_choice = UI_HOVER_NONE;
        audio_set_paused(audio, false);
        frame_clock_reset(clock);
        if (menu->ui_clicked) {
            input_latch_reset_for_ui_transition(latch, menu->mouse_x, menu->mouse_y,
                                                menu->left_down);
        }
        break;
    case MENU_ACT_PAUSE_RESTART:
        ctx->ui.pause_confirm = PAUSE_CONFIRM_RESTART;
        ctx->ui.confirm_cursor = 1;
        ctx->ui.hover_choice = UI_HOVER_NONE;
        break;
    case MENU_ACT_PAUSE_SETTINGS:
        ctx->ui.settings_return_scene = UI_SCENE_RUN;
        ctx->ui.scene = UI_SCENE_SETTINGS;
        ctx->ui.settings_cursor = 0;
        ctx->ui.hover_choice = UI_HOVER_NONE;
        sync_meta_view(ctx);
        break;
    case MENU_ACT_PAUSE_LOBBY:
        ctx->ui.pause_confirm = PAUSE_CONFIRM_LOBBY;
        ctx->ui.confirm_cursor = 1;
        ctx->ui.hover_choice = UI_HOVER_NONE;
        break;
    case MENU_ACT_CONFIRM_NO:
        ctx->ui.pause_confirm = PAUSE_CONFIRM_NONE;
        ctx->ui.hover_choice = UI_HOVER_NONE;
        break;
    case MENU_ACT_CONFIRM_YES: {
        const PauseConfirmKind kind = ctx->ui.pause_confirm;
        ctx->ui.pause_confirm = PAUSE_CONFIRM_NONE;
        abort_current_run_if_started(ctx, game);
        if (kind == PAUSE_CONFIRM_RESTART) {
            start_new_run(ctx, game, render, audio, ctx->ui.selected_brush);
            frame_clock_reset(clock);
            input_latch_reset_for_ui_transition(latch, menu->mouse_x, menu->mouse_y,
                                                menu->left_down);
        } else {
            enter_lobby_scene(ctx, game, render, audio);
            frame_clock_reset(clock);
        }
        break;
    }
    case MENU_ACT_RESULT_LOBBY:
        if (render->victory_scroll_t >= 0.75f) {
            enter_lobby_scene(ctx, game, render, audio);
            frame_clock_reset(clock);
        }
        break;
    case MENU_ACT_RESULT_RETRY:
        if (render->victory_scroll_t >= 0.75f) {
            start_new_run(ctx, game, render, audio, ctx->ui.selected_brush);
            frame_clock_reset(clock);
            input_latch_reset_for_ui_transition(latch, menu->mouse_x, menu->mouse_y,
                                                menu->left_down);
        }
        break;
    case MENU_ACT_NONE:
    default:
        break;
    }
}

int app_run_window_loop(GameState *game, AudioState *audio, RenderState *render,
                        const CliOptions *opts, uint32_t fault_mask)
{
    AppRunContext ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.cli_use_post = opts->use_post;
    init_save_context(&ctx, opts);

    if (opts->smoke_frames > 0) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
    }
    InitWindow(INK_SCREEN_W, INK_SCREEN_H, "INKSTORM");
    if (!IsWindowReady()) {
        return EXIT_FAILURE;
    }
    SetExitKey(KEY_NULL);
    SetTargetFPS(INK_TICK_HZ);

    char font_path[PATH_MAX];
    char title_font_path[PATH_MAX];
    const bool has_font =
        app_resolve_asset_path("fonts/ui_mincho.ttf", font_path, sizeof font_path);
    const bool has_title_font =
        app_resolve_asset_path("fonts/title_brush.ttf", title_font_path, sizeof title_font_path);
    const RenderOptions render_opts = {
        .use_post = opts->use_post,
        .fault_mask = fault_mask,
        .font_path = has_font ? font_path : NULL,
        .title_font_path = has_title_font ? title_font_path : NULL,
    };
    render_init(render, &render_opts);
    render_set_post_enabled(render, opts->use_post && ctx.meta.settings.post_shader_on);

    audio_init(audio, UINT64_C(1));
    audio_set_volumes(audio, ctx.meta.settings.master_vol, ctx.meta.settings.bgm_vol,
                      ctx.meta.settings.se_vol);

    ctx.ui.hover_choice = UI_HOVER_NONE;
    ctx.ui.selected_brush = BRUSH_STYLE_HEAVY;
    sync_meta_view(&ctx);

    if (opts->smoke_frames > 0) {
        start_new_run(&ctx, game, render, audio, BRUSH_STYLE_HEAVY);
    } else {
        enter_lobby_scene(&ctx, game, render, audio);
    }

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
    while (remaining_frames > 0 || (!WindowShouldClose() && !ctx.quit_requested)) {
        if (remaining_frames > 0) {
            const BrushInput in = game_autoplay_input(game);
            const GameCommand cmd = game_autoplay_command(game);
            game_step(game, &in, &cmd);
            audio_consume_events(audio, game);
            render_consume_events(render, game);
            render_update_vfx(render, game, INK_FIXED_DT);
            app_after_tick(&ctx, game);
            step_hint_queue(&ctx, INK_FIXED_DT);
        } else {
            const MenuInput menu = input_map_read_menu(game, &ctx.ui);
            handle_menu_action(&ctx, game, render, audio, &clock, &latch, &menu);

            if (ctx.quit_requested) {
                break;
            }

            if (ctx.ui.scene == UI_SCENE_RUN && !ctx.ui.paused) {
                const RawInput raw = input_map_read(game, &ctx.ui);
                input_latch_feed(&latch, &raw);

                const int ticks = frame_clock_advance(&clock, (double)GetFrameTime());
                for (int t = 0; t < ticks; ++t) {
                    const RunPhase prev_phase = game->phase;
                    const BrushInput in = input_latch_take_tick(&latch);
                    GameCommand cmd = input_latch_take_command(&latch, game->phase);
                    if (game->phase == RUN_PHASE_REWARD &&
                        (render->reward_anim_t < 0.75f || render->reward_reroll_t > 0.0f)) {
                        cmd = (GameCommand){.type = CMD_NONE, .arg = 0};
                    }
                    game_step(game, &in, &cmd);
                    audio_consume_events(audio, game);
                    render_consume_events(render, game);
                    render_update_vfx(render, game, INK_FIXED_DT);
                    app_after_tick(&ctx, game);
                    step_hint_queue(&ctx, INK_FIXED_DT);
                    if (prev_phase == RUN_PHASE_REWARD && game->phase != RUN_PHASE_REWARD) {
                        frame_clock_reset(&clock);
                        break;
                    }
                }
            } else if (!is_active_run_scene(&ctx.ui)) {
                const int ticks = frame_clock_advance(&clock, (double)GetFrameTime());
                for (int t = 0; t < ticks; ++t) {
                    step_lobby_fluid_demo(&ctx, game, render, &menu);
                }
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
