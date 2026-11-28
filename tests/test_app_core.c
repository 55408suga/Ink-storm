#include "audio_cb.h"
#include "bgm_fade.h"
#include "cli.h"
#include "combat/damage.h"
#include "frame_clock.h"
#include "hint_queue.h"
#include "input_latch.h"
#include "menu_nav.h"
#include "save.h"
#include "unity.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void setUp(void) {}

void tearDown(void) {}

static void test_cli_valid_options(void)
{
    CliOptions opts;

    char *argv_empty[] = {"inkstorm"};
    TEST_ASSERT_TRUE(cli_parse(1, argv_empty, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_WINDOW, opts.mode);
    TEST_ASSERT_EQUAL_INT(0, opts.smoke_frames);
    TEST_ASSERT_TRUE(opts.use_post);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DEFAULT, opts.save_mode);
    TEST_ASSERT_NULL(opts.save_path);

    char *argv_no_post[] = {"inkstorm", "--no-post"};
    TEST_ASSERT_TRUE(cli_parse(2, argv_no_post, &opts));
    TEST_ASSERT_FALSE(opts.use_post);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DEFAULT, opts.save_mode);

    char *argv_no_save[] = {"inkstorm", "--no-save"};
    TEST_ASSERT_TRUE(cli_parse(2, argv_no_save, &opts));
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);
    TEST_ASSERT_NULL(opts.save_path);

    char *argv_save_path[] = {"inkstorm", "--save-path", "/tmp/a.txt"};
    TEST_ASSERT_TRUE(cli_parse(3, argv_save_path, &opts));
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_EXPLICIT, opts.save_mode);
    TEST_ASSERT_NOT_NULL(opts.save_path);
    TEST_ASSERT_EQUAL_STRING("/tmp/a.txt", opts.save_path);

    char *argv_headless[] = {"inkstorm", "--headless-smoke"};
    TEST_ASSERT_TRUE(cli_parse(2, argv_headless, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_HEADLESS, opts.mode);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);

    char *argv_smoke[] = {"inkstorm", "--smoke-frames", "5"};
    TEST_ASSERT_TRUE(cli_parse(3, argv_smoke, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_WINDOW, opts.mode);
    TEST_ASSERT_EQUAL_INT(5, opts.smoke_frames);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);

    char *argv_showcase[] = {"inkstorm", "--showcase", "--smoke-frames", "3", "--no-post"};
    TEST_ASSERT_TRUE(cli_parse(5, argv_showcase, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_SHOWCASE, opts.mode);
    TEST_ASSERT_EQUAL_INT(3, opts.smoke_frames);
    TEST_ASSERT_FALSE(opts.use_post);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);
    TEST_ASSERT_NULL(opts.snap_dir);

    char *argv_demo[] = {"inkstorm", "--demo"};
    TEST_ASSERT_TRUE(cli_parse(2, argv_demo, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_DEMO, opts.mode);
    TEST_ASSERT_EQUAL_INT(0, opts.smoke_frames);
    TEST_ASSERT_TRUE(opts.use_post);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);
    TEST_ASSERT_NULL(opts.save_path);
    TEST_ASSERT_NULL(opts.snap_dir);

    char *argv_demo_flags[] = {"inkstorm", "--demo", "--no-post", "--no-save"};
    TEST_ASSERT_TRUE(cli_parse(4, argv_demo_flags, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_DEMO, opts.mode);
    TEST_ASSERT_FALSE(opts.use_post);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);
    TEST_ASSERT_NULL(opts.snap_dir);

    char *argv_demo_smoke[] = {"inkstorm", "--demo", "--smoke-frames", "10"};
    TEST_ASSERT_TRUE(cli_parse(4, argv_demo_smoke, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_DEMO, opts.mode);
    TEST_ASSERT_EQUAL_INT(10, opts.smoke_frames);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);
    TEST_ASSERT_NULL(opts.snap_dir);

    char *argv_demo_snap[] = {"inkstorm", "--demo", "--snap", "build/snaps/out"};
    TEST_ASSERT_TRUE(cli_parse(4, argv_demo_snap, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_DEMO, opts.mode);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);
    TEST_ASSERT_NOT_NULL(opts.snap_dir);
    TEST_ASSERT_EQUAL_STRING("build/snaps/out", opts.snap_dir);

    char *argv_snap_only[] = {"inkstorm", "--snap", "build/snaps/out"};
    TEST_ASSERT_TRUE(cli_parse(3, argv_snap_only, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_DEMO, opts.mode);
    TEST_ASSERT_TRUE(opts.use_post);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);
    TEST_ASSERT_NOT_NULL(opts.snap_dir);
    TEST_ASSERT_EQUAL_STRING("build/snaps/out", opts.snap_dir);

    char *argv_snap_nopost[] = {"inkstorm", "--snap", "build/snaps/out", "--no-post"};
    TEST_ASSERT_TRUE(cli_parse(4, argv_snap_nopost, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_DEMO, opts.mode);
    TEST_ASSERT_FALSE(opts.use_post);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);
    TEST_ASSERT_EQUAL_STRING("build/snaps/out", opts.snap_dir);

    char *argv_showcase_snap[] = {"inkstorm", "--showcase", "--snap", "build/snaps/sc"};
    TEST_ASSERT_TRUE(cli_parse(4, argv_showcase_snap, &opts));
    TEST_ASSERT_EQUAL_INT(APP_MODE_SHOWCASE, opts.mode);
    TEST_ASSERT_EQUAL_INT(SAVE_MODE_DISABLED, opts.save_mode);
    TEST_ASSERT_NOT_NULL(opts.snap_dir);
    TEST_ASSERT_EQUAL_STRING("build/snaps/sc", opts.snap_dir);
}

static void test_cli_usage_errors(void)
{
    char long_path[PATH_MAX + 1];
    memset(long_path, 'a', PATH_MAX);
    long_path[PATH_MAX] = '\0';

    char near_max_snap[PATH_MAX - 10];
    memset(near_max_snap, 'b', sizeof near_max_snap - 1u);
    near_max_snap[sizeof near_max_snap - 1u] = '\0';

    typedef struct {
        int argc;
        char *argv[6];
    } BadCase;

    const BadCase cases[] = {
        {2, {"inkstorm", "--bogus"}},
        {2, {"inkstorm", "--smoke-frames"}},
        {3, {"inkstorm", "--smoke-frames", "0"}},
        {3, {"inkstorm", "--smoke-frames", "-1"}},
        {3, {"inkstorm", "--smoke-frames", "abc"}},
        {3, {"inkstorm", "--smoke-frames", "5x"}},
        {3, {"inkstorm", "--smoke-frames", "2147483648"}},
        {2, {"inkstorm", "--save-path"}},
        {3, {"inkstorm", "--save-path", ""}},
        {3, {"inkstorm", "--save-path", "--no-save"}},
        {3, {"inkstorm", "--save-path", long_path}},
        {2, {"inkstorm", "--snap"}},
        {3, {"inkstorm", "--snap", ""}},
        {3, {"inkstorm", "--snap", "--no-post"}},
        {3, {"inkstorm", "--snap", long_path}},
        {3, {"inkstorm", "--snap", near_max_snap}},
        {4, {"inkstorm", "--no-save", "--save-path", "x"}},
        {4, {"inkstorm", "--headless-smoke", "--smoke-frames", "1"}},
        {3, {"inkstorm", "--headless-smoke", "--showcase"}},
        {3, {"inkstorm", "--demo", "--headless-smoke"}},
        {3, {"inkstorm", "--demo", "--showcase"}},
        {4, {"inkstorm", "--demo", "--save-path", "x"}},
        {4, {"inkstorm", "--snap", "a", "--headless-smoke"}},
        {5, {"inkstorm", "--snap", "a", "--save-path", "x"}},
        {5, {"inkstorm", "--snap", "a", "--smoke-frames", "1"}},
        {5, {"inkstorm", "--save-path", "x", "--smoke-frames", "1"}},
        {4, {"inkstorm", "--save-path", "x", "--showcase"}},
        {4, {"inkstorm", "--save-path", "x", "--headless-smoke"}},
        {3, {"inkstorm", "--no-post", "--no-post"}},
        {3, {"inkstorm", "--demo", "--demo"}},
        {5, {"inkstorm", "--snap", "a", "--snap", "b"}},
        {5, {"inkstorm", "--smoke-frames", "1", "--smoke-frames", "2"}},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        CliOptions opts;
        TEST_ASSERT_FALSE(cli_parse(cases[i].argc, cases[i].argv, &opts));
    }
}

static void test_cli_debug_faults(void)
{
    TEST_ASSERT_EQUAL_UINT32(APP_FAULT_SHADER, cli_parse_debug_faults("shader", NULL));
    TEST_ASSERT_EQUAL_UINT32(APP_FAULT_SHADER | APP_FAULT_FONT,
                             cli_parse_debug_faults(" font , shader ", NULL));
    TEST_ASSERT_EQUAL_UINT32(APP_FAULT_SPRITES, cli_parse_debug_faults("sprites", NULL));
    TEST_ASSERT_EQUAL_UINT32(APP_FAULT_SHADER | APP_FAULT_FONT | APP_FAULT_SPRITES,
                             cli_parse_debug_faults("sprites,font,shader", NULL));
    TEST_ASSERT_EQUAL_UINT32(0, cli_parse_debug_faults("sprite", NULL));
    TEST_ASSERT_EQUAL_UINT32(0, cli_parse_debug_faults("", NULL));
    TEST_ASSERT_EQUAL_UINT32(0, cli_parse_debug_faults(NULL, NULL));

    FILE *warn = tmpfile();
    TEST_ASSERT_NOT_NULL(warn);
    const uint32_t mask = cli_parse_debug_faults("shader,typo", warn);
    TEST_ASSERT_EQUAL_UINT32(APP_FAULT_SHADER, mask);

    rewind(warn);
    char buf[256];
    const size_t n = fread(buf, 1, sizeof buf - 1, warn);
    buf[n] = '\0';
    fclose(warn);

    int newlines = 0;
    for (size_t i = 0; i < n; ++i) {
        if (buf[i] == '\n') {
            ++newlines;
        }
    }
    TEST_ASSERT_EQUAL_INT(1, newlines);
    TEST_ASSERT_NOT_NULL(strstr(buf, "INKSTORM: unknown debug fault 'typo' ignored"));
}

static void test_frame_clock(void)
{
    FrameClock clock;
    frame_clock_reset(&clock);

    int total_60 = 0;
    for (int i = 0; i < 60; ++i) {
        total_60 += frame_clock_advance(&clock, 1.0 / 60.0);
    }
    TEST_ASSERT_EQUAL_INT(60, total_60);

    frame_clock_reset(&clock);
    int total_144 = 0;
    for (int i = 0; i < 144; ++i) {
        total_144 += frame_clock_advance(&clock, 1.0 / 144.0);
    }
    TEST_ASSERT_TRUE(total_144 >= 59 && total_144 <= 60);

    frame_clock_reset(&clock);
    TEST_ASSERT_EQUAL_INT(15, frame_clock_advance(&clock, 0.25));

    frame_clock_reset(&clock);
    TEST_ASSERT_EQUAL_INT(15, frame_clock_advance(&clock, 10.0));

    frame_clock_reset(&clock);
    TEST_ASSERT_EQUAL_INT(0, frame_clock_advance(&clock, 0.005));
    const double acc_before = clock.accumulator;
    TEST_ASSERT_EQUAL_INT(0, frame_clock_advance(&clock, -1.0));
    TEST_ASSERT_TRUE(clock.accumulator == acc_before);
    TEST_ASSERT_EQUAL_INT(0, frame_clock_advance(&clock, (double)NAN));
    TEST_ASSERT_TRUE(clock.accumulator == acc_before);
    TEST_ASSERT_EQUAL_INT(0, frame_clock_advance(&clock, (double)INFINITY));
    TEST_ASSERT_TRUE(clock.accumulator == acc_before);

    frame_clock_reset(&clock);
    TEST_ASSERT_TRUE(clock.accumulator == 0.0);
}

static RawInput make_raw(float x, float y)
{
    return (RawInput){
        .mouse_x = x,
        .mouse_y = y,
        .pick_pressed = -1,
    };
}

