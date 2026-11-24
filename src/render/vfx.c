#include "ink_prims.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const float k_vfx_two_pi = 6.28318530718f;

static void add_shake(RenderState *r, float amp, float dur)
{
    if (amp > r->shake_amp) {
        r->shake_amp = amp;
    }
    if (dur > r->shake_t) {
        r->shake_t = dur;
    }
}

static void trigger_flash(RenderState *r, float dur)
{
    /* Rate limit full-screen flashes to <= 3 per second (art-direction §9.7) */
    int recent = 0;
    for (int i = 0; i < r->flash_count; ++i) {
        if (r->vfx_time - r->flash_times[i] < 1.0f) {
            r->flash_times[recent++] = r->flash_times[i];
        }
    }
    r->flash_count = recent;
    if (r->flash_count >= 3) {
        return;
    }
    r->flash_times[r->flash_count++] = r->vfx_time;
    if (dur > r->flash_t) {
        r->flash_t = dur;
    }
}

static void push_spark(RenderState *r, VfxSpark s)
{
    r->sparks[r->spark_cursor] = s;
    r->spark_cursor = (r->spark_cursor + 1) % RENDER_MAX_SPARKS;
}

static void spawn_burst(RenderState *r, float x, float y, float dx, float dy, int count,
                        float speed, float size, float life, PaletteId pal, bool add_wash)
{
    const float base_ang = (dx != 0.0f || dy != 0.0f) ? atan2f(dy, dx) : 0.0f;
    const float spread = (dx != 0.0f || dy != 0.0f) ? 0.65f : k_vfx_two_pi;
    if (add_wash) {
        push_spark(r, (VfxSpark){
                          .active = true,
                          .x = x,
                          .y = y,
                          .vx = dx * 18.0f,
                          .vy = dy * 18.0f,
                          .size = size * 2.2f,
                          .age = 0.0f,
                          .life = life,
                          .color_id = (int32_t)pal,
                          .seed = (uint32_t)(ink_rng_next(&r->vfx_rng) & 0xffffffffu),
                          .is_wash = true,
                      });
    }
    for (int i = 0; i < count; ++i) {
        const float ang = base_ang + (ink_rng_f01(&r->vfx_rng) - 0.5f) * spread * 2.0f;
        const float sp = speed * (0.45f + 0.85f * ink_rng_f01(&r->vfx_rng));
        push_spark(r, (VfxSpark){
                          .active = true,
                          .x = x,
                          .y = y,
                          .vx = cosf(ang) * sp,
                          .vy = sinf(ang) * sp,
                          .size = size * (0.6f + 0.7f * ink_rng_f01(&r->vfx_rng)),
                          .age = 0.0f,
                          .life = life * (0.65f + 0.45f * ink_rng_f01(&r->vfx_rng)),
                          .color_id = (int32_t)pal,
                          .seed = (uint32_t)(ink_rng_next(&r->vfx_rng) & 0xffffffffu),
                          .is_wash = false,
                      });
    }
}

static void push_bolt(RenderState *r, float x1, float y1, float x2, float y2, float life)
{
    r->bolts[r->bolt_cursor] = (VfxBolt){
        .active = true,
        .x1 = x1,
        .y1 = y1,
        .x2 = x2,
        .y2 = y2,
        .age = 0.0f,
        .life = life,
        .seed = (uint32_t)(ink_rng_next(&r->vfx_rng) & 0xffffffffu),
    };
    r->bolt_cursor = (r->bolt_cursor + 1) % RENDER_MAX_BOLTS;
}

static void push_seal(RenderState *r, TextId glyph, float x, float y, float size, SealStyle style,
                      SealPriority priority, float life)
{
    r->seals[r->seal_cursor] = (VfxSeal){
        .active = true,
        .glyph = glyph,
        .x = x,
        .y = y,
        .size = size,
        .style = (int32_t)style,
        .priority = (int32_t)priority,
        .age = 0.0f,
        .life = life,
    };
    r->seal_cursor = (r->seal_cursor + 1) % RENDER_MAX_SEALS;
}

static void push_callout(RenderState *r, TextId text_id, int32_t count_arg, float x, float y,
                         PaletteId color, bool large, float life)
{
    r->callouts[r->callout_cursor] = (VfxCallout){
        .active = true,
        .text_id = text_id,
        .count_arg = count_arg,
        .x = x,
        .y = y,
        .age = 0.0f,
        .life = life,
        .color_id = (int32_t)color,
        .large = large,
    };
    r->callout_cursor = (r->callout_cursor + 1) % RENDER_MAX_CALLOUTS;
}

static void push_styled_stroke(RenderState *r, int32_t kind, int32_t style, float x, float y,
                               float dx, float dy, float radius, float life)
{
    r->strokes[r->stroke_cursor] = (VfxStroke){
        .active = true,
        .kind = kind,
        .style = style,
        .x = x,
        .y = y,
        .dx = dx,
        .dy = dy,
        .radius = radius,
        .age = 0.0f,
        .life = life,
        .seed = (uint32_t)(ink_rng_next(&r->vfx_rng) & 0xffffffffu),
    };
    r->stroke_cursor = (r->stroke_cursor + 1) % RENDER_MAX_STROKES;
}

static void push_stroke(RenderState *r, int32_t kind, float x, float y, float dx, float dy,
                        float radius, float life)
{
    push_styled_stroke(r, kind, -1, x, y, dx, dy, radius, life);
}

static void start_clear_banner(RenderState *r, TextId text)
{
    r->clear_banner_t = 1.0f;
    r->clear_banner_text = text;
}

static void enqueue_intro(RenderState *r, EnemyDefId def_id)
{
    if ((unsigned)def_id >= (unsigned)ENEMY_DEF_COUNT) {
        return;
    }
    if (r->intro_count >= RENDER_MAX_INTRO_QUEUE) {
        r->intro_head = (r->intro_head + 1) % RENDER_MAX_INTRO_QUEUE;
        r->intro_count--;
    }
    const int tail = (r->intro_head + r->intro_count) % RENDER_MAX_INTRO_QUEUE;
    r->intro_queue[tail] = def_id;
    r->intro_count++;
}

