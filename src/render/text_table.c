#include "text_table.h"

typedef struct {
    const char *ja;
    const char *en;
} TextEntry;

static const TextEntry k_text_table[TEXT_COUNT] = {
    [TEXT_NONE] = {.ja = "", .en = ""},
    [TEXT_GAME_TITLE] = {.ja = "墨嵐", .en = "INKSTORM"},
    [TEXT_ACT1_TITLE] = {.ja = "第一幕　開けた和紙の社", .en = "Act 1: Open Washi Shrine"},
    [TEXT_ACT2_TITLE] = {.ja = "第二幕　鳥居と岩柱", .en = "Act 2: Torii and Rock Pillars"},
    [TEXT_ACT3_TITLE] = {.ja = "第三幕　嵐の霊峰", .en = "Act 3: Storm of the Spirit Peak"},
    [TEXT_ACT1_SHORT] = {.ja = "第一幕", .en = "Act 1"},
    [TEXT_ACT2_SHORT] = {.ja = "第二幕", .en = "Act 2"},
    [TEXT_ACT3_SHORT] = {.ja = "第三幕", .en = "Act 3"},
    [TEXT_ACT1_SUBTITLE] = {.ja = "開けた和紙の社", .en = "Open Washi Shrine"},
    [TEXT_ACT2_SUBTITLE] = {.ja = "鳥居と岩柱", .en = "Torii and Rock Pillars"},
    [TEXT_ACT3_SUBTITLE] = {.ja = "嵐の霊峰", .en = "Storm of the Spirit Peak"},
    [TEXT_ENDLESS_ACT_PREFIX] = {.ja = "第", .en = "Act "},
    [TEXT_ENDLESS_ACT_SUFFIX] = {.ja = "幕", .en = "Act"},
    [TEXT_ENDLESS_LOOP_LABEL] = {.ja = "周回", .en = "Loop"},

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
    [TEXT_ENEMY_LANTERN_STORM_NAME] = {.ja = "提灯妖・雷", .en = "Storm Lantern"},
    [TEXT_ENEMY_LANTERN_STORM_INTRO] = {.ja = "雷球を墨へ通し雷墨連鎖を起こせ",
                                        .en = "Guide storm orbs into ink for chain lightning"},
    [TEXT_ENEMY_KAMAITACHI_NAME] = {.ja = "鎌鼬", .en = "Kamaitachi"},
    [TEXT_ENEMY_KAMAITACHI_INTRO] = {.ja = "逆渦で流れを乱す俊足。偏向弾で仕留めよ",
                                     .en = "Disrupts vortices with gusts; strike swiftly"},
    [TEXT_ENEMY_STONE_ONI_NAME] = {.ja = "石鬼", .en = "Stone Oni"},
    [TEXT_ENEMY_STONE_ONI_INTRO] = {.ja = "重く堅い岩鬼。墨印と柱叩きつけで砕け",
                                    .en = "Heavy rock demon; mark with ink or slam into pillars"},
    [TEXT_ENEMY_INK_LEECH_NAME] = {.ja = "墨吸い", .en = "Ink Leech"},
    [TEXT_ENEMY_INK_LEECH_INTRO] = {.ja = "墨を吸い膨らむ。炎上で焼き墨を奪い返せ",
                                    .en = "Absorbs ink to heal; burn it to burst ink back"},
    [TEXT_ENEMY_BOSS_NUE_NAME] = {.ja = "鵺", .en = "Nue"},
    [TEXT_ENEMY_BOSS_NUE_INTRO] = {.ja = "雷帯を墨で繋ぎ、停止の隙に柱へ叩きつけよ",
                                   .en = "Link lightning trails with ink; slam while paused"},
    [TEXT_ENEMY_BOSS_RYU_NAME] = {.ja = "一筆龍", .en = "Ippitsu-Ryu"},
    [TEXT_ENEMY_BOSS_RYU_INTRO] = {.ja = "三つの龍玉を凌ぎ、点睛後は三元共鳴で討て",
                                   .en =
                                       "Endure three orbs; finish with Cataclysm after awakening"},
    [TEXT_ENEMY_OFUDA_FOX_NAME] = {.ja = "狐符", .en = "Ofuda Fox"},
    [TEXT_ENEMY_OFUDA_FOX_INTRO] = {.ja = "味方に結界を張り、追尾符を放つ",
                                    .en = "Wards allies with barriers and fires homing ofuda"},
    [TEXT_ENEMY_BOSS_RYU_ORB_NAME] = {.ja = "龍玉", .en = "Dragon Orb"},
    [TEXT_ENEMY_BOSS_RYU_ORB_INTRO] = {.ja = "一筆龍が宿す属性の核",
                                       .en = "Elemental core borne by Ippitsu-Ryu"},
    [TEXT_BOSS_TENGU_EPITHET] = {.ja = "紅風の大天狗", .en = "O-Tengu of the Crimson Wind"},
    [TEXT_BOSS_NUE_EPITHET] = {.ja = "雷雲の鵺", .en = "Nue of the Thunderclouds"},
    [TEXT_BOSS_RYU_EPITHET] = {.ja = "霊峰の龍脈", .en = "Dragon Vein of the Spirit Peak"},

    [TEXT_CALLOUT_BLAZING] = {.ja = "紅蓮墨嵐！", .en = "Blazing Inkstorm!"},
    [TEXT_CALLOUT_TEMPEST] = {.ja = "雷墨連鎖", .en = "Tempest Lightning"},
    [TEXT_CALLOUT_THUNDERBURST] = {.ja = "爆雷！", .en = "Thunderburst!"},
    [TEXT_CALLOUT_CATACLYSM] = {.ja = "三元共鳴！", .en = "Cataclysm!"},
    [TEXT_CALLOUT_SLAM] = {.ja = "叩きつけ！", .en = "Slam!"},
    [TEXT_CALLOUT_CHAIN] = {.ja = "一筆連殺 ×", .en = "Chain x"},
    [TEXT_CALLOUT_SURGE] = {.ja = "墨昇華", .en = "Ink Surge"},
    [TEXT_CALLOUT_WAVE_CLEAR] = {.ja = "波を祓った", .en = "Wave Cleared"},
    [TEXT_CALLOUT_ACT_CLEAR] = {.ja = "幕を討った", .en = "Act Cleared"},

    [TEXT_REWARD_TITLE] = {.ja = "墨の恩恵を選べ", .en = "Choose a Blessing"},
    [TEXT_REWARD_RELIC_TITLE] = {.ja = "大遺物を選べ", .en = "Choose a Great Relic"},
    [TEXT_REWARD_FALLBACK_TITLE] = {.ja = "社を整えよ", .en = "Tend the Shrine"},
    [TEXT_REWARD_REROLL] = {.ja = "引き直し [R]", .en = "Reroll [R]"},
    [TEXT_REWARD_REPAIR] = {.ja = "社を修繕 +1 [4]", .en = "Repair Shrine +1 [4]"},
    [TEXT_REWARD_REPAIR_SHORT] = {.ja = "社を修繕", .en = "Repair Shrine"},
    [TEXT_REWARD_REPAIR_EXCESS] = {.ja = "超過分 +15 墨晶/HP", .en = "+15 Cr per excess HP"},
    [TEXT_REWARD_CRYSTAL_TITLE] = {.ja = "墨晶を得る", .en = "Claim Ink Crystals"},
    [TEXT_REWARD_LV] = {.ja = "Lv", .en = "Lv"},
    [TEXT_REWARD_HINT] = {.ja = "1〜3 またはクリックで選択", .en = "Press 1-3 or click to pick"},
    [TEXT_REWARD_FALLBACK_HINT] = {.ja = "1 / 2 またはクリックで選択",
                                   .en = "Press 1-2 or click to pick"},
    [TEXT_REWARD_TRADEOFF] = {.ja = "代償: ", .en = "Cost: "},

    [TEXT_AXIS_BLAZE] = {.ja = "紅蓮", .en = "Blaze"},
    [TEXT_AXIS_THUNDER] = {.ja = "雷霆", .en = "Thunder"},
    [TEXT_AXIS_GALE] = {.ja = "烈風", .en = "Gale"},
    [TEXT_AXIS_INKWAY] = {.ja = "墨道", .en = "Inkway"},
    [TEXT_AXIS_COMMON] = {.ja = "共通", .en = "Common"},

    /* 24 Upgrades */
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
    [TEXT_UPG_FLAME_GALE_NAME] = {.ja = "焔の風斬り", .en = "Flame Gale"},
    [TEXT_UPG_FLAME_GALE_LV1] = {.ja = "風斬りに炎 0.25", .en = "Gale adds 0.25 fire"},
    [TEXT_UPG_FLAME_GALE_LV2] = {.ja = "風斬りに炎 0.35", .en = "Gale adds 0.35 fire"},
    [TEXT_UPG_FLAME_GALE_LV3] = {.ja = "炎 0.5・終点で爆炎 20",
                                 .en = "Fire 0.50 + end blast 20 dmg"},
    [TEXT_UPG_INFERNO_NAME] = {.ja = "業火", .en = "Inferno"},
    [TEXT_UPG_INFERNO_LV1] = {.ja = "炎上ダメージ 14", .en = "Burn damage 14"},
    [TEXT_UPG_INFERNO_LV2] = {.ja = "炎上ダメージ 18", .en = "Burn damage 18"},
    [TEXT_UPG_INFERNO_LV3] = {.ja = "炎上 22・撃破時に火種", .en = "Burn 22 + ember on burn kill"},
    [TEXT_UPG_PHOENIX_ARROW_NAME] = {.ja = "不死鳥の矢", .en = "Phoenix Arrow"},
    [TEXT_UPG_PHOENIX_ARROW_LV1] = {.ja = "炎上弾の貫通 3", .en = "Burning shot pierce 3"},
    [TEXT_UPG_PHOENIX_ARROW_LV2] = {.ja = "炎上弾の貫通 4", .en = "Burning shot pierce 4"},
    [TEXT_UPG_PHOENIX_ARROW_LV3] = {.ja = "貫通 5・命中ごとに炎飛沫",
                                    .en = "Pierce 5 + fire splash on hit"},
    [TEXT_UPG_THUNDER_ARROW_NAME] = {.ja = "雷撃弾", .en = "Thunder Arrow"},
    [TEXT_UPG_THUNDER_ARROW_LV1] = {.ja = "雷撃弾の追加雷撃 2体",
                                    .en = "Storm shot arcs to 2 foes"},
    [TEXT_UPG_THUNDER_ARROW_LV2] = {.ja = "雷撃弾の追加雷撃 3体",
                                    .en = "Storm shot arcs to 3 foes"},
    [TEXT_UPG_THUNDER_ARROW_LV3] = {.ja = "3体・全偏向弾が自動帯電",
                                    .en = "3 arcs + all deflected shots charge"},
    [TEXT_UPG_SWIFT_THUNDER_NAME] = {.ja = "迅雷", .en = "Swift Thunder"},
    [TEXT_UPG_SWIFT_THUNDER_LV1] = {.ja = "雷撃の CD 0.45秒", .en = "Storm zap CD 0.45s"},
    [TEXT_UPG_SWIFT_THUNDER_LV2] = {.ja = "雷撃の CD 0.38秒", .en = "Storm zap CD 0.38s"},
    [TEXT_UPG_SWIFT_THUNDER_LV3] = {.ja = "雷撃の CD 0.30秒", .en = "Storm zap CD 0.30s"},
    [TEXT_UPG_THUNDERBURST_FUSE_NAME] = {.ja = "爆雷の導火", .en = "Thunderburst Fuse"},
    [TEXT_UPG_THUNDERBURST_FUSE_LV1] = {.ja = "爆雷の閾値 0.26", .en = "Thunderburst thresh 0.26"},
    [TEXT_UPG_THUNDERBURST_FUSE_LV2] = {.ja = "爆雷の閾値 0.22", .en = "Thunderburst thresh 0.22"},
    [TEXT_UPG_THUNDERBURST_FUSE_LV3] = {.ja = "閾値 0.18・半径 +30%",
                                        .en = "Thresh 0.18 + radius +30%"},
    [TEXT_UPG_MIGHTY_STROKE_NAME] = {.ja = "剛腕の筆", .en = "Mighty Stroke"},
    [TEXT_UPG_MIGHTY_STROKE_LV1] = {.ja = "風筆の力 ×1.15", .en = "Brush force x1.15"},
    [TEXT_UPG_MIGHTY_STROKE_LV2] = {.ja = "風筆の力 ×1.30", .en = "Brush force x1.30"},
    [TEXT_UPG_MIGHTY_STROKE_LV3] = {.ja = "風筆の力 ×1.45", .en = "Brush force x1.45"},
    [TEXT_UPG_MAELSTROM_BIND_NAME] = {.ja = "渦縛り", .en = "Maelstrom Bind"},
    [TEXT_UPG_MAELSTROM_BIND_LV1] = {.ja = "渦で軽量敵を1.0秒拘束",
                                     .en = "Vortex binds light foes 1.0s"},
    [TEXT_UPG_MAELSTROM_BIND_LV2] = {.ja = "中量の敵も1.0秒拘束", .en = "Binds medium foes too"},
    [TEXT_UPG_MAELSTROM_BIND_LV3] = {.ja = "拘束中の敵衝突で 40ダメ",
                                     .en = "Bound foe collisions deal 40 dmg"},
    [TEXT_UPG_TAILWIND_NAME] = {.ja = "追い風", .en = "Tailwind"},
    [TEXT_UPG_TAILWIND_LV1] = {.ja = "偏向弾の追尾力 ×1.3", .en = "Deflected homing x1.3"},
    [TEXT_UPG_TAILWIND_LV2] = {.ja = "偏向弾の追尾力 ×1.6", .en = "Deflected homing x1.6"},
    [TEXT_UPG_TAILWIND_LV3] = {.ja = "追尾 ×2.0・最高速 +20%",
                               .en = "Homing x2.0 + max speed +20%"},
    [TEXT_UPG_INK_MAGNET_NAME] = {.ja = "吸墨", .en = "Ink Magnet"},
    [TEXT_UPG_INK_MAGNET_LV1] = {.ja = "墨玉の吸引範囲 170px", .en = "Orb pull range 170px"},
    [TEXT_UPG_INK_MAGNET_LV2] = {.ja = "墨玉の吸引範囲 210px", .en = "Orb pull range 210px"},
    [TEXT_UPG_INK_MAGNET_LV3] = {.ja = "250px・渦印で全墨玉吸引",
                                 .en = "250px + cyclone pulls all orbs"},
    [TEXT_UPG_CHAIN_PRIMER_NAME] = {.ja = "呼び水", .en = "Chain Primer"},
    [TEXT_UPG_CHAIN_PRIMER_LV1] = {.ja = "一筆連殺の猶予 3.2秒", .en = "Chain grace 3.2s"},
    [TEXT_UPG_CHAIN_PRIMER_LV2] = {.ja = "一筆連殺の猶予 3.8秒", .en = "Chain grace 3.8s"},
    [TEXT_UPG_CHAIN_PRIMER_LV3] = {.ja = "一筆連殺の猶予 4.5秒", .en = "Chain grace 4.5s"},
    [TEXT_UPG_DRY_MASTERY_NAME] = {.ja = "かすれの妙", .en = "Dry Mastery"},
    [TEXT_UPG_DRY_MASTERY_LV1] = {.ja = "かすれ筆の力 ×0.75", .en = "Dry brush force x0.75"},
    [TEXT_UPG_DRY_MASTERY_LV2] = {.ja = "かすれ筆の力 ×1.00", .en = "Dry brush force x1.00"},
    [TEXT_UPG_DRY_MASTERY_LV3] = {.ja = "かすれ中消費0・偏向ダメ+20%",
                                  .en = "0 dry cost + deflected dmg +20%"},
    [TEXT_UPG_SHRINE_WARD_NAME] = {.ja = "社の護符", .en = "Shrine Ward"},
    [TEXT_UPG_SHRINE_WARD_LV1] = {.ja = "社最大 HP +1・HP +1", .en = "Shrine Max HP +1, Heal +1"},
    [TEXT_UPG_SHRINE_WARD_LV2] = {.ja = "社最大 HP +2・さらに +1",
                                  .en = "Shrine Max HP +2, Heal +1"},
    [TEXT_UPG_SHRINE_WARD_LV3] = {.ja = "社最大 HP +3・さらに +1",
                                  .en = "Shrine Max HP +3, Heal +1"},
    [TEXT_UPG_DEEP_INK_NAME] = {.ja = "濃墨", .en = "Deep Ink"},
    [TEXT_UPG_DEEP_INK_LV1] = {.ja = "墨ゲージ上限 +15", .en = "Ink Max +15"},
    [TEXT_UPG_DEEP_INK_LV2] = {.ja = "墨ゲージ上限 +30", .en = "Ink Max +30"},
    [TEXT_UPG_DEEP_INK_LV3] = {.ja = "墨ゲージ上限 +45", .en = "Ink Max +45"},
    [TEXT_UPG_DEEP_MARK_NAME] = {.ja = "墨印深化", .en = "Deep Mark"},
    [TEXT_UPG_DEEP_MARK_LV1] = {.ja = "墨印倍率 ×2.2・持続 +1秒", .en = "Ink Mark x2.2, dur +1s"},
    [TEXT_UPG_DEEP_MARK_LV2] = {.ja = "墨印倍率 ×2.4・持続 +2秒", .en = "Ink Mark x2.4, dur +2s"},
    [TEXT_UPG_DEEP_MARK_LV3] = {.ja = "墨印倍率 ×2.6・持続 +3秒", .en = "Ink Mark x2.6, dur +3s"},
    [TEXT_UPG_BARRIER_NAME] = {.ja = "結界", .en = "Barrier"},
    [TEXT_UPG_BARRIER_LV1] = {.ja = "社無敵 +0.3秒", .en = "Shrine invuln +0.3s"},
    [TEXT_UPG_BARRIER_LV2] = {.ja = "社無敵 +0.6秒", .en = "Shrine invuln +0.6s"},
    [TEXT_UPG_BARRIER_LV3] = {.ja = "+0.9秒・12秒毎敵弾1発無効",
                              .en = "+0.9s + block 1 shot per 12s"},
    [TEXT_UPG_LINGERING_EMBERS_NAME] = {.ja = "残り火", .en = "Lingering Embers"},
    [TEXT_UPG_LINGERING_EMBERS_LV1] = {.ja = "炎の減衰 0.945", .en = "Fire decay 0.945"},
    [TEXT_UPG_LINGERING_EMBERS_LV2] = {.ja = "炎の減衰 0.955", .en = "Fire decay 0.955"},
    [TEXT_UPG_LINGERING_EMBERS_LV3] = {.ja = "炎の減衰 0.965", .en = "Fire decay 0.965"},
    [TEXT_UPG_STORM_BRUSH_NAME] = {.ja = "雷雲の筆", .en = "Storm Brush"},
    [TEXT_UPG_STORM_BRUSH_LV1] = {.ja = "連殺の雷がチェイン×2毎", .en = "Chain lightning every x2"},
    [TEXT_UPG_STORM_BRUSH_LV2] = {.ja = "連殺の雷の量 ×1.5", .en = "Chain storm power x1.5"},
    [TEXT_UPG_STORM_BRUSH_LV3] = {.ja = "墨昇華中の風筆に雷が混ざる",
                                  .en = "Surge strokes infuse storm"},
    [TEXT_UPG_DOUBLE_GALE_NAME] = {.ja = "二段風斬り", .en = "Double Gale"},
    [TEXT_UPG_DOUBLE_GALE_LV1] = {.ja = "風斬りの CD 0.85秒", .en = "Gale Burst CD 0.85s"},
    [TEXT_UPG_DOUBLE_GALE_LV2] = {.ja = "風斬りの CD 0.70秒", .en = "Gale Burst CD 0.70s"},
    [TEXT_UPG_DOUBLE_GALE_LV3] = {.ja = "風斬りを2回まで蓄積", .en = "Store up to 2 Gale charges"},
    [TEXT_UPG_INKSTONE_BLESSING_NAME] = {.ja = "硯の恵み", .en = "Inkstone Blessing"},
    [TEXT_UPG_INKSTONE_BLESSING_LV1] = {.ja = "硯の回復 +20%", .en = "Inkstone regen +20%"},
    [TEXT_UPG_INKSTONE_BLESSING_LV2] = {.ja = "硯の回復 +40%", .en = "Inkstone regen +40%"},
    [TEXT_UPG_INKSTONE_BLESSING_LV3] = {.ja = "硯の回復 +60%・待ち時間0",
                                        .en = "Regen +60%, 0 delay"},

    /* 15 Relics (9 Initial + 6 Unlocked) */
    [TEXT_RELIC_VERMILION_INKSTONE_NAME] = {.ja = "朱の硯", .en = "Vermilion Inkstone"},
    [TEXT_RELIC_VERMILION_INKSTONE_EFFECT] = {.ja = "筆跡200pxごとに火種を置く",
                                              .en = "Drops ember every 200px of stroke"},
    [TEXT_RELIC_VERMILION_INKSTONE_TRADEOFF] = {.ja = "墨の減衰 0.980", .en = "Ink decay 0.980"},
    [TEXT_RELIC_THUNDER_DRUM_NAME] = {.ja = "雷神の太鼓", .en = "Thunder Drum"},
    [TEXT_RELIC_THUNDER_DRUM_EFFECT] = {.ja = "渦印の中心に落雷（雷1.0・40ダメ）",
                                        .en = "Lightning strike at cyclone center (40 dmg)"},
    [TEXT_RELIC_TENGU_FAN_NAME] = {.ja = "天狗の羽団扇", .en = "Tengu Fan"},
    [TEXT_RELIC_TENGU_FAN_EFFECT] = {.ja = "風斬りが3方向（±25度・60%）に分かれる",
                                     .en = "Gale splits 3 ways (+/-25 deg, 60% power)"},
    [TEXT_RELIC_TENGU_FAN_TRADEOFF] = {.ja = "風斬りの消費 +6", .en = "Gale ink cost +6"},
    [TEXT_RELIC_INK_DYED_SHRINE_NAME] = {.ja = "墨染めの社", .en = "Ink-Dyed Shrine"},
    [TEXT_RELIC_INK_DYED_SHRINE_EFFECT] = {.ja = "社の周囲（半径6セル）から常に墨が湧く",
                                           .en = "Shrine constantly emits ink around it"},
    [TEXT_RELIC_TWIN_BRUSH_NAME] = {.ja = "連理の筆", .en = "Twin Brush"},
    [TEXT_RELIC_TWIN_BRUSH_EFFECT] = {.ja = "社を中心に点対称の筆跡も同時に描く",
                                      .en = "Mirrors strokes symmetrically across shrine"},
    [TEXT_RELIC_TWIN_BRUSH_TRADEOFF] = {.ja = "風筆の消費 ×1.3", .en = "Stroke ink cost x1.3"},
    [TEXT_RELIC_ROSARY_SWIRL_NAME] = {.ja = "数珠の渦", .en = "Rosary Swirl"},
    [TEXT_RELIC_ROSARY_SWIRL_EFFECT] = {.ja = "墨玉回収位置に小渦が起き敵弾を逸らす",
                                        .en = "Orb pickup spawns a mini swirl"},
    [TEXT_RELIC_PACT_OF_FIRE_NAME] = {.ja = "業火の契り", .en = "Pact of Fire"},
    [TEXT_RELIC_PACT_OF_FIRE_EFFECT] = {.ja = "炎分類ダメージ ×2.0", .en = "Fire damage x2.0"},
    [TEXT_RELIC_PACT_OF_FIRE_TRADEOFF] = {.ja = "社の最大 HP -2", .en = "Shrine Max HP -2"},
    [TEXT_RELIC_LINGERING_INK_NAME] = {.ja = "残墨", .en = "Lingering Ink"},
    [TEXT_RELIC_LINGERING_INK_EFFECT] = {.ja = "墨の減衰 0.993・墨印持続 +2秒",
                                         .en = "Ink decay 0.993, Ink Mark +2s"},
    [TEXT_RELIC_WIND_RETURN_NAME] = {.ja = "返し風", .en = "Wind Return"},
    [TEXT_RELIC_WIND_RETURN_EFFECT] = {.ja = "被弾直前に社から自動風斬り（12秒毎）",
                                       .en = "Auto gale from shrine before hit (12s CD)"},
    [TEXT_RELIC_SEAL_OF_TRINITY_NAME] = {.ja = "三元の印", .en = "Seal of Trinity"},
    [TEXT_RELIC_SEAL_OF_TRINITY_EFFECT] = {.ja = "三元共鳴の閾値 0.22・CD 1.2秒",
                                           .en = "Cataclysm thresh 0.22, CD 1.2s"},
    [TEXT_RELIC_INKSTORM_SECRET_NAME] = {.ja = "墨嵐の極意", .en = "Inkstorm Secret"},
    [TEXT_RELIC_INKSTORM_SECRET_EFFECT] = {.ja = "墨昇華中の全反応ダメ ×1.5",
                                           .en = "Reaction damage x1.5 in Surge"},
    [TEXT_RELIC_INKSTORM_SECRET_TRADEOFF] = {.ja = "墨昇華がチェイン×3から",
                                             .en = "Ink Surge starts at Chain x3"},
    [TEXT_RELIC_THUNDERFIRE_GEM_NAME] = {.ja = "雷火の宝珠", .en = "Thunderfire Gem"},
    [TEXT_RELIC_THUNDERFIRE_GEM_EFFECT] = {.ja = "爆雷地点に墨玉を1個落とす（1.5秒毎）",
                                           .en = "Thunderburst drops 1 Ink Orb (1.5s CD)"},
    [TEXT_RELIC_ONI_TILE_NAME] = {.ja = "鬼瓦", .en = "Oni Tile"},
    [TEXT_RELIC_ONI_TILE_EFFECT] = {.ja = "中量も叩きつけ即撃破・重量を1段軽減",
                                    .en = "Slam one-shots medium; lightens heavy"},
    [TEXT_RELIC_ENDLESS_BRUSH_NAME] = {.ja = "無尽の筆", .en = "Endless Brush"},
    [TEXT_RELIC_ENDLESS_BRUSH_EFFECT] = {.ja = "硯の自然回復 ×2.5", .en = "Inkstone regen x2.5"},
    [TEXT_RELIC_ENDLESS_BRUSH_TRADEOFF] = {.ja = "墨ゲージ上限 -40", .en = "Ink Max -40"},
    [TEXT_RELIC_NIGHT_PARADE_SCROLL_NAME] = {.ja = "百鬼夜行の巻物", .en = "Night Parade Scroll"},
    [TEXT_RELIC_NIGHT_PARADE_SCROLL_EFFECT] = {.ja = "獲得墨晶 ×1.5・撃破時墨玉 +1",
                                               .en = "Crystals x1.5, +1 Ink Orb per kill"},
    [TEXT_RELIC_NIGHT_PARADE_SCROLL_TRADEOFF] = {.ja = "増援間隔×0.75・同時上限+2",
                                                 .en = "Spawn interval x0.75, max alive +2"},

    /* 4 Bokuan Permanent Upgrades */
    [TEXT_SHOP_UPG_SHRINE_HP_NAME] = {.ja = "社の加護", .en = "Shrine Blessing"},
    [TEXT_SHOP_UPG_SHRINE_HP_DESC] = {.ja = "社の最大 HP +1 / +2 / +3",
                                      .en = "Shrine Max HP +1 / +2 / +3"},
    [TEXT_SHOP_UPG_INK_MAX_NAME] = {.ja = "大きな硯", .en = "Grand Inkstone"},
    [TEXT_SHOP_UPG_INK_MAX_DESC] = {.ja = "墨ゲージ上限 +10 / +20 / +30",
                                    .en = "Ink Max +10 / +20 / +30"},
    [TEXT_SHOP_UPG_REROLLS_NAME] = {.ja = "再考の札", .en = "Scroll of Rethink"},
    [TEXT_SHOP_UPG_REROLLS_DESC] = {.ja = "幕ごとのリロール回数 2 / 3回",
                                    .en = "Rerolls per act: 2 / 3"},
    [TEXT_SHOP_UPG_INK_REGEN_NAME] = {.ja = "早墨", .en = "Swift Grinding"},
    [TEXT_SHOP_UPG_INK_REGEN_DESC] = {.ja = "硯の自然回復 +10% / +20% / +30%",
                                      .en = "Ink regen +10% / +20% / +30%"},

    /* 6 Bokuan Relic Unlocks */
    [TEXT_SHOP_RELIC_0_NAME] = {.ja = "三元の印", .en = "Seal of Trinity"},
    [TEXT_SHOP_RELIC_0_DESC] = {.ja = "三元共鳴の閾値 0.22・CD 1.2秒",
                                .en = "Cataclysm thresh 0.22, CD 1.2s"},
    [TEXT_SHOP_RELIC_1_NAME] = {.ja = "墨嵐の極意", .en = "Inkstorm Secret"},
    [TEXT_SHOP_RELIC_1_DESC] = {.ja = "墨昇華中の反応ダメ ×1.5（連殺×3〜）",
                                .en = "Surge reaction dmg x1.5 (Chain x3+)"},
    [TEXT_SHOP_RELIC_2_NAME] = {.ja = "雷火の宝珠", .en = "Thunderfire Gem"},
    [TEXT_SHOP_RELIC_2_DESC] = {.ja = "爆雷地点に墨玉を1個落とす",
                                .en = "Thunderburst drops 1 Ink Orb"},
    [TEXT_SHOP_RELIC_3_NAME] = {.ja = "鬼瓦", .en = "Oni Tile"},
    [TEXT_SHOP_RELIC_3_DESC] = {.ja = "中量も叩きつけ即撃破・重量を軽減",
                                .en = "Slam one-shots medium; lightens heavy"},
    [TEXT_SHOP_RELIC_4_NAME] = {.ja = "無尽の筆", .en = "Endless Brush"},
    [TEXT_SHOP_RELIC_4_DESC] = {.ja = "硯回復 ×2.5（墨上限 -40）",
                                .en = "Ink regen x2.5 (Ink Max -40)"},
    [TEXT_SHOP_RELIC_5_NAME] = {.ja = "百鬼夜行の巻物", .en = "Night Parade Scroll"},
    [TEXT_SHOP_RELIC_5_DESC] = {.ja = "墨晶 ×1.5・墨玉 +1（増援強化）",
                                .en = "Crystals x1.5, Orbs +1 (denser waves)"},

    /* 3 Brush Styles */
    [TEXT_BRUSH_HEAVY_NAME] = {.ja = "豪筆", .en = "Heavy Brush"},
    [TEXT_BRUSH_HEAVY_DESC] = {.ja = "太く強い筆致。偏向と叩きつけのバランス型",
                               .en = "Bold wide strokes for deflection and slams"},
    [TEXT_BRUSH_HEAVY_STATS] = {.ja = "半径×1.25 / 力×1.2 / 消費×1.15 / 風斬り×1.25",
                                .en = "Rad x1.25 / Force x1.2 / Cost x1.15 / Gale x1.25"},
    [TEXT_BRUSH_HEAVY_REC] = {.ja = "烈風・墨道", .en = "Gale / Inkway"},
    [TEXT_BRUSH_EMBER_NAME] = {.ja = "朱筆", .en = "Ember Brush"},
    [TEXT_BRUSH_EMBER_DESC] = {.ja = "筆跡と風斬りに火種を宿す高火力型",
                               .en = "Weaves embers into strokes and Gale Burst"},
    [TEXT_BRUSH_EMBER_STATS] = {.ja = "120px毎火種 / 風斬り炎0.35 / 消費×1.2",
                                .en = "Ember 120px / Gale fire 0.35 / Cost x1.2"},
    [TEXT_BRUSH_EMBER_REC] = {.ja = "紅蓮・雷霆", .en = "Blaze / Thunder"},
    [TEXT_BRUSH_QUILL_NAME] = {.ja = "細筆", .en = "Quill Brush"},
    [TEXT_BRUSH_QUILL_DESC] = {.ja = "軽く速い筆致。帯電渦印と墨玉回収で手数を稼ぐ",
                               .en = "Nimble strokes with charged cyclones and orb pull"},
    [TEXT_BRUSH_QUILL_STATS] = {.ja = "半径×0.8 / 消費×0.7 / 渦CD1.1秒+雷0.55 / 吸引×1.3",
                                .en = "Rad x0.8 / Cost x0.7 / Cyclone 1.1s+Storm / Pull x1.3"},
    [TEXT_BRUSH_QUILL_REC] = {.ja = "雷霆・墨道", .en = "Thunder / Inkway"},

    /* 8 First-time Control & Reaction Hints */
    [TEXT_HINT_STROKE] = {.ja = "左ドラッグで風を描き、矢を逸らせ",
                          .en = "Drag left mouse to paint wind and deflect arrows"},
    [TEXT_HINT_CYCLONE_FIREBALL] = {.ja = "右クリックの渦印で、火球を墨に巻き込め",
                                    .en = "Right-click Cyclone to pull fireballs into ink"},
    [TEXT_HINT_GALE_UMBRELLA] = {.ja = "Space の風斬りで壁へ叩きつけろ",
                                 .en = "Press Space for Gale Burst to slam foes into walls"},
    [TEXT_HINT_INK_REGEN] = {.ja = "筆を離すと、硯で墨が戻る",
                             .en = "Release the brush to refill ink at the inkstone"},
    [TEXT_HINT_REACT_BLAZING] = {.ja = "紅蓮墨嵐：墨と炎が燃え広がり、通過弾が炎上弾になる",
                                 .en = "Blazing Inkstorm: Ink+Fire burns foes and ignites shots"},
    [TEXT_HINT_REACT_TEMPEST] = {.ja = "雷墨連鎖：墨の上の雷が、近くの敵へ次々と跳ねる",
                                 .en = "Tempest Lightning: Storm over ink chains between foes"},
    [TEXT_HINT_REACT_THUNDERBURST] = {.ja = "爆雷：炎と雷が交わり、重敵も吹き飛ぶ爆風が起きる",
                                      .en = "Thunderburst: Fire+Storm blasts even heavy foes away"},
    [TEXT_HINT_REACT_CATACLYSM] = {.ja = "三元共鳴：墨・炎・雷が重なり、大爆風と広域雷撃を放つ",
                                   .en = "Cataclysm: Ink+Fire+Storm unleashes a massive blast"},

    /* Lobby, Brush Select, Bokuan, Settings, Pause */
    [TEXT_LOBBY_SUBTITLE] = {.ja = "水墨流体アクション・ローグライト",
                             .en = "Sumi-e Fluid Action Roguelite"},
    [TEXT_LOBBY_SORTIE] = {.ja = "出陣", .en = "Sortie"},
    [TEXT_LOBBY_BOKUAN] = {.ja = "墨庵", .en = "Bokuan"},
    [TEXT_LOBBY_SETTINGS] = {.ja = "設定", .en = "Settings"},
    [TEXT_LOBBY_QUIT] = {.ja = "終了", .en = "Quit"},
    [TEXT_LOBBY_CRYSTALS] = {.ja = "所持墨晶", .en = "Crystals"},
    [TEXT_LOBBY_BEST_ACT] = {.ja = "最高到達", .en = "Best Act"},
    [TEXT_LOBBY_BEST_SCORE] = {.ja = "最高スコア", .en = "Best Score"},
    [TEXT_LOBBY_CLEARS] = {.ja = "累計クリア", .en = "Clears"},
    [TEXT_LOBBY_CANVAS_HINT] = {.ja = "左ドラッグで和紙に墨を描けます",
                                .en = "Drag left mouse to paint ink on the canvas"},

    [TEXT_BRUSH_SELECT_TITLE] = {.ja = "筆を選べ", .en = "Select Your Brush"},
    [TEXT_BRUSH_SELECT_HINT] = {.ja = "1〜3 またはクリックで出陣　Esc で戻る",
                                .en = "Press 1-3 or click to start, Esc to return"},
    [TEXT_BRUSH_REC_LABEL] = {.ja = "推奨軸: ", .en = "Recommended: "},

    [TEXT_BOKUAN_TITLE] = {.ja = "墨庵", .en = "Bokuan Shop"},
    [TEXT_BOKUAN_UPGRADES_HDR] = {.ja = "恒久強化", .en = "Permanent Upgrades"},
    [TEXT_BOKUAN_RELICS_HDR] = {.ja = "大遺物の解放", .en = "Relic Unlocks"},
    [TEXT_BOKUAN_MAXED] = {.ja = "最大", .en = "MAX"},
    [TEXT_BOKUAN_UNLOCKED] = {.ja = "解放済", .en = "Unlocked"},
    [TEXT_BOKUAN_COST_UNIT] = {.ja = "墨晶", .en = "Cr"},
    [TEXT_BOKUAN_BACK] = {.ja = "ロビーへ戻る [Esc]", .en = "Back to Lobby [Esc]"},
    [TEXT_BOKUAN_HINT] = {.ja = "クリックまたは Enter で購入　Esc で戻る",
                          .en = "Click or Enter to purchase, Esc to return"},

    [TEXT_SETTINGS_TITLE] = {.ja = "設定", .en = "Settings"},
    [TEXT_SETTINGS_MASTER] = {.ja = "マスター音量", .en = "Master Volume"},
    [TEXT_SETTINGS_BGM] = {.ja = "BGM 音量", .en = "BGM Volume"},
    [TEXT_SETTINGS_SE] = {.ja = "SE 音量", .en = "SE Volume"},
    [TEXT_SETTINGS_POST] = {.ja = "ポストシェーダ", .en = "Post Shader"},
    [TEXT_SETTINGS_SHAKE] = {.ja = "画面揺れ", .en = "Screen Shake"},
    [TEXT_SETTINGS_HINTS] = {.ja = "操作ヒント", .en = "Control Hints"},
    [TEXT_SETTINGS_LANG] = {.ja = "表示言語", .en = "Language"},
    [TEXT_SETTINGS_LANG_AUTO] = {.ja = "自動", .en = "Auto"},
    [TEXT_SETTINGS_LANG_JA] = {.ja = "日本語", .en = "Japanese"},
    [TEXT_SETTINGS_LANG_EN] = {.ja = "English", .en = "English"},
    [TEXT_SETTINGS_REDUCE_FLASH] = {.ja = "閃光軽減", .en = "Reduce Flash"},
    [TEXT_SETTINGS_CURSOR_SCALE] = {.ja = "カーソル倍率", .en = "Cursor Scale"},
    [TEXT_SETTINGS_RESET_HINTS] = {.ja = "初回ヒントを初期化", .en = "Reset First-Time Hints"},
    [TEXT_SETTINGS_RESET_DEFAULTS] = {.ja = "設定を既定に戻す", .en = "Reset Settings to Default"},
    [TEXT_SETTINGS_NOTICE_HINTS] = {.ja = "初回ヒントを初期化しました",
                                    .en = "First-time hints reset"},
    [TEXT_SETTINGS_NOTICE_DEFAULTS] = {.ja = "設定を既定に戻しました",
                                       .en = "Settings restored to defaults"},
    [TEXT_SETTINGS_ON] = {.ja = "ON", .en = "ON"},
    [TEXT_SETTINGS_OFF] = {.ja = "OFF", .en = "OFF"},
    [TEXT_SETTINGS_BACK] = {.ja = "戻る [Esc]", .en = "Back [Esc]"},
    [TEXT_SETTINGS_HINT] = {.ja = "左右 / クリックで変更　Esc で保存して戻る",
                            .en = "Left/Right or click to adjust, Esc to save"},

    [TEXT_PAUSE_TITLE] = {.ja = "一時停止", .en = "Paused"},
    [TEXT_PAUSE_RESUME] = {.ja = "再開", .en = "Resume"},
    [TEXT_PAUSE_RESTART] = {.ja = "最初から", .en = "Restart"},
    [TEXT_PAUSE_SETTINGS] = {.ja = "設定", .en = "Settings"},
    [TEXT_PAUSE_LOBBY] = {.ja = "ロビーへ戻る", .en = "Return to Lobby"},
    [TEXT_PAUSE_BUILD_HDR] = {.ja = "所持強化札・大遺物", .en = "Owned Upgrades & Relics"},
    [TEXT_PAUSE_NONE] = {.ja = "なし", .en = "None"},
    [TEXT_PAUSE_CONFIRM_MSG] = {.ja = "現在の出陣を敗北として精算します。よろしいですか？",
                                .en = "End this run as a defeat and settle crystals?"},
    [TEXT_PAUSE_CONFIRM_YES] = {.ja = "はい", .en = "Yes"},
    [TEXT_PAUSE_CONFIRM_NO] = {.ja = "いいえ", .en = "No"},

    /* Result Screen */
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
    [TEXT_RESULT_RELICS] = {.ja = "大遺物", .en = "Relics"},
    [TEXT_RESULT_SEED] = {.ja = "シード", .en = "Seed"},
    [TEXT_RESULT_CRYSTALS] = {.ja = "墨晶精算", .en = "Ink Crystals"},
    [TEXT_RESULT_CRYSTALS_PREVIEW] = {.ja = "墨晶内訳（未精算）", .en = "Ink Crystals (Preview)"},
    [TEXT_RESULT_CRYSTAL_ACTS] = {.ja = "幕踏破", .en = "Acts"},
    [TEXT_RESULT_CRYSTAL_KILLS] = {.ja = "撃破", .en = "Kills"},
    [TEXT_RESULT_CRYSTAL_CHAIN] = {.ja = "最大連殺", .en = "Max Chain"},
    [TEXT_RESULT_CRYSTAL_REACTIONS] = {.ja = "反応", .en = "Reactions"},
    [TEXT_RESULT_CRYSTAL_ENDLESS] = {.ja = "エンドレス", .en = "Endless"},
    [TEXT_RESULT_CRYSTAL_BONUS] = {.ja = "巻物ボーナス", .en = "Scroll Bonus"},
    [TEXT_RESULT_CRYSTAL_TOTAL] = {.ja = "獲得計", .en = "Earned"},
    [TEXT_RESULT_CRYSTAL_OWNED] = {.ja = "所持計", .en = "Total Owned"},
    [TEXT_RESULT_ENDLESS] = {.ja = "エンドレスへ進む", .en = "Enter Endless"},
    [TEXT_RESULT_TO_LOBBY] = {.ja = "ロビーへ戻る", .en = "Return to Lobby"},
    [TEXT_RESULT_PLAY_AGAIN] = {.ja = "再出陣", .en = "Play Again"},

    /* Seals & Showcase */
    [TEXT_SEAL_PURIFY] = {.ja = "祓", .en = "P"},
    [TEXT_SEAL_SLAY] = {.ja = "討", .en = "S"},
    [TEXT_SEAL_DONE] = {.ja = "決", .en = "OK"},
    [TEXT_SEAL_SANGEN] = {.ja = "三元", .en = "TRI"},
    [TEXT_SEAL_INKSTORM] = {.ja = "墨嵐", .en = "INK"},
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
    case ENEMY_LANTERN_STORM:
        return TEXT_ENEMY_LANTERN_STORM_NAME;
    case ENEMY_KAMAITACHI:
        return TEXT_ENEMY_KAMAITACHI_NAME;
    case ENEMY_STONE_ONI:
        return TEXT_ENEMY_STONE_ONI_NAME;
    case ENEMY_INK_LEECH:
        return TEXT_ENEMY_INK_LEECH_NAME;
    case ENEMY_BOSS_NUE:
        return TEXT_ENEMY_BOSS_NUE_NAME;
    case ENEMY_BOSS_RYU:
        return TEXT_ENEMY_BOSS_RYU_NAME;
    case ENEMY_OFUDA_FOX:
        return TEXT_ENEMY_OFUDA_FOX_NAME;
    case ENEMY_BOSS_RYU_ORB:
        return TEXT_ENEMY_BOSS_RYU_ORB_NAME;
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
    case ENEMY_LANTERN_STORM:
        return TEXT_ENEMY_LANTERN_STORM_INTRO;
    case ENEMY_KAMAITACHI:
        return TEXT_ENEMY_KAMAITACHI_INTRO;
    case ENEMY_STONE_ONI:
        return TEXT_ENEMY_STONE_ONI_INTRO;
    case ENEMY_INK_LEECH:
        return TEXT_ENEMY_INK_LEECH_INTRO;
    case ENEMY_BOSS_NUE:
        return TEXT_ENEMY_BOSS_NUE_INTRO;
    case ENEMY_BOSS_RYU:
        return TEXT_ENEMY_BOSS_RYU_INTRO;
    case ENEMY_OFUDA_FOX:
        return TEXT_ENEMY_OFUDA_FOX_INTRO;
    case ENEMY_BOSS_RYU_ORB:
        return TEXT_ENEMY_BOSS_RYU_ORB_INTRO;
    default:
        return TEXT_NONE;
    }
}

