# Phase 5-Pre タスク指示書：P2 拡張の共通契約（追加 ID・イベント・Meta/Settings・RyuBodyState・骨格・先行配線）

> **必読**: [PLAN.md](../PLAN.md) §9（特に §9.1 の Phase 5 行と §9.2）、[architecture.md](../architecture.md) §1〜§9、[game-design.md](../game-design.md) §3.5 / §3.6 / §8.1〜§8.3 / §8.7 / §9.4 / §9.5 / §10.1 / §10.2 / §11.2 / §12、[art-direction.md](../art-direction.md) §6 / §10.1 / §10.3 / §10.5 / §10.6 / §11、[qa-and-testing.md](../qa-and-testing.md) §2〜§5、[phase4-pre-contracts.md](phase4-pre-contracts.md)、[phase4-integration.md](phase4-integration.md)、[AGENTS.md](../../AGENTS.md)、[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)
>
> **前提**: Phase 4（P4-Pre / P4-A / P4-B / P4-C / P4-Int）が `main` にマージ済みであり、Phase 4 Gate（全 3 幕＆墨庵通し完走・CI 全緑）を通過していること。着手時に `main` が最新であることを確かめる。
> **作業場所 / ブランチ**: リポジトリルート（`full-scale-dev/Ink-storm`、作業ツリーがクリーンな状態）で `main` から `feat/phase5-pre-contracts` を作り、**1 本の PR** にする。
> **後続**: P5-A（[phase5-roguelike-p2.md](phase5-roguelike-p2.md)）、P5-B（[phase5-endless-ryu-p2.md](phase5-endless-ryu-p2.md)）、P5-C（[phase5-ui-settings-p2.md](phase5-ui-settings-p2.md)）、P5-Int（[phase5-integration.md](phase5-integration.md)）

---

## 0. 目的と禁止事項

Phase 5（P2 拡張）では、描画・UI ファイルの競合を完全にゼロにするため、Phase 4 と同じ **3 並列 worktree 構成** を採る（`docs/PLAN.md` §9.1 の Phase 5 行を §4 で 3 並列へ同期する）。
- **P5-A**（`wt-roguelike-p2`）：残り強化札 4 種（全 28 種完成）、墨庵解放の大遺物 6 種（全 15 種完成・解放マスク制御・代償と 7 フック）、候補枯渇 2 択フォールバック（修繕＋墨晶変換）
- **P5-B**（`wt-endless-ryu-p2`）：狐符（`ENEMY_OFUDA_FOX`）＋護符結界＋追尾符（`PROJ_HOMING_OFUDA`）と第 3 幕編成復帰、エンドレスモード（第 4 幕以降周回スケール・地形/ボス循環）、一筆龍の多部位 ECS 版（3 龍玉子エンティティ＋頭部軌跡リングバッファ＋3 フェーズ）
- **P5-C**（`wt-ui-settings-p2`）：設定画面拡充（言語・閃光軽減・カーソル倍率・初回ヒント初期化・設定初期化）、エンドレス UI（遅延勝利精算・勝利画面「エンドレスへ進む」・金砂子・フォールバック 2 択 UI・第 4 幕以降 HUD/幕題字）、狐符・結界・追尾符・多部位一筆龍の水墨描画と SE・見本帳拡充・`README.txt` 更新

本タスク（P5-Pre）では、**3 並列間で 1 ファイルも重複せず、かつマージ順を問わず各ブランチ単独で CI 3 ジョブが全緑になる凍結契約・骨格・クロス所有先行配線** を `main` に確定させる。

1. **境界ヘッダと共有パイプラインの凍結**:
   - `meta.h` / `meta.c`（後方互換 `version=1` のまま `SettingsState` へ `ui_lang`・`reduce_flash`・`cursor_scale` を追加、`META_SERIALIZE_CAP` を `640` へ拡張、`MetaRunResult.bonus_crystals` 追加）
   - `game_ids.h`（`ENEMY_OFUDA_FOX`・`ENEMY_BOSS_RYU_ORB`、`PROJ_HOMING_OFUDA`、強化札 28 種、大遺物 15 種、`DMG_REACTION`）
   - `game_events.h`（`EV_BOSS_PART_BREAK`、`EVF_REWARD_CRYSTAL` とペイロード契約確定）
   - `game.h` / `game_contract.c` / `debug_api.c`（`PlayerState.gale_charges`/`gale_max_charges`、`RewardOffer.repair_amount`/`crystal_reward`、`RunProgress.endless_loop`/`endless_acts_cleared`/`bonus_crystals`、公開読み取り専用 `RyuBodyState`、公開ヘルパ 5 関数、`k_upgrade_meta[28]`、`game_run_result` のエンドレス・百鬼夜行対応、`game_debug_start_act` のエンドレス幕許可、`game_debug_force_wave_clear` の多部位対応）
   - `game_internal.h` / `hash.c`（`RunModifiers` 拡張 14 フィールドと `GameInternal.thunderfire_gem_cd` を確定し、全フィールドおよび `GameState.ryu_body` をハッシュへ組み込み）
   - `game.c`（`CMD_PICK_ENDLESS_CRYSTAL` / `CMD_START_ENDLESS` 配線、エンドレス幕クリア加算・社回復、最終幕/上限到達の `VICTORY` 判定）
   - `ui_view.h`（`UI_HOVER_CRYSTAL`、勝利精算プレビュー・閃光軽減・カーソル倍率・設定通知フラグ）
2. **新規翻訳単位の骨格とクロス所有先行配線**:
   - 新規翻訳単位 `src/game/enemies/ofuda_fox.c` の stub を追加する（`CMakeLists.txt` の `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` が自動検出するため、並列期間中は **全ブランチとも `CMakeLists.txt` を編集しない**。新規 CTest ターゲット追加も不要で、既存 15 件の CTest を各並列ブランチで排他担当する）。
   - **並列単独通過のための先行配線**:
     - P5-B 所有の `damage.c` / `waves.c` / `projectiles.c` / `enemy_defs.c` に、P5-A の新規 `RunModifiers`（`surge_reaction_dmg_mul`, `surge_min_chain`, `chain_storm_interval`, `chain_storm_mul`, `bonus_kill_orbs`, `oni_tile`, `spawn_interval_mul`, `max_alive_add`）・`game_effective_mass_tier`・`game_enemy_is_targetable`・`game_is_boss_part`・`StatusComp.barrier_t` 軽減を先行配線する（既定値では P1 と 1 ビットも変わらない）。
     - P5-A 所有の `elements.c` / `rewards.h` / `rewards.c` に、`DMG_REACTION` 付与、`game_enemy_is_targetable`、`game_is_boss_part`（炎上 0.55s）、未偏向 `PROJ_HOMING_OFUDA` の炎上弾昇格抑止、`rewards_pick_endless_crystal` 骨格、`game_is_relic_unlocked` フィルタ、`clear_reward_and_advance_act` の `endless_loop` 同期を先行配線する。
     - P5-C 所有の `text_table.h` / `text_table.c` に、`TEXT_P2_STUB` 暫定プレースホルダを返し、`test_text_table` が Pre / P5-A / P5-B の CI で緑になる状態を保つ。

