#include "enemies/boss_break.h"

#include "combat/damage.h"
#include "core.h"
#include "enemies/enemy_defs.h"

#include <math.h>
#include <stddef.h>

static const float k_break_two_pi = 6.2831853f;

float boss_break_threshold(const GameState *g)
{
    const int32_t loop_n = enemy_endless_loop_n(g);
    return 100.0f * fminf(1.45f, 1.0f + 0.15f * (float)loop_n);
}

float boss_break_duration_for(EnemyDefId def_id, BossBreakKind kind)
{
    if (def_id == ENEMY_BOSS_TENGU) {
        return 2.8f;
    }
    if (def_id == ENEMY_BOSS_NUE) {
        return 2.5f;
    }
    if (def_id == ENEMY_BOSS_RYU) {
        return kind == BOSS_BREAK_FLINCH ? 1.8f : 2.5f;
    }
    return 2.5f;
}

void boss_break_start(GameState *g, EntityId boss_id, BossBreakKind kind, float dur, int32_t orb_n)
{
    if (g == NULL || kind == BOSS_BREAK_NONE || !isfinite(dur) || dur <= 0.0f) {
        return;
    }
    const TransformComp *tr = game_transform(g, boss_id);
    EnemyComp *en = game_enemy_mut(g, boss_id);
    BossComp *boss = game_boss_mut(g, boss_id);
    if (tr == NULL || en == NULL || boss == NULL) {
        return;
    }

    boss->break_t = dur;
    boss->break_kind = (int32_t)kind;
    boss->stagger = 0.0f;
    boss->stagger_idle_t = 0.0f;
    boss->telegraph = 0.0f;
    boss->gale_t = 0.0f;
    boss->volley_t = 0.0f;

    if (en->def_id == ENEMY_BOSS_NUE) {
        en->windup = 0.0f;
    } else if (en->def_id == ENEMY_BOSS_RYU) {
        if (kind == BOSS_BREAK_STANCE) {
            g->internal.ryu_coil_t = 0.0f;
            g->internal.ryu_coil_active = false;
        } else if (kind == BOSS_BREAK_FLINCH) {
            for (int32_t s = 0; s < GAME_RYU_ORB_COUNT; ++s) {
                const EntityId oid = g->ryu_body.orb_ids[s];
                BossComp *oboss = (oid.index != ECS_INVALID_INDEX && ecs_is_alive(&g->ecs, oid))
                                      ? game_boss_mut(g, oid)
                                      : NULL;
                if (oboss != NULL) {
                    oboss->telegraph = 0.0f;
                    oboss->gale_t = 0.0f;
                }
            }
            const uint32_t hw = g->ecs.high_water;
            for (uint32_t i = 0; i < hw; ++i) {
                const EntityId gid = ecs_entity_at(&g->ecs, i);
                if (gid.index == ECS_INVALID_INDEX || !ecs_has_mask(&g->ecs, gid, COMP_GIMMICK)) {
                    continue;
                }
                const GimmickComp *gim = game_gimmick(g, gid);
                if (gim != NULL && gim->kind == (int32_t)GIMMICK_LIGHTNING_STRIKE) {
                    ecs_destroy(&g->ecs, gid);
                }
            }
        }
    }

    if (orb_n > 0) {
        for (int32_t k = 0; k < orb_n; ++k) {
            const float ang = ((float)k * k_break_two_pi) / (float)orb_n;
            (void)combat_spawn_orb(g, tr->x + cosf(ang) * 28.0f, tr->y + sinf(ang) * 28.0f);
        }
    }

    const int32_t ev_val =
        kind == BOSS_BREAK_FLINCH ? (int32_t)BOSS_BREAK_EV_FLINCH : (int32_t)BOSS_BREAK_EV_START;
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_BOSS_BREAK,
                                 .x = tr->x,
                                 .y = tr->y,
                                 .value = ev_val,
                                 .flags = 0,
                             });
}

void boss_break_add_stagger(GameState *g, EntityId boss_id, float amount)
{
    if (g == NULL || !isfinite(amount) || amount <= 0.0f) {
        return;
    }
    const HealthComp *hp = game_health(g, boss_id);
    const EnemyComp *en = game_enemy(g, boss_id);
    BossComp *boss = game_boss_mut(g, boss_id);
    if (hp == NULL || en == NULL || boss == NULL || hp->hp <= 0 || hp->invuln_t > 0.0f) {
        return;
    }
    if (boss->break_t > 0.0f || boss->break_guard_t > 0.0f) {
        return;
    }
    const bool eligible =
        (en->def_id == ENEMY_BOSS_TENGU) || (en->def_id == ENEMY_BOSS_RYU && boss->phase_idx >= 1);
    if (!eligible) {
        return;
    }

    const float thresh = boss_break_threshold(g);
    boss->stagger = ink_clampf(boss->stagger + amount, 0.0f, thresh);
    boss->stagger_idle_t = 0.0f;
    if (boss->stagger + 1e-4f >= thresh) {
        const float dur = boss_break_duration_for(en->def_id, BOSS_BREAK_STANCE);
        boss_break_start(g, boss_id, BOSS_BREAK_STANCE, dur, 3);
    }
}

