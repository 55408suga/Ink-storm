#ifndef INK_FLUID_H
#define INK_FLUID_H

#include <stdint.h>

typedef struct {
    uint64_t steps;
} FluidState;

void fluid_init(FluidState *fluid);
void fluid_step_smoke(FluidState *fluid);

#endif