### 0.1 Phase 5 の進め方

| 段階 | 担当 | ブランチ | PR とマージ |
| :--- | :--- | :--- | :--- |
| 1 | P5-Pre（本書） | `feat/phase5-pre-contracts` | 1 本。`@coderabbitai review`・CI 3 ジョブ全緑後にマージし、下の 3 つの worktree を切る |
| 2a | P5-A | `full-scale-dev/wt-roguelike-p2` / `feat/phase5-roguelike-p2` | 完了したら単独で PR → `@coderabbitai review`・CI 3 ジョブ全緑 → マージ |
| 2b | P5-B | `full-scale-dev/wt-endless-ryu-p2` / `feat/phase5-endless-ryu-p2` | 完了したら単独で PR → `@coderabbitai review`・CI 3 ジョブ全緑 → マージ（2a/2c と順不同） |
| 2c | P5-C | `full-scale-dev/wt-ui-settings-p2` / `feat/phase5-ui-settings-p2` | 完了したら単独で PR → `@coderabbitai review`・CI 3 ジョブ全緑 → マージ（2a/2b と順不同） |
| 3 | P5-Int | 2a・2b・2c マージ後の `main` から `feat/phase5-integration` | 3 並列の結合修正・実機通し完走・Phase 5 Gate を 1 本の PR に |

- 各並列ブランチは PR 作成前に最新の `main` を取り込み（`git merge main`）、先行マージされた他ブランチがあればその状態で対象テストとスモークを再実行する。

**禁止事項**
- P5-A / P5-B / P5-C の本実装（新 4 札・新 6 遺物の効果本体、狐符 AI と追尾符旋回・焼失、エンドレス周回スケール、一筆龍の多部位 3 フェーズ、設定画面拡充 UI、勝利画面 3 ボタン・遅延精算、新敵/多部位龍の本描画・SE）を本タスクで先回り実装しない。
- Phase 2〜4 で凍結済みの下位 API（`core.h`, `fluid.h`, `ecs.h`）のシグネチャを変えない。
- `MetaSettleBreakdown` の既存 7 フィールド構成を変えない（既存テストの位置初期化互換を維持）。
- `core` / `fluid` / `ecs` / `game` / `audio` に `raylib.h` を include しない。

---

## 1. ファイルと所有

### 1.1 本タスク（P5-Pre）で変更・新規作成するファイル

| ファイル | 操作 | 内容 | Pre 後の扱い |
| :--- | :---: | :--- | :--- |
| `src/game/meta.h` / `meta.c` | 更新 | `META_SERIALIZE_CAP 640`、`UiLangMode`、`SettingsState` 拡張 3 項目＋`version=1` 後方互換、`MetaRunResult.bonus_crystals`、`meta_calc_crystals` 反映（§2.1） | **凍結** |
| `src/game/game_ids.h` | 更新 | `ENEMY_OFUDA_FOX`・`ENEMY_BOSS_RYU_ORB`、`PROJ_HOMING_OFUDA`、28 札、15 遺物、`DMG_REACTION`（§2.2） | **凍結** |
| `src/game/game_events.h` | 更新 | `EV_BOSS_PART_BREAK`、`EVF_REWARD_CRYSTAL`、Phase 5 イベントペイロード契約確定（§2.3） | **凍結** |
| `src/game/game.h` / `game_contract.c` / `debug_api.c` | 更新 | `PlayerState`・`RewardOffer`・`RunProgress` 拡張、`RyuBodyState`、公開ヘルパ 5 関数、`k_upgrade_meta[28]`、`game_run_result`、`game_debug_start_act`・`game_debug_force_wave_clear` 更新（§2.4） | **凍結** |
| `src/game/game_internal.h` / `hash.c` | 更新 | `RunModifiers` 14 フィールドと `thunderfire_gem_cd` 追加、`hash_run`（`ryu_body` 含む）・`hash_internal` 全同期（§2.5） | **凍結** |
| `src/game/game.c` | 更新 | `ryu_body`/`gale_charges`/`endless_loop` 初期化、`CMD_PICK_ENDLESS_CRYSTAL` / `CMD_START_ENDLESS`、段 8 ボス撃破＆`step_aftermath` エンドレス対応（§2.6） | **凍結** |
| `src/render/ui_view.h` | 更新 | `UI_HOVER_CRYSTAL`、`settle_preview`、`reduce_flash`、`cursor_scale`、`settings_notice_t`、`settings_notice_kind` 追加（§2.7） | **凍結** |
| `src/game/roguelike/upgrades.c`、`relics.c`、`rewards.h` / `rewards.c`、`elements/elements.c`、`player/player_skills.c` | 更新 | 新 4 札・新 6 遺物の枠と P1 互換既定値、`DMG_REACTION` 付与、`game_enemy_is_targetable`/`game_is_boss_part`/未偏向追尾符ガード、`rewards_pick_endless_crystal` 骨格・解放マスクフィルタ、`gale_charges` 基本同期（§2.8） | P5-A へ移譲 |
| `src/game/enemies/ofuda_fox.c` | 新規 | 狐符の stub 翻訳単位（`ofuda_fox_spawn` / `ofuda_fox_step`）（§2.9） | P5-B へ移譲 |
| `src/game/enemies/enemy_defs.h` / `enemy_defs.c`、`combat/damage.c`、`enemies/projectiles.c`、`roguelike/waves.c` | 更新 | 12 種の `k_enemy_defs` 枠（`ENEMY_OFUDA_FOX`・`ENEMY_BOSS_RYU_ORB` の基礎パラメータ＋stub）、P5-A 用 `RunModifiers` / `game_effective_mass_tier` / `barrier_t` / `game_enemy_is_targetable` / `game_is_boss_part` 先行配線（§2.9） | P5-B へ移譲 |
| `src/render/text_table.h` / `text_table.c` | 更新 | `TEXT_P2_STUB` 暫定プレースホルダと `text_upgrade_name`/`text_upgrade_effect` の範囲ガード（§2.10） | P5-C へ移譲 |
| `tests/test_core_smoke.c`、`tests/test_app_save.c` | 更新 | Phase 5 凍結契約（28 札・15 遺物・解放マスク・`RyuBodyState`・`CMD_START_ENDLESS`・新 `SettingsState` 保存往復）のチェック追記（§3.1） | **凍結** |
| `tests/test_roguelike_meta.c` | 最小更新 | `SettingsState` 拡張 3 項目に伴う既定値・ゴールデン文字列・ラウンドトリップ・P1 セーブ後方互換チェック、`test_hook_acquisition_order_and_relic_rewards` の `unlocked_relic_mask` 整合（§3.2） | P5-A へ移譲 |
| `docs/PLAN.md`、`docs/architecture.md`、`docs/game-design.md`、`docs/qa-and-testing.md` | 更新 | Phase 5 の 3 並列構成と凍結契約を同期（§4） | **凍結**（`game-design.md` / `art-direction.md` の数値調整のみ各担当） |

