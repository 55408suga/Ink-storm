#include "audio.h"

#include "core.h"
#include "game.h"
#include "synth_voices.h"

#include <math.h>
#include <string.h>

static uint8_t clamp_vol(uint8_t v) { return v > 100 ? 100 : v; }

static BgmTrack desired_bgm_for_game(const GameState *g)
{
    switch (g->phase) {
    case RUN_PHASE_ACT_TRANSITION:
    case RUN_PHASE_REWARD:
        return BGM_BATTLE;
    case RUN_PHASE_PLAYING:
    case RUN_PHASE_WAVE_AFTERMATH:
        return g->run.wave_index == GAME_BOSS_WAVE_INDEX ? BGM_BOSS : BGM_BATTLE;
    case RUN_PHASE_VICTORY:
        return BGM_VICTORY;
    case RUN_PHASE_SHRINE_BREAK:
    case RUN_PHASE_GAME_OVER:
    default:
        return BGM_NONE;
    }
}

void audio_init(AudioState *audio, uint64_t seed)
{
    if (audio == NULL) {
        return;
    }
    memset(audio, 0, sizeof *audio);
    audio->rng = seed ^ UINT64_C(0x51a79d3319b4c2d1);
    if (audio->rng == 0) {
        audio->rng = 1;
    }
    audio->master_vol = 80;
    audio->bgm_vol = 70;
    audio->se_vol = 85;
    audio->bgm = BGM_NONE;
    audio->active_bgm = BGM_NONE;
    audio->prev_bgm = BGM_NONE;
    audio->xfade_t = 1.0f;
    audio->bgm_duck = 1.0f;
    audio->brush_rng = seed ^ UINT64_C(0x8f1bbcdcca62c1d6);
    audio_brush_reset_dsp(audio);
}

/* Scene changes drop queued combat state and fade (rather than cut) voices still ringing, so a
 * run's last gong or scrape never bleeds into the Lobby or into the next run. */
static void reset_session_audio(AudioState *audio)
{
    audio->state_q_head = 0;
    audio->state_q_count = 0;
    audio->pending_head = 0;
    audio->pending_count = 0;
    audio->next_tick_sample = audio->samples_rendered;
    audio->brush_target = 0.0f;
    audio->brush_active = false;
    audio->brush_speed = 0.0f;
    audio->brush_hold = 0u;
    audio->brush_fast_release = true;
    audio->thunderburst_next_sample = 0;
    audio->thunderburst_chain_until = 0;
    audio->thunderburst_chain = 0;
    audio->storm_zap_next_sample = 0;
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        AudioVoice *v = &audio->voices[i];
        if (!v->active || v->ui) {
            continue;
        }
        if (v->age == 0u) {
            v->active = false;
        } else {
            v->releasing = true;
        }
    }
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

void audio_set_lobby_mode(AudioState *audio, bool lobby)
{
    if (audio == NULL) {
        return;
    }
    reset_session_audio(audio);
    audio->lobby_mode = lobby;
    if (lobby) {
        audio->bgm = BGM_LOBBY;
        audio->has_consumed = true;
        audio->brush_target = 0.0f;
        audio->wind_target = 0.14f;
        audio_bgm_apply_track(audio, BGM_LOBBY);
    }
}

void audio_set_external_bgm(AudioState *audio, bool external_active)
{
    if (audio == NULL) {
        return;
    }
    audio->external_bgm = external_active;
}

void audio_set_external_bgm_mask(AudioState *audio, uint32_t track_mask)
{
    if (audio == NULL) {
        return;
    }
    audio->external_bgm_mask = track_mask;
}

void audio_set_realtime_mode(AudioState *audio, bool realtime)
{
    if (audio == NULL) {
        return;
    }
    audio->realtime_mode = realtime;
    audio->last_state_sample = audio->samples_rendered;
}

void audio_set_paused(AudioState *audio, bool paused)
{
    if (audio == NULL) {
        return;
    }
    audio->paused = paused;
    if (!paused) {
        return;
    }
    audio->brush_target = 0.0f;
    audio->brush_active = false;
    audio->brush_hold = 0u;
    /* A resume can land before the stream drains these snapshots; left intact, they would replay
     * the pre-pause stroke and its hold would then keep it alive. */
    for (int32_t i = 0; i < audio->state_q_count; ++i) {
        AudioTickState *st = &audio->state_q[(audio->state_q_head + i) % AUDIO_STATE_QUEUE_CAP];
        st->brush_active = false;
        st->brush_target = 0.0f;
        st->brush_speed = 0.0f;
    }
}

