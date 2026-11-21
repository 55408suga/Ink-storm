#ifndef INK_APP_AUDIO_OUT_H
#define INK_APP_AUDIO_OUT_H

#include "audio.h"
#include "raylib.h"

#include <stdbool.h>
#include <stdint.h>

#define AUDIO_STREAM_FRAMES 1024

typedef struct {
    AudioStream stream;
    bool ready;
    int16_t pcm[AUDIO_STREAM_FRAMES];
} AudioOut;

void audio_out_init(AudioOut *out);
void audio_out_pump(AudioOut *out, AudioState *audio);
void audio_out_close(AudioOut *out);

#endif
