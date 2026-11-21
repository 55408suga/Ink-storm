#include "ecs.h"

void ecs_init(EcsWorld *world) { *world = (EcsWorld){0}; }

uint32_t ecs_alive_count(const EcsWorld *world) { return world->alive_count; }
