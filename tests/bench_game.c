#include "game.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { WARMUP_TICKS = 120, MEASURED_TICKS = 600 };

static int compare_ms(const void *a, const void *b)
{
    const double lhs = *(const double *)a;
    const double rhs = *(const double *)b;
    return (lhs > rhs) - (lhs < rhs);
}

static double elapsed_ms(struct timespec start, struct timespec end)
{
    const double sec = (double)(end.tv_sec - start.tv_sec);
    const double nsec = (double)(end.tv_nsec - start.tv_nsec);
    return sec * 1000.0 + nsec * 1e-6;
}

int main(void)
{
    GameState *g = game_create(NULL);
    if (g == NULL) {
        fputs("Cannot allocate GameState\n", stderr);
        return EXIT_FAILURE;
    }

    game_debug_start_wave(g, 0);
    for (int i = 0; i < WARMUP_TICKS; ++i) {
        const BrushInput in = game_autoplay_input(g);
        const GameCommand cmd = game_autoplay_command(g);
        game_step(g, &in, &cmd);
    }

    double samples[MEASURED_TICKS];
    double total = 0.0;
    for (int i = 0; i < MEASURED_TICKS; ++i) {
        const BrushInput in = game_autoplay_input(g);
        const GameCommand cmd = game_autoplay_command(g);
        struct timespec t0 = {0};
        struct timespec t1 = {0};
        clock_gettime(CLOCK_MONOTONIC, &t0);
        game_step(g, &in, &cmd);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        samples[i] = elapsed_ms(t0, t1);
        total += samples[i];
    }

    qsort(samples, MEASURED_TICKS, sizeof samples[0], compare_ms);
    const double mean = total / (double)MEASURED_TICKS;
    const double p95 = samples[(MEASURED_TICKS * 95 + 99) / 100 - 1];
    printf("bench_game (skeleton) warmup=%d measured=%d mean=%.6f p95=%.6f unit=ms/tick\n",
           WARMUP_TICKS, MEASURED_TICKS, mean, p95);

    game_destroy(g);
    return EXIT_SUCCESS;
}