static void test_latch_move_first_tick_only(void)
{
    InputLatch l;
    input_latch_init(&l, 0.0f, 0.0f);

    RawInput raw = make_raw(100.0f, 50.0f);
    input_latch_feed(&l, &raw);

    const BrushInput t1 = input_latch_take_tick(&l);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t1.pmx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t1.pmy);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, t1.mx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, t1.my);

    const BrushInput t2 = input_latch_take_tick(&l);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, t2.pmx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, t2.pmy);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, t2.mx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, t2.my);
}

static void test_latch_catch_up_ticks_share_motion_without_a_skill_press(void)
{
    static const int32_t k_tick_counts[] = {2, 3};
    for (size_t c = 0; c < sizeof k_tick_counts / sizeof k_tick_counts[0]; ++c) {
        const int32_t ticks = k_tick_counts[c];
        InputLatch l;
        input_latch_init(&l, 0.0f, 0.0f);
        RawInput raw = make_raw(90.0f, -60.0f);
        raw.left_down = true;
        input_latch_feed(&l, &raw);

        for (int32_t t = 0; t < ticks; ++t) {
            const BrushInput in = input_latch_take_subtick(&l, ticks - t);
            const float from = (float)t / (float)ticks;
            const float to = (float)(t + 1) / (float)ticks;
            TEST_ASSERT_FLOAT_WITHIN(1e-3f, 90.0f * from, in.pmx);
            TEST_ASSERT_FLOAT_WITHIN(1e-3f, -60.0f * from, in.pmy);
            TEST_ASSERT_FLOAT_WITHIN(1e-3f, 90.0f * to, in.mx);
            TEST_ASSERT_FLOAT_WITHIN(1e-3f, -60.0f * to, in.my);
            TEST_ASSERT_TRUE(in.paint);
            TEST_ASSERT_FALSE(in.cyclone);
            TEST_ASSERT_FALSE(in.gale);
        }

        const BrushInput rest = input_latch_take_tick(&l);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 90.0f, rest.pmx);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 90.0f, rest.mx);
    }
}

static void test_latch_skill_press_takes_the_whole_catch_up_motion_once(void)
{
    static const int32_t k_tick_counts[] = {2, 3};
    static const bool k_cyclone[] = {true, false, true};
    static const bool k_gale[] = {false, true, true};
    for (size_t c = 0; c < sizeof k_tick_counts / sizeof k_tick_counts[0]; ++c) {
        for (size_t p = 0; p < sizeof k_cyclone / sizeof k_cyclone[0]; ++p) {
            const int32_t ticks = k_tick_counts[c];
            InputLatch l;
            input_latch_init(&l, 0.0f, 0.0f);
            RawInput raw = make_raw(90.0f, -60.0f);
            raw.left_down = true;
            raw.cyclone_pressed = k_cyclone[p];
            raw.gale_pressed = k_gale[p];
            input_latch_feed(&l, &raw);

            const BrushInput press = input_latch_take_subtick(&l, ticks);
            TEST_ASSERT_EQUAL_FLOAT(0.0f, press.pmx);
            TEST_ASSERT_EQUAL_FLOAT(0.0f, press.pmy);
            TEST_ASSERT_EQUAL_FLOAT(90.0f, press.mx);
            TEST_ASSERT_EQUAL_FLOAT(-60.0f, press.my);
            TEST_ASSERT_TRUE(press.paint);
            TEST_ASSERT_EQUAL(k_cyclone[p], press.cyclone);
            TEST_ASSERT_EQUAL(k_gale[p], press.gale);

            for (int32_t t = 1; t < ticks; ++t) {
                const BrushInput still = input_latch_take_subtick(&l, ticks - t);
                TEST_ASSERT_EQUAL_FLOAT(90.0f, still.pmx);
                TEST_ASSERT_EQUAL_FLOAT(-60.0f, still.pmy);
                TEST_ASSERT_EQUAL_FLOAT(90.0f, still.mx);
                TEST_ASSERT_EQUAL_FLOAT(-60.0f, still.my);
                TEST_ASSERT_TRUE(still.paint);
                TEST_ASSERT_FALSE(still.cyclone);
                TEST_ASSERT_FALSE(still.gale);
            }
        }
    }
}

typedef struct {
    bool cyclone_seen;
    bool gale_seen;
    GameEvent cyclone;
    GameEvent gale;
} SkillCasts;

static void record_skill_casts(const GameState *g, SkillCasts *out)
{
    for (int32_t i = 0; i < g->event_count; ++i) {
        const GameEvent *ev = &g->events[i];
        if (ev->type == EV_CYCLONE && !out->cyclone_seen) {
            out->cyclone = *ev;
            out->cyclone_seen = true;
        } else if (ev->type == EV_GALE && !out->gale_seen) {
            out->gale = *ev;
            out->gale_seen = true;
        }
    }
}

static SkillCasts cast_skills_after_aiming_right(int32_t frame_ticks, float from_x, float from_y,
                                                 float to_x, float to_y)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(7));
    game_init_sandbox(g, &cfg);

    InputLatch l;
    input_latch_init(&l, from_x - 40.0f, from_y);
    RawInput raw = make_raw(from_x, from_y);
    input_latch_feed(&l, &raw);
    const BrushInput aim = input_latch_take_tick(&l);
    game_step_sandbox(g, &aim);

    raw = make_raw(to_x, to_y);
    raw.cyclone_pressed = true;
    raw.gale_pressed = true;
    input_latch_feed(&l, &raw);
    SkillCasts casts = {0};
    for (int32_t t = 0; t < frame_ticks; ++t) {
        const BrushInput in = input_latch_take_subtick(&l, frame_ticks - t);
        game_step_sandbox(g, &in);
        record_skill_casts(g, &casts);
    }
    game_destroy(g);
    TEST_ASSERT_TRUE(casts.cyclone_seen);
    TEST_ASSERT_TRUE(casts.gale_seen);
    return casts;
}

static void test_catch_up_frame_casts_skills_at_the_sampled_cursor_and_aim(void)
{
    typedef struct {
        float from_x, from_y, to_x, to_y;
    } Motion;
    /* A long diagonal sweep, and a 4.2 px flick whose thirds fall below Gale's 2 px aim step. */
    static const Motion k_motions[] = {
        {300.0f, 300.0f, 600.0f, 420.0f},
        {640.0f, 360.0f, 643.0f, 363.0f},
    };
    for (size_t m = 0; m < sizeof k_motions / sizeof k_motions[0]; ++m) {
        const Motion mv = k_motions[m];
        const SkillCasts one =
            cast_skills_after_aiming_right(1, mv.from_x, mv.from_y, mv.to_x, mv.to_y);
        TEST_ASSERT_EQUAL_FLOAT(mv.to_x, one.cyclone.x);
        TEST_ASSERT_EQUAL_FLOAT(mv.to_y, one.cyclone.y);
        TEST_ASSERT_EQUAL_FLOAT(mv.to_x, one.gale.x);
        TEST_ASSERT_EQUAL_FLOAT(mv.to_y, one.gale.y);
        TEST_ASSERT_TRUE(one.gale.y2 > 0.1f);

        static const int32_t k_catch_up[] = {2, 3};
        for (size_t c = 0; c < sizeof k_catch_up / sizeof k_catch_up[0]; ++c) {
            const SkillCasts late = cast_skills_after_aiming_right(k_catch_up[c], mv.from_x,
                                                                   mv.from_y, mv.to_x, mv.to_y);
            TEST_ASSERT_EQUAL_FLOAT(one.cyclone.x, late.cyclone.x);
            TEST_ASSERT_EQUAL_FLOAT(one.cyclone.y, late.cyclone.y);
            TEST_ASSERT_EQUAL_FLOAT(one.gale.x, late.gale.x);
            TEST_ASSERT_EQUAL_FLOAT(one.gale.y, late.gale.y);
            TEST_ASSERT_EQUAL_FLOAT(one.gale.x2, late.gale.x2);
            TEST_ASSERT_EQUAL_FLOAT(one.gale.y2, late.gale.y2);
        }
    }
}

typedef struct {
    float aim_x, aim_y;
    GameEvent gale;
} GaleAim;

static GaleAim press_gale_after_a_small_move(int32_t move_ticks)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, UINT64_C(7));
    game_init_sandbox(g, &cfg);

    InputLatch l;
    input_latch_init(&l, 600.0f, 360.0f);
    RawInput raw = make_raw(640.0f, 360.0f);
    input_latch_feed(&l, &raw);
    const BrushInput aim_right = input_latch_take_tick(&l);
    game_step_sandbox(g, &aim_right);

    raw = make_raw(643.0f, 363.0f);
    input_latch_feed(&l, &raw);
    for (int32_t t = 0; t < move_ticks; ++t) {
        const BrushInput in = input_latch_take_subtick(&l, move_ticks - t);
        game_step_sandbox(g, &in);
    }

    raw.gale_pressed = true;
    input_latch_feed(&l, &raw);
    const BrushInput press = input_latch_take_subtick(&l, 1);
    game_step_sandbox(g, &press);
    SkillCasts casts = {0};
    record_skill_casts(g, &casts);
    const GaleAim out = {.aim_x = g->player.aim_x, .aim_y = g->player.aim_y, .gale = casts.gale};
    game_destroy(g);
    TEST_ASSERT_TRUE(casts.gale_seen);
    return out;
}

static void test_small_move_on_a_catch_up_frame_still_turns_the_next_gale(void)
{
    const GaleAim one = press_gale_after_a_small_move(1);
    TEST_ASSERT_TRUE(one.aim_y > 0.1f);
    TEST_ASSERT_TRUE(one.gale.y2 > 0.1f);

    static const int32_t k_catch_up[] = {2, 3};
    for (size_t c = 0; c < sizeof k_catch_up / sizeof k_catch_up[0]; ++c) {
        const GaleAim late = press_gale_after_a_small_move(k_catch_up[c]);
        TEST_ASSERT_EQUAL_FLOAT(one.aim_x, late.aim_x);
        TEST_ASSERT_EQUAL_FLOAT(one.aim_y, late.aim_y);
        TEST_ASSERT_EQUAL_FLOAT(one.gale.x2, late.gale.x2);
        TEST_ASSERT_EQUAL_FLOAT(one.gale.y2, late.gale.y2);
    }
}

static void test_latch_single_subtick_matches_take_tick(void)
{
    InputLatch a;
    InputLatch b;
    input_latch_init(&a, 5.0f, 5.0f);
    input_latch_init(&b, 5.0f, 5.0f);
    RawInput raw = make_raw(40.0f, 20.0f);
    raw.left_down = true;
    raw.gale_pressed = true;
    input_latch_feed(&a, &raw);
    input_latch_feed(&b, &raw);

    const BrushInput whole = input_latch_take_tick(&a);
    const BrushInput sub = input_latch_take_subtick(&b, 1);
    TEST_ASSERT_EQUAL_FLOAT(whole.pmx, sub.pmx);
    TEST_ASSERT_EQUAL_FLOAT(whole.pmy, sub.pmy);
    TEST_ASSERT_EQUAL_FLOAT(whole.mx, sub.mx);
    TEST_ASSERT_EQUAL_FLOAT(whole.my, sub.my);
    TEST_ASSERT_EQUAL(whole.paint, sub.paint);
    TEST_ASSERT_EQUAL(whole.cyclone, sub.cyclone);
    TEST_ASSERT_EQUAL(whole.gale, sub.gale);
    TEST_ASSERT_EQUAL_FLOAT(a.prev_x, b.prev_x);
    TEST_ASSERT_EQUAL_FLOAT(a.prev_y, b.prev_y);
    TEST_ASSERT_EQUAL(a.gale_pending, b.gale_pending);
}

static void test_latch_zero_tick_frame_preserves_motion(void)
{
    InputLatch l;
    input_latch_init(&l, 0.0f, 0.0f);

    RawInput r1 = make_raw(10.0f, 0.0f);
    RawInput r2 = make_raw(20.0f, 0.0f);
    input_latch_feed(&l, &r1);
    input_latch_feed(&l, &r2);

    const BrushInput t = input_latch_take_tick(&l);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t.pmx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, t.mx);
}

static void test_latch_edge_press_held_and_coalesced(void)
{
    InputLatch l;
    input_latch_init(&l, 0.0f, 0.0f);

    RawInput press = make_raw(0.0f, 0.0f);
    press.cyclone_pressed = true;
    input_latch_feed(&l, &press);
    input_latch_feed(&l, &press);

    RawInput idle = make_raw(0.0f, 0.0f);
    input_latch_feed(&l, &idle);

    const BrushInput t1 = input_latch_take_tick(&l);
    const BrushInput t2 = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(t1.cyclone);
    TEST_ASSERT_FALSE(t2.cyclone);
}