### 1.2 並列期間（P5-A / P5-B / P5-C）の排他所有表

**3 つの並列ブランチ間で編集対象ファイルが 1 つも重ならない**よう、以下のとおり厳密に分割する。

| 区分 | 所有ファイル |
| :--- | :--- |
| **凍結**（並列期間中は編集禁止。変更が必要な場合は §1.3 の契約変更手続きに従う） | `CMakeLists.txt`、`CMakePresets.json`、`src/core/*`、`src/fluid/*`、`src/ecs/*`、`src/game/meta.h`、`src/game/meta.c`、`src/game/game.h`、`src/game/game_ids.h`、`src/game/game_events.h`、`src/game/game_components.h`、`src/game/game_contract.c`、`src/game/game_internal.h`、`src/game/game.c`、`src/game/hash.c`、`src/game/debug_api.c`、`src/render/ui_view.h`、`tests/test_core_smoke.c`、`tests/test_fluid.c`、`tests/bench_fluid.c`、`tests/test_ecs.c`、`tests/test_app_save.c`、`tools/*`、`.github/*`、`flake.*`、`.agents/*`、`AGENTS.md`、`assets/*`、`docs/PLAN.md`、`docs/architecture.md`、`docs/qa-and-testing.md` |
| **P5-A** (`wt-roguelike-p2`) | `src/game/player/*`（`player_skills.c`/`.h`）、`src/game/elements/*`（`elements.c`/`.h`）、`src/game/roguelike/upgrades.c`/`.h`、`src/game/roguelike/relics.c`/`.h`、`src/game/roguelike/rewards.c`/`.h`、`src/game/roguelike/hints.c`/`.h`、`tests/test_elements.c`、`tests/test_roguelike_meta.c` |
| **P5-B** (`wt-endless-ryu-p2`) | `src/game/combat/*`（`damage.c`/`.h`）、`src/game/enemies/*`（`enemy_defs.c`/`.h`、`ofuda_fox.c`、`archer.c`、`lantern.c`、`umbrella.c`、`kamaitachi.c`、`stone_oni.c`、`ink_leech.c`、`boss_tengu.c`、`boss_nue.c`、`boss_ryu.c`、`projectiles.c`/`.h`）、`src/game/roguelike/waves.c`/`.h`、`src/game/roguelike/stage_gimmicks.c`/`.h`、`src/game/autoplay.c`、`tests/test_acts_enemies.c`、`tests/test_determinism_run.c`、`tests/bench_game.c`、`docs/game-design.md`（数値調整のみ） |
| **P5-C** (`wt-ui-settings-p2`) | `src/render/*`（`ui_view.h` を除く全ファイル）、`src/audio/*`、`src/app/*`、`README.txt`、`tests/test_app_core.c`、`tests/test_audio.c`、`tests/test_text_table.c`、`docs/art-direction.md`（数値調整のみ） |

### 1.3 並列期間中の契約変更手続き
- 並列作業中に凍結ファイルの変更が不可避となった場合、当該ブランチで直接書き換えず、PR 本文または作業メモに「変更理由・影響ファイル・最小差分案」を記載して統合担当へエスカレーションする。
- 承認された契約変更は小さな単独 PR として先に `main` へマージし、他の並列 worktree が `git merge main` で取り込む。

---

## 2. 契約仕様の詳細

### 2.1 `src/game/meta.h` と `src/game/meta.c`（凍結）

既存 `META_SAVE_VERSION = 1` の後方互換性を保ったまま、設定拡充 3 項目とエンドレス候補枯渇の墨晶加算を統合する。

1. **バッファ容量・列挙型・構造体拡張（`meta.h`）**:
   - `#define META_SERIALIZE_CAP 640`（旧 `512` から拡張。`src/app/save.c` は `META_SERIALIZE_CAP + 64` を参照するため自動追従する）。
   - `SettingsState` の直前に言語モード列挙型を追加し、`SettingsState` の末尾に 3 フィールドを追記する：
     ```c
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
         uint8_t ui_lang;       /* UiLangMode (0=AUTO, 1=JA, 2=EN), 既定 UI_LANG_AUTO (0) */
         bool reduce_flash;     /* 画面全体の明滅抑止, 既定 false */
         uint8_t cursor_scale;  /* 100 / 125 / 150, 既定 100 */
     } SettingsState;
     ```
   - `MetaRunResult` の末尾に `int32_t bonus_crystals;`（エンドレス候補枯渇の「墨晶 +10 / +30」およびボス修繕 +3 超過変換の累積。既定 0）を追記する。
   - **`MetaSettleBreakdown` の 7 フィールド配置は一切変更しない**（`from_acts`, `from_kills`, `from_chain`, `from_reactions`, `from_endless`, `from_bonus`, `total_crystals`）。
2. **`meta.c` の挙動**:
   - `meta_settings_default()`: `.ui_lang = (uint8_t)UI_LANG_AUTO, .reduce_flash = false, .cursor_scale = 100` を既定値に含める。
   - `meta_calc_crystals(res)`:
     ```c
     const int64_t endless_acts = clamp_count(res->endless_acts_cleared, META_MAX_ENDLESS_ACTS);
     const int64_t bonus_crys = clamp_count(res->bonus_crystals, META_MAX_CRYSTALS);
     /* from_endless にエンドレス幕報酬とラン内墨晶代替を合算（飽和上限 META_MAX_CRYSTALS） */
     .from_endless = clamp_count(endless_acts * 40 + bonus_crys, META_MAX_CRYSTALS),
     ```
     これにより `crystal_bonus_x15`（百鬼夜行の巻物）が `base` 全体に対して切り捨て `base / 2` で一貫適用され、既存の `test_crystal_breakdowns` の位置初期化も全件そのまま通過する。
   - `meta_serialize`: 既存 `opt_hints=%d\n` の直後に以下 3 行を出力する：
     ```c
     "opt_lang=%u\n"
     "opt_reduce_flash=%d\n"
     "opt_cursor_scale=%u\n"
     ```
   - `meta_deserialize`: `SaveField.kind` に `CURSOR_SCALE` を追加し、末尾 3 キーを登録する：
     - `opt_lang`: `BYTE`, `maximum = UI_LANG_COUNT - 1`（範囲外は `[0, 2]` へクランプ）
     - `opt_reduce_flash`: `TOGGLE`（`parsed != 0`）
     - `opt_cursor_scale`: `CURSOR_SCALE`（`parsed < 113 ? 100 : (parsed < 138 ? 125 : 150)` へ正規化）
     - 既存の P1 セーブファイル（上記 3 キーを含まない `version=1` テキスト）はエラーにせず、`meta_init_default` の既定値（`UI_LANG_AUTO`, `false`, `100`）で正常ロードする。