TextId text_boss_epithet(EnemyDefId id)
{
    switch (id) {
    case ENEMY_BOSS_TENGU:
        return TEXT_BOSS_TENGU_EPITHET;
    case ENEMY_BOSS_NUE:
        return TEXT_BOSS_NUE_EPITHET;
    case ENEMY_BOSS_RYU:
        return TEXT_BOSS_RYU_EPITHET;
    default:
        return TEXT_NONE;
    }
}

TextId text_upgrade_name(UpgradeId id)
{
    if ((unsigned)id >= (unsigned)UPG_COUNT) {
        return TEXT_NONE;
    }
    return (TextId)(TEXT_UPG_EMBER_TIP_NAME + (int)id * 4);
}

TextId text_upgrade_effect(UpgradeId id, int level)
{
    if ((unsigned)id >= (unsigned)UPG_COUNT || level < 1 || level > 3) {
        return TEXT_NONE;
    }
    return (TextId)(TEXT_UPG_EMBER_TIP_NAME + (int)id * 4 + level);
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
    switch (act_index) {
    case 0:
        return TEXT_ACT1_TITLE;
    case 1:
        return TEXT_ACT2_TITLE;
    case 2:
        return TEXT_ACT3_TITLE;
    default:
        return TEXT_NONE;
    }
}

