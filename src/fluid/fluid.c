#include "fluid.h"

#include <float.h>
#include <math.h>
#include <string.h>

static float clamp(float value, float lo, float hi)
{
    return value < lo ? lo : value > hi ? hi : value;
}

static int clamp_count(int value, int limit)
{
    return value < 0 ? 0 : value > limit ? limit : value;
}

FluidConfig fluid_config_default(void)
{
    return (FluidConfig){
        .project_iters = 12,
        .vorticity_eps = 9.5f,
        .vorticity_thresh = 0.35f,
        .max_vel = 125.0f,
        .max_dye = 2.5f,
        .vel_damp = 0.985f,
        .dye_damp = {0.985f, 0.935f, 0.905f, 0.950f},
        .active_channels = 3,
    };
}

static float damping(float value, float fallback)
{
    return isfinite(value) ? clamp(value, 0.0f, 1.0f) : fallback;
}

void fluid_init(FluidState *f, const FluidConfig *cfg)
{
    if (!f) {
        return;
    }
    const FluidConfig defaults = fluid_config_default();
    /* Take a copy first so fluid_init(f, &f->cfg) is also valid. */
    FluidConfig c = cfg ? *cfg : defaults;
    c.project_iters = clamp_count(c.project_iters, 64);
    c.active_channels = clamp_count(c.active_channels, FLUID_MAX_CHANNELS);
    if (!isfinite(c.vorticity_eps) || c.vorticity_eps < 0.0f) {
        c.vorticity_eps = defaults.vorticity_eps;
    }
    if (!isfinite(c.vorticity_thresh) || c.vorticity_thresh < 0.0f) {
        c.vorticity_thresh = defaults.vorticity_thresh;
    }
    if (!isfinite(c.max_vel) || c.max_vel <= 0.0f) {
        c.max_vel = defaults.max_vel;
    }
    if (!isfinite(c.max_dye) || c.max_dye <= 0.0f) {
        c.max_dye = defaults.max_dye;
    }
    c.vel_damp = damping(c.vel_damp, defaults.vel_damp);
    for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
        c.dye_damp[ch] = damping(c.dye_damp[ch], defaults.dye_damp[ch]);
    }
    memset(f, 0, sizeof *f);
    f->cfg = c;
    f->pristine = 1;
    fluid_clear_solids(f);
    f->mutation_rev = 1;
    f->solid_rev = 1;
}

void fluid_reset(FluidState *f)
{
    if (!f) {
        return;
    }
    f->stats = (FluidStats){0};
    f->steps = 0;
    memset(f->u, 0, sizeof f->u);
    memset(f->v, 0, sizeof f->v);
    memset(f->u0, 0, sizeof f->u0);
    memset(f->v0, 0, sizeof f->v0);
    memset(f->div, 0, sizeof f->div);
    memset(f->p, 0, sizeof f->p);
    memset(f->curl, 0, sizeof f->curl);
    memset(f->dye, 0, sizeof f->dye);
    memset(f->dye0, 0, sizeof f->dye0);
    f->pristine = 1;
    f->mut_borrow_depth = 0;
    ++f->mutation_rev;
    if (f->mutation_rev == 0) {
        f->mutation_rev = 1;
    }
}

static void set_solid(FluidState *f, int i, bool solid)
{
    const uint8_t next = solid ? 1u : 0u;
    if (f->solid[i] != next) {
        f->solid[i] = next;
        f->geometry_valid = 0;
        ++f->solid_rev;
        ++f->mutation_rev;
    }
    if (solid) {
        f->u[i] = 0.0f;
        f->v[i] = 0.0f;
        for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
            f->dye[ch][i] = 0.0f;
        }
    }
}

void fluid_clear_solids(FluidState *f)
{
    if (!f) {
        return;
    }
    for (int y = 0; y < FLUID_H; ++y) {
        for (int x = 0; x < FLUID_W; ++x) {
            bool edge = x == 0 || x == FLUID_W - 1 || y == 0 || y == FLUID_H - 1;
            set_solid(f, y * FLUID_W + x, edge);
        }
    }
    f->geometry_valid = 0;
}

static float center_px(int cell) { return ((float)cell + 0.5f) * FLUID_CELL_PX; }

