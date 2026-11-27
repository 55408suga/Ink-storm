#include "bgm_fade.h"

#include <stddef.h>

static const float k_fade_per_second = 2.0f;
static const float k_pause_duck_min = 0.35f;
static const float k_pause_duck_slew = (1.0f - 0.35f) / 0.08f;

void bgm_fade_reset(BgmFade *f)
{
    if (f != NULL) {
        *f = (BgmFade){.cur = BGM_NONE,
                       .prev = BGM_NONE,
                       .prev2 = BGM_NONE,
                       .gain_cur = 0.0f,
                       .gain_prev = 0.0f,
                       .gain_prev2 = 0.0f,
                       .pause_duck = 1.0f};
    }
}

/* Keep the louder outgoing track in `prev` and the quieter one in `prev2` so rapid 3-track
 * switches taper both smoothly without hard-cutting the louder outgoing stream. */
static void order_outgoing(BgmFade *f)
{
    if (f->prev == BGM_NONE && f->prev2 != BGM_NONE) {
        f->prev = f->prev2;
        f->gain_prev = f->gain_prev2;
        f->prev2 = BGM_NONE;
        f->gain_prev2 = 0.0f;
    } else if (f->prev != BGM_NONE && f->prev2 != BGM_NONE && f->gain_prev2 > f->gain_prev) {
        const BgmTrack tmp_t = f->prev;
        const float tmp_g = f->gain_prev;
        f->prev = f->prev2;
        f->gain_prev = f->gain_prev2;
        f->prev2 = tmp_t;
        f->gain_prev2 = tmp_g;
    }
}

/* Each track keeps its own gain. A track that stops being current carries its present gain into
 * the fade-out, so interrupting a fade (A -> none -> B, A -> B -> A, or A -> B -> C) never makes
 * the old track jump back to full volume, restart from the top, or hard-cut the loudest deck. */
BgmFadeActions bgm_fade_request(BgmFade *f, BgmTrack desired)
{
    BgmFadeActions act = {.stop = BGM_NONE, .play = BGM_NONE};
    if (f == NULL || desired == f->cur) {
        return act;
    }
    if (desired != BGM_NONE && desired == f->prev) {
        const BgmTrack back = f->prev;
        const float back_gain = f->gain_prev;
        f->prev = f->cur;
        f->gain_prev = f->gain_cur;
        f->cur = back;
        f->gain_cur = back_gain;
        order_outgoing(f);
        return act;
    }
    if (desired != BGM_NONE && desired == f->prev2) {
        const BgmTrack back = f->prev2;
        const float back_gain = f->gain_prev2;
        f->prev2 = f->cur;
        f->gain_prev2 = f->gain_cur;
        f->cur = back;
        f->gain_cur = back_gain;
        order_outgoing(f);
        return act;
    }
    if (f->cur == BGM_NONE) {
        f->cur = desired;
        f->gain_cur = 0.0f;
        act.play = desired;
        return act;
    }
    if (f->prev == BGM_NONE) {
        f->prev = f->cur;
        f->gain_prev = f->gain_cur;
    } else if (f->prev2 == BGM_NONE) {
        f->prev2 = f->cur;
        f->gain_prev2 = f->gain_cur;
        order_outgoing(f);
    } else {
        /* All 3 slots occupied: keep the two loudest outgoing tracks and stop only the quietest. */
        order_outgoing(f);
        if (f->gain_cur <= f->gain_prev2) {
            act.stop = f->cur;
        } else {
            act.stop = f->prev2;
            f->prev2 = f->cur;
            f->gain_prev2 = f->gain_cur;
            order_outgoing(f);
        }
    }
    f->cur = desired;
    f->gain_cur = 0.0f;
    act.play = desired;
    return act;
}

BgmFadeFinished bgm_fade_step_all(BgmFade *f, float dt)
{
    BgmFadeFinished out = {.stop = {BGM_NONE, BGM_NONE}, .count = 0};
    if (f == NULL || dt <= 0.0f) {
        return out;
    }
    const float delta = dt * k_fade_per_second;
    if (f->cur != BGM_NONE) {
        f->gain_cur = f->gain_cur + delta > 1.0f ? 1.0f : f->gain_cur + delta;
    }
    if (f->prev != BGM_NONE) {
        f->gain_prev = f->gain_prev - delta < 0.0f ? 0.0f : f->gain_prev - delta;
        if (f->gain_prev <= 0.0f) {
            out.stop[out.count++] = f->prev;
            f->prev = BGM_NONE;
            f->gain_prev = 0.0f;
        }
    }
    if (f->prev2 != BGM_NONE) {
        f->gain_prev2 = f->gain_prev2 - delta < 0.0f ? 0.0f : f->gain_prev2 - delta;
        if (f->gain_prev2 <= 0.0f) {
            out.stop[out.count++] = f->prev2;
            f->prev2 = BGM_NONE;
            f->gain_prev2 = 0.0f;
        }
    }
    order_outgoing(f);
    return out;
}

BgmTrack bgm_fade_step(BgmFade *f, float dt)
{
    const BgmFadeFinished fin = bgm_fade_step_all(f, dt);
    return fin.count > 0 ? fin.stop[0] : BGM_NONE;
}

float bgm_fade_step_duck(BgmFade *f, bool paused, float dt)
{
    if (f == NULL) {
        return paused ? k_pause_duck_min : 1.0f;
    }
    if (f->pause_duck <= 0.0f) {
        f->pause_duck = 1.0f;
    }
    const float target = paused ? k_pause_duck_min : 1.0f;
    if (dt > 0.0f) {
        const float step = dt * k_pause_duck_slew;
        if (f->pause_duck < target) {
            f->pause_duck = f->pause_duck + step > target ? target : f->pause_duck + step;
        } else if (f->pause_duck > target) {
            f->pause_duck = f->pause_duck - step < target ? target : f->pause_duck - step;
        }
    }
    return f->pause_duck;
}