TextId text_act_short(int act_index)
{
    switch (act_index) {
    case 0:
        return TEXT_ACT1_SHORT;
    case 1:
        return TEXT_ACT2_SHORT;
    case 2:
        return TEXT_ACT3_SHORT;
    default:
        return TEXT_NONE;
    }
}

TextId text_relic_name(RelicId id)
{
    switch (id) {
    case RELIC_VERMILION_INKSTONE:
        return TEXT_RELIC_VERMILION_INKSTONE_NAME;
    case RELIC_THUNDER_DRUM:
        return TEXT_RELIC_THUNDER_DRUM_NAME;
    case RELIC_TENGU_FAN:
        return TEXT_RELIC_TENGU_FAN_NAME;
    case RELIC_INK_DYED_SHRINE:
        return TEXT_RELIC_INK_DYED_SHRINE_NAME;
    case RELIC_TWIN_BRUSH:
        return TEXT_RELIC_TWIN_BRUSH_NAME;
    case RELIC_ROSARY_SWIRL:
        return TEXT_RELIC_ROSARY_SWIRL_NAME;
    case RELIC_PACT_OF_FIRE:
        return TEXT_RELIC_PACT_OF_FIRE_NAME;
    case RELIC_LINGERING_INK:
        return TEXT_RELIC_LINGERING_INK_NAME;
    case RELIC_WIND_RETURN:
        return TEXT_RELIC_WIND_RETURN_NAME;
    case RELIC_SEAL_OF_TRINITY:
        return TEXT_RELIC_SEAL_OF_TRINITY_NAME;
    case RELIC_INKSTORM_SECRET:
        return TEXT_RELIC_INKSTORM_SECRET_NAME;
    case RELIC_THUNDERFIRE_GEM:
        return TEXT_RELIC_THUNDERFIRE_GEM_NAME;
    case RELIC_ONI_TILE:
        return TEXT_RELIC_ONI_TILE_NAME;
    case RELIC_ENDLESS_BRUSH:
        return TEXT_RELIC_ENDLESS_BRUSH_NAME;
    case RELIC_NIGHT_PARADE_SCROLL:
        return TEXT_RELIC_NIGHT_PARADE_SCROLL_NAME;
    default:
        return TEXT_NONE;
    }
}

