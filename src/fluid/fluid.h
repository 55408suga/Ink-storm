#ifndef INK_FLUID_H
#define INK_FLUID_H

#include <stdbool.h>
#include <stdint.h>

#define FLUID_W 160
#define FLUID_H 90
#define FLUID_N (FLUID_W * FLUID_H)
#define FLUID_CELL_PX 8.0f
#define FLUID_MAX_CHANNELS 4

typedef struct {
    int project_iters;                  /* 既定: 12（調整可能） */
    float vorticity_eps;                /* 既定: 9.5f */
    float vorticity_thresh;             /* 既定: 0.35f */
    float max_vel;                      /* 既定: 125.0f */
    float max_dye;                      /* 既定: 2.5f */
    float vel_damp;                     /* 速度減衰 */
    float dye_damp[FLUID_MAX_CHANNELS]; /* チャンネル別減衰率 */
    int active_channels;                /* 既定: 3（最大 FLUID_MAX_CHANNELS） */
} FluidConfig;

typedef struct {
    float max_vel;
    float max_vort;
    float max_dye[FLUID_MAX_CHANNELS];
    double solve_ms; /* 計測専用（決定性ハッシュ対象外） */
} FluidStats;

typedef struct {
    FluidConfig cfg;
    FluidStats stats;
    uint64_t steps;
    float u[FLUID_N], v[FLUID_N];
    float u0[FLUID_N], v0[FLUID_N];
    float div[FLUID_N], p[FLUID_N], curl[FLUID_N];
    float dye[FLUID_MAX_CHANNELS][FLUID_N];
    float dye0[FLUID_MAX_CHANNELS][FLUID_N];
    uint8_t solid[FLUID_N]; /* 0=流体, 1=障害物/外周壁 */
} FluidState;

FluidConfig fluid_config_default(void);
void fluid_init(FluidState *f, const FluidConfig *cfg);
void fluid_reset(FluidState *f); /* 速度・染料を0初期化（solidは維持） */

/* 障害物（solid 境界）操作・クエリ */
void fluid_clear_solids(FluidState *f); /* 外周1セルのみ壁に戻す */
void fluid_set_solid_rect(FluidState *f, float px, float py, float pw, float ph, bool solid);
void fluid_set_solid_circle(FluidState *f, float px, float py, float radius_px, bool solid);
bool fluid_is_solid(const FluidState *f, float px, float py);
bool fluid_solid_normal(const FluidState *f, float px, float py, float *nx, float *ny);

/* 数値ステップ・注入・サンプリング */
void fluid_step(FluidState *f, float dt);
void fluid_splat(FluidState *f, float px, float py, float rad_cells, float fx, float fy,
                 const float *dye_add);
void fluid_swirl(FluidState *f, float px, float py, float rad_cells, float strength,
                 const float *dye_add);
void fluid_add_radial_impulse(FluidState *f, float px, float py, float rad_cells, float peak_speed);
void fluid_sample(const FluidState *f, float px, float py, float *out_u, float *out_v,
                  float *out_dye);

/* game/elements 向けの直列2フェーズ連携アクセサ */
const float *fluid_channel_const(const FluidState *f, int ch);
float *fluid_channel_mut(FluidState *f, int ch); /* 借用期限: 次回 fluid_step / fluid_reset まで */
void fluid_sanitize_channels(
    FluidState *f); /* elements_step 後に solid=0・[0,max_dye]・有限性を保証 */

/* 決定性・健全性検証 */
uint64_t fluid_hash(const FluidState *f);
bool fluid_validate_finite(const FluidState *f);

#endif
