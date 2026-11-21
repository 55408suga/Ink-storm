#include "ecs.h"

#include <string.h>

void ecs_init(EcsWorld *w)
{
    if (!w) {
        return;
    }
    memset(w, 0, sizeof *w);
    for (uint32_t i = 0; i < ECS_MAX_ENTITIES; ++i) {
        w->gen[i] = 1;
        w->free_list[i] = ECS_MAX_ENTITIES - 1 - i;
    }
    w->free_count = ECS_MAX_ENTITIES;
}

EntityId ecs_spawn(EcsWorld *w)
{
    if (!w || w->free_count == 0) {
        return (EntityId){ECS_INVALID_INDEX, 0};
    }
    const uint32_t index = w->free_list[--w->free_count];
    w->alive[index] = 1;
    w->pending_destroy[index] = 0;
    w->comp_mask[index] = 0;
    ++w->alive_count;
    if (w->high_water <= index) {
        w->high_water = index + 1;
    }
    return (EntityId){index, w->gen[index]};
}

bool ecs_is_alive(const EcsWorld *w, EntityId id)
{
    return w && id.index < ECS_MAX_ENTITIES && w->alive[id.index] &&
           !w->pending_destroy[id.index] && w->gen[id.index] == id.gen;
}

void ecs_destroy(EcsWorld *w, EntityId id)
{
    if (ecs_is_alive(w, id)) {
        w->pending_destroy[id.index] = 1;
        --w->alive_count;
    }
}

void ecs_flush_destroy(EcsWorld *w)
{
    if (!w) {
        return;
    }
    /* Pushing in descending order makes the next spawn reuse the lowest released slot. */
    for (uint32_t i = w->high_water; i-- > 0;) {
        if (!w->pending_destroy[i]) {
            continue;
        }
        w->alive[i] = 0;
        w->pending_destroy[i] = 0;
        w->comp_mask[i] = 0;
        if (++w->gen[i] == 0) {
            w->gen[i] = 1;
        }
        w->free_list[w->free_count++] = i;
    }
    while (w->high_water > 0 && !w->alive[w->high_water - 1]) {
        --w->high_water;
    }
}

bool ecs_add_mask(EcsWorld *w, EntityId id, uint64_t mask)
{
    if (!ecs_is_alive(w, id)) {
        return false;
    }
    w->comp_mask[id.index] |= mask;
    return true;
}

bool ecs_remove_mask(EcsWorld *w, EntityId id, uint64_t mask)
{
    if (!ecs_is_alive(w, id)) {
        return false;
    }
    w->comp_mask[id.index] &= ~mask;
    return true;
}

bool ecs_has_mask(const EcsWorld *w, EntityId id, uint64_t mask)
{
    return ecs_is_alive(w, id) && (w->comp_mask[id.index] & mask) == mask;
}

uint32_t ecs_alive_count(const EcsWorld *w) { return w ? w->alive_count : 0; }

EntityId ecs_entity_at(const EcsWorld *w, uint32_t index)
{
    if (!w || index >= w->high_water || !w->alive[index] || w->pending_destroy[index]) {
        return (EntityId){ECS_INVALID_INDEX, 0};
    }
    return (EntityId){index, w->gen[index]};
}