TextId text_relic_effect(RelicId id)
{
    switch (id) {
    case RELIC_VERMILION_INKSTONE:
        return TEXT_RELIC_VERMILION_INKSTONE_EFFECT;
    case RELIC_THUNDER_DRUM:
        return TEXT_RELIC_THUNDER_DRUM_EFFECT;
    case RELIC_TENGU_FAN:
        return TEXT_RELIC_TENGU_FAN_EFFECT;
    case RELIC_INK_DYED_SHRINE:
        return TEXT_RELIC_INK_DYED_SHRINE_EFFECT;
    case RELIC_TWIN_BRUSH:
        return TEXT_RELIC_TWIN_BRUSH_EFFECT;
    case RELIC_ROSARY_SWIRL:
        return TEXT_RELIC_ROSARY_SWIRL_EFFECT;
    case RELIC_PACT_OF_FIRE:
        return TEXT_RELIC_PACT_OF_FIRE_EFFECT;
    case RELIC_LINGERING_INK:
        return TEXT_RELIC_LINGERING_INK_EFFECT;
    case RELIC_WIND_RETURN:
        return TEXT_RELIC_WIND_RETURN_EFFECT;
    case RELIC_SEAL_OF_TRINITY:
        return TEXT_RELIC_SEAL_OF_TRINITY_EFFECT;
    case RELIC_INKSTORM_SECRET:
        return TEXT_RELIC_INKSTORM_SECRET_EFFECT;
    case RELIC_THUNDERFIRE_GEM:
        return TEXT_RELIC_THUNDERFIRE_GEM_EFFECT;
    case RELIC_ONI_TILE:
        return TEXT_RELIC_ONI_TILE_EFFECT;
    case RELIC_ENDLESS_BRUSH:
        return TEXT_RELIC_ENDLESS_BRUSH_EFFECT;
    case RELIC_NIGHT_PARADE_SCROLL:
        return TEXT_RELIC_NIGHT_PARADE_SCROLL_EFFECT;
    default:
        return TEXT_NONE;
    }
}

