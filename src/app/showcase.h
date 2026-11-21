#ifndef INK_APP_SHOWCASE_H
#define INK_APP_SHOWCASE_H

#include "cli.h"
#include "game.h"
#include "render.h"

int app_run_showcase(GameState *game, RenderState *render, const CliOptions *opts,
                     uint32_t fault_mask);

#endif
