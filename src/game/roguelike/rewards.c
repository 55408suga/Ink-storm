#include "roguelike/rewards.h"

#include "core.h"
#include "roguelike/relics.h"
#include "roguelike/upgrades.h"
#include "roguelike/waves.h"

#include <stddef.h>
#include <string.h>

static bool already_chosen(const int32_t chosen[GAME_MAX_REWARD_CHOICES], int32_t count,
                           UpgradeId id)
{
    for (int32_t i = 0; i < count; ++i) {
        if (chosen[i] == (int32_t)id) {
            return true;
        }
    }
    return false;
}

static void copy_pool(UpgradeId dst[UPG_COUNT], const UpgradeId src[UPG_COUNT], int32_t count)
{
    for (int32_t i = 0; i < count; ++i) {
        dst[i] = src[i];
    }
}

static bool id_in_list(const int32_t *ids, int32_t n, int32_t id)
{
    for (int32_t i = 0; i < n; ++i) {
        if (ids[i] == id) {
            return true;
        }
    }
    return false;
}

static bool upgrade_offerable(const GameState *g, UpgradeId id)
{
    return game_upgrade_level(g, id) < game_upgrade_max_level(id);
}

static bool relic_offerable(const GameState *g, RelicId id)
{
    return game_is_relic_unlocked(g, id) && !game_has_relic(g, id);
}

/* Builds the candidate pool, leaving out `exclude` unless that would shrink the pool below a
 * full offer; excluded ids are then restored in order so a reroll still fills every slot. */
static int32_t build_upgrade_pool(const GameState *g, const int32_t *exclude, int32_t n_exclude,
                                  UpgradeId out[UPG_COUNT])
{
    int32_t count = 0;
    int32_t eligible = 0;
    for (int32_t i = 0; i < (int32_t)UPG_COUNT; ++i) {
        const UpgradeId id = (UpgradeId)i;
        if (!upgrade_offerable(g, id)) {
            continue;
        }
        ++eligible;
        if (!id_in_list(exclude, n_exclude, i)) {
            out[count++] = id;
        }
    }
    const int32_t want = eligible < GAME_MAX_REWARD_CHOICES ? eligible : GAME_MAX_REWARD_CHOICES;
    for (int32_t k = 0; k < n_exclude && count < want; ++k) {
        const int32_t id = exclude[k];
        if (id >= 0 && id < (int32_t)UPG_COUNT && upgrade_offerable(g, (UpgradeId)id)) {
            out[count++] = (UpgradeId)id;
        }
    }
    return count;
}

