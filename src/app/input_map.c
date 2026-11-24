#include "input_map.h"

#include "menu_nav.h"
#include "raylib.h"
#include "render.h"

#include <stddef.h>

static bool is_enter_pressed(void) { return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER); }

static bool any_nav_key_pressed(void)
{
    return IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_LEFT) ||
           IsKeyPressed(KEY_RIGHT);
}

static int32_t pick_hover(UiView *ui, int32_t hit, bool moved)
{
    const int32_t hover =
        menu_pointer_visible_hover(&ui->pointer, hit, moved, any_nav_key_pressed());
    ui->hover_choice = hover;
    return hover;
}

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

    if (g == NULL) {
        return raw;
    }
    /* The Result screen hover belongs to input_map_read_menu; clearing it here hid every hover. */
    if (g->phase == RUN_PHASE_VICTORY || g->phase == RUN_PHASE_GAME_OVER) {
        return raw;
    }
    if (ui != NULL) {
        ui->hover_choice = UI_HOVER_NONE;
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
        .lobby_cyclone = false,
        .lobby_gale = false,
        .lobby_brush_step = 0,
    };

    if (ui == NULL) {
        return out;
    }

    if (IsKeyPressed(KEY_F1)) {
        ui->xray = !ui->xray;
    }
    const bool moved = menu_pointer_track(&ui->pointer, mouse.x, mouse.y);

    if (ui->scene == UI_SCENE_TITLE_LOBBY) {
        LobbyLayout layout;
        render_lobby_layout(&layout);
        int32_t hit = UI_HOVER_NONE;
        for (int32_t i = 0; i < 4; ++i) {
            if (CheckCollisionPointRec(mouse, layout.items[i])) {
                hit = i;
                break;
            }
        }
        const bool on_brush = CheckCollisionPointRec(mouse, layout.brush_rect);
        const bool on_ui = hit >= 0 || on_brush ||
                           CheckCollisionPointRec(mouse, layout.title_rect) ||
                           CheckCollisionPointRec(mouse, layout.footer_rect) ||
                           CheckCollisionPointRec(mouse, layout.hint_rect);
        const bool was_stroking = ui->lobby_stroke_active;
        const bool stroking =
            menu_lobby_stroke_gate(&ui->lobby_stroke_active, left_pressed, left_down, on_ui);
        if (stroking || was_stroking) {
            hit = UI_HOVER_NONE;
        }
        ui->lobby_canvas_hover = !on_ui || stroking;
        const int32_t hover = pick_hover(ui, hit, moved);
        if (hover >= 0) {
            ui->lobby_cursor = hover;
        }

        if (IsKeyPressed(KEY_UP)) {
            ui->lobby_cursor = (ui->lobby_cursor + 3) % 4;
        } else if (IsKeyPressed(KEY_DOWN)) {
            ui->lobby_cursor = (ui->lobby_cursor + 1) % 4;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            out.lobby_brush_step = -1;
        } else if (IsKeyPressed(KEY_RIGHT)) {
            out.lobby_brush_step = 1;
        } else if (left_pressed && on_brush && !stroking) {
            const bool left_half = mouse.x < layout.brush_rect.x + layout.brush_rect.width * 0.5f;
            out.lobby_brush_step = left_half ? -1 : 1;
            out.ui_clicked = true;
        }

        int32_t pick = -1;
        if (left_pressed && hit >= 0 && !stroking) {
            pick = hit;
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
        } else {
            out.lobby_paint = stroking;
            out.lobby_cyclone = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_E);
            out.lobby_gale = IsKeyPressed(KEY_SPACE);
        }
        return out;
    }

    if (ui->scene == UI_SCENE_BRUSH_SELECT) {
        BrushSelectLayout layout;
        render_brush_select_layout(&layout);
        int32_t hit = UI_HOVER_NONE;
        for (int32_t i = 0; i < BRUSH_STYLE_COUNT; ++i) {
            if (CheckCollisionPointRec(mouse, layout.cards[i])) {
                hit = i;
                break;
            }
        }
        if (hit == UI_HOVER_NONE && CheckCollisionPointRec(mouse, layout.back_rect)) {
            hit = BRUSH_STYLE_COUNT;
        }
        const int32_t hover = pick_hover(ui, hit, moved);
        if (hover >= 0 && hover < BRUSH_STYLE_COUNT) {
            ui->selected_brush = (BrushStyleId)hover;
        }

        if (IsKeyPressed(KEY_LEFT)) {
            ui->selected_brush = (BrushStyleId)(((int)ui->selected_brush + BRUSH_STYLE_COUNT - 1) %
                                                BRUSH_STYLE_COUNT);
        } else if (IsKeyPressed(KEY_RIGHT)) {
            ui->selected_brush = (BrushStyleId)(((int)ui->selected_brush + 1) % BRUSH_STYLE_COUNT);
        }

        if (IsKeyPressed(KEY_ESCAPE) || (left_pressed && hit == BRUSH_STYLE_COUNT)) {
            out.action = MENU_ACT_BACK;
            out.ui_clicked = left_pressed;
        } else if (left_pressed && hit >= 0 && hit < BRUSH_STYLE_COUNT) {
            out.action = MENU_ACT_BRUSH_PICK;
            out.arg = hit;
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
        int32_t hit = UI_HOVER_NONE;
        for (int32_t i = 0; i < META_UPG_COUNT; ++i) {
            if (CheckCollisionPointRec(mouse, layout.upgrades[i])) {
                hit = i;
                break;
            }
        }
        if (hit == UI_HOVER_NONE) {
            for (int32_t s = 0; s < META_RELIC_UNLOCK_COUNT; ++s) {
                if (CheckCollisionPointRec(mouse, layout.relics[s])) {
                    hit = META_UPG_COUNT + s;
                    break;
                }
            }
        }
        if (hit == UI_HOVER_NONE && CheckCollisionPointRec(mouse, layout.back_rect)) {
            hit = total;
        }
        const int32_t hover = pick_hover(ui, hit, moved);
        if (hover >= 0) {
            ui->shop_cursor = hover;
        }

        if (IsKeyPressed(KEY_UP)) {
            ui->shop_cursor = menu_nav_bokuan(ui->shop_cursor, MENU_NAV_UP);
        } else if (IsKeyPressed(KEY_DOWN)) {
            ui->shop_cursor = menu_nav_bokuan(ui->shop_cursor, MENU_NAV_DOWN);
        } else if (IsKeyPressed(KEY_LEFT)) {
            ui->shop_cursor = menu_nav_bokuan(ui->shop_cursor, MENU_NAV_LEFT);
        } else if (IsKeyPressed(KEY_RIGHT)) {
            ui->shop_cursor = menu_nav_bokuan(ui->shop_cursor, MENU_NAV_RIGHT);
        }

        if (IsKeyPressed(KEY_ESCAPE) || (left_pressed && hit == total)) {
            out.action = MENU_ACT_BACK;
            out.ui_clicked = left_pressed;
            return out;
        }
        int32_t act_idx = -1;
        if (left_pressed && hit >= 0 && hit < total) {
            act_idx = hit;
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

        int32_t hit = UI_HOVER_NONE;
        for (int32_t i = 0; i < SETTINGS_ROW_COUNT; ++i) {
            if (CheckCollisionPointRec(mouse, layout.rows[i])) {
                hit = i;
                break;
            }
        }
        if (hit == UI_HOVER_NONE && CheckCollisionPointRec(mouse, layout.reset_hints_rect)) {
            hit = idx_hints;
        } else if (hit == UI_HOVER_NONE &&
                   CheckCollisionPointRec(mouse, layout.reset_defaults_rect)) {
            hit = idx_defaults;
        } else if (hit == UI_HOVER_NONE && CheckCollisionPointRec(mouse, layout.back_rect)) {
            hit = idx_back;
        }
        const int32_t hover = pick_hover(ui, hit, moved);
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

        if (IsKeyPressed(KEY_ESCAPE) || (left_pressed && hit == idx_back) ||
            (is_enter_pressed() && ui->settings_cursor == idx_back)) {
            out.action = MENU_ACT_BACK;
            out.ui_clicked = left_pressed;
            return out;
        }
        if ((left_pressed && hit == idx_hints) ||
            (is_enter_pressed() && ui->settings_cursor == idx_hints)) {
            out.action = MENU_ACT_SETTINGS_RESET_HINTS;
            out.ui_clicked = left_pressed;
            return out;
        }
        if ((left_pressed && hit == idx_defaults) ||
            (is_enter_pressed() && ui->settings_cursor == idx_defaults)) {
            out.action = MENU_ACT_SETTINGS_RESET_DEFAULTS;
            out.ui_clicked = left_pressed;
            return out;
        }

        if (left_pressed && hit >= 0 && hit < SETTINGS_ROW_COUNT) {
            out.ui_clicked = true;
            ui->settings_cursor = hit;
            if (CheckCollisionPointRec(mouse, layout.minus_rects[hit])) {
                out.action = MENU_ACT_SETTINGS_CHANGE;
                out.arg = hit;
                out.delta = -1;
            } else if (CheckCollisionPointRec(mouse, layout.plus_rects[hit]) || hit >= 3) {
                out.action = MENU_ACT_SETTINGS_CHANGE;
                out.arg = hit;
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
        int32_t hit = UI_HOVER_NONE;
        if (layout.has_endless && CheckCollisionPointRec(mouse, layout.endless_rect)) {
            hit = MENU_RESULT_ENDLESS;
        } else if (CheckCollisionPointRec(mouse, layout.lobby_rect)) {
            hit = MENU_RESULT_LOBBY;
        } else if (CheckCollisionPointRec(mouse, layout.retry_rect)) {
            hit = MENU_RESULT_RETRY;
        }
        if (!layout.has_endless && ui->result_cursor == MENU_RESULT_ENDLESS) {
            ui->result_cursor = MENU_RESULT_LOBBY;
        }
        const int32_t hover = pick_hover(ui, hit, moved);
        if (hover >= 0) {
            ui->result_cursor = hover;
        }
        if (IsKeyPressed(KEY_LEFT)) {
            ui->result_cursor =
                menu_nav_result(ui->result_cursor, layout.has_endless, MENU_NAV_LEFT);
        } else if (IsKeyPressed(KEY_RIGHT)) {
            ui->result_cursor =
                menu_nav_result(ui->result_cursor, layout.has_endless, MENU_NAV_RIGHT);
        }

        int32_t pick = -1;
        if (left_pressed && hit >= 0) {
            pick = hit;
            out.ui_clicked = true;
        } else if (is_enter_pressed()) {
            pick = ui->result_cursor;
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            pick = MENU_RESULT_LOBBY;
        }
        if (pick == MENU_RESULT_ENDLESS && layout.has_endless) {
            out.action = MENU_ACT_RESULT_ENDLESS;
        } else if (pick == MENU_RESULT_RETRY) {
            out.action = MENU_ACT_RESULT_RETRY;
        } else if (pick == MENU_RESULT_LOBBY) {
            out.action = MENU_ACT_RESULT_LOBBY;
        }
        return out;
    }

    if (ui->paused) {
        PauseLayout layout;
        render_pause_layout(&layout);
        if (ui->pause_confirm != PAUSE_CONFIRM_NONE) {
            int32_t hit = UI_HOVER_NONE;
            if (CheckCollisionPointRec(mouse, layout.confirm_yes)) {
                hit = 0;
            } else if (CheckCollisionPointRec(mouse, layout.confirm_no)) {
                hit = 1;
            }
            const int32_t hover = pick_hover(ui, hit, moved);
            if (hover >= 0) {
                ui->confirm_cursor = hover;
            }
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) {
                ui->confirm_cursor = 1 - ui->confirm_cursor;
            }
            if (IsKeyPressed(KEY_ESCAPE) || (left_pressed && hit == 1)) {
                out.action = MENU_ACT_CONFIRM_NO;
                out.ui_clicked = left_pressed;
            } else if (left_pressed && hit == 0) {
                out.action = MENU_ACT_CONFIRM_YES;
                out.ui_clicked = true;
            } else if (is_enter_pressed()) {
                out.action = ui->confirm_cursor == 0 ? MENU_ACT_CONFIRM_YES : MENU_ACT_CONFIRM_NO;
            }
            return out;
        }

        int32_t hit = UI_HOVER_NONE;
        for (int32_t i = 0; i < 4; ++i) {
            if (CheckCollisionPointRec(mouse, layout.items[i])) {
                hit = i;
                break;
            }
        }
        const int32_t hover = pick_hover(ui, hit, moved);
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
        } else if (left_pressed && hit >= 0) {
            pick = hit;
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
