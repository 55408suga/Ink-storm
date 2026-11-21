#include "headless.h"

#include "paths.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

enum { HEADLESS_SMOKE_STEPS = 600 };

static bool is_paused_phase(RunPhase phase)
{
    return phase == RUN_PHASE_REWARD || phase == RUN_PHASE_VICTORY || phase == RUN_PHASE_GAME_OVER;
}

int app_run_headless_smoke(GameState *game, AudioState *audio)
{
    if (game == NULL || audio == NULL) {
        fputs("Headless smoke failed: null application state.\n", stderr);
        return EXIT_FAILURE;
    }

    audio_init(audio, UINT64_C(1));

    int16_t pcm[AUDIO_SAMPLES_PER_TICK];
    for (int step = 0; step < HEADLESS_SMOKE_STEPS; ++step) {
        const uint64_t prev_ticks = game->ticks;
        const BrushInput in = game_autoplay_input(game);
        const GameCommand cmd = game_autoplay_command(game);
        game_step(game, &in, &cmd);

        const uint64_t delta = game->ticks - prev_ticks;
        if (delta > 1u || (delta == 0u && !is_paused_phase(game->phase))) {
            fprintf(stderr,
                    "Headless smoke failed: ticks contract violated at step %d "
                    "(delta=%" PRIu64 ", phase=%d)\n",
                    step, delta, (int)game->phase);
            return EXIT_FAILURE;
        }

        audio_consume_events(audio, game);
        audio_render(audio, pcm, AUDIO_SAMPLES_PER_TICK);
    }

    if (game->fluid.steps == 0u) {
        fputs("Headless smoke failed: fluid never stepped (stuck in transition).\n", stderr);
        return EXIT_FAILURE;
    }
    if (!fluid_validate_finite(&game->fluid)) {
        fputs("Headless smoke failed: non-finite fluid state detected.\n", stderr);
        return EXIT_FAILURE;
    }
    if (!audio_validate_finite(audio)) {
        fputs("Headless smoke failed: non-finite audio state detected.\n", stderr);
        return EXIT_FAILURE;
    }

    char font_path[PATH_MAX];
    if (app_resolve_asset_path("fonts/ui_mincho.ttf", font_path, sizeof font_path)) {
        puts("Font: assets/fonts/ui_mincho.ttf found");
    } else {
        puts("Font: unavailable (ASCII fallback)");
    }

    printf("Headless smoke passed. ticks=%" PRIu64 " hash=0x%016" PRIx64 "\n", game->ticks,
           game_hash(game));
    return EXIT_SUCCESS;
}