static void generate_upgrade_offer(GameState *g, const int32_t *exclude, int32_t n_exclude)
{
    RewardOffer *offer = &g->run.reward;
    UpgradeId base[UPG_COUNT];
    const int32_t base_count = build_upgrade_pool(g, exclude, n_exclude, base);

    memset(offer->choice_ids, 0, sizeof offer->choice_ids);
    memset(offer->choice_next_level, 0, sizeof offer->choice_next_level);
    offer->choice_count = 0;

    if (base_count == 0) {
        offer->kind = REWARD_KIND_FALLBACK;
        offer->repair_available = true;
        offer->repair_amount = 1;
        offer->crystal_reward = 10;
        ++offer->serial;
        g->internal.first_reward_offered = true;
        return;
    }

    offer->kind = REWARD_KIND_UPGRADE;
    const bool is_first = !g->internal.first_reward_offered;

    for (int32_t slot = 0; slot < GAME_MAX_REWARD_CHOICES; ++slot) {
        UpgradeId pool[UPG_COUNT];
        int32_t pool_count = 0;
        for (int32_t i = 0; i < base_count; ++i) {
            if (!already_chosen(offer->choice_ids, offer->choice_count, base[i])) {
                pool[pool_count++] = base[i];
            }
        }
        if (pool_count == 0) {
            break;
        }

        /* 2.2: First offer of the run restricts slot 0 to recommended axes if non-empty. */
        if (is_first && slot == 0) {
            UpgradeId filtered[UPG_COUNT];
            int32_t filtered_count = 0;
            for (int32_t i = 0; i < pool_count; ++i) {
                if (game_is_recommended_axis(g->config.brush_style, game_upgrade_axis(pool[i]))) {
                    filtered[filtered_count++] = pool[i];
                }
            }
            if (filtered_count > 0) {
                copy_pool(pool, filtered, filtered_count);
                pool_count = filtered_count;
            }
        }

        /* 2.3: Slot 2 avoids repeating the same axis if slots 0 and 1 share an axis. */
        if (slot == 2) {
            const UpgradeAxis axis0 = game_upgrade_axis((UpgradeId)offer->choice_ids[0]);
            const UpgradeAxis axis1 = game_upgrade_axis((UpgradeId)offer->choice_ids[1]);
            if (axis0 == axis1) {
                UpgradeId filtered[UPG_COUNT];
                int32_t filtered_count = 0;
                for (int32_t i = 0; i < pool_count; ++i) {
                    if (game_upgrade_axis(pool[i]) != axis0) {
                        filtered[filtered_count++] = pool[i];
                    }
                }
                if (filtered_count > 0) {
                    copy_pool(pool, filtered, filtered_count);
                    pool_count = filtered_count;
                }
            }
        }

        /* 2.4: Always draw once; < 0.4 narrows to owned upgrades (Lv1..2) if non-empty. */
        const float owned_roll = ink_rng_f01(&g->rng);
        if (owned_roll < 0.4f) {
            UpgradeId filtered[UPG_COUNT];
            int32_t filtered_count = 0;
            for (int32_t i = 0; i < pool_count; ++i) {
                if (game_upgrade_level(g, pool[i]) > 0) {
                    filtered[filtered_count++] = pool[i];
                }
            }
            if (filtered_count > 0) {
                copy_pool(pool, filtered, filtered_count);
                pool_count = filtered_count;
            }
        }

        /* 2.5: Always draw weighted choice (recommended axis = 3, other = 2). */
        uint64_t total_weight = 0;
        uint32_t weights[UPG_COUNT];
        for (int32_t i = 0; i < pool_count; ++i) {
            const uint32_t w =
                game_is_recommended_axis(g->config.brush_style, game_upgrade_axis(pool[i])) ? 3u
                                                                                            : 2u;
            weights[i] = w;
            total_weight += w;
        }
        if (total_weight == 0) {
            break;
        }
        const uint64_t pick_val = ink_rng_next(&g->rng) % total_weight;
        uint64_t acc = 0;
        UpgradeId chosen = pool[pool_count - 1];
        for (int32_t i = 0; i < pool_count; ++i) {
            acc += weights[i];
            if (pick_val < acc) {
                chosen = pool[i];
                break;
            }
        }

        offer->choice_ids[slot] = (int32_t)chosen;
        offer->choice_next_level[slot] = (uint8_t)(game_upgrade_level(g, chosen) + 1u);
        offer->choice_count = slot + 1;
    }

    offer->repair_available = g->shrine_count > 0 && g->shrines[0].hp < g->shrines[0].max_hp;
    offer->repair_amount = 1;
    offer->crystal_reward = 0;
    ++offer->serial;
    g->internal.first_reward_offered = true;
}

void rewards_generate_offer(GameState *g)
{
    if (g == NULL) {
        return;
    }
    generate_upgrade_offer(g, NULL, 0);
}

static void clear_reward_and_advance_act(GameState *g)
{
    g->run.reward.kind = REWARD_KIND_NONE;
    g->run.reward.choice_count = 0;
    g->run.reward.repair_available = false;
    g->run.reward.repair_amount = 0;
    g->run.reward.crystal_reward = 0;
    g->internal.wave_clear_is_boss = false;
    ++g->run.act_index;
    g->run.endless_loop = g->run.act_index / GAME_MAIN_ACT_COUNT;
    g->run.wave_index = 0;
    g->internal.act_prepared = false;
    g->phase = RUN_PHASE_ACT_TRANSITION;
    g->phase_t = 0.0f;
}