---

### 2.2 `src/game/game_ids.h`（凍結）

既存の P0 / P1 列挙値（値と順序）は一切変更せず、各 enum の `*_COUNT` の直前に P2 要素を追記する。

1. **`EnemyDefId`（全 12 種）**:
   ```c
   /* 既存 0..9 の直後に追記 */
   ENEMY_OFUDA_FOX,    /* 10: 3-2 狐符 */
   ENEMY_BOSS_RYU_ORB, /* 11: 一筆龍 多部位版の龍玉子エンティティ */
   ENEMY_DEF_COUNT     /* 12 */
   ```
2. **`ProjKind`（全 6 種）**:
   ```c
   /* 既存 0..4 の直後に追記 */
   PROJ_HOMING_OFUDA,  /* 5: 追尾符（140px/s, 結合 0.6, 毎秒 90°旋回, 炎>0.3で焼失） */
   PROJ_KIND_COUNT     /* 6 */
   ```
3. **`UpgradeId`（全 28 種：各軸 6 種 × 4 軸 ＋ 共通 4 種）**:
   ```c
   /* 既存 0..23 の直後に追記 */
   UPG_LINGERING_EMBERS,   /* 24: 紅蓮 - 残り火 */
   UPG_STORM_BRUSH,        /* 25: 雷霆 - 雷雲の筆 */
   UPG_DOUBLE_GALE,        /* 26: 烈風 - 二段風斬り */
   UPG_INKSTONE_BLESSING,  /* 27: 墨道 - 硯の恵み */
   UPG_COUNT               /* 28 */
   ```
4. **`RelicId`（全 15 種：初期 9 種 ＋ 墨庵解放 6 種）**:
   スロット番号 `0..5`（`meta.h` の `META_RELIC_UNLOCK_COUNT = 6`・`text_shop_relic_name(0..5)`）と厳密に 1 対 1 対応させる：
   ```c
   /* 既存 0..8 の直後に追記 */
   RELIC_SEAL_OF_TRINITY,     /* 9  (解放枠 0): 三元の印 */
   RELIC_INKSTORM_SECRET,     /* 10 (解放枠 1): 墨嵐の極意 */
   RELIC_THUNDERFIRE_GEM,     /* 11 (解放枠 2): 雷火の宝珠 */
   RELIC_ONI_TILE,            /* 12 (解放枠 3): 鬼瓦 */
   RELIC_ENDLESS_BRUSH,       /* 13 (解放枠 4): 無尽の筆 */
   RELIC_NIGHT_PARADE_SCROLL, /* 14 (解放枠 5): 百鬼夜行の巻物 */
   RELIC_COUNT                /* 15 */
   ```
5. **ダメージ分類フラグ追加**:
   ```c
   #define DMG_REACTION (UINT32_C(1) << 7) /* 4 属性反応由来ダメージ（墨嵐の極意の対象） */
   ```

---

### 2.3 `src/game/game_events.h`（凍結）

1. **`GameEventType`**: `EV_SKILL_DENIED` の直後・`EV_TYPE_COUNT` の直前に `EV_BOSS_PART_BREAK` を追加する。
2. **フラグ**: `EVF_REWARD_RELIC` の直後に次を追加する：
   ```c
   #define EVF_REWARD_CRYSTAL (UINT32_C(1) << 14)
   ```
3. **Phase 5 イベントペイロード契約**:

| イベント | `x, y` | `x2, y2` | `value` | `flags` |
| :--- | :--- | :--- | :--- | :--- |
| `EV_BOSS_PART_BREAK` | 破壊された龍玉の座標 (px) | `0, 0` | `part_id`（`0`=炎玉, `1`=雷玉, `2`=風玉） | 致命打の `DMG_*` |
| `EV_REWARD_PICK`（墨晶代替） | `0, 0` | `0, 0` | 獲得した墨晶数（`10` / `30`） | `EVF_REWARD_CRYSTAL` |
| `EV_BOSS_PHASE`（一筆龍多部位版） | 龍の頭部座標 (px) | `0, 0` | `2`（点睛：3 龍玉全破壊時）、`3`（昇龍：龍睛 HP 40% 以下） | `0` |

---

### 2.4 `src/game/game.h`、`src/game/game_contract.c`、`src/game/debug_api.c`（凍結）

1. **公開構造体フィールド追加（`game.h`）**:
   - `PlayerState` の `float gale_cd, gale_cd_total;` の直後に次を追加：
     ```c
     int32_t gale_charges;
     int32_t gale_max_charges;
     ```
   - `RewardOffer` の `bool repair_available;` の直後に次を追加：
     ```c
     int32_t repair_amount;  /* 社修繕時の回復量（通常 1, ボス枯渇代替 3） */
     int32_t crystal_reward; /* REWARD_KIND_FALLBACK の墨晶代替量（通常 10, ボス 30, 非代替 0） */
     ```
   - `RunProgress` の `int32_t acts_cleared;` の直後に次を追加：
     ```c
     int32_t endless_loop;         /* 0=本編(Act 1..3), 1=Act 4..6, ... (= act_index / 3) */
     int32_t endless_acts_cleared; /* エンドレス幕クリア累計（上限 META_MAX_ENDLESS_ACTS） */
     int32_t bonus_crystals;       /* ラン内候補枯渇の墨晶選択・超過修繕変換累計 */
     ```
   - **`RyuBodyState`（多部位一筆龍の公開読み取り状態、`GameState` に追加）**:
     `PendingImpulse` 定義の直後に配置し、`GameState` の `pending_impulse_count` と `internal` の間に `RyuBodyState ryu_body;` を持つ（`ink_render` が `g->internal` に触れずに胴の軌跡と龍玉破壊区間を描画するための公開契約）：
     ```c
     #define GAME_RYU_TRAIL_CAP 128
     #define GAME_RYU_ORB_COUNT 3

     typedef struct {
         EntityId head_id;
         EntityId orb_ids[GAME_RYU_ORB_COUNT]; /* 0=炎玉(0px), 1=雷玉(450px), 2=風玉(900px) */
         uint8_t orb_broken_mask;              /* bit0=炎玉, bit1=雷玉, bit2=風玉 */
         int32_t active_orb_slot;              /* 第1フェーズ攻撃担当(0..2), 第2以降 -1 */
         GameVec2 trail[GAME_RYU_TRAIL_CAP];   /* 頭部軌跡リングバッファ（頭が最新） */
         int32_t trail_head;                   /* 最新サンプルの添字 (0..GAME_RYU_TRAIL_CAP-1) */
         int32_t trail_count;                  /* 有効点数 (0..GAME_RYU_TRAIL_CAP) */
     } RyuBodyState;
     ```
