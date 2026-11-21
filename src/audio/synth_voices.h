#ifndef INK_AUDIO_SYNTH_VOICES_H
#define INK_AUDIO_SYNTH_VOICES_H

#include "audio.h"

#include <stdbool.h>
#include <stdint.h>

extern const float k_yo_scale[8];

float audio_noise_next(AudioState *audio);
void audio_voice_spawn(AudioState *audio, AudioVoiceKind kind, uint64_t start_sample, float freq,
                       float param);
float audio_voices_step(AudioState *audio, uint64_t sample_idx, float nz);
float audio_bgm_step(AudioState *audio, float nz);
void audio_bgm_apply_track(AudioState *audio, BgmTrack next);

#endif