void boss_break_end(GameState *g, EntityId boss_id, bool emit_end)
{
    if (g == NULL) {
        return;
    }
    const TransformComp *tr = game_transform(g, boss_id);
    BossComp *boss = game_boss_mut(g, boss_id);
    if (boss == NULL) {
        return;
    }
    const bool was_active = boss->break_t > 0.0f;
    boss->break_t = 0.0f;
    boss->break_kind = (int32_t)BOSS_BREAK_NONE;
    boss->stagger = 0.0f;
    boss->stagger_idle_t = 0.0f;
    if (emit_end && was_active && tr != NULL) {
        (void)game_push_event(g, (GameEvent){
                                     .type = EV_BOSS_BREAK,
                                     .x = tr->x,
                                     .y = tr->y,
                                     .value = (int32_t)BOSS_BREAK_EV_END,
                                     .flags = 0,
                                 });
    }
}

void boss_break_step(GameState *g, EntityId boss_id)
{
    if (g == NULL) {
        return;
    }
    const TransformComp *tr = game_transform(g, boss_id);
    EnemyComp *en = game_enemy_mut(g, boss_id);
    BossComp *boss = game_boss_mut(g, boss_id);
    if (tr == NULL || en == NULL || boss == NULL) {
        return;
    }

    if (boss->break_t > 0.0f) {
        boss->break_t -= INK_FIXED_DT;
        if (boss->break_t <= 1e-4f) {
            const BossBreakKind finished = (BossBreakKind)boss->break_kind;
            boss->break_t = 0.0f;
            boss->break_kind = (int32_t)BOSS_BREAK_NONE;
            boss->stagger = 0.0f;
            boss->stagger_idle_t = 0.0f;
            if (finished != BOSS_BREAK_FLINCH) {
                if (en->def_id == ENEMY_BOSS_TENGU) {
                    boss->break_guard_t = 3.0f;
                } else if (en->def_id == ENEMY_BOSS_NUE) {
                    boss->break_guard_t = 3.0f;
                    en->fire_cd = fmaxf(en->fire_cd, 0.6f);
                } else if (en->def_id == ENEMY_BOSS_RYU) {
                    boss->break_guard_t = 4.0f;
                }
            }
            (void)game_push_event(g, (GameEvent){
                                         .type = EV_BOSS_BREAK,
                                         .x = tr->x,
                                         .y = tr->y,
                                         .value = (int32_t)BOSS_BREAK_EV_END,
                                         .flags = 0,
                                     });
        }
        return;
    }

    if (boss->break_guard_t > 0.0f) {
        boss->break_guard_t -= INK_FIXED_DT;
        if (boss->break_guard_t <= 1e-4f) {
            boss->break_guard_t = 0.0f;
        }
    }

    if (boss->stagger > 0.0f) {
        boss->stagger_idle_t += INK_FIXED_DT;
        if (boss->stagger_idle_t >= 2.5f) {
            boss->stagger -= 8.0f * INK_FIXED_DT;
            if (boss->stagger <= 1e-4f) {
                boss->stagger = 0.0f;
                boss->stagger_idle_t = 0.0f;
            }
        }
        const float thresh = boss_break_threshold(g);
        boss->stagger = ink_clampf(boss->stagger, 0.0f, thresh);
    }
}

float boss_break_damage_mul(const GameState *g, EntityId target, uint32_t dmg_flags)
{
    if (g == NULL || (dmg_flags & DMG_SLAM) != 0u) {
        return 1.0f;
    }
    const EnemyComp *en = game_enemy(g, target);
    const BossComp *boss = game_boss(g, target);
    if (en == NULL || boss == NULL) {
        return 1.0f;
    }
    if (en->def_id == ENEMY_BOSS_RYU_ORB) {
        const EntityId head =
            boss->parent_id.index != ECS_INVALID_INDEX ? boss->parent_id : g->ryu_body.head_id;
        const BossComp *hboss = (head.index != ECS_INVALID_INDEX) ? game_boss(g, head) : NULL;
        if (hboss != NULL && hboss->break_t > 0.0f &&
            hboss->break_kind == (int32_t)BOSS_BREAK_FLINCH) {
            return 1.25f;
        }
        return 1.0f;
    }
    if (boss->break_t <= 0.0f) {
        return 1.0f;
    }
    if (en->def_id == ENEMY_BOSS_RYU && boss->phase_idx >= 2 && (dmg_flags & DMG_CATACLYSM) != 0u) {
        return 1.25f;
    }
    return 1.5f;
}
