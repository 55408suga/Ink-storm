#include "core.h"
#include "fluid.h"
#include "game.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum { WARMUP_TICKS = 120, MEASURED_TICKS = 600, TRIALS = 5 };

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

static double percentile(const double sorted[MEASURED_TICKS], int percent)
{
    return sorted[(MEASURED_TICKS * percent + 99) / 100 - 1];
}

static int32_t count_projectiles(const GameState *g)
{
    int32_t n = 0;
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        const EntityId id = ecs_entity_at(&g->ecs, i);
        if (id.index != ECS_INVALID_INDEX && ecs_has_mask(&g->ecs, id, COMP_PROJ)) {
            ++n;
        }
    }
    return n;
}

static int32_t count_reaction_events(const GameState *g)
{
    int32_t n = 0;
    for (int32_t i = 0; i < g->event_count; ++i) {
        const GameEventType t = g->events[i].type;
        if (t == EV_IGNITE || t == EV_STORM_ZAP || t == EV_THUNDERBURST || t == EV_CATACLYSM) {
            ++n;
        }
    }
    return n;
}

static void run_extra_stress_load(void)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x5eed));
    GameState *g = game_create(&cfg);
    if (g == NULL) {
        return;
    }
    while (g->phase == RUN_PHASE_ACT_TRANSITION) {
        game_step(g, NULL, NULL);
    }
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    /* Keep shrine and boss alive throughout the stress window so all 600 ticks stay in PLAYING. */
    if (g->shrine_count > 0) {
        g->shrines[0].max_hp = 10000;
        g->shrines[0].hp = 10000;
    }
    if (g->run.boss_id.index != ECS_INVALID_INDEX) {
        HealthComp *bhp = game_health_mut(g, g->run.boss_id);
        if (bhp != NULL) {
            bhp->max_hp = 100000;
            bhp->hp = 100000;
        }
    }

    for (int i = 0; i < 16; ++i) {
        const float ang = (float)i * (6.2831853f / 16.0f);
        const EnemyDefId def =
            (i % 3 == 0) ? ENEMY_UMBRELLA : ((i % 3 == 1) ? ENEMY_LANTERN_FIRE : ENEMY_ARCHER);
        const EntityId eid = game_debug_spawn_enemy(g, def, 640.0f + cosf(ang) * 420.0f,
                                                    360.0f + sinf(ang) * 240.0f);
        HealthComp *ehp = game_health_mut(g, eid);
        if (ehp != NULL) {
            ehp->max_hp = 5000;
            ehp->hp = 5000;
        }
    }
    for (int i = 0; i < 48; ++i) {
        const float ang = (float)i * (6.2831853f / 48.0f);
        const float x = 640.0f + cosf(ang) * 320.0f;
        const float y = 360.0f + sinf(ang) * 200.0f;
        const ProjKind pk =
            (i % 3 == 0) ? PROJ_FIREBALL : ((i % 3 == 1) ? PROJ_STORM_ORB : PROJ_ARROW);
        (void)game_debug_spawn_projectile(g, pk, x, y, -cosf(ang) * 200.0f, -sinf(ang) * 200.0f);
    }

    double game_ms[MEASURED_TICKS];
    double fluid_ms[MEASURED_TICKS];
    double game_sum = 0.0;
    double fluid_sum = 0.0;
    double alive_sum = 0.0;
    double proj_sum = 0.0;
    bool all_playing = true;
    for (int tick = 0; tick < WARMUP_TICKS + MEASURED_TICKS; ++tick) {
        const BrushInput in = game_autoplay_input(g);
        struct timespec t0 = {0};
        struct timespec t1 = {0};
        clock_gettime(CLOCK_MONOTONIC, &t0);
        game_step(g, &in, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        if (tick >= WARMUP_TICKS) {
            const int idx = tick - WARMUP_TICKS;
            game_ms[idx] = elapsed_ms(t0, t1);
            fluid_ms[idx] = g->fluid.stats.solve_ms;
            game_sum += game_ms[idx];
            fluid_sum += fluid_ms[idx];
            alive_sum += (double)ecs_alive_count(&g->ecs);
            proj_sum += (double)count_projectiles(g);
            if (g->phase != RUN_PHASE_PLAYING) {
                all_playing = false;
            }
        }
    }
    qsort(game_ms, MEASURED_TICKS, sizeof game_ms[0], compare_ms);
    qsort(fluid_ms, MEASURED_TICKS, sizeof fluid_ms[0], compare_ms);
    printf("stress(+16 enemies,+48 proj) status=%s game_mean=%.6f game_p50=%.6f game_p95=%.6f "
           "game_max=%.6f fluid_mean=%.6f fluid_p95=%.6f avg_alive=%.2f avg_proj=%.2f finite=%s\n",
           all_playing ? "PLAYING" : "INVALID", game_sum / (double)MEASURED_TICKS,
           percentile(game_ms, 50), percentile(game_ms, 95), game_ms[MEASURED_TICKS - 1],
           fluid_sum / (double)MEASURED_TICKS, percentile(fluid_ms, 95),
           alive_sum / (double)MEASURED_TICKS, proj_sum / (double)MEASURED_TICKS,
           fluid_validate_finite(&g->fluid) ? "true" : "false");
    game_destroy(g);
}