static void test_latch_reward_commands_and_priority(void)
{
    InputLatch l;
    input_latch_init(&l, 0.0f, 0.0f);

    RawInput r = make_raw(0.0f, 0.0f);
    r.pick_pressed = 1;
    input_latch_feed(&l, &r);
    TEST_ASSERT_EQUAL_INT(CMD_NONE, input_latch_take_command(&l, RUN_PHASE_PLAYING).type);
    TEST_ASSERT_EQUAL_INT(CMD_NONE, input_latch_take_command(&l, RUN_PHASE_REWARD).type);

    input_latch_feed(&l, &r);
    const GameCommand c1 = input_latch_take_command(&l, RUN_PHASE_REWARD);
    TEST_ASSERT_EQUAL_INT(CMD_PICK_REWARD, c1.type);
    TEST_ASSERT_EQUAL_INT32(1, c1.arg);
    TEST_ASSERT_EQUAL_INT(CMD_NONE, input_latch_take_command(&l, RUN_PHASE_REWARD).type);

    RawInput both = make_raw(0.0f, 0.0f);
    both.pick_pressed = 2;
    both.reroll_pressed = true;
    both.repair_pressed = true;
    input_latch_feed(&l, &both);
    const GameCommand c2 = input_latch_take_command(&l, RUN_PHASE_REWARD);
    TEST_ASSERT_EQUAL_INT(CMD_PICK_REWARD, c2.type);
    TEST_ASSERT_EQUAL_INT32(2, c2.arg);
    TEST_ASSERT_EQUAL_INT(CMD_NONE, input_latch_take_command(&l, RUN_PHASE_REWARD).type);
}

static void test_latch_paint_suppression_after_ui_click(void)
{
    InputLatch l;
    input_latch_init(&l, 10.0f, 10.0f);

    RawInput ui = make_raw(10.0f, 10.0f);
    ui.left_down = true;
    ui.ui_click = true;
    input_latch_feed(&l, &ui);
    TEST_ASSERT_FALSE(input_latch_take_tick(&l).paint);

    RawInput still_down = make_raw(20.0f, 10.0f);
    still_down.left_down = true;
    input_latch_feed(&l, &still_down);
    TEST_ASSERT_FALSE(input_latch_take_tick(&l).paint);

    RawInput released = make_raw(20.0f, 10.0f);
    input_latch_feed(&l, &released);
    TEST_ASSERT_FALSE(input_latch_take_tick(&l).paint);

    RawInput pressed_again = make_raw(30.0f, 10.0f);
    pressed_again.left_down = true;
    input_latch_feed(&l, &pressed_again);
    TEST_ASSERT_TRUE(input_latch_take_tick(&l).paint);
}

static void test_latch_confirm_once(void)
{
    InputLatch l;
    input_latch_init(&l, 0.0f, 0.0f);

    RawInput r = make_raw(0.0f, 0.0f);
    r.confirm_pressed = true;
    input_latch_feed(&l, &r);
    TEST_ASSERT_TRUE(input_latch_take_confirm(&l));
    TEST_ASSERT_FALSE(input_latch_take_confirm(&l));
}

static void test_latch_ui_transition_suppresses_paint_until_release(void)
{
    InputLatch l;
    /* Simulate clicking a Brush Select card or Pause Resume button while left button is still held
     */
    input_latch_reset_for_ui_transition(&l, 320.0f, 240.0f, true);
    TEST_ASSERT_TRUE(l.paint_suppressed);
    TEST_ASSERT_FALSE(input_latch_take_tick(&l).paint);

    /* Dragging while button stays held must not paint */
    RawInput drag = make_raw(420.0f, 280.0f);
    drag.left_down = true;
    input_latch_feed(&l, &drag);
    const BrushInput held_tick = input_latch_take_tick(&l);
    TEST_ASSERT_FALSE(held_tick.paint);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 320.0f, held_tick.pmx);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 420.0f, held_tick.mx);

    /* Releasing button clears suppression */
    RawInput up = make_raw(420.0f, 280.0f);
    up.left_down = false;
    input_latch_feed(&l, &up);
    TEST_ASSERT_FALSE(l.paint_suppressed);
    TEST_ASSERT_FALSE(input_latch_take_tick(&l).paint);

    /* Next press paints normally */
    RawInput down = make_raw(440.0f, 290.0f);
    down.left_down = true;
    input_latch_feed(&l, &down);
    TEST_ASSERT_TRUE(input_latch_take_tick(&l).paint);

    /* Keyboard transition (left_down == false) does not leave paint_suppressed stuck */
    input_latch_reset_for_ui_transition(&l, 100.0f, 100.0f, false);
    TEST_ASSERT_FALSE(l.paint_suppressed);
    RawInput press = make_raw(110.0f, 100.0f);
    press.left_down = true;
    input_latch_feed(&l, &press);
    TEST_ASSERT_TRUE(input_latch_take_tick(&l).paint);
}

static void test_latch_endless_crystal_command(void)
{
    InputLatch l;
    input_latch_init(&l, 0.0f, 0.0f);

    /* Non-reward phase must not emit CMD_PICK_ENDLESS_CRYSTAL and must clear pending flag */
    RawInput cry = make_raw(0.0f, 0.0f);
    cry.crystal_pressed = true;
    input_latch_feed(&l, &cry);
    TEST_ASSERT_EQUAL_INT(CMD_NONE, input_latch_take_command(&l, RUN_PHASE_PLAYING).type);
    TEST_ASSERT_EQUAL_INT(CMD_NONE, input_latch_take_command(&l, RUN_PHASE_REWARD).type);

    /* RUN_PHASE_REWARD emits CMD_PICK_ENDLESS_CRYSTAL exactly once */
    input_latch_feed(&l, &cry);
    const GameCommand c1 = input_latch_take_command(&l, RUN_PHASE_REWARD);
    TEST_ASSERT_EQUAL_INT(CMD_PICK_ENDLESS_CRYSTAL, c1.type);
    TEST_ASSERT_EQUAL_INT32(0, c1.arg);
    TEST_ASSERT_EQUAL_INT(CMD_NONE, input_latch_take_command(&l, RUN_PHASE_REWARD).type);

    /* UI transition reset clears crystal_pending */
    input_latch_feed(&l, &cry);
    TEST_ASSERT_TRUE(l.crystal_pending);
    input_latch_reset_for_ui_transition(&l, 10.0f, 20.0f, false);
    TEST_ASSERT_FALSE(l.crystal_pending);
    TEST_ASSERT_EQUAL_INT(CMD_NONE, input_latch_take_command(&l, RUN_PHASE_REWARD).type);
}

static void test_hints_wait_while_hidden_and_never_duplicate(void)
{
    HintQueue q;
    hint_queue_clear(&q);
    TEST_ASSERT_TRUE(hint_queue_push(&q, HINT_STROKE));
    TEST_ASSERT_FALSE(hint_queue_push(&q, HINT_STROKE));
    TEST_ASSERT_TRUE(hint_queue_push(&q, HINT_INK_REGEN));
    HintId active = HINT_STROKE;
    float active_t = 0.0f;

    for (int i = 0; i < 300; ++i) {
        TEST_ASSERT_EQUAL_INT32(-1, hint_queue_step(&q, &active, &active_t, 1.0f / 60.0f, false));
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, active_t);

    TEST_ASSERT_EQUAL_FLOAT(4.0f, HINT_SHOW_SECONDS);

    TEST_ASSERT_EQUAL_INT32(HINT_STROKE,
                            hint_queue_step(&q, &active, &active_t, 1.0f / 60.0f, true));
    TEST_ASSERT_EQUAL_FLOAT(HINT_SHOW_SECONDS, active_t);
    for (int i = 0; i < 300; ++i) {
        (void)hint_queue_step(&q, &active, &active_t, 1.0f / 60.0f, false);
    }
    TEST_ASSERT_EQUAL_FLOAT(HINT_SHOW_SECONDS, active_t);

    /* Verify active_t remains positive at 2.5s before transitioning after HINT_SHOW_SECONDS. */
    const int mid_ticks = 150;
    int32_t started = -1;
    for (int i = 0; i < mid_ticks; ++i) {
        started = hint_queue_step(&q, &active, &active_t, 1.0f / 60.0f, true);
        TEST_ASSERT_EQUAL_INT32(-1, started);
    }
    TEST_ASSERT_EQUAL_INT(HINT_STROKE, active);
    TEST_ASSERT_TRUE(active_t > 1.0f);

    const int max_ticks = (int)(HINT_SHOW_SECONDS * 60.0f) + 15;
    for (int i = mid_ticks; i < max_ticks && started < 0; ++i) {
        started = hint_queue_step(&q, &active, &active_t, 1.0f / 60.0f, true);
    }
    TEST_ASSERT_EQUAL_INT32(HINT_INK_REGEN, started);
}

static void test_bgm_interrupted_fade_out_never_swells_back(void)
{
    BgmFade f;
    bgm_fade_reset(&f);
    BgmFadeActions act = bgm_fade_request(&f, BGM_BATTLE);
    TEST_ASSERT_EQUAL_INT(BGM_BATTLE, act.play);
    for (int i = 0; i < 40; ++i) {
        (void)bgm_fade_step(&f, 1.0f / 60.0f);
    }
    TEST_ASSERT_EQUAL_FLOAT(1.0f, f.gain_cur);

    act = bgm_fade_request(&f, BGM_NONE);
    TEST_ASSERT_EQUAL_INT(BGM_NONE, act.play);
    for (int i = 0; i < 15; ++i) {
        (void)bgm_fade_step(&f, 1.0f / 60.0f);
    }
    const float battle_gain = f.gain_prev;
    TEST_ASSERT_TRUE(battle_gain < 0.6f);

    act = bgm_fade_request(&f, BGM_LOBBY);
    TEST_ASSERT_EQUAL_INT(BGM_LOBBY, act.play);
    TEST_ASSERT_EQUAL_INT(BGM_NONE, act.stop);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f.gain_cur);
    TEST_ASSERT_EQUAL_INT(BGM_BATTLE, f.prev);
    TEST_ASSERT_EQUAL_FLOAT(battle_gain, f.gain_prev);
    (void)bgm_fade_step(&f, 1.0f / 60.0f);
    TEST_ASSERT_TRUE(f.gain_prev < battle_gain);
}

static void test_bgm_returning_to_fading_track_swaps_without_restart(void)
{
    BgmFade f;
    bgm_fade_reset(&f);
    (void)bgm_fade_request(&f, BGM_BATTLE);
    for (int i = 0; i < 40; ++i) {
        (void)bgm_fade_step(&f, 1.0f / 60.0f);
    }
    (void)bgm_fade_request(&f, BGM_BOSS);
    for (int i = 0; i < 12; ++i) {
        (void)bgm_fade_step(&f, 1.0f / 60.0f);
    }
    const float boss_gain = f.gain_cur;
    const float battle_gain = f.gain_prev;
    const BgmFadeActions act = bgm_fade_request(&f, BGM_BATTLE);
    TEST_ASSERT_EQUAL_INT(BGM_NONE, act.play);
    TEST_ASSERT_EQUAL_INT(BGM_NONE, act.stop);
    TEST_ASSERT_EQUAL_INT(BGM_BATTLE, f.cur);
    TEST_ASSERT_EQUAL_FLOAT(battle_gain, f.gain_cur);
    TEST_ASSERT_EQUAL_INT(BGM_BOSS, f.prev);
    TEST_ASSERT_EQUAL_FLOAT(boss_gain, f.gain_prev);
    BgmTrack finished = BGM_NONE;
    for (int i = 0; i < 60 && finished == BGM_NONE; ++i) {
        finished = bgm_fade_step(&f, 1.0f / 60.0f);
    }
    TEST_ASSERT_EQUAL_INT(BGM_BOSS, finished);
}

