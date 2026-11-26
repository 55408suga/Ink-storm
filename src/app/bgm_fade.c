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
                       .gain_cur = 0.0f,
                       .gain_prev = 0.0f,
                       .pause_duck = 1.0f};
    }
}

/* Each track keeps its own gain. A track that stops being current carries its present gain into
 * the fade-out, so interrupting a fade (A -> none -> B, or A -> B -> A) never makes the old track
 * jump back to full volume or restart from the top. */
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
        return act;
    }
    if (f->cur == BGM_NONE) {
        f->cur = desired;
        f->gain_cur = 0.0f;
        act.play = desired;
        return act;
    }
    if (f->prev != BGM_NONE) {
        act.stop = f->prev;
    }
    f->prev = f->cur;
    f->gain_prev = f->gain_cur;
    f->cur = desired;
    f->gain_cur = 0.0f;
    act.play = desired;
    return act;
}

BgmTrack bgm_fade_step(BgmFade *f, float dt)
{
    if (f == NULL || dt <= 0.0f) {
        return BGM_NONE;
    }
    const float delta = dt * k_fade_per_second;
    if (f->cur != BGM_NONE) {
        f->gain_cur = f->gain_cur + delta > 1.0f ? 1.0f : f->gain_cur + delta;
    }
    if (f->prev == BGM_NONE) {
        return BGM_NONE;
    }
    f->gain_prev = f->gain_prev - delta < 0.0f ? 0.0f : f->gain_prev - delta;
    if (f->gain_prev > 0.0f) {
        return BGM_NONE;
    }
    const BgmTrack finished = f->prev;
    f->prev = BGM_NONE;
    return finished;
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