void fluid_set_solid_rect(FluidState *f, float px, float py, float pw, float ph, bool solid)
{
    if (!f || !isfinite(px) || !isfinite(py) || !isfinite(pw) || !isfinite(ph) || pw <= 0.0f ||
        ph <= 0.0f) {
        return;
    }
    const double right = (double)px + pw;
    const double bottom = (double)py + ph;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        float cy = center_px(y);
        if (cy < py || (cy != py && cy >= bottom)) {
            continue;
        }
        for (int x = 1; x < FLUID_W - 1; ++x) {
            float cx = center_px(x);
            if (cx >= px && (cx == px || cx < right)) {
                set_solid(f, y * FLUID_W + x, solid);
            }
        }
    }
}

void fluid_set_solid_circle(FluidState *f, float px, float py, float radius_px, bool solid)
{
    if (!f || !isfinite(px) || !isfinite(py) || !isfinite(radius_px) || radius_px <= 0.0f) {
        return;
    }
    const double r2 = (double)radius_px * radius_px;
    for (int y = 1; y < FLUID_H - 1; ++y) {
        if (center_px(y) < (double)py - radius_px || center_px(y) > (double)py + radius_px) {
            continue;
        }
        const double dy = (double)center_px(y) - py;
        for (int x = 1; x < FLUID_W - 1; ++x) {
            if (center_px(x) < (double)px - radius_px || center_px(x) > (double)px + radius_px) {
                continue;
            }
            const double dx = (double)center_px(x) - px;
            if (dx * dx + dy * dy <= r2) {
                set_solid(f, y * FLUID_W + x, solid);
            }
        }
    }
}

bool fluid_is_solid(const FluidState *f, float px, float py)
{
    if (!f) {
        return false;
    }
    if (!isfinite(px) || !isfinite(py) || px < 0 || py < 0 || px >= FLUID_W * FLUID_CELL_PX ||
        py >= FLUID_H * FLUID_CELL_PX) {
        return true;
    }
    /* With non power-of-two cell sizes the quotient of an in-range coordinate can round up to the
     * grid size, so the index is clamped after the division. */
    const int x = clamp_count((int)(px / FLUID_CELL_PX), FLUID_W - 1);
    const int y = clamp_count((int)(py / FLUID_CELL_PX), FLUID_H - 1);
    return f->solid[y * FLUID_W + x] != 0;
}

int fluid_cell_reach(float radius_px)
{
    if (isnan(radius_px) || radius_px <= 0.0f) {
        return 0;
    }
    return clamp_count((int)ceilf(fminf(radius_px / FLUID_CELL_PX, (float)FLUID_W)), FLUID_W);
}

static void sum_fluid_offsets(const FluidState *f, int x, int y, int reach, int *sx, int *sy)
{
    for (int dy = -reach; dy <= reach; ++dy) {
        int row = y + dy;
        if (row < 0 || row >= FLUID_H) {
            continue;
        }
        for (int dx = -reach; dx <= reach; ++dx) {
            int col = x + dx;
            if (col >= 0 && col < FLUID_W && !f->solid[row * FLUID_W + col]) {
                *sx += dx;
                *sy += dy;
            }
        }
    }
}

bool fluid_solid_normal(const FluidState *f, float px, float py, float *nx, float *ny)
{
    int sx = 0, sy = 0;
    if (f && isfinite(px) && isfinite(py)) {
        int x = (int)clamp(px / FLUID_CELL_PX, 0, FLUID_W - 1);
        int y = (int)clamp(py / FLUID_CELL_PX, 0, FLUID_H - 1);
        sum_fluid_offsets(f, x, y, 1, &sx, &sy);
        if (sx == 0 && sy == 0 && fluid_is_solid(f, px, py)) {
            sum_fluid_offsets(f, x, y, 2, &sx, &sy);
        }
    }
    bool found = sx != 0 || sy != 0;
    float scale = found ? 1.0f / sqrtf((float)(sx * sx + sy * sy)) : 0.0f;
    if (nx) {
        *nx = sx * scale;
    }
    if (ny) {
        *ny = sy * scale;
    }
    return found;
}