static void test_menu_resting_mouse_yields_to_keyboard_until_moved(void)
{
    UiPointer p = {0};
    TEST_ASSERT_TRUE(menu_pointer_track(&p, 100.0f, 100.0f));
    TEST_ASSERT_EQUAL_INT32(2, menu_pointer_visible_hover(&p, 2, true, false));

    TEST_ASSERT_FALSE(menu_pointer_track(&p, 100.0f, 100.0f));
    TEST_ASSERT_EQUAL_INT32(UI_HOVER_NONE, menu_pointer_visible_hover(&p, 2, false, true));
    TEST_ASSERT_FALSE(menu_pointer_track(&p, 100.2f, 100.0f));
    TEST_ASSERT_EQUAL_INT32(UI_HOVER_NONE, menu_pointer_visible_hover(&p, 2, false, false));

    TEST_ASSERT_TRUE(menu_pointer_track(&p, 140.0f, 100.0f));
    TEST_ASSERT_EQUAL_INT32(1, menu_pointer_visible_hover(&p, 1, true, false));

    /* A keyboard shortcut pressed on the same frame as mouse motion still suppresses hover until
     * a later mouse move arrives without a keyboard shortcut. */
    TEST_ASSERT_EQUAL_INT32(UI_HOVER_NONE, menu_pointer_visible_hover(&p, 1, true, true));
    TEST_ASSERT_EQUAL_INT32(UI_HOVER_NONE, menu_pointer_visible_hover(&p, 1, false, false));
    TEST_ASSERT_EQUAL_INT32(1, menu_pointer_visible_hover(&p, 1, true, false));
}

static void test_menu_new_context_keeps_default_focus_under_resting_mouse(void)
{
    UiPointer p = {0};
    menu_pointer_enter_context(&p, 1);
    bool moved = menu_pointer_track(&p, 500.0f, 410.0f);
    TEST_ASSERT_EQUAL_INT32(0, menu_pointer_visible_hover(&p, 0, moved, false));

    menu_pointer_enter_context(&p, 2);
    moved = menu_pointer_track(&p, 500.0f, 410.0f);
    TEST_ASSERT_EQUAL_INT32(UI_HOVER_NONE, menu_pointer_visible_hover(&p, 0, moved, false));
    menu_pointer_enter_context(&p, 2);
    moved = menu_pointer_track(&p, 500.0f, 410.0f);
    TEST_ASSERT_EQUAL_INT32(UI_HOVER_NONE, menu_pointer_visible_hover(&p, 0, moved, false));

    moved = menu_pointer_track(&p, 520.0f, 410.0f);
    TEST_ASSERT_EQUAL_INT32(0, menu_pointer_visible_hover(&p, 0, moved, false));
}

static void test_menu_bokuan_grid_navigation(void)
{
    const int32_t upg = META_UPG_COUNT;
    const int32_t back = META_UPG_COUNT + META_RELIC_UNLOCK_COUNT;
    typedef struct {
        int32_t from;
        MenuNavKey key;
        int32_t to;
    } NavCase;
    const NavCase cases[] = {
        {0, MENU_NAV_RIGHT, upg + 0},
        {1, MENU_NAV_RIGHT, upg + 2},
        {3, MENU_NAV_RIGHT, upg + 4},
        {upg + 0, MENU_NAV_RIGHT, upg + 1},
        {upg + 1, MENU_NAV_RIGHT, upg + 1},
        {upg + 1, MENU_NAV_LEFT, upg + 0},
        {upg + 3, MENU_NAV_LEFT, upg + 2},
        {upg + 2, MENU_NAV_LEFT, 1},
        {upg + 4, MENU_NAV_LEFT, 2},
        {upg + 1, MENU_NAV_DOWN, upg + 3},
        {upg + 5, MENU_NAV_DOWN, back},
        {upg + 0, MENU_NAV_UP, back},
        {0, MENU_NAV_UP, back},
        {upg - 1, MENU_NAV_DOWN, back},
        {back, MENU_NAV_DOWN, 0},
        {back, MENU_NAV_UP, upg - 1},
        {0, MENU_NAV_LEFT, 0},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        TEST_ASSERT_EQUAL_INT32(cases[i].to, menu_nav_bokuan(cases[i].from, cases[i].key));
    }
}

static void test_menu_result_cursor_walks_visible_buttons(void)
{
    TEST_ASSERT_EQUAL_INT32(MENU_RESULT_RETRY,
                            menu_nav_result(MENU_RESULT_LOBBY, false, MENU_NAV_RIGHT));
    TEST_ASSERT_EQUAL_INT32(MENU_RESULT_LOBBY,
                            menu_nav_result(MENU_RESULT_RETRY, false, MENU_NAV_RIGHT));
    TEST_ASSERT_EQUAL_INT32(MENU_RESULT_ENDLESS,
                            menu_nav_result(MENU_RESULT_LOBBY, true, MENU_NAV_LEFT));
    TEST_ASSERT_EQUAL_INT32(MENU_RESULT_RETRY,
                            menu_nav_result(MENU_RESULT_ENDLESS, true, MENU_NAV_LEFT));
    TEST_ASSERT_EQUAL_INT32(MENU_RESULT_LOBBY,
                            menu_nav_result(MENU_RESULT_ENDLESS, false, MENU_NAV_RIGHT));
}

static void test_menu_lobby_stroke_starts_only_on_open_canvas(void)
{
    bool active = false;
    TEST_ASSERT_FALSE(menu_lobby_stroke_gate(&active, true, true, true));
    TEST_ASSERT_FALSE(menu_lobby_stroke_gate(&active, false, true, false));
    TEST_ASSERT_FALSE(menu_lobby_stroke_gate(&active, false, false, false));

    TEST_ASSERT_TRUE(menu_lobby_stroke_gate(&active, true, true, false));
    TEST_ASSERT_TRUE(menu_lobby_stroke_gate(&active, false, true, true));
    TEST_ASSERT_FALSE(menu_lobby_stroke_gate(&active, false, false, true));
}

static void test_menu_guide_page_clamps_to_bounds(void)
{
    TEST_ASSERT_EQUAL_INT32(0, menu_nav_guide_page(0, -1));
    TEST_ASSERT_EQUAL_INT32(0, menu_nav_guide_page(-5, 0));
    TEST_ASSERT_EQUAL_INT32(1, menu_nav_guide_page(0, 1));
    TEST_ASSERT_EQUAL_INT32(3, menu_nav_guide_page(4, -1));
    TEST_ASSERT_EQUAL_INT32(GUIDE_SLIDE_COUNT - 1, menu_nav_guide_page(GUIDE_SLIDE_COUNT - 1, 1));
    TEST_ASSERT_EQUAL_INT32(GUIDE_SLIDE_COUNT - 1, menu_nav_guide_page(99, 0));
    for (int32_t i = 0; i < GUIDE_SLIDE_COUNT; ++i) {
        TEST_ASSERT_EQUAL_INT32(i, menu_nav_guide_page(i, 0));
    }
}

static void test_menu_transition_frame_skips_gameplay(void)
{
    TEST_ASSERT_FALSE(menu_action_ends_frame(MENU_ACT_NONE));
    TEST_ASSERT_TRUE(menu_action_ends_frame(MENU_ACT_PAUSE_RESUME));
    TEST_ASSERT_TRUE(menu_action_ends_frame(MENU_ACT_BRUSH_PICK));
    TEST_ASSERT_TRUE(menu_action_ends_frame(MENU_ACT_RESULT_RETRY));
    TEST_ASSERT_TRUE(menu_action_ends_frame(MENU_ACT_LOBBY_GUIDE));
    TEST_ASSERT_TRUE(menu_action_ends_frame(MENU_ACT_GUIDE_PREV));
    TEST_ASSERT_TRUE(menu_action_ends_frame(MENU_ACT_GUIDE_NEXT));
    TEST_ASSERT_TRUE(menu_action_ends_frame(MENU_ACT_GUIDE_JUMP));
}

static void test_latch_anchor_starts_stroke_at_press_but_keeps_skills(void)
{
    InputLatch l;
    input_latch_init(&l, 160.0f, 400.0f);
    const RawInput queued = {
        .mouse_x = 160.0f, .mouse_y = 400.0f, .gale_pressed = true, .pick_pressed = -1};
    input_latch_feed(&l, &queued);
    input_latch_anchor(&l, 450.0f, 400.0f);
    const RawInput press = {
        .mouse_x = 450.0f, .mouse_y = 400.0f, .left_down = true, .pick_pressed = -1};
    input_latch_feed(&l, &press);
    const BrushInput in = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(in.paint);
    TEST_ASSERT_EQUAL_FLOAT(450.0f, in.pmx);
    TEST_ASSERT_EQUAL_FLOAT(450.0f, in.mx);
    TEST_ASSERT_TRUE(in.gale);
}

static void test_latch_resume_reanchors_and_drops_queued_input(void)
{
    InputLatch l;
    input_latch_init(&l, 10.0f, 10.0f);
    RawInput raw = {.mouse_x = 600.0f, .mouse_y = 400.0f, .left_down = true, .pick_pressed = 1};
    raw.cyclone_pressed = true;
    input_latch_feed(&l, &raw);

    input_latch_reset_for_ui_transition(&l, 600.0f, 400.0f, false);
    const BrushInput in = input_latch_take_tick(&l);
    TEST_ASSERT_EQUAL_FLOAT(600.0f, in.pmx);
    TEST_ASSERT_EQUAL_FLOAT(600.0f, in.mx);
    TEST_ASSERT_FALSE(in.cyclone);
    TEST_ASSERT_EQUAL_INT(CMD_NONE, input_latch_take_command(&l, RUN_PHASE_REWARD).type);
}

typedef struct {
    pthread_mutex_t sync_mtx;
    pthread_cond_t sync_cv;
    bool detached;
    bool stop;
    size_t active_callbacks;
    size_t active_nonzero_bytes;
    size_t silent_callbacks;
    size_t silent_nonzero_bytes;
    bool guard_corrupted;
} AudioCbWorkerCtx;

static void *audio_cb_test_worker(void *arg)
{
    static const unsigned int k_chunks[4] = {330u, 550u, 735u, 1024u};
    AudioCbWorkerCtx *ctx = (AudioCbWorkerCtx *)arg;
    unsigned char raw[1u + 1024u * sizeof(int16_t) + 1u];
    size_t step = 0u;
    const struct timespec pause_ns = {0, 100000L};

    for (;;) {
        (void)pthread_mutex_lock(&ctx->sync_mtx);
        if (ctx->stop) {
            (void)pthread_mutex_unlock(&ctx->sync_mtx);
            break;
        }
        const bool was_detached = ctx->detached;
        (void)pthread_mutex_unlock(&ctx->sync_mtx);

        const unsigned int frames = k_chunks[step & 3u];
        const size_t total_bytes = (size_t)frames * sizeof(int16_t);
        step++;

        /* Fill with sentinel so detached callbacks must actively overwrite every byte with zero,
         * and keep 1-byte misaligned output plus leading/trailing guard bytes. */
        memset(raw, 0xA5, sizeof raw);
        audio_cb_render(raw + 1u, frames);

        const bool guard_bad = (raw[0] != 0xA5u) || (raw[1u + total_bytes] != 0xA5u);
        size_t nonzero = 0u;
        for (size_t b = 0u; b < total_bytes; ++b) {
            if (raw[1u + b] != 0u) {
                nonzero++;
            }
        }

        (void)pthread_mutex_lock(&ctx->sync_mtx);
        if (guard_bad) {
            ctx->guard_corrupted = true;
        }
        if (was_detached) {
            ctx->silent_callbacks++;
            ctx->silent_nonzero_bytes += nonzero;
        } else {
            ctx->active_callbacks++;
            ctx->active_nonzero_bytes += nonzero;
        }
        (void)pthread_cond_signal(&ctx->sync_cv);
        (void)pthread_mutex_unlock(&ctx->sync_mtx);
        (void)nanosleep(&pause_ns, NULL);
    }
    return NULL;
}

