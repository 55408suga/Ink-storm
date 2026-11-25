#include "fluid.h"

#include <float.h>
#include <math.h>
#include <string.h>
#include <time.h>

static const float k_fast_field_limit = 1e15f;
static const float k_fast_eps_limit = 1e10f;
static const float k_fast_dt_limit = 1e5f;

static inline float clamp(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

static float limit_magnitude(double value, float limit)
{
    return value < -limit ? -limit : (value > limit ? limit : (float)value);
}

typedef struct {
    int base;
    float w00, w10, w01, w11;
} Stencil;

static inline Stencil bilinear(float x, float y)
{
    int x0 = (int)x, y0 = (int)y;
    float dx = x - x0, dy = y - y0;
    return (Stencil){x0 + FLUID_W * y0, (1 - dx) * (1 - dy), dx * (1 - dy), (1 - dx) * dy, dx * dy};
}

static inline float interpolate_safe(const float *field, Stencil s)
{
    const float *p = field + s.base;
    /* Sum in double so rounded weights cannot overflow a field near FLT_MAX. */
    return limit_magnitude((double)s.w00 * p[0] + (double)s.w10 * p[1] +
                               (double)s.w01 * p[FLUID_W] + (double)s.w11 * p[FLUID_W + 1],
                           FLT_MAX);
}

static inline float interpolate_fast(const float *restrict field, Stencil s)
{
    const float *p = field + s.base;
    return s.w00 * p[0] + s.w10 * p[1] + s.w01 * p[FLUID_W] + s.w11 * p[FLUID_W + 1];
}

void fluid_sample(const FluidState *f, float px, float py, float *out_u, float *out_v,
                  float *out_dye)
{
    if (!f) {
        return;
    }
    bool valid = isfinite(px) && isfinite(py);
    Stencil s = {0};
    if (valid) {
        s = bilinear(clamp(px / FLUID_CELL_PX - 0.5f, 0.5f, FLUID_W - 1.5f),
                     clamp(py / FLUID_CELL_PX - 0.5f, 0.5f, FLUID_H - 1.5f));
    }
    if (out_u) {
        *out_u = valid ? interpolate_safe(f->u, s) : 0;
    }
    if (out_v) {
        *out_v = valid ? interpolate_safe(f->v, s) : 0;
    }
    if (out_dye) {
        for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
            out_dye[ch] =
                valid && ch < f->cfg.active_channels ? interpolate_safe(f->dye[ch], s) : 0;
        }
    }
}

/* Rebuild cached geometry tables only when solid obstacles change. */
static void prepare_geometry(const uint8_t *restrict solid, float *restrict inv_neighbors,
                             float inv_rb[2][FLUID_N / 2], uint8_t *restrict near_wall)
{
    enum { RB_W = FLUID_W / 2 };
    static const float reciprocal[] = {0, 1, 0.5f, 1.0f / 3.0f, 0.25f};
    memset(inv_neighbors, 0, sizeof(float) * FLUID_N);
    memset(inv_rb, 0, sizeof(float) * FLUID_N);
    for (int y = 1; y < FLUID_H - 1; ++y) {
        int row = y * FLUID_W;
        int rb_row = y * RB_W;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = row + x;
            int count = (solid[i - 1] == 0) + (solid[i + 1] == 0) + (solid[i - FLUID_W] == 0) +
                        (solid[i + FLUID_W] == 0);
            float inv = solid[i] ? 0.0f : reciprocal[count];
            inv_neighbors[i] = inv;
            inv_rb[(x + y) & 1][rb_row + (x >> 1)] = inv;
        }
    }

    memset(near_wall, 0, sizeof(uint8_t) * FLUID_N);
    for (int y = 0; y < FLUID_H; ++y) {
        for (int x = 0; x < FLUID_W; ++x) {
            if (solid[x + FLUID_W * y]) {
                int left = x >= 3 ? x - 3 : 0;
                int right = x + 3 < FLUID_W ? x + 3 : FLUID_W - 1;
                for (int row = y >= 3 ? y - 3 : 0; row <= y + 3 && row < FLUID_H; ++row) {
                    memset(near_wall + left + FLUID_W * row, 1, (size_t)(right - left + 1));
                }
            }
        }
    }
}