/* Encoded loops can carry digital silence (e.g. padding to a round length); grains or one-shot
 * reads landing there would drop the brush out, so only the audible span is kept. A span too
 * short to hold a grain counts as no loop at all. */
static bool near_silent(int16_t s) { return s >= -16 && s <= 16; }

static size_t first_audible(const int16_t *pcm, size_t len)
{
    for (size_t i = 0u; i < len; ++i) {
        if (!near_silent(pcm[i])) {
            return i;
        }
    }
    return len;
}

static size_t end_of_audible(const int16_t *pcm, size_t len)
{
    for (size_t i = len; i > 0u; --i) {
        if (!near_silent(pcm[i - 1u])) {
            return i;
        }
    }
    return 0u;
}

static void audible_span(const int16_t *pcm, size_t len, const int16_t **out_pcm, size_t *out_len)
{
    enum { MIN_SPAN = 1024 };
    *out_pcm = NULL;
    *out_len = 0u;
    if (pcm == NULL) {
        return;
    }
    const size_t begin = first_audible(pcm, len);
    const size_t end = end_of_audible(pcm, len);
    if (end > begin && end - begin >= (size_t)MIN_SPAN) {
        *out_pcm = pcm + begin;
        *out_len = end - begin;
    }
}

static int32_t abs_sample(int16_t s) { return s < 0 ? -(int32_t)s : (int32_t)s; }

static int64_t subblock_sum(const int16_t *pcm, size_t block_idx, size_t sub_len)
{
    int64_t sum = 0;
    const size_t base = block_idx * sub_len;
    for (size_t i = 0u; i < sub_len; ++i) {
        sum += (int64_t)abs_sample(pcm[base + i]);
    }
    return sum;
}

static int64_t sustained_block_level(const int16_t *pcm, size_t block_idx, size_t sub_len,
                                     size_t run_blocks)
{
    int64_t lo = subblock_sum(pcm, block_idx, sub_len);
    for (size_t k = 1u; k < run_blocks; ++k) {
        const int64_t v = subblock_sum(pcm, block_idx + k, sub_len);
        if (v < lo) {
            lo = v;
        }
    }
    return lo;
}

/* Find the first sustained rise within the clip's opening by taking the minimum energy across four
 * consecutive 16-sample sub-blocks (1.45 ms). An isolated spike of any amplitude hits at most one
 * sub-block so its minimum stays near zero, while a bounded prefix ignores louder late tails. */
static size_t attack_start(const int16_t *pcm, size_t len)
{
    enum {
        SUB = 16,
        RUN = 4,
        WIN = SUB * RUN,
        PRE_ROLL = 132,
        MIN_REMAINING = 1024,
        MAX_PREFIX = AUDIO_SAMPLE_RATE * 150 / 1000
    };
    if (pcm == NULL || len <= (size_t)(MIN_REMAINING + WIN)) {
        return 0u;
    }
    size_t search_end = len - (size_t)MIN_REMAINING;
    if (search_end > len / 2u) {
        search_end = len / 2u;
    }
    if (search_end > (size_t)MAX_PREFIX) {
        search_end = (size_t)MAX_PREFIX;
    }
    const size_t n_blocks = search_end / (size_t)SUB;
    if (n_blocks < (size_t)RUN) {
        return 0u;
    }

    const size_t n_runs = n_blocks - (size_t)RUN + 1u;
    int64_t peak_sus = 0;
    for (size_t b = 0u; b < n_runs; ++b) {
        const int64_t sus = sustained_block_level(pcm, b, (size_t)SUB, (size_t)RUN);
        if (sus > peak_sus) {
            peak_sus = sus;
        }
    }
    if (peak_sus <= 0) {
        return 0u;
    }

    const int64_t threshold = peak_sus / 4;
    for (size_t b = 0u; b < n_runs; ++b) {
        if (sustained_block_level(pcm, b, (size_t)SUB, (size_t)RUN) >= threshold) {
            if (b == 0u) {
                return 0u;
            }
            const size_t cross = (b + (size_t)RUN) * (size_t)SUB;
            return cross > (size_t)PRE_ROLL ? cross - (size_t)PRE_ROLL : 0u;
        }
    }
    return 0u;
}

