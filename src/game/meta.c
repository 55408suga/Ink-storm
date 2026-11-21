#include "meta.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct {
    uint8_t levels;
    int32_t prices[3];
} upgrades[META_UPG_COUNT] = {
    [META_UPG_SHRINE_HP] = {3, {80, 140, 200}},
    [META_UPG_INK_MAX] = {3, {60, 100, 140}},
    [META_UPG_REROLLS] = {2, {100, 200}},
    [META_UPG_INK_REGEN] = {3, {60, 100, 140}},
};

#define UNLOCKABLE_RELICS_MASK ((UINT64_C(1) << META_RELIC_UNLOCK_COUNT) - 1)

static int32_t clamp_count(int64_t value, int32_t maximum)
{
    if (value < 0) {
        return 0;
    }
    return value > maximum ? maximum : (int32_t)value;
}

SettingsState meta_settings_default(void)
{
    return (SettingsState){.master_vol = 80,
                           .bgm_vol = 70,
                           .se_vol = 85,
                           .shake_scale = 100,
                           .post_shader_on = true,
                           .hints_on = true};
}

void meta_init_default(MetaState *meta)
{
    if (meta != NULL) {
        *meta = (MetaState){.version = META_SAVE_VERSION, .settings = meta_settings_default()};
    }
}

uint8_t meta_upgrade_max_level(MetaUpgradeKind kind)
{
    if ((unsigned)kind >= META_UPG_COUNT) {
        return 0;
    }
    return upgrades[kind].levels;
}

int32_t meta_upgrade_next_cost(const MetaState *meta, MetaUpgradeKind kind)
{
    if (meta == NULL || (unsigned)kind >= META_UPG_COUNT ||
        meta->upgrade_lv[kind] >= upgrades[kind].levels) {
        return -1;
    }
    return upgrades[kind].prices[meta->upgrade_lv[kind]];
}

bool meta_buy_upgrade(MetaState *meta, MetaUpgradeKind kind)
{
    const int32_t cost = meta_upgrade_next_cost(meta, kind);
    if (cost < 0 || meta->ink_crystals < cost) {
        return false;
    }
    meta->ink_crystals -= cost;
    ++meta->upgrade_lv[kind];
    return true;
}

int32_t meta_relic_unlock_cost(uint32_t slot) { return slot < META_RELIC_UNLOCK_COUNT ? 100 : -1; }

bool meta_is_relic_unlocked(const MetaState *meta, uint32_t slot)
{
    return meta != NULL && slot < META_RELIC_UNLOCK_COUNT &&
           (meta->unlocked_relic_mask & (UINT64_C(1) << slot)) != 0;
}

bool meta_buy_relic_unlock(MetaState *meta, uint32_t slot)
{
    const int32_t cost = meta_relic_unlock_cost(slot);
    if (meta == NULL || cost < 0 || meta->ink_crystals < cost ||
        meta_is_relic_unlocked(meta, slot)) {
        return false;
    }
    meta->ink_crystals -= cost;
    meta->unlocked_relic_mask |= UINT64_C(1) << slot;
    return true;
}

MetaSettleBreakdown meta_calc_crystals(const MetaRunResult *res)
{
    if (res == NULL) {
        return (MetaSettleBreakdown){0};
    }
    static const int32_t act_rewards[] = {0, 30, 70, 120};
    const int64_t thunderbursts = clamp_count(res->thunderbursts, INT32_MAX);
    const int64_t cataclysms = clamp_count(res->cataclysms, INT32_MAX);
    MetaSettleBreakdown reward = {
        .from_acts = act_rewards[clamp_count(res->acts_cleared, 3)],
        .from_kills = clamp_count((int64_t)res->kills / 5, INT32_MAX),
        .from_chain = clamp_count((int64_t)res->max_chain * 2, 30),
        .from_reactions = clamp_count(thunderbursts + cataclysms * 3, 20),
        .from_endless = (int64_t)clamp_count(res->endless_acts_cleared, META_MAX_ENDLESS_ACTS) * 40,
    };
    const int64_t base = (int64_t)reward.from_acts + reward.from_kills + reward.from_chain +
                         reward.from_reactions + reward.from_endless;
    reward.from_bonus = res->crystal_bonus_x15 ? (int32_t)(base / 2) : 0;
    reward.total_crystals = clamp_count(base + reward.from_bonus, META_MAX_CRYSTALS);
    return reward;
}