static void generate_relic_offer(GameState *g, const int32_t *exclude, int32_t n_exclude)
{
    RewardOffer *offer = &g->run.reward;
    RelicId pool[RELIC_COUNT];
    int32_t pool_count = 0;
    int32_t eligible = 0;
    for (int32_t i = 0; i < (int32_t)RELIC_COUNT; ++i) {
        const RelicId id = (RelicId)i;
        if (!relic_offerable(g, id)) {
            continue;
        }
        ++eligible;
        if (!id_in_list(exclude, n_exclude, i)) {
            pool[pool_count++] = id;
        }
    }
    const int32_t want = eligible < GAME_MAX_REWARD_CHOICES ? eligible : GAME_MAX_REWARD_CHOICES;
    for (int32_t k = 0; k < n_exclude && pool_count < want; ++k) {
        const int32_t id = exclude[k];
        if (id >= 0 && id < (int32_t)RELIC_COUNT && relic_offerable(g, (RelicId)id)) {
            pool[pool_count++] = (RelicId)id;
        }
    }

    memset(offer->choice_ids, 0, sizeof offer->choice_ids);
    memset(offer->choice_next_level, 0, sizeof offer->choice_next_level);
    offer->choice_count = 0;

    if (pool_count == 0) {
        offer->kind = REWARD_KIND_FALLBACK;
        offer->repair_available = true;
        offer->repair_amount = 3;
        offer->crystal_reward = 30;
        ++offer->serial;
        return;
    }

    offer->kind = REWARD_KIND_RELIC;
    offer->repair_available = false;
    offer->repair_amount = 0;
    offer->crystal_reward = 0;

    for (int32_t slot = 0; slot < GAME_MAX_REWARD_CHOICES && pool_count > 0; ++slot) {
        const int32_t pick = (int32_t)(ink_rng_next(&g->rng) % (uint64_t)pool_count);
        offer->choice_ids[slot] = (int32_t)pool[pick];
        offer->choice_next_level[slot] = 1u;
        offer->choice_count = slot + 1;
        for (int32_t k = pick; k + 1 < pool_count; ++k) {
            pool[k] = pool[k + 1];
        }
        --pool_count;
    }
    ++offer->serial;
}

void rewards_generate_relic_offer(GameState *g)
{
    if (g == NULL) {
        return;
    }
    generate_relic_offer(g, NULL, 0);
}

static int32_t count_offerable(const GameState *g, RewardKind kind)
{
    int32_t n = 0;
    if (kind == REWARD_KIND_RELIC) {
        for (int32_t i = 0; i < (int32_t)RELIC_COUNT; ++i) {
            n += relic_offerable(g, (RelicId)i) ? 1 : 0;
        }
    } else {
        for (int32_t i = 0; i < (int32_t)UPG_COUNT; ++i) {
            n += upgrade_offerable(g, (UpgradeId)i) ? 1 : 0;
        }
    }
    return n;
}

bool rewards_pick_reward(GameState *g, int32_t choice_index)
{
    if (g == NULL || g->phase != RUN_PHASE_REWARD || choice_index < 0 ||
        choice_index >= g->run.reward.choice_count || choice_index >= GAME_MAX_REWARD_CHOICES) {
        return false;
    }
    if (g->run.reward.kind == REWARD_KIND_UPGRADE) {
        const UpgradeId chosen = (UpgradeId)g->run.reward.choice_ids[choice_index];
        if (!roguelike_grant_upgrade(g, chosen)) {
            return false;
        }
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_REWARD_PICK,
                                     .value = (int32_t)chosen,
                                     .flags = 0,
                                 });
        g->run.reward.kind = REWARD_KIND_NONE;
        g->run.reward.choice_count = 0;
        g->run.reward.repair_available = false;
        g->run.reward.repair_amount = 0;
        g->run.reward.crystal_reward = 0;
        waves_start_wave(g, g->run.wave_index + 1);
        return true;
    }
    if (g->run.reward.kind == REWARD_KIND_RELIC) {
        const RelicId chosen = (RelicId)g->run.reward.choice_ids[choice_index];
        if (!roguelike_grant_relic(g, chosen)) {
            return false;
        }
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_REWARD_PICK,
                                     .value = (int32_t)chosen,
                                     .flags = EVF_REWARD_RELIC,
                                 });
        clear_reward_and_advance_act(g);
        return true;
    }
    return false;
}