static void confine_fast(FluidState *f, float dt)
{
    float *restrict u = f->u;
    float *restrict v = f->v;
    float *restrict curl_arr = f->curl;
    const uint8_t *restrict solid = f->solid;

    float peak = 0.0f;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        int row = y * FLUID_W;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = row + x;
            if (!solid[i]) {
                float c = 0.5f * ((v[i + 1] - v[i - 1]) - (u[i + FLUID_W] - u[i - FLUID_W]));
                curl_arr[i] = c;
                float ac = fabsf(c);
                if (ac > peak) {
                    peak = ac;
                }
            } else {
                curl_arr[i] = 0.0f;
            }
        }
    }
    f->stats.max_vort = peak;
    const float thresh = f->cfg.vorticity_thresh;
    const float eps = f->cfg.vorticity_eps;
    if (peak < thresh || eps == 0.0f) {
        return;
    }
    const float limit = f->cfg.max_vel;
    const float eps_dt = eps * dt;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        int row = y * FLUID_W;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = row + x;
            float c = curl_arr[i];
            if (c != 0.0f && fabsf(c) >= thresh) {
                float gx = 0.5f * (fabsf(curl_arr[i + 1]) - fabsf(curl_arr[i - 1]));
                float gy = 0.5f * (fabsf(curl_arr[i + FLUID_W]) - fabsf(curl_arr[i - FLUID_W]));
                float len = sqrtf(gx * gx + gy * gy) + 1e-4f;
                float force = eps_dt * c / len;
                u[i] = clamp(u[i] + gy * force, -limit, limit);
                v[i] = clamp(v[i] - gx * force, -limit, limit);
            }
        }
    }
}

static void confine_safe(FluidState *f, float dt)
{
    memset(f->curl, 0, sizeof f->curl);
    float peak = 0;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = x + y * FLUID_W;
            if (!f->solid[i]) {
                f->curl[i] =
                    limit_magnitude(0.5 * (((double)f->v[i + 1] - f->v[i - 1]) -
                                           ((double)f->u[i + FLUID_W] - f->u[i - FLUID_W])),
                                    FLT_MAX);
                if (fabsf(f->curl[i]) > peak) {
                    peak = fabsf(f->curl[i]);
                }
            }
        }
    }
    f->stats.max_vort = peak;
    if (peak < f->cfg.vorticity_thresh || f->cfg.vorticity_eps == 0) {
        return;
    }
    float limit = f->cfg.max_vel;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = x + y * FLUID_W;
            float curl = f->curl[i];
            if (curl != 0 && fabsf(curl) >= f->cfg.vorticity_thresh) {
                double gx = 0.5 * (fabsf(f->curl[i + 1]) - fabsf(f->curl[i - 1]));
                double gy = 0.5 * (fabsf(f->curl[i + FLUID_W]) - fabsf(f->curl[i - FLUID_W]));
                double len = sqrt(gx * gx + gy * gy) + 1e-4;
                double force = (double)f->cfg.vorticity_eps * curl * dt / len;
                f->u[i] = limit_magnitude(f->u[i] + gy * force, limit);
                f->v[i] = limit_magnitude(f->v[i] - gx * force, limit);
            }
        }
    }
}