void render_reset_vfx(RenderState *r, uint64_t seed)
{
    if (r == NULL) {
        return;
    }
    r->vfx_rng = seed ^ UINT64_C(0x9e3779b97f4a7c15);
    if (r->vfx_rng == 0) {
        r->vfx_rng = 1;
    }
    r->vfx_time = 0.0f;
    r->shake_t = 0.0f;
    r->shake_amp = 0.0f;
    r->flash_t = 0.0f;
    r->flash_count = 0;
    r->boss_banner_t = 0.0f;
    r->clear_banner_t = 0.0f;
    r->clear_banner_text = TEXT_NONE;
    r->gameover_drip_t = 0.0f;
    r->gameover_drip_active = false;
    r->victory_scroll_t = 0.0f;
    r->reward_anim_t = 0.0f;
    r->reward_reroll_t = 0.0f;
    r->last_reward_serial = 0;
    r->skill_deny_t[0] = 0.0f;
    r->skill_deny_t[1] = 0.0f;
    r->shrine_flicker_t = 0.0f;
    r->major_fx_active = 0;
    r->wind_telegraph_t = 0.0f;
    r->wind_telegraph_dir = 0;
    r->ryu_trail_n = 0;
    r->ryu_trail_id = (EntityId){ECS_INVALID_INDEX, 0};
    r->ryu_awaken_t = 0.0f;
    r->prev_nue_id = (EntityId){ECS_INVALID_INDEX, 0};
    memset(r->prev_proj_bounces, 0, sizeof r->prev_proj_bounces);
    memset(r->prev_proj_gen, 0, sizeof r->prev_proj_gen);
    r->intro_head = 0;
    r->intro_count = 0;
    r->intro_t = 0.0f;

    memset(r->sparks, 0, sizeof r->sparks);
    memset(r->bolts, 0, sizeof r->bolts);
    memset(r->seals, 0, sizeof r->seals);
    memset(r->callouts, 0, sizeof r->callouts);
    memset(r->strokes, 0, sizeof r->strokes);
    memset(r->cracks, 0, sizeof r->cracks);
    r->spark_cursor = 0;
    r->bolt_cursor = 0;
    r->seal_cursor = 0;
    r->callout_cursor = 0;
    r->stroke_cursor = 0;
    r->crack_cursor = 0;
}

void render_consume_events(RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL) {
        return;
    }
    const int32_t count = g->event_count > GAME_MAX_EVENTS ? GAME_MAX_EVENTS : g->event_count;
    for (int32_t i = 0; i < count; ++i) {
        const GameEvent *ev = &g->events[i];
        r->events_seen++;

        switch (ev->type) {
        case EV_KILL:
            spawn_burst(r, ev->x, ev->y, ev->x2, ev->y2, 8, 175.0f, 9.0f, 0.60f, PAL_SUMI_DARK,
                        true);
            if ((ev->flags & DMG_SLAM) != 0) {
                push_callout(r, TEXT_CALLOUT_SLAM, 0, ev->x, ev->y - 24.0f, PAL_SHU, false, 0.90f);
            } else if (g->run.chain >= 2) {
                push_callout(r, TEXT_CALLOUT_CHAIN, g->run.chain, ev->x, ev->y - 22.0f, PAL_KIN,
                             false, 0.85f);
            }
            break;
        case EV_DEFLECT:
            spawn_burst(r, ev->x, ev->y, ev->x2, ev->y2, 5, 145.0f, 5.5f, 0.22f, PAL_KIN, false);
            break;
        case EV_SLAM:
            spawn_burst(r, ev->x, ev->y, ev->x2, ev->y2, 12, 240.0f, 11.0f, 0.34f, PAL_SUMI_SCORCH,
                        true);
            push_stroke(r, 1, ev->x, ev->y, ev->x2, ev->y2, 72.0f, 0.30f);
            add_shake(r, 4.5f, 0.16f);
            break;
        case EV_IGNITE:
            spawn_burst(r, ev->x, ev->y, 0.0f, -1.0f, 10, 110.0f, 8.0f, 0.65f, PAL_SHU_HOT, false);
            push_callout(r, TEXT_CALLOUT_BLAZING, 0, ev->x, ev->y - 18.0f, PAL_SHU, true, 1.25f);
            break;
        case EV_STORM_ZAP:
            push_bolt(r, ev->x, ev->y, ev->x2, ev->y2, 0.22f);
            break;
        case EV_THUNDERBURST:
            if (r->major_fx_active < 2) {
                push_stroke(r, 5, ev->x, ev->y, 0.0f, 0.0f, ev->x2 > 10.0f ? ev->x2 : 72.0f, 0.40f);
                r->major_fx_active++;
            }
            spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 9, 250.0f, 10.0f, 0.42f, PAL_SHU, true);
            spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 9, 260.0f, 9.0f, 0.40f, PAL_AO_HOT, false);
            push_callout(r, TEXT_CALLOUT_THUNDERBURST, 0, ev->x, ev->y - 26.0f, PAL_SHU, true,
                         1.15f);
            add_shake(r, 5.5f, 0.22f);
            break;
        case EV_CATACLYSM:
            if (r->major_fx_active < 2) {
                push_stroke(r, 6, ev->x, ev->y, 0.0f, 0.0f, ev->x2 > 10.0f ? ev->x2 : 96.0f, 0.60f);
                r->major_fx_active++;
            }
            push_seal(r, TEXT_SEAL_SANGEN, ev->x, ev->y, 70.0f, SEAL_HAKUBUN, SEAL_PRIO_REACTION,
                      0.65f);
            spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 14, 290.0f, 10.0f, 0.60f, PAL_KIN_HOT, true);
            spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 8, 220.0f, 8.0f, 0.50f, PAL_SHU, false);
            spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 8, 240.0f, 8.0f, 0.50f, PAL_AO_HOT, false);
            push_callout(r, TEXT_CALLOUT_CATACLYSM, 0, ev->x, ev->y - 38.0f, PAL_KIN, true, 1.35f);
            add_shake(r, 9.0f, 0.34f);
            trigger_flash(r, 0.05f);
            break;
        case EV_WIND_TELEGRAPH:
            r->wind_telegraph_t = 1.5f;
            r->wind_telegraph_dir = ev->value;
            break;
        case EV_CYCLONE:
            push_styled_stroke(r, 0, (int32_t)g->config.brush_style, ev->x, ev->y, 0.0f, 0.0f,
                               ev->value > 0 ? (float)ev->value : 110.0f, 0.72f);
            break;
        case EV_GALE:
            push_styled_stroke(r, 1, (int32_t)g->config.brush_style, ev->x, ev->y, ev->x2, ev->y2,
                               240.0f, 0.36f);
            break;
        case EV_ORB_PICKUP:
            push_stroke(r, 3, ev->x, ev->y, g->player.cursor_x, g->player.cursor_y,
                        (ev->flags & EVF_ORB_AUTO) != 0 ? 1.0f : 0.6f, 0.20f);
            spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 3, 60.0f, 4.0f, 0.20f, PAL_KIN_HOT, false);
            break;
        case EV_SHRINE_HIT:
            if ((ev->flags & EVF_SHRINE_BLOCKED) != 0) {
                push_stroke(r, 12, ev->x, ev->y, ev->x2, ev->y2, 46.0f, 0.45f);
                spawn_burst(r, ev->x + ev->x2 * 34.0f, ev->y + ev->y2 * 34.0f, ev->x2, ev->y2, 6,
                            120.0f, 5.0f, 0.30f, PAL_KIN_HOT, false);
                break;
            }
            r->shrine_flicker_t = 0.45f;
            r->cracks[r->crack_cursor] = (VfxCrack){
                .active = true,
                .x = ev->x,
                .y = ev->y,
                .dx = ev->x2,
                .dy = ev->y2,
                .age = 0.0f,
                .life = 0.45f,
                .seed = (uint32_t)(ink_rng_next(&r->vfx_rng) & 0xffffffffu),
            };
            r->crack_cursor = (r->crack_cursor + 1) % RENDER_MAX_CRACKS;
            add_shake(r, 6.0f, 0.22f);
            trigger_flash(r, 0.05f);
            break;
        case EV_SURGE_START:
            push_callout(r, TEXT_CALLOUT_SURGE, 0, ev->x, ev->y - 34.0f, PAL_KIN, false, 0.95f);
            break;
        case EV_BOSS_ENTER:
            r->boss_banner_t = 2.0f;
            r->boss_banner_id = (EnemyDefId)ev->value;
            add_shake(r, 5.0f, 0.24f);
            break;
        case EV_BOSS_PHASE: {
            const EnemyComp *ben = game_enemy(g, g->run.boss_id);
            if (ben != NULL && ben->def_id == ENEMY_BOSS_RYU) {
                r->ryu_awaken_t = 1.5f;
            }
            if (r->major_fx_active < 2) {
                push_stroke(r, 4, ev->x, ev->y, 0.0f, 0.0f, 130.0f, 1.50f);
                spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 18, 260.0f, 12.0f, 0.85f, PAL_SUMI_DARK,
                            true);
                r->major_fx_active++;
            }
            add_shake(r, 7.0f, 0.30f);
            break;
        }
        case EV_BOSS_PART_BREAK: {
            const PaletteId pal =
                (ev->value == 0) ? PAL_SHU : ((ev->value == 1) ? PAL_AO : PAL_GOFUN);
            const PaletteId hot =
                (ev->value == 0) ? PAL_SHU_HOT : ((ev->value == 1) ? PAL_AO_HOT : PAL_SUMI_LIGHT);
            spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 14, 240.0f, 10.5f, 0.55f, pal, true);
            spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 8, 190.0f, 7.5f, 0.42f, hot, false);
            push_stroke(r, 9, ev->x, ev->y, (float)ev->value, 0.0f, 52.0f, 0.48f);
            add_shake(r, 5.0f, 0.20f);
            break;
        }
        /* Milestones bypass the major-effect budget: a busy screen must never swallow the
         * moment that tells the player the wave or act is won. */
        case EV_WAVE_CLEAR:
            push_stroke(r, 10, ev->x, ev->y, 0.0f, 0.0f, 900.0f, 0.90f);
            push_seal(r, TEXT_SEAL_PURIFY, (float)INK_SCREEN_W * 0.5f + 210.0f, 300.0f, 62.0f,
                      SEAL_HAKUBUN, SEAL_PRIO_MILESTONE, 1.0f);
            spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 12, 260.0f, 8.0f, 0.55f, PAL_GOFUN, true);
            start_clear_banner(r, TEXT_CALLOUT_WAVE_CLEAR);
            break;
        case EV_ACT_CLEAR:
            push_stroke(r, 2, 0.0f, (float)INK_SCREEN_H * 0.5f, 1.0f, 0.0f, (float)INK_SCREEN_W,
                        1.50f);
            push_stroke(r, 10, ev->x, ev->y, 0.0f, 0.0f, 1100.0f, 1.0f);
            push_seal(r, TEXT_SEAL_SLAY, (float)INK_SCREEN_W * 0.5f + 230.0f, 300.0f, 86.0f,
                      SEAL_HAKUBUN, SEAL_PRIO_MILESTONE, 1.0f);
            start_clear_banner(r, TEXT_CALLOUT_ACT_CLEAR);
            break;
        case EV_PURIFY: {
            float dx = ev->x - ev->x2;
            float dy = ev->y - ev->y2;
            const float d = sqrtf(dx * dx + dy * dy);
            dx = d > 1e-3f ? dx / d : 0.0f;
            dy = d > 1e-3f ? dy / d : -1.0f;
            spawn_burst(r, ev->x, ev->y, dx, dy, 6, 150.0f, 7.5f, 0.70f, PAL_GOFUN, true);
            spawn_burst(r, ev->x, ev->y, 0.0f, -1.0f, 4, 110.0f, 4.0f, 0.55f, PAL_KIN_HOT, false);
            break;
        }
        case EV_REWARD_PICK:
            push_seal(r, TEXT_SEAL_DONE, (float)INK_SCREEN_W * 0.5f, (float)INK_SCREEN_H * 0.52f,
                      62.0f, SEAL_HAKUBUN, SEAL_PRIO_STAMP, 0.60f);
            break;
        case EV_REWARD_REROLL:
            r->reward_reroll_t = 0.40f;
            break;
        case EV_ENEMY_INTRO:
            enqueue_intro(r, (EnemyDefId)ev->value);
            break;
        case EV_SKILL_DENIED:
            if (ev->value >= 0 && ev->value < SKILL_COUNT) {
                r->skill_deny_t[ev->value] = 0.30f;
            }
            break;
        case EV_GAME_OVER:
            r->gameover_drip_active = true;
            r->gameover_drip_t = 0.0f;
            break;
        case EV_VICTORY:
            r->victory_scroll_t = 0.0f;
            break;
        default:
            break;
        }
    }
}

