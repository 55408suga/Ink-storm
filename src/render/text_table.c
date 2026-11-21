#include "text_table.h"

typedef struct {
    const char *ja;
    const char *en;
} TextEntry;

static const TextEntry k_text_table[TEXT_COUNT] = {
    [TEXT_NONE] = {.ja = "", .en = ""},
    [TEXT_GAME_TITLE] = {.ja = "墨嵐", .en = "INKSTORM"},
    [TEXT_ACT1_TITLE] = {.ja = "第一幕　開けた和紙の社", .en = "Act 1: Open Washi Shrine"},

    [TEXT_HUD_ACT] = {.ja = "第一幕", .en = "Act 1"},
    [TEXT_HUD_WAVE] = {.ja = "波", .en = "Wave"},
    [TEXT_HUD_BOSS] = {.ja = "主", .en = "Boss"},
    [TEXT_HUD_QUOTA] = {.ja = "祓", .en = "Kills"},
    [TEXT_HUD_SCORE] = {.ja = "点", .en = "Score"},
    [TEXT_HUD_INK] = {.ja = "墨", .en = "Ink"},
    [TEXT_HUD_CHAIN] = {.ja = "一筆連殺", .en = "Chain"},
    [TEXT_HUD_SHRINE] = {.ja = "社", .en = "Shrine"},

    [TEXT_ENEMY_ARCHER_NAME] = {.ja = "弓妖", .en = "Bow Yokai"},
    [TEXT_ENEMY_ARCHER_INTRO] = {.ja = "矢を風筆で逸らし撃ち返せ",
                                 .en = "Deflect arrows with brush wind"},
    [TEXT_ENEMY_LANTERN_FIRE_NAME] = {.ja = "提灯妖・炎", .en = "Fire Lantern"},
    [TEXT_ENEMY_LANTERN_FIRE_INTRO] = {.ja = "火球を墨の渦へ導き着火せよ",
                                       .en = "Pull fireballs into ink vortex"},
    [TEXT_ENEMY_UMBRELLA_NAME] = {.ja = "唐傘妖", .en = "Umbrella Yokai"},
    [TEXT_ENEMY_UMBRELLA_INTRO] = {.ja = "風斬りで壁や敵へ叩きつけよ",
                                   .en = "Slam into walls with Gale Burst"},
    [TEXT_ENEMY_BOSS_TENGU_NAME] = {.ja = "大天狗", .en = "O-Tengu"},
    [TEXT_ENEMY_BOSS_TENGU_INTRO] = {.ja = "紅風を押し返し火球を奪え",
                                     .en = "Push back crimson wind"},
    [TEXT_BOSS_TENGU_EPITHET] = {.ja = "紅風の大天狗", .en = "O-Tengu of the Crimson Wind"},

    [TEXT_CALLOUT_BLAZING] = {.ja = "紅蓮墨嵐！", .en = "Blazing Inkstorm!"},
    [TEXT_CALLOUT_TEMPEST] = {.ja = "雷墨連鎖", .en = "Tempest Lightning"},
    [TEXT_CALLOUT_SLAM] = {.ja = "叩きつけ！", .en = "Slam!"},
    [TEXT_CALLOUT_CHAIN] = {.ja = "一筆連殺 ×", .en = "Chain x"},
    [TEXT_CALLOUT_SURGE] = {.ja = "墨昇華", .en = "Ink Surge"},
    [TEXT_CALLOUT_WAVE_CLEAR] = {.ja = "波を祓った", .en = "Wave Cleared"},
    [TEXT_CALLOUT_ACT_CLEAR] = {.ja = "幕を討った", .en = "Act Cleared"},

    [TEXT_REWARD_TITLE] = {.ja = "墨の恩恵を選べ", .en = "Choose a Blessing"},
    [TEXT_REWARD_FALLBACK_TITLE] = {.ja = "社を整えよ", .en = "Tend the Shrine"},
    [TEXT_REWARD_REROLL] = {.ja = "引き直し [R]", .en = "Reroll [R]"},
    [TEXT_REWARD_REPAIR] = {.ja = "社を修繕 +1 [4]", .en = "Repair Shrine +1 [4]"},
    [TEXT_REWARD_LV] = {.ja = "Lv", .en = "Lv"},
    [TEXT_REWARD_HINT] = {.ja = "1〜3 またはクリックで選択", .en = "Press 1-3 or click to pick"},

    [TEXT_AXIS_BLAZE] = {.ja = "紅蓮", .en = "Blaze"},
    [TEXT_AXIS_THUNDER] = {.ja = "雷霆", .en = "Thunder"},
    [TEXT_AXIS_GALE] = {.ja = "烈風", .en = "Gale"},
    [TEXT_AXIS_INKWAY] = {.ja = "墨道", .en = "Inkway"},
    [TEXT_AXIS_COMMON] = {.ja = "共通", .en = "Common"},

    [TEXT_UPG_EMBER_TIP_NAME] = {.ja = "火種の筆", .en = "Ember Tip"},
    [TEXT_UPG_EMBER_TIP_LV1] = {.ja = "筆跡160pxごとに火種", .en = "Ember every 160px of stroke"},
    [TEXT_UPG_EMBER_TIP_LV2] = {.ja = "筆跡120pxごとに火種", .en = "Ember every 120px of stroke"},
    [TEXT_UPG_EMBER_TIP_LV3] = {.ja = "80pxごとに火種・量1.5倍",
                                .en = "Ember every 80px, 1.5x fire"},
    [TEXT_UPG_WILDFIRE_NAME] = {.ja = "延焼", .en = "Wildfire"},
    [TEXT_UPG_WILDFIRE_LV1] = {.ja = "燃焼量 ×1.25", .en = "Burn rate x1.25"},
    [TEXT_UPG_WILDFIRE_LV2] = {.ja = "燃焼量 ×1.5", .en = "Burn rate x1.50"},
    [TEXT_UPG_WILDFIRE_LV3] = {.ja = "燃焼量 ×1.8", .en = "Burn rate x1.80"},
    [TEXT_UPG_CHARGED_SEAL_NAME] = {.ja = "帯電印", .en = "Charged Seal"},
    [TEXT_UPG_CHARGED_SEAL_LV1] = {.ja = "渦印に雷 0.3", .en = "Cyclone adds 0.30 storm"},
    [TEXT_UPG_CHARGED_SEAL_LV2] = {.ja = "渦印に雷 0.45", .en = "Cyclone adds 0.45 storm"},
    [TEXT_UPG_CHARGED_SEAL_LV3] = {.ja = "渦印に雷 0.6・持続中も注入",
                                   .en = "Cyclone adds 0.60 storm over time"},
    [TEXT_UPG_CHAIN_LIGHTNING_NAME] = {.ja = "連鎖雷", .en = "Chain Lightning"},
    [TEXT_UPG_CHAIN_LIGHTNING_LV1] = {.ja = "連鎖のホップ +1", .en = "Lightning hops +1"},
    [TEXT_UPG_CHAIN_LIGHTNING_LV2] = {.ja = "連鎖のホップ +2", .en = "Lightning hops +2"},
    [TEXT_UPG_CHAIN_LIGHTNING_LV3] = {.ja = "ホップ +3・距離 +30%", .en = "Hops +3, range +30%"},
    [TEXT_UPG_GREAT_VORTEX_NAME] = {.ja = "大渦", .en = "Great Vortex"},
    [TEXT_UPG_GREAT_VORTEX_LV1] = {.ja = "渦半径 +15%・持続 +0.2秒",
                                   .en = "Vortex rad +15%, dur +0.2s"},
    [TEXT_UPG_GREAT_VORTEX_LV2] = {.ja = "渦半径 +30%・持続 +0.4秒",
                                   .en = "Vortex rad +30%, dur +0.4s"},
    [TEXT_UPG_GREAT_VORTEX_LV3] = {.ja = "渦半径 +45%・持続 +0.6秒",
                                   .en = "Vortex rad +45%, dur +0.6s"},
    [TEXT_UPG_SLAM_SHOCKWAVE_NAME] = {.ja = "叩きつけ衝撃", .en = "Slam Shockwave"},
    [TEXT_UPG_SLAM_SHOCKWAVE_LV1] = {.ja = "叩きつけ衝撃波 半径64・20",
                                     .en = "Slam wave rad 64, 20 dmg"},
    [TEXT_UPG_SLAM_SHOCKWAVE_LV2] = {.ja = "衝撃波 半径80・30", .en = "Slam wave rad 80, 30 dmg"},
    [TEXT_UPG_SLAM_SHOCKWAVE_LV3] = {.ja = "衝撃波 半径96・40・吹飛",
                                     .en = "Slam wave rad 96, 40 dmg + knock"},
    [TEXT_UPG_RICH_INK_NAME] = {.ja = "墨玉増量", .en = "Rich Ink"},
    [TEXT_UPG_RICH_INK_LV1] = {.ja = "33%で墨玉 +1", .en = "33% chance for +1 Ink Orb"},
    [TEXT_UPG_RICH_INK_LV2] = {.ja = "66%で墨玉 +1", .en = "66% chance for +1 Ink Orb"},
    [TEXT_UPG_RICH_INK_LV3] = {.ja = "100%で墨玉 +1", .en = "Always +1 Ink Orb"},
    [TEXT_UPG_LASTING_SURGE_NAME] = {.ja = "長き昇華", .en = "Lasting Surge"},
    [TEXT_UPG_LASTING_SURGE_LV1] = {.ja = "墨昇華 4.6秒", .en = "Ink Surge lasts 4.6s"},
    [TEXT_UPG_LASTING_SURGE_LV2] = {.ja = "墨昇華 5.6秒", .en = "Ink Surge lasts 5.6s"},
    [TEXT_UPG_LASTING_SURGE_LV3] = {.ja = "墨昇華 6.6秒", .en = "Ink Surge lasts 6.6s"},

    [TEXT_RESULT_VICTORY] = {.ja = "勝利　社を護り抜いた", .en = "VICTORY - Shrine Preserved"},
    [TEXT_RESULT_DEFEAT] = {.ja = "落墨　社の灯は消えた", .en = "DEFEAT - The Shrine Fell"},
    [TEXT_RESULT_REACHED] = {.ja = "到達", .en = "Reached"},
    [TEXT_RESULT_SCORE] = {.ja = "スコア", .en = "Score"},
    [TEXT_RESULT_KILLS] = {.ja = "撃破数", .en = "Kills"},
    [TEXT_RESULT_MAX_CHAIN] = {.ja = "最大連殺", .en = "Max Chain"},
    [TEXT_RESULT_REACTIONS] = {.ja = "属性反応", .en = "Reactions"},
    [TEXT_RESULT_BLAZING_LABEL] = {.ja = "紅蓮墨嵐", .en = "Blazing"},
    [TEXT_RESULT_TEMPEST_LABEL] = {.ja = "雷墨連鎖", .en = "Tempest"},
    [TEXT_RESULT_THUNDERBURST_LABEL] = {.ja = "爆雷", .en = "Thunderburst"},
    [TEXT_RESULT_CATACLYSM_LABEL] = {.ja = "三元共鳴", .en = "Cataclysm"},
    [TEXT_RESULT_UPGRADES] = {.ja = "取得した札", .en = "Upgrades"},
    [TEXT_RESULT_SEED] = {.ja = "シード", .en = "Seed"},
    [TEXT_RESULT_CRYSTALS] = {.ja = "墨晶精算", .en = "Ink Crystals"},
    [TEXT_RESULT_CRYSTAL_ACTS] = {.ja = "幕踏破", .en = "Acts"},
    [TEXT_RESULT_CRYSTAL_KILLS] = {.ja = "撃破", .en = "Kills"},
    [TEXT_RESULT_CRYSTAL_CHAIN] = {.ja = "最大連殺", .en = "Max Chain"},
    [TEXT_RESULT_CRYSTAL_REACTIONS] = {.ja = "反応", .en = "Reactions"},
    [TEXT_RESULT_CRYSTAL_TOTAL] = {.ja = "獲得計", .en = "Earned"},
    [TEXT_RESULT_CRYSTAL_OWNED] = {.ja = "所持計", .en = "Total Owned"},
    [TEXT_RESULT_RETRY] = {.ja = "クリックで再出陣", .en = "Click or Enter to Retry"},

    [TEXT_SEAL_PURIFY] = {.ja = "祓", .en = "P"},
    [TEXT_SEAL_SLAY] = {.ja = "討", .en = "S"},
    [TEXT_SEAL_DONE] = {.ja = "決", .en = "OK"},
    [TEXT_SHOWCASE_HEADER] = {.ja = "見本帳", .en = "Showcase"},
};

