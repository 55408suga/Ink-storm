# Phase 5-A タスク指示書：P2 ローグライト・強化札 28 種・墨庵解放遺物 6 種（全 15 種）・候補枯渇 2 択（`wt-roguelike-p2`）

> **必読**: [phase5-pre-contracts.md](phase5-pre-contracts.md)（特に §0.1 の進め方、§1.2 の排他所有表、§2 の凍結契約）、[PLAN.md](../PLAN.md) §9.1、[architecture.md](../architecture.md) §6 / §7、[game-design.md](../game-design.md) §3.6 / §5.3 / §5.4 / §9.1〜§9.5、[qa-and-testing.md](../qa-and-testing.md) §2、[AGENTS.md](../../AGENTS.md)、[inkstorm-content-authoring](../../.agents/skills/inkstorm-content-authoring/SKILL.md)、[inkstorm-fluid-sim](../../.agents/skills/inkstorm-fluid-sim/SKILL.md)、[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)
>
> **worktree / ブランチ**: `full-scale-dev/wt-roguelike-p2` / `feat/phase5-roguelike-p2`（P5-Pre マージ後の `main` から分岐）。完了したら単独で `main` へ PR を出す。
> **並列ペア**: P5-B（[phase5-endless-ryu-p2.md](phase5-endless-ryu-p2.md)）、P5-C（[phase5-ui-settings-p2.md](phase5-ui-settings-p2.md)）。合流と完了ゲートは P5-Int（[phase5-integration.md](phase5-integration.md)）。

---

## 0. 目的と責務境界

P2 拡張のローグライトビルド全カタログ（強化札 28 種＋大遺物 15 種）と、エンドレス周回時の報酬候補枯渇 2 択（修繕 vs 墨晶代替）を完成させる。

1. **残り強化札 4 種（全 28 種完成）**（game-design §9.4）：
   - 紅蓮：`残り火`（`UPG_LINGERING_EMBERS`）
   - 雷霆：`雷雲の筆`（`UPG_STORM_BRUSH`）
   - 烈風：`二段風斬り`（`UPG_DOUBLE_GALE` — 2 チャージ制 `PlayerState.gale_charges` 含む）
   - 墨道：`硯の恵み`（`UPG_INKSTONE_BLESSING`）
2. **墨庵解放の大遺物 6 種（全 15 種完成）**（game-design §9.5）：
   - `三元の印`（`RELIC_SEAL_OF_TRINITY`）、`墨嵐の極意`（`RELIC_INKSTORM_SECRET`）、`雷火の宝珠`（`RELIC_THUNDERFIRE_GEM`）、`鬼瓦`（`RELIC_ONI_TILE`）、`無尽の筆`（`RELIC_ENDLESS_BRUSH`）、`百鬼夜行の巻物`（`RELIC_NIGHT_PARADE_SCROLL`）。
   - `g->config.unlocked_relic_mask` による報酬プール解放制御、代償・7 フック実行順・流体/経済クランプの厳密反映。
3. **報酬候補枯渇時の 2 択フォールバック**（game-design §3.6、architecture §6.2）：
   - 全 28 札が Lv3 に達した後の通常ウェーブ報酬：「社を修繕 +1（`CMD_REPAIR_SHRINE`）」vs「墨晶 +10（`CMD_PICK_ENDLESS_CRYSTAL`）」。
   - 解放済み大遺物をすべて所持した後のボス報酬：「社を修繕 +3（最大 HP 超過分は 1 につき墨晶 +15 へ変換、`CMD_REPAIR_SHRINE`）」vs「墨晶 +30（`CMD_PICK_ENDLESS_CRYSTAL`）」。

