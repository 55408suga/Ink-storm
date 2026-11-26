#include "app_loop.h"
#include "audio.h"
#include "audio_out.h"
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
    static const struct {
        uint32_t bit;
        const char *name;
    } k_faults[] = {
        {APP_FAULT_SHADER, "shader"},
        {APP_FAULT_FONT, "font"},
        {APP_FAULT_SPRITES, "sprites"},
    };
    bool any = false;
    for (size_t i = 0; i < sizeof k_faults / sizeof k_faults[0]; ++i) {
        if ((mask & k_faults[i].bit) == 0u) {
            continue;
        }
        fputs(any ? "," : "INKSTORM: debug faults enabled: ", stderr);
        fputs(k_faults[i].name, stderr);
        any = true;
    }
    if (any) {
        fputc('\n', stderr);
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
                if (opts.smoke_frames == 0 && opts.snap_dir == NULL) {
                    audio_out_configure_env();
                }
                status = app_run_demo(game, audio, render, &opts, fault_mask);
            } else {
                if (opts.smoke_frames == 0) {
                    audio_out_configure_env();
                }
                status = app_run_window_loop(game, audio, render, &opts, fault_mask);
            }
            free(render);
        }
    }

    free(audio);
    game_destroy(game);
    return status;
}
