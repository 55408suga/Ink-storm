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

static double percentile(const double *sorted, int count, int percent)
{
    if (count <= 0) {
        return 0.0;
    }
    int idx = (count * percent + 99) / 100 - 1;
    if (idx < 0) {
        idx = 0;
    } else if (idx >= count) {
        idx = count - 1;
    }
    return sorted[idx];
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

static void setup_act_boss_bench(GameState *g, int32_t act_index)
{
    game_debug_start_act(g, act_index);
    while (g->phase == RUN_PHASE_ACT_TRANSITION) {
        game_step(g, NULL, NULL);
    }
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    /* Keep shrine, boss, and any Ryu orbs alive throughout the measured window so all 600 ticks
     * stay in PLAYING. */
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
    for (int32_t s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
        const EntityId oid = g->ryu_body.orb_ids[s];
        if (oid.index != ECS_INVALID_INDEX && ecs_is_alive(&g->ecs, oid)) {
            HealthComp *ohp = game_health_mut(g, oid);
            if (ohp != NULL) {
                ohp->max_hp = 100000;
                ohp->hp = 100000;
            }
        }
    }
}

static bool run_endless_ryu_fox_bench(const RunConfig *cfg)
{
    double game_means[TRIALS];
    double game_p95s[TRIALS];
    double fluid_means[TRIALS];
    double fluid_p95s[TRIALS];
    double fluid_solved_means[TRIALS];
    double fluid_solved_p95s[TRIALS];
    int skipped_totals[TRIALS];
    uint64_t ref_hash = 0;
    bool valid = true;

    for (int trial = 0; trial < TRIALS; ++trial) {
        GameState *g = game_create(cfg);
        if (g == NULL) {
            return false;
        }
        /* Endless Act 6 (act_index=5, loop n=1): Multi-Part Ippitsu-Ryu (3 orbs + 128-pt trail)
         * + Ofuda Fox + Homing Ofuda + Mountain Wind + Spirit Pillars. */
        setup_act_boss_bench(g, 5);
        const EntityId fox = game_debug_spawn_enemy(g, ENEMY_OFUDA_FOX, 980.0f, 360.0f);
        HealthComp *fhp = game_health_mut(g, fox);
        if (fhp != NULL) {
            fhp->max_hp = 50000;
            fhp->hp = 50000;
        }
        const EntityId escort = game_debug_spawn_enemy(g, ENEMY_STONE_ONI, 880.0f, 440.0f);
        HealthComp *ehp = game_health_mut(g, escort);
        if (ehp != NULL) {
            ehp->max_hp = 50000;
            ehp->hp = 50000;
        }
        (void)game_debug_spawn_projectile(g, PROJ_HOMING_OFUDA, 940.0f, 360.0f, -140.0f, 0.0f);

        if (trial == 0) {
            printf("endless_ryu_fox act=6 loop=1 orbs=%d trail_cap=%d warmup=%d measured=%d "
                   "trials=%d unit=ms/tick\n",
                   GAME_RYU_ORB_COUNT, GAME_RYU_TRAIL_CAP, WARMUP_TICKS, MEASURED_TICKS, TRIALS);
        }

        double game_samples[MEASURED_TICKS];
        double fluid_samples[MEASURED_TICKS];
        double fluid_solved_samples[MEASURED_TICKS];
        double game_total = 0.0;
        double fluid_total = 0.0;
        double fluid_solved_total = 0.0;
        int solved_count = 0;
        int skipped_hitstop = 0;
        double alive_sum = 0.0;
        double proj_sum = 0.0;
        bool all_playing = true;

        for (int tick = 0; tick < WARMUP_TICKS + MEASURED_TICKS; ++tick) {
            const BrushInput in = game_autoplay_input(g);
            const uint64_t steps_before = g->fluid.steps;
            struct timespec t0 = {0};
            struct timespec t1 = {0};
            clock_gettime(CLOCK_MONOTONIC, &t0);
            game_step(g, &in, NULL);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            if (tick >= WARMUP_TICKS) {
                const int idx = tick - WARMUP_TICKS;
                const bool solved = g->fluid.steps > steps_before;
                const double f_ms = solved ? g->fluid.stats.solve_ms : 0.0;
                game_samples[idx] = elapsed_ms(t0, t1);
                fluid_samples[idx] = f_ms;
                game_total += game_samples[idx];
                fluid_total += f_ms;
                if (solved) {
                    fluid_solved_samples[solved_count++] = f_ms;
                    fluid_solved_total += f_ms;
                } else {
                    ++skipped_hitstop;
                }
                alive_sum += (double)ecs_alive_count(&g->ecs);
                proj_sum += (double)count_projectiles(g);
                if (g->phase != RUN_PHASE_PLAYING) {
                    all_playing = false;
                }
            }
        }

        qsort(game_samples, MEASURED_TICKS, sizeof game_samples[0], compare_ms);
        qsort(fluid_samples, MEASURED_TICKS, sizeof fluid_samples[0], compare_ms);
        if (solved_count > 0) {
            qsort(fluid_solved_samples, (size_t)solved_count, sizeof fluid_solved_samples[0],
                  compare_ms);
        }

        game_means[trial] = game_total / (double)MEASURED_TICKS;
        game_p95s[trial] = percentile(game_samples, MEASURED_TICKS, 95);
        fluid_means[trial] = fluid_total / (double)MEASURED_TICKS;
        fluid_p95s[trial] = percentile(fluid_samples, MEASURED_TICKS, 95);
        fluid_solved_means[trial] =
            solved_count > 0 ? fluid_solved_total / (double)solved_count : 0.0;
        fluid_solved_p95s[trial] = percentile(fluid_solved_samples, solved_count, 95);
        skipped_totals[trial] = skipped_hitstop;

        const uint64_t h = game_hash(g);
        const bool finite = fluid_validate_finite(&g->fluid);
        if (trial == 0) {
            ref_hash = h;
        }
        const bool trial_ok = finite && all_playing && solved_count > 0 && h == ref_hash;
        if (!trial_ok) {
            valid = false;
        }

        printf("endless_trial=%d status=%s game_mean=%.6f game_p95=%.6f fluid_mean=%.6f "
               "fluid_p95=%.6f fluid_solved_mean=%.6f fluid_solved_p95=%.6f skipped_hitstop=%d "
               "avg_alive=%.2f avg_proj=%.2f hash=%016" PRIx64 " finite=%s\n",
               trial + 1, trial_ok ? "PLAYING" : "INVALID", game_means[trial], game_p95s[trial],
               fluid_means[trial], fluid_p95s[trial], fluid_solved_means[trial],
               fluid_solved_p95s[trial], skipped_hitstop, alive_sum / (double)MEASURED_TICKS,
               proj_sum / (double)MEASURED_TICKS, h, finite ? "true" : "false");

        game_destroy(g);
    }

    qsort(game_means, TRIALS, sizeof game_means[0], compare_ms);
    qsort(game_p95s, TRIALS, sizeof game_p95s[0], compare_ms);
    qsort(fluid_means, TRIALS, sizeof fluid_means[0], compare_ms);
    qsort(fluid_p95s, TRIALS, sizeof fluid_p95s[0], compare_ms);
    qsort(fluid_solved_means, TRIALS, sizeof fluid_solved_means[0], compare_ms);
    qsort(fluid_solved_p95s, TRIALS, sizeof fluid_solved_p95s[0], compare_ms);

    const double med_game_mean = game_means[TRIALS / 2];
    const double med_game_p95 = game_p95s[TRIALS / 2];
    const bool budget_ok = valid && med_game_mean < 4.0 && med_game_p95 < 4.0;
    printf("endless_ryu_fox median game_mean=%.6f game_p95=%.6f fluid_mean=%.6f fluid_p95=%.6f "
           "fluid_solved_mean=%.6f fluid_solved_p95=%.6f skipped_hitstop=%d\n",
           med_game_mean, med_game_p95, fluid_means[TRIALS / 2], fluid_p95s[TRIALS / 2],
           fluid_solved_means[TRIALS / 2], fluid_solved_p95s[TRIALS / 2],
           skipped_totals[TRIALS / 2]);
    printf("endless_ryu_fox budget(<4.0ms mean & p95): %s\n",
           budget_ok ? "PASS" : (valid ? "FAIL" : "FAIL(INVALID)"));
    return valid;
}

static bool run_extra_stress_load(void)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x5eed));
    GameState *g = game_create(&cfg);
    if (g == NULL) {
        return false;
    }
    /* Act 2 Torii + Rock Pillar solids + Nue + 16 mixed enemies + 48 mixed projectiles. */
    setup_act_boss_bench(g, 1);

    static const EnemyDefId k_stress_defs[6] = {
        ENEMY_UMBRELLA,  ENEMY_LANTERN_STORM, ENEMY_KAMAITACHI,
        ENEMY_STONE_ONI, ENEMY_INK_LEECH,     ENEMY_ARCHER,
    };
    for (int i = 0; i < 16; ++i) {
        const float ang = (float)i * (6.2831853f / 16.0f);
        const EnemyDefId def = k_stress_defs[i % 6];
        const EntityId eid = game_debug_spawn_enemy(g, def, 640.0f + cosf(ang) * 420.0f,
                                                    360.0f + sinf(ang) * 240.0f);
        HealthComp *ehp = game_health_mut(g, eid);
        if (ehp != NULL) {
            ehp->max_hp = 5000;
            ehp->hp = 5000;
        }
    }
    static const ProjKind k_stress_projs[5] = {
        PROJ_FIREBALL, PROJ_STORM_ORB, PROJ_WIND_SICKLE, PROJ_BOUNCE_ORB, PROJ_ARROW,
    };
    for (int i = 0; i < 48; ++i) {
        const float ang = (float)i * (6.2831853f / 48.0f);
        const float x = 640.0f + cosf(ang) * 320.0f;
        const float y = 360.0f + sinf(ang) * 200.0f;
        const ProjKind pk = k_stress_projs[i % 5];
        (void)game_debug_spawn_projectile(g, pk, x, y, -cosf(ang) * 220.0f, -sinf(ang) * 220.0f);
    }

    double game_ms[MEASURED_TICKS];
    double fluid_ms[MEASURED_TICKS];
    double fluid_solved_ms[MEASURED_TICKS];
    double game_sum = 0.0;
    double fluid_sum = 0.0;
    double fluid_solved_sum = 0.0;
    int solved_count = 0;
    int skipped_hitstop = 0;
    double alive_sum = 0.0;
    double proj_sum = 0.0;
    bool all_playing = true;
    for (int tick = 0; tick < WARMUP_TICKS + MEASURED_TICKS; ++tick) {
        const BrushInput in = game_autoplay_input(g);
        const uint64_t steps_before = g->fluid.steps;
        struct timespec t0 = {0};
        struct timespec t1 = {0};
        clock_gettime(CLOCK_MONOTONIC, &t0);
        game_step(g, &in, NULL);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        if (tick >= WARMUP_TICKS) {
            const int idx = tick - WARMUP_TICKS;
            const bool solved = g->fluid.steps > steps_before;
            const double f_ms = solved ? g->fluid.stats.solve_ms : 0.0;
            game_ms[idx] = elapsed_ms(t0, t1);
            fluid_ms[idx] = f_ms;
            game_sum += game_ms[idx];
            fluid_sum += f_ms;
            if (solved) {
                fluid_solved_ms[solved_count++] = f_ms;
                fluid_solved_sum += f_ms;
            } else {
                ++skipped_hitstop;
            }
            alive_sum += (double)ecs_alive_count(&g->ecs);
            proj_sum += (double)count_projectiles(g);
            if (g->phase != RUN_PHASE_PLAYING) {
                all_playing = false;
            }
        }
    }
    qsort(game_ms, MEASURED_TICKS, sizeof game_ms[0], compare_ms);
    qsort(fluid_ms, MEASURED_TICKS, sizeof fluid_ms[0], compare_ms);
    if (solved_count > 0) {
        qsort(fluid_solved_ms, (size_t)solved_count, sizeof fluid_solved_ms[0], compare_ms);
    }
    const bool finite = fluid_validate_finite(&g->fluid);
    const bool ok = all_playing && finite && solved_count > 0;
    printf("stress(act2 solids,+16 enemies,+48 proj) status=%s game_mean=%.6f game_p50=%.6f "
           "game_p95=%.6f game_max=%.6f fluid_mean=%.6f fluid_p95=%.6f "
           "fluid_solved_mean=%.6f fluid_solved_p95=%.6f skipped_hitstop=%d avg_alive=%.2f "
           "avg_proj=%.2f finite=%s\n",
           ok ? "PLAYING" : "INVALID", game_sum / (double)MEASURED_TICKS,
           percentile(game_ms, MEASURED_TICKS, 50), percentile(game_ms, MEASURED_TICKS, 95),
           game_ms[MEASURED_TICKS - 1], fluid_sum / (double)MEASURED_TICKS,
           percentile(fluid_ms, MEASURED_TICKS, 95),
           solved_count > 0 ? fluid_solved_sum / (double)solved_count : 0.0,
           percentile(fluid_solved_ms, solved_count, 95), skipped_hitstop,
           alive_sum / (double)MEASURED_TICKS, proj_sum / (double)MEASURED_TICKS,
           finite ? "true" : "false");
    game_destroy(g);
    return ok;
}

