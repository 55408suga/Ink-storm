#ifndef INK_AUDIO_H
#define INK_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_SAMPLE_RATE 44100

typedef enum { BGM_NONE = 0, BGM_LOBBY, BGM_BATTLE, BGM_BOSS, BGM_VICTORY } BgmTrack;

typedef struct {
    uint64_t samples_rendered;
    uint64_t events_consumed;
    uint64_t rng;
    uint8_t master_vol;
    uint8_t bgm_vol;
    uint8_t se_vol;
    BgmTrack bgm;
} AudioState;

struct GameState;

void audio_init(AudioState *audio, uint64_t seed);
void audio_set_volumes(AudioState *audio, uint8_t master, uint8_t bgm, uint8_t se);
void audio_consume_events(AudioState *audio, const struct GameState *game);
void audio_render(AudioState *audio, int16_t *samples, size_t sample_count);
BgmTrack audio_bgm_track(const AudioState *audio);
bool audio_validate_finite(const AudioState *audio);

#endif
