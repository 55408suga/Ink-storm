#ifndef INK_AUDIO_H
#define INK_AUDIO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t samples_rendered;
} AudioState;

void audio_init(AudioState *audio);
void audio_render_silence(AudioState *audio, int16_t *samples, size_t sample_count);

#endif
