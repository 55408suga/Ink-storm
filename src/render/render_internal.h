#ifndef INK_RENDER_INTERNAL_H
#define INK_RENDER_INTERNAL_H

#include "palette.h"
#include "render.h"
#include "render_rules.h"

#include <stdbool.h>
#include <stdint.h>

#define RENDER_FAULT_SHADER (UINT32_C(1) << 0)
#define RENDER_FAULT_FONT (UINT32_C(1) << 1)

typedef struct {
    float w_max;
    float w_min_ratio;
    float entry;
    float exit;
    float dry;
    uint32_t seed;
} BrushProfile;

typedef struct {
    PaletteId color;
    float alpha;
} InkTone;

typedef enum { SEAL_HAKUBUN = 0, SEAL_SHUBUN } SealStyle;

/* Blend mode tracking (counts switches for F1 budget <= 12) */
void render_set_blend(RenderState *r, int mode);
void render_reset_blend(RenderState *r);

/* Paper, background cache & terrain (paper.c) */
#define RENDER_BG_PAD 16
void render_paper_init(RenderState *r);
void render_paper_close(RenderState *r);
void render_paper_prepare_bg(RenderState *r, BgKey key);
void render_paper_draw_bg(RenderState *r, BgKey key);
void render_paper_draw_terrain(RenderState *r, const GameState *g, int act_index);
void render_paper_draw_weather(RenderState *r, const GameState *g, int act_index);
/* Sumi-e landscapes baked into the background cache (scenery.c) */
void render_scenery_draw(RenderState *r, BgKey key);

/* Fluid visualization (fluid_view.c) */
void render_fluid_init(RenderState *r, const RenderOptions *opt);
void render_fluid_close(RenderState *r);
bool render_fluid_gpu_active(const RenderState *r);
void render_fluid_update_and_draw(RenderState *r, const GameState *g);

/* Post-processing shader (post_fx.c) */
void render_post_init(RenderState *r, const RenderOptions *opt);
void render_post_close(RenderState *r);

/* Fonts & text measurement cache (fonts.c) */
void render_fonts_init(RenderState *r, const RenderOptions *opt);
void render_fonts_close(RenderState *r);
Vector2 render_measure_text(const RenderState *r, const char *text, float size, bool title_font);
void render_draw_text(const RenderState *r, const char *text, Vector2 pos, float size,
                      PaletteId color, float alpha, bool title_font);
void render_draw_text_id(const RenderState *r, TextId id, Vector2 pos, float size, PaletteId color,
                         float alpha, bool title_font);
void render_draw_text_centered(const RenderState *r, const char *text, Vector2 center, float size,
                               PaletteId color, float alpha, bool title_font);
void render_draw_number_fixed(const RenderState *r, const char *digits, Vector2 pos, float size,
                              PaletteId color, float alpha);
float render_measure_number_fixed(const RenderState *r, const char *digits, float size);
void render_draw_text_fit(const RenderState *r, const char *text, Vector2 center, float max_w,
                          float size, float min_size, PaletteId color, float alpha,
                          bool title_font);
float render_draw_text_wrapped(const RenderState *r, const char *text, Rectangle box, float size,
                               float min_size, PaletteId color, float alpha, bool title_font,
                               bool center);

/* Entities (draw_entities.c) */
void render_draw_entities(RenderState *r, const GameState *g, int act_index);

/* VFX (vfx.c) */
void render_vfx_draw_world(RenderState *r, const GameState *g, const UiView *ui);
void render_vfx_draw_flash(RenderState *r, const UiView *ui);
void render_vfx_draw_ui(RenderState *r, const GameState *g);
Vector2 render_vfx_shake_offset(const RenderState *r, const UiView *ui);

/* HUD & UI (hud.c, ui_reward.c, ui_result.c, ui_banner.c, ui_lobby.c, ui_shop.c, ui_pause.c,
 * ui_settings.c, xray.c) */
void render_draw_hud(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_brush_cursor(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_reward_ui(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_result_ui(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_banners(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_clear_banner(RenderState *r);
void render_draw_hint_strip(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_lobby_ui(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_brush_select_ui(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_bokuan_ui(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_pause_ui(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_settings_ui(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_showcase_overlay(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_xray(const RenderState *r, const GameState *g);

#endif
