#include "roguelike/hints.h"

#include "core.h"

#include <stddef.h>
#include <stdint.h>

static bool hint_already_triggered(const GameState *g, HintId id)
{
    return (g->internal.triggered_hints_mask & (UINT32_C(1) << (uint32_t)id)) != 0;
}

static void trigger_hint_once(GameState *g, HintId id, float x, float y)
{
    const uint32_t bit = UINT32_C(1) << (uint32_t)id;
    if ((g->internal.triggered_hints_mask & bit) != 0) {
        return;
    }
    g->internal.triggered_hints_mask |= bit;
    (void)game_push_event(g, (GameEvent){
                                 .type = EV_HINT_TRIGGER,
                                 .x = x,
                                 .y = y,
                                 .value = (int32_t)id,
                                 .flags = 0,
                             });
}

void hints_check_tick(GameState *g)
{
    if (g == NULL || g->phase != RUN_PHASE_PLAYING) {
        return;
    }

    const float sx = g->shrine_count > 0 ? g->shrines[0].x : (float)INK_SCREEN_W * 0.5f;
    const float sy = g->shrine_count > 0 ? g->shrines[0].y : (float)INK_SCREEN_H * 0.5f;

    if (!hint_already_triggered(g, HINT_STROKE) && g->run.act_index == 0 &&
        g->run.wave_index == 0) {
        trigger_hint_once(g, HINT_STROKE, sx, sy);
    }

    const bool need_fireball = !hint_already_triggered(g, HINT_CYCLONE_FIREBALL);
    const bool need_umbrella = !hint_already_triggered(g, HINT_GALE_UMBRELLA);
    if (need_fireball || need_umbrella) {
        const uint32_t hw = g->ecs.high_water;
        for (uint32_t i = 0; i < hw; ++i) {
            const EntityId id = ecs_entity_at(&g->ecs, i);
            if (id.index == ECS_INVALID_INDEX) {
                continue;
            }
            const TransformComp *tr = game_transform(g, id);
            if (tr == NULL || tr->dissolving) {
                continue;
            }
            if (!hint_already_triggered(g, HINT_CYCLONE_FIREBALL)) {
                const ProjComp *proj = game_proj(g, id);
                if (proj != NULL && !proj->deflected && proj->kind == PROJ_FIREBALL) {
                    trigger_hint_once(g, HINT_CYCLONE_FIREBALL, tr->x, tr->y);
                }
            }
            if (!hint_already_triggered(g, HINT_GALE_UMBRELLA)) {
                const EnemyComp *en = game_enemy(g, id);
                if (en != NULL && en->def_id == ENEMY_UMBRELLA) {
                    trigger_hint_once(g, HINT_GALE_UMBRELLA, tr->x, tr->y);
                }
            }
            if (hint_already_triggered(g, HINT_CYCLONE_FIREBALL) &&
                hint_already_triggered(g, HINT_GALE_UMBRELLA)) {
                break;
            }
        }
    }

    if (!hint_already_triggered(g, HINT_INK_REGEN) && g->player.ink < 20.0f) {
        trigger_hint_once(g, HINT_INK_REGEN, g->player.cursor_x, g->player.cursor_y);
    }

    const int32_t ev_n = g->event_count;
    for (int32_t i = 0; i < ev_n; ++i) {
        const GameEvent *ev = &g->events[i];
        switch (ev->type) {
        case EV_IGNITE:
            trigger_hint_once(g, HINT_REACT_BLAZING, ev->x, ev->y);
            break;
        case EV_STORM_ZAP:
            trigger_hint_once(g, HINT_REACT_TEMPEST, ev->x2, ev->y2);
            break;
        case EV_THUNDERBURST:
            trigger_hint_once(g, HINT_REACT_THUNDERBURST, ev->x, ev->y);
            break;
        case EV_CATACLYSM:
            trigger_hint_once(g, HINT_REACT_CATACLYSM, ev->x, ev->y);
            break;
        default:
            break;
        }
    }
}