void audio_set_brush_samples(AudioState *audio, const int16_t *flow_pcm, size_t flow_len,
                             const int16_t *swish_pcm, size_t swish_len)
{
    if (audio == NULL) {
        return;
    }
    audible_span(flow_pcm, flow_len, &audio->brush_flow_pcm, &audio->brush_flow_len);
    audible_span(swish_pcm, swish_len, &audio->brush_swish_pcm, &audio->brush_swish_len);
    audio->brush_swish_onset = attack_start(audio->brush_swish_pcm, audio->brush_swish_len);
    audio_brush_reset_dsp(audio);
}

void audio_trigger_ui_stamp(AudioState *audio)
{
    if (audio == NULL) {
        return;
    }
    audio_voice_spawn_ui(audio, AUDIO_VOICE_REWARD_PICK, 110.0f, 0.0f);
}

void audio_trigger_ui_cue(AudioState *audio, AudioUiCue cue)
{
    if (audio == NULL) {
        return;
    }
    switch (cue) {
    case AUDIO_UI_DENY:
        audio_voice_spawn_ui(audio, AUDIO_VOICE_SKILL_DENIED, 430.0f, 0.0f);
        break;
    case AUDIO_UI_BACK:
        audio_voice_spawn_ui(audio, AUDIO_VOICE_UI_CLICK, 587.33f, 0.0f);
        break;
    case AUDIO_UI_PREVIEW:
        audio_voice_spawn_ui(audio, AUDIO_VOICE_UI_CLICK, 987.77f, 1.0f);
        break;
    case AUDIO_UI_CLICK:
    default:
        audio_voice_spawn_ui(audio, AUDIO_VOICE_UI_CLICK, 783.99f, 0.0f);
        break;
    }
}

static void enqueue_tick_state(AudioState *audio, AudioTickState st)
{
    if (audio->state_q_count >= AUDIO_STATE_QUEUE_CAP) {
        audio->state_q_head = (audio->state_q_head + 1) % AUDIO_STATE_QUEUE_CAP;
        audio->state_q_count--;
    }
    const int tail = (audio->state_q_head + audio->state_q_count) % AUDIO_STATE_QUEUE_CAP;
    audio->state_q[tail] = st;
    audio->state_q_count++;
}

/* Redistribute queued realtime ticks evenly across [samples_rendered, limit) so a multi-tick
 * catch-up frame stays within the live lead budget without collapsing earlier brush snapshots or
 * stacking pending voices onto the same sample. */
static void compress_realtime_queue(AudioState *audio, uint64_t limit)
{
    const uint64_t now = audio->samples_rendered;
    const uint64_t span = limit > now ? limit - now : 0u;
    const int32_t m = audio->state_q_count;
    if (m <= 0) {
        for (int32_t i = 0; i < audio->pending_count; ++i) {
            AudioPendingVoice *p =
                &audio->pending[(audio->pending_head + i) % AUDIO_PENDING_VOICE_CAP];
            if (p->start_sample > limit) {
                p->start_sample = limit;
            }
        }
        return;
    }

    uint64_t old_t[AUDIO_STATE_QUEUE_CAP];
    uint64_t new_t[AUDIO_STATE_QUEUE_CAP];
    for (int32_t i = 0; i < m; ++i) {
        const int32_t idx = (audio->state_q_head + i) % AUDIO_STATE_QUEUE_CAP;
        old_t[i] = audio->state_q[idx].at_sample;
        uint64_t target = now + ((uint64_t)i * span) / (uint64_t)m;
        if (target > old_t[i]) {
            target = old_t[i];
        }
        new_t[i] = target;
        audio->state_q[idx].at_sample = new_t[i];
    }
    for (int32_t i = 0; i < audio->pending_count; ++i) {
        AudioPendingVoice *p = &audio->pending[(audio->pending_head + i) % AUDIO_PENDING_VOICE_CAP];
        if (p->start_sample <= old_t[0]) {
            p->start_sample = new_t[0];
        } else if (p->start_sample >= old_t[m - 1]) {
            p->start_sample = new_t[m - 1];
        } else {
            int32_t slot = m - 1;
            for (int32_t k = 0; k < m; ++k) {
                if (old_t[k] >= p->start_sample) {
                    slot = k;
                    break;
                }
            }
            p->start_sample = new_t[slot];
        }
    }
}

/* Ticks are placed on a running cursor rather than consumed_ticks * 735: once the renderer has
 * run ahead (pause, scene change, slow frame) the fixed grid would pin every later tick to
 * samples_rendered and collapse multi-tick frames onto one sample. */
