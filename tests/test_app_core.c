#include "cli.h"
#include "frame_clock.h"
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
}

static void test_cli_usage_errors(void)
{
    char long_path[PATH_MAX + 1];
    memset(long_path, 'a', PATH_MAX);
    long_path[PATH_MAX] = '\0';

    typedef struct {
        int argc;
        char *argv[6];
    } BadCase;

    const BadCase cases[] = {
        {2, {"inkstorm", "--bogus"}},
        {2, {"inkstorm", "--demo"}},
        {3, {"inkstorm", "--snap", "d"}},
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
        {4, {"inkstorm", "--no-save", "--save-path", "x"}},
        {4, {"inkstorm", "--headless-smoke", "--smoke-frames", "1"}},
        {3, {"inkstorm", "--headless-smoke", "--showcase"}},
        {5, {"inkstorm", "--save-path", "x", "--smoke-frames", "1"}},
        {4, {"inkstorm", "--save-path", "x", "--showcase"}},
        {4, {"inkstorm", "--save-path", "x", "--headless-smoke"}},
        {3, {"inkstorm", "--no-post", "--no-post"}},
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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cli_valid_options);
    RUN_TEST(test_cli_usage_errors);
    RUN_TEST(test_cli_debug_faults);
    RUN_TEST(test_frame_clock);
    return UNITY_END();
}
