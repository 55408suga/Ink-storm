#include "palette.h"
#include "render_internal.h"

#include "core.h"

#include <math.h>
#include <stddef.h>

static float smoothstep01(float x)
{
    const float t = ink_clampf(x, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void render_fluid_init(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    Image img_ink = GenImageColor(FLUID_W, FLUID_H, pal_white());
    r->fluid_ink_tex = LoadTextureFromImage(img_ink);
    UnloadImage(img_ink);

    Image img_elem = GenImageColor(FLUID_W, FLUID_H, pal_blank());
    r->fluid_elem_tex = LoadTextureFromImage(img_elem);
    UnloadImage(img_elem);

    if (r->fluid_ink_tex.id != 0) {
        SetTextureFilter(r->fluid_ink_tex, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(r->fluid_ink_tex, TEXTURE_WRAP_CLAMP);
    }
    if (r->fluid_elem_tex.id != 0) {
        SetTextureFilter(r->fluid_elem_tex, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(r->fluid_elem_tex, TEXTURE_WRAP_CLAMP);
    }
}

void render_fluid_close(RenderState *r)
{
    if (r == NULL) {
        return;
    }
    if (r->fluid_ink_tex.id != 0) {
        UnloadTexture(r->fluid_ink_tex);
        r->fluid_ink_tex = (Texture2D){0};
    }
    if (r->fluid_elem_tex.id != 0) {
        UnloadTexture(r->fluid_elem_tex);
        r->fluid_elem_tex = (Texture2D){0};
    }
}

void render_fluid_update_and_draw(RenderState *r, const GameState *g)
{
    if (r == NULL || g == NULL || r->fluid_ink_tex.id == 0 || r->fluid_elem_tex.id == 0) {
        return;
    }
    const float *ink = fluid_channel_const(&g->fluid, 0);
    const float *fire = fluid_channel_const(&g->fluid, 1);
    const float *storm = fluid_channel_const(&g->fluid, 2);
    if (ink == NULL) {
        return;
    }

    const Color white = pal_white();
    const Color blank = pal_blank();

    for (int y = 0; y < FLUID_H; ++y) {
        const int y_up = y > 0 ? y - 1 : 0;
        const int y_dn = y + 1 < FLUID_H ? y + 1 : FLUID_H - 1;
        for (int x = 0; x < FLUID_W; ++x) {
            const int idx = y * FLUID_W + x;
            const float d = ink[idx];

            /* Ink tone curve & edge pooling (art-direction §9.4) */
            if (d < 0.05f) {
                r->fluid_ink_pixels[idx] = white;
            } else {
                const int xl = x > 0 ? x - 1 : 0;
                const int xr = x + 1 < FLUID_W ? x + 1 : FLUID_W - 1;
                const float gx = ink[y * FLUID_W + xr] - ink[y * FLUID_W + xl];
                const float gy = ink[y_dn * FLUID_W + x] - ink[y_up * FLUID_W + x];
                const float grad = sqrtf(gx * gx + gy * gy);
                const float pool = ink_clampf(grad * 0.55f, 0.0f, 0.32f);

                if (d < 0.25f) {
                    const float u = smoothstep01((d - 0.05f) / 0.20f);
                    const float a = ink_clampf(0.15f + 0.30f * u + pool * 0.35f, 0.0f, 0.92f);
                    r->fluid_ink_pixels[idx] =
                        pool > 0.14f ? pal_mul_tint(PAL_SUMI_LIGHT, PAL_SUMI_HEAVY, pool * 2.0f, a)
                                     : pal_mul_tint(PAL_SUMI_CLEAR, PAL_SUMI_LIGHT, u, a);
                } else if (d < 0.80f) {
                    const float u = (d - 0.25f) / 0.55f;
                    const float a = ink_clampf(0.45f + 0.40f * u + pool * 0.20f, 0.0f, 0.92f);
                    r->fluid_ink_pixels[idx] =
                        pool > 0.14f ? pal_mul_tint(PAL_SUMI_HEAVY, PAL_SUMI_DARK, u, a)
                                     : pal_mul_tint(PAL_SUMI_LIGHT, PAL_SUMI_DARK, u, a);
                } else {
                    r->fluid_ink_pixels[idx] =
                        pal_mul_tint(PAL_SUMI_SCORCH, PAL_SUMI_SCORCH, 0.0f, 0.92f);
                }
            }

            /* Fire & storm two-layer elemental overlay (outer base color, bright hot core) */
            const float f = fire != NULL ? fire[idx] : 0.0f;
            const float s = storm != NULL ? storm[idx] : 0.0f;
            if (f < 0.05f && s < 0.05f) {
                r->fluid_elem_pixels[idx] = blank;
            } else if (f >= s) {
                const float hot = smoothstep01((f - 0.32f) / 0.58f);
                const float a = ink_clampf(smoothstep01((f - 0.05f) / 0.45f) * 0.86f, 0.0f, 0.90f);
                r->fluid_elem_pixels[idx] = pal_lerp(PAL_SHU, PAL_SHU_HOT, hot, a);
            } else {
                const float hot = smoothstep01((s - 0.30f) / 0.55f);
                const float a = ink_clampf(smoothstep01((s - 0.05f) / 0.42f) * 0.86f, 0.0f, 0.90f);
                r->fluid_elem_pixels[idx] = pal_lerp(PAL_AO, PAL_AO_HOT, hot, a);
            }
        }
    }

    UpdateTexture(r->fluid_ink_tex, r->fluid_ink_pixels);
    UpdateTexture(r->fluid_elem_tex, r->fluid_elem_pixels);

    const Rectangle src = {0.0f, 0.0f, (float)FLUID_W, (float)FLUID_H};
    const Rectangle dst = {0.0f, 0.0f, (float)INK_SCREEN_W, (float)INK_SCREEN_H};

    /* Ink uses multiply blend so layers darken naturally into the washi paper */
    render_set_blend(r, BLEND_MULTIPLIED);
    DrawTexturePro(r->fluid_ink_tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, white);

    /* Elemental colors use standard alpha blend so fire/storm remain legible over dark ink */
    render_set_blend(r, BLEND_ALPHA);
    DrawTexturePro(r->fluid_elem_tex, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, white);
}