static void project_fast(FluidState *f)
{
    enum { RB_W = FLUID_W / 2, RB_N = FLUID_N / 2 };
    float *restrict u = f->u;
    float *restrict v = f->v;
    float *restrict div = f->div;
    float *restrict p = f->p;
    const float *restrict inv_red = f->inv_rb[0];
    const float *restrict inv_blk = f->inv_rb[1];
    const uint8_t *restrict solid = f->solid;
    const uint8_t *restrict near_wall = f->near_wall;

    float *restrict div_red = f->rb_div[0];
    float *restrict div_blk = f->rb_div[1];
    float *restrict p_red = f->rb_p[0];
    float *restrict p_blk = f->rb_p[1];
    memset(p_red, 0, sizeof(float) * RB_N);
    memset(p_blk, 0, sizeof(float) * RB_N);

    for (int y = 1; y < FLUID_H - 1; ++y) {
        int row = y * FLUID_W;
        int rb_row = y * RB_W;
        if ((y & 1) == 0) {
            for (int k = 0; k < RB_W; ++k) {
                int i_even = row + (k << 1);
                int i_odd = i_even + 1;
                float d_even = (k > 0 && !solid[i_even])
                                   ? -0.5f * ((u[i_even + 1] - u[i_even - 1]) +
                                              (v[i_even + FLUID_W] - v[i_even - FLUID_W]))
                                   : 0.0f;
                float d_odd = (k < RB_W - 1 && !solid[i_odd])
                                  ? -0.5f * ((u[i_odd + 1] - u[i_odd - 1]) +
                                             (v[i_odd + FLUID_W] - v[i_odd - FLUID_W]))
                                  : 0.0f;
                div[i_even] = d_even;
                div[i_odd] = d_odd;
                div_red[rb_row + k] = d_even;
                div_blk[rb_row + k] = d_odd;
            }
        } else {
            for (int k = 0; k < RB_W; ++k) {
                int i_even = row + (k << 1);
                int i_odd = i_even + 1;
                float d_even = (k > 0 && !solid[i_even])
                                   ? -0.5f * ((u[i_even + 1] - u[i_even - 1]) +
                                              (v[i_even + FLUID_W] - v[i_even - FLUID_W]))
                                   : 0.0f;
                float d_odd = (k < RB_W - 1 && !solid[i_odd])
                                  ? -0.5f * ((u[i_odd + 1] - u[i_odd - 1]) +
                                             (v[i_odd + FLUID_W] - v[i_odd - FLUID_W]))
                                  : 0.0f;
                div[i_even] = d_even;
                div[i_odd] = d_odd;
                div_blk[rb_row + k] = d_even;
                div_red[rb_row + k] = d_odd;
            }
        }
    }

    const int iters = f->cfg.project_iters;
    if (iters == 0) {
        memset(p, 0, sizeof f->p);
        return;
    }

    /* Compact Red-Black Gauss-Seidel: unit-stride contiguous loops with zero loop-carried dep. */
    for (int iter = 0; iter < iters; ++iter) {
        for (int y = 1; y < FLUID_H - 1; ++y) {
            const int rb_row = y * RB_W;
            const float *restrict dr = div_red + rb_row;
            const float *restrict ir = inv_red + rb_row;
            float *restrict pr = p_red + rb_row;
            const float *restrict pb = p_blk + rb_row;
            const float *restrict pb_up = pb - RB_W;
            const float *restrict pb_dn = pb + RB_W;
            if ((y & 1) == 0) {
                for (int k = 1; k < RB_W; ++k) {
                    pr[k] = (dr[k] + pb[k - 1] + pb[k] + pb_up[k] + pb_dn[k]) * ir[k];
                }
            } else {
                for (int k = 0; k < RB_W - 1; ++k) {
                    pr[k] = (dr[k] + pb[k] + pb[k + 1] + pb_up[k] + pb_dn[k]) * ir[k];
                }
            }
        }
        for (int y = 1; y < FLUID_H - 1; ++y) {
            const int rb_row = y * RB_W;
            const float *restrict db = div_blk + rb_row;
            const float *restrict ib = inv_blk + rb_row;
            float *restrict pb = p_blk + rb_row;
            const float *restrict pr = p_red + rb_row;
            const float *restrict pr_up = pr - RB_W;
            const float *restrict pr_dn = pr + RB_W;
            if ((y & 1) == 0) {
                for (int k = 0; k < RB_W - 1; ++k) {
                    pb[k] = (db[k] + pr[k] + pr[k + 1] + pr_up[k] + pr_dn[k]) * ib[k];
                }
            } else {
                for (int k = 1; k < RB_W; ++k) {
                    pb[k] = (db[k] + pr[k - 1] + pr[k] + pr_up[k] + pr_dn[k]) * ib[k];
                }
            }
        }
    }

    /* Scatter Red/Black pressures back to f->p (borders stay 0). */
    memset(p, 0, sizeof(float) * FLUID_W);
    memset(p + (FLUID_H - 1) * FLUID_W, 0, sizeof(float) * FLUID_W);
    for (int y = 1; y < FLUID_H - 1; ++y) {
        const int row = y * FLUID_W;
        const int rb_row = y * RB_W;
        if ((y & 1) == 0) {
            for (int k = 0; k < RB_W; ++k) {
                p[row + (k << 1)] = p_red[rb_row + k];
                p[row + (k << 1) + 1] = p_blk[rb_row + k];
            }
        } else {
            for (int k = 0; k < RB_W; ++k) {
                p[row + (k << 1)] = p_blk[rb_row + k];
                p[row + (k << 1) + 1] = p_red[rb_row + k];
            }
        }
    }

    const float limit = f->cfg.max_vel;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        int row = y * FLUID_W;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = row + x;
            if (!near_wall[i]) {
                u[i] = clamp(u[i] - 0.5f * (p[i + 1] - p[i - 1]), -limit, limit);
                v[i] = clamp(v[i] - 0.5f * (p[i + FLUID_W] - p[i - FLUID_W]), -limit, limit);
            } else if (!solid[i]) {
                float pi = p[i];
                float pr_v = solid[i + 1] ? pi : p[i + 1];
                float pl_v = solid[i - 1] ? pi : p[i - 1];
                float pd_v = solid[i + FLUID_W] ? pi : p[i + FLUID_W];
                float pu_v = solid[i - FLUID_W] ? pi : p[i - FLUID_W];
                u[i] = clamp(u[i] - 0.5f * (pr_v - pl_v), -limit, limit);
                v[i] = clamp(v[i] - 0.5f * (pd_v - pu_v), -limit, limit);
            }
        }
    }
}

