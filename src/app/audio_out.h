#ifndef INK_APP_AUDIO_OUT_H
#define INK_APP_AUDIO_OUT_H

#include "audio.h"
#include "bgm_fade.h"
#include "raylib.h"

#include <stdbool.h>
#include <stdint.h>

/* raylib treats this as a minimum and pads each stream half with silence when the device period
 * is longer. Its public API hides the actual half size, and filling the stream from raylib's
 * audio-thread callback instead would race the game thread over AudioState, so the pump renders
 * this fixed size on the main thread. Every frame of a half delays each brush and skill sound, so
 * frame jitter is absorbed by refilling several halves per pump rather than by a larger half. */
#define AUDIO_STREAM_FRAMES 1024
#define AUDIO_MUSIC_STREAM_FRAMES 4096
#define AUDIO_BGM_TRACK_SLOTS 5

typedef struct {
    AudioStream stream;
    bool ready;
    int16_t pcm[AUDIO_STREAM_FRAMES];

    Music bgm[AUDIO_BGM_TRACK_SLOTS];
    bool bgm_loaded[AUDIO_BGM_TRACK_SLOTS];
    BgmFade fade;

    Wave brush_flow;
    Wave brush_swish;
    bool brush_flow_loaded;
    bool brush_swish_loaded;
    AudioState *attached_audio;
} AudioOut;

/* `audio` borrows the decoded brush loops, so it must outlive `out` until audio_out_close(),
 * which detaches them from it before unloading. */
void audio_out_init(AudioOut *out, AudioState *audio);
void audio_out_pump(AudioOut *out, AudioState *audio);
void audio_out_close(AudioOut *out);

#endif
