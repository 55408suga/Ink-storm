#ifndef INK_APP_INPUT_MAP_H
#define INK_APP_INPUT_MAP_H

#include "game.h"
#include "input_latch.h"
#include "ui_view.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    MENU_ACT_NONE = 0,
    MENU_ACT_TOGGLE_PAUSE,
    MENU_ACT_LOBBY_SORTIE,
    MENU_ACT_LOBBY_BOKUAN,
    MENU_ACT_LOBBY_SETTINGS,
    MENU_ACT_LOBBY_QUIT,
    MENU_ACT_BRUSH_PICK,
    MENU_ACT_BACK,
    MENU_ACT_SHOP_BUY_UPG,
    MENU_ACT_SHOP_BUY_RELIC,
    MENU_ACT_SETTINGS_CHANGE,
    MENU_ACT_SETTINGS_RESET_HINTS,
    MENU_ACT_SETTINGS_RESET_DEFAULTS,
    MENU_ACT_PAUSE_RESUME,
    MENU_ACT_PAUSE_RESTART,
    MENU_ACT_PAUSE_SETTINGS,
    MENU_ACT_PAUSE_LOBBY,
    MENU_ACT_CONFIRM_YES,
    MENU_ACT_CONFIRM_NO,
    MENU_ACT_RESULT_ENDLESS,
    MENU_ACT_RESULT_LOBBY,
    MENU_ACT_RESULT_RETRY
} MenuAction;

typedef struct {
    MenuAction action;
    int32_t arg;
    int32_t delta;
    float mouse_x, mouse_y;
    bool left_down;
    bool ui_clicked;
    bool lobby_paint;
} MenuInput;

RawInput input_map_read(const GameState *g, UiView *ui);
MenuInput input_map_read_menu(const GameState *g, UiView *ui);

#endif
