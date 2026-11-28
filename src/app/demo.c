#include "demo.h"

#include "audio_out.h"
#include "core.h"
#include "frame_clock.h"
#include "paths.h"
#include "raylib.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const uint64_t k_demo_seed = UINT64_C(0x14c5060a20260601);

bool app_snap_ensure_dir(const char *dir_path)
{
    if (dir_path == NULL || dir_path[0] == '\0') {
        fputs("INKSTORM: snapshot directory path is empty\n", stderr);
        return false;
    }

    char buf[PATH_MAX];
    const size_t raw_len = strlen(dir_path);
    if (raw_len >= sizeof buf) {
        fprintf(stderr, "INKSTORM: snapshot directory path too long: '%s'\n", dir_path);
        return false;
    }
    memcpy(buf, dir_path, raw_len + 1u);

    size_t len = raw_len;
    while (len > 1u && buf[len - 1u] == '/') {
        buf[--len] = '\0';
    }

    struct stat st;
    if (stat(buf, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "INKSTORM: snapshot path exists and is not a directory: '%s'\n", buf);
            return false;
        }
        return true;
    }

    for (char *p = buf + 1; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, 0755) != 0) {
                if (errno != EEXIST || stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) {
                    fprintf(stderr, "INKSTORM: failed to create snapshot directory '%s': %s\n", buf,
                            strerror(errno));
                    return false;
                }
            }
            *p = '/';
        }
    }

    if (mkdir(buf, 0755) != 0) {
        if (errno != EEXIST || stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "INKSTORM: failed to create snapshot directory '%s': %s\n", buf,
                    strerror(errno));
            return false;
        }
    }

    if (stat(buf, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "INKSTORM: snapshot path is not an accessible directory: '%s'\n", buf);
        return false;
    }
    return true;
}