static uint64_t schedule_tick(AudioState *audio)
{
    audio->consumed_ticks++;
    uint64_t at_sample = audio->next_tick_sample >= audio->samples_rendered
                             ? audio->next_tick_sample
                             : audio->samples_rendered;
    if (audio->realtime_mode) {
        /* A live device clock drifts against the 60 Hz loop, and a 15-tick lead lets that drift
         * settle into a lasting quarter-second lag; cap lead relative to samples_rendered (not a
         * global grid) and compress overdue bursts cleanly into [samples_rendered, limit]. */
        const uint64_t max_lead = (uint64_t)AUDIO_SAMPLES_PER_TICK * 2u;
        const uint64_t limit = audio->samples_rendered + max_lead;
        if (at_sample > limit) {
            compress_realtime_queue(audio, limit);
            at_sample = limit;
        }
        audio->next_tick_sample = at_sample + (uint64_t)AUDIO_SAMPLES_PER_TICK;
        return at_sample;
    }

    const uint64_t max_lead = (uint64_t)AUDIO_SAMPLES_PER_TICK * 15u;
    const uint64_t limit =
        ((audio->samples_rendered + max_lead) / (uint64_t)AUDIO_SAMPLES_PER_TICK) *
        (uint64_t)AUDIO_SAMPLES_PER_TICK;
    if (at_sample > limit && limit >= audio->samples_rendered) {
        const uint64_t excess = at_sample - limit;
        for (int32_t i = 0; i < audio->state_q_count; ++i) {
            const int32_t idx = (audio->state_q_head + i) % AUDIO_STATE_QUEUE_CAP;
            if (audio->state_q[idx].at_sample > audio->samples_rendered) {
                const uint64_t delta = audio->state_q[idx].at_sample - audio->samples_rendered;
                audio->state_q[idx].at_sample -= delta > excess ? excess : delta;
            }
        }
        for (int32_t i = 0; i < audio->pending_count; ++i) {
            AudioPendingVoice *p =
                &audio->pending[(audio->pending_head + i) % AUDIO_PENDING_VOICE_CAP];
            if (p->start_sample > audio->samples_rendered) {
                const uint64_t delta = p->start_sample - audio->samples_rendered;
                p->start_sample -= delta > excess ? excess : delta;
            }
        }
        at_sample = limit;
    }
    audio->next_tick_sample = at_sample + (uint64_t)AUDIO_SAMPLES_PER_TICK;
    return at_sample;
}

static AudioTickState brush_tick_state(const AudioState *audio, const GameState *game)
{
    const bool active = !audio->paused && game->player.painting;
    return (AudioTickState){
        .brush_target = active ? 1.0f : 0.0f,
        .brush_active = active,
        .brush_speed = active ? ink_clampf(game->player.stroke_speed / 900.0f, 0.0f, 1.0f) : 0.0f,
        .brush_style = (int32_t)game->config.brush_style,
        .brush_dry = game->player.dry,
        .brush_surge = game->player.surge_t > 0.0f,
    };
}

