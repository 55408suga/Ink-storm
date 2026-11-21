#include "render.h"

#include "core.h"
#include "rlgl.h"

#include <inttypes.h>
#include <stddef.h>

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

RenderState render_init(bool use_post, int width, int height)
{
    if (!use_post) {
        TraceLog(LOG_INFO, "INKSTORM: post shader disabled; drawing directly");
        return (RenderState){0};
    }

    Shader shader = LoadShaderFromMemory(NULL, post_fragment);
    // raylib can return its valid default shader when a custom shader fails.
    if (!IsShaderValid(shader) || shader.id == rlGetShaderIdDefault()) {
        UnloadShader(shader);
        TraceLog(LOG_WARNING, "INKSTORM: post shader unavailable; drawing directly");
        return (RenderState){0};
    }

    RenderTexture2D scene = LoadRenderTexture(width, height);
    if (!IsRenderTextureValid(scene)) {
        UnloadRenderTexture(scene);
        UnloadShader(shader);
        TraceLog(LOG_WARNING, "INKSTORM: render texture unavailable; drawing directly");
        return (RenderState){0};
    }

    TraceLog(LOG_INFO, "INKSTORM: post shader enabled");
    return (RenderState){.post_shader = shader, .scene = scene};
}

static void draw_scene(const GameState *game)
{
    const Color paper = {243, 232, 210, 255};
    const Color ink = {42, 33, 29, 255};
    ClearBackground(paper);
    DrawText("INKSTORM " INK_VERSION, 32, 32, 32, ink);
    DrawText(TextFormat("Tick %" PRIu64 "  |  Entities %" PRIu32, game->ticks,
                        ecs_alive_count(&game->ecs)),
             32, 76, 20, ink);
}

void render_draw(const RenderState *render, const GameState *game)
{
    if (render->post_shader.id == 0) {
        BeginDrawing();
        draw_scene(game);
        EndDrawing();
        return;
    }

    BeginTextureMode(render->scene);
    draw_scene(game);
    EndTextureMode();

    const Texture2D texture = render->scene.texture;
    const Rectangle source = {0, 0, (float)texture.width, -(float)texture.height};
    BeginDrawing();
    BeginShaderMode(render->post_shader);
    DrawTextureRec(texture, source, (Vector2){0, 0}, WHITE);
    EndShaderMode();
    EndDrawing();
}

void render_close(const RenderState *render)
{
    if (render->post_shader.id != 0) {
        UnloadRenderTexture(render->scene);
        UnloadShader(render->post_shader);
    }
}
