#ifndef INK_UI_VIEW_H
#define INK_UI_VIEW_H

#include "meta.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum { UI_SCENE_RUN = 0, UI_SCENE_SHOWCASE } UiScene;

#define UI_HOVER_NONE (-1)
#define UI_HOVER_REPAIR 3
#define UI_HOVER_REROLL 4

typedef struct {
    UiScene scene;
    int32_t hover_choice;
    bool xray;
    uint8_t shake_scale;
    bool settled;
    MetaSettleBreakdown settle;
    int32_t crystals_after;
    int32_t showcase_page;
} UiView;

#endif