bool meta_settle_result(MetaState *meta, const MetaRunResult *res, bool *run_settled_flag,
                        MetaSettleBreakdown *out_breakdown)
{
    if (meta == NULL || res == NULL || run_settled_flag == NULL || *run_settled_flag) {
        if (out_breakdown != NULL) {
            *out_breakdown = (MetaSettleBreakdown){0};
        }
        return false;
    }
    const MetaSettleBreakdown reward = meta_calc_crystals(res);
    meta->ink_crystals =
        clamp_count((int64_t)meta->ink_crystals + reward.total_crystals, META_MAX_CRYSTALS);
    const int32_t score = clamp_count(res->score, META_MAX_SCORE);
    const int32_t act = clamp_count(res->reached_act, META_MAX_BEST_ACT);
    if (score > meta->best_score) {
        meta->best_score = score;
    }
    if (act > meta->best_act) {
        meta->best_act = act;
    }
    meta->runs_played = clamp_count((int64_t)meta->runs_played + 1, META_MAX_COUNTERS);
    if (res->cleared) {
        meta->clears = clamp_count((int64_t)meta->clears + 1, META_MAX_COUNTERS);
    }
    *run_settled_flag = true;
    if (out_breakdown != NULL) {
        *out_breakdown = reward;
    }
    return true;
}

RunConfig meta_run_config_preset(BrushStyleId style, uint64_t seed)
{
    return (RunConfig){.seed = seed,
                       .brush_style =
                           (unsigned)style < BRUSH_STYLE_COUNT ? style : BRUSH_STYLE_HEAVY};
}

RunConfig meta_build_run_config(const MetaState *meta, BrushStyleId style, uint64_t seed)
{
    RunConfig config = meta_run_config_preset(style, seed);
    if (meta != NULL) {
        memcpy(config.meta_upgrade_lv, meta->upgrade_lv, sizeof config.meta_upgrade_lv);
        config.unlocked_relic_mask = meta->unlocked_relic_mask & UNLOCKABLE_RELICS_MASK;
    }
    return config;
}

bool meta_is_hint_seen(const MetaState *meta, uint32_t hint_bit)
{
    return meta != NULL && hint_bit < META_HINT_BITS &&
           (meta->seen_hints_mask & (UINT32_C(1) << hint_bit)) != 0;
}

void meta_mark_hint_seen(MetaState *meta, uint32_t hint_bit)
{
    if (meta != NULL && hint_bit < META_HINT_BITS) {
        meta->seen_hints_mask |= UINT32_C(1) << hint_bit;
    }
}

