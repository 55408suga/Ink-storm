#include "audio.h"
#include "cli.h"
#include "core.h"
#include "frame_clock.h"
#include "game.h"
#include "input_latch.h"
#include "paths.h"
#include "render.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

enum { HEADLESS_TICKS = 5 };

static int run_headless_smoke(GameState *game, AudioState *audio)
{
    audio_init(audio, UINT64_C(1));
    for (int tick = 0; tick < HEADLESS_TICKS; ++tick) {
        const BrushInput in = game_autoplay_input(game);
        const GameCommand cmd = game_autoplay_command(game);
        game_step(game, &in, &cmd);
        audio_consume_events(audio, game);
        int16_t samples[16];
        audio_render(audio, samples, sizeof samples / sizeof samples[0]);
    }

    char assets[PATH_MAX];
    if (app_resolve_asset_path("", assets, sizeof assets)) {
        printf("Assets: %s\n", assets);
    } else {
        puts("Assets unavailable, using built-in defaults.");
    }
    puts("Headless smoke passed.");
    return EXIT_SUCCESS;
}

static int run_window(GameState *game, AudioState *audio, RenderState *render,
                      const CliOptions *opts, uint32_t fault_mask)
{
    if (opts->smoke_frames > 0) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
    }
    InitWindow(INK_SCREEN_W, INK_SCREEN_H, "INKSTORM");
    if (!IsWindowReady()) {
        return EXIT_FAILURE;
    }
    SetTargetFPS(INK_TICK_HZ);

    const RenderOptions render_opts = {
        .use_post = opts->use_post,
        .fault_mask = fault_mask,
    };
    render_init(render, &render_opts);
    render_reset_vfx(render, game->config.seed);
    audio_init(audio, UINT64_C(1));

    FrameClock clock;
    frame_clock_reset(&clock);

    const Vector2 init_mouse = GetMousePosition();
    InputLatch latch;
    input_latch_init(&latch, init_mouse.x, init_mouse.y);

    UiView ui = {0};
    ui.scene = opts->mode == APP_MODE_SHOWCASE ? UI_SCENE_SHOWCASE : UI_SCENE_RUN;
    ui.hover_choice = UI_HOVER_NONE;
    ui.shake_scale = 100;

    int remaining_frames = opts->smoke_frames;
    while (remaining_frames > 0 || !WindowShouldClose()) {
        if (remaining_frames > 0) {
            const BrushInput in = game_autoplay_input(game);
            const GameCommand cmd = game_autoplay_command(game);
            game_step(game, &in, &cmd);
            audio_consume_events(audio, game);
            render_consume_events(render, game);
            render_update_vfx(render, game, INK_FIXED_DT);
        } else {
            const Vector2 mouse = GetMousePosition();
            const RawInput raw = {
                .mouse_x = mouse.x,
                .mouse_y = mouse.y,
                .left_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                .pick_pressed = -1,
            };
            input_latch_feed(&latch, &raw);

            const int ticks = frame_clock_advance(&clock, (double)GetFrameTime());
            for (int t = 0; t < ticks; ++t) {
                const BrushInput in = input_latch_take_tick(&latch);
                const GameCommand cmd = input_latch_take_command(&latch, game->phase);
                game_step(game, &in, &cmd);
                audio_consume_events(audio, game);
                render_consume_events(render, game);
                render_update_vfx(render, game, INK_FIXED_DT);
            }
        }
        render_draw(render, game, &ui);
        if (remaining_frames > 0 && --remaining_frames == 0) {
            break;
        }
    }

    render_close(render);
    CloseWindow();
    return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
    CliOptions opts;
    if (!cli_parse(argc, argv, &opts)) {
        cli_print_usage(stderr, argc > 0 ? argv[0] : "inkstorm");
        return EXIT_FAILURE;
    }

    const uint32_t fault_mask = cli_parse_debug_faults(getenv("INKSTORM_DEBUG_FAULTS"), stderr);

    GameState *game = game_create(NULL);
    AudioState *audio = calloc(1, sizeof *audio);
    if (game == NULL || audio == NULL) {
        fputs("Failed to allocate application state.\n", stderr);
        free(audio);
        game_destroy(game);
        return EXIT_FAILURE;
    }

    int status = EXIT_FAILURE;
    if (opts.mode == APP_MODE_HEADLESS) {
        status = run_headless_smoke(game, audio);
    } else {
        RenderState *render = calloc(1, sizeof *render);
        if (render == NULL) {
            fputs("Failed to allocate render state.\n", stderr);
        } else {
            status = run_window(game, audio, render, &opts, fault_mask);
            free(render);
        }
    }

    free(audio);
    game_destroy(game);
    return status;
}
