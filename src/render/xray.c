#include "render_internal.h"

#include "core.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

typedef struct {
    int ink_cells;
    int fire_cells;
    int storm_cells;
    int react_cells;
} XrayCellCounts;

static XrayCellCounts count_element_cells(const FluidState *f)
{
    XrayCellCounts c = {0};
    const float *ink_ch = fluid_channel_const(f, 0);
    const float *fire_ch = fluid_channel_const(f, 1);
    const float *storm_ch = fluid_channel_const(f, 2);
    if (ink_ch == NULL || fire_ch == NULL || storm_ch == NULL) {
        return c;
    }
    for (int y = 1; y < FLUID_H - 1; ++y) {
        const int row = y * FLUID_W;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            const int i = row + x;
            if (f->solid[i] != 0) {
                continue;
            }
            const float ink = ink_ch[i];
            const float fire = fire_ch[i];
            const float storm = storm_ch[i];
            const bool has_ink = ink > 0.20f;
            const bool has_fire = fire > 0.14f;
            const bool has_storm = storm > 0.14f;
            if (has_ink) {
                ++c.ink_cells;
            }
            if (has_fire) {
                ++c.fire_cells;
            }
            if (has_storm) {
                ++c.storm_cells;
            }
            if ((has_ink && has_fire) || (ink > 0.24f && has_storm) ||
                (fire > 0.30f && storm > 0.30f) || (ink > 0.30f && fire > 0.30f && storm > 0.30f)) {
                ++c.react_cells;
            }
        }
    }
    return c;
}

static void draw_solid_borders(const FluidState *f)
{
    const Color solid_col = pal_color_alpha(PAL_KIN, 0.50f);
    const int cell_px = (int)FLUID_CELL_PX;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        const int row = y * FLUID_W;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            if (f->solid[row + x] != 0) {
                DrawRectangleLines(x * cell_px, y * cell_px, cell_px, cell_px, solid_col);
            }
        }
    }
}

static void draw_velocity_and_vorticity(const FluidState *f)
{
    /* Batch by primitive topology so rlgl keeps all RL_LINES in one batch and all
     * RL_TRIANGLES (arrow tips) in a second batch instead of flushing per cell. */
    const float vort_thresh = f->cfg.vorticity_thresh;

    /* Pass 1 (RL_LINES): solid cell borders, velocity vectors, and vorticity rings. */
    draw_solid_borders(f);

    for (int y = 4; y < FLUID_H - 1; y += 4) {
        const int row = y * FLUID_W;
        const float cy = ((float)y + 0.5f) * FLUID_CELL_PX;
        for (int x = 4; x < FLUID_W - 1; x += 4) {
            const int idx = row + x;
            if (f->solid[idx] != 0) {
                continue;
            }
            const float cx = ((float)x + 0.5f) * FLUID_CELL_PX;
            const float u = f->u[idx];
            const float v = f->v[idx];
            const float speed = hypotf(u, v);
            if (speed > 0.5f) {
                const float inv = 1.0f / speed;
                const float len = ink_clampf(4.0f + speed * 0.45f, 4.0f, 26.0f);
                const Vector2 start = {cx, cy};
                const Vector2 tip = {cx + u * inv * len, cy + v * inv * len};
                const float t = ink_clampf((speed - 0.5f) / 35.0f, 0.0f, 1.0f);
                const Color vcol = pal_lerp(PAL_GOFUN, PAL_KIN_HOT, t, 0.48f + 0.36f * t);
                DrawLineV(start, tip, vcol);
            }

            const float vort = 0.5f * ((f->v[idx + 1] - f->v[idx - 1]) -
                                       (f->u[idx + FLUID_W] - f->u[idx - FLUID_W]));
            const float av = fabsf(vort);
            if (av >= vort_thresh) {
                const float vt = ink_clampf((av - vort_thresh) / 12.0f, 0.0f, 1.0f);
                const float rad = 4.0f + 9.0f * vt;
                const float alpha = 0.35f + 0.45f * vt;
                const PaletteId pid = (vort > 0.0f) ? PAL_SHU : PAL_AO;
                DrawPolyLines((Vector2){cx, cy}, 10, rad, 0.0f, pal_color_alpha(pid, alpha));
            }
        }
    }

    /* Pass 2 (RL_QUADS): velocity vector arrow tip dots (4 verts/dot instead of 108). */
    for (int y = 4; y < FLUID_H - 1; y += 4) {
        const int row = y * FLUID_W;
        const float cy = ((float)y + 0.5f) * FLUID_CELL_PX;
        for (int x = 4; x < FLUID_W - 1; x += 4) {
            const int idx = row + x;
            if (f->solid[idx] != 0) {
                continue;
            }
            const float u = f->u[idx];
            const float v = f->v[idx];
            const float speed = hypotf(u, v);
            if (speed > 0.5f) {
                const float cx = ((float)x + 0.5f) * FLUID_CELL_PX;
                const float inv = 1.0f / speed;
                const float len = ink_clampf(4.0f + speed * 0.45f, 4.0f, 26.0f);
                const Vector2 tip = {cx + u * inv * len - 1.5f, cy + v * inv * len - 1.5f};
                const float t = ink_clampf((speed - 0.5f) / 35.0f, 0.0f, 1.0f);
                const Color vcol = pal_lerp(PAL_GOFUN, PAL_KIN_HOT, t, 0.48f + 0.36f * t);
                DrawRectangleV(tip, (Vector2){3.0f, 3.0f}, vcol);
            }
        }
    }
}

