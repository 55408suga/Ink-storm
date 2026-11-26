#ifndef INK_RENDER_H
#define INK_RENDER_H

#include "game.h"
#include "raylib.h"
#include "text_table.h"
#include "ui_view.h"
#include "vfx_rules.h"

#include <stdbool.h>
#include <stdint.h>

#define RENDER_MAX_SPARKS 512
#define RENDER_MAX_BOLTS 32
#define RENDER_MAX_SEALS 16
#define RENDER_MAX_CALLOUTS 24
#define RENDER_MAX_STROKES 32
#define RENDER_MAX_CRACKS 8
#define RENDER_MAX_INTRO_QUEUE 8
#define RENDER_RYU_TRAIL_LEN 24
#define SETTINGS_ROW_COUNT 9
#define RENDER_INK_LUT_LEVELS 256
#define RENDER_GRAD_LUT_LEVELS 24
#define RENDER_ELEM_LUT_LEVELS 64

typedef struct {
    bool use_post;
    uint32_t fault_mask;
    const char *font_path;
    const char *title_font_path;
} RenderOptions;

typedef struct {
    Rectangle cards[GAME_MAX_REWARD_CHOICES];
    int32_t card_count;
    Rectangle repair_rect;
    bool has_repair;
    Rectangle reroll_rect;
    bool has_reroll;
    Rectangle crystal_rect;
    bool has_crystal;
} RewardLayout;

typedef struct {
    Rectangle items[4]; /* 0: Sortie, 1: Bokuan, 2: Settings, 3: Quit */
    Rectangle title_rect;
    Rectangle footer_rect;
    Rectangle brush_rect;
    Rectangle hint_rect;
} LobbyLayout;

typedef struct {
    Rectangle cards[BRUSH_STYLE_COUNT];
    Rectangle back_rect;
} BrushSelectLayout;

typedef struct {
    Rectangle upgrades[META_UPG_COUNT];
    Rectangle relics[META_RELIC_UNLOCK_COUNT];
    Rectangle back_rect;
} BokuanLayout;

typedef struct {
    Rectangle items[4]; /* 0: Resume, 1: Restart, 2: Settings, 3: Lobby */
    Rectangle confirm_yes;
    Rectangle confirm_no;
} PauseLayout;

typedef struct {
    Rectangle rows[SETTINGS_ROW_COUNT];
    Rectangle minus_rects[SETTINGS_ROW_COUNT];
    Rectangle plus_rects[SETTINGS_ROW_COUNT];
    Rectangle reset_hints_rect;
    Rectangle reset_defaults_rect;
    Rectangle back_rect;
} SettingsLayout;

typedef struct {
    Rectangle endless_rect;
    bool has_endless;
    Rectangle lobby_rect;
    Rectangle retry_rect;
} ResultLayout;

typedef struct {
    bool active;
    float x, y;
    float vx, vy;
    float size;
    float age, life;
    int32_t color_id;
    uint32_t seed;
    bool is_wash;
} VfxSpark;

typedef struct {
    bool active;
    float x1, y1, x2, y2;
    float age, life;
    uint32_t seed;
} VfxBolt;

typedef enum {
    SEAL_PRIO_STAMP = 1, /* UI confirmations (reward pick) */
    SEAL_PRIO_REACTION,  /* elemental reactions (Cataclysm) */
    SEAL_PRIO_MILESTONE  /* wave / act clear */
} SealPriority;

typedef struct {
    bool active;
    TextId glyph;
    float x, y;
    float size;
    int32_t style;
    int32_t priority;
    float age, life;
} VfxSeal;

typedef struct {
    bool active;
    TextId text_id;
    int32_t count_arg;
    float x, y;
    float age, life;
    int32_t color_id;
    bool large;
} VfxCallout;

typedef struct {
    bool active;
    int32_t kind;  /* 0=cyclone, 1=gale, 2=act wipe, 3=orb trail, 4=boss burst,
                      5=thunderburst enso, 6=cataclysm gold enso, 7=bounce enso,
                      8=nue dash trail, 9=ryu orb break, 10=purification shockwave,
                      11=brush pressure ripple, 12=barrier ward ring */
    int32_t style; /* BrushStyleId for cyclone/gale/ripple, -1 for neutral */
    float x, y, dx, dy;
    float radius;
    float age, life;
    uint32_t seed;
} VfxStroke;

typedef struct {
    bool active;
    float x, y, dx, dy;
    float age, life;
    uint32_t seed;
} VfxCrack;

typedef struct {
    double draw_ms;
    uint32_t prim_verts;
    uint32_t blend_switches;
} RenderMetrics;

