#ifndef INK_APP_INPUT_MAP_H
#define INK_APP_INPUT_MAP_H

#include "game.h"
#include "input_latch.h"
#include "ui_view.h"

RawInput input_map_read(const GameState *g, UiView *ui);

#endif
