#include "render_internal.h"

#include "core.h"
#include "rlgl.h"

#include <stddef.h>

/* Single concatenated GLSL string literal (<= 4095 bytes for -Woverlength-strings). Edge
 * darkening and the vignette back off on bright saturated pixels so fire, storm and gold stay lit.
 */
static const char k_post_fragment[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 resolution;\n"
    "out vec4 finalColor;\n"
    "float h(vec2 p){\n"
    "  vec3 q=fract(vec3(p.xyx)*.1031);\n"
    "  q+=dot(q,q.yzx+33.33);\n"
    "  return fract((q.x+q.y)*q.z);\n"
    "}\n"
    "float vn(vec2 p){\n"
    "  vec2 i=floor(p),f=fract(p);\n"
    "  f=f*f*(3.-2.*f);\n"
    "  return mix(mix(h(i),h(i+vec2(1.,0.)),f.x),mix(h(i+vec2(0.,1.)),h(i+1.),f.x),f.y);\n"
    "}\n"
    "void main(){\n"
    "  vec2 uv=fragTexCoord;\n"
    "  vec2 res=max(resolution,vec2(1.));\n"
    "  vec2 px=uv*res;\n"
    "  vec2 t=1./res;\n"
    "  vec3 c=texture(texture0,uv).rgb;\n"
    "  vec3 avg=.25*(texture(texture0,uv+vec2(0.,t.y)).rgb+texture(texture0,uv-vec2(0.,t.y)).rgb"
    "+texture(texture0,uv+vec2(t.x,0.)).rgb+texture(texture0,uv-vec2(t.x,0.)).rgb);\n"
    "  float lum=dot(c,vec3(.299,.587,.114));\n"
    "  float sat=max(c.r,max(c.g,c.b))-min(c.r,min(c.g,c.b));\n"
    "  float glow=smoothstep(.12,.42,sat)*smoothstep(.45,.85,lum);\n"
    "  float edge=max(dot(avg-c,vec3(.3333)),0.);\n"
    "  c*=1.-min(edge*1.8,.1)*(1.-glow)*(1.-smoothstep(.4,.98,lum));\n"
    "  float g=(vn(px*.33)-.5)*.5+(vn(px*1.4+17.)-.5)*.35+(h(px)-.5)*.3;\n"
    "  c+=vec3(1.,.97,.9)*g*.034*(.45+2.2*lum*(1.-lum));\n"
    "  vec2 d=(uv-.5)*vec2(1.2,.92);\n"
    "  float vig=smoothstep(.44,.98,length(d))*.30+smoothstep(.43,.5,abs(uv.x-.5))*.07;\n"
    "  c=mix(c,c*vec3(.91,.86,.77),vig*(1.-.7*glow));\n"
    "  finalColor=vec4(clamp(c,0.,1.),1.);\n"
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
        if (!force_shader_fault) {
            render_discard_shader(shader);
        }
        TraceLog(LOG_WARNING, "INKSTORM: post shader unavailable; drawing directly");
        return;
    }

    RenderTexture2D scene = LoadRenderTexture(INK_SCREEN_W, INK_SCREEN_H);
    if (!render_texture_usable(scene)) {
        if (scene.id != 0) {
            UnloadRenderTexture(scene);
        }
        UnloadShader(shader);
        TraceLog(LOG_WARNING, "INKSTORM: render texture unavailable; drawing directly");
        return;
    }

    SetTextureWrap(scene.texture, TEXTURE_WRAP_CLAMP);
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
