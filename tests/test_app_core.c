#include "bgm_fade.h"
#include "cli.h"
#include "frame_clock.h"
#include "hint_queue.h"
#include "input_latch.h"
#include "menu_nav.h"
#include "unity.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

static void test_latch_catch_up_ticks_share_motion_and_fire_skills_once(void)
{
    static const int32_t k_tick_counts[] = {2, 3};
    for (size_t c = 0; c < sizeof k_tick_counts / sizeof k_tick_counts[0]; ++c) {
        const int32_t ticks = k_tick_counts[c];
        InputLatch l;
        input_latch_init(&l, 0.0f, 0.0f);
        RawInput raw = make_raw(90.0f, -60.0f);
        raw.left_down = true;
        raw.cyclone_pressed = true;
        raw.gale_pressed = true;
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
            TEST_ASSERT_EQUAL(t == 0, in.cyclone);
            TEST_ASSERT_EQUAL(t == 0, in.gale);
        }

        const BrushInput rest = input_latch_take_tick(&l);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 90.0f, rest.pmx);
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 90.0f, rest.mx);
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

    TEST_ASSERT_EQUAL_INT32(HINT_STROKE,
                            hint_queue_step(&q, &active, &active_t, 1.0f / 60.0f, true));
    TEST_ASSERT_EQUAL_FLOAT(HINT_SHOW_SECONDS, active_t);
    for (int i = 0; i < 300; ++i) {
        (void)hint_queue_step(&q, &active, &active_t, 1.0f / 60.0f, false);
    }
    TEST_ASSERT_EQUAL_FLOAT(HINT_SHOW_SECONDS, active_t);

    int32_t started = -1;
    for (int i = 0; i < 130 && started < 0; ++i) {
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

static void test_menu_transition_frame_skips_gameplay(void)
{
    TEST_ASSERT_FALSE(menu_action_ends_frame(MENU_ACT_NONE));
    TEST_ASSERT_TRUE(menu_action_ends_frame(MENU_ACT_PAUSE_RESUME));
    TEST_ASSERT_TRUE(menu_action_ends_frame(MENU_ACT_BRUSH_PICK));
    TEST_ASSERT_TRUE(menu_action_ends_frame(MENU_ACT_RESULT_RETRY));
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cli_valid_options);
    RUN_TEST(test_cli_usage_errors);
    RUN_TEST(test_cli_debug_faults);
    RUN_TEST(test_frame_clock);
    RUN_TEST(test_latch_move_first_tick_only);
    RUN_TEST(test_latch_zero_tick_frame_preserves_motion);
    RUN_TEST(test_latch_catch_up_ticks_share_motion_and_fire_skills_once);
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
    RUN_TEST(test_menu_transition_frame_skips_gameplay);
    RUN_TEST(test_latch_anchor_starts_stroke_at_press_but_keeps_skills);
    RUN_TEST(test_latch_resume_reanchors_and_drops_queued_input);
    return UNITY_END();
}
