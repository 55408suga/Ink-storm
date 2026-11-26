#ifndef INK_AUDIO_H
#define INK_AUDIO_H

#include "audio_dsp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_SAMPLES_PER_TICK 735
#define AUDIO_MAX_VOICES 24
#define AUDIO_STATE_QUEUE_CAP 32
#define AUDIO_PENDING_VOICE_CAP 256
#define AUDIO_SKILL_PARAM_DRY 4
#define AUDIO_SKILL_PARAM_SURGE 8
#define AUDIO_BRUSH_DELAY_LEN 1024
#define AUDIO_BRUSH_SVF_COUNT 13
#define AUDIO_BRUSH_WALK_COUNT 6
#define AUDIO_VOICE_SVF_COUNT 5
#define AUDIO_EXTERNAL_SE_CEILING 0.52f
#define AUDIO_EXTERNAL_BGM_GAIN 0.45f

typedef enum { BGM_NONE = 0, BGM_LOBBY, BGM_BATTLE, BGM_BOSS, BGM_VICTORY } BgmTrack;

typedef enum {
    AUDIO_VOICE_NONE = 0,
    AUDIO_VOICE_KILL_TAIKO,
    AUDIO_VOICE_CHAIN_KOTO,
    AUDIO_VOICE_DEFLECT_BAMBOO,
    AUDIO_VOICE_SLAM_TAIKO,
    AUDIO_VOICE_IGNITE_FLAME,
    AUDIO_VOICE_STORM_ZAP,
    AUDIO_VOICE_THUNDERBURST,
    AUDIO_VOICE_CATACLYSM,
    AUDIO_VOICE_WIND_HOWL,
    AUDIO_VOICE_CYCLONE_SWIRL,
    AUDIO_VOICE_GALE_SLASH,
    AUDIO_VOICE_ORB_DROP,
    AUDIO_VOICE_SHRINE_GONG,
    AUDIO_VOICE_SURGE_KOTO,
    AUDIO_VOICE_BOSS_TAIKO,
    AUDIO_VOICE_WAVE_CLEAR,
    AUDIO_VOICE_ACT_CLEAR,
    AUDIO_VOICE_REWARD_PICK,
    AUDIO_VOICE_REWARD_REROLL,
    AUDIO_VOICE_PART_BREAK,
    AUDIO_VOICE_SKILL_DENIED,
    AUDIO_VOICE_GAME_OVER,
    AUDIO_VOICE_UI_CLICK,
    AUDIO_VOICE_WARD_BELL,
    AUDIO_VOICE_PURIFY
} AudioVoiceKind;

typedef enum { AUDIO_UI_CLICK = 0, AUDIO_UI_BACK, AUDIO_UI_DENY, AUDIO_UI_PREVIEW } AudioUiCue;

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
    AudioSvf svf[AUDIO_VOICE_SVF_COUNT];
    AudioPink pink;
    float sample_pos;
    uint32_t age;
    bool releasing;
    bool ui;
} AudioVoice;

typedef struct {
    AudioVoiceKind kind;
    uint64_t start_sample;
    float freq;
    float param;
} AudioPendingVoice;

typedef struct {
    uint64_t at_sample;
    BgmTrack bgm;
    float brush_target;
    float brush_speed;
    bool brush_active;
    float wind_target;
    int32_t brush_style;
    bool brush_dry;
    bool brush_surge;
    bool lobby;
} AudioTickState;

typedef struct {
    uint64_t sample;
    float drum_env;
    float drum_ph;
    float koto_env;
    float koto_ph;
    float koto_freq;
    float drone_ph;
    float victory_t;
    int32_t victory_step;
} AudioBgmSynth;

typedef struct {
    uint64_t samples_rendered;
    uint64_t next_tick_sample;
    uint64_t last_state_sample;
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
    bool lobby_mode;
    bool external_bgm;
    uint32_t external_bgm_mask;
    bool paused;
    bool realtime_mode;

    /* Continuous brush stroke */
    float brush_target;
    float brush_env;
    float brush_speed;
    float brush_speed_smooth;
    float brush_hold_speed;
    float brush_surge_env;
    uint32_t brush_hold;
    bool brush_active;
    bool brush_fast_release;
    bool brush_dsp_live;
    int32_t brush_style;
    bool brush_dry;
    bool brush_surge;
    uint64_t brush_rng;
    uint32_t brush_delay_pos;
    AudioPink brush_pink[2];
    AudioWalk brush_walk[AUDIO_BRUSH_WALK_COUNT];
    AudioSvf brush_svf[AUDIO_BRUSH_SVF_COUNT];
    AudioGrainLoop brush_flow_grain;
    AudioGrainLoop brush_swish_grain;
    float brush_delay[2][AUDIO_BRUSH_DELAY_LEN];
    /* Borrowed 16-bit mono loops at AUDIO_SAMPLE_RATE; NULL selects the procedural fallback. */
    const int16_t *brush_flow_pcm;
    size_t brush_flow_len;
    const int16_t *brush_swish_pcm;
    size_t brush_swish_len;
    /* The grains keep the whole swish for texture; only the one-shot skips its quiet lead-in. */
    size_t brush_swish_onset;

    /* Ambient wind */
    float bgm_duck;
    float wind_target;
    float wind_env;
    float wind_lp;
    float wind_bp;

    /* Procedural BGM oscillator / sequencer decks (active + outgoing crossfade) */
    AudioBgmSynth bgm_cur;
    AudioBgmSynth bgm_prev;

    /* Reaction retrigger limits on the sample clock, so batched and per-tick rendering agree */
    uint64_t thunderburst_next_sample;
    uint64_t thunderburst_chain_until;
    uint64_t storm_zap_next_sample;
    uint8_t thunderburst_chain;

    AudioVoice voices[AUDIO_MAX_VOICES];
    AudioPendingVoice pending[AUDIO_PENDING_VOICE_CAP];
    int32_t pending_head;
    int32_t pending_count;
    AudioTickState state_q[AUDIO_STATE_QUEUE_CAP];
    int32_t state_q_head;
    int32_t state_q_count;
} AudioState;

struct GameState;

void audio_init(AudioState *audio, uint64_t seed);
void audio_set_volumes(AudioState *audio, uint8_t master, uint8_t bgm, uint8_t se);
void audio_set_lobby_mode(AudioState *audio, bool lobby);
void audio_set_external_bgm(AudioState *audio, bool external_active);
/* Bitmask of loaded external BgmTrack slots (1u << track) so each crossfade deck independently
 * suppresses procedural fallback on externally backed tracks while letting outgoing or incoming
 * procedural fallback tracks crossfade smoothly. */
void audio_set_external_bgm_mask(AudioState *audio, uint32_t track_mask);
void audio_set_paused(AudioState *audio, bool paused);
/* Off by default so offline and batched rendering keep their full sub-tick schedule. */
void audio_set_realtime_mode(AudioState *audio, bool realtime);
/* The caller keeps both buffers alive until it clears them with (NULL, 0, NULL, 0). */
void audio_set_brush_samples(AudioState *audio, const int16_t *flow_pcm, size_t flow_len,
                             const int16_t *swish_pcm, size_t swish_len);
void audio_trigger_ui_stamp(AudioState *audio);
void audio_trigger_ui_cue(AudioState *audio, AudioUiCue cue);
void audio_consume_events(AudioState *audio, const struct GameState *game);
void audio_consume_lobby_events(AudioState *audio, const struct GameState *game);
void audio_render(AudioState *audio, int16_t *samples, size_t sample_count);
BgmTrack audio_bgm_track(const AudioState *audio);
bool audio_validate_finite(const AudioState *audio);

#endif
