#include "fluid.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WARMUP_TICKS = 120, MEASURED_TICKS = 600, TRIALS = 5 };
static const float k_dt = 1.0f / 60.0f;

static void place_obstacles(FluidState *f)
{
    fluid_set_solid_rect(f, 372, 320, 16, 16, true);
    fluid_set_solid_rect(f, 372, 384, 16, 16, true);
    fluid_set_solid_rect(f, 892, 320, 16, 16, true);
    fluid_set_solid_rect(f, 892, 384, 16, 16, true);
    fluid_set_solid_circle(f, 340, 190, 24, true);
    fluid_set_solid_circle(f, 940, 190, 24, true);
    fluid_set_solid_circle(f, 340, 530, 24, true);
    fluid_set_solid_circle(f, 940, 530, 24, true);
}

static void apply_input(FluidState *f, int tick)
{
    static const float primary[FLUID_MAX_CHANNELS] = {1.2f, 0, 0, 0};
    static const float secondary[FLUID_MAX_CHANNELS] = {0.4f, 0.8f, 0, 0};
    static const float swirl[FLUID_MAX_CHANNELS] = {0.3f, 0, 0.7f, 0};

    float a = 0.05f * tick;
    fluid_splat(f, 640 + 380 * cosf(a), 360 + 220 * sinf(1.3f * a), 4.5f, -110 * sinf(a),
                90 * cosf(1.3f * a), primary);
    if (tick % 7 == 0) {
        fluid_splat(f, 640 + 300 * sinf(2 * a), 200, 3.0f, 0, 60, secondary);
    }
    if (tick % 20 == 0) {
        fluid_swirl(f, 640 + 200 * cosf(0.7f * a), 360, 9.0f, 70, swirl);
    }
    if (tick % 45 == 0) {
        fluid_add_radial_impulse(f, 640 - 250 * cosf(a), 360 + 150 * sinf(a), 10.0f, 110);
    }
}

static int compare_ms(const void *a, const void *b)
{
    double lhs = *(const double *)a;
    double rhs = *(const double *)b;
    return (lhs > rhs) - (lhs < rhs);
}

/* Percentile ranks are one-based. */
static double percentile(const double sorted[MEASURED_TICKS], int percent)
{
    return sorted[(MEASURED_TICKS * percent + 99) / 100 - 1];
}

int main(void)
{
    FluidState *f = calloc(1, sizeof *f);
    FluidState *snapshot = calloc(1, sizeof *snapshot);
    if (f == NULL || snapshot == NULL) {
        fputs("Cannot allocate fluid benchmark states\n", stderr);
        free(snapshot);
        free(f);
        return EXIT_FAILURE;
    }

    double means[TRIALS], p95s[TRIALS], samples[MEASURED_TICKS];
    uint64_t reference_hash = 0;
    bool valid = true;

    for (int trial = 0; trial < TRIALS; ++trial) {
        fluid_init(f, NULL);
        place_obstacles(f);
        if (trial == 0) {
            printf("grid=%dx%d project_iters=%d active_channels=%d obstacles=8 hz=60 seed=none "
                   "warmup=%d measured=%d trials=%d unit=ms/tick\n",
                   FLUID_W, FLUID_H, f->cfg.project_iters, f->cfg.active_channels, WARMUP_TICKS,
                   MEASURED_TICKS, TRIALS);
        }
        double total = 0.0;
        for (int tick = 0; tick < WARMUP_TICKS + MEASURED_TICKS; ++tick) {
            apply_input(f, tick);
            fluid_step(f, k_dt);
            if (tick >= WARMUP_TICKS) {
                samples[tick - WARMUP_TICKS] = f->stats.solve_ms;
                total += f->stats.solve_ms;
            }
        }

        qsort(samples, MEASURED_TICKS, sizeof samples[0], compare_ms);
        means[trial] = total / MEASURED_TICKS;
        p95s[trial] = percentile(samples, 95);
        uint64_t hash = fluid_hash(f);
        bool finite = fluid_validate_finite(f);
        if (trial == 0) {
            reference_hash = hash;
        }
        if (!finite || hash != reference_hash) {
            valid = false;
        }
        printf("trial=%d mean=%.6f p50=%.6f p95=%.6f max=%.6f hash=%016" PRIx64 " finite=%s\n",
               trial + 1, means[trial], percentile(samples, 50), p95s[trial],
               samples[MEASURED_TICKS - 1], hash, finite ? "true" : "false");
    }

    qsort(means, TRIALS, sizeof means[0], compare_ms);
    qsort(p95s, TRIALS, sizeof p95s[0], compare_ms);
    double median_mean = means[TRIALS / 2];
    double median_p95 = p95s[TRIALS / 2];
    printf("median mean=%.6f p95=%.6f\n", median_mean, median_p95);
    printf("budget(<2.0ms mean & p95): %s\n",
           median_mean < 2.0 && median_p95 < 2.0 ? "PASS" : "FAIL");

    memcpy(snapshot, f, sizeof *f);
    for (int i = 0; i < MEASURED_TICKS; ++i) {
        memcpy(f, snapshot, sizeof *f);
        fluid_step(f, k_dt);
        samples[i] = f->stats.solve_ms;
    }
    qsort(samples, MEASURED_TICKS, sizeof samples[0], compare_ms);
    printf("noise floor (same state, %d steps) p50=%.6f p95=%.6f\n", MEASURED_TICKS,
           percentile(samples, 50), percentile(samples, 95));
    valid = valid && fluid_validate_finite(f);

    free(snapshot);
    free(f);
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