static void spawn_brush_flavor(RenderState *r, const GameState *g)
{
    if (!g->player.painting || g->player.dry || g->player.stroke_speed <= 80.0f) {
        return;
    }
    const float cx = g->player.cursor_x;
    const float cy = g->player.cursor_y;
    const float ax = g->player.aim_x;
    const float ay = g->player.aim_y;
    const uint64_t t = g->ticks;
    switch (g->config.brush_style) {
    case BRUSH_STYLE_EMBER:
        if (t % 3u == 0u) {
            spawn_burst(r, cx, cy, 0.0f, -1.0f, 2, 70.0f, 4.8f, 0.34f, PAL_SHU_HOT, false);
            spawn_burst(r, cx - ax * 10.0f, cy - ay * 10.0f, -ax, -ay, 1, 40.0f, 3.2f, 0.42f,
                        PAL_KIN_HOT, false);
        }
        break;
    case BRUSH_STYLE_QUILL:
        if (t % 4u == 0u) {
            spawn_burst(r, cx, cy, 0.0f, 0.0f, 1, 80.0f, 3.0f, 0.20f, PAL_AO_HOT, false);
        }
        if (t % 10u == 0u) {
            const float a = ink_rng_f01(&r->vfx_rng) * k_vfx_two_pi;
            push_bolt(r, cx, cy, cx + cosf(a) * 22.0f, cy + sinf(a) * 22.0f, 0.12f);
        }
        break;
    case BRUSH_STYLE_HEAVY:
    default:
        if (t % 3u == 0u) {
            spawn_burst(r, cx, cy, -ax, -ay, 2, 60.0f, 6.5f, 0.40f, PAL_SUMI_DARK, false);
        }
        if (t % 12u == 0u) {
            push_styled_stroke(r, 11, BRUSH_STYLE_HEAVY, cx, cy, ax, ay, 30.0f, 0.35f);
        }
        break;
    }
}