size_t meta_serialize(const MetaState *meta, char *buf, size_t buf_size)
{
    if (buf == NULL) {
        buf_size = 0;
    }
    if (meta != NULL) {
        const int length = snprintf(
            buf, buf_size,
            "version=%" PRIu32 "\n"
            "ink_crystals=%" PRId32 "\n"
            "upg_shrine_hp=%u\n"
            "upg_ink_max=%u\n"
            "upg_rerolls=%u\n"
            "upg_ink_regen=%u\n"
            "unlocked_relic_mask=0x%016" PRIx64 "\n"
            "best_score=%" PRId32 "\n"
            "best_act=%" PRId32 "\n"
            "runs_played=%" PRId32 "\n"
            "clears=%" PRId32 "\n"
            "seen_hints_mask=0x%08" PRIx32 "\n"
            "opt_master_vol=%u\n"
            "opt_bgm_vol=%u\n"
            "opt_se_vol=%u\n"
            "opt_shake_scale=%u\n"
            "opt_post_shader=%d\n"
            "opt_hints=%d\n",
            meta->version, meta->ink_crystals, (unsigned)meta->upgrade_lv[META_UPG_SHRINE_HP],
            (unsigned)meta->upgrade_lv[META_UPG_INK_MAX],
            (unsigned)meta->upgrade_lv[META_UPG_REROLLS],
            (unsigned)meta->upgrade_lv[META_UPG_INK_REGEN], meta->unlocked_relic_mask,
            meta->best_score, meta->best_act, meta->runs_played, meta->clears,
            meta->seen_hints_mask, (unsigned)meta->settings.master_vol,
            (unsigned)meta->settings.bgm_vol, (unsigned)meta->settings.se_vol,
            (unsigned)meta->settings.shake_scale, meta->settings.post_shader_on,
            meta->settings.hints_on);
        if (length >= 0) {
            return (size_t)length;
        }
    }
    if (buf_size > 0) {
        buf[0] = '\0';
    }
    return 0;
}

typedef struct {
    const char *begin;
    const char *end;
} TextSpan;

