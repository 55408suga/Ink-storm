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

#endif
