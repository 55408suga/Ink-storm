#include "audio_cb.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum { AUDIO_CB_CHUNK_FRAMES = 512 };

/* raylib's AudioCallback carries no user pointer, so the callback reaches the state it renders
 * through this module-level bridge. */
static pthread_mutex_t g_audio_cb_mtx = PTHREAD_MUTEX_INITIALIZER;
static AudioState *g_audio_cb_state = NULL;

void audio_cb_lock(void) { (void)pthread_mutex_lock(&g_audio_cb_mtx); }

void audio_cb_unlock(void) { (void)pthread_mutex_unlock(&g_audio_cb_mtx); }

void audio_cb_attach_locked(AudioState *audio) { g_audio_cb_state = audio; }

void audio_cb_detach(void)
{
    audio_cb_lock();
    g_audio_cb_state = NULL;
    audio_cb_unlock();
}

void audio_cb_render(void *buffer_data, unsigned int frames)
{
    if (buffer_data == NULL || frames == 0u) {
        return;
    }
    /* raylib's mixer hands in a byte buffer whose alignment and declared type are uint8_t, so
     * render into typed PCM storage and copy the bytes in bounded chunks. */
    unsigned char *dst = (unsigned char *)buffer_data;
    int16_t scratch[AUDIO_CB_CHUNK_FRAMES];
    size_t remaining = (size_t)frames;

    audio_cb_lock();
    while (remaining > 0u) {
        const size_t chunk =
            remaining < (size_t)AUDIO_CB_CHUNK_FRAMES ? remaining : (size_t)AUDIO_CB_CHUNK_FRAMES;
        if (g_audio_cb_state != NULL) {
            audio_render(g_audio_cb_state, scratch, chunk);
            memcpy(dst, scratch, chunk * sizeof(int16_t));
        } else {
            memset(dst, 0, chunk * sizeof(int16_t));
        }
        dst += chunk * sizeof(int16_t);
        remaining -= chunk;
    }
    audio_cb_unlock();
}