static int32_t shown_choice_count(const GameState *g)
{
    const int32_t shown = g->run.reward.choice_count;
    if (shown < 0) {
        return 0;
    }
    return shown > GAME_MAX_REWARD_CHOICES ? GAME_MAX_REWARD_CHOICES : shown;
}

bool rewards_can_reroll(const GameState *g)
{
    if (g == NULL || g->phase != RUN_PHASE_REWARD || g->run.rerolls_left <= 0 ||
        (g->run.reward.kind != REWARD_KIND_UPGRADE && g->run.reward.kind != REWARD_KIND_RELIC)) {
        return false;
    }
    return count_offerable(g, g->run.reward.kind) > shown_choice_count(g);
}

bool rewards_reroll(GameState *g)
{
    if (!rewards_can_reroll(g)) {
        return false;
    }
    const RewardKind kind = g->run.reward.kind;
    const int32_t shown = shown_choice_count(g);
    int32_t previous[GAME_MAX_REWARD_CHOICES];
    memcpy(previous, g->run.reward.choice_ids, sizeof previous);
    --g->run.rerolls_left;
    if (kind == REWARD_KIND_RELIC) {
        generate_relic_offer(g, previous, shown);
    } else {
        generate_upgrade_offer(g, previous, shown);
    }
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_REWARD_REROLL,
                                 .value = g->run.rerolls_left,
                                 .flags = 0,
                             });
    return true;
}

static void add_bonus_crystals(GameState *g, int64_t gain)
{
    if (gain <= 0) {
        return;
    }
    const int64_t cur = g->run.bonus_crystals > 0 ? (int64_t)g->run.bonus_crystals : 0;
    const int64_t next =
        gain > (int64_t)META_MAX_CRYSTALS - cur ? (int64_t)META_MAX_CRYSTALS : cur + gain;
    g->run.bonus_crystals = (int32_t)next;
}

bool rewards_repair_shrine(GameState *g)
{
    if (g == NULL || g->phase != RUN_PHASE_REWARD || !g->run.reward.repair_available) {
        return false;
    }
    const int32_t amount = g->run.reward.repair_amount > 0 ? g->run.reward.repair_amount : 1;
    if (g->shrine_count > 0) {
        ShrineState *sh = &g->shrines[0];
        int32_t missing = sh->max_hp - sh->hp;
        if (missing < 0) {
            missing = 0;
        }
        const int32_t healed = amount < missing ? amount : missing;
        const int32_t overflow = amount - healed;
        sh->hp += healed;
        if (g->internal.wave_clear_is_boss && g->run.reward.kind == REWARD_KIND_FALLBACK &&
            overflow > 0) {
            add_bonus_crystals(g, (int64_t)overflow * 15);
        }
    }
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_REWARD_PICK,
                                 .value = -1,
                                 .flags = EVF_REWARD_REPAIR,
                             });
    if (g->internal.wave_clear_is_boss) {
        clear_reward_and_advance_act(g);
    } else {
        g->run.reward.kind = REWARD_KIND_NONE;
        g->run.reward.choice_count = 0;
        g->run.reward.repair_available = false;
        g->run.reward.repair_amount = 0;
        g->run.reward.crystal_reward = 0;
        waves_start_wave(g, g->run.wave_index + 1);
    }
    return true;
}

bool rewards_pick_endless_crystal(GameState *g)
{
    if (g == NULL || g->phase != RUN_PHASE_REWARD || g->run.reward.kind != REWARD_KIND_FALLBACK ||
        g->run.reward.crystal_reward <= 0) {
        return false;
    }
    const int32_t gain = g->run.reward.crystal_reward;
    add_bonus_crystals(g, (int64_t)gain);
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_REWARD_PICK,
                                 .value = gain,
                                 .flags = EVF_REWARD_CRYSTAL,
                             });
    if (g->internal.wave_clear_is_boss) {
        clear_reward_and_advance_act(g);
    } else {
        g->run.reward.kind = REWARD_KIND_NONE;
        g->run.reward.choice_count = 0;
        g->run.reward.repair_available = false;
        g->run.reward.repair_amount = 0;
        g->run.reward.crystal_reward = 0;
        waves_start_wave(g, g->run.wave_index + 1);
    }
    return true;
}