static void spawn_event_voices(AudioState *audio, const GameState *game, uint64_t at_sample)
{
    const int32_t count = game->event_count > GAME_MAX_EVENTS ? GAME_MAX_EVENTS : game->event_count;
    const int skill_bits = ((int)game->config.brush_style & 3) |
                           (game->player.dry ? AUDIO_SKILL_PARAM_DRY : 0) |
                           (game->player.surge_t > 0.0f ? AUDIO_SKILL_PARAM_SURGE : 0);
    const float brush_param = (float)skill_bits;
    bool auto_orb_played = false;
    bool purify_played = false;
    /* Same-tick reaction repeats are dropped here rather than left to the retrigger gap: in a
     * batched frame they would otherwise fill the pending queue ahead of later milestone cues. */
    bool thunderburst_played = false;
    bool storm_zap_played = false;

    for (int32_t i = 0; i < count; ++i) {
        const GameEvent *ev = &game->events[i];
        audio->events_consumed++;

        switch (ev->type) {
        case EV_KILL: {
            audio_voice_spawn(audio, AUDIO_VOICE_KILL_TAIKO, at_sample, 135.0f, 0.0f);
            if (game->run.chain >= 2) {
                int idx = game->run.chain - 2;
                if (idx > 7) {
                    idx = 7;
                }
                audio_voice_spawn(audio, AUDIO_VOICE_CHAIN_KOTO, at_sample, k_yo_scale[idx], 0.0f);
            }
            break;
        }
        case EV_DEFLECT:
            audio_voice_spawn(audio, AUDIO_VOICE_DEFLECT_BAMBOO, at_sample, 780.0f, 0.0f);
            break;
        case EV_SLAM:
            audio_voice_spawn(audio, AUDIO_VOICE_SLAM_TAIKO, at_sample, 76.0f, 0.0f);
            break;
        case EV_IGNITE:
            audio_voice_spawn(audio, AUDIO_VOICE_IGNITE_FLAME, at_sample, 120.0f, 0.0f);
            break;
        case EV_STORM_ZAP:
            if (!storm_zap_played) {
                storm_zap_played = true;
                audio_voice_spawn(audio, AUDIO_VOICE_STORM_ZAP, at_sample, 1320.0f, 0.0f);
            }
            break;
        case EV_THUNDERBURST:
            if (!thunderburst_played) {
                thunderburst_played = true;
                audio_voice_spawn(audio, AUDIO_VOICE_THUNDERBURST, at_sample, 62.0f, 0.0f);
            }
            break;
        case EV_CATACLYSM:
            audio_voice_spawn(audio, AUDIO_VOICE_CATACLYSM, at_sample, 54.0f, 0.0f);
            break;
        case EV_WIND_TELEGRAPH:
            audio_voice_spawn(audio, AUDIO_VOICE_WIND_HOWL, at_sample, 196.0f, 0.0f);
            break;
        case EV_CYCLONE:
            audio_voice_spawn(audio, AUDIO_VOICE_CYCLONE_SWIRL, at_sample, 145.0f, brush_param);
            break;
        case EV_GALE:
            audio_voice_spawn(audio, AUDIO_VOICE_GALE_SLASH, at_sample, 175.0f, brush_param);
            break;
        case EV_ORB_PICKUP:
            if ((ev->flags & EVF_ORB_AUTO) != 0) {
                if (auto_orb_played) {
                    break;
                }
                auto_orb_played = true;
            }
            audio_voice_spawn(audio, AUDIO_VOICE_ORB_DROP, at_sample, 640.0f, 0.0f);
            break;
        case EV_SHRINE_HIT:
            if ((ev->flags & EVF_SHRINE_BLOCKED) != 0) {
                audio_voice_spawn(audio, AUDIO_VOICE_WARD_BELL, at_sample, 1046.5f, 0.0f);
            } else {
                audio_voice_spawn(audio, AUDIO_VOICE_SHRINE_GONG, at_sample, 152.0f, 0.0f);
            }
            break;
        case EV_SURGE_START:
            audio_voice_spawn(audio, AUDIO_VOICE_SURGE_KOTO, at_sample, k_yo_scale[2], 0.0f);
            break;
        case EV_BOSS_ENTER:
            audio_voice_spawn(audio, AUDIO_VOICE_BOSS_TAIKO, at_sample, 68.0f, 0.0f);
            break;
        case EV_BOSS_PHASE:
            audio_voice_spawn(audio, AUDIO_VOICE_BOSS_TAIKO, at_sample, 68.0f, 0.0f);
            audio_voice_spawn(audio, AUDIO_VOICE_SHRINE_GONG, at_sample, 152.0f, 0.0f);
            break;
        case EV_BOSS_PART_BREAK:
            audio_voice_spawn(audio, AUDIO_VOICE_PART_BREAK, at_sample, 987.77f, (float)ev->value);
            break;
        case EV_WAVE_CLEAR:
            audio_voice_spawn(audio, AUDIO_VOICE_WAVE_CLEAR, at_sample, k_yo_scale[2], 0.0f);
            break;
        case EV_ACT_CLEAR:
            audio_voice_spawn(audio, AUDIO_VOICE_ACT_CLEAR, at_sample, 82.0f, 0.0f);
            break;
        case EV_REWARD_PICK:
            audio_voice_spawn(audio, AUDIO_VOICE_REWARD_PICK, at_sample, 110.0f, 0.0f);
            break;
        case EV_REWARD_REROLL:
            audio_voice_spawn(audio, AUDIO_VOICE_REWARD_REROLL, at_sample, 0.0f, 0.0f);
            break;
        case EV_SKILL_DENIED:
            audio_voice_spawn(audio, AUDIO_VOICE_SKILL_DENIED, at_sample, 520.0f, 0.0f);
            break;
        case EV_GAME_OVER:
            if ((ev->flags & EVF_RUN_ABORTED) == 0) {
                audio_voice_spawn(audio, AUDIO_VOICE_GAME_OVER, at_sample, 110.0f, 0.0f);
            }
            break;
        case EV_PURIFY:
            if (!purify_played) {
                purify_played = true;
                audio_voice_spawn(audio, AUDIO_VOICE_PURIFY, at_sample, 1318.5f, 0.0f);
            }
            break;
        default:
            break;
        }
    }
}