static void test_audio_cb_concurrent_render_and_detach(void)
{
    GameState *g = game_create(NULL);
    AudioState *a = calloc(1, sizeof *a);
    TEST_ASSERT_NOT_NULL(g);
    TEST_ASSERT_NOT_NULL(a);

    audio_init(a, UINT64_C(11));
    audio_set_realtime_mode(a, true);
    audio_set_volumes(a, 100, 0, 100);
    audio_set_external_bgm(a, true);

    audio_cb_lock();
    audio_cb_attach_locked(a);
    audio_trigger_ui_stamp(a);
    audio_cb_unlock();

    AudioCbWorkerCtx ctx;
    memset(&ctx, 0, sizeof ctx);
    TEST_ASSERT_EQUAL_INT(0, pthread_mutex_init(&ctx.sync_mtx, NULL));
    TEST_ASSERT_EQUAL_INT(0, pthread_cond_init(&ctx.sync_cv, NULL));

    pthread_t worker;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&worker, NULL, audio_cb_test_worker, &ctx));

    /* Feed live audio ticks and wait until the worker proves it rendered audible bytes before
     * detaching, avoiding any CI scheduling race. */
    const struct timespec tick_ns = {0, 200000L};
    for (int i = 0; i < 30; ++i) {
        g->player.painting = (i % 3) != 0;
        g->player.stroke_speed = g->player.painting ? 700.0f : 0.0f;
        g->event_count = 0;
        if (i % 4 == 0) {
            game_push_event(g, (GameEvent){.type = EV_GALE});
        } else if (i % 5 == 0) {
            game_push_event(g, (GameEvent){.type = EV_CYCLONE});
        }
        audio_cb_lock();
        audio_consume_events(a, g);
        if (i == 12) {
            audio_set_paused(a, true);
        } else if (i == 16) {
            audio_set_paused(a, false);
        }
        audio_cb_unlock();
        (void)nanosleep(&tick_ns, NULL);
    }

    (void)pthread_mutex_lock(&ctx.sync_mtx);
    while (ctx.active_callbacks == 0u || ctx.active_nonzero_bytes == 0u) {
        (void)pthread_cond_wait(&ctx.sync_cv, &ctx.sync_mtx);
    }
    (void)pthread_mutex_unlock(&ctx.sync_mtx);

    /* Detach through the real production helper while the callback thread is running. */
    audio_cb_detach();
    (void)pthread_mutex_lock(&ctx.sync_mtx);
    ctx.detached = true;
    while (ctx.silent_callbacks == 0u) {
        (void)pthread_cond_wait(&ctx.sync_cv, &ctx.sync_mtx);
    }
    ctx.stop = true;
    (void)pthread_mutex_unlock(&ctx.sync_mtx);

    TEST_ASSERT_EQUAL_INT(0, pthread_join(worker, NULL));
    (void)pthread_cond_destroy(&ctx.sync_cv);
    (void)pthread_mutex_destroy(&ctx.sync_mtx);

    TEST_ASSERT_FALSE(ctx.guard_corrupted);
    TEST_ASSERT_TRUE(ctx.active_callbacks > 0u);
    TEST_ASSERT_TRUE(ctx.active_nonzero_bytes > 0u);
    TEST_ASSERT_TRUE(ctx.silent_callbacks > 0u);
    TEST_ASSERT_EQUAL_size_t(0u, ctx.silent_nonzero_bytes);
    TEST_ASSERT_TRUE(audio_validate_finite(a));

    free(a);
    game_destroy(g);
}

static void test_hints_off_clears_queued_and_active_hints(void)
{
    HintQueue q;
    hint_queue_clear(&q);
    TEST_ASSERT_TRUE(hint_queue_push(&q, HINT_STROKE));
    TEST_ASSERT_TRUE(hint_queue_push(&q, HINT_INK_REGEN));
    HintId active = HINT_STROKE;
    float active_t = 0.0f;

    TEST_ASSERT_EQUAL_INT32(
        HINT_STROKE, hint_queue_step_enabled(&q, &active, &active_t, 1.0f / 60.0f, true, true));
    TEST_ASSERT_EQUAL_FLOAT(HINT_SHOW_SECONDS, active_t);

    /* Turning hints OFF drops both active and queued hints without marking anything seen. */
    TEST_ASSERT_EQUAL_INT32(
        -1, hint_queue_step_enabled(&q, &active, &active_t, 1.0f / 60.0f, true, false));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, active_t);
    TEST_ASSERT_EQUAL_INT32(0, q.count);

    for (int i = 0; i < 10; ++i) {
        TEST_ASSERT_EQUAL_INT32(
            -1, hint_queue_step_enabled(&q, &active, &active_t, 1.0f / 60.0f, true, true));
    }
    TEST_ASSERT_EQUAL_FLOAT(0.0f, active_t);
}

static FILE *test_fail_fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    errno = EIO;
    return NULL;
}

static void test_dirty_save_retries_after_transient_write_failure(void)
{
    const char *base = getenv("TMPDIR");
    if (base == NULL || base[0] == '\0') {
        base = "/tmp";
    }
    char dir[PATH_MAX];
    const int dn = snprintf(dir, sizeof dir, "%s/inkstorm-dirty-XXXXXX", base);
    TEST_ASSERT_TRUE(dn > 0 && (size_t)dn < sizeof dir);
    TEST_ASSERT_NOT_NULL(mkdtemp(dir));

    char path[PATH_MAX + 32];
    const int pn = snprintf(path, sizeof path, "%s/save.txt", dir);
    TEST_ASSERT_TRUE(pn > 0 && (size_t)pn < sizeof path);

    MetaState meta;
    meta_init_default(&meta);
    meta.runs_played = 5;
    meta.ink_crystals = 140;

    bool dirty = false;
    const SaveIoOps fail_ops = {.fopen = test_fail_fopen};
    TEST_ASSERT_FALSE(save_persist_meta(path, true, &meta, &dirty, &fail_ops));
    TEST_ASSERT_TRUE(dirty);

    TEST_ASSERT_TRUE(save_retry_if_dirty(path, true, &meta, &dirty, NULL));
    TEST_ASSERT_FALSE(dirty);

    /* Clean state is a no-op even if the disk later fails. */
    TEST_ASSERT_TRUE(save_retry_if_dirty(path, true, &meta, &dirty, &fail_ops));
    TEST_ASSERT_FALSE(dirty);

    MetaState loaded;
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_OK, save_load(path, &loaded));
    TEST_ASSERT_EQUAL_INT32(5, loaded.runs_played);
    TEST_ASSERT_EQUAL_INT32(140, loaded.ink_crystals);

    (void)remove(path);
    (void)rmdir(dir);
}

static void test_bgm_pause_duck_ramps_smoothly(void)
{
    BgmFade f;
    bgm_fade_reset(&f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, f.pause_duck);

    const float after_one_pause = bgm_fade_step_duck(&f, true, 1.0f / 60.0f);
    TEST_ASSERT_TRUE(after_one_pause < 1.0f && after_one_pause > 0.35f);

    float duck = after_one_pause;
    for (int i = 0; i < 10; ++i) {
        duck = bgm_fade_step_duck(&f, true, 1.0f / 60.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.35f, duck);

    const float after_one_resume = bgm_fade_step_duck(&f, false, 1.0f / 60.0f);
    TEST_ASSERT_TRUE(after_one_resume > 0.35f && after_one_resume < 1.0f);

    for (int i = 0; i < 10; ++i) {
        duck = bgm_fade_step_duck(&f, false, 1.0f / 60.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, duck);
}

static void test_esc_resume_with_held_lmb_suppresses_paint(void)
{
    InputLatch l;
    input_latch_init(&l, 400.0f, 300.0f);
    /* Resuming via Esc while LMB is held must suppress paint until LMB is released. */
    input_latch_reset_for_ui_transition(&l, 400.0f, 300.0f, true);

    RawInput held = make_raw(450.0f, 320.0f);
    held.left_down = true;
    input_latch_feed(&l, &held);
    TEST_ASSERT_FALSE(input_latch_take_tick(&l).paint);

    RawInput released = make_raw(450.0f, 320.0f);
    input_latch_feed(&l, &released);
    TEST_ASSERT_FALSE(input_latch_take_tick(&l).paint);

    RawInput repressed = make_raw(470.0f, 320.0f);
    repressed.left_down = true;
    input_latch_feed(&l, &repressed);
    TEST_ASSERT_TRUE(input_latch_take_tick(&l).paint);
}

static int g_fail_fopen_calls = 0;
static int g_fail_fopen_limit = 0;

static FILE *test_fail_n_fopen(const char *path, const char *mode)
{
    ++g_fail_fopen_calls;
    if (g_fail_fopen_calls <= g_fail_fopen_limit) {
        errno = EIO;
        return NULL;
    }
    return fopen(path, mode);
}

static void test_exit_settlement_drains_act3_boss_victory_and_retries_dirty_save(void)
{
    const char *base = getenv("TMPDIR");
    if (base == NULL || base[0] == '\0') {
        base = "/tmp";
    }
    char dir[PATH_MAX];
    const int dn = snprintf(dir, sizeof dir, "%s/inkstorm-exit-XXXXXX", base);
    TEST_ASSERT_TRUE(dn > 0 && (size_t)dn < sizeof dir);
    TEST_ASSERT_NOT_NULL(mkdtemp(dir));

    char path[PATH_MAX + 32];
    const int pn = snprintf(path, sizeof path, "%s/save.txt", dir);
    TEST_ASSERT_TRUE(pn > 0 && (size_t)pn < sizeof path);

    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_act(g, 2);
    g->run.acts_cleared = 2;
    for (int t = 0; t < 240 && g->phase == RUN_PHASE_ACT_TRANSITION; ++t) {
        game_step(g, NULL, NULL);
    }
    game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
    for (int s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
        (void)combat_apply_damage(g, g->ryu_body.orb_ids[s], 500, DMG_PHYSICAL, 0.0f, 0.0f);
    }
    ecs_flush_destroy(&g->ecs);

    const EntityId head = g->run.boss_id;
    game_health_mut(g, head)->invuln_t = 0.0f;
    game_health_mut(g, head)->hp = 10;
    (void)combat_apply_damage(g, head, 100, DMG_PHYSICAL, 0.0f, 0.0f);
    g->hitstop_t = 0.05f;
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    TEST_ASSERT_TRUE(game_wave_goal_met(g));

    MetaState meta;
    meta_init_default(&meta);
    bool dirty = false;
    MetaSettleBreakdown br = {0};
    g_fail_fopen_calls = 0;
    g_fail_fopen_limit = 1;
    const SaveIoOps fail_once_ops = {.fopen = test_fail_n_fopen};

    /* Production exit helper drains hitstop + victory aftermath, settles 1 campaign clear, and
     * retries a transient first write failure before shutdown so one exit call persists on disk. */
    TEST_ASSERT_TRUE(
        save_finish_run_on_exit(path, true, &meta, &dirty, g, true, &br, &fail_once_ops));
    TEST_ASSERT_EQUAL_INT(2, g_fail_fopen_calls);
    TEST_ASSERT_FALSE(dirty);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_VICTORY, g->phase);
    TEST_ASSERT_TRUE(g->run.run_settled);
    TEST_ASSERT_EQUAL_INT32(1, meta.clears);
    TEST_ASSERT_EQUAL_INT32(1, meta.runs_played);

    const MetaRunResult res = game_run_result(g);
    const MetaSettleBreakdown expected = meta_calc_crystals(&res);
    TEST_ASSERT_TRUE(expected.total_crystals >= 120);
    TEST_ASSERT_EQUAL_INT32(expected.total_crystals, br.total_crystals);
    TEST_ASSERT_EQUAL_INT32(expected.total_crystals, meta.ink_crystals);

    /* Calling exit settlement again on an already-settled run is an idempotent no-op. */
    TEST_ASSERT_TRUE(
        save_finish_run_on_exit(path, true, &meta, &dirty, g, true, NULL, &fail_once_ops));
    TEST_ASSERT_EQUAL_INT(2, g_fail_fopen_calls);
    TEST_ASSERT_EQUAL_INT32(1, meta.clears);
    TEST_ASSERT_EQUAL_INT32(1, meta.runs_played);
    TEST_ASSERT_EQUAL_INT32(expected.total_crystals, meta.ink_crystals);

    /* Reload from disk and verify persisted campaign clear + exact victory crystals. */
    MetaState loaded;
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_OK, save_load(path, &loaded));
    TEST_ASSERT_EQUAL_INT32(1, loaded.clears);
    TEST_ASSERT_EQUAL_INT32(1, loaded.runs_played);
    TEST_ASSERT_EQUAL_INT32(expected.total_crystals, loaded.ink_crystals);

    /* Act-1 intro scroll exit does not abort or count as a played run. */
    GameState *intro = game_create(NULL);
    TEST_ASSERT_NOT_NULL(intro);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_ACT_TRANSITION, intro->phase);
    TEST_ASSERT_EQUAL_INT32(0, intro->run.act_index);
    TEST_ASSERT_TRUE(save_finish_run_on_exit(path, true, &meta, &dirty, intro, true, NULL, NULL));
    TEST_ASSERT_EQUAL_INT32(1, meta.runs_played);
    TEST_ASSERT_FALSE(intro->run.run_settled);
    game_destroy(intro);

    /* Mid-run exit aborts and settles as a non-clear run. */
    GameState *mid = game_create(NULL);
    TEST_ASSERT_NOT_NULL(mid);
    game_debug_start_wave(mid, 0);
    TEST_ASSERT_TRUE(save_finish_run_on_exit(path, true, &meta, &dirty, mid, true, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_GAME_OVER, mid->phase);
    TEST_ASSERT_TRUE(mid->run.run_settled);
    TEST_ASSERT_EQUAL_INT32(1, meta.clears);
    TEST_ASSERT_EQUAL_INT32(2, meta.runs_played);
    game_destroy(mid);

    game_destroy(g);
    (void)remove(path);
    (void)rmdir(dir);
}

