#ifndef INK_APP_BGM_FADE_H
#define INK_APP_BGM_FADE_H

#include "audio.h"

#include <stdbool.h>

typedef struct {
    BgmTrack cur;
    BgmTrack prev;
    BgmTrack prev2;
    float gain_cur;
    float gain_prev;
    float gain_prev2;
    float pause_duck;
} BgmFade;

typedef struct {
    BgmTrack stop;
    BgmTrack play;
} BgmFadeActions;

typedef struct {
    BgmTrack stop[2];
    int count;
} BgmFadeFinished;

void bgm_fade_reset(BgmFade *f);
BgmFadeActions bgm_fade_request(BgmFade *f, BgmTrack desired);
BgmTrack bgm_fade_step(BgmFade *f, float dt);
BgmFadeFinished bgm_fade_step_all(BgmFade *f, float dt);
float bgm_fade_step_duck(BgmFade *f, bool paused, float dt);

#endif