void audio_consume_events(AudioState *audio, const struct GameState *game)
{
    if (audio == NULL || game == NULL) {
        return;
    }
    const uint64_t at_sample = schedule_tick(audio);

    audio->lobby_mode = false;
    const BgmTrack next_bgm = desired_bgm_for_game(game);
    audio->bgm = next_bgm;

    float wind_target = 0.0f;
    if (game->phase == RUN_PHASE_GAME_OVER || game->phase == RUN_PHASE_SHRINE_BREAK) {
        wind_target = 0.35f;
    } else if (game->phase == RUN_PHASE_PLAYING || game->phase == RUN_PHASE_WAVE_AFTERMATH) {
        wind_target =
            ink_clampf(game->fluid.stats.max_vort * 0.035f + (next_bgm == BGM_BOSS ? 0.16f : 0.05f),
                       0.0f, 1.0f);
    } else if (game->phase == RUN_PHASE_VICTORY) {
        wind_target = 0.12f;
    }

    AudioTickState st = brush_tick_state(audio, game);
    st.at_sample = at_sample;
    st.bgm = next_bgm;
    st.wind_target = wind_target;
    st.lobby = false;
    enqueue_tick_state(audio, st);
    spawn_event_voices(audio, game, at_sample);
}

void audio_consume_lobby_events(AudioState *audio, const struct GameState *game)
{
    if (audio == NULL || game == NULL) {
        return;
    }
    const uint64_t at_sample = schedule_tick(audio);
    AudioTickState st = brush_tick_state(audio, game);
    st.at_sample = at_sample;
    st.bgm = BGM_LOBBY;
    st.wind_target = 0.14f;
    st.lobby = true;
    enqueue_tick_state(audio, st);
    spawn_event_voices(audio, game, at_sample);
}

static void drain_state_queue(AudioState *audio, uint64_t sample_idx)
{
    while (audio->state_q_count > 0 &&
           audio->state_q[audio->state_q_head].at_sample <= sample_idx) {
        const AudioTickState st = audio->state_q[audio->state_q_head];
        audio->state_q_head = (audio->state_q_head + 1) % AUDIO_STATE_QUEUE_CAP;
        audio->state_q_count--;

        if (audio->lobby_mode && !st.lobby) {
            continue;
        }
        audio->has_consumed = true;
        audio->last_state_sample = sample_idx;
        audio->brush_target = audio->paused ? 0.0f : st.brush_target;
        audio->brush_active = !audio->paused && st.brush_active;
        audio->brush_speed = audio->brush_active ? st.brush_speed : 0.0f;
        audio->wind_target = st.wind_target;
        audio->brush_style = st.brush_style;
        audio->brush_dry = st.brush_dry;
        audio->brush_surge = st.brush_surge;
        if (!audio->lobby_mode) {
            audio_bgm_apply_track(audio, st.bgm);
        }
    }
}

