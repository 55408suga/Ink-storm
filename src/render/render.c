#include "render.h"

#include "core.h"
#include "render_internal.h"
#include "rlgl.h"

#include <stddef.h>
#include <string.h>

void render_set_blend(RenderState *r, int mode)
{
    if (r == NULL || r->cur_blend == mode) {
        return;
    }
    if (r->cur_blend != BLEND_ALPHA) {
        EndBlendMode();
    }
    if (mode != BLEND_ALPHA) {
        BeginBlendMode(mode);
    }
    r->cur_blend = mode;
    r->metrics.blend_switches++;
}

void render_reset_blend(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    if (r->cur_blend != BLEND_ALPHA) {
        EndBlendMode();
        r->cur_blend = BLEND_ALPHA;
        r->metrics.blend_switches++;
    }
}

void render_init(RenderState *r, const RenderOptions *opt)
{
    if (r == NULL) {
        return;
    }
    memset(r, 0, sizeof *r);
    r->cur_blend = BLEND_ALPHA;
    r->bg_cached_act = -1;

    render_paper_init(r);
    render_fluid_init(r);
    render_post_init(r, opt);
    render_fonts_init(r, opt);
    render_reset_vfx(r, UINT64_C(1));
}

static void draw_world_layers(RenderState *r, const GameState *g, const UiView *ui)
{
    const int act = g->run.act_index;
    const Vector2 shake = render_vfx_shake_offset(r, ui);

    Camera2D cam = {0};
    cam.offset = shake;
    cam.zoom = 1.0f;

    BeginMode2D(cam);
    /* 1. Washi paper base (always drawn, with or without post shader) */
    render_paper_draw_base(r, act);
    /* 2. Cached edge background (Act 1 susuki + mist) */
    render_paper_draw_bg(r, act);
    /* 3. Terrain (none in Act 1 P0) */
    /* 4. Fluid (multiply ink + alpha fire/storm) */
    render_fluid_update_and_draw(r, g);
    /* 5 & 6. Ink orbs, projectiles, shrines, enemies, boss */
    render_draw_entities(r, g, act);
    /* 7. World VFX (sparks, spirals, gale strokes, lightning bolts) */
    render_vfx_draw_world(r, g);
    render_reset_blend(r);
    EndMode2D();
}

static void draw_ui_layers(RenderState *r, const GameState *g, const UiView *ui)
{
    /* 9. HUD & UI always drawn directly AFTER post shader so text stays sharp */
    render_vfx_draw_ui(r, g);
    render_draw_hud(r, g, ui);
    render_draw_banners(r, g, ui);
    render_draw_reward_ui(r, g, ui);
    render_draw_result_ui(r, g, ui);
    render_draw_showcase_overlay(r, g, ui);
    if (ui != NULL && ui->xray) {
        render_draw_xray(r, g);
    }
    render_reset_blend(r);
}

void render_draw(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL) {
        return;
    }
    const double t0 = GetTime();
    r->metrics = (RenderMetrics){0};
    r->cur_blend = BLEND_ALPHA;
    ++r->frames_drawn;

    if (r->post_shader.id != 0 && r->scene.id != 0) {
        BeginTextureMode(r->scene);
        draw_world_layers(r, g, ui);
        EndTextureMode();

        const Texture2D tex = r->scene.texture;
        const Rectangle src = {0.0f, 0.0f, (float)tex.width, -(float)tex.height};
        BeginDrawing();
        /* 8. Post shader (paper fiber, subtle pooling edge, warm vignette) */
        BeginShaderMode(r->post_shader);
        DrawTextureRec(tex, src, (Vector2){0.0f, 0.0f}, pal_white());
        EndShaderMode();
        draw_ui_layers(r, g, ui);
    } else {
        BeginDrawing();
        draw_world_layers(r, g, ui);
        draw_ui_layers(r, g, ui);
    }

    r->metrics.draw_ms = (GetTime() - t0) * 1000.0;
    r->last_metrics = r->metrics;
    EndDrawing();
}

void render_close(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    render_fonts_close(r);
    render_post_close(r);
    render_fluid_close(r);
    render_paper_close(r);
    memset(r, 0, sizeof *r);
}