static void test_exit_settlement_double_write_failure_recovers_on_dirty_retry(void)
{
    const char *base = getenv("TMPDIR");
    if (base == NULL || base[0] == '\0') {
        base = "/tmp";
    }
    char dir[PATH_MAX];
    const int dn = snprintf(dir, sizeof dir, "%s/inkstorm-exit2-XXXXXX", base);
    TEST_ASSERT_TRUE(dn > 0 && (size_t)dn < sizeof dir);
    TEST_ASSERT_NOT_NULL(mkdtemp(dir));

    char path[PATH_MAX + 32];
    const int pn = snprintf(path, sizeof path, "%s/save.txt", dir);
    TEST_ASSERT_TRUE(pn > 0 && (size_t)pn < sizeof path);

    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);

    MetaState meta;
    meta_init_default(&meta);
    bool dirty = false;
    g_fail_fopen_calls = 0;
    g_fail_fopen_limit = 2;
    const SaveIoOps fail_twice_ops = {.fopen = test_fail_n_fopen};

    /* Initial exit write and built-in exit retry both fail: run settles in memory, dirty stays
     * set, and nothing is written to disk yet. */
    TEST_ASSERT_FALSE(
        save_finish_run_on_exit(path, true, &meta, &dirty, g, true, NULL, &fail_twice_ops));
    TEST_ASSERT_EQUAL_INT(2, g_fail_fopen_calls);
    TEST_ASSERT_TRUE(dirty);
    TEST_ASSERT_TRUE(g->run.run_settled);
    TEST_ASSERT_EQUAL_INT32(1, meta.runs_played);

    MetaState missing_check;
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_MISSING, save_load(path, &missing_check));

    /* A later retry via save_retry_if_dirty flushes the settled state cleanly. */
    TEST_ASSERT_TRUE(save_retry_if_dirty(path, true, &meta, &dirty, &fail_twice_ops));
    TEST_ASSERT_EQUAL_INT(3, g_fail_fopen_calls);
    TEST_ASSERT_FALSE(dirty);

    MetaState loaded;
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_OK, save_load(path, &loaded));
    TEST_ASSERT_EQUAL_INT32(1, loaded.runs_played);

    /* An earlier mid-run dirty flag (without settling a new run) is also flushed on exit. */
    meta.ink_crystals += 55;
    dirty = true;
    TEST_ASSERT_TRUE(save_finish_run_on_exit(path, true, &meta, &dirty, g, false, NULL, NULL));
    TEST_ASSERT_FALSE(dirty);
    TEST_ASSERT_EQUAL_INT(SAVE_LOAD_OK, save_load(path, &loaded));
    TEST_ASSERT_EQUAL_INT32(meta.ink_crystals, loaded.ink_crystals);

    game_destroy(g);
    (void)remove(path);
    (void)rmdir(dir);
}

static void test_resolve_and_settle_matches_exit_for_act3_boss_hitstop(void)
{
    GameState *g_pause = game_create(NULL);
    GameState *g_exit = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g_pause);
    TEST_ASSERT_NOT_NULL(g_exit);

    GameState *pair[2] = {g_pause, g_exit};
    for (int i = 0; i < 2; ++i) {
        GameState *g = pair[i];
        game_debug_start_act(g, 2);
        g->run.acts_cleared = 2;
        for (int t = 0; t < 240 && g->phase == RUN_PHASE_ACT_TRANSITION; ++t) {
            game_step(g, NULL, NULL);
        }
        game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX);
        for (int s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
            (void)combat_apply_damage(g, g->ryu_body.orb_ids[s], 500, DMG_PHYSICAL, 0.0f, 0.0f);
        }
        ecs_flush_destroy(&g->ecs);
        const EntityId head = g->run.boss_id;
        game_health_mut(g, head)->invuln_t = 0.0f;
        game_health_mut(g, head)->hp = 10;
        (void)combat_apply_damage(g, head, 100, DMG_PHYSICAL, 0.0f, 0.0f);
        g->hitstop_t = 0.05f;
        TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
        TEST_ASSERT_TRUE(game_wave_goal_met(g));
    }

    MetaState meta_pause;
    MetaState meta_exit;
    meta_init_default(&meta_pause);
    meta_init_default(&meta_exit);
    MetaSettleBreakdown br_pause = {0};
    MetaSettleBreakdown br_exit = {0};
    bool dirty_exit = false;

    const char *base = getenv("TMPDIR");
    if (base == NULL || base[0] == '\0') {
        base = "/tmp";
    }
    char dir[PATH_MAX];
    const int dn = snprintf(dir, sizeof dir, "%s/inkstorm-pause-XXXXXX", base);
    TEST_ASSERT_TRUE(dn > 0 && (size_t)dn < sizeof dir);
    TEST_ASSERT_NOT_NULL(mkdtemp(dir));
    char path[PATH_MAX + 32];
    const int pn = snprintf(path, sizeof path, "%s/save.txt", dir);
    TEST_ASSERT_TRUE(pn > 0 && (size_t)pn < sizeof path);

    /* Pause -> Lobby / Pause -> Restart helper. */
    TEST_ASSERT_TRUE(save_resolve_and_settle_run(&meta_pause, g_pause, &br_pause));
    /* Window-close / shutdown helper. */
    TEST_ASSERT_TRUE(
        save_finish_run_on_exit(path, true, &meta_exit, &dirty_exit, g_exit, true, &br_exit, NULL));

    TEST_ASSERT_EQUAL_INT(RUN_PHASE_VICTORY, g_pause->phase);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_VICTORY, g_exit->phase);
    TEST_ASSERT_EQUAL_INT32(1, meta_pause.clears);
    TEST_ASSERT_EQUAL_INT32(1, meta_pause.runs_played);
    TEST_ASSERT_EQUAL_INT32(meta_exit.clears, meta_pause.clears);
    TEST_ASSERT_EQUAL_INT32(meta_exit.runs_played, meta_pause.runs_played);
    TEST_ASSERT_EQUAL_INT32(meta_exit.ink_crystals, meta_pause.ink_crystals);
    TEST_ASSERT_EQUAL_INT32(br_exit.total_crystals, br_pause.total_crystals);

    game_destroy(g_exit);
    game_destroy(g_pause);
    (void)remove(path);
    (void)rmdir(dir);
}

static void test_latch_retains_skill_edges_across_hitstop_and_clears_on_phase_change(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    g->hitstop_t = 0.05f;

    InputLatch l;
    input_latch_init(&l, 400.0f, 300.0f);
    RawInput raw = make_raw(460.0f, 300.0f);
    raw.gale_pressed = true;
    raw.cyclone_pressed = true;
    input_latch_feed(&l, &raw);

    /* During hitstop: pointer advances, but Gale/Cyclone stay latched instead of being dropped. */
    const BrushInput frozen = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 460.0f, frozen.mx);
    TEST_ASSERT_FALSE(frozen.gale);
    TEST_ASSERT_FALSE(frozen.cyclone);
    TEST_ASSERT_TRUE(l.gale_pending);
    TEST_ASSERT_TRUE(l.cyclone_pending);

    /* First live combat tick after hitstop consumes the retained skill edges. */
    g->hitstop_t = 0.0f;
    const BrushInput live = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_TRUE(live.gale);
    TEST_ASSERT_TRUE(live.cyclone);
    TEST_ASSERT_FALSE(l.gale_pending);
    TEST_ASSERT_FALSE(l.cyclone_pending);

    /* Phase change helper drops stale pending skills and hitstop motion cleanly. */
    input_latch_feed(&l, &raw);
    TEST_ASSERT_TRUE(l.gale_pending);
    TEST_ASSERT_TRUE(l.cyclone_pending);
    input_latch_clear_skills(&l);
    TEST_ASSERT_FALSE(l.gale_pending);
    TEST_ASSERT_FALSE(l.cyclone_pending);

    /* Sweeping left during hitstop while aiming right must preserve the leftward aim delta for
     * the buffered Gale fired on the first live combat tick. */
    g->player.aim_x = 1.0f;
    g->player.aim_y = 0.0f;
    g->player.ink = g->player.ink_max;
    g->player.gale_charges = g->player.gale_max_charges;
    g->hitstop_t = 0.05f;
    input_latch_init(&l, 500.0f, 300.0f);

    RawInput sweep_left = make_raw(420.0f, 300.0f);
    sweep_left.gale_pressed = true;
    input_latch_feed(&l, &sweep_left);
    RawInput still_left = make_raw(420.0f, 300.0f);

    for (int t = 0; t < 3; ++t) {
        const BrushInput bi = input_latch_take_subtick_for_game(&l, 1, g);
        game_step(g, &bi, NULL);
        input_latch_feed(&l, &still_left);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->hitstop_t);

    const BrushInput post = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_TRUE(post.gale);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 420.0f, post.mx);
    TEST_ASSERT_TRUE(post.mx - post.pmx < -2.0f);
    game_step(g, &post, NULL);
    TEST_ASSERT_TRUE(g->player.aim_x < -0.9f);
    bool saw_left_gale = false;
    for (int32_t e = 0; e < g->event_count; ++e) {
        if (g->events[e].type == EV_GALE && fabsf(g->events[e].x - 420.0f) < 1.0f &&
            g->events[e].x2 < -0.9f) {
            saw_left_gale = true;
        }
    }
    TEST_ASSERT_TRUE(saw_left_gale);

    game_destroy(g);
}

static void test_bgm_rapid_three_track_switch_tapers_both_outgoing_tracks(void)
{
    BgmFade f;
    bgm_fade_reset(&f);
    (void)bgm_fade_request(&f, BGM_BATTLE);
    for (int i = 0; i < 40; ++i) {
        (void)bgm_fade_step(&f, 1.0f / 60.0f);
    }
    TEST_ASSERT_EQUAL_FLOAT(1.0f, f.gain_cur);

    /* Battle -> Boss mid-fade (~0.10s): Battle ~0.83, Boss ~0.17. */
    (void)bgm_fade_request(&f, BGM_BOSS);
    for (int i = 0; i < 6; ++i) {
        (void)bgm_fade_step(&f, 1.0f / 60.0f);
    }
    const float battle_mid = f.gain_prev;
    const float boss_mid = f.gain_cur;
    TEST_ASSERT_TRUE(battle_mid > 0.75f && boss_mid > 0.10f);

    /* Rapid switch to Lobby: neither outgoing track is hard-cut; louder Battle stays in prev,
     * quieter Boss moves to prev2, and both taper monotonically to 0. */
    const BgmFadeActions act = bgm_fade_request(&f, BGM_LOBBY);
    TEST_ASSERT_EQUAL_INT(BGM_LOBBY, act.play);
    TEST_ASSERT_EQUAL_INT(BGM_NONE, act.stop);
    TEST_ASSERT_EQUAL_INT(BGM_LOBBY, f.cur);
    TEST_ASSERT_EQUAL_INT(BGM_BATTLE, f.prev);
    TEST_ASSERT_EQUAL_INT(BGM_BOSS, f.prev2);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, battle_mid, f.gain_prev);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, boss_mid, f.gain_prev2);

    /* Returning to prev2 (Boss) swaps it back into cur without restarting it. */
    BgmFade ret = f;
    const BgmFadeActions ret_act = bgm_fade_request(&ret, BGM_BOSS);
    TEST_ASSERT_EQUAL_INT(BGM_NONE, ret_act.play);
    TEST_ASSERT_EQUAL_INT(BGM_NONE, ret_act.stop);
    TEST_ASSERT_EQUAL_INT(BGM_BOSS, ret.cur);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, boss_mid, ret.gain_cur);
    TEST_ASSERT_EQUAL_INT(BGM_BATTLE, ret.prev);

    /* Stepping completes both outgoing streams cleanly with no hard cuts. */
    bool stopped_battle = false;
    bool stopped_boss = false;
    float last_p1 = f.gain_prev;
    float last_p2 = f.gain_prev2;
    for (int i = 0; i < 60; ++i) {
        const BgmFadeFinished fin = bgm_fade_step_all(&f, 1.0f / 60.0f);
        for (int k = 0; k < fin.count; ++k) {
            if (fin.stop[k] == BGM_BATTLE) {
                stopped_battle = true;
            } else if (fin.stop[k] == BGM_BOSS) {
                stopped_boss = true;
            }
        }
        TEST_ASSERT_TRUE(f.gain_prev <= last_p1 + 1e-6f);
        TEST_ASSERT_TRUE(f.gain_prev2 <= last_p2 + 1e-6f);
        last_p1 = f.gain_prev;
        last_p2 = f.gain_prev2;
    }
    TEST_ASSERT_TRUE(stopped_battle);
    TEST_ASSERT_TRUE(stopped_boss);
    TEST_ASSERT_EQUAL_INT(BGM_NONE, f.prev);
    TEST_ASSERT_EQUAL_INT(BGM_NONE, f.prev2);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, f.gain_cur);
}

