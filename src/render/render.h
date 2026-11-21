#ifndef INK_RENDER_H
#define INK_RENDER_H

#include "game.h"
#include "raylib.h"

typedef struct {
    Shader post_shader;
    RenderTexture2D scene;
} RenderState;

RenderState render_init(bool use_post, int width, int height);
void render_draw(const RenderState *render, const GameState *game);
void render_close(const RenderState *render);

#endif