static void project_safe(FluidState *f)
{
    const float *inv_neighbors = f->inv_neighbors;
    memset(f->p, 0, sizeof f->p);
    /* Reserve headroom for 64 pressure sweeps even with a very large velocity limit. */
    float scale = f->cfg.project_iters > 0 && f->cfg.max_vel > FLT_MAX / 1024 ? 1.0f / 1024 : 1;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = x + y * FLUID_W;
            f->div[i] = f->solid[i]
                            ? 0
                            : limit_magnitude(-0.5 * scale *
                                                  (((double)f->u[i + 1] - f->u[i - 1]) +
                                                   ((double)f->v[i + FLUID_W] - f->v[i - FLUID_W])),
                                              FLT_MAX);
        }
    }
    if (f->cfg.project_iters == 0) {
        return;
    }
    float *p = f->p;
    for (int iter = 0; iter < f->cfg.project_iters; ++iter) {
        for (int y = 1; y < FLUID_H - 1; ++y) {
            float left = 0;
            for (int x = 1; x < FLUID_W - 1; ++x) {
                int i = x + y * FLUID_W;
                /* Solid pressure stays zero. Add the loop-carried (left) neighbor last. */
                left = (f->div[i] + p[i + 1] + p[i - FLUID_W] + p[i + FLUID_W] + left) *
                       inv_neighbors[i];
                p[i] = left;
            }
        }
    }

    float limit = f->cfg.max_vel;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = x + y * FLUID_W;
            if (!f->solid[i]) {
                float pr = f->solid[i + 1] ? p[i] : p[i + 1];
                float pl = f->solid[i - 1] ? p[i] : p[i - 1];
                float pd = f->solid[i + FLUID_W] ? p[i] : p[i + FLUID_W];
                float pu = f->solid[i - FLUID_W] ? p[i] : p[i - FLUID_W];
                f->u[i] = limit_magnitude(f->u[i] - (0.5 / scale) * ((double)pr - pl), limit);
                f->v[i] = limit_magnitude(f->v[i] - (0.5 / scale) * ((double)pd - pu), limit);
            }
        }
    }
    if (scale != 1) {
        for (int i = 0; i < FLUID_N; ++i) {
            f->div[i] = limit_magnitude((double)f->div[i] / scale, FLT_MAX);
            p[i] = limit_magnitude((double)p[i] / scale, FLT_MAX);
        }
    }
}

/* Clip at the last fluid point on the ray, then discard solid interpolation taps. */
static Stencil trace_walls(const uint8_t *solid, int x, int y, float ex, float ey, float distance2)
{
    int n = (int)ceilf(2 * sqrtf(distance2));
    float qx = (float)x, qy = (float)y;
    if (n > 0) {
        float sx = (ex - (float)x) / (float)n, sy = (ey - (float)y) / (float)n;
        for (int k = 1; k <= n; ++k) {
            float tx = k == n ? ex : (float)x + (float)k * sx;
            float ty = k == n ? ey : (float)y + (float)k * sy;
            if (solid[(int)(tx + 0.5f) + FLUID_W * (int)(ty + 0.5f)]) {
                break;
            }
            qx = tx;
            qy = ty;
        }
    }
    Stencil s = bilinear(qx, qy);
    const uint8_t *mask = solid + s.base;
    if (mask[0] || mask[1] || mask[FLUID_W] || mask[FLUID_W + 1]) {
        if (mask[0]) {
            s.w00 = 0;
        }
        if (mask[1]) {
            s.w10 = 0;
        }
        if (mask[FLUID_W]) {
            s.w01 = 0;
        }
        if (mask[FLUID_W + 1]) {
            s.w11 = 0;
        }
        float inv_sum = 1.0f / (s.w00 + s.w10 + s.w01 + s.w11);
        s.w00 *= inv_sum;
        s.w10 *= inv_sum;
        s.w01 *= inv_sum;
        s.w11 *= inv_sum;
    }
    return s;
}

