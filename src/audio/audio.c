#include "audio.h"

#include "game.h"

#include <string.h>

static uint8_t clamp_vol(uint8_t v) { return v > 100 ? 100 : v; }

void audio_init(AudioState *audio, uint64_t seed)
{
    if (audio == NULL) {
        return;
    }
    memset(audio, 0, sizeof *audio);
    audio->rng = seed;
    audio->master_vol = 80;
    audio->bgm_vol = 70;
    audio->se_vol = 85;
    audio->bgm = BGM_NONE;
}

void audio_set_volumes(AudioState *audio, uint8_t master, uint8_t bgm, uint8_t se)
{
    if (audio == NULL) {
        return;
    }
    audio->master_vol = clamp_vol(master);
    audio->bgm_vol = clamp_vol(bgm);
    audio->se_vol = clamp_vol(se);
}

void audio_consume_events(AudioState *audio, const struct GameState *game)
{
    if (audio == NULL || game == NULL) {
        return;
    }
    audio->events_consumed += (uint64_t)(game->event_count > 0 ? game->event_count : 0);
}

void audio_render(AudioState *audio, int16_t *samples, size_t sample_count)
{
    if (samples != NULL && sample_count > 0) {
        memset(samples, 0, sample_count * sizeof samples[0]);
    }
    if (audio != NULL) {
        audio->samples_rendered += sample_count;
    }
}

BgmTrack audio_bgm_track(const AudioState *audio) { return audio != NULL ? audio->bgm : BGM_NONE; }

bool audio_validate_finite(const AudioState *audio) { return audio != NULL; }
