#ifndef INK_AUDIO_DSP_H
#define INK_AUDIO_DSP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Trapezoidal (TPT) state-variable filter; cutoff and Q are clamped so per-sample modulation
 * stays stable at any setting. */
typedef struct {
    float ic1eq;
    float ic2eq;
} AudioSvf;

typedef struct {
    float lp;
    float bp;
    float hp;
} AudioSvfOut;

/* Paul Kellet's refined pink-noise filter (-3 dB/oct). */
typedef struct {
    float b[7];
} AudioPink;

/* Two-pole smoothed noise: a slow random drift with roughly unit spread. */
typedef struct {
    float y1;
    float y2;
} AudioWalk;

/* Two Hann-windowed read heads half a grain apart over a window of a borrowed PCM loop. Each head
 * jumps to a fresh random offset only while its window is at zero, so the loop never clicks. */
typedef struct {
    uint32_t start;
    uint32_t span;
    float grain_len;
    float ph[2];
    float base[2];
} AudioGrainLoop;

typedef struct {
    float k;
    float a1;
    float a2;
    float a3;
} AudioSvfCoeffs;

AudioSvfCoeffs audio_svf_make_coeffs(float cutoff_hz, float q);
AudioSvfOut audio_svf_step_coeffs(AudioSvf *f, float in, AudioSvfCoeffs c);
AudioSvfOut audio_svf_step(AudioSvf *f, float in, float cutoff_hz, float q);
float audio_pink_step(AudioPink *p, float white);
float audio_walk_step(AudioWalk *w, float white, float hz);
void audio_grain_setup(AudioGrainLoop *g, size_t pcm_len, float start_frac, float end_frac,
                       float grain_samples);
float audio_grain_step(AudioGrainLoop *g, const int16_t *pcm, size_t pcm_len, float rate,
                       uint64_t *rng);
/* One-shot read in [-1, 1]: silent outside the buffer, faded over its last `fade` samples so a
 * read that runs off the end never clicks. */
float audio_pcm_read(const int16_t *pcm, size_t pcm_len, float pos, float fade);
float audio_smoothstep(float edge0, float edge1, float x);

bool audio_svf_finite(const AudioSvf *f);
bool audio_pink_finite(const AudioPink *p);
bool audio_walk_finite(const AudioWalk *w);
bool audio_grain_finite(const AudioGrainLoop *g);

#endif
