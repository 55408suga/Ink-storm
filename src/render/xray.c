#include "render_internal.h"

#include <inttypes.h>
#include <stdio.h>

void render_draw_xray(const RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL) {
        return;
    }
    const Color bg = pal_color_alpha(PAL_SUMI_SCORCH, 0.82f);
    const Color fg = pal_color(PAL_GOFUN);
    const Color acc = pal_color(PAL_KIN_HOT);

    DrawRectangle(12, 66, 360, 152, bg);
    DrawRectangleLines(12, 66, 360, 152, acc);

    const bool post_on = r->post_enabled && r->post_shader.id != 0;
    const char *font_lbl = !r->use_ja ? "ASCII" : (r->title_is_brush ? "JA-BRUSH" : "JA-MINCHO");

    DrawText("F1 X-RAY METRICS (P1)", 22, 74, 16, acc);
    DrawText(TextFormat("render_draw : %.2f ms  (budget < 4.0)", r->last_metrics.draw_ms), 22, 96,
             16, fg);
    DrawText(TextFormat("prim_verts  : %" PRIu32 "  (budget <= 60000)", r->last_metrics.prim_verts),
             22, 116, 16, fg);
    DrawText(
        TextFormat("blend_sw    : %" PRIu32 "  (budget <= 12)", r->last_metrics.blend_switches), 22,
        136, 16, fg);
    DrawText(TextFormat("fluid_solve : %.2f ms", g->fluid.stats.solve_ms), 22, 156, 16, fg);
    DrawText(TextFormat("entities    : %" PRIu32 "   dropped_ev: %" PRIu32,
                        ecs_alive_count(&g->ecs), g->dropped_events),
             22, 176, 16, fg);
    DrawText(TextFormat("post_shader : %s   font: %s", post_on ? "ON" : "DIRECT", font_lbl), 22,
             196, 16, fg);
}