static void track_ryu_and_bounces(RenderState *r, const GameState *g)
{
    /* Track Ippitsu-Ryu head trail ring buffer (24 points) & Nue dash trail */
    const EnemyComp *ben = game_enemy(g, g->run.boss_id);
    const TransformComp *btr = game_transform(g, g->run.boss_id);
    if (ben != NULL && btr != NULL && ben->def_id == ENEMY_BOSS_RYU) {
        if (r->ryu_trail_id.index != g->run.boss_id.index ||
            r->ryu_trail_id.gen != g->run.boss_id.gen) {
            r->ryu_trail_id = g->run.boss_id;
            r->ryu_trail_n = 1;
            r->ryu_trail[0] = (Vector2){btr->x, btr->y};
        } else {
            const float dx = btr->x - r->ryu_trail[0].x;
            const float dy = btr->y - r->ryu_trail[0].y;
            if (dx * dx + dy * dy >= 16.0f || r->ryu_trail_n < 4) {
                const int n = r->ryu_trail_n < RENDER_RYU_TRAIL_LEN ? r->ryu_trail_n + 1
                                                                    : RENDER_RYU_TRAIL_LEN;
                for (int i = n - 1; i > 0; --i) {
                    r->ryu_trail[i] = r->ryu_trail[i - 1];
                }
                r->ryu_trail[0] = (Vector2){btr->x, btr->y};
                r->ryu_trail_n = n;
            }
        }
    } else {
        r->ryu_trail_n = 0;
        r->ryu_trail_id = (EntityId){ECS_INVALID_INDEX, 0};
    }

    if (ben != NULL && btr != NULL && ben->def_id == ENEMY_BOSS_NUE) {
        if (r->prev_nue_id.index == g->run.boss_id.index &&
            r->prev_nue_id.gen == g->run.boss_id.gen) {
            const float dx = btr->x - r->prev_nue_pos.x;
            const float dy = btr->y - r->prev_nue_pos.y;
            if (dx * dx + dy * dy > 400.0f) {
                push_stroke(r, 8, r->prev_nue_pos.x, r->prev_nue_pos.y, btr->x, btr->y, 12.0f,
                            0.50f);
            }
        }
        r->prev_nue_id = g->run.boss_id;
        r->prev_nue_pos = (Vector2){btr->x, btr->y};
    } else {
        r->prev_nue_id = (EntityId){ECS_INVALID_INDEX, 0};
    }

    /* Detect bounce-orb ricochet and emit mini broken enso */
    const uint32_t hw = g->ecs.high_water;
    for (uint32_t i = 0; i < hw; ++i) {
        if (!g->ecs.alive[i]) {
            r->prev_proj_gen[i] = 0;
            continue;
        }
        const EntityId id = {.index = i, .gen = g->ecs.gen[i]};
        const ProjComp *pr = game_proj(g, id);
        const TransformComp *tr = game_transform(g, id);
        if (pr == NULL || tr == NULL || pr->kind != PROJ_BOUNCE_ORB) {
            r->prev_proj_gen[i] = 0;
            continue;
        }
        if (r->prev_proj_gen[i] == id.gen && pr->bounces_left < r->prev_proj_bounces[i]) {
            push_stroke(r, 7, tr->x, tr->y, 0.0f, 0.0f, 22.0f, 0.30f);
        }
        r->prev_proj_gen[i] = id.gen;
        r->prev_proj_bounces[i] = pr->bounces_left;
    }

    spawn_brush_flavor(r, g);
}

void render_update_vfx(RenderState *r, const GameState *g, float dt)
{
    if (r == NULL || g == NULL || dt <= 0.0f) {
        return;
    }
    r->vfx_time += dt;

    if (r->shake_t > 0.0f) {
        r->shake_t = fmaxf(0.0f, r->shake_t - dt);
        if (r->shake_t <= 0.0f) {
            r->shake_amp = 0.0f;
        }
    }
    if (r->flash_t > 0.0f) {
        r->flash_t = fmaxf(0.0f, r->flash_t - dt);
    }
    if (r->shrine_flicker_t > 0.0f) {
        r->shrine_flicker_t = fmaxf(0.0f, r->shrine_flicker_t - dt);
    }
    if (r->wind_telegraph_t > 0.0f) {
        r->wind_telegraph_t = fmaxf(0.0f, r->wind_telegraph_t - dt);
    }
    if (r->ryu_awaken_t > 0.0f) {
        r->ryu_awaken_t = fmaxf(0.0f, r->ryu_awaken_t - dt);
    }
    for (int s = 0; s < SKILL_COUNT; ++s) {
        if (r->skill_deny_t[s] > 0.0f) {
            r->skill_deny_t[s] = fmaxf(0.0f, r->skill_deny_t[s] - dt);
        }
    }
    if (r->boss_banner_t > 0.0f) {
        r->boss_banner_t = fmaxf(0.0f, r->boss_banner_t - dt);
    }
    if (r->clear_banner_t > 0.0f) {
        r->clear_banner_t = fmaxf(0.0f, r->clear_banner_t - dt);
    }
    if (r->gameover_drip_active) {
        r->gameover_drip_t = fminf(1.5f, r->gameover_drip_t + dt);
    }
    if (g->phase == RUN_PHASE_VICTORY || g->phase == RUN_PHASE_GAME_OVER) {
        r->victory_scroll_t = fminf(1.0f, r->victory_scroll_t + dt * 1.6f);
    } else {
        r->victory_scroll_t = 0.0f;
    }

    if (g->phase == RUN_PHASE_REWARD) {
        if (r->last_reward_serial != g->run.reward.serial) {
            r->last_reward_serial = g->run.reward.serial;
            r->reward_anim_t = 0.0f;
        }
        r->reward_anim_t = fminf(1.0f, r->reward_anim_t + dt * 2.5f);
    } else {
        r->reward_anim_t = 0.0f;
    }
    if (r->reward_reroll_t > 0.0f) {
        r->reward_reroll_t = fmaxf(0.0f, r->reward_reroll_t - dt);
    }

    if (r->intro_count > 0 && r->boss_banner_t <= 0.0f &&
        (g->phase == RUN_PHASE_PLAYING || g->phase == RUN_PHASE_WAVE_AFTERMATH)) {
        r->intro_t += dt;
        if (r->intro_t >= 2.0f) {
            r->intro_head = (r->intro_head + 1) % RENDER_MAX_INTRO_QUEUE;
            r->intro_count--;
            r->intro_t = 0.0f;
        }
    }

    track_ryu_and_bounces(r, g);

    for (int i = 0; i < RENDER_MAX_SPARKS; ++i) {
        VfxSpark *p = &r->sparks[i];
        if (!p->active) {
            continue;
        }
        p->age += dt;
        if (p->age >= p->life) {
            p->active = false;
            continue;
        }
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vx *= 0.94f;
        p->vy *= 0.94f;
    }

    int major = r->boss_banner_t > 0.0f ? 1 : 0;
    for (int i = 0; i < RENDER_MAX_BOLTS; ++i) {
        if (r->bolts[i].active) {
            r->bolts[i].age += dt;
            if (r->bolts[i].age >= r->bolts[i].life) {
                r->bolts[i].active = false;
            }
        }
    }
    for (int i = 0; i < RENDER_MAX_SEALS; ++i) {
        if (r->seals[i].active) {
            r->seals[i].age += dt;
            if (r->seals[i].age >= r->seals[i].life) {
                r->seals[i].active = false;
            }
        }
    }
    for (int i = 0; i < RENDER_MAX_CALLOUTS; ++i) {
        if (r->callouts[i].active) {
            r->callouts[i].age += dt;
            r->callouts[i].y -= 18.0f * dt;
            if (r->callouts[i].age >= r->callouts[i].life) {
                r->callouts[i].active = false;
            }
        }
    }
    for (int i = 0; i < RENDER_MAX_STROKES; ++i) {
        if (r->strokes[i].active) {
            r->strokes[i].age += dt;
            if (r->strokes[i].age >= r->strokes[i].life) {
                r->strokes[i].active = false;
            } else if (r->strokes[i].kind == 2 || r->strokes[i].kind == 4 ||
                       r->strokes[i].kind == 5 || r->strokes[i].kind == 6) {
                major++;
            }
        }
    }
    for (int i = 0; i < RENDER_MAX_CRACKS; ++i) {
        if (r->cracks[i].active) {
            r->cracks[i].age += dt;
            if (r->cracks[i].age >= r->cracks[i].life) {
                r->cracks[i].active = false;
            }
        }
    }
    r->major_fx_active = major;
}

