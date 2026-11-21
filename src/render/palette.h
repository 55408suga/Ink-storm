#ifndef INK_RENDER_PALETTE_H
#define INK_RENDER_PALETTE_H

#include "game_ids.h"
#include "raylib.h"

typedef enum {
    PAL_SUMI_SCORCH = 0,
    PAL_SUMI_DARK,
    PAL_SUMI_HEAVY,
    PAL_SUMI_LIGHT,
    PAL_SUMI_CLEAR,
    PAL_GOFUN,
    PAL_SHU,
    PAL_SHU_HOT,
    PAL_AO,
    PAL_AO_HOT,
    PAL_KIN,
    PAL_KIN_HOT,
    PAL_BENGARA,
    PAL_PAPER_ACT1,
    PAL_PAPER_ACT2,
    PAL_PAPER_ACT3,
    PAL_COUNT
} PaletteId;

Color pal_color(PaletteId id);
Color pal_color_alpha(PaletteId id, float alpha);
Color pal_lerp(PaletteId a, PaletteId b, float t, float alpha);
Color pal_mul_tint(PaletteId a, PaletteId b, float t, float alpha);
Color pal_blank(void);
Color pal_white(void);
PaletteId pal_paper_for_act(int act_index);
PaletteId pal_axis_color(UpgradeAxis axis);

#endif
