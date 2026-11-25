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

/* GPU 水墨流体シェーダはポスト処理が有効で、シェーダが読み込めたときだけ使う */
bool render_fluid_use_gpu(bool post_enabled, unsigned int fluid_shader_id);

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

/* 折れ線の区間ごとのサンプル数を決め、合計が cap - 1 を超えないよう全区間に配分する。
 * 戻り値は終点を含めた総サンプル数 */
int render_polyline_plan(const float *seg_len, int segs, int cap, int *steps);

#endif
