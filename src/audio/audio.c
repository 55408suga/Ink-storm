#include "audio.h"

void audio_init(AudioState *audio) { *audio = (AudioState){0}; }

void audio_render_silence(AudioState *audio, int16_t *samples, size_t sample_count)
{
    for (size_t i = 0; i < sample_count; ++i) {
        samples[i] = 0;
    }
    audio->samples_rendered += sample_count;
}
