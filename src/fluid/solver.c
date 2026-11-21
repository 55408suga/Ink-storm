#include "fluid.h"

#include <float.h>
#include <math.h>
#include <string.h>
#include <time.h>

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

static inline float interpolate(const float *field, Stencil s)
{
    const float *p = field + s.base;
    // Sum in double so rounded weights cannot overflow a field near FLT_MAX.
    return limit_magnitude((double)s.w00 * p[0] + (double)s.w10 * p[1] +
                               (double)s.w01 * p[FLUID_W] + (double)s.w11 * p[FLUID_W + 1],
                           FLT_MAX);
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
        *out_u = valid ? interpolate(f->u, s) : 0;
    }
    if (out_v) {
        *out_v = valid ? interpolate(f->v, s) : 0;
    }
    if (out_dye) {
        for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
            out_dye[ch] = valid && ch < f->cfg.active_channels ? interpolate(f->dye[ch], s) : 0;
        }
    }
}

// Build both geometry caches once for the two projections and the two advections.
static void prepare_geometry(const uint8_t *solid, float *inv_neighbors, uint8_t *near_wall)
{
    static const float reciprocal[] = {0, 1, 0.5f, 1.0f / 3.0f, 0.25f};
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = x + y * FLUID_W;
            int count = (solid[i - 1] == 0) + (solid[i + 1] == 0) + (solid[i - FLUID_W] == 0) +
                        (solid[i + FLUID_W] == 0);
            inv_neighbors[i] = solid[i] ? 0 : reciprocal[count];
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

static void confine(FluidState *f, float dt)
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

static void project(FluidState *f, const float *inv_neighbors)
{
    memset(f->p, 0, sizeof f->p);
    // Reserve headroom for 64 pressure sweeps even with a very large velocity limit.
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
                // Solid pressure stays zero. Add the loop-carried (left) neighbor last.
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

// Clip at the last fluid point on the ray, then discard solid interpolation taps.
static Stencil trace_walls(const uint8_t *solid, int x, int y, float ex, float ey, float distance2)
{
    int n = (int)ceilf(2 * sqrtf(distance2));
    float qx = x, qy = y;
    if (n > 0) {
        float sx = (ex - x) / n, sy = (ey - y) / n;
        for (int k = 1; k <= n; ++k) {
            float tx = k == n ? ex : x + k * sx;
            float ty = k == n ? ey : y + k * sy;
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
        float inv_sum = 1 / (s.w00 + s.w10 + s.w01 + s.w11);
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
    float ex = clamp(x - dt * u, 0.5f, FLUID_W - 1.5f);
    float ey = clamp(y - dt * v, 0.5f, FLUID_H - 1.5f);
    float dx = ex - x, dy = ey - y;
    float distance2 = dx * dx + dy * dy;
    if (!near_wall && distance2 <= 4) {
        return bilinear(ex, ey);
    }
    return trace_walls(solid, x, y, ex, ey, distance2);
}

static void advect_velocity(FluidState *f, float dt, const uint8_t *near_wall)
{
    memcpy(f->u0, f->u, sizeof f->u);
    memcpy(f->v0, f->v, sizeof f->v);
    float limit = f->cfg.max_vel;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        for (int x = 1; x < FLUID_W - 1; ++x) {
            int i = x + y * FLUID_W;
            if (!f->solid[i]) {
                Stencil s = backtrace(f->solid, near_wall[i], x, y, f->u0[i], f->v0[i], dt);
                f->u[i] = clamp(interpolate(f->u0, s) * f->cfg.vel_damp, -limit, limit);
                f->v[i] = clamp(interpolate(f->v0, s) * f->cfg.vel_damp, -limit, limit);
            }
        }
    }
}

static void advect_scalars(FluidState *f, float dt, const uint8_t *near_wall)
{
    int channels = f->cfg.active_channels;
    memcpy(f->dye0, f->dye, (size_t)channels * sizeof f->dye[0]);
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
                Stencil s = backtrace(f->solid, near_wall[i], x, y, f->u[i], f->v[i], dt);
                for (int ch = 0; ch < channels; ++ch) {
                    float dye =
                        clamp(interpolate(f->dye0[ch], s) * f->cfg.dye_damp[ch], 0, f->cfg.max_dye);
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
    float inv_neighbors[FLUID_N];
    uint8_t near_wall[FLUID_N];
    prepare_geometry(f->solid, inv_neighbors, near_wall);
    f->stats = (FluidStats){0};
    confine(f, dt);
    project(f, inv_neighbors);
    advect_velocity(f, dt, near_wall);
    project(f, inv_neighbors);
    advect_scalars(f, dt, near_wall);
    ++f->steps;
    clock_gettime(CLOCK_MONOTONIC, &end);
    f->stats.solve_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) * 1e-6;
}