int main(void)
{
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(0x5eed));
    double game_means[TRIALS];
    double game_p95s[TRIALS];
    double fluid_means[TRIALS];
    double fluid_p95s[TRIALS];
    double fluid_solved_means[TRIALS];
    double fluid_solved_p95s[TRIALS];
    int skipped_totals[TRIALS];
    uint64_t ref_hash = 0;
    bool valid = true;

    for (int trial = 0; trial < TRIALS; ++trial) {
        GameState *g = game_create(&cfg);
        if (g == NULL) {
            fputs("Cannot allocate GameState\n", stderr);
            return EXIT_FAILURE;
        }
        /* Act 3 boss wave: rock/spirit pillars + mountain wind + Ippitsu-Ryu + escorts. */
        setup_act_boss_bench(g, 2);

        if (trial == 0) {
            printf("grid=%dx%d project_iters=%d active_channels=%d hz=%d seed=0x%" PRIx64
                   " act=3 wave=boss warmup=%d measured=%d trials=%d unit=ms/tick\n",
                   FLUID_W, FLUID_H, g->fluid.cfg.project_iters, g->fluid.cfg.active_channels,
                   INK_TICK_HZ, cfg.seed, WARMUP_TICKS, MEASURED_TICKS, TRIALS);
        }

        double game_samples[MEASURED_TICKS];
        double fluid_samples[MEASURED_TICKS];
        double fluid_solved_samples[MEASURED_TICKS];
        double game_total = 0.0;
        double fluid_total = 0.0;
        double fluid_solved_total = 0.0;
        int solved_count = 0;
        int skipped_hitstop = 0;
        double alive_sum = 0.0;
        double proj_sum = 0.0;
        double react_sum = 0.0;
        bool all_playing = true;

        for (int tick = 0; tick < WARMUP_TICKS + MEASURED_TICKS; ++tick) {
            const BrushInput in = game_autoplay_input(g);
            const uint64_t steps_before = g->fluid.steps;
            struct timespec t0 = {0};
            struct timespec t1 = {0};
            clock_gettime(CLOCK_MONOTONIC, &t0);
            game_step(g, &in, NULL);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            if (tick >= WARMUP_TICKS) {
                const int idx = tick - WARMUP_TICKS;
                const bool solved = g->fluid.steps > steps_before;
                const double f_ms = solved ? g->fluid.stats.solve_ms : 0.0;
                game_samples[idx] = elapsed_ms(t0, t1);
                fluid_samples[idx] = f_ms;
                game_total += game_samples[idx];
                fluid_total += f_ms;
                if (solved) {
                    fluid_solved_samples[solved_count++] = f_ms;
                    fluid_solved_total += f_ms;
                } else {
                    ++skipped_hitstop;
                }
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
        if (solved_count > 0) {
            qsort(fluid_solved_samples, (size_t)solved_count, sizeof fluid_solved_samples[0],
                  compare_ms);
        }

        game_means[trial] = game_total / (double)MEASURED_TICKS;
        game_p95s[trial] = percentile(game_samples, MEASURED_TICKS, 95);
        fluid_means[trial] = fluid_total / (double)MEASURED_TICKS;
        fluid_p95s[trial] = percentile(fluid_samples, MEASURED_TICKS, 95);
        fluid_solved_means[trial] =
            solved_count > 0 ? fluid_solved_total / (double)solved_count : 0.0;
        fluid_solved_p95s[trial] = percentile(fluid_solved_samples, solved_count, 95);
        skipped_totals[trial] = skipped_hitstop;

        const uint64_t h = game_hash(g);
        const bool finite = fluid_validate_finite(&g->fluid);
        if (trial == 0) {
            ref_hash = h;
        }
        const bool trial_ok = finite && all_playing && solved_count > 0 && h == ref_hash;
        if (!trial_ok) {
            valid = false;
        }

        printf("trial=%d status=%s game_mean=%.6f game_p50=%.6f game_p95=%.6f game_max=%.6f "
               "fluid_mean=%.6f fluid_p50=%.6f fluid_p95=%.6f fluid_max=%.6f "
               "fluid_solved_mean=%.6f fluid_solved_p95=%.6f skipped_hitstop=%d "
               "avg_alive=%.2f avg_proj=%.2f avg_react=%.3f hash=%016" PRIx64 " finite=%s\n",
               trial + 1, trial_ok ? "PLAYING" : "INVALID", game_means[trial],
               percentile(game_samples, MEASURED_TICKS, 50), game_p95s[trial],
               game_samples[MEASURED_TICKS - 1], fluid_means[trial],
               percentile(fluid_samples, MEASURED_TICKS, 50), fluid_p95s[trial],
               fluid_samples[MEASURED_TICKS - 1], fluid_solved_means[trial],
               fluid_solved_p95s[trial], skipped_hitstop, alive_sum / (double)MEASURED_TICKS,
               proj_sum / (double)MEASURED_TICKS, react_sum / (double)MEASURED_TICKS, h,
               finite ? "true" : "false");

        game_destroy(g);
    }

    qsort(game_means, TRIALS, sizeof game_means[0], compare_ms);
    qsort(game_p95s, TRIALS, sizeof game_p95s[0], compare_ms);
    qsort(fluid_means, TRIALS, sizeof fluid_means[0], compare_ms);
    qsort(fluid_p95s, TRIALS, sizeof fluid_p95s[0], compare_ms);
    qsort(fluid_solved_means, TRIALS, sizeof fluid_solved_means[0], compare_ms);
    qsort(fluid_solved_p95s, TRIALS, sizeof fluid_solved_p95s[0], compare_ms);

    const double med_game_mean = game_means[TRIALS / 2];
    const double med_game_p95 = game_p95s[TRIALS / 2];
    const double med_fluid_mean = fluid_means[TRIALS / 2];
    const double med_fluid_p95 = fluid_p95s[TRIALS / 2];
    const double med_fluid_solved_mean = fluid_solved_means[TRIALS / 2];
    const double med_fluid_solved_p95 = fluid_solved_p95s[TRIALS / 2];
    const bool act3_budget_ok = valid && med_game_mean < 4.0 && med_game_p95 < 4.0;
    printf("median game_mean=%.6f game_p95=%.6f fluid_mean=%.6f fluid_p95=%.6f "
           "fluid_solved_mean=%.6f fluid_solved_p95=%.6f skipped_hitstop=%d\n",
           med_game_mean, med_game_p95, med_fluid_mean, med_fluid_p95, med_fluid_solved_mean,
           med_fluid_solved_p95, skipped_totals[TRIALS / 2]);
    printf("budget(<4.0ms mean & p95): %s\n",
           act3_budget_ok ? "PASS" : (valid ? "FAIL" : "FAIL(INVALID)"));

    if (!run_endless_ryu_fox_bench(&cfg)) {
        valid = false;
    }
    if (!run_extra_stress_load()) {
        valid = false;
    }
    return valid ? EXIT_SUCCESS : EXIT_FAILURE;
}
