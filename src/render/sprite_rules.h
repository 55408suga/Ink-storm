#ifndef INK_RENDER_SPRITE_RULES_H
#define INK_RENDER_SPRITE_RULES_H

#include "game_ids.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* raylib
 * に依存しないスプライトの規則（マニフェスト・状態選択・変換・縁取り・アトラス配置・龍の背骨）。
 * 単体テスト対象 */

#define SPRITE_ATLAS_PX 2048
#define SPRITE_CANVAS_MAX 512
#define SPRITE_TURN_TICKS 6
#define RENDER_RYU_SPINE_PTS 64
#define SPRITE_PNG_MAX_IDAT_BYTES (2u * 1024u * 1024u)

typedef enum {
    SPR_NONE = -1,
    SPR_SHRINE = 0,
    SPR_ARCHER_IDLE,
    SPR_ARCHER_DRAW,
    SPR_LANTERN_FIRE,
    SPR_LANTERN_STORM,
    SPR_UMBRELLA_HOP,
    SPR_UMBRELLA_CROUCH,
    SPR_KAMAITACHI,
    SPR_STONE_ONI,
    SPR_INK_LEECH,
    SPR_OFUDA_FOX,
    SPR_OFUDA_FOX_CAST,
    SPR_TENGU_IDLE,
    SPR_TENGU_WINDUP,
    SPR_NUE_PROWL,
    SPR_NUE_CROUCH,
    SPR_RYU_HEAD_DORMANT,
    SPR_RYU_HEAD_AWAKENED,
    SPR_HOJU_FIRE,
    SPR_HOJU_STORM,
    SPR_HOJU_WIND,
    SPR_COUNT
} SpriteId;

typedef enum {
    SPR_GROUP_SHRINE = 0,
    SPR_GROUP_ARCHER,
    SPR_GROUP_LANTERN_FIRE,
    SPR_GROUP_LANTERN_STORM,
    SPR_GROUP_UMBRELLA,
    SPR_GROUP_KAMAITACHI,
    SPR_GROUP_STONE_ONI,
    SPR_GROUP_INK_LEECH,
    SPR_GROUP_OFUDA_FOX,
    SPR_GROUP_TENGU,
    SPR_GROUP_NUE,
    SPR_GROUP_RYU,
    SPR_GROUP_HOJU,
    SPR_GROUP_COUNT
} SpriteGroupId;

typedef enum {
    SPR_ORIENT_NONE = 0,
    SPR_ORIENT_MIRROR_X,
    SPR_ORIENT_FACE_SHRINE,
    SPR_ORIENT_ROTATE_MIRROR
} SpriteOrient;

typedef enum {
    SPR_ANCHOR_FLAME = 0,
    SPR_ANCHOR_BOW_GRIP,
    SPR_ANCHOR_NOCK,
    SPR_ANCHOR_GLOW,
    SPR_ANCHOR_MOUTH,
    SPR_ANCHOR_TRAIL_ROOT,
    SPR_ANCHOR_FOOT_L,
    SPR_ANCHOR_FOOT_R,
    SPR_ANCHOR_TAIL_ROOT,
    SPR_ANCHOR_TALISMAN,
    SPR_ANCHOR_FAN_TIP,
    SPR_ANCHOR_FACE,
    SPR_ANCHOR_EYE_L,
    SPR_ANCHOR_EYE_R,
    SPR_ANCHOR_NECK,
    SPR_ANCHOR_WHISKER_A,
    SPR_ANCHOR_WHISKER_B,
    SPR_ANCHOR_CORE,
    SPR_ANCHOR_COUNT
} SpriteAnchorId;

/* キャンバス座標 [0, 1]。原点は左上、v は下向き。当たり判定の中心は (0.5, 0.5) */
typedef struct {
    float u, v;
} SpriteUv;

typedef struct {
    float u, v;
    bool used;
} SpriteAnchor;

typedef struct {
    const char *file; /* sprite_dir 直下の小文字スネークケースの ASCII 名 */
    int32_t canvas;   /* 256 または 512 */
    SpriteGroupId group;
    bool group_required; /* 欠けると、その群のキャラクター全体を手続き描画に戻す */
    bool optional;       /* 中核 20 枚に数えない差分 */
    SpriteId fallback;   /* 状態差分が欠けたときに使う基本スプライト */
    float body_frac;     /* k: 当たり判定の直径 / キャンバス幅 */
    float fixed_size_px; /* > 0 なら 2r/k の代わりに使う（社） */
    float ref_radius;    /* 縁取りの膨張量を決める基準半径 */
    SpriteUv foot;
    SpriteOrient orient;
    float halo_pad_px;
    float breath_amp, breath_period_s;
    bool jelly; /* x と y を同じ振幅で逆位相に伸縮する */
    SpriteAnchor anchor[SPR_ANCHOR_COUNT];
    SpriteUv extent;               /* 灯り・祠の窓・墨晶・凪の半径（キャンバス単位） */
    SpriteUv horn_a[3], horn_b[3]; /* 龍の角の金線。他は未使用 */
} SpriteDef;

const SpriteDef *sprite_def(SpriteId id);
bool sprite_anchor(const SpriteDef *d, SpriteAnchorId a, SpriteUv *out);
int32_t sprite_core_count(void);
float sprite_display_size(const SpriteDef *d, float radius);
void sprite_halo_params(const SpriteDef *d, int32_t *pad_tex, int32_t *feather_tex);