void audio_render(AudioState *audio, int16_t *samples, size_t sample_count)
{
    if (samples == NULL || sample_count == 0) {
        return;
    }
    if (audio == NULL) {
        memset(samples, 0, sample_count * sizeof samples[0]);
        return;
    }

    const float master_gain = (float)audio->master_vol * 0.01f;
    const float bgm_gain = (float)audio->bgm_vol * 0.01f;
    const float se_gain = (float)audio->se_vol * 0.01f;

    /* In live callback mode, silence a held brush if the game loop stalls without publishing ticks
     * (~200 ms grace absorbs ordinary frame jitter without cutting off normal strokes). */
    enum { AUDIO_BRUSH_STALE_SAMPLES = AUDIO_SAMPLES_PER_TICK * 12 };
    const float duck_target = audio->paused ? 0.35f : 1.0f;
    for (size_t i = 0; i < sample_count; ++i) {
        const uint64_t sample_idx = audio->samples_rendered;
        drain_state_queue(audio, sample_idx);
        if (audio->realtime_mode && audio->brush_active && audio->state_q_count == 0 &&
            sample_idx >= audio->last_state_sample &&
            sample_idx - audio->last_state_sample >= (uint64_t)AUDIO_BRUSH_STALE_SAMPLES) {
            audio->brush_active = false;
            audio->brush_target = 0.0f;
            audio->brush_speed = 0.0f;
        }

        const float nz = audio_noise_next(audio);
        const float se = audio_voices_step(audio, sample_idx, nz);
        const float bgm = audio_bgm_step(audio, nz);
        audio->bgm_duck += (duck_target - audio->bgm_duck) * 0.0008f;

        float mix = (se * se_gain + bgm * bgm_gain * audio->bgm_duck) * master_gain;
        if (!isfinite(mix) || master_gain <= 0.0f) {
            mix = 0.0f;
        }
        const float clipped = ink_clampf(tanhf(mix * 1.15f), -1.0f, 1.0f);
        int32_t pcm = (int32_t)lroundf(clipped * 32767.0f);
        if (pcm > 32767) {
            pcm = 32767;
        } else if (pcm < -32767) {
            pcm = -32767;
        }
        samples[i] = (int16_t)pcm;
        audio->samples_rendered++;
    }
}

BgmTrack audio_bgm_track(const AudioState *audio) { return audio != NULL ? audio->bgm : BGM_NONE; }

static bool queued_ticks_finite(const AudioState *audio)
{
    if (audio->state_q_count < 0 || audio->state_q_count > AUDIO_STATE_QUEUE_CAP ||
        audio->state_q_head < 0 || audio->state_q_head >= AUDIO_STATE_QUEUE_CAP) {
        return false;
    }
    for (int32_t i = 0; i < audio->state_q_count; ++i) {
        const AudioTickState *st =
            &audio->state_q[(audio->state_q_head + i) % AUDIO_STATE_QUEUE_CAP];
        if (!isfinite(st->brush_target) || !isfinite(st->brush_speed) ||
            !isfinite(st->wind_target)) {
            return false;
        }
    }
    return true;
}

static bool pending_voices_finite(const AudioState *audio)
{
    if (audio->pending_count < 0 || audio->pending_count > AUDIO_PENDING_VOICE_CAP ||
        audio->pending_head < 0 || audio->pending_head >= AUDIO_PENDING_VOICE_CAP) {
        return false;
    }
    for (int32_t i = 0; i < audio->pending_count; ++i) {
        const AudioPendingVoice *pv =
            &audio->pending[(audio->pending_head + i) % AUDIO_PENDING_VOICE_CAP];
        if (!isfinite(pv->freq) || !isfinite(pv->param)) {
            return false;
        }
    }
    return true;
}

bool audio_validate_finite(const AudioState *audio)
{
    if (audio == NULL) {
        return false;
    }
    if (!isfinite(audio->xfade_t) || !isfinite(audio->wind_target) || !isfinite(audio->wind_env) ||
        !isfinite(audio->wind_lp) || !isfinite(audio->wind_bp) || !isfinite(audio->bgm_drum_env) ||
        !isfinite(audio->bgm_drum_ph) || !isfinite(audio->bgm_koto_env) ||
        !isfinite(audio->bgm_koto_ph) || !isfinite(audio->bgm_koto_freq) ||
        !isfinite(audio->bgm_drone_ph) || !isfinite(audio->bgm_victory_t) ||
        !isfinite(audio->bgm_duck)) {
        return false;
    }
    if (!audio_brush_validate_finite(audio) || !queued_ticks_finite(audio) ||
        !pending_voices_finite(audio)) {
        return false;
    }
    for (int i = 0; i < AUDIO_MAX_VOICES; ++i) {
        const AudioVoice *v = &audio->voices[i];
        if (!isfinite(v->env) || !isfinite(v->phase) || !isfinite(v->phase2) ||
            !isfinite(v->freq) || !isfinite(v->param) || !isfinite(v->lp) || !isfinite(v->bp) ||
            !isfinite(v->sample_pos) || !audio_pink_finite(&v->pink)) {
            return false;
        }
        for (int k = 0; k < AUDIO_VOICE_SVF_COUNT; ++k) {
            if (!audio_svf_finite(&v->svf[k])) {
                return false;
            }
        }
    }
    return true;
}