Vector2 render_vfx_shake_offset(const RenderState *r, const UiView *ui)
{
    if (r == NULL || r->shake_t <= 0.0f || r->shake_amp <= 0.01f) {
        return (Vector2){0.0f, 0.0f};
    }
    const float scale = ui != NULL ? ((float)ui->shake_scale * 0.01f) : 1.0f;
    if (scale <= 0.001f) {
        return (Vector2){0.0f, 0.0f};
    }
    const float mag = r->shake_amp * scale * ink_clampf(r->shake_t * 5.0f, 0.0f, 1.0f);
    return (Vector2){
        sinf(r->vfx_time * 95.0f) * mag,
        cosf(r->vfx_time * 113.0f) * mag,
    };
}

static void draw_cyclone_stroke(RenderState *r, const VfxStroke *s, float u, float a)
{
    const bool heavy = s->style == BRUSH_STYLE_HEAVY;
    const bool ember = s->style == BRUSH_STYLE_EMBER;
    const bool quill = s->style == BRUSH_STYLE_QUILL;
    const float arm_w = heavy ? 10.5f : (quill ? 4.0f : 6.5f);
    for (int arm = 0; arm < 2; ++arm) {
        Vector2 pts[14];
        const float base_a = (float)arm * 3.14159f + u * 5.2f;
        for (int k = 0; k < 14; ++k) {
            const float ku = (float)k / 13.0f;
            const float ang = base_a + ku * 4.4f;
            const float rad = s->radius * (0.18f + 0.82f * ku) * (0.75f + 0.25f * u);
            pts[k] = (Vector2){s->x + cosf(ang) * rad, s->y + sinf(ang) * rad};
        }
        ink_stroke(r, pts, 14,
                   (BrushProfile){.w_max = arm_w,
                                  .w_min_ratio = 0.25f,
                                  .entry = 0.18f,
                                  .exit = 0.35f,
                                  .dry = heavy ? 0.70f : 0.4f,
                                  .seed = s->seed + (uint32_t)arm},
                   (InkTone){.color = heavy ? PAL_SUMI_SCORCH : PAL_SUMI_DARK, .alpha = a},
                   ink_clampf(u * 3.5f, 0.15f, 1.0f));
    }
    if (heavy) {
        ink_arc_stroke(r, (Vector2){s->x, s->y}, s->radius * (0.9f + 0.25f * u), u * 2.0f,
                       u * 2.0f + 4.6f,
                       (BrushProfile){.w_max = 7.0f,
                                      .w_min_ratio = 0.2f,
                                      .entry = 0.2f,
                                      .exit = 0.4f,
                                      .dry = 0.8f,
                                      .seed = s->seed + 7u},
                       (InkTone){.color = PAL_SUMI_HEAVY, .alpha = a * 0.6f});
    } else if (ember) {
        ink_arc_stroke(r, (Vector2){s->x, s->y}, s->radius * (0.45f + 0.35f * u), -u * 6.0f,
                       -u * 6.0f + 5.2f,
                       (BrushProfile){.w_max = 5.0f,
                                      .w_min_ratio = 0.3f,
                                      .entry = 0.15f,
                                      .exit = 0.35f,
                                      .dry = 0.3f,
                                      .seed = s->seed + 11u},
                       (InkTone){.color = PAL_SHU, .alpha = a * 0.9f});
    } else if (quill) {
        uint64_t rng = (uint64_t)s->seed ^ UINT64_C(0x51554c4c41524353);
        for (int k = 0; k < 3; ++k) {
            const float ang = u * 7.0f + (float)k * 2.094f;
            const float r0 = s->radius * 0.55f;
            const float r1 = s->radius * (0.85f + 0.15f * ink_rng_f01(&rng));
            const Vector2 zig[3] = {
                {s->x + cosf(ang) * r0, s->y + sinf(ang) * r0},
                {s->x + cosf(ang + 0.18f) * (r0 + r1) * 0.5f,
                 s->y + sinf(ang + 0.18f) * (r0 + r1) * 0.5f},
                {s->x + cosf(ang - 0.05f) * r1, s->y + sinf(ang - 0.05f) * r1},
            };
            ink_stroke(r, zig, 3,
                       (BrushProfile){.w_max = 2.4f,
                                      .w_min_ratio = 0.4f,
                                      .entry = 0.05f,
                                      .exit = 0.2f,
                                      .dry = 0.0f,
                                      .seed = s->seed + 20u + (uint32_t)k},
                       (InkTone){.color = PAL_AO_HOT, .alpha = a}, 1.0f);
        }
    }
    const float mark = 5.0f * (1.0f - 0.5f * u);
    ink_teardrop(r, (Vector2){s->x, s->y}, (Vector2){0.0f, -1.0f}, mark * 1.3f, mark,
                 pal_color_alpha(PAL_SHU, a));
}

static void draw_gale_stroke(RenderState *r, const VfxStroke *s, float u, float a)
{
    const bool heavy = s->style == BRUSH_STYLE_HEAVY;
    const bool ember = s->style == BRUSH_STYLE_EMBER;
    const bool quill = s->style == BRUSH_STYLE_QUILL;
    const Vector2 pts[3] = {
        {s->x, s->y},
        {s->x + s->dx * s->radius * 0.5f - s->dy * 12.0f,
         s->y + s->dy * s->radius * 0.5f + s->dx * 12.0f},
        {s->x + s->dx * s->radius, s->y + s->dy * s->radius},
    };
    const float reveal = ink_clampf(u * 4.0f, 0.2f, 1.0f);
    ink_stroke(r, pts, 3,
               (BrushProfile){.w_max = heavy ? 20.0f : (quill ? 8.0f : 14.0f),
                              .w_min_ratio = 0.18f,
                              .entry = 0.08f,
                              .exit = 0.55f,
                              .dry = heavy ? 0.80f : (quill ? 0.25f : 0.65f),
                              .seed = s->seed},
               (InkTone){.color = heavy ? PAL_SUMI_SCORCH : PAL_SUMI_DARK, .alpha = a}, reveal);
    if (ember) {
        ink_stroke(r, pts, 3,
                   (BrushProfile){.w_max = 6.0f,
                                  .w_min_ratio = 0.2f,
                                  .entry = 0.1f,
                                  .exit = 0.5f,
                                  .dry = 0.4f,
                                  .seed = s->seed + 3u},
                   (InkTone){.color = PAL_SHU, .alpha = a * 0.9f}, reveal);
    } else if (quill) {
        for (int side = -1; side <= 1; side += 2) {
            const float off = (float)side * 14.0f;
            const Vector2 rail[2] = {
                {s->x - s->dy * off, s->y + s->dx * off},
                {s->x + s->dx * s->radius * 0.8f - s->dy * off,
                 s->y + s->dy * s->radius * 0.8f + s->dx * off},
            };
            ink_stroke(r, rail, 2,
                       (BrushProfile){.w_max = 2.2f,
                                      .w_min_ratio = 0.4f,
                                      .entry = 0.1f,
                                      .exit = 0.5f,
                                      .dry = 0.0f,
                                      .seed = s->seed + 5u + (uint32_t)(side + 1)},
                       (InkTone){.color = PAL_AO_HOT, .alpha = a * 0.85f}, reveal);
        }
    }
}