2. **公開クエリ関数（`game.h` 宣言 / `game_contract.c` 実装）**:
   ```c
   bool game_is_boss_part(EnemyDefId id);
   bool game_enemy_is_targetable(const GameState *g, EntityId id);
   int32_t game_relic_unlock_slot(RelicId id);
   bool game_is_relic_unlocked(const GameState *g, RelicId id);
   MassTier game_effective_mass_tier(const GameState *g, MassTier raw);
   ```
   - `game_is_boss_part(id)`: `id == ENEMY_BOSS_RYU_ORB` のとき `true`。
   - `game_enemy_is_targetable(g, id)`: `g != NULL && ecs_is_alive(&g->ecs, id)` かつ `TransformComp` / `EnemyComp` が非 NULL・`!tr->dissolving` であり、さらに `en->def_id == ENEMY_BOSS_RYU` の場合は `BossComp *b = game_boss(g, id);` が `b == NULL || b->phase_idx >= 1 || g->ryu_body.head_id.index != id.index` を満たすこと（P1 簡易版では `ryu_body.head_id` が無効なので従来どおり標的になり、P5-B で多部位版が `ryu_body.head_id = id` を登録した後は第 1 フェーズ中 `phase_idx == 0` の頭部だけ標的・被弾から除外される）。
   - `game_relic_unlock_slot(id)`: `id >= RELIC_SEAL_OF_TRINITY && id < RELIC_COUNT` なら `(int32_t)(id - RELIC_SEAL_OF_TRINITY)`（`0..5`）、初期 9 種および範囲外は `-1`。
   - `game_is_relic_unlocked(g, id)`: `(unsigned)id >= (unsigned)RELIC_COUNT` なら `false`。`slot = game_relic_unlock_slot(id)` が `< 0` なら `true`、`slot >= 0` なら `g != NULL && (g->config.unlocked_relic_mask & (UINT64_C(1) << (unsigned)slot)) != 0`。
   - `game_effective_mass_tier(g, raw)`: `g != NULL && g->internal.mods.oni_tile && raw == MASS_HEAVY` のとき `MASS_MEDIUM`、それ以外は `raw`。
3. **`game_contract.c` の更新**:
   - `k_upgrade_meta[UPG_COUNT]`（28 件）：`UPG_LINGERING_EMBERS`（`AXIS_BLAZE`, 3）、`UPG_STORM_BRUSH`（`AXIS_THUNDER`, 3）、`UPG_DOUBLE_GALE`（`AXIS_GALE`, 3）、`UPG_INKSTONE_BLESSING`（`AXIS_INKWAY`, 3）を登録する。
   - `game_run_result(g)`:
     - `res.endless_acts_cleared = g->run.endless_acts_cleared;`
     - `res.bonus_crystals = g->run.bonus_crystals;`
     - `res.crystal_bonus_x15 = game_has_relic(g, RELIC_NIGHT_PARADE_SCROLL);`
4. **`debug_api.c` の更新**:
   - `game_debug_start_act(g, act_index)`: 許容範囲を `0 <= act_index && act_index < META_MAX_BEST_ACT` に広げ、`g->run.endless_loop = act_index / GAME_MAIN_ACT_COUNT;` を同期する（範囲外 `< 0 || >= META_MAX_BEST_ACT` は no-op）。
   - `game_debug_force_wave_clear(g)`: ボスウェーブで `g->run.boss_id` が生存しているとき、もし `g->ryu_body.head_id.index == g->run.boss_id.index && g->ryu_body.head_id.gen == g->run.boss_id.gen` であれば、生存中の `ryu_body.orb_ids[0..2]` を `ecs_destroy` し、`ryu_body.orb_broken_mask = 0x7u`、`boss->phase_idx = 1`（0 だった場合）にしてから `hp->invuln_t = 0.0f` ＆致命ダメージを与える（P1 簡易版・P2 多部位版の双方で安全にボス撃破できる）。

---

### 2.5 `src/game/game_internal.h` と `src/game/hash.c`（凍結）

1. **`RunModifiers` への追加フィールド（末尾に追加）**:
   ```c
   /* Phase 5 additions (all defaults match P1 behavior identically) */
   int32_t chain_storm_interval; /* 既定 3（雷雲の筆 Lv1+ で 2） */
   float chain_storm_mul;        /* 既定 1.0f（雷雲の筆 Lv2+ で 1.5f） */
   bool surge_stroke_storm;      /* 既定 false（雷雲の筆 Lv3 で true） */
   int32_t gale_max_charges;     /* 既定 1（二段風斬り Lv3 で 2） */
   float cataclysm_thresh;       /* 既定 0.30f（三元の印で 0.22f） */
   float cataclysm_cd_base;      /* 既定 2.0f（三元の印で 1.2f） */
   float surge_reaction_dmg_mul; /* 既定 1.0f（墨嵐の極意で 1.5f） */
   int32_t surge_min_chain;      /* 既定 2（墨嵐の極意で 3） */
   bool thunderfire_gem;         /* 既定 false（雷火の宝珠で true） */
   bool oni_tile;                /* 既定 false（鬼瓦で true） */
   bool night_parade_scroll;     /* 既定 false（百鬼夜行の巻物で true） */
   int32_t bonus_kill_orbs;      /* 既定 0（百鬼夜行の巻物で +1） */
   float spawn_interval_mul;     /* 既定 1.0f（百鬼夜行の巻物で 0.75f） */
   int32_t max_alive_add;        /* 既定 0（百鬼夜行の巻物で +2） */
   ```
2. **`GameInternal` への追加フィールド（末尾に追加）**:
   ```c
   /* Phase 5-A fields */
   float thunderfire_gem_cd;     /* 雷火の宝珠 1.5s CD */
   ```
3. **`hash.c` の同期**:
   - `hash_run`: `PlayerState` に `gale_charges`/`gale_max_charges`、`RewardOffer` に `repair_amount`/`crystal_reward`、`RunProgress` に `endless_loop`/`endless_acts_cleared`/`bonus_crystals`、さらに末尾に `g->ryu_body` の全フィールド（`head_id`, `orb_ids[0..2]`, `orb_broken_mask`, `active_orb_slot`, `trail_head`, `trail_count`, `trail[0..trail_count)`）をハッシュする。
   - `hash_internal`: `RunModifiers` の新 14 フィールドおよび `in->thunderfire_gem_cd` を定義順そのままにハッシュする。