typedef struct {
    Shader post_shader;
    int post_res_loc;
    bool post_enabled;
    RenderTexture2D scene;
    /* 不透明な和紙＋山水背景（画面揺れ用に四辺 16px の余白付き） */
    RenderTexture2D bg_cache;
    int32_t bg_cached_kind; /* BgKind。未生成は -1 */
    int32_t bg_cached_act;
    Texture2D paper_tex; /* 継ぎ目のない 512x512 和紙（REPEAT） */
    Shader fluid_shader;
    int fluid_paper_loc;
    int fluid_max_dye_loc;
    /* 流体テクスチャは 2 枚ずつ交互に更新し、前フレームが描画中のテクスチャへ転送して
     * CPU が GPU の完了を待つ暗黙の同期を避ける */
    Texture2D fluid_field_tex[2]; /* GPU 経路: (ink, fire, storm, solid) RGBA8 */
    Texture2D fluid_ink_tex[2];   /* CPU 経路: 墨の乗算色 */
    Texture2D fluid_elem_tex[2];  /* CPU 経路: 火・雷の乗算済みアルファ */
    int32_t fluid_tex_slot;
    unsigned char fluid_field_px[FLUID_N * 4];
    Color fluid_ink_px[FLUID_N];
    Color fluid_elem_px[FLUID_N];
    /* CPU 経路の色テーブル（濃度は sqrt 量子化）: 墨 [濃度][勾配]、火・雷 [火][雷] */
    Color fluid_ink_lut[RENDER_INK_LUT_LEVELS * RENDER_GRAD_LUT_LEVELS];
    Color fluid_elem_lut[RENDER_ELEM_LUT_LEVELS * RENDER_ELEM_LUT_LEVELS];

    Font font_body;
    Font font_title;
    bool custom_fonts;
    bool owns_body_font;
    bool owns_title_font;
    bool title_is_brush;
    bool use_ja;

    uint64_t frames_drawn;
    uint64_t vfx_rng;
    uint64_t events_seen;
    float vfx_time;

    /* Screen shake & flash rate limiter */
    float shake_t;
    float shake_amp;
    float flash_t;
    float flash_times[4];
    int32_t flash_count;
    VfxThunderburstThrottle thunderburst_fx;

    /* Banners, intro queue, major effects */
    float boss_banner_t;
    EnemyDefId boss_banner_id;
    float clear_banner_t;
    TextId clear_banner_text;
    float gameover_drip_t;
    bool gameover_drip_active;
    float victory_scroll_t;
    float reward_anim_t;
    float reward_reroll_t;
    uint32_t last_reward_serial;
    float skill_deny_t[SKILL_COUNT];
    float shrine_flicker_t;
    int32_t major_fx_active;

    /* Act 3 wind telegraph & boss visual history */
    float wind_telegraph_t;
    int32_t wind_telegraph_dir;
    Vector2 ryu_trail[RENDER_RYU_TRAIL_LEN];
    int32_t ryu_trail_n;
    EntityId ryu_trail_id;
    float ryu_awaken_t;

    /* Bounce orb & Nue dash tracking */
    int32_t prev_proj_bounces[ECS_MAX_ENTITIES];
    uint32_t prev_proj_gen[ECS_MAX_ENTITIES];
    Vector2 prev_nue_pos;
    EntityId prev_nue_id;

    EnemyDefId intro_queue[RENDER_MAX_INTRO_QUEUE];
    int32_t intro_head;
    int32_t intro_count;
    float intro_t;

    VfxSpark sparks[RENDER_MAX_SPARKS];
    int32_t spark_cursor;
    VfxBolt bolts[RENDER_MAX_BOLTS];
    int32_t bolt_cursor;
    VfxSeal seals[RENDER_MAX_SEALS];
    int32_t seal_cursor;
    VfxCallout callouts[RENDER_MAX_CALLOUTS];
    int32_t callout_cursor;
    VfxStroke strokes[RENDER_MAX_STROKES];
    int32_t stroke_cursor;
    VfxCrack cracks[RENDER_MAX_CRACKS];
    int32_t crack_cursor;

    int32_t cur_blend;
    RenderMetrics metrics;
    RenderMetrics last_metrics;
} RenderState;

void render_init(RenderState *r, const RenderOptions *opt);
void render_reset_vfx(RenderState *r, uint64_t seed);
void render_set_post_enabled(RenderState *r, bool enabled);
void render_set_lang_mode(RenderState *r, UiLangMode mode);
void render_consume_events(RenderState *r, const GameState *g);
void render_update_vfx(RenderState *r, const GameState *g, float dt);
void render_draw(RenderState *r, const GameState *g, const UiView *ui);
void render_draw_to_texture(RenderState *r, const GameState *g, const UiView *ui,
                            RenderTexture2D target);
bool render_texture_usable(RenderTexture2D target); /* ID・添付に加えて FBO の完全性も検査 */
void render_close(RenderState *r);

void render_reward_layout(const GameState *g, RewardLayout *out);
void render_lobby_layout(LobbyLayout *out);
void render_brush_select_layout(BrushSelectLayout *out);
void render_bokuan_layout(BokuanLayout *out);
void render_pause_layout(PauseLayout *out);
void render_settings_layout(SettingsLayout *out);
void render_result_layout(bool show_endless, ResultLayout *out);

#endif