static inline Stencil backtrace(const uint8_t *solid, bool near_wall, int x, int y, float u,
                                float v, float dt)
{
    float ex = clamp((float)x - dt * u, 0.5f, FLUID_W - 1.5f);
    float ey = clamp((float)y - dt * v, 0.5f, FLUID_H - 1.5f);
    float dx = ex - (float)x, dy = ey - (float)y;
    float distance2 = dx * dx + dy * dy;
    if (!near_wall && distance2 <= 4.0f) {
        return bilinear(ex, ey);
    }
    return trace_walls(solid, x, y, ex, ey, distance2);
}

static void advect_velocity_fast(FluidState *f, float dt)
{
    memcpy(f->u0, f->u, sizeof f->u);
    memcpy(f->v0, f->v, sizeof f->v);

    float *restrict u = f->u;
    float *restrict v = f->v;
    const float *restrict u0 = f->u0;
    const float *restrict v0 = f->v0;
    const uint8_t *restrict solid = f->solid;
    const uint8_t *restrict near_wall = f->near_wall;
    const float limit = f->cfg.max_vel;
    const float damp = f->cfg.vel_damp;

    for (int y = 1; y < FLUID_H - 1; ++y) {
        int row = y * FLUID_W;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = row + x;
            if (!solid[i]) {
                Stencil s = backtrace(solid, near_wall[i] != 0, x, y, u0[i], v0[i], dt);
                u[i] = clamp(interpolate_fast(u0, s) * damp, -limit, limit);
                v[i] = clamp(interpolate_fast(v0, s) * damp, -limit, limit);
            }
        }
    }
}

static void advect_velocity_safe(FluidState *f, float dt)
{
    memcpy(f->u0, f->u, sizeof f->u);
    memcpy(f->v0, f->v, sizeof f->v);
    const uint8_t *near_wall = f->near_wall;
    float limit = f->cfg.max_vel;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = x + y * FLUID_W;
            if (!f->solid[i]) {
                Stencil s = backtrace(f->solid, near_wall[i] != 0, x, y, f->u0[i], f->v0[i], dt);
                f->u[i] = clamp(interpolate_safe(f->u0, s) * f->cfg.vel_damp, -limit, limit);
                f->v[i] = clamp(interpolate_safe(f->v0, s) * f->cfg.vel_damp, -limit, limit);
            }
        }
    }
}

