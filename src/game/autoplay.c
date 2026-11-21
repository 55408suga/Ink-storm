#include "game.h"

#include <math.h>
#include <stddef.h>

BrushInput game_autoplay_input(const GameState *g)
{
    /* P3-Pre stub */
    BrushInput in = {0};
    if (g == NULL) {
        return in;
    }
    const float angle = (float)g->ticks * 0.06f;
    const float prev_angle = (float)(g->ticks > 0 ? g->ticks - 1 : 0) * 0.06f;
    in.mx = 640.0f + 200.0f * cosf(angle);
    in.my = 360.0f + 130.0f * sinf(angle);
    in.pmx = 640.0f + 200.0f * cosf(prev_angle);
    in.pmy = 360.0f + 130.0f * sinf(prev_angle);
    in.paint = true;
    return in;
}

GameCommand game_autoplay_command(const GameState *g)
{
    /* P3-Pre stub */
    if (g != NULL && g->phase == RUN_PHASE_REWARD) {
        return (GameCommand){.type = CMD_PICK_REWARD, .arg = 0};
    }
    return (GameCommand){.type = CMD_NONE, .arg = 0};
}