void render_vfx_draw_world(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL) {
        return;
    }
    render_set_blend(r, BLEND_ALPHA);

    for (int i = 0; i < RENDER_MAX_SPARKS; ++i) {
        const VfxSpark *p = &r->sparks[i];
        if (!p->active || p->life <= 0.0f) {
            continue;
        }
        const float u = ink_clampf(p->age / p->life, 0.0f, 1.0f);
        const float a = (1.0f - u) * 0.85f;
        const PaletteId pal = (PaletteId)p->color_id;
        if (p->is_wash) {
            const float rad = p->size * (1.0f + 0.12f * u);
            ink_wash(r, (Vector2){p->x, p->y}, rad, rad * 0.85f, u,
                     (InkTone){.color = pal, .alpha = a * 0.55f}, 1, p->seed);
        } else {
            const Vector2 dir =
                (p->vx != 0.0f || p->vy != 0.0f) ? (Vector2){p->vx, p->vy} : (Vector2){1.0f, 0.0f};
            ink_teardrop(r, (Vector2){p->x, p->y}, dir, p->size * (1.0f + 0.1f * u),
                         p->size * 0.52f, pal_color_alpha(pal, a));
        }
    }

    for (int i = 0; i < RENDER_MAX_STROKES; ++i) {
        const VfxStroke *s = &r->strokes[i];
        if (!s->active || s->life <= 0.0f) {
            continue;
        }
        const float u = ink_clampf(s->age / s->life, 0.0f, 1.0f);
        const float a = (1.0f - u * u) * 0.88f;

        if (s->kind == 0) {
            draw_cyclone_stroke(r, s, u, a);
        } else if (s->kind == 1) {
            draw_gale_stroke(r, s, u, a);
        } else if (s->kind == 10) {
            /* Purification shockwave: a pale enso sweeping out from the clear point */
            const float rad = s->radius * (0.08f + 0.92f * sqrtf(u));
            const float gap_a = (float)(s->seed % 360u) * 0.01745f;
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad, gap_a, gap_a + 5.9f,
                           (BrushProfile){.w_max = 22.0f * (1.0f - 0.6f * u),
                                          .w_min_ratio = 0.25f,
                                          .entry = 0.12f,
                                          .exit = 0.30f,
                                          .dry = 0.55f,
                                          .seed = s->seed},
                           (InkTone){.color = PAL_GOFUN, .alpha = a * 0.85f});
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad * 0.94f, gap_a + 0.2f, gap_a + 5.6f,
                           (BrushProfile){.w_max = 4.0f,
                                          .w_min_ratio = 0.3f,
                                          .entry = 0.12f,
                                          .exit = 0.30f,
                                          .dry = 0.2f,
                                          .seed = s->seed + 9u},
                           (InkTone){.color = PAL_KIN, .alpha = a * 0.75f});
        } else if (s->kind == 11) {
            const float rad = s->radius * (0.5f + 0.9f * u);
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad, 0.0f, k_vfx_two_pi * 0.92f,
                           (BrushProfile){.w_max = 3.2f,
                                          .w_min_ratio = 0.3f,
                                          .entry = 0.15f,
                                          .exit = 0.35f,
                                          .dry = 0.6f,
                                          .seed = s->seed},
                           (InkTone){.color = PAL_SUMI_HEAVY, .alpha = a * 0.55f});
        } else if (s->kind == 12) {
            const float rad = s->radius * (0.85f + 0.35f * u);
            const float mid = atan2f(s->dy, s->dx);
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad, mid - 1.1f, mid + 1.1f,
                           (BrushProfile){.w_max = 6.0f,
                                          .w_min_ratio = 0.3f,
                                          .entry = 0.15f,
                                          .exit = 0.15f,
                                          .dry = 0.1f,
                                          .seed = s->seed},
                           (InkTone){.color = PAL_KIN, .alpha = a});
        } else if (s->kind == 2) {
            const float wx = u * (float)INK_SCREEN_W;
            const Vector2 pts[3] = {{wx - 180.0f, 60.0f}, {wx, 360.0f}, {wx - 120.0f, 660.0f}};
            ink_stroke(r, pts, 3,
                       (BrushProfile){.w_max = 42.0f,
                                      .w_min_ratio = 0.30f,
                                      .entry = 0.15f,
                                      .exit = 0.35f,
                                      .dry = 0.6f,
                                      .seed = s->seed},
                       (InkTone){.color = PAL_GOFUN, .alpha = a * 0.72f}, 1.0f);
        } else if (s->kind == 3) {
            const Vector2 pts[2] = {{s->x, s->y}, {s->dx, s->dy}};
            ink_stroke(r, pts, 2,
                       (BrushProfile){.w_max = 3.2f,
                                      .w_min_ratio = 0.25f,
                                      .entry = 0.1f,
                                      .exit = 0.55f,
                                      .dry = 0.2f,
                                      .seed = s->seed},
                       (InkTone){.color = PAL_KIN, .alpha = a}, 1.0f);
        } else if (s->kind == 4) {
            ink_wash(r, (Vector2){s->x, s->y}, s->radius * (0.4f + u), s->radius * (0.35f + u),
                     u * 1.4f, (InkTone){.color = PAL_SUMI_HEAVY, .alpha = a * 0.45f}, 2, s->seed);
        } else if (s->kind == 5) {
            /* Thunderburst: Zen enso with 1 gap + 8 outward thick radial flicks */
            const float rad = s->radius * (0.45f + 0.65f * u);
            const float gap_a = (float)(s->seed % 360u) * 0.01745f;
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad, gap_a, gap_a + 5.45f,
                           (BrushProfile){.w_max = 11.5f,
                                          .w_min_ratio = 0.20f,
                                          .entry = 0.14f,
                                          .exit = 0.32f,
                                          .dry = 0.45f,
                                          .seed = s->seed},
                           (InkTone){.color = PAL_SUMI_SCORCH, .alpha = a});
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad * 0.88f, gap_a + 0.15f, gap_a + 5.20f,
                           (BrushProfile){.w_max = 4.5f,
                                          .w_min_ratio = 0.25f,
                                          .entry = 0.15f,
                                          .exit = 0.35f,
                                          .dry = 0.25f,
                                          .seed = s->seed + 3u},
                           (InkTone){.color = PAL_AO_HOT, .alpha = a * 0.85f});
            for (int k = 0; k < 8; ++k) {
                const float fa = gap_a + (float)k * 0.78f + ((k & 1) ? 0.12f : -0.08f);
                const float r0 = rad * 0.55f;
                const float r1 = rad * (1.18f + 0.25f * (float)(k % 3));
                const Vector2 ray[2] = {
                    {s->x + cosf(fa) * r0, s->y + sinf(fa) * r0},
                    {s->x + cosf(fa) * r1, s->y + sinf(fa) * r1},
                };
                ink_stroke(r, ray, 2,
                           (BrushProfile){.w_max = 6.2f,
                                          .w_min_ratio = 0.18f,
                                          .entry = 0.08f,
                                          .exit = 0.60f,
                                          .dry = 0.35f,
                                          .seed = s->seed + 20u + (uint32_t)k},
                           (InkTone){.color = (k & 1) ? PAL_SHU : PAL_AO, .alpha = a}, 1.0f);
            }
        } else if (s->kind == 6) {
            /* Cataclysm: gold-rimmed grand enso + inward-then-outward flicks + gold sunago */
            const float rad = s->radius * (u < 0.25f ? (1.15f - u * 1.4f) : (0.55f + u * 0.75f));
            const float gap_a = (float)(s->seed % 360u) * 0.01745f;
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad, gap_a, gap_a + 5.60f,
                           (BrushProfile){.w_max = 16.0f,
                                          .w_min_ratio = 0.20f,
                                          .entry = 0.14f,
                                          .exit = 0.30f,
                                          .dry = 0.35f,
                                          .seed = s->seed},
                           (InkTone){.color = PAL_SUMI_SCORCH, .alpha = a});
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad + 6.0f, gap_a + 0.08f, gap_a + 5.52f,
                           (BrushProfile){.w_max = 6.2f,
                                          .w_min_ratio = 0.25f,
                                          .entry = 0.12f,
                                          .exit = 0.30f,
                                          .dry = 0.20f,
                                          .seed = s->seed + 5u},
                           (InkTone){.color = PAL_KIN, .alpha = a * 0.96f});
            for (int k = 0; k < 10; ++k) {
                const float fa = gap_a + (float)k * 0.628f;
                const float r0 = rad * 0.65f;
                const float r1 = rad * (1.25f + 0.22f * (float)(k % 3));
                const Vector2 ray[2] = {
                    {s->x + cosf(fa) * r0, s->y + sinf(fa) * r0},
                    {s->x + cosf(fa) * r1, s->y + sinf(fa) * r1},
                };
                ink_stroke(r, ray, 2,
                           (BrushProfile){.w_max = 5.4f,
                                          .w_min_ratio = 0.20f,
                                          .entry = 0.10f,
                                          .exit = 0.55f,
                                          .dry = 0.30f,
                                          .seed = s->seed + 40u + (uint32_t)k},
                           (InkTone){.color = (k % 3 == 0) ? PAL_KIN_HOT
                                                           : ((k % 3 == 1) ? PAL_SHU : PAL_AO),
                                     .alpha = a},
                           1.0f);
            }
        } else if (s->kind == 7) {
            /* Mini broken enso for bounce-orb ricochet */
            const float rad = s->radius * (0.65f + 0.55f * u);
            const float gap_a = (float)(s->seed % 360u) * 0.01745f;
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad, gap_a, gap_a + 5.1f,
                           (BrushProfile){.w_max = 3.6f,
                                          .w_min_ratio = 0.25f,
                                          .entry = 0.15f,
                                          .exit = 0.35f,
                                          .dry = 0.30f,
                                          .seed = s->seed},
                           (InkTone){.color = PAL_AO_HOT, .alpha = a});
        } else if (s->kind == 8) {
            /* Nue dash dragon-vein + azure lightning trail */
            const Vector2 pts[2] = {{s->x, s->y}, {s->dx, s->dy}};
            ink_stroke(r, pts, 2,
                       (BrushProfile){.w_max = 10.0f,
                                      .w_min_ratio = 0.20f,
                                      .entry = 0.12f,
                                      .exit = 0.45f,
                                      .dry = 0.55f,
                                      .seed = s->seed},
                       (InkTone){.color = PAL_SUMI_CLEAR, .alpha = a * 0.72f}, 1.0f);
            ink_stroke(r, pts, 2,
                       (BrushProfile){.w_max = 3.2f,
                                      .w_min_ratio = 0.30f,
                                      .entry = 0.10f,
                                      .exit = 0.40f,
                                      .dry = 0.20f,
                                      .seed = s->seed + 2u},
                       (InkTone){.color = PAL_AO, .alpha = a * 0.85f}, 1.0f);
        } else if (s->kind == 9) {
            /* Ryu Dragon Orb shatter: elemental ink splatter + broken enso */
            const int part = (int)lroundf(s->dx);
            const PaletteId pal = (part == 0) ? PAL_SHU : ((part == 1) ? PAL_AO : PAL_GOFUN);
            const PaletteId rim =
                (part == 0) ? PAL_SHU_HOT : ((part == 1) ? PAL_AO_HOT : PAL_SUMI_DARK);
            const float rad = s->radius * (0.45f + 0.85f * u);
            const float gap_a = (float)(s->seed % 360u) * 0.01745f;
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad, gap_a, gap_a + 4.95f,
                           (BrushProfile){.w_max = 8.5f,
                                          .w_min_ratio = 0.20f,
                                          .entry = 0.14f,
                                          .exit = 0.38f,
                                          .dry = 0.48f,
                                          .seed = s->seed},
                           (InkTone){.color = pal, .alpha = a});
            ink_arc_stroke(r, (Vector2){s->x, s->y}, rad * 0.84f, gap_a + 0.25f, gap_a + 4.60f,
                           (BrushProfile){.w_max = 3.4f,
                                          .w_min_ratio = 0.25f,
                                          .entry = 0.12f,
                                          .exit = 0.35f,
                                          .dry = 0.30f,
                                          .seed = s->seed + 7u},
                           (InkTone){.color = rim, .alpha = a * 0.90f});
            uint64_t srng = (uint64_t)s->seed ^ UINT64_C(0x504152544252454b);
            if (srng == 0u) {
                srng = 1u;
            }
            const float sp_ang = gap_a + 2.2f;
            ink_splatter(r, (Vector2){s->x, s->y}, (Vector2){cosf(sp_ang), sinf(sp_ang)},
                         0.85f * (1.0f - 0.35f * u), (InkTone){.color = pal, .alpha = a * 0.82f},
                         &srng);
        }
    }

    for (int i = 0; i < RENDER_MAX_CRACKS; ++i) {
        const VfxCrack *c = &r->cracks[i];
        if (!c->active || c->life <= 0.0f) {
            continue;
        }
        const float u = ink_clampf(c->age / c->life, 0.0f, 1.0f);
        const float a = (1.0f - u) * 0.85f;
        const Vector2 pts[4] = {
            {c->x, c->y},
            {c->x + c->dx * 18.0f - c->dy * 8.0f, c->y + c->dy * 18.0f + c->dx * 8.0f},
            {c->x + c->dx * 36.0f + c->dy * 6.0f, c->y + c->dy * 36.0f - c->dx * 6.0f},
            {c->x + c->dx * 54.0f, c->y + c->dy * 54.0f},
        };
        ink_stroke(r, pts, 4,
                   (BrushProfile){.w_max = 2.8f,
                                  .w_min_ratio = 0.35f,
                                  .entry = 0.1f,
                                  .exit = 0.45f,
                                  .dry = 0.2f,
                                  .seed = c->seed},
                   (InkTone){.color = PAL_SUMI_HEAVY, .alpha = a}, 1.0f);
    }

    bool any_bolt = false;
    for (int i = 0; i < RENDER_MAX_BOLTS; ++i) {
        const VfxBolt *b = &r->bolts[i];
        if (!b->active || b->life <= 0.0f) {
            continue;
        }
        any_bolt = true;
        const float u = ink_clampf(b->age / b->life, 0.0f, 1.0f);
        const float a = (1.0f - u) * 0.96f;
        Vector2 pts[9];
        const float dx = b->x2 - b->x1;
        const float dy = b->y2 - b->y1;
        const float len = sqrtf(dx * dx + dy * dy);
        const Vector2 perp = len > 1e-3f ? (Vector2){-dy / len, dx / len} : (Vector2){0.0f, 1.0f};
        uint64_t rng = (uint64_t)b->seed ^ UINT64_C(0x123456789abcdef);
        for (int k = 0; k < 9; ++k) {
            const float ku = (float)k / 8.0f;
            const float jitter = (k == 0 || k == 8) ? 0.0f : (ink_rng_f01(&rng) - 0.5f) * 26.0f;
            pts[k] =
                (Vector2){b->x1 + dx * ku + perp.x * jitter, b->y1 + dy * ku + perp.y * jitter};
        }
        ink_stroke(r, pts, 9,
                   (BrushProfile){.w_max = 5.2f,
                                  .w_min_ratio = 0.30f,
                                  .entry = 0.12f,
                                  .exit = 0.25f,
                                  .dry = 0.0f,
                                  .seed = b->seed},
                   (InkTone){.color = PAL_AO, .alpha = a}, 1.0f);
    }
    if (any_bolt) {
        render_set_blend(r, BLEND_ADDITIVE);
        for (int i = 0; i < RENDER_MAX_BOLTS; ++i) {
            const VfxBolt *b = &r->bolts[i];
            if (!b->active || b->life <= 0.0f) {
                continue;
            }
            const float u = ink_clampf(b->age / b->life, 0.0f, 1.0f);
            const float a = (1.0f - u) * 0.90f;
            Vector2 pts[9];
            const float dx = b->x2 - b->x1;
            const float dy = b->y2 - b->y1;
            const float len = sqrtf(dx * dx + dy * dy);
            const Vector2 perp =
                len > 1e-3f ? (Vector2){-dy / len, dx / len} : (Vector2){0.0f, 1.0f};
            uint64_t rng = (uint64_t)b->seed ^ UINT64_C(0x123456789abcdef);
            for (int k = 0; k < 9; ++k) {
                const float ku = (float)k / 8.0f;
                const float jitter = (k == 0 || k == 8) ? 0.0f : (ink_rng_f01(&rng) - 0.5f) * 26.0f;
                pts[k] =
                    (Vector2){b->x1 + dx * ku + perp.x * jitter, b->y1 + dy * ku + perp.y * jitter};
            }
            ink_stroke(r, pts, 9,
                       (BrushProfile){.w_max = 1.6f,
                                      .w_min_ratio = 0.4f,
                                      .entry = 0.05f,
                                      .exit = 0.1f,
                                      .dry = 0.0f,
                                      .seed = b->seed + 1u},
                       (InkTone){.color = PAL_AO_HOT, .alpha = a}, 1.0f);
        }
        render_set_blend(r, BLEND_ALPHA);
    }

    (void)ui;
}