TextId text_relic_tradeoff(RelicId id)
{
    switch (id) {
    case RELIC_VERMILION_INKSTONE:
        return TEXT_RELIC_VERMILION_INKSTONE_TRADEOFF;
    case RELIC_TENGU_FAN:
        return TEXT_RELIC_TENGU_FAN_TRADEOFF;
    case RELIC_TWIN_BRUSH:
        return TEXT_RELIC_TWIN_BRUSH_TRADEOFF;
    case RELIC_PACT_OF_FIRE:
        return TEXT_RELIC_PACT_OF_FIRE_TRADEOFF;
    case RELIC_INKSTORM_SECRET:
        return TEXT_RELIC_INKSTORM_SECRET_TRADEOFF;
    case RELIC_ENDLESS_BRUSH:
        return TEXT_RELIC_ENDLESS_BRUSH_TRADEOFF;
    case RELIC_NIGHT_PARADE_SCROLL:
        return TEXT_RELIC_NIGHT_PARADE_SCROLL_TRADEOFF;
    default:
        return TEXT_NONE;
    }
}

TextId text_shop_upgrade_name(MetaUpgradeKind kind)
{
    switch (kind) {
    case META_UPG_SHRINE_HP:
        return TEXT_SHOP_UPG_SHRINE_HP_NAME;
    case META_UPG_INK_MAX:
        return TEXT_SHOP_UPG_INK_MAX_NAME;
    case META_UPG_REROLLS:
        return TEXT_SHOP_UPG_REROLLS_NAME;
    case META_UPG_INK_REGEN:
        return TEXT_SHOP_UPG_INK_REGEN_NAME;
    default:
        return TEXT_NONE;
    }
}

