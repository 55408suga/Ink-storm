#include "render_internal.h"

#include "core.h"
#include "rlgl.h"

#include <stddef.h>

/* Single concatenated GLSL string literal (<= 4095 bytes for -Woverlength-strings) */
static const char k_post_fragment[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 resolution;\n"
    "out vec4 finalColor;\n"
    "float hash12(vec2 p) {\n"
    "    vec3 p3 = fract(vec3(p.xyx) * 0.1031);\n"
    "    p3 += dot(p3, p3.yzx + 33.33);\n"
    "    return fract((p3.x + p3.y) * p3.z);\n"
    "}\n"
    "void main() {\n"
    "    vec2 uv = fragTexCoord;\n"
    "    vec2 res = max(resolution, vec2(1.0));\n"
    "    vec2 px = uv * res;\n"
    "    vec2 texel = 1.0 / res;\n"
    "    vec3 c = texture(texture0, uv).rgb;\n"
    "    vec3 n = texture(texture0, uv + vec2(0.0, texel.y * 1.25)).rgb;\n"
    "    vec3 s = texture(texture0, uv - vec2(0.0, texel.y * 1.25)).rgb;\n"
    "    vec3 e = texture(texture0, uv + vec2(texel.x * 1.25, 0.0)).rgb;\n"
    "    vec3 w = texture(texture0, uv - vec2(texel.x * 1.25, 0.0)).rgb;\n"
    "    float edge = length(c - 0.25 * (n + s + e + w));\n"
    "    c *= 1.0 - clamp(edge * 0.24, 0.0, 0.09);\n"
    "    float f1 = hash12(floor(px * vec2(0.32, 1.35)));\n"
    "    float f2 = hash12(floor(px * vec2(1.40, 0.28)) + 19.3);\n"
    "    float grain = (f1 * 0.5 + f2 * 0.5 - 0.5) * 0.032;\n"
    "    c += vec3(grain, grain * 0.94, grain * 0.80);\n"
    "    vec2 d = (uv - 0.5) * vec2(1.12, 1.0);\n"
    "    float vig = smoothstep(0.48, 0.98, length(d));\n"
    "    c = mix(c, c * vec3(0.90, 0.86, 0.78), vig * 0.34);\n"
    "    finalColor = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
    "}\n";

void render_post_init(RenderState *r, const RenderOptions *opt)
{
    if (r == NULL) {
        return;
    }
    r->post_shader = (Shader){0};
    r->post_res_loc = -1;
    r->scene = (RenderTexture2D){0};
    r->post_enabled = false;

    const bool use_post = opt == NULL || opt->use_post;
    if (!use_post) {
        TraceLog(LOG_INFO, "INKSTORM: post shader disabled; drawing directly");
        return;
    }

    const bool force_shader_fault = opt != NULL && (opt->fault_mask & RENDER_FAULT_SHADER) != 0;
    Shader shader = {0};
    if (!force_shader_fault) {
        shader = LoadShaderFromMemory(NULL, k_post_fragment);
    }
    if (force_shader_fault || !IsShaderValid(shader) || shader.id == rlGetShaderIdDefault()) {
        if (shader.id != 0 && shader.id != rlGetShaderIdDefault()) {
            UnloadShader(shader);
        }
        TraceLog(LOG_WARNING, "INKSTORM: post shader unavailable; drawing directly");
        return;
    }

    RenderTexture2D scene = LoadRenderTexture(INK_SCREEN_W, INK_SCREEN_H);
    if (!IsRenderTextureValid(scene)) {
        UnloadRenderTexture(scene);
        UnloadShader(shader);
        TraceLog(LOG_WARNING, "INKSTORM: render texture unavailable; drawing directly");
        return;
    }

    r->post_shader = shader;
    r->scene = scene;
    r->post_enabled = true;
    r->post_res_loc = GetShaderLocation(shader, "resolution");
    if (r->post_res_loc >= 0) {
        const Vector2 res = {(float)INK_SCREEN_W, (float)INK_SCREEN_H};
        SetShaderValue(shader, r->post_res_loc, &res, SHADER_UNIFORM_VEC2);
    }
    TraceLog(LOG_INFO, "INKSTORM: post shader enabled");
}

void render_set_post_enabled(RenderState *r, bool enabled)
{
    if (r == NULL) {
        return;
    }
    r->post_enabled = enabled && r->post_shader.id != 0 && r->scene.id != 0;
}

void render_post_close(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    if (r->scene.id != 0) {
        UnloadRenderTexture(r->scene);
        r->scene = (RenderTexture2D){0};
    }
    if (r->post_shader.id != 0) {
        UnloadShader(r->post_shader);
        r->post_shader = (Shader){0};
    }
    r->post_res_loc = -1;
    r->post_enabled = false;
}
