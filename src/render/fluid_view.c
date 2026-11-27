#include "palette.h"
#include "render_internal.h"
#include "render_rules.h"

#include "core.h"
#include "rlgl.h"

#include <math.h>
#include <stddef.h>

/* Single concatenated GLSL string literal (<= 4095 bytes for -Woverlength-strings).
 * texture0 = (ink, fire, storm, solid) field, texture1 = seamless washi; output is premultiplied.
 */
static const char k_fluid_fragment[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform sampler2D texture1;\n"
    "uniform vec2 u_fluidSize;\n"
    "uniform vec2 u_paperScale;\n"
    "uniform float u_maxDye;\n"
    "out vec4 finalColor;\n"
    "vec4 cw(float v){\n"
    "  vec4 n=vec4(1.,2.,3.,4.)-v;\n"
    "  vec4 s=n*n*n;\n"
    "  float x=s.x,y=s.y-4.*s.x,z=s.z-4.*s.y+6.*s.x;\n"
    "  return vec4(x,y,z,6.-x-y-z)/6.;\n"
    "}\n"
    "vec4 field(vec2 uv){\n"
    "  vec2 p=uv*u_fluidSize-.5;\n"
    "  vec2 f=fract(p);\n"
    "  p-=f;\n"
    "  vec4 X=cw(f.x),Y=cw(f.y);\n"
    "  vec4 s=vec4(X.xz+X.yw,Y.xz+Y.yw);\n"
    "  vec4 o=(p.xxyy+vec2(-.5,1.5).xyxy+vec4(X.yw,Y.yw)/s)/u_fluidSize.xxyy;\n"
    "  vec4 a=texture(texture0,o.xz),b=texture(texture0,o.yz);\n"
    "  vec4 c=texture(texture0,o.xw),d=texture(texture0,o.yw);\n"
    "  float sx=s.x/(s.x+s.y),sy=s.z/(s.z+s.w);\n"
    "  return mix(mix(d,c,sx),mix(b,a,sx),sy);\n"
    "}\n"
    "void main(){\n"
    "  vec2 uv=fragTexCoord;\n"
    "  vec2 pu=uv*u_paperScale;\n"
    "  float pp=texture(texture1,pu).r;\n"
    "  float pf=texture(texture1,pu*.37+vec2(.31,.17)).r;\n"
    "  vec2 bleed=(vec2(pp,pf)-.95)*6./u_fluidSize;\n"
    "  vec4 F=field(uv+bleed);\n"
    "  vec3 v=min(F.rgb*F.rgb*u_maxDye,vec3(4.));\n"
    "  ivec2 cell=ivec2(clamp(uv*u_fluidSize,vec2(0.),u_fluidSize-1.));\n"
    "  float open=1.-texelFetch(texture0,cell,0).a;\n"
    "  float d=v.r,on=step(.05,d);\n"
    "  float pa=smoothstep(.05,.25,d),hv=smoothstep(.25,.5,d),dn=smoothstep(.25,.8,d);\n"
    "  vec3 c=mix(vec3(.79,.76,.70),vec3(.55,.52,.48),pa);\n"
    "  c=mix(c,vec3(.29,.27,.24),hv);\n"
    "  c=mix(c,vec3(.17,.15,.14),smoothstep(.5,.72,d));\n"
    "  c=mix(c,vec3(.10,.09,.08),smoothstep(.72,.8,d));\n"
    "  float cpp=max(abs(dFdx(uv.x))*u_fluidSize.x,1e-4);\n"
    "  float g=length(vec2(dFdx(d),dFdy(d)))/cpp;\n"
    "  float pool=smoothstep(.02,.45,g*.55)*(1.-hv);\n"
    "  c=mix(c,vec3(.29,.27,.24),pool*.6);\n"
    "  float gr=clamp((.975-pp)*9.,-.4,1.);\n"
    "  c*=(1.-.16*gr*(1.-.5*dn))*mix(1.,pp/.95,.4);\n"
    "  float ai=on*(.15+.3*pa+.47*dn+.28*pool)*open;\n"
    "  float af=smoothstep(.05,.5,v.g),as=smoothstep(.05,.46,v.b);\n"
    "  vec3 fc=mix(vec3(.78,.27,.17),vec3(.94,.54,.24),smoothstep(.3,.95,v.g));\n"
    "  fc=mix(fc,vec3(1.,.9,.66),smoothstep(.9,2.,v.g)*.7);\n"
    "  vec3 sc=mix(vec3(.18,.44,.7),vec3(.62,.83,.95),smoothstep(.3,.9,v.b));\n"
    "  sc=mix(sc,vec3(.93,.97,1.),smoothstep(.9,2.,v.b)*.6);\n"
    "  float ae=(1.-(1.-af)*(1.-as))*open;\n"
    "  vec3 ec=(fc*af+sc*as)/max(af+as,1e-4);\n"
    "  ec=mix(ec,vec3(.96,.83,.48),smoothstep(0.,.55,min(af,as))*.8);\n"
    "  ae*=.88+.12*smoothstep(.9,1.,pf);\n"
    "  float occ=ae*.85;\n"
    "  finalColor=vec4(ec*ae+c*ai*(1.-occ),occ+ai*(1.-occ));\n"
    "}\n";

static RenderRgba pal_rgba(PaletteId id)
{
    const Color c = pal_color(id);
    return (RenderRgba){(float)c.r / 255.0f, (float)c.g / 255.0f, (float)c.b / 255.0f, 1.0f};
}

static RenderFluidPalette fluid_palette(void)
{
    return (RenderFluidPalette){
        .sumi_clear = pal_rgba(PAL_SUMI_CLEAR),
        .sumi_light = pal_rgba(PAL_SUMI_LIGHT),
        .sumi_heavy = pal_rgba(PAL_SUMI_HEAVY),
        .sumi_dark = pal_rgba(PAL_SUMI_DARK),
        .sumi_scorch = pal_rgba(PAL_SUMI_SCORCH),
        .shu = pal_rgba(PAL_SHU),
        .shu_hot = pal_rgba(PAL_SHU_HOT),
        .ao = pal_rgba(PAL_AO),
        .ao_hot = pal_rgba(PAL_AO_HOT),
        .kin_hot = pal_rgba(PAL_KIN_HOT),
    };
}

static unsigned char unit_to_u8(float v)
{
    return (unsigned char)(ink_clampf(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

/* Density range covered by the CPU lookup tables; denser cells saturate at the darkest tone. */
static const float k_lut_max_density = 3.0f;
static const float k_lut_max_grad = 0.85f;

static float lut_density(int level, int levels)
{
    const float n = (float)level / (float)(levels - 1);
    return n * n * k_lut_max_density;
}

static int lut_level(float value, int levels)
{
    if (!(value > 0.0f)) {
        return 0;
    }
    const float n = sqrtf(fminf(value / k_lut_max_density, 1.0f));
    return (int)(n * (float)(levels - 1) + 0.5f);
}

static void build_cpu_luts(RenderState *r)
{
    const RenderFluidPalette pal = fluid_palette();
    for (int d = 0; d < RENDER_INK_LUT_LEVELS; ++d) {
        const float ink = lut_density(d, RENDER_INK_LUT_LEVELS);
        for (int gi = 0; gi < RENDER_GRAD_LUT_LEVELS; ++gi) {
            const float grad = k_lut_max_grad * (float)gi / (float)(RENDER_GRAD_LUT_LEVELS - 1);
            const RenderRgba t = render_fluid_cpu_ink(&pal, ink, grad);
            r->fluid_ink_lut[d * RENDER_GRAD_LUT_LEVELS + gi] =
                (Color){unit_to_u8(t.r), unit_to_u8(t.g), unit_to_u8(t.b), 255};
        }
    }
    for (int fi = 0; fi < RENDER_ELEM_LUT_LEVELS; ++fi) {
        const float fire = lut_density(fi, RENDER_ELEM_LUT_LEVELS);
        for (int si = 0; si < RENDER_ELEM_LUT_LEVELS; ++si) {
            const RenderRgba e =
                render_fluid_cpu_elem(&pal, fire, lut_density(si, RENDER_ELEM_LUT_LEVELS));
            r->fluid_elem_lut[fi * RENDER_ELEM_LUT_LEVELS + si] =
                (Color){unit_to_u8(e.r), unit_to_u8(e.g), unit_to_u8(e.b), unit_to_u8(e.a)};
        }
    }
}

static Texture2D load_field_texture(Color fill)
{
    Image img = GenImageColor(FLUID_W, FLUID_H, fill);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    if (tex.id != 0) {
        SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
    }
    return tex;
}

static void load_fluid_shader(RenderState *r)
{
    Shader shader = LoadShaderFromMemory(NULL, k_fluid_fragment);
    if (!IsShaderValid(shader) || shader.id == rlGetShaderIdDefault()) {
        render_discard_shader(shader);
        TraceLog(LOG_WARNING, "INKSTORM: fluid shader unavailable; using CPU fluid shading");
        return;
    }
    r->fluid_shader = shader;
    r->fluid_paper_loc = GetShaderLocation(shader, "texture1");
    const int size_loc = GetShaderLocation(shader, "u_fluidSize");
    const int scale_loc = GetShaderLocation(shader, "u_paperScale");
    r->fluid_max_dye_loc = GetShaderLocation(shader, "u_maxDye");
    const Vector2 size = {(float)FLUID_W, (float)FLUID_H};
    const float paper_w = r->paper_tex.id != 0 ? (float)r->paper_tex.width : (float)INK_SCREEN_W;
    const float paper_h = r->paper_tex.id != 0 ? (float)r->paper_tex.height : (float)INK_SCREEN_H;
    const Vector2 scale = {(float)INK_SCREEN_W / paper_w, (float)INK_SCREEN_H / paper_h};
    if (size_loc >= 0) {
        SetShaderValue(shader, size_loc, &size, SHADER_UNIFORM_VEC2);
    }
    if (scale_loc >= 0) {
        SetShaderValue(shader, scale_loc, &scale, SHADER_UNIFORM_VEC2);
    }
    TraceLog(LOG_INFO, "INKSTORM: fluid shader enabled");
}

void render_fluid_init(RenderState *r, const RenderOptions *opt)
{
    if (r == NULL) {
        return;
    }
    r->fluid_shader = (Shader){0};
    r->fluid_paper_loc = -1;
    r->fluid_max_dye_loc = -1;
    for (int i = 0; i < 2; ++i) {
        r->fluid_field_tex[i] = load_field_texture(pal_blank());
        r->fluid_ink_tex[i] = load_field_texture(pal_white());
        r->fluid_elem_tex[i] = load_field_texture(pal_blank());
    }
    r->fluid_tex_slot = 0;
    r->fluid_cache = (FluidUploadCache){0};
    build_cpu_luts(r);

    /* The GPU path rides on the post-processing switch, so --no-post and an injected shader fault
     * both exercise the CPU shading below. */
    const bool use_post = opt == NULL || opt->use_post;
    const bool shader_fault = opt != NULL && (opt->fault_mask & RENDER_FAULT_SHADER) != 0;
    if (use_post && !shader_fault && r->fluid_field_tex[0].id != 0 &&
        r->fluid_field_tex[1].id != 0) {
        load_fluid_shader(r);
    }
}

static void unload_field_texture(Texture2D *tex)
{
    if (tex->id != 0) {
        UnloadTexture(*tex);
        *tex = (Texture2D){0};
    }
}

void render_fluid_close(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    for (int i = 0; i < 2; ++i) {
        unload_field_texture(&r->fluid_field_tex[i]);
        unload_field_texture(&r->fluid_ink_tex[i]);
        unload_field_texture(&r->fluid_elem_tex[i]);
    }
    if (r->fluid_shader.id != 0) {
        UnloadShader(r->fluid_shader);
        r->fluid_shader = (Shader){0};
    }
    r->fluid_paper_loc = -1;
    r->fluid_max_dye_loc = -1;
    r->fluid_cache = (FluidUploadCache){0};
}

void render_fluid_invalidate(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    r->fluid_cache = (FluidUploadCache){0};
}

bool render_fluid_gpu_active(const RenderState *r)
{
    return r != NULL &&
           render_fluid_use_gpu(r->post_enabled, r->fluid_shader.id,
                                r->fluid_paper_loc >= 0 ? r->paper_tex.id : 0u) &&
           r->fluid_field_tex[0].id != 0 && r->fluid_field_tex[1].id != 0;
}

static void draw_fluid_gpu(RenderState *r, const GameState *g, int32_t slot, bool upload)
{
    const Texture2D field = r->fluid_field_tex[slot];
    if (upload) {
        render_fluid_encode_field(&g->fluid, r->fluid_field_px);
        UpdateTexture(field, r->fluid_field_px);
    }

    const Rectangle src = {0.0f, 0.0f, (float)FLUID_W, (float)FLUID_H};
    const Rectangle dst = {0.0f, 0.0f, (float)INK_SCREEN_W, (float)INK_SCREEN_H};
    render_set_blend(r, BLEND_ALPHA_PREMULTIPLY);
    BeginShaderMode(r->fluid_shader);
    SetShaderValueTexture(r->fluid_shader, r->fluid_paper_loc, r->paper_tex);
    if (r->fluid_max_dye_loc >= 0) {
        const float max_dye = g->fluid.cfg.max_dye;
        SetShaderValue(r->fluid_shader, r->fluid_max_dye_loc, &max_dye, SHADER_UNIFORM_FLOAT);
    }
    DrawTexturePro(field, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, pal_white());
    EndShaderMode();
    render_set_blend(r, BLEND_ALPHA);
}

static void draw_fluid_cpu(RenderState *r, const GameState *g, int32_t slot, bool upload)
{
    const Texture2D ink_tex = r->fluid_ink_tex[slot];
    const Texture2D elem_tex = r->fluid_elem_tex[slot];
    if (upload) {
        const float *ink = fluid_channel_const(&g->fluid, 0);
        const float *fire = fluid_channel_const(&g->fluid, 1);
        const float *storm = fluid_channel_const(&g->fluid, 2);
        if (ink == NULL || fire == NULL || storm == NULL) {
            return;
        }
        const float grad_scale = (float)(RENDER_GRAD_LUT_LEVELS - 1) / k_lut_max_grad;
        for (int y = 0; y < FLUID_H; ++y) {
            const int y_up = y > 0 ? y - 1 : 0;
            const int y_dn = y + 1 < FLUID_H ? y + 1 : FLUID_H - 1;
            for (int x = 0; x < FLUID_W; ++x) {
                const int idx = y * FLUID_W + x;
                const int xl = x > 0 ? x - 1 : 0;
                const int xr = x + 1 < FLUID_W ? x + 1 : FLUID_W - 1;
                const float gx = 0.5f * (ink[y * FLUID_W + xr] - ink[y * FLUID_W + xl]);
                const float gy = 0.5f * (ink[y_dn * FLUID_W + x] - ink[y_up * FLUID_W + x]);
                const float grad_level = fminf(sqrtf(gx * gx + gy * gy) * grad_scale,
                                               (float)(RENDER_GRAD_LUT_LEVELS - 1));
                const int gi = isfinite(grad_level) ? (int)(grad_level + 0.5f) : 0;
                const int di = lut_level(ink[idx], RENDER_INK_LUT_LEVELS);
                r->fluid_ink_px[idx] = r->fluid_ink_lut[di * RENDER_GRAD_LUT_LEVELS + gi];
                const int fi = lut_level(fire[idx], RENDER_ELEM_LUT_LEVELS);
                const int si = lut_level(storm[idx], RENDER_ELEM_LUT_LEVELS);
                r->fluid_elem_px[idx] = r->fluid_elem_lut[fi * RENDER_ELEM_LUT_LEVELS + si];
            }
        }
        UpdateTexture(ink_tex, r->fluid_ink_px);
        UpdateTexture(elem_tex, r->fluid_elem_px);
    }

    const Rectangle src = {0.0f, 0.0f, (float)FLUID_W, (float)FLUID_H};
    const Rectangle dst = {0.0f, 0.0f, (float)INK_SCREEN_W, (float)INK_SCREEN_H};
    render_set_blend(r, BLEND_MULTIPLIED);
    DrawTexturePro(ink_tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, pal_white());
    /* Premultiplied texels filter without the dark fringe that straight alpha over black gives. */
    render_set_blend(r, BLEND_ALPHA_PREMULTIPLY);
    DrawTexturePro(elem_tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, pal_white());
    render_set_blend(r, BLEND_ALPHA);
}

void render_fluid_update_and_draw(RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL) {
        return;
    }
    const bool use_gpu = render_fluid_gpu_active(r);
    if (!use_gpu && (r->fluid_ink_tex[0].id == 0 || r->fluid_ink_tex[1].id == 0 ||
                     r->fluid_elem_tex[0].id == 0 || r->fluid_elem_tex[1].id == 0)) {
        return;
    }

    const bool borrow_open = fluid_has_open_mut_borrow(&g->fluid);
    const bool hit =
        !borrow_open && render_fluid_cache_matches(&r->fluid_cache, &g->fluid, use_gpu);
    int32_t slot;
    if (hit) {
        slot = r->fluid_cache.slot;
    } else {
        slot = r->fluid_tex_slot;
        r->fluid_cache = (FluidUploadCache){
            .valid = !borrow_open,
            .field_ptr = &g->fluid,
            .steps = g->fluid.steps,
            .mutation_rev = g->fluid.mutation_rev,
            .solid_rev = g->fluid.solid_rev,
            .pristine = fluid_is_pristine(&g->fluid),
            .dims_key = RENDER_FLUID_DIMS_KEY,
            .max_dye = g->fluid.cfg.max_dye,
            .gpu_path = use_gpu,
            .slot = slot,
        };
        r->fluid_tex_slot ^= 1;
    }

    if (use_gpu) {
        draw_fluid_gpu(r, g, slot, !hit);
    } else {
        draw_fluid_cpu(r, g, slot, !hit);
    }
}
