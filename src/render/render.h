#ifndef INK_RENDER_H
#define INK_RENDER_H

#include "game.h"
#include "raylib.h"
#include "ui_view.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool use_post;
    uint32_t fault_mask;
} RenderOptions;

typedef struct {
    Shader post_shader;
    RenderTexture2D scene;
    Texture2D fluid_tex;
    Color fluid_pixels[FLUID_N];
    uint64_t frames_drawn;
    uint64_t vfx_rng;
    uint64_t events_seen;
    float vfx_time;
} RenderState;

void render_init(RenderState *r, const RenderOptions *opt);
void render_reset_vfx(RenderState *r, uint64_t seed);
void render_consume_events(RenderState *r, const GameState *g);
void render_update_vfx(RenderState *r, const GameState *g, float dt);
void render_draw(RenderState *r, const GameState *g, const UiView *ui);
void render_close(RenderState *r);

#endif