TextId text_shop_upgrade_desc(MetaUpgradeKind kind)
{
    switch (kind) {
    case META_UPG_SHRINE_HP:
        return TEXT_SHOP_UPG_SHRINE_HP_DESC;
    case META_UPG_INK_MAX:
        return TEXT_SHOP_UPG_INK_MAX_DESC;
    case META_UPG_REROLLS:
        return TEXT_SHOP_UPG_REROLLS_DESC;
    case META_UPG_INK_REGEN:
        return TEXT_SHOP_UPG_INK_REGEN_DESC;
    default:
        return TEXT_NONE;
    }
}

TextId text_shop_relic_name(uint32_t slot)
{
    if (slot >= (uint32_t)META_RELIC_UNLOCK_COUNT) {
        return TEXT_NONE;
    }
    return (TextId)(TEXT_SHOP_RELIC_0_NAME + (int)slot * 2);
}

TextId text_shop_relic_desc(uint32_t slot)
{
    if (slot >= (uint32_t)META_RELIC_UNLOCK_COUNT) {
        return TEXT_NONE;
    }
    return (TextId)(TEXT_SHOP_RELIC_0_DESC + (int)slot * 2);
}

TextId text_brush_name(BrushStyleId style)
{
    switch (style) {
    case BRUSH_STYLE_HEAVY:
        return TEXT_BRUSH_HEAVY_NAME;
    case BRUSH_STYLE_EMBER:
        return TEXT_BRUSH_EMBER_NAME;
    case BRUSH_STYLE_QUILL:
        return TEXT_BRUSH_QUILL_NAME;
    default:
        return TEXT_NONE;
    }
}