int main(void)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x5eed));
    double game_means[TRIALS];
    double game_p95s[TRIALS];
    double fluid_means[TRIALS];
    double fluid_p95s[TRIALS];
    uint64_t ref_hash = 0;
    bool valid = true;

    for (int trial = 0; trial < TRIALS; ++trial) {
        GameState *g = game_create(&cfg);
        if (g == NULL) {
            fputs("Cannot allocate GameState\n", stderr);
            return EXIT_FAILURE;
        }
        while (g->phase == RUN_PHASE_ACT_TRANSITION) {
            game_step(g, NULL, NULL);
        }
        game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);

        if (trial == 0) {
            printf("grid=%dx%d project_iters=%d active_channels=%d hz=%d seed=0x%" PRIx64
                   " wave=boss warmup=%d measured=%d trials=%d unit=ms/tick\n",
                   FLUID_W, FLUID_H, g->fluid.cfg.project_iters, g->fluid.cfg.active_channels,
                   INK_TICK_HZ, cfg.seed, WARMUP_TICKS, MEASURED_TICKS, TRIALS);
        }

        double game_samples[MEASURED_TICKS];
        double fluid_samples[MEASURED_TICKS];
        double game_total = 0.0;
        double fluid_total = 0.0;
        double alive_sum = 0.0;
        double proj_sum = 0.0;
        double react_sum = 0.0;
        bool all_playing = true;

        for (int tick = 0; tick < WARMUP_TICKS + MEASURED_TICKS; ++tick) {
            const BrushInput in = game_autoplay_input(g);
            struct timespec t0 = {0};
            struct timespec t1 = {0};
            clock_gettime(CLOCK_MONOTONIC, &t0);
            game_step(g, &in, NULL);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            if (tick >= WARMUP_TICKS) {
                const int idx = tick - WARMUP_TICKS;
                game_samples[idx] = elapsed_ms(t0, t1);
                fluid_samples[idx] = g->fluid.stats.solve_ms;
                game_total += game_samples[idx];
                fluid_total += fluid_samples[idx];
                alive_sum += (double)ecs_alive_count(&g->ecs);
                proj_sum += (double)count_projectiles(g);
                react_sum += (double)count_reaction_events(g);
                if (g->phase != RUN_PHASE_PLAYING) {
                    all_playing = false;
                }
            }
        }

        qsort(game_samples, MEASURED_TICKS, sizeof game_samples[0], compare_ms);
        qsort(fluid_samples, MEASURED_TICKS, sizeof fluid_samples[0], compare_ms);

        game_means[trial] = game_total / (double)MEASURED_TICKS;
        game_p95s[trial] = percentile(game_samples, 95);
        fluid_means[trial] = fluid_total / (double)MEASURED_TICKS;
        fluid_p95s[trial] = percentile(fluid_samples, 95);

        const uint64_t h = game_hash(g);
        const bool finite = fluid_validate_finite(&g->fluid);
        if (trial == 0) {
            ref_hash = h;
        }
        if (!finite || h != ref_hash) {
            valid = false;
        }

        printf("trial=%d status=%s game_mean=%.6f game_p50=%.6f game_p95=%.6f game_max=%.6f "
               "fluid_mean=%.6f fluid_p50=%.6f fluid_p95=%.6f fluid_max=%.6f "
               "avg_alive=%.2f avg_proj=%.2f avg_react=%.3f hash=%016" PRIx64 " finite=%s\n",
               trial + 1, all_playing ? "PLAYING" : "INVALID", game_means[trial],
               percentile(game_samples, 50), game_p95s[trial], game_samples[MEASURED_TICKS - 1],
               fluid_means[trial], percentile(fluid_samples, 50), fluid_p95s[trial],
               fluid_samples[MEASURED_TICKS - 1], alive_sum / (double)MEASURED_TICKS,
               proj_sum / (double)MEASURED_TICKS, react_sum / (double)MEASURED_TICKS, h,
               finite ? "true" : "false");

        game_destroy(g);
    }

    qsort(game_means, TRIALS, sizeof game_means[0], compare_ms);
    qsort(game_p95s, TRIALS, sizeof game_p95s[0], compare_ms);
    qsort(fluid_means, TRIALS, sizeof fluid_means[0], compare_ms);
    qsort(fluid_p95s, TRIALS, sizeof fluid_p95s[0], compare_ms);

    const double med_game_mean = game_means[TRIALS / 2];
    const double med_game_p95 = game_p95s[TRIALS / 2];
    const double med_fluid_mean = fluid_means[TRIALS / 2];
    const double med_fluid_p95 = fluid_p95s[TRIALS / 2];
    printf("median game_mean=%.6f game_p95=%.6f fluid_mean=%.6f fluid_p95=%.6f\n", med_game_mean,
           med_game_p95, med_fluid_mean, med_fluid_p95);
    printf("budget(<4.0ms mean & p95): %s\n",
           (med_game_mean < 4.0 && med_game_p95 < 4.0) ? "PASS" : "FAIL");

    run_extra_stress_load();
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