static void test_reward_keyboard_pick_with_held_lmb_suppresses_paint_and_selection_tick(void)
{
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    g->phase = RUN_PHASE_REWARD;
    g->run.reward.kind = REWARD_KIND_UPGRADE;
    g->run.reward.choice_count = 3;
    g->run.reward.choice_ids[0] = (int32_t)UPG_STORM_BRUSH;
    g->run.reward.choice_ids[1] = (int32_t)UPG_DOUBLE_GALE;
    g->run.reward.choice_ids[2] = (int32_t)UPG_SHRINE_WARD;
    g->run.reward.choice_next_level[0] = 1;
    g->run.reward.choice_next_level[1] = 1;
    g->run.reward.choice_next_level[2] = 1;

    InputLatch l;
    input_latch_init(&l, 420.0f, 300.0f);
    RawInput pick_while_held = make_raw(450.0f, 320.0f);
    pick_while_held.left_down = true;
    pick_while_held.pick_pressed = 0;
    pick_while_held.ui_click = false;
    input_latch_feed(&l, &pick_while_held);

    const BrushInput sel_in = input_latch_take_subtick_for_game(&l, 1, g);
    const GameCommand cmd = input_latch_take_command(&l, g->phase);
    TEST_ASSERT_FALSE(sel_in.paint);
    TEST_ASSERT_EQUAL_INT(CMD_PICK_REWARD, cmd.type);

    game_step(g, &sel_in, &cmd);
    TEST_ASSERT_EQUAL_INT(RUN_PHASE_PLAYING, g->phase);
    TEST_ASSERT_FALSE(g->player.painting);

    input_latch_reset_for_ui_transition(&l, pick_while_held.mouse_x, pick_while_held.mouse_y,
                                        pick_while_held.left_down);
    RawInput still_held = make_raw(480.0f, 340.0f);
    still_held.left_down = true;
    input_latch_feed(&l, &still_held);
    TEST_ASSERT_FALSE(input_latch_take_subtick_for_game(&l, 1, g).paint);

    RawInput released = make_raw(480.0f, 340.0f);
    input_latch_feed(&l, &released);
    TEST_ASSERT_FALSE(input_latch_take_subtick_for_game(&l, 1, g).paint);

    RawInput fresh_press = make_raw(500.0f, 340.0f);
    fresh_press.left_down = true;
    input_latch_feed(&l, &fresh_press);
    TEST_ASSERT_TRUE(input_latch_take_subtick_for_game(&l, 1, g).paint);

    game_destroy(g);
}

static void test_cli_snap_dir_accepts_exact_path_max_bound(void)
{
    const size_t max_dir = (size_t)PATH_MAX - CLI_SNAP_SUFFIX_LEN - 1u;
    char ok_dir[PATH_MAX];
    ok_dir[0] = '/';
    memset(ok_dir + 1, 'a', max_dir - 1u);
    ok_dir[max_dir] = '\0';
    TEST_ASSERT_EQUAL_size_t(max_dir, cli_trimmed_dir_len(ok_dir));

    CliOptions opt;
    char *argv_ok[] = {"inkstorm", "--snap", ok_dir};
    TEST_ASSERT_TRUE(cli_parse(3, argv_ok, &opt));
    TEST_ASSERT_EQUAL_STRING(ok_dir, opt.snap_dir);

    /* Trailing separator trims back to max_dir and remains valid when raw length < PATH_MAX. */
    char ok_slash[PATH_MAX];
    memcpy(ok_slash, ok_dir, max_dir);
    ok_slash[max_dir] = '/';
    ok_slash[max_dir + 1u] = '\0';
    TEST_ASSERT_EQUAL_size_t(max_dir, cli_trimmed_dir_len(ok_slash));
    char *argv_slash[] = {"inkstorm", "--snap", ok_slash};
    TEST_ASSERT_TRUE(cli_parse(3, argv_slash, &opt));

    /* One byte longer exceeds PATH_MAX once "/snap_00.png" is appended. */
    char too_long[PATH_MAX];
    too_long[0] = '/';
    memset(too_long + 1, 'a', max_dir);
    too_long[max_dir + 1u] = '\0';
    char *argv_bad[] = {"inkstorm", "--snap", too_long};
    TEST_ASSERT_FALSE(cli_parse(3, argv_bad, &opt));
}

static void test_latch_ccw_cyclone_priority_subtick_and_clear(void)
{
    InputLatch l;
    input_latch_init(&l, 300.0f, 300.0f);

    /* 1. CCW press becomes cyclone && cyclone_ccw. */
    RawInput raw = make_raw(340.0f, 320.0f);
    raw.cyclone_ccw_pressed = true;
    input_latch_feed(&l, &raw);
    const BrushInput ccw_in = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(ccw_in.cyclone);
    TEST_ASSERT_TRUE(ccw_in.cyclone_ccw);
    const BrushInput next_in = input_latch_take_tick(&l);
    TEST_ASSERT_FALSE(next_in.cyclone);
    TEST_ASSERT_FALSE(next_in.cyclone_ccw);

    /* 2. Same-frame CW + CCW resolves to CW only and clears CCW pending (not queued). */
    raw = make_raw(360.0f, 320.0f);
    raw.cyclone_pressed = true;
    raw.cyclone_ccw_pressed = true;
    input_latch_feed(&l, &raw);
    const BrushInput both_in = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(both_in.cyclone);
    TEST_ASSERT_FALSE(both_in.cyclone_ccw);
    const BrushInput after_both = input_latch_take_tick(&l);
    TEST_ASSERT_FALSE(after_both.cyclone);
    TEST_ASSERT_FALSE(after_both.cyclone_ccw);

    /* 3. Catch-up sub-tick frame with CCW edge gives the whole motion to the CCW tick. */
    input_latch_init(&l, 300.0f, 300.0f);
    raw = make_raw(600.0f, 420.0f);
    raw.cyclone_ccw_pressed = true;
    input_latch_feed(&l, &raw);
    const BrushInput sub0 = input_latch_take_subtick(&l, 3);
    TEST_ASSERT_TRUE(sub0.cyclone);
    TEST_ASSERT_TRUE(sub0.cyclone_ccw);
    TEST_ASSERT_EQUAL_FLOAT(600.0f, sub0.mx);
    TEST_ASSERT_EQUAL_FLOAT(420.0f, sub0.my);
    TEST_ASSERT_EQUAL_FLOAT(300.0f, sub0.pmx);
    TEST_ASSERT_EQUAL_FLOAT(300.0f, sub0.pmy);
    const BrushInput sub1 = input_latch_take_subtick(&l, 2);
    TEST_ASSERT_FALSE(sub1.cyclone);
    TEST_ASSERT_FALSE(sub1.cyclone_ccw);

    /* 4. Reward phase suppresses and clears CCW; hitstop retains it until hitstop ends. */
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    g->phase = RUN_PHASE_REWARD;
    raw = make_raw(620.0f, 430.0f);
    raw.cyclone_ccw_pressed = true;
    input_latch_feed(&l, &raw);
    const BrushInput rew_in = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_FALSE(rew_in.cyclone);
    TEST_ASSERT_FALSE(rew_in.cyclone_ccw);

    g->phase = RUN_PHASE_PLAYING;
    g->hitstop_t = 0.05f;
    raw = make_raw(650.0f, 440.0f);
    raw.cyclone_ccw_pressed = true;
    input_latch_feed(&l, &raw);
    const BrushInput hs_in = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_FALSE(hs_in.cyclone);
    TEST_ASSERT_FALSE(hs_in.cyclone_ccw);
    g->hitstop_t = 0.0f;
    const BrushInput post_hs = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_TRUE(post_hs.cyclone);
    TEST_ASSERT_TRUE(post_hs.cyclone_ccw);

    /* 5. input_latch_clear_skills clears CCW pending. */
    raw = make_raw(660.0f, 440.0f);
    raw.cyclone_ccw_pressed = true;
    input_latch_feed(&l, &raw);
    input_latch_clear_skills(&l);
    const BrushInput cleared = input_latch_take_tick(&l);
    TEST_ASSERT_FALSE(cleared.cyclone);
    TEST_ASSERT_FALSE(cleared.cyclone_ccw);

    game_destroy(g);
}

