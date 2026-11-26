#ifndef INK_UI_VIEW_H
#define INK_UI_VIEW_H

#include "game_ids.h"
#include "meta.h"

#include <stdbool.h>
#include <stdint.h>

#define LOBBY_MENU_COUNT 5
#define GUIDE_SLIDE_COUNT 5

typedef enum {
    UI_SCENE_RUN = 0,
    UI_SCENE_SHOWCASE,
    UI_SCENE_TITLE_LOBBY,
    UI_SCENE_BRUSH_SELECT,
    UI_SCENE_BOKUAN,
    UI_SCENE_SETTINGS,
    UI_SCENE_GUIDE
} UiScene;

typedef enum {
    PAUSE_CONFIRM_NONE = 0,
    PAUSE_CONFIRM_RESTART,
    PAUSE_CONFIRM_LOBBY
} PauseConfirmKind;

typedef struct {
    float x, y;
    bool valid;
    bool hover_suppressed;
    int32_t context;
} UiPointer;

#define UI_HOVER_NONE (-1)
#define UI_HOVER_REPAIR 3
#define UI_HOVER_REROLL 4
#define UI_HOVER_CRYSTAL 5

typedef struct {
    UiScene scene;
    UiScene settings_return_scene;
    int32_t hover_choice;
    bool xray;
    uint8_t shake_scale;
    bool settled;
    MetaSettleBreakdown settle;
    int32_t crystals_after;
    int32_t showcase_page;
    int32_t showcase_subpage;

    /* Phase 4 additions: Lobby / Brush Select / Bokuan / Settings / Pause / Hints */
    BrushStyleId selected_brush;
    int32_t lobby_cursor;
    int32_t shop_cursor;
    int32_t settings_cursor;
    bool paused;
    int32_t pause_cursor;
    PauseConfirmKind pause_confirm;
    int32_t confirm_cursor;
    HintId active_hint;
    float active_hint_t;
    MetaState meta_view;

    /* Phase 5 additions: deferred Victory settlement, expanded settings, feedback */
    bool settle_preview;
    bool reduce_flash;
    uint8_t cursor_scale;
    float settings_notice_t;
    int32_t settings_notice_kind;

    /* Menu pointer tracking, Result keyboard cursor, Lobby test-stroke gate, Guide slide */
    UiPointer pointer;
    int32_t result_cursor;
    int32_t guide_page;
    bool lobby_stroke_active;
    bool lobby_canvas_hover;
} UiView;

#endif
