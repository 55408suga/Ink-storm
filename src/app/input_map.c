#include "input_map.h"

#include "raylib.h"
#include "render.h"

#include <stddef.h>

RawInput input_map_read(const GameState *g, UiView *ui)
{
    const Vector2 mouse = GetMousePosition();
    const bool left_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    RawInput raw = {
        .mouse_x = mouse.x,
        .mouse_y = mouse.y,
        .left_down = left_down,
        .pick_pressed = -1,
    };

    if (ui != NULL) {
        if (IsKeyPressed(KEY_F1)) {
            ui->xray = !ui->xray;
        }
        ui->hover_choice = UI_HOVER_NONE;
    }

    if (g == NULL) {
        return raw;
    }

    if (g->phase == RUN_PHASE_REWARD) {
        RewardLayout layout;
        render_reward_layout(g, &layout);

        int32_t hover = UI_HOVER_NONE;
        for (int32_t i = 0; i < layout.card_count; ++i) {
            if (CheckCollisionPointRec(mouse, layout.cards[i])) {
                hover = i;
                break;
            }
        }
        if (hover == UI_HOVER_NONE && layout.has_repair &&
            CheckCollisionPointRec(mouse, layout.repair_rect)) {
            hover = UI_HOVER_REPAIR;
        }
        if (hover == UI_HOVER_NONE && layout.has_reroll &&
            CheckCollisionPointRec(mouse, layout.reroll_rect)) {
            hover = UI_HOVER_REROLL;
        }
        if (ui != NULL) {
            ui->hover_choice = hover;
        }

        if (left_pressed && hover != UI_HOVER_NONE) {
            raw.ui_click = true;
            if (hover >= 0 && hover < layout.card_count) {
                raw.pick_pressed = hover;
            } else if (hover == UI_HOVER_REPAIR) {
                raw.repair_pressed = true;
            } else if (hover == UI_HOVER_REROLL) {
                raw.reroll_pressed = true;
            }
        }

        if (IsKeyPressed(KEY_ONE)) {
            raw.pick_pressed = 0;
        } else if (IsKeyPressed(KEY_TWO)) {
            raw.pick_pressed = 1;
        } else if (IsKeyPressed(KEY_THREE)) {
            raw.pick_pressed = 2;
        }
        if (IsKeyPressed(KEY_FOUR)) {
            raw.repair_pressed = true;
        }
        if (IsKeyPressed(KEY_R)) {
            raw.reroll_pressed = true;
        }
        return raw;
    }

    if (g->phase == RUN_PHASE_VICTORY || g->phase == RUN_PHASE_GAME_OVER) {
        if (left_pressed) {
            raw.confirm_pressed = true;
            raw.ui_click = true;
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            raw.confirm_pressed = true;
        }
        return raw;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_E)) {
        raw.cyclone_pressed = true;
    }
    if (IsKeyPressed(KEY_SPACE)) {
        raw.gale_pressed = true;
    }
    return raw;
}
