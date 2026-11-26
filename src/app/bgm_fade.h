#ifndef INK_APP_BGM_FADE_H
#define INK_APP_BGM_FADE_H

#include "audio.h"

#include <stdbool.h>

typedef struct {
    BgmTrack cur;
    BgmTrack prev;
    float gain_cur;
    float gain_prev;
    float pause_duck;
} BgmFade;

typedef struct {
    BgmTrack stop;
    BgmTrack play;
} BgmFadeActions;

void bgm_fade_reset(BgmFade *f);
BgmFadeActions bgm_fade_request(BgmFade *f, BgmTrack desired);
BgmTrack bgm_fade_step(BgmFade *f, float dt);
float bgm_fade_step_duck(BgmFade *f, bool paused, float dt);

#endif
