#ifndef INK_APP_AUDIO_OUT_H
#define INK_APP_AUDIO_OUT_H

#include "audio.h"
#include "raylib.h"

#include <stdbool.h>
#include <stdint.h>

#define AUDIO_STREAM_FRAMES 1024
#define AUDIO_BGM_TRACK_SLOTS 5

typedef struct {
    AudioStream stream;
    bool ready;
    int16_t pcm[AUDIO_STREAM_FRAMES];

    Music bgm[AUDIO_BGM_TRACK_SLOTS];
    bool bgm_loaded[AUDIO_BGM_TRACK_SLOTS];
    BgmTrack current_ext_track;
    BgmTrack prev_ext_track;
    float ext_xfade;
} AudioOut;

void audio_out_init(AudioOut *out);
void audio_out_pump(AudioOut *out, AudioState *audio);
void audio_out_close(AudioOut *out);

#endif
