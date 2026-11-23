#include "input_map.h"

#include "raylib.h"
#include "render.h"

#include <stddef.h>

static bool is_enter_pressed(void) { return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER); }

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
        if (hover == UI_HOVER_NONE && layout.has_crystal &&
            CheckCollisionPointRec(mouse, layout.crystal_rect)) {
            hover = UI_HOVER_CRYSTAL;
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
            } else if (hover == UI_HOVER_CRYSTAL) {
                raw.crystal_pressed = true;
            } else if (hover == UI_HOVER_REROLL) {
                raw.reroll_pressed = true;
            }
        }

        if (g->run.reward.kind == REWARD_KIND_FALLBACK) {
            if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_FOUR)) {
                raw.repair_pressed = true;
            } else if (IsKeyPressed(KEY_TWO) && layout.has_crystal) {
                raw.crystal_pressed = true;
            }
        } else {
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
        }
        return raw;
    }

    if (g->phase == RUN_PHASE_VICTORY || g->phase == RUN_PHASE_GAME_OVER) {
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

MenuInput input_map_read_menu(const GameState *g, UiView *ui)
{
    const Vector2 mouse = GetMousePosition();
    const bool left_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    const bool left_pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    MenuInput out = {
        .action = MENU_ACT_NONE,
        .arg = 0,
        .delta = 0,
        .mouse_x = mouse.x,
        .mouse_y = mouse.y,
        .left_down = left_down,
        .ui_clicked = false,
        .lobby_paint = false,
    };

    if (ui == NULL) {
        return out;
    }

    if (IsKeyPressed(KEY_F1)) {
        ui->xray = !ui->xray;
    }

    if (ui->scene == UI_SCENE_TITLE_LOBBY) {
        LobbyLayout layout;
        render_lobby_layout(&layout);
        int32_t hover = UI_HOVER_NONE;
        for (int32_t i = 0; i < 4; ++i) {
            if (CheckCollisionPointRec(mouse, layout.items[i])) {
                hover = i;
                break;
            }
        }
        ui->hover_choice = hover;
        if (hover >= 0) {
            ui->lobby_cursor = hover;
        }

        if (IsKeyPressed(KEY_UP)) {
            ui->lobby_cursor = (ui->lobby_cursor + 3) % 4;
        } else if (IsKeyPressed(KEY_DOWN)) {
            ui->lobby_cursor = (ui->lobby_cursor + 1) % 4;
        }

        int32_t pick = -1;
        if (left_pressed && hover >= 0) {
            pick = hover;
            out.ui_clicked = true;
        } else if (is_enter_pressed()) {
            pick = ui->lobby_cursor;
        } else if (IsKeyPressed(KEY_ONE)) {
            pick = 0;
        } else if (IsKeyPressed(KEY_TWO)) {
            pick = 1;
        } else if (IsKeyPressed(KEY_THREE)) {
            pick = 2;
        } else if (IsKeyPressed(KEY_FOUR) || IsKeyPressed(KEY_ESCAPE)) {
            pick = 3;
        }

        if (pick == 0) {
            out.action = MENU_ACT_LOBBY_SORTIE;
        } else if (pick == 1) {
            out.action = MENU_ACT_LOBBY_BOKUAN;
        } else if (pick == 2) {
            out.action = MENU_ACT_LOBBY_SETTINGS;
        } else if (pick == 3) {
            out.action = MENU_ACT_LOBBY_QUIT;
        } else if (left_down && hover == UI_HOVER_NONE) {
            out.lobby_paint = true;
        }
        return out;
    }

    if (ui->scene == UI_SCENE_BRUSH_SELECT) {
        BrushSelectLayout layout;
        render_brush_select_layout(&layout);
        int32_t hover = UI_HOVER_NONE;
        for (int32_t i = 0; i < BRUSH_STYLE_COUNT; ++i) {
            if (CheckCollisionPointRec(mouse, layout.cards[i])) {
                hover = i;
                break;
            }
        }
        if (hover == UI_HOVER_NONE && CheckCollisionPointRec(mouse, layout.back_rect)) {
            hover = BRUSH_STYLE_COUNT;
        }
        ui->hover_choice = hover;
        if (hover >= 0 && hover < BRUSH_STYLE_COUNT) {
            ui->selected_brush = (BrushStyleId)hover;
        }

        if (IsKeyPressed(KEY_LEFT)) {
            ui->selected_brush = (BrushStyleId)(((int)ui->selected_brush + BRUSH_STYLE_COUNT - 1) %
                                                BRUSH_STYLE_COUNT);
        } else if (IsKeyPressed(KEY_RIGHT)) {
            ui->selected_brush = (BrushStyleId)(((int)ui->selected_brush + 1) % BRUSH_STYLE_COUNT);
        }

        if (IsKeyPressed(KEY_ESCAPE) || (left_pressed && hover == BRUSH_STYLE_COUNT)) {
            out.action = MENU_ACT_BACK;
            out.ui_clicked = left_pressed;
        } else if (left_pressed && hover >= 0 && hover < BRUSH_STYLE_COUNT) {
            out.action = MENU_ACT_BRUSH_PICK;
            out.arg = hover;
            out.ui_clicked = true;
        } else if (IsKeyPressed(KEY_ONE)) {
            out.action = MENU_ACT_BRUSH_PICK;
            out.arg = BRUSH_STYLE_HEAVY;
        } else if (IsKeyPressed(KEY_TWO)) {
            out.action = MENU_ACT_BRUSH_PICK;
            out.arg = BRUSH_STYLE_EMBER;
        } else if (IsKeyPressed(KEY_THREE)) {
            out.action = MENU_ACT_BRUSH_PICK;
            out.arg = BRUSH_STYLE_QUILL;
        } else if (is_enter_pressed()) {
            out.action = MENU_ACT_BRUSH_PICK;
            out.arg = (int32_t)ui->selected_brush;
        }
        return out;
    }

    if (ui->scene == UI_SCENE_BOKUAN) {
        BokuanLayout layout;
        render_bokuan_layout(&layout);
        const int32_t total = META_UPG_COUNT + META_RELIC_UNLOCK_COUNT;
        int32_t hover = UI_HOVER_NONE;
        for (int32_t i = 0; i < META_UPG_COUNT; ++i) {
            if (CheckCollisionPointRec(mouse, layout.upgrades[i])) {
                hover = i;
                break;
            }
        }
        if (hover == UI_HOVER_NONE) {
            for (int32_t s = 0; s < META_RELIC_UNLOCK_COUNT; ++s) {
                if (CheckCollisionPointRec(mouse, layout.relics[s])) {
                    hover = META_UPG_COUNT + s;
                    break;
                }
            }
        }
        if (hover == UI_HOVER_NONE && CheckCollisionPointRec(mouse, layout.back_rect)) {
            hover = total;
        }
        ui->hover_choice = hover;
        if (hover >= 0) {
            ui->shop_cursor = hover;
        }

        if (IsKeyPressed(KEY_UP)) {
            ui->shop_cursor = (ui->shop_cursor + total) % (total + 1);
        } else if (IsKeyPressed(KEY_DOWN)) {
            ui->shop_cursor = (ui->shop_cursor + 1) % (total + 1);
        } else if (IsKeyPressed(KEY_LEFT)) {
            if (ui->shop_cursor >= META_UPG_COUNT && ui->shop_cursor < total) {
                ui->shop_cursor = (ui->shop_cursor - META_UPG_COUNT) % META_UPG_COUNT;
            }
        } else if (IsKeyPressed(KEY_RIGHT)) {
            if (ui->shop_cursor < META_UPG_COUNT) {
                ui->shop_cursor = META_UPG_COUNT + (ui->shop_cursor < 3 ? ui->shop_cursor * 2 : 4);
            }
        }

        if (IsKeyPressed(KEY_ESCAPE) || (left_pressed && hover == total)) {
            out.action = MENU_ACT_BACK;
            out.ui_clicked = left_pressed;
            return out;
        }
        int32_t act_idx = -1;
        if (left_pressed && hover >= 0 && hover < total) {
            act_idx = hover;
            out.ui_clicked = true;
        } else if (is_enter_pressed()) {
            if (ui->shop_cursor == total) {
                out.action = MENU_ACT_BACK;
                return out;
            }
            act_idx = ui->shop_cursor;
        }
        if (act_idx >= 0 && act_idx < META_UPG_COUNT) {
            out.action = MENU_ACT_SHOP_BUY_UPG;
            out.arg = act_idx;
        } else if (act_idx >= META_UPG_COUNT && act_idx < total) {
            out.action = MENU_ACT_SHOP_BUY_RELIC;
            out.arg = act_idx - META_UPG_COUNT;
        }
        return out;
    }

    if (ui->scene == UI_SCENE_SETTINGS) {
        SettingsLayout layout;
        render_settings_layout(&layout);
        const int32_t focus_count =
            SETTINGS_ROW_COUNT + 3; /* 0..8 rows, 9 hints, 10 def, 11 back */
        const int32_t idx_hints = SETTINGS_ROW_COUNT;
        const int32_t idx_defaults = SETTINGS_ROW_COUNT + 1;
        const int32_t idx_back = SETTINGS_ROW_COUNT + 2;

        int32_t hover = UI_HOVER_NONE;
        for (int32_t i = 0; i < SETTINGS_ROW_COUNT; ++i) {
            if (CheckCollisionPointRec(mouse, layout.rows[i])) {
                hover = i;
                break;
            }
        }
        if (hover == UI_HOVER_NONE && CheckCollisionPointRec(mouse, layout.reset_hints_rect)) {
            hover = idx_hints;
        } else if (hover == UI_HOVER_NONE &&
                   CheckCollisionPointRec(mouse, layout.reset_defaults_rect)) {
            hover = idx_defaults;
        } else if (hover == UI_HOVER_NONE && CheckCollisionPointRec(mouse, layout.back_rect)) {
            hover = idx_back;
        }
        ui->hover_choice = hover;
        if (hover >= 0) {
            ui->settings_cursor = hover;
        }

        if (IsKeyPressed(KEY_UP)) {
            ui->settings_cursor = (ui->settings_cursor + focus_count - 1) % focus_count;
        } else if (IsKeyPressed(KEY_DOWN)) {
            ui->settings_cursor = (ui->settings_cursor + 1) % focus_count;
        } else if (ui->settings_cursor == idx_hints && IsKeyPressed(KEY_RIGHT)) {
            ui->settings_cursor = idx_defaults;
        } else if (ui->settings_cursor == idx_defaults && IsKeyPressed(KEY_LEFT)) {
            ui->settings_cursor = idx_hints;
        }

        if (IsKeyPressed(KEY_ESCAPE) || (left_pressed && hover == idx_back) ||
            (is_enter_pressed() && ui->settings_cursor == idx_back)) {
            out.action = MENU_ACT_BACK;
            out.ui_clicked = left_pressed;
            return out;
        }
        if ((left_pressed && hover == idx_hints) ||
            (is_enter_pressed() && ui->settings_cursor == idx_hints)) {
            out.action = MENU_ACT_SETTINGS_RESET_HINTS;
            out.ui_clicked = left_pressed;
            return out;
        }
        if ((left_pressed && hover == idx_defaults) ||
            (is_enter_pressed() && ui->settings_cursor == idx_defaults)) {
            out.action = MENU_ACT_SETTINGS_RESET_DEFAULTS;
            out.ui_clicked = left_pressed;
            return out;
        }

        if (left_pressed && hover >= 0 && hover < SETTINGS_ROW_COUNT) {
            out.ui_clicked = true;
            if (CheckCollisionPointRec(mouse, layout.minus_rects[hover])) {
                out.action = MENU_ACT_SETTINGS_CHANGE;
                out.arg = hover;
                out.delta = -1;
            } else if (CheckCollisionPointRec(mouse, layout.plus_rects[hover]) || hover >= 3) {
                out.action = MENU_ACT_SETTINGS_CHANGE;
                out.arg = hover;
                out.delta = 1;
            }
            return out;
        }

        if (ui->settings_cursor >= 0 && ui->settings_cursor < SETTINGS_ROW_COUNT) {
            if (IsKeyPressed(KEY_LEFT)) {
                out.action = MENU_ACT_SETTINGS_CHANGE;
                out.arg = ui->settings_cursor;
                out.delta = -1;
            } else if (IsKeyPressed(KEY_RIGHT) || is_enter_pressed()) {
                out.action = MENU_ACT_SETTINGS_CHANGE;
                out.arg = ui->settings_cursor;
                out.delta = 1;
            }
        }
        return out;
    }

    /* UI_SCENE_RUN */
    if (g != NULL && (g->phase == RUN_PHASE_VICTORY || g->phase == RUN_PHASE_GAME_OVER)) {
        const bool show_endless =
            (g->phase == RUN_PHASE_VICTORY && !g->run.run_settled && g->run.endless_loop == 0);
        ResultLayout layout;
        render_result_layout(show_endless, &layout);
        int32_t hover = UI_HOVER_NONE;
        if (layout.has_endless && CheckCollisionPointRec(mouse, layout.endless_rect)) {
            hover = 2;
        } else if (CheckCollisionPointRec(mouse, layout.lobby_rect)) {
            hover = 0;
        } else if (CheckCollisionPointRec(mouse, layout.retry_rect)) {
            hover = 1;
        }
        ui->hover_choice = hover;
        if (left_pressed && hover == 2 && layout.has_endless) {
            out.action = MENU_ACT_RESULT_ENDLESS;
            out.ui_clicked = true;
        } else if (left_pressed && hover == 0) {
            out.action = MENU_ACT_RESULT_LOBBY;
            out.ui_clicked = true;
        } else if (left_pressed && hover == 1) {
            out.action = MENU_ACT_RESULT_RETRY;
            out.ui_clicked = true;
        } else if (is_enter_pressed() || IsKeyPressed(KEY_ESCAPE)) {
            out.action = MENU_ACT_RESULT_LOBBY;
        }
        return out;
    }

    if (ui->paused) {
        PauseLayout layout;
        render_pause_layout(&layout);
        if (ui->pause_confirm != PAUSE_CONFIRM_NONE) {
            int32_t hover = UI_HOVER_NONE;
            if (CheckCollisionPointRec(mouse, layout.confirm_yes)) {
                hover = 0;
            } else if (CheckCollisionPointRec(mouse, layout.confirm_no)) {
                hover = 1;
            }
            ui->hover_choice = hover;
            if (hover >= 0) {
                ui->confirm_cursor = hover;
            }
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) {
                ui->confirm_cursor = 1 - ui->confirm_cursor;
            }
            if (IsKeyPressed(KEY_ESCAPE) || (left_pressed && hover == 1)) {
                out.action = MENU_ACT_CONFIRM_NO;
                out.ui_clicked = left_pressed;
            } else if (left_pressed && hover == 0) {
                out.action = MENU_ACT_CONFIRM_YES;
                out.ui_clicked = true;
            } else if (is_enter_pressed()) {
                out.action = ui->confirm_cursor == 0 ? MENU_ACT_CONFIRM_YES : MENU_ACT_CONFIRM_NO;
            }
            return out;
        }

        int32_t hover = UI_HOVER_NONE;
        for (int32_t i = 0; i < 4; ++i) {
            if (CheckCollisionPointRec(mouse, layout.items[i])) {
                hover = i;
                break;
            }
        }
        ui->hover_choice = hover;
        if (hover >= 0) {
            ui->pause_cursor = hover;
        }
        if (IsKeyPressed(KEY_UP)) {
            ui->pause_cursor = (ui->pause_cursor + 3) % 4;
        } else if (IsKeyPressed(KEY_DOWN)) {
            ui->pause_cursor = (ui->pause_cursor + 1) % 4;
        }

        int32_t pick = -1;
        if (IsKeyPressed(KEY_ESCAPE)) {
            pick = 0;
        } else if (left_pressed && hover >= 0) {
            pick = hover;
            out.ui_clicked = true;
        } else if (is_enter_pressed()) {
            pick = ui->pause_cursor;
        } else if (IsKeyPressed(KEY_ONE)) {
            pick = 0;
        } else if (IsKeyPressed(KEY_TWO)) {
            pick = 1;
        } else if (IsKeyPressed(KEY_THREE)) {
            pick = 2;
        } else if (IsKeyPressed(KEY_FOUR)) {
            pick = 3;
        }

        if (pick == 0) {
            out.action = MENU_ACT_PAUSE_RESUME;
        } else if (pick == 1) {
            out.action = MENU_ACT_PAUSE_RESTART;
        } else if (pick == 2) {
            out.action = MENU_ACT_PAUSE_SETTINGS;
        } else if (pick == 3) {
            out.action = MENU_ACT_PAUSE_LOBBY;
        }
        return out;
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
        out.action = MENU_ACT_TOGGLE_PAUSE;
    }
    return out;
}