static bool cell_is_solid(const FluidState *f, int cx, int cy)
{
    return cx < 0 || cx >= FLUID_W || cy < 0 || cy >= FLUID_H || f->solid[cy * FLUID_W + cx] != 0;
}

static void place_clear_point(const FluidState *f, int clear_cx, int clear_cy, double x0_px,
                              double y0_px, double dx_px, double dy_px, double t_hit, float nx,
                              float ny, float *out_x, float *out_y)
{
    const double eps_px = 0.25;
    const float cand_x = (float)(x0_px + dx_px * t_hit + (double)nx * eps_px);
    const float cand_y = (float)(y0_px + dy_px * t_hit + (double)ny * eps_px);
    if (!fluid_is_solid(f, cand_x, cand_y)) {
        *out_x = cand_x;
        *out_y = cand_y;
        return;
    }
    const double back_t = t_hit > 1e-3 ? t_hit - 1e-3 : 0.0;
    const float back_x = (float)(x0_px + dx_px * back_t);
    const float back_y = (float)(y0_px + dy_px * back_t);
    if (!fluid_is_solid(f, back_x, back_y)) {
        *out_x = back_x;
        *out_y = back_y;
        return;
    }
    *out_x = ((float)clear_cx + 0.5f) * FLUID_CELL_PX;
    *out_y = ((float)clear_cy + 0.5f) * FLUID_CELL_PX;
}

static double clamp_d(double value, double lo, double hi)
{
    return value < lo ? lo : value > hi ? hi : value;
}

