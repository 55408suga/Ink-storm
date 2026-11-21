#ifndef INK_ECS_H
#define INK_ECS_H

#include <stdbool.h>
#include <stdint.h>

#define ECS_MAX_ENTITIES 512
#define ECS_INVALID_INDEX UINT32_MAX

typedef struct {
    uint32_t index;
    uint32_t gen;
} EntityId;

typedef struct {
    uint32_t gen[ECS_MAX_ENTITIES];
    uint64_t comp_mask[ECS_MAX_ENTITIES];
    uint8_t alive[ECS_MAX_ENTITIES];
    uint8_t pending_destroy[ECS_MAX_ENTITIES];
    uint32_t free_list[ECS_MAX_ENTITIES];
    uint32_t free_count;
    uint32_t alive_count;
    uint32_t high_water; /* Exclusive upper bound for ascending slot iteration. */
} EcsWorld;

void ecs_init(EcsWorld *w);
EntityId ecs_spawn(EcsWorld *w);
bool ecs_is_alive(const EcsWorld *w, EntityId id);
/* The ID is invalidated immediately. Its slot is reclaimed by ecs_flush_destroy. */
void ecs_destroy(EcsWorld *w, EntityId id);
void ecs_flush_destroy(EcsWorld *w);
bool ecs_add_mask(EcsWorld *w, EntityId id, uint64_t mask);
bool ecs_remove_mask(EcsWorld *w, EntityId id, uint64_t mask);
bool ecs_has_mask(const EcsWorld *w, EntityId id, uint64_t mask);
uint32_t ecs_alive_count(const EcsWorld *w);
EntityId ecs_entity_at(const EcsWorld *w, uint32_t index);

#endif
