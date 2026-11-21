#include "roguelike/stage_gimmicks.h"

#include <stddef.h>

void stage_gimmicks_setup_act(GameState *g, int32_t act_index)
{
    (void)act_index;
    if (g == NULL) {
        return;
    }
    fluid_clear_solids(&g->fluid);
}

void stage_gimmicks_step(GameState *g) { (void)g; }
