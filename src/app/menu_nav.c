#include "menu_nav.h"

#include "meta.h"

#include <math.h>
#include <stddef.h>

bool menu_pointer_track(UiPointer *p, float x, float y)
{
    if (p == NULL) {
        return false;
    }
    const bool moved = !p->valid || fabsf(x - p->x) > 0.5f || fabsf(y - p->y) > 0.5f;
    p->x = x;
    p->y = y;
    p->valid = true;
    return moved;
}

/* A menu or dialog that opens under a resting mouse must keep its default focus (e.g. "No" in a
 * confirmation); hover only takes over once the pointer actually moves inside the new context. */
void menu_pointer_enter_context(UiPointer *p, int32_t context)
{
    if (p != NULL && p->context != context) {
        p->context = context;
        p->hover_suppressed = true;
    }
}

/* A resting mouse keeps re-reporting the same hit every frame; once the keyboard takes over, that
 * stale hover must not snap the cursor back until the mouse actually moves again. */
int32_t menu_pointer_visible_hover(UiPointer *p, int32_t hit, bool moved, bool nav_key)
{
    if (p == NULL) {
        return hit;
    }
    if (nav_key) {
        p->hover_suppressed = true;
    } else if (moved) {
        p->hover_suppressed = false;
    }
    return p->hover_suppressed ? UI_HOVER_NONE : hit;
}

int32_t menu_nav_bokuan(int32_t cursor, MenuNavKey key)
{
    const int32_t upg_n = META_UPG_COUNT;
    const int32_t relic_n = META_RELIC_UNLOCK_COUNT;
    const int32_t back = upg_n + relic_n;
    if (cursor < 0 || cursor > back) {
        return 0;
    }
    if (cursor == back) {
        return key == MENU_NAV_UP ? upg_n - 1 : (key == MENU_NAV_DOWN ? 0 : back);
    }
    if (cursor < upg_n) {
        switch (key) {
        case MENU_NAV_UP:
            return cursor > 0 ? cursor - 1 : back;
        case MENU_NAV_DOWN:
            return cursor + 1 < upg_n ? cursor + 1 : back;
        case MENU_NAV_RIGHT: {
            const int32_t last_row = (relic_n - 1) / 2;
            const int32_t row = cursor < last_row ? cursor : last_row;
            return upg_n + row * 2;
        }
        case MENU_NAV_LEFT:
        default:
            return cursor;
        }
    }
    const int32_t s = cursor - upg_n;
    const int32_t col = s % 2;
    const int32_t row = s / 2;
    switch (key) {
    case MENU_NAV_UP:
        return s >= 2 ? cursor - 2 : back;
    case MENU_NAV_DOWN:
        return s + 2 < relic_n ? cursor + 2 : back;
    case MENU_NAV_LEFT:
        return col == 1 ? cursor - 1 : (row < upg_n ? row : upg_n - 1);
    case MENU_NAV_RIGHT:
    default:
        return (col == 0 && s + 1 < relic_n) ? cursor + 1 : cursor;
    }
}

int32_t menu_nav_result(int32_t cursor, bool has_endless, MenuNavKey key)
{
    static const int32_t k_with_endless[3] = {MENU_RESULT_ENDLESS, MENU_RESULT_LOBBY,
                                              MENU_RESULT_RETRY};
    static const int32_t k_plain[2] = {MENU_RESULT_LOBBY, MENU_RESULT_RETRY};
    const int32_t *order = has_endless ? k_with_endless : k_plain;
    const int32_t n = has_endless ? 3 : 2;
    int32_t pos = -1;
    for (int32_t i = 0; i < n; ++i) {
        if (order[i] == cursor) {
            pos = i;
        }
    }
    if (pos < 0) {
        return MENU_RESULT_LOBBY;
    }
    if (key == MENU_NAV_LEFT) {
        pos = (pos + n - 1) % n;
    } else if (key == MENU_NAV_RIGHT) {
        pos = (pos + 1) % n;
    }
    return order[pos];
}

/* A Lobby test stroke may only begin on open canvas, but once begun it keeps painting while the
 * drag crosses buttons, and the press that started it never doubles as a button click. */
bool menu_lobby_stroke_gate(bool *active, bool pressed, bool down, bool press_on_ui)
{
    if (active == NULL) {
        return false;
    }
    if (!down) {
        *active = false;
    } else if (pressed) {
        *active = !press_on_ui;
    }
    return *active;
}

bool menu_action_ends_frame(MenuAction action) { return action != MENU_ACT_NONE; }