bool app_snap_capture(RenderState *render, const GameState *game, const UiView *ui,
                      RenderTexture2D target, const char *dir_path, int snap_index)
{
    if (render == NULL || game == NULL || dir_path == NULL || target.id == 0 || snap_index <= 0) {
        return false;
    }

    const size_t dir_len = cli_trimmed_dir_len(dir_path);

    char path[PATH_MAX];
    const int n =
        (dir_len == 1u && dir_path[0] == '/')
            ? snprintf(path, sizeof path, "/snap_%02d.png", snap_index)
            : snprintf(path, sizeof path, "%.*s/snap_%02d.png", (int)dir_len, dir_path, snap_index);
    if (n < 0 || (size_t)n >= sizeof path) {
        fprintf(stderr, "INKSTORM: snapshot file path exceeds PATH_MAX for '%s'\n", dir_path);
        return false;
    }

    render_draw_to_texture(render, game, ui, target);
    Image img = LoadImageFromTexture(target.texture);
    if (img.data == NULL) {
        fprintf(stderr, "INKSTORM: failed to read offscreen texture for '%s'\n", path);
        return false;
    }
    if (img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    if (img.data == NULL) {
        fprintf(stderr, "INKSTORM: failed to convert offscreen texture for '%s'\n", path);
        return false;
    }
    ImageFlipVertical(&img);
    if (img.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 && img.width > 0 && img.height > 0) {
        unsigned char *px = (unsigned char *)img.data;
        const size_t pixel_count = (size_t)img.width * (size_t)img.height;
        for (size_t i = 0; i < pixel_count; ++i) {
            px[i * 4u + 3u] = 255u;
        }
    }
    const bool ok = ExportImage(img, path);
    UnloadImage(img);
    if (!ok) {
        fprintf(stderr, "INKSTORM: failed to export snapshot PNG '%s'\n", path);
        return false;
    }
    TraceLog(LOG_INFO, "INKSTORM: saved snapshot %s (tick=%" PRIu64 " hash=%016" PRIx64 ")", path,
             game->ticks, game_hash(game));
    return true;
}

static void demo_step_tick(GameState *game, AudioState *audio, RenderState *render,
                           const BrushInput *in, const GameCommand *cmd)
{
    game_step(game, in, cmd);
    audio_out_lock();
    audio_consume_events(audio, game);
    audio_out_unlock();
    render_consume_events(render, game);
    render_update_vfx(render, game, INK_FIXED_DT);
}

static void demo_step_autoplay(GameState *game, AudioState *audio, RenderState *render, int ticks)
{
    for (int t = 0; t < ticks && game->phase == RUN_PHASE_PLAYING; ++t) {
        const BrushInput in = game_autoplay_input(game);
        demo_step_tick(game, audio, render, &in, NULL);
    }
}

static void demo_wait_act_transition(GameState *game, AudioState *audio, RenderState *render)
{
    for (int t = 0; t < 160 && game->phase == RUN_PHASE_ACT_TRANSITION; ++t) {
        demo_step_tick(game, audio, render, NULL, NULL);
    }
}

static int run_demo_snap_mode(GameState *game, AudioState *audio, RenderState *render, UiView *ui,
                              const char *snap_dir)
{
    RenderTexture2D target = LoadRenderTexture(INK_SCREEN_W, INK_SCREEN_H);
    if (!render_texture_usable(target) || target.id == render->scene.id ||
        target.id == render->bg_cache.id) {
        fputs("INKSTORM: failed to create offscreen render texture for --snap\n", stderr);
        if (target.id != 0) {
            UnloadRenderTexture(target);
        }
        return EXIT_FAILURE;
    }

    bool ok = true;

    /* snap_01: Act 1 lively combat (wind brush, vortex, projectile deflection, reactions) */
    game_debug_start_wave(game, 1);
    demo_step_autoplay(game, audio, render, 220);
    ok = ok && app_snap_capture(render, game, ui, target, snap_dir, 1);

    /* snap_02: Reward screen (3-choice upgrade cards, reroll, shrine repair clearly legible) */
    if (ok) {
        if (game->phase == RUN_PHASE_PLAYING) {
            game_debug_force_wave_clear(game);
        }
        for (int t = 0; t < 120 && game->phase != RUN_PHASE_REWARD; ++t) {
            demo_step_tick(game, audio, render, NULL, NULL);
        }
        for (int t = 0; t < 45; ++t) {
            demo_step_tick(game, audio, render, NULL, NULL);
        }
        ok = app_snap_capture(render, game, ui, target, snap_dir, 2);
    }

    /* snap_03: Act 2 Torii & Rock Pillars (solid flow wrap-around + Nue boss & reactions) */
    if (ok) {
        game_debug_start_act(game, 1);
        demo_wait_act_transition(game, audio, render);
        game_debug_start_wave(game, GAME_BOSS_WAVE_INDEX);
        demo_step_autoplay(game, audio, render, 210);
        ok = app_snap_capture(render, game, ui, target, snap_dir, 3);
    }

    /* snap_04: Act 3 Storm Peak normal combat (Spirit Pillars, mountain wind, Ofuda Fox) */
    if (ok) {
        game_debug_start_act(game, 2);
        demo_wait_act_transition(game, audio, render);
        game_debug_start_wave(game, 2);
        demo_step_autoplay(game, audio, render, 240);
        ok = app_snap_capture(render, game, ui, target, snap_dir, 4);
    }

    /* snap_05: Act 3 Boss Ippitsu-Ryu multi-part battle (3 Dragon Orbs + 1-stroke trail + HP bar)
     */
    if (ok) {
        game_debug_start_wave(game, GAME_BOSS_WAVE_INDEX);
        demo_step_autoplay(game, audio, render, 220);
        ok = app_snap_capture(render, game, ui, target, snap_dir, 5);
    }

    /* snap_06: High-load multi-part Ryu combat + F1 X-ray ON (warm a few frames for steady-state
     * render_draw metrics rather than post-readback pipeline stall). */
    if (ok) {
        demo_step_autoplay(game, audio, render, 45);
        ui->xray = true;
        for (int w = 0; w < 5; ++w) {
            render_draw_to_texture(render, game, ui, target);
        }
        /* Unpresented back-to-back frames queue GPU work, and the next draw would time that
         * backlog instead of command issue. Reading the target back drains the queue the way the
         * paced 60 Hz loop does between frames. */
        Image drained = LoadImageFromTexture(target.texture);
        UnloadImage(drained);
        render_draw_to_texture(render, game, ui, target);
        TraceLog(LOG_INFO,
                 "INKSTORM: snap_06 F1 metrics render_draw=%.2fms prim_verts=%" PRIu32
                 " blend_sw=%" PRIu32 " fluid_solve=%.2fms",
                 render->last_metrics.draw_ms, render->last_metrics.prim_verts,
                 render->last_metrics.blend_switches, game->fluid.stats.solve_ms);
        ok = app_snap_capture(render, game, ui, target, snap_dir, 6);
        ui->xray = false;
    }

    UnloadRenderTexture(target);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int run_demo_live_mode(GameState *game, AudioState *audio, RenderState *render, UiView *ui,
                              const CliOptions *opts)
{
    /* Jump directly into Act 1 Wave 1 so ink fluid & combat highlights appear within 5 seconds. */
    game_debug_start_wave(game, 0);

    AudioOut audio_out = {0};
    if (opts->smoke_frames == 0) {
        audio_out_init(&audio_out, audio);
    }

    FrameClock clock;
    frame_clock_reset(&clock);

    int remaining_frames = opts->smoke_frames;
    int reward_hold_ticks = 0;
    uint32_t last_reward_serial = 0;
    int result_hold_ticks = 0;
    bool exit_requested = false;

    while (remaining_frames > 0 || (!WindowShouldClose() && !exit_requested)) {
        if (remaining_frames > 0) {
            const BrushInput in = game_autoplay_input(game);
            const GameCommand cmd = game_autoplay_command(game);
            demo_step_tick(game, audio, render, &in, &cmd);
        } else {
            if (IsKeyPressed(KEY_ESCAPE)) {
                break;
            }
            if (IsKeyPressed(KEY_F1)) {
                ui->xray = !ui->xray;
            }

            const int ticks = frame_clock_advance(&clock, (double)GetFrameTime());
            for (int t = 0; t < ticks; ++t) {
                const RunPhase prev_phase = game->phase;
                const BrushInput in = game_autoplay_input(game);
                GameCommand cmd = {.type = CMD_NONE, .arg = 0};

                if (game->phase == RUN_PHASE_REWARD) {
                    if (game->run.reward.serial != last_reward_serial) {
                        last_reward_serial = game->run.reward.serial;
                        reward_hold_ticks = 0;
                    }
                    if (render->reward_anim_t >= 1.0f && render->reward_reroll_t <= 0.0f) {
                        ++reward_hold_ticks;
                        if (reward_hold_ticks >= 90) {
                            cmd = game_autoplay_command(game);
                            reward_hold_ticks = 0;
                        }
                    }
                } else {
                    reward_hold_ticks = 0;
                }

                demo_step_tick(game, audio, render, &in, &cmd);

                if (prev_phase == RUN_PHASE_REWARD && game->phase != RUN_PHASE_REWARD) {
                    frame_clock_reset(&clock);
                    break;
                }

                if (game->phase == RUN_PHASE_VICTORY || game->phase == RUN_PHASE_GAME_OVER) {
                    const MetaRunResult res = game_run_result(game);
                    ui->settle = meta_calc_crystals(&res);
                    ui->settle_preview = (game->phase == RUN_PHASE_VICTORY);
                    ui->settled = (game->phase == RUN_PHASE_GAME_OVER);
                    ui->crystals_after = ui->settle.total_crystals;
                    ++result_hold_ticks;
                    if (result_hold_ticks >= 180) {
                        exit_requested = true;
                        break;
                    }
                } else {
                    result_hold_ticks = 0;
                }
            }
            audio_out_pump(&audio_out, audio);
        }

        render_draw(render, game, ui);
        if (remaining_frames > 0 && --remaining_frames == 0) {
            break;
        }
    }

    if (opts->smoke_frames == 0) {
        audio_out_close(&audio_out);
    }
    return EXIT_SUCCESS;
}

int app_run_demo(GameState *game, AudioState *audio, RenderState *render, const CliOptions *opts,
                 uint32_t fault_mask)
{
    if (game == NULL || audio == NULL || render == NULL || opts == NULL) {
        return EXIT_FAILURE;
    }

    const bool snap_mode = (opts->snap_dir != NULL);
    if (snap_mode && !app_snap_ensure_dir(opts->snap_dir)) {
        return EXIT_FAILURE;
    }

    if (snap_mode || opts->smoke_frames > 0) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
    }
    InitWindow(INK_SCREEN_W, INK_SCREEN_H, "INKSTORM - Demo");
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
    char sprite_dir[PATH_MAX];
    const bool has_sprites = app_resolve_asset_path("sprites", sprite_dir, sizeof sprite_dir);
    const RenderOptions render_opts = {
        .use_post = opts->use_post,
        .fault_mask = fault_mask,
        .font_path = has_font ? font_path : NULL,
        .title_font_path = has_title_font ? title_font_path : NULL,
        .sprite_dir = has_sprites ? sprite_dir : NULL,
    };
    render_init(render, &render_opts);

    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, k_demo_seed);
    game_init(game, &cfg);
    render_reset_vfx(render, cfg.seed);
    audio_init(audio, UINT64_C(1));
    audio_set_lobby_mode(audio, false);

    UiView ui = {0};
    meta_init_default(&ui.meta_view);
    ui.meta_view.settings.hints_on = false;
    ui.meta_view.settings.shake_scale = 100;
    ui.meta_view.settings.reduce_flash = false;
    ui.meta_view.settings.cursor_scale = 100;
    ui.scene = UI_SCENE_RUN;
    ui.hover_choice = UI_HOVER_NONE;
    ui.selected_brush = cfg.brush_style;
    ui.shake_scale = 100;
    ui.reduce_flash = false;
    ui.cursor_scale = 100;

    TraceLog(LOG_INFO, "INKSTORM: demo mode seed=0x%016" PRIx64 " post=%s mode=%s", cfg.seed,
             opts->use_post ? "on" : "off", snap_mode ? opts->snap_dir : "live");

    const int status = snap_mode ? run_demo_snap_mode(game, audio, render, &ui, opts->snap_dir)
                                 : run_demo_live_mode(game, audio, render, &ui, opts);

    render_close(render);
    CloseWindow();
    return status;
}
