#ifndef INK_ECS_H
#define INK_ECS_H

#include <stdint.h>

typedef struct {
    uint32_t index;
    uint32_t gen;
} EntityId;

typedef struct {
    uint32_t alive_count;
} EcsWorld;

void ecs_init(EcsWorld *world);
uint32_t ecs_alive_count(const EcsWorld *world);

#endif
