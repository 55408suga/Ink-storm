#ifndef INK_APP_MENU_NAV_H
#define INK_APP_MENU_NAV_H

#include "input_map.h"
#include "ui_view.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum { MENU_NAV_UP = 0, MENU_NAV_DOWN, MENU_NAV_LEFT, MENU_NAV_RIGHT } MenuNavKey;

#define MENU_RESULT_LOBBY 0
#define MENU_RESULT_RETRY 1
#define MENU_RESULT_ENDLESS 2

bool menu_pointer_track(UiPointer *p, float x, float y);
void menu_pointer_enter_context(UiPointer *p, int32_t context);
int32_t menu_pointer_visible_hover(UiPointer *p, int32_t hit, bool moved, bool nav_key);
int32_t menu_nav_bokuan(int32_t cursor, MenuNavKey key);
int32_t menu_nav_result(int32_t cursor, bool has_endless, MenuNavKey key);
int32_t menu_nav_guide_page(int32_t page, int32_t delta);
bool menu_lobby_stroke_gate(bool *active, bool pressed, bool down, bool press_on_ui);
bool menu_action_ends_frame(MenuAction action);

#endif