---

### 2.6 `src/game/game.c` のエンドレス進行・コマンド配線（凍結）

1. **`game_init`**:
   - `g->ryu_body.head_id = (EntityId){ECS_INVALID_INDEX, 0};`、`orb_ids[0..2]` を同値で初期化し、`g->ryu_body.active_orb_slot = -1;` に設定する。
   - `g->config.endless_loop > 0` のテスト・プリセット起動対応：`loop = clamp(g->config.endless_loop, 0, ENDLESS_MAX_LOOP_N)` とし、`loop > 0` なら `g->run.endless_loop = loop; g->run.act_index = loop * GAME_MAIN_ACT_COUNT; g->run.acts_cleared = GAME_MAIN_ACT_COUNT;` を設定する（`loop == 0` は従来どおり `act_index = 0`）。
   - `roguelike_recompute_modifiers(g)` の直後、`g->player.gale_max_charges = g->internal.mods.gale_max_charges; g->player.gale_charges = g->player.gale_max_charges;` を初期化する。
2. **`handle_command`（段 1）**:
   - `CMD_PICK_ENDLESS_CRYSTAL`: `(void)rewards_pick_endless_crystal(g);` を呼ぶ。
   - `CMD_START_ENDLESS`:
     - **有効条件**: `g->phase == RUN_PHASE_VICTORY && !g->run.run_settled && g->run.endless_loop == 0` のときのみ実行（それ以外は純粋 no-op）。
     - **処理**:
       1. 社 HP を `+3` 回復（`max_hp` 上限。game-design §3.6「進む前に、第 3 幕ボスの報酬（大遺物 3 択 ＋ 社 HP +3）を受け取る」）。
       2. `g->run.endless_loop = 1;`、`g->internal.wave_clear_is_boss = true;`
       3. `rewards_generate_relic_offer(g);`
       4. `g->phase = RUN_PHASE_REWARD; g->phase_t = 0.0f;`
       5. 選択完了後、`clear_reward_and_advance_act` が `act_index` を `2 → 3`（第 4 幕）へ進め、`RUN_PHASE_ACT_TRANSITION` を開始する。
3. **ボス撃破（段 8）と `step_aftermath`**:
   - 段 8 ボス撃破時：
     - `if (g->run.act_index < GAME_MAIN_ACT_COUNT) { ++g->run.acts_cleared; } else if (g->run.endless_acts_cleared < META_MAX_ENDLESS_ACTS) { ++g->run.endless_acts_cleared; }`
     - 社 HP `+3` の条件を `if ((g->run.act_index + 1 < g->run.act_count || g->run.endless_loop > 0) && g->shrine_count > 0)` にする（本編第 3 幕ボス撃破直後は勝利画面へ行き、`CMD_START_ENDLESS` を選んだ時点で初めて +3 回復する）。
   - `step_aftermath`（`g->internal.wave_clear_is_boss` 真・1.0 秒経過時）：
     - `const bool reached_main_victory = (g->run.endless_loop == 0 && g->run.act_index + 1 >= g->run.act_count);`
     - `const bool reached_endless_cap = (g->run.act_index + 1 >= META_MAX_BEST_ACT);`
     - `if (reached_main_victory || reached_endless_cap)` なら `RUN_PHASE_VICTORY` へ遷移し `EV_VICTORY` を出す。それ以外（本編第 1・2 幕、およびエンドレスの全幕）は `rewards_generate_relic_offer(g)` → `RUN_PHASE_REWARD` へ遷移する。

---

### 2.7 `src/render/ui_view.h`（凍結）

`UI_HOVER_REROLL` の直後に `#define UI_HOVER_CRYSTAL 5` を追加し、`UiView` の末尾に次を追加する：
```c
    /* Phase 5 additions: deferred Victory settlement, expanded settings, feedback */
    bool settle_preview;         /* 勝利画面で未精算のまま内訳プレビューを表示中 */
    bool reduce_flash;           /* meta.settings.reduce_flash のミラー */
    uint8_t cursor_scale;        /* meta.settings.cursor_scale のミラー (100/125/150) */
    float settings_notice_t;     /* ヒント初期化/設定初期化の確認表示残り秒数 */
    int32_t settings_notice_kind;/* 0=なし, 1=ヒント初期化済み, 2=既定へ戻した */
```

---

### 2.8 P5-A へ移譲する骨格と先行配線（`upgrades.c`, `relics.c`, `rewards.*`, `elements.c`, `player_skills.c`）

- **`src/game/roguelike/upgrades.c`**:
  - `roguelike_recompute_modifiers(g)` に新 14 フィールドの **P1 互換既定値** を設定：`chain_storm_interval = 3`, `chain_storm_mul = 1.0f`, `surge_stroke_storm = false`, `gale_max_charges = 1`, `cataclysm_thresh = 0.30f`, `cataclysm_cd_base = 2.0f`, `surge_reaction_dmg_mul = 1.0f`, `surge_min_chain = 2`, `thunderfire_gem = false`, `oni_tile = false`, `night_parade_scroll = false`, `bonus_kill_orbs = 0`, `spawn_interval_mul = 1.0f`, `max_alive_add = 0`。
  - 新規 4 札（`UPG_LINGERING_EMBERS`〜`UPG_INKSTONE_BLESSING`）の定義枠を no-op で登録。
  - `g->player.ink_max` 計算に下限 `30.0f` を設け（無尽の筆 `-40` への備え）、`g->player.gale_max_charges` を `mods.gale_max_charges` に同期する（上限増時に増加分だけ `gale_charges` も加算）。
- **`src/game/roguelike/relics.c`**:
  - 新規 6 遺物（`RELIC_SEAL_OF_TRINITY`〜`RELIC_NIGHT_PARADE_SCROLL`）の定義枠を no-op で登録（`RELIC_COUNT = 15` に対応）。
- **`src/game/roguelike/rewards.h` / `rewards.c`**:
  - `bool rewards_pick_endless_crystal(GameState *g);` を宣言・骨格実装する（`REWARD_KIND_FALLBACK` かつ `crystal_reward > 0` のとき `bonus_crystals` 加算・`EV_REWARD_PICK`＋`EVF_REWARD_CRYSTAL`・次ウェーブまたは次幕へ遷移）。
  - `rewards_generate_offer`: 通常時は `repair_amount = 1, crystal_reward = 0`、候補 0 件の `REWARD_KIND_FALLBACK` 時は `repair_amount = 1, crystal_reward = 10`。
  - `rewards_generate_relic_offer`: 候補収集条件を `game_is_relic_unlocked(g, id) && !game_has_relic(g, id)` に更新する。通常時は `repair_amount = 0, crystal_reward = 0`、候補 0 件の `REWARD_KIND_FALLBACK` 時は `repair_amount = 3, crystal_reward = 30`。
  - `clear_reward_and_advance_act`: `++g->run.act_index;` の直後に `g->run.endless_loop = g->run.act_index / GAME_MAIN_ACT_COUNT;` を同期し、`repair_amount = 0; crystal_reward = 0;` をクリアする。
