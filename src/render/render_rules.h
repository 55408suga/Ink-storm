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

/* CPU フォールバックの墨: 乗算合成用の色（a=1）。grad はセル単位の濃度勾配の大きさ */
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

#endif
