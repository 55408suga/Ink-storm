#include "palette.h"

#include "core.h"
#include "game.h"

static const Color k_palette[PAL_COUNT] = {
    [PAL_SUMI_SCORCH] = {0x1a, 0x17, 0x14, 0xff}, [PAL_SUMI_DARK] = {0x2b, 0x27, 0x23, 0xff},
    [PAL_SUMI_HEAVY] = {0x4a, 0x44, 0x3d, 0xff},  [PAL_SUMI_LIGHT] = {0x8c, 0x85, 0x7a, 0xff},
    [PAL_SUMI_CLEAR] = {0xc9, 0xc1, 0xb2, 0xff},  [PAL_GOFUN] = {0xf7, 0xf2, 0xe6, 0xff},
    [PAL_SHU] = {0xc8, 0x44, 0x2b, 0xff},         [PAL_SHU_HOT] = {0xf0, 0x8a, 0x3c, 0xff},
    [PAL_AO] = {0x2f, 0x6f, 0xb3, 0xff},          [PAL_AO_HOT] = {0x9f, 0xd3, 0xf2, 0xff},
    [PAL_KIN] = {0xc9, 0xa0, 0x4a, 0xff},         [PAL_KIN_HOT] = {0xf1, 0xd2, 0x7a, 0xff},
    [PAL_BENGARA] = {0x8e, 0x3b, 0x2c, 0xff},     [PAL_PAPER_ACT1] = {0xed, 0xe4, 0xd0, 0xff},
    [PAL_PAPER_ACT2] = {0xea, 0xdc, 0xc4, 0xff},  [PAL_PAPER_ACT3] = {0xda, 0xd8, 0xcf, 0xff},
};

static unsigned char to_u8(float v)
{
    const float clamped = ink_clampf(v, 0.0f, 255.0f);
    return (unsigned char)(clamped + 0.5f);
}

Color pal_color(PaletteId id)
{
    if ((unsigned)id >= (unsigned)PAL_COUNT) {
        return k_palette[PAL_SUMI_DARK];
    }
    return k_palette[id];
}

Color pal_color_alpha(PaletteId id, float alpha)
{
    Color c = pal_color(id);
    c.a = to_u8(ink_clampf(alpha, 0.0f, 1.0f) * 255.0f);
    return c;
}

Color pal_lerp(PaletteId a, PaletteId b, float t, float alpha)
{
    const Color ca = pal_color(a);
    const Color cb = pal_color(b);
    const float u = ink_clampf(t, 0.0f, 1.0f);
    return (Color){
        to_u8((float)ca.r + ((float)cb.r - (float)ca.r) * u),
        to_u8((float)ca.g + ((float)cb.g - (float)ca.g) * u),
        to_u8((float)ca.b + ((float)cb.b - (float)ca.b) * u),
        to_u8(ink_clampf(alpha, 0.0f, 1.0f) * 255.0f),
    };
}

Color pal_mul_tint(PaletteId a, PaletteId b, float t, float alpha)
{
    const Color c = pal_lerp(a, b, t, 1.0f);
    const float k = ink_clampf(alpha, 0.0f, 1.0f);
    return (Color){
        to_u8(255.0f + ((float)c.r - 255.0f) * k),
        to_u8(255.0f + ((float)c.g - 255.0f) * k),
        to_u8(255.0f + ((float)c.b - 255.0f) * k),
        255,
    };
}

Color pal_blank(void) { return (Color){0, 0, 0, 0}; }

Color pal_white(void) { return (Color){255, 255, 255, 255}; }

PaletteId pal_paper_for_act(int act_index)
{
    const int stage_act = act_index >= 0 ? (act_index % GAME_MAIN_ACT_COUNT) : 0;
    if (stage_act == 1) {
        return PAL_PAPER_ACT2;
    }
    if (stage_act == 2) {
        return PAL_PAPER_ACT3;
    }
    return PAL_PAPER_ACT1;
}

PaletteId pal_axis_color(UpgradeAxis axis)
{
    switch (axis) {
    case AXIS_BLAZE:
        return PAL_SHU;
    case AXIS_THUNDER:
        return PAL_AO;
    case AXIS_GALE:
        return PAL_SUMI_HEAVY;
    case AXIS_INKWAY:
        return PAL_KIN;
    case AXIS_COMMON:
    default:
        return PAL_SUMI_DARK;
    }
}