**禁止事項**
- P5-Pre §1.2 の凍結ファイル（`CMakeLists.txt`, `meta.*`, `game.h`, `game_ids.h`, `game_events.h`, `game_components.h`, `game_contract.c`, `game_internal.h`, `game.c`, `hash.c`, `debug_api.c`, `ui_view.h` 等）を編集しない。
- P5-B の所有ファイル（`src/game/combat/*`, `src/game/enemies/*`, `src/game/roguelike/waves.*`, `src/game/roguelike/stage_gimmicks.*`, `src/game/autoplay.c`, `tests/test_acts_enemies.c`, `tests/test_determinism_run.c`, `tests/bench_game.c`）を編集しない（`damage.c` / `waves.c` 側の P2 修飾子参照は P5-Pre で先行配線済み）。
- P5-C の所有ファイル（`src/render/*`, `src/audio/*`, `src/app/*`, `README.txt` 等）を編集しない。
- `raylib.h` の include、60Hz ループ内の `malloc`/`free`、可変 `static` 変数、`rand()`/`time()` の使用は厳禁。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/game/player/player_skills.h` / `player_skills.c` | 更新 | `UPG_DOUBLE_GALE` Lv3 の 2 チャージ制（`gale_charges`/`gale_max_charges` 回復・消費）、`UPG_STORM_BRUSH` Lv3 の墨昇華中風筆雷注入（`twin_brush` 対応）、`UPG_INKSTONE_BLESSING` Lv3 の回復待ち時間 0、`RELIC_ONI_TILE` 適用時の `game_effective_mass_tier` 維持（§2.1 / §2.2） |
| `src/game/elements/elements.h` / `elements.c` | 更新 | `RELIC_SEAL_OF_TRINITY`（`cataclysm_thresh` / `cataclysm_cd_base` 参照）、`RELIC_ONI_TILE` の爆発吹き飛び（`game_effective_mass_tier`）、P5-Pre 先行配線（`DMG_REACTION`・`game_enemy_is_targetable`・`game_is_boss_part`・未偏向追尾符ガード）の維持（§2.2） |
| `src/game/roguelike/upgrades.h` / `upgrades.c` | 更新 | 新規強化札 4 種（全 28 種）の修飾子＆フック本実装、`UPG_MAELSTROM_BIND` の `game_effective_mass_tier` 対応、`roguelike_recompute_modifiers` での `ink_max` 下限（`30.0f`）・`gale_max_charges` 増分反映（§2.1 / §2.2） |
| `src/game/roguelike/relics.h` / `relics.c` | 更新 | 墨庵解放の大遺物 6 種（全 15 種）の修飾子・代償・`on_reaction` / `on_tick` フック本実装（§2.2） |
| `src/game/roguelike/rewards.h` / `rewards.c` | 更新 | 解放マスク適用・候補枯渇 2 択フォールバック（通常 +1/+10、ボス +3＆超過 1 HP につき墨晶 +15 / +30）、`rewards_repair_shrine` と `rewards_pick_endless_crystal` の本実装（§2.3） |
| `src/game/roguelike/hints.h` / `hints.c` | 必要時更新 | 既存 8 ヒント発火の整合維持 |
| `tests/test_elements.c` | 更新 | `RELIC_SEAL_OF_TRINITY`（閾値 0.22・CD 1.2s）・`RELIC_INKSTORM_SECRET`（墨昇華中全 4 反応 ×1.5）・`RELIC_THUNDERFIRE_GEM`（爆雷墨玉ドロップ 1.5s CD）の単体テスト追加（§3.1） |
| `tests/test_roguelike_meta.c` | 更新 | 新 4 札・解放 6 遺物・代償・解放マスク制御・候補枯渇 2 択（通常＆ボス超過変換）・`game_run_result` の `bonus_crystals`/`crystal_bonus_x15` 精算テスト追加（§3.2） |

---

## 2. 実装仕様

### 2.1 残り強化札 4 種（全 28 種完成、`game-design.md` §9.4）

各 Lv の数値は**その Lv 時点の合計効果**である。

| ID | 軸 | 札名 | Lv1 / Lv2 / Lv3 の効果と実装仕様 |
| :--- | :---: | :--- | :--- |
| `UPG_LINGERING_EMBERS` | 紅蓮 | 残り火 | Pass 1 で `mods.fire_damp_override = 0.945f / 0.955f / 0.965f`（既定 `0.935f` を置換）。`roguelike_recompute_modifiers` 末尾の既存同期により `g->fluid.cfg.dye_damp[1]` へ即座に反映される |
| `UPG_STORM_BRUSH` | 雷霆 | 雷雲の筆 | ・Lv1 以上：`mods.chain_storm_interval = 2`（既定 `3`。Pre で `damage.c` に配線済み：チェイン `2, 4, 6, ...` で連殺の雷が発火）<br>・Lv2 以上：`mods.chain_storm_mul = 1.5f`（既定 `1.0f`。Pre で `damage.c` に配線済み：連殺の雷の注入量 `1.15f * 1.5f = 1.725f`）<br>・Lv3：`mods.surge_stroke_storm = true`。`player_skills.c` の風筆処理で `g->player.surge_t > 0.0f && !dry` のとき、各 splat 点の `dye[2]`（雷）に `ink_clampf(0.18f + mdist * 0.008f, 0.18f, 0.45f) / (float)steps * 2.1f` を同時注入する（`mods->twin_brush` 併用時は点対称側にも同じ `dye` で注入） |
| `UPG_DOUBLE_GALE` | 烈風 | 二段風斬り | ・Lv1：`mods.gale_cd = 0.85f`<br>・Lv2：`mods.gale_cd = 0.70f`<br>・Lv3：`mods.gale_cd = 0.70f`, `mods.gale_max_charges = 2`<br>・**2 チャージ制御（`player_skills.c`＋`upgrades.c`）**:<br>　1. `roguelike_recompute_modifiers` で `old_max = g->player.gale_max_charges;` とし、`g->player.gale_max_charges = mods.gale_max_charges;` を更新。上限が増えた場合は `g->player.gale_charges += (mods.gale_max_charges - old_max)`（`max` 上限）とし、Lv3 取得直後から 2 回分使えるようにする。<br>　2. `player_skills_step` で `g->player.gale_charges < g->player.gale_max_charges` の間 `gale_cd` を減算し、`gale_cd <= 1e-4f` になったら `++g->player.gale_charges;` とする。なお `gale_charges < gale_max_charges` なら `gale_cd = mods->gale_cd;` で次のチャージ補充を開始し、満タンなら `gale_cd = 0.0f;` とする。<br>　3. `cast_gale` の発動条件を `g->player.gale_charges <= 0 || g->player.ink < cost` とする。発動時は `--g->player.gale_charges;` とし、`if (g->player.gale_cd <= 0.0f) { g->player.gale_cd = mods->gale_cd; }` として補充タイマーを動かす（`gale_max_charges == 1` のときは P1 と完全同一挙動） |
| `UPG_INKSTONE_BLESSING` | 墨道 | 硯の恵み | Pass 2 で `mods.ink_regen_mul *= 1.20f / 1.40f / 1.60f`。Lv3 は `mods.ink_regen_no_delay = true` とし、`player_skills.c` の `regen_ink_idle` で `(g->internal.mods.ink_regen_no_delay || g->player.idle_t > 0.32f) ? 26.0f : 15.0f` として筆を離した直後から高速回復に入る |

---

### 2.2 墨庵解放の大遺物 6 種（全 15 種完成、`game-design.md` §9.5）

`src/game/roguelike/relics.c` にスロット 0〜5 に対応する 6 種を本実装する。

| ID（解放枠） | 大遺物名 | 効果の実装 | 代償の実装 |
| :--- | :--- | :--- | :--- |
| `RELIC_SEAL_OF_TRINITY`（枠 0） | 三元の印 | Pass 1：`mods.cataclysm_thresh = 0.22f`（既定 `0.30f`）、`mods.cataclysm_cd_base = 1.2f`（既定 `2.0f`）。`elements.c` の `elements_stage2_cataclysm` でセル判定に `g->internal.mods.cataclysm_thresh`、発動後 CD に `g->internal.mods.cataclysm_cd_base` を使う | なし |
| `RELIC_INKSTORM_SECRET`（枠 1） | 墨嵐の極意 | Pass 2：`mods.surge_reaction_dmg_mul *= 1.5f`（Pre で `damage.c` に配線済み：墨昇華中 `g->player.surge_t > 0.0f` かつ `(dmg_flags & DMG_REACTION) != 0` の全 4 反応ダメージが `×1.5`） | Pass 1：`if (mods.surge_min_chain < 3) { mods.surge_min_chain = 3; }`（Pre で `damage.c` に配線済み：チェイン 2 では墨昇華が発動せず、チェイン 3 から発動） |
| `RELIC_THUNDERFIRE_GEM`（枠 2） | 雷火の宝珠 | Pass 1：`mods.thunderfire_gem = true`。<br>・`on_tick`：`g->internal.thunderfire_gem_cd` を減算（`<= 1e-4f` で `0.0f`）。<br>・`on_reaction`：`a->kind == REACT_THUNDERBURST && g->internal.thunderfire_gem_cd <= 0.0f` のとき、`(a->x, a->y)` に `combat_spawn_orb(g, a->x, a->y)` で墨玉を 1 個落とし、`g->internal.thunderfire_gem_cd = 1.5f` をセットする（1 ティックに最大 3 ブロック爆雷が同時発火しても 1.5 秒に 1 個までに制限） | なし |
| `RELIC_ONI_TILE`（枠 3） | 鬼瓦 | Pass 1：`mods.oni_tile = true`。`game_effective_mass_tier(g, raw)` が `MASS_HEAVY → MASS_MEDIUM` を返す。次を連動させる：<br>・`damage.c`（Pre 先行配線済み）：実効 `MASS_MEDIUM`（元の重量級含む）の叩きつけを即撃破にする（停止中の鵺 `ENEMY_BOSS_NUE` は従来どおり 60 ダメージ維持）。<br>・`player_skills.c`（Pre 先行配線済み）：風斬りの衝撃倍率を実効質量（`0.6f`）で計算。<br>・`elements.c`：`mass_explosion_multiplier` で `game_effective_mass_tier(g, vel->mass_tier)` を使用（重量級も `0.75f` で吹き飛ぶ）。<br>・`upgrades.c`：`UPG_MAELSTROM_BIND` Lv2+ の渦縛り対象判定に `game_effective_mass_tier(g, vel->mass_tier)` を使用（重量級も Lv2+ の渦縛り対象になる） | なし |
| `RELIC_ENDLESS_BRUSH`（枠 4） | 無尽の筆 | Pass 2：`mods.ink_regen_mul *= 2.5f`（硯の自然回復 2.5 倍） | Pass 1：`mods.ink_max_add -= 40.0f`（`roguelike_recompute_modifiers` で `g->player.ink_max = fmaxf(30.0f, 100.0f + meta_ink + mods.ink_max_add)` とし、現在 `ink` が新 `ink_max` を超えていればクランプ＋`dry` 更新） |
| `RELIC_NIGHT_PARADE_SCROLL`（枠 5） | 百鬼夜行の巻物 | ・Pass 1：`mods.night_parade_scroll = true; mods.bonus_kill_orbs += 1;`（Pre で `damage.c` に配線済み：敵撃破時の墨玉ドロップ `+1`）<br>・精算：`game_run_result`（Pre で `game_contract.c` に配線済み）が `res.crystal_bonus_x15 = game_has_relic(g, RELIC_NIGHT_PARADE_SCROLL);` を立て、獲得墨晶 `×1.5`（切り捨て）を適用 | Pass 1：`mods.max_alive_add += 2;`、Pass 2：`mods.spawn_interval_mul *= 0.75f;`（Pre で `waves.c` に配線済み：増援間隔 `×0.75`、同時上限 `+2`） |

---

### 2.3 解放マスク制御と候補枯渇 2 択フォールバック（`src/game/roguelike/rewards.c`）

`architecture.md` §6.2 と `game-design.md` §3.6 / §9.2 に従い、以下を厳密に実装する：

1. **強化札 28 種抽選（`rewards_generate_offer`）**:
   - 未 Lv3 の強化札が 1 件以上あれば従来どおり最大 3 枚提示（`offer->kind = REWARD_KIND_UPGRADE`, `offer->repair_available = (sh->hp < sh->max_hp)`, `offer->repair_amount = 1`, `offer->crystal_reward = 0`）。
   - **候補 0 件（全 28 札が Lv3）**:
     - `offer->kind = REWARD_KIND_FALLBACK`、`offer->choice_count = 0`、`offer->repair_available = true`（HP 満タン時も選択可とし、修繕選択時は上限維持のまま次ウェーブへ進行してスタックさせない）、`offer->repair_amount = 1`、`offer->crystal_reward = 10`、`++offer->serial`。
2. **大遺物 15 種抽選（`rewards_generate_relic_offer`）**:
   - 候補プールは `game_is_relic_unlocked(g, id) && !game_has_relic(g, id)` を満たす `RelicId` のみとする（未解放の墨庵遺物は絶対に出現しない）。
   - 候補 1 件以上：`offer->kind = REWARD_KIND_RELIC`、`offer->repair_available = false`、`offer->repair_amount = 0`、`offer->crystal_reward = 0`、重複なしで最大 3 個提示、`++offer->serial`。
   - **候補 0 件（解放済み大遺物をすべて所持）**:
     - `offer->kind = REWARD_KIND_FALLBACK`、`offer->choice_count = 0`、`offer->repair_available = true`、`offer->repair_amount = 3`、`offer->crystal_reward = 30`、`++offer->serial`。
3. **社を修繕（`rewards_repair_shrine`）**:
   - 有効条件：`g->phase == RUN_PHASE_REWARD && g->run.reward.repair_available`。
   - 回復量：`const int32_t amount = g->run.reward.repair_amount > 0 ? g->run.reward.repair_amount : 1;`
   - **ボス枯渇代替時の超過 HP → 墨晶変換（`architecture.md` §6.2）**:
     - `g->shrine_count > 0` において `missing = sh->max_hp - sh->hp`（下限 0）、`healed = amount < missing ? amount : missing`、`overflow = amount - healed`、`sh->hp += healed;`。
     - `g->internal.wave_clear_is_boss && g->run.reward.kind == REWARD_KIND_FALLBACK && overflow > 0` のとき、`overflow * 15` を `g->run.bonus_crystals` へ飽和加算する（上限 `META_MAX_CRYSTALS`）。
   - `EV_REWARD_PICK`（`value = -1`, `flags = EVF_REWARD_REPAIR`）を発行し、`wave_clear_is_boss` なら `clear_reward_and_advance_act(g)`、通常ウェーブなら `reward` をクリアして `waves_start_wave(g, g->run.wave_index + 1)` へ進む。
4. **墨晶代替選択（`rewards_pick_endless_crystal`）**:
   - 有効条件：`g->phase == RUN_PHASE_REWARD && g->run.reward.kind == REWARD_KIND_FALLBACK && g->run.reward.crystal_reward > 0`（それ以外は純粋 no-op で `false` を返す）。
   - `const int32_t gain = g->run.reward.crystal_reward;` を `g->run.bonus_crystals` へ飽和加算（上限 `META_MAX_CRYSTALS`）する。
   - `EV_REWARD_PICK`（`value = gain`, `flags = EVF_REWARD_CRYSTAL`）を発行する。
   - `wave_clear_is_boss` なら `clear_reward_and_advance_act(g)`、通常ウェーブなら `reward` をクリアして `waves_start_wave(g, g->run.wave_index + 1)` へ進み `true` を返す。

---

## 3. テスト要件

### 3.1 `tests/test_elements.c`（追記）
既存の全ケースを維持したまま、以下を追加する：
1. **三元の印（`RELIC_SEAL_OF_TRINITY`）**: `ink = fire = storm = 0.25f` を 8 セル置いたとき、未所持では `EV_CATACLYSM` が不発だが、`RELIC_SEAL_OF_TRINITY` 所持時は発火し、全体 CD が `1.2s`（72 ティック）後に再発火可能になること。
2. **墨嵐の極意（`RELIC_INKSTORM_SECRET`）**:
   - チェイン 2 撃破では `g->player.surge_t == 0.0f`（墨昇華不発）、チェイン 3 撃破で初めて `g->player.surge_t > 0.0f` になること。
   - 墨昇華中（`surge_t > 0.0f`）に紅蓮墨嵐の炎上 DoT（`10 → 15`）・雷墨連鎖初撃（`10 → 15`）・爆雷（`30 → 45`）・三元共鳴本体（`80 → 120`）の 4 反応すべてが `×1.5` になり、通常の偏向矢ダメージ（`DMG_REACTION` なし）は `×1.5` にならないこと。
3. **雷火の宝珠（`RELIC_THUNDERFIRE_GEM`）**: 爆雷発火時に墨玉エンティティが 1 個生成され、同一ティックに複数ブロック爆雷が起きても、および `1.5s`（90 ティック）未満の再爆雷でも追加生成されないこと。`1.5s` 経過後の再爆雷で再び 1 個生成されること。

### 3.2 `tests/test_roguelike_meta.c`（追記）
既存の全ケースを維持したまま、以下を追加する：
1. **新規強化札 4 種（全 28 種完成）**:
   - `UPG_LINGERING_EMBERS`：Lv1/2/3 で `fluid.cfg.dye_damp[1]` が `0.945 / 0.955 / 0.965` になること。
   - `UPG_STORM_BRUSH`：Lv1 でチェイン 2 撃破時に連殺の雷が注入され、Lv2 で雷量が `×1.5`、Lv3 で墨昇華中の非かすれ風筆が雷チャンネルへ注入すること（非昇華中・かすれ中は注入しない）。
   - `UPG_DOUBLE_GALE`：Lv1 で `gale_cd == 0.85f`、Lv2 で `0.70f`、Lv3 で `gale_max_charges == 2` になり 2 連続で風斬りを撃てること、2 回目使用後に 0.70 秒ごとに 1 チャージずつ補充されること。
   - `UPG_INKSTONE_BLESSING`：Lv1/2/3 で自然回復が `+20% / +40% / +60%` になり、Lv3 で `idle_t == 0` の直後ティックから毎秒 `26 * 1.6` 速度で回復すること。
2. **墨庵解放遺物 6 種と解放マスク制御**:
   - `RELIC_ONI_TILE`：石鬼（`MASS_HEAVY`）が風斬りで `MASS_MEDIUM` 倍率（`0.6f`）で吹き飛び、中量敵（`ENEMY_KAMAITACHI`）と石鬼が叩きつけで即撃破になること（停止中の鵺は 60 ダメージ維持）。
   - `RELIC_ENDLESS_BRUSH`：`ink_regen_mul == 2.5f`、`ink_max == 60.0f`（初期 100 - 40、最小下限 30.0f クランプ確認）。
   - `RELIC_NIGHT_PARADE_SCROLL`：撃破時の墨玉が `+1`、通常ウェーブの増援間隔が `×0.75`・同時上限が `+2`、`game_run_result` の `crystal_bonus_x15 == true` で精算墨晶が `×1.5`（切り捨て）になること。
   - 解放マスク制御：`unlocked_relic_mask == 0` のとき初期 9 種を全所持すると即 `REWARD_KIND_FALLBACK` になり、スロット 0 (`0x1`) を解放すると `RELIC_SEAL_OF_TRINITY` のみが抽選されること。
3. **候補枯渇 2 択フォールバック＆超過 HP 変換**:
   - 全 28 札を Lv3 にした状態で `rewards_generate_offer` → `REWARD_KIND_FALLBACK`（`repair_amount == 1, crystal_reward == 10`）。`CMD_PICK_ENDLESS_CRYSTAL` で `bonus_crystals == 10`、`EV_REWARD_PICK`（`EVF_REWARD_CRYSTAL`）、次ウェーブ開始を確認。
   - 全解放済み大遺物を所持した状態で `rewards_generate_relic_offer` → `REWARD_KIND_FALLBACK`（`repair_amount == 3, crystal_reward == 30`）。社 HP が `max_hp - 1` のとき `CMD_REPAIR_SHRINE` で社 HP `+1`（満タン）かつ超過 2 HP 分が `bonus_crystals += 30` に変換され、`ACT_TRANSITION`（`act_index + 1`）へ進むこと。別インスタンスで `CMD_PICK_ENDLESS_CRYSTAL` を選ぶと `bonus_crystals += 30` で次幕へ進み、`meta_settle_run` の `from_endless` に反映されること。
   - 非フォールバック状態での `CMD_PICK_ENDLESS_CRYSTAL` が純粋 no-op であること。

---

## 4. ローカル検証・PR と完了条件

```sh
# 1. ASan/UBSan: 変更範囲のビルドと関連テスト
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan --target ink_game inkstorm \
        test_elements test_roguelike_meta test_core_smoke &&
    nix develop -c ctest --preset asan --output-on-failure \
        -R '^(test_elements|test_roguelike_meta|test_core_smoke|test_cli_headless_smoke)$'