- **`src/game/elements/elements.c`（P5-B 単独通過のための先行配線）**:
  - 4 反応すべての `combat_apply_damage*` 呼び出し（段 2 共鳴本体＆落雷、段 3 爆雷、段 4 炎上 DoT＆雷墨連鎖初撃・ホップ）に `DMG_REACTION` を付与する。
  - 標的チェックを `!game_enemy_is_targetable(g, id)` に統一し、炎上間隔を `(game_is_boss_enemy(en->def_id) || game_is_boss_part(en->def_id)) ? 0.55f : 0.36f` にする。
  - `elements_stage5_projectiles` で未偏向の `PROJ_HOMING_OFUDA`（`proj->kind == PROJ_HOMING_OFUDA && !proj->deflected`）を炎上弾昇格から除外する（炎 > 0.30 で焼失するため）。
- **`src/game/player/player_skills.c`**:
  - `gale_max_charges == 1` でも P1 と完全同一に動く最小の `gale_charges` 同期（`gale_cd == 0` 時に `gale_charges = 1`、発動時に `gale_charges--`）と `mass_gale_multiplier(game_effective_mass_tier(g, vel->mass_tier))` の参照のみ先行配線する（2 チャージの本実装と `surge_stroke_storm` / `ink_regen_no_delay` は P5-A）。

---

### 2.9 P5-B へ移譲する骨格と先行配線（`ofuda_fox.c`, `enemy_defs.*`, `damage.c`, `projectiles.c`, `waves.c`）

- **`src/game/enemies/ofuda_fox.c`（新規 stub）**: `ofuda_fox_spawn` / `ofuda_fox_step` の空実装を置く。
- **`src/game/enemies/enemy_defs.h` / `enemy_defs.c`**:
  - `ofuda_fox_spawn` / `ofuda_fox_step` 宣言を追加。
  - `k_enemy_defs[ENEMY_DEF_COUNT]`（12 件）に `ENEMY_OFUDA_FOX`（HP `30`, 半径 `17.0f`, `MASS_MEDIUM`, 結合 `1.5f`, 点 `200`, 墨玉 `2`）と `ENEMY_BOSS_RYU_ORB`（HP `300`, 半径 `30.0f`, `MASS_BOSS`, 結合 `0.0f`, 点 `300`, 墨玉 `2`, `spawn_fn = NULL`, `ai_step_fn = NULL`）を登録する。
  - `enemy_spawn` で `game_is_boss_part(def_id)` のとき `COMP_BOSS` を付与し、`g->run.boss_id` は上書きしない。
  - `enemy_apply_elite` で `game_is_boss_enemy(en->def_id) || game_is_boss_part(en->def_id) || en->def_id == ENEMY_OFUDA_FOX` を除外する。
- **`src/game/combat/damage.c`（P5-A 単独通過のための先行配線）**:
  - `combat_apply_damage_f`:
    - 先頭で `!game_enemy_is_targetable(g, target)` なら `0` を返す。
    - `is_boss_like = is_boss || game_is_boss_part(en->def_id)` で墨印 `×1.5` と雷耐性 `-50%` を龍玉にも適用。
    - `if (g->player.surge_t > 0.0f && (dmg_flags & DMG_REACTION) != 0u) { dmg *= mods->surge_reaction_dmg_mul; }`
    - `if (st->barrier_t > 0.0f) { dmg *= 0.5f; }`（護符結界 -50%）
  - `kill_enemy`:
    - 墨昇華発動条件を `if (g->run.chain >= mods->surge_min_chain)` に変更（墨 `+8 * min(chain, 4)` は従来どおり `chain >= 2`）。
    - 連殺の雷の発火条件を `const int32_t ci = mods->chain_storm_interval > 0 ? mods->chain_storm_interval : 3; if (g->run.chain >= ci && (g->run.chain % ci) == 0)`、雷量を `1.15f * mods->chain_storm_mul` にする。
    - 墨玉ドロップ数を `(is_elite ? 5 : def->orb_count) + (game_is_boss_part(def_id) ? 0 : mods->bonus_kill_orbs)` にする。
    - `!game_is_boss_part(def_id)` ガードを `wave_kills` 加算に足す。
  - `slam_damage_for_enemy` / `combat_physics_and_slams_step`:
    - `MassTier eff_tier = game_effective_mass_tier(g, vel->mass_tier);` を使い、`ENEMY_BOSS_NUE` は常に `60.0f`、それ以外は `eff_tier == MASS_LIGHT || (mods->oni_tile && eff_tier == MASS_MEDIUM)` で即撃破 `(float)(max_hp > 0 ? max_hp * 4 : 1000)` とする（`ELITE_RIGID` かつ `barrier_t` の `0.25x` でも `max_hp` 以上で即撃破成立）。重量級スタン条件は `eff_tier == MASS_HEAVY || nue_slam_active` とする。
- **`src/game/enemies/projectiles.c`**:
  - 偏向後の追尾・命中・雷撃バウンス対象から `!game_enemy_is_targetable(g, eid)` を除外する。
  - `ENEMY_BOSS_RYU_ORB` の追尾優先（`×0.65f`）と命中半径（`36.0f`）を配線する。
- **`src/game/roguelike/waves.c`（P5-A 単独通過のための先行配線）**:
  - `waves_start_wave` / `waves_step_spawns` で `spawn_interval * g->internal.mods.spawn_interval_mul`（下限 `0.35f`）と `max_alive + g->internal.mods.max_alive_add` を適用する（既定 `1.0f` / `0` では P1 と完全同一）。
  - `waves_count_alive_escorts` と `waves_step_spawns` で `!game_is_boss_part(en->def_id)` を除外する。

---

### 2.10 P5-C へ移譲する暫定文字列表対応（`src/render/text_table.h` / `text_table.c`）

`test_text_table` が P5-Pre / P5-A / P5-B の CI で確実に通るよう、`TEXT_P2_STUB`（JA: `"準備中"`, EN: `"TBD"`）を `text_table.h` / `text_table.c` に一時追加する：
- `text_enemy_name` / `text_enemy_intro`: `ENEMY_OFUDA_FOX`, `ENEMY_BOSS_RYU_ORB` に対し `TEXT_P2_STUB` を返す。
- `text_upgrade_name` / `text_upgrade_effect`: **重要** — 既存実装は `TEXT_UPG_EMBER_TIP_NAME + (int)id * 4` の添字演算を行っており、`id >= 24`（`UPG_LINGERING_EMBERS`〜`UPG_INKSTONE_BLESSING`）をそのまま渡すと `TEXT_RELIC_*` 領域へはみ出す。`if (id >= UPG_LINGERING_EMBERS) return TEXT_P2_STUB;` で安全にガードする。
- `text_relic_name` / `text_relic_effect`: `RELIC_SEAL_OF_TRINITY`〜`RELIC_NIGHT_PARADE_SCROLL`（9..14）に対し `TEXT_P2_STUB` を返す（`text_relic_tradeoff` は既存 4 種のみ非 `TEXT_NONE` を維持し、P5-C で解放 6 種の正式文字列と代償 3 種へ置換＋`TEXT_P2_STUB` 削除を行う）。

