#include "game.h"
#include "render_internal.h"
#include "unity.h"

#include <stdlib.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void feed(RenderState *r, GameState *g, GameEventType type, uint32_t flags)
{
    g->event_count = 1;
    memset(&g->events[0], 0, sizeof g->events[0]);
    g->events[0] = (GameEvent){.type = type, .x = 640.0f, .y = 360.0f, .flags = flags};
    render_consume_events(r, g);
}

static int count_strokes(const RenderState *r, int32_t kind)
{
    int n = 0;
    for (int i = 0; i < RENDER_MAX_STROKES; ++i) {
        n += (r->strokes[i].active && r->strokes[i].kind == kind) ? 1 : 0;
    }
    return n;
}

static int count_seals(const RenderState *r)
{
    int n = 0;
    for (int i = 0; i < RENDER_MAX_SEALS; ++i) {
        n += r->seals[i].active ? 1 : 0;
    }
    return n;
}

typedef struct {
    RenderState *r;
    GameState *g;
} Fixture;

static Fixture fixture(void)
{
    Fixture f = {.r = calloc(1, sizeof(RenderState)), .g = game_create(NULL)};
    TEST_ASSERT_NOT_NULL(f.r);
    TEST_ASSERT_NOT_NULL(f.g);
    render_reset_vfx(f.r, UINT64_C(1));
    return f;
}

static void release(Fixture f)
{
    game_destroy(f.g);
    free(f.r);
}

static void test_milestones_play_even_when_major_effects_are_saturated(void)
{
    Fixture f = fixture();
    f.r->major_fx_active = 2;
    feed(f.r, f.g, EV_ACT_CLEAR, 0);
    TEST_ASSERT_EQUAL_INT(1, count_strokes(f.r, 2));
    TEST_ASSERT_EQUAL_INT(1, count_strokes(f.r, 10));
    TEST_ASSERT_TRUE(f.r->clear_banner_t > 0.0f);
    TEST_ASSERT_EQUAL_INT(TEXT_CALLOUT_ACT_CLEAR, f.r->clear_banner_text);

    f.r->major_fx_active = 2;
    feed(f.r, f.g, EV_BOSS_ENTER, 0);
    TEST_ASSERT_TRUE(f.r->boss_banner_t > 1.9f);

    f.r->major_fx_active = 2;
    feed(f.r, f.g, EV_WAVE_CLEAR, 0);
    TEST_ASSERT_EQUAL_INT(TEXT_CALLOUT_WAVE_CLEAR, f.r->clear_banner_text);
    TEST_ASSERT_EQUAL_INT(2, count_strokes(f.r, 10));
    release(f);
}

static void test_cyclone_mark_lives_in_its_stroke_not_a_seal_slot(void)
{
    Fixture f = fixture();
    f.g->config.brush_style = BRUSH_STYLE_QUILL;
    for (int i = 0; i < 4; ++i) {
        feed(f.r, f.g, EV_CYCLONE, 0);
    }
    TEST_ASSERT_EQUAL_INT(0, count_seals(f.r));
    TEST_ASSERT_EQUAL_INT(4, count_strokes(f.r, 0));
    for (int i = 0; i < RENDER_MAX_STROKES; ++i) {
        if (f.r->strokes[i].active) {
            TEST_ASSERT_EQUAL_INT32(BRUSH_STYLE_QUILL, f.r->strokes[i].style);
        }
    }
    release(f);
}

static void test_seal_pick_prefers_milestone_then_newest(void)
{
    Fixture f = fixture();
    for (int i = 0; i < 5; ++i) {
        feed(f.r, f.g, EV_REWARD_PICK, 0);
    }
    feed(f.r, f.g, EV_CATACLYSM, 0);
    feed(f.r, f.g, EV_WAVE_CLEAR, 0);
    for (int i = 0; i < 3; ++i) {
        feed(f.r, f.g, EV_REWARD_PICK, 0);
    }
    int32_t picked[2];
    TEST_ASSERT_EQUAL_INT32(2, render_vfx_pick_seals(f.r, picked));
    TEST_ASSERT_EQUAL_INT(TEXT_SEAL_PURIFY, f.r->seals[picked[0]].glyph);
    TEST_ASSERT_EQUAL_INT(TEXT_SEAL_SANGEN, f.r->seals[picked[1]].glyph);

    f.r->seals[picked[0]].active = false;
    f.r->seals[picked[1]].active = false;
    for (int i = 0; i < RENDER_MAX_SEALS; ++i) {
        f.r->seals[i].age = (float)(RENDER_MAX_SEALS - i);
    }
    TEST_ASSERT_EQUAL_INT32(2, render_vfx_pick_seals(f.r, picked));
    TEST_ASSERT_TRUE(f.r->seals[picked[0]].age <= f.r->seals[picked[1]].age);
    release(f);
}

static void test_blocked_shrine_hit_shows_ward_without_crack_or_flash(void)
{
    Fixture f = fixture();
    feed(f.r, f.g, EV_SHRINE_HIT, EVF_SHRINE_BLOCKED);
    TEST_ASSERT_EQUAL_INT(1, count_strokes(f.r, 12));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f.r->flash_t);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f.r->shrine_flicker_t);
    feed(f.r, f.g, EV_SHRINE_HIT, 0);
    TEST_ASSERT_TRUE(f.r->shrine_flicker_t > 0.0f);
    release(f);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_milestones_play_even_when_major_effects_are_saturated);
    RUN_TEST(test_cyclone_mark_lives_in_its_stroke_not_a_seal_slot);
    RUN_TEST(test_seal_pick_prefers_milestone_then_newest);
    RUN_TEST(test_blocked_shrine_hit_shows_ward_without_crack_or_flash);
    return UNITY_END();
}
