#ifndef INK_APP_DEMO_H
#define INK_APP_DEMO_H

#include "audio.h"
#include "cli.h"
#include "game.h"
#include "render.h"

#include <stdbool.h>
#include <stdint.h>

int app_run_demo(GameState *game, AudioState *audio, RenderState *render, const CliOptions *opts,
                 uint32_t fault_mask);

bool app_snap_ensure_dir(const char *dir_path);
bool app_snap_capture(RenderState *render, const GameState *game, const UiView *ui,
                      RenderTexture2D target, const char *dir_path, int snap_index);

#endif