void render_vfx_draw_flash(RenderState *r, const UiView *ui)
{
    const bool suppress_flash = (ui != NULL && ui->reduce_flash);
    if (r == NULL || r->flash_t <= 0.0f || suppress_flash) {
        return;
    }
    const Vector2 screen[4] = {{0.0f, 0.0f},
                               {(float)INK_SCREEN_W, 0.0f},
                               {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                               {0.0f, (float)INK_SCREEN_H}};
    ink_fill_poly(r, screen, 4, pal_color_alpha(PAL_GOFUN, r->flash_t * 3.0f));
}

/* Only two seals fit on screen at once; milestone seals outrank reaction and UI stamps, and the
 * newest wins within a tier, instead of whichever happened to sit at the lowest array slot. */
int32_t render_vfx_pick_seals(const RenderState *r, int32_t out[2])
{
    out[0] = -1;
    out[1] = -1;
    for (int32_t i = 0; i < RENDER_MAX_SEALS; ++i) {
        const VfxSeal *s = &r->seals[i];
        if (!s->active || s->life <= 0.0f) {
            continue;
        }
        for (int32_t slot = 0; slot < 2; ++slot) {
            const VfxSeal *cur = out[slot] >= 0 ? &r->seals[out[slot]] : NULL;
            const bool better = cur == NULL || s->priority > cur->priority ||
                                (s->priority == cur->priority && s->age < cur->age);
            if (better) {
                if (slot == 0) {
                    out[1] = out[0];
                }
                out[slot] = i;
                break;
            }
        }
    }
    return (out[0] >= 0 ? 1 : 0) + (out[1] >= 0 ? 1 : 0);
}

void render_vfx_draw_ui(RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL) {
        return;
    }

    int32_t picked[2];
    const int32_t seal_n = render_vfx_pick_seals(r, picked);
    for (int32_t k = seal_n - 1; k >= 0; --k) {
        const VfxSeal *s = &r->seals[picked[k]];
        const float u = ink_clampf(s->age / s->life, 0.0f, 1.0f);
        seal_stamp(r, s->glyph, (Vector2){s->x, s->y}, s->size, (SealStyle)s->style, u);
    }

    /* Callouts: brush title font strictly >= 40px (art-direction §9.6) */
    for (int i = 0; i < RENDER_MAX_CALLOUTS; ++i) {
        const VfxCallout *c = &r->callouts[i];
        if (!c->active || c->life <= 0.0f) {
            continue;
        }
        const float u = ink_clampf(c->age / c->life, 0.0f, 1.0f);
        const float a = ink_clampf((1.0f - u) * 1.4f, 0.0f, 1.0f);
        const float fsize =
            c->large ? (c->text_id == TEXT_CALLOUT_CATACLYSM ? 46.0f : 40.0f) : 28.0f;
        char buf[96];
        if (c->count_arg > 0) {
            snprintf(buf, sizeof buf, "%s%d", text_get(c->text_id, r->use_ja), c->count_arg);
        } else {
            snprintf(buf, sizeof buf, "%s", text_get(c->text_id, r->use_ja));
        }
        const Vector2 m = render_measure_text(r, buf, fsize, c->large);
        const float half_w = m.x * 0.5f + 10.0f;
        const float half_h = m.y * 0.5f + 4.0f;
        const float x = ink_clampf(c->x, 8.0f + half_w, (float)INK_SCREEN_W - 8.0f - half_w);
        const float y = ink_clampf(c->y, 78.0f + half_h, (float)INK_SCREEN_H - 8.0f - half_h);
        const Rectangle backing = {x - half_w, y - half_h, half_w * 2.0f, half_h * 2.0f};
        ink_fill_panel(r, backing, PAL_GOFUN, 0.80f * a, PAL_SUMI_CLEAR, 0.0f, 700u + (uint32_t)i);
        render_draw_text_centered(r, buf, (Vector2){x, y}, fsize, (PaletteId)c->color_id, a,
                                  c->large);
    }
}
