#include "fluid.h"

void fluid_init(FluidState *fluid) { *fluid = (FluidState){0}; }

void fluid_step_smoke(FluidState *fluid) { ++fluid->steps; }