const char *text_get(TextId id, bool use_ja)
{
    if ((unsigned)id >= (unsigned)TEXT_COUNT) {
        return "";
    }
    return use_ja ? k_text_table[id].ja : k_text_table[id].en;
}

TextId text_enemy_name(EnemyDefId id)
{
    switch (id) {
    case ENEMY_ARCHER:
        return TEXT_ENEMY_ARCHER_NAME;
    case ENEMY_LANTERN_FIRE:
        return TEXT_ENEMY_LANTERN_FIRE_NAME;
    case ENEMY_UMBRELLA:
        return TEXT_ENEMY_UMBRELLA_NAME;
    case ENEMY_BOSS_TENGU:
        return TEXT_ENEMY_BOSS_TENGU_NAME;
    default:
        return TEXT_NONE;
    }
}

TextId text_enemy_intro(EnemyDefId id)
{
    switch (id) {
    case ENEMY_ARCHER:
        return TEXT_ENEMY_ARCHER_INTRO;
    case ENEMY_LANTERN_FIRE:
        return TEXT_ENEMY_LANTERN_FIRE_INTRO;
    case ENEMY_UMBRELLA:
        return TEXT_ENEMY_UMBRELLA_INTRO;
    case ENEMY_BOSS_TENGU:
        return TEXT_ENEMY_BOSS_TENGU_INTRO;
    default:
        return TEXT_NONE;
    }
}

