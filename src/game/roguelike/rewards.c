#include "roguelike/rewards.h"

#include "core.h"
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

void rewards_generate_offer(GameState *g)
{
    if (g == NULL) {
        return;
    }
    RewardOffer *offer = &g->run.reward;
    UpgradeId base[UPG_COUNT];
    int32_t base_count = 0;
    for (int32_t i = 0; i < (int32_t)UPG_COUNT; ++i) {
        const UpgradeId id = (UpgradeId)i;
        if (game_upgrade_level(g, id) < game_upgrade_max_level(id)) {
            base[base_count++] = id;
        }
    }

    memset(offer->choice_ids, 0, sizeof offer->choice_ids);
    memset(offer->choice_next_level, 0, sizeof offer->choice_next_level);
    offer->choice_count = 0;

    if (base_count == 0) {
        offer->kind = REWARD_KIND_FALLBACK;
        offer->repair_available = true;
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
    ++offer->serial;
    g->internal.first_reward_offered = true;
}

bool rewards_pick_upgrade(GameState *g, int32_t choice_index)
{
    if (g == NULL || g->phase != RUN_PHASE_REWARD || g->run.reward.kind != REWARD_KIND_UPGRADE ||
        choice_index < 0 || choice_index >= g->run.reward.choice_count ||
        choice_index >= GAME_MAX_REWARD_CHOICES) {
        return false;
    }
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
    waves_start_wave(g, g->run.wave_index + 1);
    return true;
}

bool rewards_reroll(GameState *g)
{
    if (g == NULL || g->phase != RUN_PHASE_REWARD || g->run.reward.kind != REWARD_KIND_UPGRADE ||
        g->run.rerolls_left <= 0) {
        return false;
    }
    --g->run.rerolls_left;
    rewards_generate_offer(g);
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_REWARD_REROLL,
                                 .value = g->run.rerolls_left,
                                 .flags = 0,
                             });
    return true;
}

bool rewards_repair_shrine(GameState *g)
{
    if (g == NULL || g->phase != RUN_PHASE_REWARD || !g->run.reward.repair_available) {
        return false;
    }
    if (g->shrine_count > 0 && g->shrines[0].hp < g->shrines[0].max_hp) {
        ++g->shrines[0].hp;
    }
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_REWARD_PICK,
                                 .value = -1,
                                 .flags = EVF_REWARD_REPAIR,
                             });
    g->run.reward.kind = REWARD_KIND_NONE;
    g->run.reward.choice_count = 0;
    g->run.reward.repair_available = false;
    waves_start_wave(g, g->run.wave_index + 1);
    return true;
}
