#include "app_loop.h"
#include "audio.h"
#include "cli.h"
#include "demo.h"
#include "game.h"
#include "headless.h"
#include "render.h"
#include "showcase.h"

#include <stdio.h>
#include <stdlib.h>

static void log_debug_faults(uint32_t mask)
{
    if (mask == 0u) {
        return;
    }
    const bool sh = (mask & APP_FAULT_SHADER) != 0u;
    const bool fn = (mask & APP_FAULT_FONT) != 0u;
    if (sh && fn) {
        fputs("INKSTORM: debug faults enabled: shader,font\n", stderr);
    } else if (sh) {
        fputs("INKSTORM: debug faults enabled: shader\n", stderr);
    } else if (fn) {
        fputs("INKSTORM: debug faults enabled: font\n", stderr);
    }
}

int main(int argc, char **argv)
{
    CliOptions opts;
    if (!cli_parse(argc, argv, &opts)) {
        cli_print_usage(stderr, argc > 0 ? argv[0] : "inkstorm");
        return EXIT_FAILURE;
    }

    const uint32_t fault_mask = cli_parse_debug_faults(getenv("INKSTORM_DEBUG_FAULTS"), stderr);
    log_debug_faults(fault_mask);

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
        status = app_run_headless_smoke(game, audio);
    } else {
        RenderState *render = calloc(1, sizeof *render);
        if (render == NULL) {
            fputs("Failed to allocate render state.\n", stderr);
        } else {
            if (opts.mode == APP_MODE_SHOWCASE) {
                status = app_run_showcase(game, render, &opts, fault_mask);
            } else if (opts.mode == APP_MODE_DEMO) {
                status = app_run_demo(game, audio, render, &opts, fault_mask);
            } else {
                status = app_run_window_loop(game, audio, render, &opts, fault_mask);
            }
            free(render);
        }
    }

    free(audio);
    game_destroy(game);
    return status;
}
