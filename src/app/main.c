#include "audio.h"
#include "game.h"
#include "paths.h"
#include "render.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    WINDOW_WIDTH = 1280,
    WINDOW_HEIGHT = 720,
    TICK_HZ = 60,
    HEADLESS_TICKS = 5,
};

static int run_headless_smoke(GameState *game, AudioState *audio)
{
    for (int tick = 0; tick < HEADLESS_TICKS; ++tick) {
        game_step_smoke(game);
    }
    int16_t samples[16];
    audio_render_silence(audio, samples, sizeof samples / sizeof samples[0]);

    char assets[PATH_MAX];
    if (app_resolve_asset_path("", assets, sizeof assets)) {
        printf("Assets: %s\n", assets);
    } else {
        puts("Assets unavailable, using built-in defaults.");
    }
    puts("Headless smoke passed.");
    return EXIT_SUCCESS;
}

static int run_window(GameState *game, bool use_post, int smoke_frames)
{
    if (smoke_frames > 0) {
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
    }
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "INKSTORM");
    if (!IsWindowReady()) {
        return EXIT_FAILURE;
    }
    SetTargetFPS(TICK_HZ);

    const double step_dt = 1.0 / (double)TICK_HZ;
    const double max_frame_dt = 0.25;
    double accumulator = 0.0;
    const RenderState render = render_init(use_post, GetScreenWidth(), GetScreenHeight());
    while (smoke_frames > 0 || !WindowShouldClose()) {
        if (smoke_frames > 0) {
            game_step_smoke(game);
        } else {
            double frame_dt = (double)GetFrameTime();
            if (frame_dt < 0.0) {
                frame_dt = 0.0;
            } else if (frame_dt > max_frame_dt) {
                frame_dt = max_frame_dt;
            }
            accumulator += frame_dt;
            while (accumulator >= step_dt) {
                game_step_smoke(game);
                accumulator -= step_dt;
            }
        }
        render_draw(&render, game);
        if (smoke_frames > 0 && --smoke_frames == 0) {
            break;
        }
    }
    render_close(&render);
    CloseWindow();
    return EXIT_SUCCESS;
}

static int parse_frames(const char *value)
{
    char *end;
    errno = 0;
    const long frames = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || frames < 1 || frames > INT_MAX) {
        return 0;
    }
    return (int)frames;
}

static int usage(const char *program)
{
    fprintf(stderr, "Usage: %s [--headless-smoke | --smoke-frames N] [--no-post]\n", program);
    return EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    bool headless = false;
    bool use_post = true;
    int smoke_frames = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--headless-smoke") == 0) {
            headless = true;
        } else if (strcmp(argv[i], "--no-post") == 0) {
            use_post = false;
        } else if (strcmp(argv[i], "--smoke-frames") == 0 && i + 1 < argc) {
            smoke_frames = parse_frames(argv[++i]);
            if (smoke_frames == 0) {
                return usage(argv[0]);
            }
        } else {
            return usage(argv[0]);
        }
    }
    if (headless && smoke_frames > 0) {
        return usage(argv[0]);
    }

    GameState game;
    game_init(&game, UINT64_C(1));
    if (headless) {
        AudioState audio;
        audio_init(&audio);
        return run_headless_smoke(&game, &audio);
    }
    return run_window(&game, use_post, smoke_frames);
}