FluidRayHit fluid_trace_solid_segment(const FluidState *f, float x0_px, float y0_px, float x1_px,
                                      float y1_px)
{
    FluidRayHit out = {
        .hit = false,
        .t = 1.0f,
        .hit_x = x1_px,
        .hit_y = y1_px,
        .nx = 0.0f,
        .ny = 0.0f,
        .cell_x = -1,
        .cell_y = -1,
    };
    if (!f || !isfinite(x0_px) || !isfinite(y0_px) || !isfinite(x1_px) || !isfinite(y1_px)) {
        return out;
    }

    const double sx = (double)x0_px / (double)FLUID_CELL_PX;
    const double sy = (double)y0_px / (double)FLUID_CELL_PX;
    const double fx = floor(sx);
    const double fy = floor(sy);
    if (fx < 0.0 || fx >= (double)FLUID_W || fy < 0.0 || fy >= (double)FLUID_H) {
        out.hit = true;
        out.t = 0.0f;
        out.hit_x = x0_px;
        out.hit_y = y0_px;
        out.cell_x = (int)clamp_d(fx, -1.0, (double)FLUID_W);
        out.cell_y = (int)clamp_d(fy, -1.0, (double)FLUID_H);
        (void)fluid_solid_normal(f, x0_px, y0_px, &out.nx, &out.ny);
        return out;
    }

    int cx = (int)fx;
    int cy = (int)fy;
    if (cell_is_solid(f, cx, cy)) {
        out.hit = true;
        out.t = 0.0f;
        out.hit_x = x0_px;
        out.hit_y = y0_px;
        out.cell_x = cx;
        out.cell_y = cy;
        (void)fluid_solid_normal(f, x0_px, y0_px, &out.nx, &out.ny);
        return out;
    }

    const double ex = (double)x1_px / (double)FLUID_CELL_PX;
    const double ey = (double)y1_px / (double)FLUID_CELL_PX;
    const double dx = ex - sx;
    const double dy = ey - sy;
    const double dx_px = (double)x1_px - (double)x0_px;
    const double dy_px = (double)y1_px - (double)y0_px;

    const int step_x = dx > 0.0 ? 1 : (dx < 0.0 ? -1 : 0);
    const int step_y = dy > 0.0 ? 1 : (dy < 0.0 ? -1 : 0);
    if (step_x == 0 && step_y == 0) {
        return out;
    }

    double t_delta_x = DBL_MAX;
    double t_max_x = DBL_MAX;
    if (step_x > 0) {
        t_delta_x = 1.0 / dx;
        t_max_x = ((double)(cx + 1) - sx) * t_delta_x;
    } else if (step_x < 0) {
        t_delta_x = -1.0 / dx;
        t_max_x = (sx - (double)cx) * t_delta_x;
    }

    double t_delta_y = DBL_MAX;
    double t_max_y = DBL_MAX;
    if (step_y > 0) {
        t_delta_y = 1.0 / dy;
        t_max_y = ((double)(cy + 1) - sy) * t_delta_y;
    } else if (step_y < 0) {
        t_delta_y = -1.0 / dy;
        t_max_y = (sy - (double)cy) * t_delta_y;
    }

    const int max_steps = FLUID_W + FLUID_H + 4;
    for (int iter = 0; iter < max_steps; ++iter) {
        const double t_next = t_max_x < t_max_y ? t_max_x : t_max_y;
        if (t_next > 1.0) {
            break;
        }
        const double tie = fabs(t_max_x - t_max_y);
        if (tie <= 1e-6) {
            const int nx_c = cx + step_x;
            const int ny_c = cy + step_y;
            const bool sx_solid = cell_is_solid(f, nx_c, cy);
            const bool sy_solid = cell_is_solid(f, cx, ny_c);
            const bool diag_solid = cell_is_solid(f, nx_c, ny_c);
            if (sx_solid || sy_solid || diag_solid) {
                const double t_hit = clamp_d(t_next, 0.0, 1.0);
                out.hit = true;
                out.t = (float)t_hit;
                if (sx_solid && !sy_solid) {
                    out.nx = -(float)step_x;
                    out.ny = 0.0f;
                    out.cell_x = nx_c;
                    out.cell_y = cy;
                } else if (sy_solid && !sx_solid) {
                    out.nx = 0.0f;
                    out.ny = -(float)step_y;
                    out.cell_x = cx;
                    out.cell_y = ny_c;
                } else {
                    static const float k_inv_sqrt2 = 0.70710678f;
                    out.nx = -(float)step_x * k_inv_sqrt2;
                    out.ny = -(float)step_y * k_inv_sqrt2;
                    out.cell_x = nx_c;
                    out.cell_y = ny_c;
                }
                place_clear_point(f, cx, cy, (double)x0_px, (double)y0_px, dx_px, dy_px, t_hit,
                                  out.nx, out.ny, &out.hit_x, &out.hit_y);
                return out;
            }
            cx = nx_c;
            cy = ny_c;
            t_max_x += t_delta_x;
            t_max_y += t_delta_y;
        } else if (t_max_x < t_max_y) {
            const int nx_c = cx + step_x;
            if (cell_is_solid(f, nx_c, cy)) {
                const double t_hit = clamp_d(t_max_x, 0.0, 1.0);
                out.hit = true;
                out.t = (float)t_hit;
                out.nx = -(float)step_x;
                out.ny = 0.0f;
                out.cell_x = nx_c;
                out.cell_y = cy;
                place_clear_point(f, cx, cy, (double)x0_px, (double)y0_px, dx_px, dy_px, t_hit,
                                  out.nx, out.ny, &out.hit_x, &out.hit_y);
                return out;
            }
            cx = nx_c;
            t_max_x += t_delta_x;
        } else {
            const int ny_c = cy + step_y;
            if (cell_is_solid(f, cx, ny_c)) {
                const double t_hit = clamp_d(t_max_y, 0.0, 1.0);
                out.hit = true;
                out.t = (float)t_hit;
                out.nx = 0.0f;
                out.ny = -(float)step_y;
                out.cell_x = cx;
                out.cell_y = ny_c;
                place_clear_point(f, cx, cy, (double)x0_px, (double)y0_px, dx_px, dy_px, t_hit,
                                  out.nx, out.ny, &out.hit_x, &out.hit_y);
                return out;
            }
            cy = ny_c;
            t_max_y += t_delta_y;
        }
    }
    return out;
}

bool fluid_is_pristine(const FluidState *f) { return f != NULL && f->pristine != 0; }

bool fluid_has_open_mut_borrow(const FluidState *f)
{
    return f != NULL && f->mut_borrow_depth > 0u;
}

typedef enum { SPLAT, SWIRL, RADIAL } Injection;