static void advect_scalars_fast(FluidState *f, float dt)
{
    const int channels = f->cfg.active_channels;
    memcpy(f->dye0, f->dye, (size_t)channels * sizeof f->dye[0]);

    const float *restrict u = f->u;
    const float *restrict v = f->v;
    const uint8_t *restrict solid = f->solid;
    const uint8_t *restrict near_wall = f->near_wall;
    const float max_dye = f->cfg.max_dye;

    float max_speed2 = 0.0f;
    if (channels == 3) {
        const float *restrict src0 = f->dye0[0];
        const float *restrict src1 = f->dye0[1];
        const float *restrict src2 = f->dye0[2];
        float *restrict dst0 = f->dye[0];
        float *restrict dst1 = f->dye[1];
        float *restrict dst2 = f->dye[2];
        const float damp0 = f->cfg.dye_damp[0];
        const float damp1 = f->cfg.dye_damp[1];
        const float damp2 = f->cfg.dye_damp[2];
        float max0 = 0.0f;
        float max1 = 0.0f;
        float max2 = 0.0f;

        for (int y = 1; y < FLUID_H - 1; ++y) {
            int row = y * FLUID_W;
            for (int x = 1; x < FLUID_W - 1; ++x) {
                int i = row + x;
                if (solid[i]) {
                    continue;
                }
                float ui = u[i];
                float vi = v[i];
                float sp2 = ui * ui + vi * vi;
                if (sp2 > max_speed2) {
                    max_speed2 = sp2;
                }
                Stencil s = backtrace(solid, near_wall[i] != 0, x, y, ui, vi, dt);
                float d0 = clamp(interpolate_fast(src0, s) * damp0, 0.0f, max_dye);
                float d1 = clamp(interpolate_fast(src1, s) * damp1, 0.0f, max_dye);
                float d2 = clamp(interpolate_fast(src2, s) * damp2, 0.0f, max_dye);
                dst0[i] = d0;
                dst1[i] = d1;
                dst2[i] = d2;
                if (d0 > max0) {
                    max0 = d0;
                }
                if (d1 > max1) {
                    max1 = d1;
                }
                if (d2 > max2) {
                    max2 = d2;
                }
            }
        }
        f->stats.max_dye[0] = max0;
        f->stats.max_dye[1] = max1;
        f->stats.max_dye[2] = max2;
    } else {
        float max_d[FLUID_MAX_CHANNELS] = {0};
        for (int y = 1; y < FLUID_H - 1; ++y) {
            int row = y * FLUID_W;
            for (int x = 1; x < FLUID_W - 1; ++x) {
                int i = row + x;
                if (solid[i]) {
                    continue;
                }
                float ui = u[i];
                float vi = v[i];
                float sp2 = ui * ui + vi * vi;
                if (sp2 > max_speed2) {
                    max_speed2 = sp2;
                }
                if (channels > 0) {
                    Stencil s = backtrace(solid, near_wall[i] != 0, x, y, ui, vi, dt);
                    for (int ch = 0; ch < channels; ++ch) {
                        float dye = clamp(interpolate_fast(f->dye0[ch], s) * f->cfg.dye_damp[ch],
                                          0.0f, max_dye);
                        f->dye[ch][i] = dye;
                        if (dye > max_d[ch]) {
                            max_d[ch] = dye;
                        }
                    }
                }
            }
        }
        for (int ch = 0; ch < channels; ++ch) {
            f->stats.max_dye[ch] = max_d[ch];
        }
    }
    f->stats.max_vel = sqrtf(max_speed2);
}

static void advect_scalars_safe(FluidState *f, float dt)
{
    int channels = f->cfg.active_channels;
    memcpy(f->dye0, f->dye, (size_t)channels * sizeof f->dye[0]);
    const uint8_t *near_wall = f->near_wall;
    double max_speed2 = 0;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = x + y * FLUID_W;
            if (f->solid[i]) {
                continue;
            }
            double speed2 = (double)f->u[i] * f->u[i] + (double)f->v[i] * f->v[i];
            if (speed2 > max_speed2) {
                max_speed2 = speed2;
            }
            if (channels > 0) {
                Stencil s = backtrace(f->solid, near_wall[i] != 0, x, y, f->u[i], f->v[i], dt);
                for (int ch = 0; ch < channels; ++ch) {
                    float dye = clamp(interpolate_safe(f->dye0[ch], s) * f->cfg.dye_damp[ch], 0,
                                      f->cfg.max_dye);
                    f->dye[ch][i] = dye;
                    if (dye > f->stats.max_dye[ch]) {
                        f->stats.max_dye[ch] = dye;
                    }
                }
            }
        }
    }
    f->stats.max_vel = limit_magnitude(sqrt(max_speed2), FLT_MAX);
}

void fluid_step(FluidState *f, float dt)
{
    if (!f || !isfinite(dt) || dt <= 0) {
        return;
    }
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    if (!f->geometry_valid) {
        prepare_geometry(f->solid, f->inv_neighbors, f->inv_rb, f->near_wall);
        f->geometry_valid = 1;
    }
    f->stats = (FluidStats){0};

    const bool fast_vel = f->cfg.max_vel <= k_fast_field_limit &&
                          f->cfg.vorticity_eps <= k_fast_eps_limit && dt <= k_fast_dt_limit;
    const bool fast_dye = f->cfg.max_dye <= k_fast_field_limit;

    if (fast_vel) {
        confine_fast(f, dt);
        project_fast(f);
        advect_velocity_fast(f, dt);
        project_fast(f);
    } else {
        confine_safe(f, dt);
        project_safe(f);
        advect_velocity_safe(f, dt);
        project_safe(f);
    }

    if (fast_vel && fast_dye) {
        advect_scalars_fast(f, dt);
    } else {
        advect_scalars_safe(f, dt);
    }

    ++f->steps;
    clock_gettime(CLOCK_MONOTONIC, &end);
    f->stats.solve_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) * 1e-6;
}