/* 群の必須メンバーがすべて揃った群だけを有効にする。戻り値は有効な群の数 */
int32_t sprite_groups_resolve(const bool present[SPR_COUNT], bool group_ok[SPR_GROUP_COUNT]);
/* 描けるスプライトを返す。状態差分が欠ければ基本スプライト、群が欠ければ SPR_NONE */
SpriteId sprite_resolve(SpriteId id, const bool present[SPR_COUNT],
                        const bool group_ok[SPR_GROUP_COUNT]);

typedef struct {
    EnemyDefId def_id;
    float windup;
    int32_t ai_state;
    int32_t phase_milli;
    float telegraph;
    int32_t phase_idx;
    int32_t part_id;
} SpriteStateIn;

SpriteId sprite_select(const SpriteStateIn *in); /* SPR_NONE: スプライトを持たない */
float sprite_umbrella_hop(int32_t phase_milli);  /* 跳ねの高さ h ∈ [0, 1] */

typedef struct {
    float x, y; /* 当たり判定の中心（ワールド px） */
    float size; /* S */
    float sx, sy;
    float rot; /* 足元の支点まわりのラジアン */
    bool flip_v;
    float dx, dy;
} SpriteXform;

typedef struct {
    float x[4], y[4], u[4], v[4];
} SpriteQuad;

/* 1 体を 1 フレーム描くための計画。縁取り・体・乗算・芯の各段で同じものを使う */
typedef struct {
    SpriteId id;
    const SpriteDef *def;
    SpriteXform xf;
    float body_a;
    float halo_a;
    float dissolve; /* 生存中は 0、墨還り中は 0..1 */
    bool dissolving;
    float face; /* 反転した部品を置く画面 x 方向の ±1 */
} SpritePlan;

void sprite_xform_point(const SpriteDef *d, const SpriteXform *xf, SpriteUv p, float *wx,
                        float *wy);
/* (u0, v0)-(u1, v1) はアトラス上の UV。頂点順は画面上で常に表向きになる。xf が NULL なら 0 で埋める
 */
void sprite_quad(const SpriteDef *d, const SpriteXform *xf, float u0, float v0, float u1, float v1,
                 SpriteQuad *out);

void sprite_breath(float amp, float period_s, float t_s, float phase, bool jelly, float *sx,
                   float *sy);

typedef struct {
    float dx, dy, rot, scale;
} SpriteBoil;

/* 支点から 0.8·size 以内の点の揺れを 0.8px 以内に抑える */
SpriteBoil sprite_boil(uint32_t seed, float size_px);

int8_t sprite_face_update(int8_t prev, float cos_facing);
float sprite_turn_sx(int8_t face, uint32_t elapsed_ticks);

/* alpha (w*h) から半分の解像度のマスク ((w/2)*(h/2)) を作る。scratch は 2*(w/2)*(h/2) バイト以上。
 * 不正な寸法では何も書かずに false を返す。内部で確保しない */
bool sprite_halo_from_alpha(const uint8_t *alpha, int32_t w, int32_t h, int32_t pad_tex,
                            int32_t feather_tex, uint8_t *out, uint8_t *scratch);

typedef struct {
    int32_t x, y, w, h;
} SpriteRect;

bool sprite_atlas_slot(SpriteId id, bool halo, SpriteRect *out);
bool sprite_join_path(const char *dir, const char *file, char *out, size_t size);
/* PNG の IHDR から寸法を読む。展開前に寸法違いを弾くために使う */
bool sprite_png_dimensions(const uint8_t *data, size_t size, int32_t *w, int32_t *h);
/* 展開前の厳密な検査: 8bit RGBA・非インターレースで寸法が一致し、全チャンクが入力内に収まり、
 * 空でない IDAT（合計 SPRITE_PNG_MAX_IDAT_BYTES 以下）と IEND があること */
bool sprite_png_validate(const uint8_t *data, size_t size, int32_t expect_w, int32_t expect_h);
/* 不透明な画素があり、縁に透明な画素が残ること。全面ベタや空の絵を弾く */
bool sprite_alpha_usable(const uint8_t *rgba, int32_t w, int32_t h);

float sprite_polyline_length(const GameVec2 *pts, int32_t n);
/* 弧長で等間隔に out_n 点へ置き直す。長さ 0 や点不足では 0 を返す */
int32_t sprite_resample_polyline(const GameVec2 *in, int32_t n, GameVec2 *out, int32_t out_n);
/* 先頭から弧長 d_neck までを首の差し込み口 socket に置き換える。常に socket から始め、
 * 軌跡が d_neck より短ければ socket だけの 1 点を返す */
int32_t sprite_spine_socket(const GameVec2 *in, int32_t n, GameVec2 socket, float d_neck,
                            GameVec2 *out, int32_t cap);
float sprite_polyline_nearest_arc(const GameVec2 *pts, int32_t n, GameVec2 q);

typedef struct {
    float amp, wavelength, period_s;
} SpriteSpineWave;

SpriteSpineWave sprite_ryu_wave(bool awakened);
float sprite_ryu_spine_width(bool awakened, float t_s);
/* うねりの包絡: 首で 0 から立ち上がり、尾へ向けて増し、玉の位置で 0 に留める */
float sprite_spine_envelope(float s, float total_len, const float *orb_s, int32_t orb_n);
/* in と out は同じ配列でもよい */
void sprite_spine_undulate(const GameVec2 *in, int32_t n, const SpriteSpineWave *w, float t_s,
                           const float *orb_s, int32_t orb_n, GameVec2 *out);

#endif