static void inject(FluidState *f, Injection kind, float px, float py, float radius, float fx,
                   float fy, const float *dye_add)
{
    if (!f || !isfinite(px) || !isfinite(py) || !isfinite(radius) || radius <= 0.0f ||
        !isfinite(fx) || !isfinite(fy)) {
        return;
    }
    if (dye_add) {
        for (int ch = 0; ch < f->cfg.active_channels; ++ch) {
            if (!isfinite(dye_add[ch])) {
                return;
            }
        }
    }
    radius = fminf(radius, FLUID_W + FLUID_H);
    const float cx = px / FLUID_CELL_PX - 0.5f;
    const float cy = py / FLUID_CELL_PX - 0.5f;
    const int xmin = (int)clamp(cx - radius, 1, FLUID_W - 2);
    const int xmax = (int)clamp(cx + radius, 1, FLUID_W - 2);
    const int ymin = (int)clamp(cy - radius, 1, FLUID_H - 2);
    const int ymax = (int)clamp(cy + radius, 1, FLUID_H - 2);
    const float r2 = radius * radius;

    bool modified = false;
    for (int y = ymin; y <= ymax; ++y) {
        for (int x = xmin; x <= xmax; ++x) {
            int i = y * FLUID_W + x;
            float dx = x - cx, dy = y - cy;
            float d2 = dx * dx + dy * dy;
            if (d2 > r2 || f->solid[i]) {
                continue;
            }
            float du, dv, dye_weight = 0.0f;
            if (kind == SPLAT) {
                /* At the exact center this also handles radii whose square underflows. */
                dye_weight = d2 == 0.0f ? 1.0f : expf(-d2 / (0.55f * r2));
                du = fx * dye_weight;
                dv = fy * dye_weight;
            } else {
                float d = sqrtf(d2);
                if (kind == SWIRL) {
                    static const float k_pi = 3.14159265f;
                    if (d < 0.35f) {
                        continue;
                    }
                    float ring = sinf(k_pi * d / radius);
                    du = (-dy / d) * fx * ring;
                    dv = (dx / d) * fx * ring;
                    dye_weight = ring * ring;
                } else {
                    if (d < 0.25f) {
                        continue;
                    }
                    float speed = fx * (1.0f - d / radius);
                    du = speed * (dx / d);
                    dv = speed * (dy / d);
                }
            }
            const float next_u = clamp(f->u[i] + du, -f->cfg.max_vel, f->cfg.max_vel);
            const float next_v = clamp(f->v[i] + dv, -f->cfg.max_vel, f->cfg.max_vel);
            if (next_u != f->u[i] || next_v != f->v[i]) {
                modified = true;
            }
            f->u[i] = next_u;
            f->v[i] = next_v;
            if (dye_add) {
                for (int ch = 0; ch < f->cfg.active_channels; ++ch) {
                    const float next_d =
                        clamp(f->dye[ch][i] + dye_add[ch] * dye_weight, 0.0f, f->cfg.max_dye);
                    if (next_d != f->dye[ch][i]) {
                        modified = true;
                    }
                    f->dye[ch][i] = next_d;
                }
            }
        }
    }
    if (modified) {
        f->pristine = 0;
        ++f->mutation_rev;
    }
}

void fluid_splat(FluidState *f, float px, float py, float rad_cells, float fx, float fy,
                 const float *dye_add)
{
    inject(f, SPLAT, px, py, rad_cells, fx, fy, dye_add);
}

void fluid_swirl(FluidState *f, float px, float py, float rad_cells, float strength,
                 const float *dye_add)
{
    inject(f, SWIRL, px, py, rad_cells, strength, 0.0f, dye_add);
}

void fluid_add_radial_impulse(FluidState *f, float px, float py, float rad_cells, float peak_speed)
{
    inject(f, RADIAL, px, py, rad_cells, peak_speed, 0.0f, NULL);
}

const float *fluid_channel_const(const FluidState *f, int ch)
{
    return f && ch >= 0 && ch < FLUID_MAX_CHANNELS ? f->dye[ch] : NULL;
}

float *fluid_channel_mut(FluidState *f, int ch)
{
    /* Inactive channels stay zero only while nothing outside the solver can write them, which lets
     * fluid_sanitize_channels skip them. */
    if (f && ch >= 0 && ch < f->cfg.active_channels) {
        f->pristine = 0;
        if (f->mut_borrow_depth < UINT32_MAX) {
            ++f->mut_borrow_depth;
        }
        ++f->mutation_rev;
        return f->dye[ch];
    }
    return NULL;
}

