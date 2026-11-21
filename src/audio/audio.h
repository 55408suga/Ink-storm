#ifndef INK_AUDIO_H
#define INK_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_SAMPLES_PER_TICK 735
#define AUDIO_MAX_VOICES 24
#define AUDIO_STATE_QUEUE_CAP 32

typedef enum { BGM_NONE = 0, BGM_LOBBY, BGM_BATTLE, BGM_BOSS, BGM_VICTORY } BgmTrack;

typedef enum {
    AUDIO_VOICE_NONE = 0,
    AUDIO_VOICE_KILL_TAIKO,
    AUDIO_VOICE_CHAIN_KOTO,
    AUDIO_VOICE_DEFLECT_BAMBOO,
    AUDIO_VOICE_SLAM_TAIKO,
    AUDIO_VOICE_IGNITE_FLAME,
    AUDIO_VOICE_STORM_ZAP,
    AUDIO_VOICE_CYCLONE_SWIRL,
    AUDIO_VOICE_GALE_SLASH,
    AUDIO_VOICE_ORB_DROP,
    AUDIO_VOICE_SHRINE_GONG,
    AUDIO_VOICE_SURGE_KOTO,
    AUDIO_VOICE_BOSS_TAIKO,
    AUDIO_VOICE_WAVE_CLEAR,
    AUDIO_VOICE_ACT_CLEAR,
    AUDIO_VOICE_REWARD_PICK,
    AUDIO_VOICE_REWARD_REROLL
} AudioVoiceKind;

typedef struct {
    AudioVoiceKind kind;
    bool active;
    uint64_t start_sample;
    uint64_t birth_seq;
    float env;
    float phase;
    float phase2;
    float freq;
    float param;
    float lp;
    float bp;
    uint32_t age;
} AudioVoice;

typedef struct {
    uint64_t at_sample;
    BgmTrack bgm;
    float brush_target;
    float wind_target;
} AudioTickState;

typedef struct {
    uint64_t samples_rendered;
    uint64_t consumed_ticks;
    uint64_t events_consumed;
    uint64_t voice_seq;
    uint64_t rng;
    uint8_t master_vol;
    uint8_t bgm_vol;
    uint8_t se_vol;

    BgmTrack bgm;
    BgmTrack active_bgm;
    BgmTrack prev_bgm;
    float xfade_t;
    bool has_consumed;

    /* Continuous brush scrape & ambient wind */
    float brush_target;
    float brush_env;
    float brush_lp;
    float brush_bp;
    float wind_target;
    float wind_env;
    float wind_lp;
    float wind_bp;

    /* Procedural BGM oscillator / sequencer state */
    uint64_t bgm_sample;
    float bgm_drum_env;
    float bgm_drum_ph;
    float bgm_koto_env;
    float bgm_koto_ph;
    float bgm_koto_freq;
    float bgm_drone_ph;
    float bgm_victory_t;
    int32_t bgm_victory_step;

    AudioVoice voices[AUDIO_MAX_VOICES];
    AudioTickState state_q[AUDIO_STATE_QUEUE_CAP];
    int32_t state_q_head;
    int32_t state_q_count;
} AudioState;

struct GameState;

void audio_init(AudioState *audio, uint64_t seed);
void audio_set_volumes(AudioState *audio, uint8_t master, uint8_t bgm, uint8_t se);
void audio_consume_events(AudioState *audio, const struct GameState *game);
void audio_render(AudioState *audio, int16_t *samples, size_t sample_count);
BgmTrack audio_bgm_track(const AudioState *audio);
bool audio_validate_finite(const AudioState *audio);

#endif
