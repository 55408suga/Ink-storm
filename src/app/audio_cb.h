#ifndef INK_APP_AUDIO_CB_H
#define INK_APP_AUDIO_CB_H

#include "audio.h"

/* Raylib-free mutex bridge shared by the audio-thread callback and headless concurrency tests.
 * Never hold this lock across a raylib audio call (raylib holds its own mixer lock during the
 * callback, so inverting the order would deadlock). */
void audio_cb_lock(void);
void audio_cb_unlock(void);

/* Attach while already holding audio_cb_lock(); detach acquires the lock internally so any
 * in-flight callback finishes before the caller unregisters the stream or frees PCM buffers. */
void audio_cb_attach_locked(AudioState *audio);
void audio_cb_detach(void);

/* Matches raylib's AudioCallback signature: renders `frames` mono int16 samples into byte storage
 * via typed scratch memory and outputs silence when detached. */
void audio_cb_render(void *buffer_data, unsigned int frames);

#endif
