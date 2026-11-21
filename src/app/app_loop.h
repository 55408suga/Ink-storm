#ifndef INK_APP_LOOP_H
#define INK_APP_LOOP_H

#include "audio.h"
#include "cli.h"
#include "game.h"
#include "render.h"

int app_run_window_loop(GameState *game, AudioState *audio, RenderState *render,
                        const CliOptions *opts, uint32_t fault_mask);

#endif