TextId text_brush_desc(BrushStyleId style)
{
    switch (style) {
    case BRUSH_STYLE_HEAVY:
        return TEXT_BRUSH_HEAVY_DESC;
    case BRUSH_STYLE_EMBER:
        return TEXT_BRUSH_EMBER_DESC;
    case BRUSH_STYLE_QUILL:
        return TEXT_BRUSH_QUILL_DESC;
    default:
        return TEXT_NONE;
    }
}

TextId text_brush_stats(BrushStyleId style)
{
    switch (style) {
    case BRUSH_STYLE_HEAVY:
        return TEXT_BRUSH_HEAVY_STATS;
    case BRUSH_STYLE_EMBER:
        return TEXT_BRUSH_EMBER_STATS;
    case BRUSH_STYLE_QUILL:
        return TEXT_BRUSH_QUILL_STATS;
    default:
        return TEXT_NONE;
    }
}

TextId text_brush_recommended(BrushStyleId style)
{
    switch (style) {
    case BRUSH_STYLE_HEAVY:
        return TEXT_BRUSH_HEAVY_REC;
    case BRUSH_STYLE_EMBER:
        return TEXT_BRUSH_EMBER_REC;
    case BRUSH_STYLE_QUILL:
        return TEXT_BRUSH_QUILL_REC;
    default:
        return TEXT_NONE;
    }
}

TextId text_hint_message(HintId id)
{
    if ((unsigned)id >= (unsigned)HINT_COUNT) {
        return TEXT_NONE;
    }
    return (TextId)(TEXT_HINT_STROKE + (int)id);
}
