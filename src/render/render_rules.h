#ifndef INK_RENDER_RULES_H
#define INK_RENDER_RULES_H

#include "fluid.h"

#include <stdbool.h>
#include <stdint.h>

/* raylib に依存しない描画判定・色計算（単体テスト対象） */

typedef enum { BG_LOBBY = 0, BG_ACT } BgKind;

typedef struct {
    BgKind kind;
    int32_t act_index; /* BG_LOBBY では常に 0 */
} BgKey;

BgKey render_bg_key(bool combat_world, int32_t act_index);
bool render_bg_key_equal(BgKey a, BgKey b);

/* GPU
 * 水墨流体シェーダはポスト処理が有効で、シェーダと和紙テクスチャ（texture1）が揃ったときだけ使う。
 * 和紙がないと texture1 が流体テクスチャの unit を指し、濃度を紙として読んでしまう */
bool render_fluid_use_gpu(bool post_enabled, unsigned int fluid_shader_id,
                          unsigned int paper_tex_id);

/* (ink, fire, storm, solid) を FLUID_W x FLUID_H の RGBA8 に詰める。濃度は max_dye で正規化して
 * sqrt で符号化し、淡墨域の 8bit 量子化段差を抑える（シェーダ側で二乗して復元） */
#define RENDER_FLUID_FIELD_BYTES (FLUID_N * 4)
void render_fluid_encode_field(const FluidState *f, unsigned char *rgba);
float render_fluid_decode_channel(unsigned char texel, float max_dye);

typedef struct {
    float r, g, b, a;
} RenderRgba;

typedef struct {
    RenderRgba sumi_clear, sumi_light, sumi_heavy, sumi_dark, sumi_scorch;
    RenderRgba shu, shu_hot, ao, ao_hot, kin_hot;
} RenderFluidPalette;

/* 墨の濃度曲線（art-direction §9.4）: rgb はトーン、a は不透明度。濃度 0.05 未満は a=0。
 * grad はセル単位の濃度勾配の大きさで、墨溜まりを足す。GPU シェーダも同じ式で評価する */
RenderRgba render_fluid_ink_curve(const RenderFluidPalette *pal, float ink, float grad);
/* CPU フォールバックの墨: 濃度曲線を紙へ乗算合成する色（a=1） */
RenderRgba render_fluid_cpu_ink(const RenderFluidPalette *pal, float ink, float grad);
/* CPU フォールバックの火・雷: 乗算済みアルファ（rgb <= a）。透明域が黒く滲まない */
RenderRgba render_fluid_cpu_elem(const RenderFluidPalette *pal, float fire, float storm);

typedef struct {
    int32_t seg; /* 区間番号 */
    float t;     /* 区間内の位置 [0, 1) */
} RenderPolySample;

/* 折れ線を最大 cap - 1 点でサンプルする位置を決める（終点は呼び出し側で足す）。区間の長さに応じて
 * 2〜24 点を配り、予算を超えるときは全区間で分け合う。区間数が予算以上なら全区間の累積長に沿って
 * 等間隔に配り、末尾の区間を弦で省略しない。戻り値はサンプル数 */
int render_polyline_plan(const float *seg_len, int segs, int cap, RenderPolySample *out);
int render_polyline_segment_steps(float seg_len); /* 予算に余裕があるときの区間あたり点数 */

/* 第 3 幕雨アニメーションの独立周期（1340px 水平 / 800px 垂直で途切れなく循環） */
#define RENDER_RAIN_PERIOD_X ((float)INK_SCREEN_W + 60.0f)
#define RENDER_RAIN_PERIOD_Y ((float)INK_SCREEN_H + 80.0f)
void render_rain_advance(float *phase_x, float *phase_y, float dt);
void render_rain_streak_pos(int streak_idx, float phase_x, float phase_y, float *out_x,
                            float *out_y);

/* 流体テクスチャ転送キャッシュ（静止・ポーズ・未描画ロビーで再符号化と転送を省く） */
typedef struct {
    bool valid;
    const FluidState *field_ptr;
    uint64_t instance_id;
    uint64_t steps;
    uint64_t mutation_rev;
    float max_dye;
    bool gpu_path;
    int32_t slot;
} FluidUploadCache;

bool render_fluid_cache_matches(const FluidUploadCache *cache, const FluidState *f, bool gpu_path);

#endif