void fluid_end_mut_borrow(FluidState *f)
{
    if (!f) {
        return;
    }
    if (f->mut_borrow_depth > 0u) {
        --f->mut_borrow_depth;
    }
    ++f->mutation_rev;
}

const float *fluid_u_const(const FluidState *f) { return f ? f->u : NULL; }

const float *fluid_v_const(const FluidState *f) { return f ? f->v : NULL; }

const uint8_t *fluid_solid_const(const FluidState *f) { return f ? f->solid : NULL; }

void fluid_sanitize_channels(FluidState *f)
{
    if (!f) {
        return;
    }
    bool changed = false;
    for (int ch = 0; ch < f->cfg.active_channels; ++ch) {
        for (int i = 0; i < FLUID_N; ++i) {
            float value = f->dye[ch][i];
            float next = f->solid[i] || !isfinite(value) ? 0.0f : clamp(value, 0, f->cfg.max_dye);
            if (next != value || isnan(value)) {
                f->dye[ch][i] = next;
                changed = true;
            }
        }
    }
    if (changed) {
        ++f->mutation_rev;
    }
}

bool fluid_validate_finite(const FluidState *f)
{
    if (!f) {
        return false;
    }
    for (int i = 0; i < FLUID_N; ++i) {
        if (!isfinite(f->u[i]) || !isfinite(f->v[i]) || !isfinite(f->u0[i]) ||
            !isfinite(f->v0[i]) || !isfinite(f->div[i]) || !isfinite(f->p[i]) ||
            !isfinite(f->curl[i]) || (f->solid[i] && (f->u[i] != 0 || f->v[i] != 0))) {
            return false;
        }
        for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
            if (!isfinite(f->dye[ch][i]) || !isfinite(f->dye0[ch][i]) ||
                (f->solid[i] && f->dye[ch][i] != 0)) {
                return false;
            }
        }
    }
    return true;
}

static uint64_t hash_byte(uint64_t h, uint8_t b) { return (h ^ b) * UINT64_C(0x100000001b3); }

static uint64_t hash_u32(uint64_t h, uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        h = hash_byte(h, (uint8_t)(value >> shift));
    }
    return h;
}

static uint64_t hash_float(uint64_t h, float value)
{
    uint32_t bits = 0;
    _Static_assert(sizeof bits == sizeof value, "fluid_hash needs 32-bit floats");
    if (value != 0.0f) {
        memcpy(&bits, &value, sizeof bits);
    }
    return hash_u32(h, bits);
}

static uint64_t hash_floats(uint64_t h, const float *values, int count)
{
    for (int i = 0; i < count; ++i) {
        h = hash_float(h, values[i]);
    }
    return h;
}

uint64_t fluid_hash(const FluidState *f)
{
    if (!f) {
        return 0;
    }
    uint64_t h = UINT64_C(0xcbf29ce484222325);
    h = hash_u32(h, (uint32_t)f->steps);
    h = hash_u32(h, (uint32_t)(f->steps >> 32));
    h = hash_byte(h, f->pristine ? 1u : 0u);
    const FluidConfig *c = &f->cfg;
    h = hash_u32(h, (uint32_t)c->project_iters);
    h = hash_float(h, c->vorticity_eps);
    h = hash_float(h, c->vorticity_thresh);
    h = hash_float(h, c->max_vel);
    h = hash_float(h, c->max_dye);
    h = hash_float(h, c->vel_damp);
    h = hash_floats(h, c->dye_damp, FLUID_MAX_CHANNELS);
    h = hash_u32(h, (uint32_t)c->active_channels);
    for (int i = 0; i < FLUID_N; ++i) {
        h = hash_byte(h, f->solid[i]);
    }
    h = hash_floats(h, f->u, FLUID_N);
    h = hash_floats(h, f->v, FLUID_N);
    for (int ch = 0; ch < FLUID_MAX_CHANNELS; ++ch) {
        h = hash_floats(h, f->dye[ch], FLUID_N);
    }
    return h;
}
