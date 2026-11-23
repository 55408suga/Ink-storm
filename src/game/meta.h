#ifndef INK_META_H
#define INK_META_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define META_SAVE_VERSION 1
#define META_MAX_CRYSTALS 9999999
#define META_MAX_SCORE 999999999
#define META_MAX_COUNTERS 9999999
#define ENDLESS_MAX_LOOP_N 100
#define META_MAX_ENDLESS_ACTS (ENDLESS_MAX_LOOP_N * 3)
#define META_MAX_BEST_ACT (3 + META_MAX_ENDLESS_ACTS)
#define META_RELIC_UNLOCK_COUNT 6
#define META_HINT_BITS 32
#define META_SERIALIZE_CAP 640

typedef enum {
    META_UPG_SHRINE_HP = 0,
    META_UPG_INK_MAX,
    META_UPG_REROLLS,
    META_UPG_INK_REGEN,
    META_UPG_COUNT
} MetaUpgradeKind;

typedef enum {
    UI_LANG_AUTO = 0,
    UI_LANG_JA,
    UI_LANG_EN,
    UI_LANG_COUNT
} UiLangMode;

typedef struct {
    uint8_t master_vol;
    uint8_t bgm_vol;
    uint8_t se_vol;
    uint8_t shake_scale;
    bool post_shader_on;
    bool hints_on;
    uint8_t ui_lang;      /* UiLangMode (0=AUTO, 1=JA, 2=EN), default UI_LANG_AUTO (0) */
    bool reduce_flash;    /* Suppress screen-wide flashing, default false */
    uint8_t cursor_scale; /* 100 / 125 / 150, default 100 */
} SettingsState;

typedef struct {
    uint32_t version;
    int32_t ink_crystals;
    uint8_t upgrade_lv[META_UPG_COUNT];
    uint64_t unlocked_relic_mask;
    int32_t best_score;
    int32_t best_act;
    int32_t runs_played;
    int32_t clears;
    uint32_t seen_hints_mask;
    SettingsState settings;
} MetaState;

typedef enum {
    BRUSH_STYLE_HEAVY = 0,
    BRUSH_STYLE_EMBER,
    BRUSH_STYLE_QUILL,
    BRUSH_STYLE_COUNT
} BrushStyleId;

typedef struct {
    uint64_t seed;
    BrushStyleId brush_style;
    uint8_t meta_upgrade_lv[META_UPG_COUNT];
    uint64_t unlocked_relic_mask;
    int32_t endless_loop; /* 0 for the main run, 1 for the first endless loop. */
} RunConfig;

typedef struct {
    int32_t acts_cleared; /* Main campaign only. */
    int32_t endless_acts_cleared;
    int32_t reached_act; /* Includes endless acts, starting at act 4. */
    int32_t kills;
    int32_t max_chain;
    int32_t thunderbursts;
    int32_t cataclysms;
    int32_t score;
    bool cleared;
    bool crystal_bonus_x15;
    int32_t bonus_crystals;
} MetaRunResult;

typedef struct {
    int32_t from_acts, from_kills, from_chain, from_reactions, from_endless;
    int32_t from_bonus;
    int32_t total_crystals;
} MetaSettleBreakdown;

void meta_init_default(MetaState *meta);
SettingsState meta_settings_default(void);

uint8_t meta_upgrade_max_level(MetaUpgradeKind kind);
int32_t meta_upgrade_next_cost(const MetaState *meta, MetaUpgradeKind kind);
bool meta_buy_upgrade(MetaState *meta, MetaUpgradeKind kind);
int32_t meta_relic_unlock_cost(uint32_t slot);
bool meta_is_relic_unlocked(const MetaState *meta, uint32_t slot);
bool meta_buy_relic_unlock(MetaState *meta, uint32_t slot);

MetaSettleBreakdown meta_calc_crystals(const MetaRunResult *res);
/* Each run owns a flag initialized to false. Only its first settlement pays out. */
bool meta_settle_result(MetaState *meta, const MetaRunResult *res, bool *run_settled_flag,
                        MetaSettleBreakdown *out_breakdown);

RunConfig meta_build_run_config(const MetaState *meta, BrushStyleId style, uint64_t seed);
RunConfig meta_run_config_preset(BrushStyleId style, uint64_t seed);
bool meta_is_hint_seen(const MetaState *meta, uint32_t hint_bit);
void meta_mark_hint_seen(MetaState *meta, uint32_t hint_bit);

/* Returns the required byte count excluding NUL, including when buf is NULL. */
size_t meta_serialize(const MetaState *meta, char *buf, size_t buf_size);
/* Invalid input resets the entire destination to the defaults. */
bool meta_deserialize(MetaState *meta, const char *text);

#endif
