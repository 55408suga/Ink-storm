#ifndef INK_RENDER_PRIMS_H
#define INK_RENDER_PRIMS_H

#include "render_internal.h"

/* Core 6 brush primitives (art-direction §8) */
void ink_stroke(RenderState *r, const Vector2 *pts, int n, BrushProfile prof, InkTone tone,
                float reveal);
void ink_wash(RenderState *r, Vector2 center, float rx, float ry, float rot, InkTone tone,
              int layers, uint32_t seed);
void ink_splatter(RenderState *r, Vector2 at, Vector2 dir, float energy, InkTone tone,
                  uint64_t *vfx_rng);
void ink_outline(RenderState *r, const Vector2 *ctrl, int n, bool closed, float w_max,
                 Vector2 shade_dir, uint32_t seed);
void ink_knockout(RenderState *r, const Vector2 *ctrl, int n, float pad_px, int act_index);
/* 紙抜きの色（幕の和紙と胡粉の中間、α 0.85）。スプライトの縁取りも同じ色を使う */
Color ink_knockout_color(int act_index);
/* 筆線の幅を両側 pad_px 広げた紙抜き。alpha は紙抜き本来の不透明度に掛ける */
void ink_stroke_knockout(RenderState *r, const Vector2 *pts, int n, BrushProfile prof, float pad_px,
                         int act_index, float alpha, float reveal);
void seal_stamp(RenderState *r, TextId glyph, Vector2 at, float size, SealStyle style, float t);

/* Organic polygon & UI brush helpers (all rlgl RL_TRIANGLES) */
void ink_fill_poly(RenderState *r, const Vector2 *pts, int n, Color c);
void ink_fill_panel(RenderState *r, Rectangle rect, PaletteId fill_pal, float fill_alpha,
                    PaletteId border_pal, float border_alpha, uint32_t seed);
void ink_arc_stroke(RenderState *r, Vector2 center, float radius, float a0, float a1,
                    BrushProfile prof, InkTone tone);
void ink_dashed_stroke(RenderState *r, Vector2 a, Vector2 b, float dash_len, float gap_len,
                       float w_max, InkTone tone, uint32_t seed);
void ink_teardrop(RenderState *r, Vector2 at, Vector2 dir, float len, float wid, Color c);
void ink_outline_tone(RenderState *r, const Vector2 *ctrl, int n, bool closed, float w_max,
                      Vector2 shade_dir, InkTone tone, uint32_t seed);
/* 2 本の折れ線の間を上下でグラデーション塗り（山の稜線から霧へ消える帯など） */
void ink_fill_gradient_strip(RenderState *r, const Vector2 *top, const Vector2 *bottom, int n,
                             Color top_c, Color bottom_c);
/* 頂点ごとの色で 2 本の折れ線の間を塗る（両端を霧に溶かす山並みなど） */
void ink_fill_strip_shaded(RenderState *r, const Vector2 *top, const Vector2 *bottom,
                           const Color *top_c, const Color *bottom_c, int n);
/* 中心色から外周色へ放射状に変化する楕円（霧・落ち影・発光） */
void ink_radial_glow(RenderState *r, Vector2 center, float rx, float ry, Color inner, Color outer);

#endif