---

## 3. テスト契約の同期

### 3.1 凍結テストの更新（`tests/test_core_smoke.c`, `tests/test_app_save.c`）
1. **`tests/test_core_smoke.c`**: `test_phase5_pre_contracts` を追記する：
   - `UPG_COUNT == 28`、`RELIC_COUNT == 15`、`ENEMY_DEF_COUNT == 12`、`PROJ_KIND_COUNT == 6`。
   - 全 28 札の `game_upgrade_max_level == 3`・有効 `UpgradeAxis`、全 15 遺物の `game_relic_unlock_slot`（0..8 は `-1`、9..14 は `0..5`）と `game_is_relic_unlocked` のマスク照合。
   - `game_is_boss_part(ENEMY_BOSS_RYU_ORB)` が真、他は偽。
   - `game_debug_start_act(g, 3)` で `act_index == 3, endless_loop == 1`、範囲外（`-1`, `META_MAX_BEST_ACT`）は no-op（既存の `test_phase4_pre_contracts` の上限チェックを `META_MAX_BEST_ACT` に更新する）。
   - `CMD_START_ENDLESS` が `RUN_PHASE_VICTORY` 以外では no-op であり、`RUN_PHASE_VICTORY && !run_settled && endless_loop == 0` で社 HP `+3`・`endless_loop == 1`・`RUN_PHASE_REWARD`（`REWARD_KIND_RELIC` または `REWARD_KIND_FALLBACK`）へ遷移すること。
   - `game_init` で `RyuBodyState` が無効 ID・`trail_count == 0` で初期化され、`game_hash` に反映されること。
2. **`tests/test_app_save.c`**:
   - `assert_meta_equal` に `ui_lang`, `reduce_flash`, `cursor_scale` の検証を追加し、`test_save_roundtrip` で非既定値（`UI_LANG_EN`, `true`, `125`）を往復確認する。

### 3.2 `tests/test_roguelike_meta.c` の最小整合（P5-A へ移譲）
- `defaults`（`.settings = {80, 70, 85, 100, true, true, UI_LANG_AUTO, false, 100}`）、`saved`（`.settings = {11, 22, 33, 50, false, false, UI_LANG_EN, true, 125}`）、`assert_settings` を新 3 項目へ同期する。
- `test_serialized_save_matches_architecture_example` の `example` と `golden[]` に `opt_lang=0\nopt_reduce_flash=0\nopt_cursor_scale=100\n` を追加し、P1 形式セーブ（新 3 キー省略）が既定値で正常復元される検証および `opt_cursor_scale` の閾値（`<113 → 100`, `113..137 → 125`, `>=138 → 150`）テストを追加する。
- `test_hook_acquisition_order_and_relic_rewards` のブロック 2 で `cfg.unlocked_relic_mask = (UINT64_C(1) << META_RELIC_UNLOCK_COUNT) - 1u;` を設定し、全 15 遺物解放状態で「14 個所持 → 残り 1 個提示、15 個全所持 → `REWARD_KIND_FALLBACK`」が成立するよう整合する。

---

## 4. 設計書同期

本 PR 内で次の設計書を実契約と一致させる：
- `docs/PLAN.md` §9.1：Phase 5 行を「`P5-Pre` → 3 並列（`P5-A` `wt-roguelike-p2` / `P5-B` `wt-endless-ryu-p2` / `P5-C` `wt-ui-settings-p2`）→ `P5-Int`」に更新し、各指示書への相対リンクを張る。
- `docs/architecture.md`：§5.3（`GameState.ryu_body`）・§6.3（28 札 / 15 遺物）・§7.1（`EV_BOSS_PART_BREAK` / `EVF_REWARD_CRYSTAL`）・§8.1 / §8.2（`SettingsState` 拡張 3 項目、`opt_lang` / `opt_reduce_flash` / `opt_cursor_scale`、`META_SERIALIZE_CAP = 640`）を同期する。
- `docs/game-design.md` §11.2：設定画面に「表示言語（自動/日本語/English）・閃光軽減（ON/OFF）・カーソル倍率（100%/125%/150%）・初回ヒントを初期化・設定を既定に戻す」を追記する。
- `docs/qa-and-testing.md`：Phase 5 の検証対象（28 札/15 遺物、狐符・追尾符、エンドレス決定性・遅延精算、多部位一筆龍、拡充設定のセーブ後方互換）を追記する。

---

## 5. ローカル検証・PR と完了条件

```sh
# 1. ASan/UBSan ビルドと関連テスト（既存 15 件のうち契約影響テストすべて）
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan --target inkstorm \
        test_core_smoke test_roguelike_meta test_elements test_acts_enemies \
        test_determinism_run test_app_save test_text_table &&
    nix develop -c ctest --preset asan --output-on-failure \
        -R '^(test_core_smoke|test_roguelike_meta|test_elements|test_acts_enemies|test_determinism_run|test_app_save|test_text_table|test_cli_headless_smoke)$'

# 2. 境界確認（いずれも 0 件）
grep -rn 'raylib.h' src/core src/fluid src/ecs src/game src/audio \
    src/render/ui_view.h src/render/text_table.* src/app/save.*
grep -rnE '(->|\.)internal\b' src/render src/audio src/app
```

**完了条件（Definition of Done）**
- [ ] §2 の全契約（`meta.*`, `game_ids.h`, `game_events.h`, `game.h`, `game_contract.c`, `debug_api.c`, `game_internal.h`, `hash.c`, `game.c`, `ui_view.h`）と先行配線・stub が揃い、P5-A / P5-B / P5-C の本実装を先回りしていない。
- [ ] 上記 ASan/UBSan ビルド・CTest 対象すべてと `--headless-smoke` が通る。
- [ ] §4 の設計書同期が完了している。
- [ ] PR 作成直後に `@coderabbitai review` を投稿し、全指摘を 4 分類トリアージ（修正必須 / 軽微改善 / 設計意図説明 / 誤検知）して必要な修正と返信を完了した（[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md) 準拠）。
- [ ] PR の CI 3 ジョブ（`build-asan-and-test`, `static-analysis`, `dist-verify`）がすべて緑。
