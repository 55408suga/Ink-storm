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
                      float life)
{
    r->seals[r->seal_cursor] = (VfxSeal){
        .active = true,
        .glyph = glyph,
        .x = x,
        .y = y,
        .size = size,
        .style = (int32_t)style,
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

static void push_stroke(RenderState *r, int32_t kind, float x, float y, float dx, float dy,
                        float radius, float life)
{
    r->strokes[r->stroke_cursor] = (VfxStroke){
        .active = true,
        .kind = kind,
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
        case EV_CYCLONE:
            push_stroke(r, 0, ev->x, ev->y, 0.0f, 0.0f, ev->value > 0 ? (float)ev->value : 110.0f,
                        0.72f);
            push_seal(r, TEXT_NONE, ev->x, ev->y, 26.0f, SEAL_SHUBUN, 0.65f);
            break;
        case EV_GALE:
            push_stroke(r, 1, ev->x, ev->y, ev->x2, ev->y2, 240.0f, 0.36f);
            break;
        case EV_ORB_PICKUP:
            push_stroke(r, 3, ev->x, ev->y, g->player.cursor_x, g->player.cursor_y,
                        (ev->flags & EVF_ORB_AUTO) != 0 ? 1.0f : 0.6f, 0.20f);
            spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 3, 60.0f, 4.0f, 0.20f, PAL_KIN_HOT, false);
            break;
        case EV_SHRINE_HIT:
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
            if (r->major_fx_active < 2) {
                r->boss_banner_t = 2.0f;
                r->boss_banner_id = (EnemyDefId)ev->value;
                r->major_fx_active++;
            }
            add_shake(r, 5.0f, 0.24f);
            break;
        case EV_BOSS_PHASE:
            if (r->major_fx_active < 2) {
                push_stroke(r, 4, ev->x, ev->y, 0.0f, 0.0f, 130.0f, 1.50f);
                spawn_burst(r, ev->x, ev->y, 0.0f, 0.0f, 18, 260.0f, 12.0f, 0.85f, PAL_SUMI_DARK,
                            true);
                r->major_fx_active++;
            }
            add_shake(r, 7.0f, 0.30f);
            break;
        case EV_WAVE_CLEAR:
            push_seal(r, TEXT_SEAL_PURIFY, (float)INK_SCREEN_W * 0.5f, 128.0f, 54.0f, SEAL_HAKUBUN,
                      1.10f);
            break;
        case EV_ACT_CLEAR:
            if (r->major_fx_active < 2) {
                push_stroke(r, 2, 0.0f, (float)INK_SCREEN_H * 0.5f, 1.0f, 0.0f, (float)INK_SCREEN_W,
                            1.50f);
                push_seal(r, TEXT_SEAL_SLAY, (float)INK_SCREEN_W * 0.5f,
                          (float)INK_SCREEN_H * 0.42f, 86.0f, SEAL_HAKUBUN, 1.50f);
                r->major_fx_active++;
            }
            break;
        case EV_REWARD_PICK:
            push_seal(r, TEXT_SEAL_DONE, (float)INK_SCREEN_W * 0.5f, (float)INK_SCREEN_H * 0.52f,
                      62.0f, SEAL_HAKUBUN, 0.60f);
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
    for (int s = 0; s < SKILL_COUNT; ++s) {
        if (r->skill_deny_t[s] > 0.0f) {
            r->skill_deny_t[s] = fmaxf(0.0f, r->skill_deny_t[s] - dt);
        }
    }
    if (r->boss_banner_t > 0.0f) {
        r->boss_banner_t = fmaxf(0.0f, r->boss_banner_t - dt);
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

    if (r->intro_count > 0) {
        r->intro_t += dt;
        if (r->intro_t >= 2.0f) {
            r->intro_head = (r->intro_head + 1) % RENDER_MAX_INTRO_QUEUE;
            r->intro_count--;
            r->intro_t = 0.0f;
        }
    }

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
            } else if (r->strokes[i].kind == 2 || r->strokes[i].kind == 4) {
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

void render_vfx_draw_world(RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL) {
        return;
    }
    render_set_blend(r, BLEND_ALPHA);

    /* Teardrop sparks & ink splatter washes */
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

    /* Cyclone spirals, gale dragon-vein strokes, orb pickup flicks, act wipes */
    for (int i = 0; i < RENDER_MAX_STROKES; ++i) {
        const VfxStroke *s = &r->strokes[i];
        if (!s->active || s->life <= 0.0f) {
            continue;
        }
        const float u = ink_clampf(s->age / s->life, 0.0f, 1.0f);
        const float a = (1.0f - u * u) * 0.88f;

        if (s->kind == 0) {
            /* Two rotating spiral ink_strokes */
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
                           (BrushProfile){.w_max = 6.5f,
                                          .w_min_ratio = 0.25f,
                                          .entry = 0.18f,
                                          .exit = 0.35f,
                                          .dry = 0.4f,
                                          .seed = s->seed + (uint32_t)arm},
                           (InkTone){.color = PAL_SUMI_DARK, .alpha = a},
                           ink_clampf(u * 3.5f, 0.15f, 1.0f));
            }
        } else if (s->kind == 1) {
            /* Dragon-vein gale burst stroke */
            const Vector2 pts[3] = {
                {s->x, s->y},
                {s->x + s->dx * s->radius * 0.5f - s->dy * 12.0f,
                 s->y + s->dy * s->radius * 0.5f + s->dx * 12.0f},
                {s->x + s->dx * s->radius, s->y + s->dy * s->radius},
            };
            ink_stroke(r, pts, 3,
                       (BrushProfile){.w_max = 14.0f,
                                      .w_min_ratio = 0.18f,
                                      .entry = 0.08f,
                                      .exit = 0.55f,
                                      .dry = 0.65f,
                                      .seed = s->seed},
                       (InkTone){.color = PAL_SUMI_DARK, .alpha = a},
                       ink_clampf(u * 4.0f, 0.2f, 1.0f));
        } else if (s->kind == 2) {
            /* Act clear sweeping wipe */
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
            /* Orb pickup short flick toward cursor */
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
            /* Boss phase burst wash */
            ink_wash(r, (Vector2){s->x, s->y}, s->radius * (0.4f + u), s->radius * (0.35f + u),
                     u * 1.4f, (InkTone){.color = PAL_SUMI_HEAVY, .alpha = a * 0.45f}, 2, s->seed);
        }
    }

    /* Shrine hit paper cracks */
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

    /* Lightning bolts: 8-node zigzag, outer azure + 1px additive white-hot core */
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

    /* Rate-limited subtle paper brighten flash */
    if (r->flash_t > 0.0f) {
        const Vector2 screen[4] = {{0.0f, 0.0f},
                                   {(float)INK_SCREEN_W, 0.0f},
                                   {(float)INK_SCREEN_W, (float)INK_SCREEN_H},
                                   {0.0f, (float)INK_SCREEN_H}};
        ink_fill_poly(r, screen, 4, pal_color_alpha(PAL_GOFUN, r->flash_t * 3.0f));
    }
}

void render_vfx_draw_ui(RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL) {
        return;
    }

    /* Up to 2 seals drawn simultaneously (art-direction §12.3) */
    int seals_drawn = 0;
    for (int i = 0; i < RENDER_MAX_SEALS && seals_drawn < 2; ++i) {
        const VfxSeal *s = &r->seals[i];
        if (!s->active || s->life <= 0.0f) {
            continue;
        }
        const float u = ink_clampf(s->age / s->life, 0.0f, 1.0f);
        seal_stamp(r, s->glyph, (Vector2){s->x, s->y}, s->size, (SealStyle)s->style, u);
        seals_drawn++;
    }

    /* Callouts (drawn after post-shader so text stays crisp) */
    for (int i = 0; i < RENDER_MAX_CALLOUTS; ++i) {
        const VfxCallout *c = &r->callouts[i];
        if (!c->active || c->life <= 0.0f) {
            continue;
        }
        const float u = ink_clampf(c->age / c->life, 0.0f, 1.0f);
        const float a = ink_clampf((1.0f - u) * 1.4f, 0.0f, 1.0f);
        const float fsize = c->large ? 36.0f : 28.0f;
        char buf[96];
        if (c->count_arg > 0) {
            snprintf(buf, sizeof buf, "%s%d", text_get(c->text_id, r->use_ja), c->count_arg);
        } else {
            snprintf(buf, sizeof buf, "%s", text_get(c->text_id, r->use_ja));
        }
        const Vector2 m = render_measure_text(r, buf, fsize, c->large);
        const Rectangle backing = {c->x - m.x * 0.5f - 10.0f, c->y - m.y * 0.5f - 4.0f, m.x + 20.0f,
                                   m.y + 8.0f};
        ink_fill_panel(r, backing, PAL_GOFUN, 0.80f * a, PAL_SUMI_CLEAR, 0.0f, 700u + (uint32_t)i);
        render_draw_text_centered(r, buf, (Vector2){c->x, c->y}, fsize, (PaletteId)c->color_id, a,
                                  c->large);
    }
}