# 2. 境界と排他所有の確認（いずれも 0 件・空）
grep -rn 'raylib.h\|rand(\|time(' src/game
git diff --stat main -- CMakeLists.txt CMakePresets.json src/core src/fluid src/ecs \
    src/game/meta.h src/game/meta.c src/game/game.h src/game/game_ids.h src/game/game_events.h \
    src/game/game_components.h src/game/game_contract.c src/game/game_internal.h \
    src/game/game.c src/game/hash.c src/game/debug_api.c src/game/combat src/game/enemies \
    src/game/roguelike/waves.c src/game/roguelike/waves.h src/game/roguelike/stage_gimmicks.c \
    src/game/roguelike/stage_gimmicks.h src/game/autoplay.c \
    tests/test_core_smoke.c tests/test_fluid.c tests/bench_fluid.c tests/test_ecs.c \
    tests/test_app_save.c tests/test_app_core.c tests/test_audio.c tests/test_text_table.c \
    tests/test_acts_enemies.c tests/test_determinism_run.c tests/bench_game.c \
    src/render src/audio src/app assets tools .github flake.nix flake.lock .agents docs \
    README.txt AGENTS.md
```

**完了条件（Definition of Done）**
- [ ] §2 の仕様（残り 4 札・解放 6 遺物・解放マスク・候補枯渇 2 択と超過 HP 変換）をすべて満たし、凍結ファイルおよび P5-B / P5-C の担当ファイルに差分がない。
- [ ] `test_elements`、`test_roguelike_meta`、`test_core_smoke`、`test_cli_headless_smoke` が ASan/UBSan（`-Werror`）で全通過する。
- [ ] PR 作成直後に `@coderabbitai review` を投稿し、全指摘を 4 分類トリアージして必要な修正と返信を完了した（[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md) 準拠）。
- [ ] PR の CI 3 ジョブ（`build-asan-and-test`, `static-analysis`, `dist-verify`）がすべて緑。
