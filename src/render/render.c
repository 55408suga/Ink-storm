#include "render.h"

#include "core.h"
#include "rlgl.h"

#include <inttypes.h>
#include <stddef.h>
#include <string.h>

#define RENDER_FAULT_SHADER (UINT32_C(1) << 0)

static const char post_fragment[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 color = texture(texture0, fragTexCoord) * fragColor;\n"
    "    finalColor = vec4(color.rgb * vec3(1.0, 0.99, 0.97), color.a);\n"
    "}\n";

void render_init(RenderState *r, const RenderOptions *opt)
{
    if (r == NULL) {
        return;
    }
    memset(r, 0, sizeof *r);

    Image img = GenImageColor(FLUID_W, FLUID_H, BLANK);
    r->fluid_tex = LoadTextureFromImage(img);
    UnloadImage(img);
    if (r->fluid_tex.id != 0) {
        SetTextureFilter(r->fluid_tex, TEXTURE_FILTER_BILINEAR);
    }

    const bool use_post = opt == NULL || opt->use_post;
    if (!use_post) {
        TraceLog(LOG_INFO, "INKSTORM: post shader disabled; drawing directly");
        return;
    }

    const bool force_shader_fault = opt != NULL && (opt->fault_mask & RENDER_FAULT_SHADER) != 0;
    Shader shader = {0};
    if (!force_shader_fault) {
        shader = LoadShaderFromMemory(NULL, post_fragment);
    }
    /* raylib can return its valid default shader when a custom shader fails. */
    if (force_shader_fault || !IsShaderValid(shader) || shader.id == rlGetShaderIdDefault()) {
        if (shader.id != 0) {
            UnloadShader(shader);
        }
        TraceLog(LOG_WARNING, "INKSTORM: post shader unavailable; drawing directly");
        return;
    }

    RenderTexture2D scene = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
    if (!IsRenderTextureValid(scene)) {
        UnloadRenderTexture(scene);
        UnloadShader(shader);
        TraceLog(LOG_WARNING, "INKSTORM: render texture unavailable; drawing directly");
        return;
    }

    TraceLog(LOG_INFO, "INKSTORM: post shader enabled");
    r->post_shader = shader;
    r->scene = scene;
}

void render_reset_vfx(RenderState *r, uint64_t seed)
{
    if (r == NULL) {
        return;
    }
    r->vfx_rng = seed;
    r->vfx_time = 0.0f;
}

void render_consume_events(RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL) {
        return;
    }
    r->events_seen += (uint64_t)(g->event_count > 0 ? g->event_count : 0);
}

void render_update_vfx(RenderState *r, const GameState *g, float dt)
{
    if (r == NULL || g == NULL) {
        return;
    }
    r->vfx_time += dt;
}

static void update_fluid_texture(RenderState *r, const GameState *g)
{
    if (r->fluid_tex.id == 0) {
        return;
    }
    const float *ink = fluid_channel_const(&g->fluid, 0);
    if (ink == NULL) {
        return;
    }
    for (int i = 0; i < FLUID_N; ++i) {
        const float d = ink_clampf(ink[i], 0.0f, 1.0f);
        const unsigned char alpha = (unsigned char)(d * 230.0f);
        r->fluid_pixels[i] = (Color){36, 31, 27, alpha};
    }
    UpdateTexture(r->fluid_tex, r->fluid_pixels);
}

static void draw_scene(const RenderState *r, const GameState *g)
{
    const Color paper = {243, 232, 210, 255};
    const Color ink = {42, 33, 29, 255};
    ClearBackground(paper);
    if (r->fluid_tex.id != 0) {
        const Rectangle src = {0.0f, 0.0f, (float)FLUID_W, (float)FLUID_H};
        const Rectangle dst = {0.0f, 0.0f, (float)INK_SCREEN_W, (float)INK_SCREEN_H};
        DrawTexturePro(r->fluid_tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    }
    DrawText("INKSTORM " INK_VERSION, 32, 32, 32, ink);
    DrawText(
        TextFormat("Tick %" PRIu64 "  |  Entities %" PRIu32, g->ticks, ecs_alive_count(&g->ecs)),
        32, 76, 20, ink);
}

void render_draw(RenderState *r, const GameState *g, const UiView *ui)
{
    if (r == NULL || g == NULL) {
        return;
    }
    (void)ui;
    ++r->frames_drawn;
    update_fluid_texture(r, g);

    if (r->post_shader.id == 0) {
        BeginDrawing();
        draw_scene(r, g);
        EndDrawing();
        return;
    }

    BeginTextureMode(r->scene);
    draw_scene(r, g);
    EndTextureMode();

    const Texture2D texture = r->scene.texture;
    const Rectangle source = {0.0f, 0.0f, (float)texture.width, -(float)texture.height};
    BeginDrawing();
    BeginShaderMode(r->post_shader);
    DrawTextureRec(texture, source, (Vector2){0.0f, 0.0f}, WHITE);
    EndShaderMode();
    EndDrawing();
}

void render_close(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    if (r->fluid_tex.id != 0) {
        UnloadTexture(r->fluid_tex);
    }
    if (r->post_shader.id != 0) {
        UnloadRenderTexture(r->scene);
        UnloadShader(r->post_shader);
    }
    memset(r, 0, sizeof *r);
}