void render_draw_xray(const RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL) {
        return;
    }

    draw_velocity_and_vorticity(&g->fluid);

    const XrayCellCounts cells = count_element_cells(&g->fluid);

    const Color bg = pal_color_alpha(PAL_SUMI_SCORCH, 0.84f);
    const Color fg = pal_color(PAL_GOFUN);
    const Color acc = pal_color(PAL_KIN_HOT);

    DrawRectangle(12, 66, 560, 158, bg);
    DrawRectangleLines(12, 66, 560, 158, acc);

    const bool post_on = r->post_enabled && r->post_shader.id != 0;
    const char *font_lbl = !r->use_ja ? "ASCII" : (r->title_is_brush ? "JA-BRUSH" : "JA-MINCHO");

    DrawText("F1 X-RAY", 22, 74, 16, acc);
    DrawText(TextFormat("render_draw : %.2f ms  (budget < 4.0)", r->last_metrics.draw_ms), 22, 96,
             16, fg);
    DrawText(TextFormat("prim_verts  : %" PRIu32 " (<= 60000)   blend_sw: %" PRIu32 " (<= 12)",
                        r->last_metrics.prim_verts, r->last_metrics.blend_switches),
             22, 116, 16, fg);
    DrawText(TextFormat("fluid_solve : %.2f ms  (< 2.0)   max_v: %.1f  max_vort: %.1f",
                        g->fluid.stats.solve_ms, g->fluid.stats.max_vel, g->fluid.stats.max_vort),
             22, 136, 16, fg);
    DrawText(TextFormat("cells       : ink=%d  fire=%d  storm=%d  react=%d", cells.ink_cells,
                        cells.fire_cells, cells.storm_cells, cells.react_cells),
             22, 156, 16, fg);
    DrawText(TextFormat("entities    : %" PRIu32 "  dropped_ev: %" PRIu32 "  act:%d-%d",
                        ecs_alive_count(&g->ecs), g->dropped_events, g->run.act_index + 1,
                        g->run.wave_index + 1),
             22, 176, 16, fg);
    DrawText(TextFormat("post_shader : %s   fluid_shader: %s   font: %s", post_on ? "ON" : "DIRECT",
                        render_fluid_gpu_active(r) ? "GPU" : "CPU", font_lbl),
             22, 196, 16, fg);
}