static TextSpan trim_space_tab(const char *begin, const char *end)
{
    while (begin < end && (*begin == ' ' || *begin == '\t')) {
        ++begin;
    }
    while (begin < end && (end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }
    return (TextSpan){begin, end};
}

typedef struct {
    const char *key;
    union {
        uint32_t *u32;
        int32_t *i32;
        uint8_t *byte;
        uint64_t *u64;
        bool *toggle;
    } dest;
    enum { VERSION, COUNT, BYTE, RELIC_MASK, HINT_MASK, SHAKE, TOGGLE } kind;
    int32_t maximum;
} SaveField;

static bool read_save_mask(const SaveField *field, const char *number)
{
    if (number[0] == '-') {
        return false;
    }
    const bool hex = number[0] == '0' && (number[1] == 'x' || number[1] == 'X');
    char *end;
    const uint64_t bits = strtoull(number, &end, hex ? 16 : 10);
    if (*end != '\0') {
        return false;
    }
    if (field->kind == RELIC_MASK) {
        *field->dest.u64 = bits & UNLOCKABLE_RELICS_MASK;
    } else {
        *field->dest.u32 = (uint32_t)bits;
    }
    return true;
}

static bool read_save_field(const SaveField *field, TextSpan value)
{
    char number[32];
    const size_t length = (size_t)(value.end - value.begin);
    if (length == 0 || length >= sizeof number || isspace((unsigned char)*value.begin)) {
        return false;
    }
    memcpy(number, value.begin, length);
    number[length] = '\0';
    if (field->kind == RELIC_MASK || field->kind == HINT_MASK) {
        return read_save_mask(field, number);
    }
    char *end;
    /* strtoll also saturates on ERANGE. Apply the field's narrower bounds below. */
    const int64_t parsed = strtoll(number, &end, 10);
    if (*end != '\0') {
        return false;
    }
    switch (field->kind) {
    case VERSION:
        *field->dest.u32 = parsed == META_SAVE_VERSION ? META_SAVE_VERSION : 0;
        break;
    case COUNT:
        *field->dest.i32 = clamp_count(parsed, field->maximum);
        break;
    case BYTE:
        *field->dest.byte = (uint8_t)clamp_count(parsed, field->maximum);
        break;
    case SHAKE:
        *field->dest.byte = parsed < 25 ? 0 : (parsed < 75 ? 50 : 100);
        break;
    case TOGGLE:
        *field->dest.toggle = parsed != 0;
        break;
    default:
        return false;
    }
    return true;
}

static bool read_save_line(const SaveField *fields, size_t count, const char *begin,
                           const char *end)
{
    if (begin < end && end[-1] == '\r') {
        --end;
    }
    const TextSpan line = trim_space_tab(begin, end);
    if (line.begin == line.end || *line.begin == '#') {
        return true;
    }
    const char *equal = memchr(line.begin, '=', (size_t)(line.end - line.begin));
    if (equal == NULL) {
        return false;
    }
    const TextSpan key = trim_space_tab(line.begin, equal);
    const size_t length = (size_t)(key.end - key.begin);
    if (length == 0) {
        return false;
    }
    for (size_t i = 0; i < count; ++i) {
        if (strlen(fields[i].key) == length && memcmp(fields[i].key, key.begin, length) == 0) {
            return read_save_field(&fields[i], trim_space_tab(equal + 1, line.end));
        }
    }
    return true; /* Unknown keys may contain arbitrary values. */
}

bool meta_deserialize(MetaState *meta, const char *text)
{
    if (meta == NULL) {
        return false;
    }
    MetaState tmp;
    meta_init_default(&tmp);
    tmp.version = 0; /* A compatible version must appear explicitly. */
    const SaveField fields[] = {
        {"version", {.u32 = &tmp.version}, VERSION, 0},
        {"ink_crystals", {.i32 = &tmp.ink_crystals}, COUNT, META_MAX_CRYSTALS},
        {"upg_shrine_hp",
         {.byte = &tmp.upgrade_lv[META_UPG_SHRINE_HP]},
         BYTE,
         upgrades[META_UPG_SHRINE_HP].levels},
        {"upg_ink_max",
         {.byte = &tmp.upgrade_lv[META_UPG_INK_MAX]},
         BYTE,
         upgrades[META_UPG_INK_MAX].levels},
        {"upg_rerolls",
         {.byte = &tmp.upgrade_lv[META_UPG_REROLLS]},
         BYTE,
         upgrades[META_UPG_REROLLS].levels},
        {"upg_ink_regen",
         {.byte = &tmp.upgrade_lv[META_UPG_INK_REGEN]},
         BYTE,
         upgrades[META_UPG_INK_REGEN].levels},
        {"unlocked_relic_mask", {.u64 = &tmp.unlocked_relic_mask}, RELIC_MASK, 0},
        {"best_score", {.i32 = &tmp.best_score}, COUNT, META_MAX_SCORE},
        {"best_act", {.i32 = &tmp.best_act}, COUNT, META_MAX_BEST_ACT},
        {"runs_played", {.i32 = &tmp.runs_played}, COUNT, META_MAX_COUNTERS},
        {"clears", {.i32 = &tmp.clears}, COUNT, META_MAX_COUNTERS},
        {"seen_hints_mask", {.u32 = &tmp.seen_hints_mask}, HINT_MASK, 0},
        {"opt_master_vol", {.byte = &tmp.settings.master_vol}, BYTE, 100},
        {"opt_bgm_vol", {.byte = &tmp.settings.bgm_vol}, BYTE, 100},
        {"opt_se_vol", {.byte = &tmp.settings.se_vol}, BYTE, 100},
        {"opt_shake_scale", {.byte = &tmp.settings.shake_scale}, SHAKE, 0},
        {"opt_post_shader", {.toggle = &tmp.settings.post_shader_on}, TOGGLE, 0},
        {"opt_hints", {.toggle = &tmp.settings.hints_on}, TOGGLE, 0},
    };
    if (text != NULL) {
        while (*text != '\0') {
            const char *end = text + strcspn(text, "\n");
            if (!read_save_line(fields, sizeof fields / sizeof fields[0], text, end)) {
                meta_init_default(meta);
                return false;
            }
            text = *end == '\n' ? end + 1 : end;
        }
        if (tmp.version == META_SAVE_VERSION) {
            *meta = tmp;
            return true;
        }
    }
    meta_init_default(meta);
    return false;
}