TextId text_boss_epithet(EnemyDefId id)
{
    if (id == ENEMY_BOSS_TENGU) {
        return TEXT_BOSS_TENGU_EPITHET;
    }
    return TEXT_NONE;
}

TextId text_upgrade_name(UpgradeId id)
{
    switch (id) {
    case UPG_EMBER_TIP:
        return TEXT_UPG_EMBER_TIP_NAME;
    case UPG_WILDFIRE:
        return TEXT_UPG_WILDFIRE_NAME;
    case UPG_CHARGED_SEAL:
        return TEXT_UPG_CHARGED_SEAL_NAME;
    case UPG_CHAIN_LIGHTNING:
        return TEXT_UPG_CHAIN_LIGHTNING_NAME;
    case UPG_GREAT_VORTEX:
        return TEXT_UPG_GREAT_VORTEX_NAME;
    case UPG_SLAM_SHOCKWAVE:
        return TEXT_UPG_SLAM_SHOCKWAVE_NAME;
    case UPG_RICH_INK:
        return TEXT_UPG_RICH_INK_NAME;
    case UPG_LASTING_SURGE:
        return TEXT_UPG_LASTING_SURGE_NAME;
    default:
        return TEXT_NONE;
    }
}

TextId text_upgrade_effect(UpgradeId id, int level)
{
    if (level < 1 || level > 3) {
        return TEXT_NONE;
    }
    const int offset = level - 1;
    switch (id) {
    case UPG_EMBER_TIP:
        return (TextId)(TEXT_UPG_EMBER_TIP_LV1 + offset);
    case UPG_WILDFIRE:
        return (TextId)(TEXT_UPG_WILDFIRE_LV1 + offset);
    case UPG_CHARGED_SEAL:
        return (TextId)(TEXT_UPG_CHARGED_SEAL_LV1 + offset);
    case UPG_CHAIN_LIGHTNING:
        return (TextId)(TEXT_UPG_CHAIN_LIGHTNING_LV1 + offset);
    case UPG_GREAT_VORTEX:
        return (TextId)(TEXT_UPG_GREAT_VORTEX_LV1 + offset);
    case UPG_SLAM_SHOCKWAVE:
        return (TextId)(TEXT_UPG_SLAM_SHOCKWAVE_LV1 + offset);
    case UPG_RICH_INK:
        return (TextId)(TEXT_UPG_RICH_INK_LV1 + offset);
    case UPG_LASTING_SURGE:
        return (TextId)(TEXT_UPG_LASTING_SURGE_LV1 + offset);
    default:
        return TEXT_NONE;
    }
}

TextId text_axis_name(UpgradeAxis axis)
{
    switch (axis) {
    case AXIS_BLAZE:
        return TEXT_AXIS_BLAZE;
    case AXIS_THUNDER:
        return TEXT_AXIS_THUNDER;
    case AXIS_GALE:
        return TEXT_AXIS_GALE;
    case AXIS_INKWAY:
        return TEXT_AXIS_INKWAY;
    case AXIS_COMMON:
        return TEXT_AXIS_COMMON;
    default:
        return TEXT_NONE;
    }
}

TextId text_act_title(int act_index)
{
    if (act_index == 0) {
        return TEXT_ACT1_TITLE;
    }
    return TEXT_NONE;
}