static void test_latch_multiframe_cw_and_ccw_preserve_both_spins_and_coords(void)
{
    /* (a) Same-frame simultaneous CW + CCW resolves to CW only at that frame's cursor. */
    InputLatch l;
    input_latch_init(&l, 100.0f, 100.0f);
    RawInput same = make_raw(410.0f, 260.0f);
    same.cyclone_pressed = true;
    same.cyclone_ccw_pressed = true;
    input_latch_feed(&l, &same);
    const BrushInput s0 = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(s0.cyclone);
    TEST_ASSERT_FALSE(s0.cyclone_ccw);
    TEST_ASSERT_TRUE(s0.has_cyclone_pos);
    TEST_ASSERT_EQUAL_FLOAT(410.0f, s0.cyclone_x);
    TEST_ASSERT_EQUAL_FLOAT(260.0f, s0.cyclone_y);
    TEST_ASSERT_EQUAL_FLOAT(410.0f, s0.mx);
    TEST_ASSERT_EQUAL_FLOAT(260.0f, s0.my);
    const BrushInput s1 = input_latch_take_tick(&l);
    TEST_ASSERT_FALSE(s1.cyclone);
    TEST_ASSERT_FALSE(s1.cyclone_ccw);
    TEST_ASSERT_FALSE(s1.has_cyclone_pos);

    /* (b) Distinct frames (no tick popped between them): CW at (x1, y1) then CCW at (x2, y2),
     * keeping live paint and live cursor position intact while preserving both captured coords. */
    input_latch_init(&l, 100.0f, 100.0f);
    RawInput f1 = make_raw(220.0f, 180.0f);
    f1.left_down = true;
    f1.cyclone_pressed = true;
    RawInput f2 = make_raw(540.0f, 390.0f);
    f2.left_down = true;
    f2.cyclone_ccw_pressed = true;
    input_latch_feed(&l, &f1);
    input_latch_feed(&l, &f2);

    const BrushInput t1 = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(t1.paint);
    TEST_ASSERT_EQUAL_FLOAT(540.0f, t1.mx);
    TEST_ASSERT_EQUAL_FLOAT(390.0f, t1.my);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, t1.pmx);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, t1.pmy);
    TEST_ASSERT_TRUE(t1.cyclone);
    TEST_ASSERT_FALSE(t1.cyclone_ccw);
    TEST_ASSERT_TRUE(t1.has_cyclone_pos);
    TEST_ASSERT_EQUAL_FLOAT(220.0f, t1.cyclone_x);
    TEST_ASSERT_EQUAL_FLOAT(180.0f, t1.cyclone_y);

    const BrushInput t2 = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(t2.paint);
    TEST_ASSERT_EQUAL_FLOAT(540.0f, t2.mx);
    TEST_ASSERT_EQUAL_FLOAT(390.0f, t2.my);
    TEST_ASSERT_TRUE(t2.cyclone);
    TEST_ASSERT_TRUE(t2.cyclone_ccw);
    TEST_ASSERT_TRUE(t2.has_cyclone_pos);
    TEST_ASSERT_EQUAL_FLOAT(540.0f, t2.cyclone_x);
    TEST_ASSERT_EQUAL_FLOAT(390.0f, t2.cyclone_y);

    const BrushInput t3 = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(t3.paint);
    TEST_ASSERT_FALSE(t3.cyclone);
    TEST_ASSERT_FALSE(t3.cyclone_ccw);
    TEST_ASSERT_FALSE(t3.has_cyclone_pos);

    /* (c) Vice-versa: CCW on frame 1 at (x1, y1) then CW on frame 2 at (x2, y2). */
    input_latch_init(&l, 100.0f, 100.0f);
    RawInput r1 = make_raw(310.0f, 420.0f);
    r1.left_down = true;
    r1.cyclone_ccw_pressed = true;
    RawInput r2 = make_raw(720.0f, 210.0f);
    r2.left_down = true;
    r2.cyclone_pressed = true;
    input_latch_feed(&l, &r1);
    input_latch_feed(&l, &r2);

    const BrushInput rt1 = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(rt1.paint);
    TEST_ASSERT_EQUAL_FLOAT(720.0f, rt1.mx);
    TEST_ASSERT_EQUAL_FLOAT(210.0f, rt1.my);
    TEST_ASSERT_TRUE(rt1.cyclone);
    TEST_ASSERT_TRUE(rt1.cyclone_ccw);
    TEST_ASSERT_TRUE(rt1.has_cyclone_pos);
    TEST_ASSERT_EQUAL_FLOAT(310.0f, rt1.cyclone_x);
    TEST_ASSERT_EQUAL_FLOAT(420.0f, rt1.cyclone_y);

    const BrushInput rt2 = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(rt2.paint);
    TEST_ASSERT_EQUAL_FLOAT(720.0f, rt2.mx);
    TEST_ASSERT_EQUAL_FLOAT(210.0f, rt2.my);
    TEST_ASSERT_TRUE(rt2.cyclone);
    TEST_ASSERT_FALSE(rt2.cyclone_ccw);
    TEST_ASSERT_TRUE(rt2.has_cyclone_pos);
    TEST_ASSERT_EQUAL_FLOAT(720.0f, rt2.cyclone_x);
    TEST_ASSERT_EQUAL_FLOAT(210.0f, rt2.cyclone_y);

    const BrushInput rt3 = input_latch_take_tick(&l);
    TEST_ASSERT_TRUE(rt3.paint);
    TEST_ASSERT_FALSE(rt3.cyclone);
    TEST_ASSERT_FALSE(rt3.cyclone_ccw);
    TEST_ASSERT_FALSE(rt3.has_cyclone_pos);

    /* (d) Multi-frame hitstop with held LMB: CW on hitstop frame 1 + CCW on hitstop frame 2
     * keep live painting continuous and live (mx, my) intact after hitstop ends, while spawning
     * both vortices at their exact captured coordinates without triggering idle ink regen. */
    GameState *g = game_create(NULL);
    TEST_ASSERT_NOT_NULL(g);
    game_debug_start_wave(g, 0);
    g->internal.spawn_cd = 999.0f;
    g->player.ink = g->player.ink_max;
    g->hitstop_t = 0.035f;

    input_latch_init(&l, 400.0f, 300.0f);
    RawInput hs1 = make_raw(480.0f, 260.0f);
    hs1.left_down = true;
    hs1.cyclone_pressed = true;
    input_latch_feed(&l, &hs1);
    const BrushInput frozen1 = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_TRUE(frozen1.paint);
    TEST_ASSERT_FALSE(frozen1.cyclone);
    TEST_ASSERT_FALSE(frozen1.cyclone_ccw);
    game_step(g, &frozen1, NULL);

    RawInput hs2 = make_raw(680.0f, 340.0f);
    hs2.left_down = true;
    hs2.cyclone_ccw_pressed = true;
    input_latch_feed(&l, &hs2);
    const BrushInput frozen2 = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_TRUE(frozen2.paint);
    TEST_ASSERT_FALSE(frozen2.cyclone);
    TEST_ASSERT_FALSE(frozen2.cyclone_ccw);
    game_step(g, &frozen2, NULL);
    for (int t = 0; t < 4 && g->hitstop_t > 0.0f; ++t) {
        RawInput hold = make_raw(760.0f, 410.0f);
        hold.left_down = true;
        input_latch_feed(&l, &hold);
        const BrushInput f = input_latch_take_subtick_for_game(&l, 1, g);
        TEST_ASSERT_TRUE(f.paint);
        TEST_ASSERT_FALSE(f.cyclone);
        game_step(g, &f, NULL);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, g->hitstop_t);

    RawInput drag1 = make_raw(780.0f, 420.0f);
    drag1.left_down = true;
    input_latch_feed(&l, &drag1);
    const BrushInput live1 = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_TRUE(live1.paint);
    TEST_ASSERT_EQUAL_FLOAT(780.0f, live1.mx);
    TEST_ASSERT_EQUAL_FLOAT(420.0f, live1.my);
    TEST_ASSERT_TRUE(live1.cyclone);
    TEST_ASSERT_FALSE(live1.cyclone_ccw);
    TEST_ASSERT_TRUE(live1.has_cyclone_pos);
    TEST_ASSERT_EQUAL_FLOAT(480.0f, live1.cyclone_x);
    TEST_ASSERT_EQUAL_FLOAT(260.0f, live1.cyclone_y);
    game_step(g, &live1, NULL);
    TEST_ASSERT_TRUE(g->player.painting);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g->player.idle_t);
    TEST_ASSERT_EQUAL_FLOAT(780.0f, g->player.cursor_x);
    TEST_ASSERT_EQUAL_FLOAT(420.0f, g->player.cursor_y);
    bool saw_cw = false;
    for (int32_t i = 0; i < g->event_count; ++i) {
        if (g->events[i].type == EV_CYCLONE && (g->events[i].flags & EVF_CYCLONE_CCW) == 0u &&
            fabsf(g->events[i].x - 480.0f) < 1.0f && fabsf(g->events[i].y - 260.0f) < 1.0f) {
            saw_cw = true;
        }
    }
    TEST_ASSERT_TRUE(saw_cw);

    RawInput drag2 = make_raw(810.0f, 440.0f);
    drag2.left_down = true;
    input_latch_feed(&l, &drag2);
    const BrushInput live2 = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_TRUE(live2.paint);
    TEST_ASSERT_EQUAL_FLOAT(810.0f, live2.mx);
    TEST_ASSERT_EQUAL_FLOAT(440.0f, live2.my);
    TEST_ASSERT_TRUE(live2.cyclone);
    TEST_ASSERT_TRUE(live2.cyclone_ccw);
    TEST_ASSERT_TRUE(live2.has_cyclone_pos);
    TEST_ASSERT_EQUAL_FLOAT(680.0f, live2.cyclone_x);
    TEST_ASSERT_EQUAL_FLOAT(340.0f, live2.cyclone_y);
    game_step(g, &live2, NULL);
    TEST_ASSERT_TRUE(g->player.painting);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g->player.idle_t);
    TEST_ASSERT_EQUAL_FLOAT(810.0f, g->player.cursor_x);
    TEST_ASSERT_EQUAL_FLOAT(440.0f, g->player.cursor_y);
    bool saw_ccw = false;
    for (int32_t i = 0; i < g->event_count; ++i) {
        if (g->events[i].type == EV_CYCLONE && (g->events[i].flags & EVF_CYCLONE_CCW) != 0u &&
            fabsf(g->events[i].x - 680.0f) < 1.0f && fabsf(g->events[i].y - 340.0f) < 1.0f) {
            saw_ccw = true;
        }
    }
    TEST_ASSERT_TRUE(saw_ccw);

    bool active_cw = false;
    bool active_ccw = false;
    for (int32_t i = 0; i < GAME_MAX_ACTIVE_CYCLONES; ++i) {
        const ActiveCyclone *ac = &g->internal.cyclones[i];
        if (!ac->active) {
            continue;
        }
        if (ac->spin > 0.0f && fabsf(ac->x - 480.0f) < 1.0f && fabsf(ac->y - 260.0f) < 1.0f) {
            active_cw = true;
        } else if (ac->spin < 0.0f && fabsf(ac->x - 680.0f) < 1.0f &&
                   fabsf(ac->y - 340.0f) < 1.0f) {
            active_ccw = true;
        }
    }
    TEST_ASSERT_TRUE(active_cw);
    TEST_ASSERT_TRUE(active_ccw);

    const BrushInput live3 = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_TRUE(live3.paint);
    TEST_ASSERT_FALSE(live3.cyclone);
    TEST_ASSERT_FALSE(live3.cyclone_ccw);
    TEST_ASSERT_FALSE(live3.has_cyclone_pos);

    /* (e) Reward phase clears both pending CW and CCW in a single tick. */
    g->phase = RUN_PHASE_REWARD;
    input_latch_feed(&l, &f1);
    input_latch_feed(&l, &f2);
    const BrushInput rew = input_latch_take_subtick_for_game(&l, 1, g);
    TEST_ASSERT_FALSE(rew.cyclone);
    TEST_ASSERT_FALSE(rew.cyclone_ccw);
    TEST_ASSERT_FALSE(rew.has_cyclone_pos);
    TEST_ASSERT_FALSE(l.cyclone_pending);
    TEST_ASSERT_FALSE(l.cyclone_ccw_pending);

    game_destroy(g);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cli_valid_options);
    RUN_TEST(test_cli_usage_errors);
    RUN_TEST(test_cli_debug_faults);
    RUN_TEST(test_frame_clock);
    RUN_TEST(test_latch_move_first_tick_only);
    RUN_TEST(test_latch_zero_tick_frame_preserves_motion);
    RUN_TEST(test_latch_catch_up_ticks_share_motion_without_a_skill_press);
    RUN_TEST(test_latch_skill_press_takes_the_whole_catch_up_motion_once);
    RUN_TEST(test_catch_up_frame_casts_skills_at_the_sampled_cursor_and_aim);
    RUN_TEST(test_small_move_on_a_catch_up_frame_still_turns_the_next_gale);
    RUN_TEST(test_latch_single_subtick_matches_take_tick);
    RUN_TEST(test_latch_edge_press_held_and_coalesced);
    RUN_TEST(test_latch_reward_commands_and_priority);
    RUN_TEST(test_latch_paint_suppression_after_ui_click);
    RUN_TEST(test_latch_confirm_once);
    RUN_TEST(test_latch_ui_transition_suppresses_paint_until_release);
    RUN_TEST(test_latch_endless_crystal_command);
    RUN_TEST(test_hints_wait_while_hidden_and_never_duplicate);
    RUN_TEST(test_bgm_interrupted_fade_out_never_swells_back);
    RUN_TEST(test_bgm_returning_to_fading_track_swaps_without_restart);
    RUN_TEST(test_menu_resting_mouse_yields_to_keyboard_until_moved);
    RUN_TEST(test_menu_new_context_keeps_default_focus_under_resting_mouse);
    RUN_TEST(test_menu_bokuan_grid_navigation);
    RUN_TEST(test_menu_result_cursor_walks_visible_buttons);
    RUN_TEST(test_menu_lobby_stroke_starts_only_on_open_canvas);
    RUN_TEST(test_menu_guide_page_clamps_to_bounds);
    RUN_TEST(test_menu_transition_frame_skips_gameplay);
    RUN_TEST(test_latch_anchor_starts_stroke_at_press_but_keeps_skills);
    RUN_TEST(test_latch_resume_reanchors_and_drops_queued_input);
    RUN_TEST(test_audio_cb_concurrent_render_and_detach);
    RUN_TEST(test_hints_off_clears_queued_and_active_hints);
    RUN_TEST(test_dirty_save_retries_after_transient_write_failure);
    RUN_TEST(test_bgm_pause_duck_ramps_smoothly);
    RUN_TEST(test_esc_resume_with_held_lmb_suppresses_paint);
    RUN_TEST(test_exit_settlement_drains_act3_boss_victory_and_retries_dirty_save);
    RUN_TEST(test_exit_settlement_double_write_failure_recovers_on_dirty_retry);
    RUN_TEST(test_resolve_and_settle_matches_exit_for_act3_boss_hitstop);
    RUN_TEST(test_latch_retains_skill_edges_across_hitstop_and_clears_on_phase_change);
    RUN_TEST(test_bgm_rapid_three_track_switch_tapers_both_outgoing_tracks);
    RUN_TEST(test_reward_keyboard_pick_with_held_lmb_suppresses_paint_and_selection_tick);
    RUN_TEST(test_cli_snap_dir_accepts_exact_path_max_bound);
    RUN_TEST(test_latch_ccw_cyclone_priority_subtick_and_clear);
    RUN_TEST(test_latch_multiframe_cw_and_ccw_preserve_both_spins_and_coords);
    return UNITY_END();
}
