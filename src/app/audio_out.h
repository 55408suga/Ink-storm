#ifndef INK_APP_AUDIO_OUT_H
#define INK_APP_AUDIO_OUT_H

#include "audio.h"
#include "bgm_fade.h"
#include "raylib.h"

#include <stdbool.h>
#include <stdint.h>

/* raylib's audio thread pulls the SE stream through a callback, so this only sizes the buffer
 * the stream is created with; music keeps the larger size it needs on the game thread. */
#define AUDIO_STREAM_FRAMES 512
#define AUDIO_MUSIC_STREAM_FRAMES 4096
#define AUDIO_BGM_TRACK_SLOTS 5

typedef struct {
    AudioStream stream;
    bool ready;

    Music bgm[AUDIO_BGM_TRACK_SLOTS];
    bool bgm_loaded[AUDIO_BGM_TRACK_SLOTS];
    BgmFade fade;

    Wave brush_flow;
    Wave brush_swish;
    bool brush_flow_loaded;
    bool brush_swish_loaded;
    AudioState *attached_audio;
} AudioOut;

/* Call during single-threaded startup before graphics or audio libraries initialize to request
 * low-latency PulseAudio buffering on WSLg without overriding a user-provided value. */
void audio_out_configure_env(void);
/* `audio` borrows the decoded brush loops, so it must outlive `out` until audio_out_close(),
 * which detaches them from it before unloading. */
void audio_out_init(AudioOut *out, AudioState *audio);
void audio_out_pump(AudioOut *out, AudioState *audio);
void audio_out_close(AudioOut *out);

/* Between init and close the audio thread renders `audio`, so every other audio_* call on it
 * must run inside this lock. Never hold it across a raylib call: raylib holds its own mixer lock
 * while it runs the render callback, and the two locks would deadlock. */
void audio_out_lock(void);
void audio_out_unlock(void);

#endif
