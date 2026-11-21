# Phase 4-Pre タスク指示書：P1 本編の共通契約（追加 ID・イベント・UiView/GameState 拡張枠・テスト登録・骨格）

> **必読**: [PLAN.md](../PLAN.md) §9（特に §9.1 の Phase 4 行と §9.2）、[architecture.md](../architecture.md) §1〜§9、[game-design.md](../game-design.md) §3〜§12、[art-direction.md](../art-direction.md) §6 / §10〜§15、[qa-and-testing.md](../qa-and-testing.md) §2〜§5、[phase3-pre-contracts.md](phase3-pre-contracts.md)、[phase3-integration.md](phase3-integration.md)、[AGENTS.md](../../AGENTS.md)
>
> **前提**: P3-Int（`feat/phase3-integration`）が `main` にマージされ、Phase 3 Gate（第 1 幕 P0 縦切り完走・CI 全緑）を通過していること。着手時に `main` が最新であることを確かめる。
> **作業場所 / ブランチ**: リポジトリルート（`full-scale-dev/Ink-storm`、作業ツリーがクリーンな状態）で `main` から `feat/phase4-pre-contracts` を作り、**1 本の PR** にする。
> **後続**: P4-A（[phase4-roguelike-p1.md](phase4-roguelike-p1.md)）、P4-B（[phase4-acts-enemies-p1.md](phase4-acts-enemies-p1.md)）、P4-C（[phase4-ui-assets-p1.md](phase4-ui-assets-p1.md)）、P4-Int（[phase4-integration.md](phase4-integration.md)）

---

## 0. 目的と禁止事項

Phase 4（P1 本編完成）では、次の **3 つの worktree を同時並列**で進める。
- **P4-A**（`wt-roguelike-p1`）：筆スタイル 3 種（豪筆・朱筆・細筆）、爆雷・三元共鳴、強化札 24 種、初期大遺物 9 種、`RunModifiers`＋7 フック、初回ヒント発火
- **P4-B**（`wt-acts-enemies-p1`）：第 2 幕（鳥居/岩柱 `solid`・鵺）、第 3 幕（山颪・霊柱・一筆龍簡易版）、新通常敵 4 種（提灯妖・雷 / 鎌鼬 / 石鬼 / 墨吸い）、エリート変異 4 種
- **P4-C**（`wt-ui-assets-p1`）：タイトル/ロビー・筆選択・墨庵・設定・ポーズ（`Esc` ポーズ化に伴う `AGENTS.md` / `README.txt` 更新含む）・初回操作ヒント UI・第 2/3 幕情景と新敵/ボス描画・毛筆体フォント・外部 BGM・`assets/LICENSES.txt`

P4-A と P4-B が同じ `src/game/` 内を並列編集し、P4-C が `src/render/`・`src/audio/`・`src/app/` を並列編集するため、本タスク（P4-Pre）で **3 並列が 1 ファイルも衝突せず、かつマージ順を問わず各ブランチ単独で CI 全緑になる境界契約と骨格** を `main` に確定させる。

1. **境界ヘッダと共有パイプラインの凍結**:
   - `game_ids.h`（敵 10 種・弾 5 種・強化札 24 種・初期大遺物 9 種・エリート変異フラグ・`ENEMY_FLAG_SUMMONED`・ギミック種別 3 種・`DMG_CATACLYSM`）
   - `game_events.h`（`EVF_REWARD_RELIC` 追加と、`EV_THUNDERBURST` / `EV_CATACLYSM` / `EV_HINT_TRIGGER` / `EV_WIND_TELEGRAPH` / `EV_REWARD_PICK` のペイロード確定）
   - `game.h`（`RunProgress` への所持大遺物配列 `relics[]` / `relic_count` 追加、`game_has_relic` / `game_is_boss_enemy` / `game_debug_start_act` 宣言）
   - `game_internal.h` と `hash.c`（`#include "ecs.h"` 追加、P4-A 用 `RunModifiers` / 反応 CD / フック状態と P4-B 用幕ギミック / エリート抽選 / ボス状態の枠を先行確定して凍結）
   - `game.c`（本編 3 幕 `act_count = GAME_MAIN_ACT_COUNT`、第 1・2 幕ボス撃破後の「社 HP +3 → 大遺物 3 択 → 次幕 `ACT_TRANSITION`」と第 3 幕ボス撃破後の `VICTORY` 遷移、段 6 `stage_gimmicks_step`・段 7e `hints_check_tick` 呼び出しを先行配線して凍結）
   - `ui_view.h`（ロビー・筆選択・墨庵・設定・ポーズ・ヒント表示・見本帳サブページ切替用の `UiScene` / `UiView` 拡張枠を確定して凍結）
2. **新規ファイルの骨格・テスト登録・クロス所有モジュール間の先行配線**:
   - P4-B 専用の単体テスト `test_acts_enemies` を `CMakeLists.txt` に先行登録し、骨格テストを置く。並列期間中は **P4-A / P4-B / P4-C のいずれも `CMakeLists.txt` を編集しない**（`src/<module>/*.c` は `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` で自動検出される）。
   - 新規翻訳単位（`src/game/roguelike/relics.c` / `.h`、`hints.c` / `.h`、`stage_gimmicks.c` / `.h`）の骨格を置く。
   - **並列単独通過のための先行配線（重要）**:
     - P4-B 所有となる `damage.c` / `projectiles.c` / `autoplay.c` に、P4-A の強化札・大遺物（`RunModifiers`・`barrier_shield_ready`・`rosary_swirl` 等）と `REWARD_KIND_RELIC`・`game_is_boss_enemy` の参照を先行配線する（既定値は P0 と完全同一）。これにより P4-A は P4-B マージ前でも全 24 札・9 遺物の単体テストを単独で通せる。
     - P4-A 所有となる `elements.c` に、P4-B のエリート耐性（`ELITE_INK_REPEL`・`ELITE_STORMPROOF`）と `game_is_boss_enemy`・`RunModifiers` 参照を先行配線する。これにより P4-B は P4-A マージ前でもエリート・新ボスの単体テストを単独で通せる。
     - P4-C 所有となる `text_table.h` / `text_table.c` に、新規 6 敵・16 札に対する非 `TEXT_NONE` の暫定プレースホルダを返し、`test_text_table` が P4-Pre / P4-A / P4-B の CI で緑になる状態を保つ。

### 0.1 Phase 4 の進め方

| 段階 | 担当 | ブランチ | PR とマージ |
| :--- | :--- | :--- | :--- |
| 1 | P4-Pre（本書） | `feat/phase4-pre-contracts` | 1 本。CI 全緑・CodeRabbit 確認後にマージし、下の 3 つの worktree を切る |
| 2a | P4-A | `full-scale-dev/wt-roguelike-p1` / `feat/phase4-roguelike-p1` | 完了したら単独で PR → `@coderabbitai review`・CI 全緑 → マージ |
| 2b | P4-B | `full-scale-dev/wt-acts-enemies-p1` / `feat/phase4-acts-enemies-p1` | 完了したら単独で PR → `@coderabbitai review`・CI 全緑 → マージ（2a/2c と順不同） |
| 2c | P4-C | `full-scale-dev/wt-ui-assets-p1` / `feat/phase4-ui-assets-p1` | 完了したら単独で PR → `@coderabbitai review`・CI 全緑 → マージ（2a/2b と順不同） |
| 3 | P4-Int | 2a・2b・2c マージ後の `main` から `feat/phase4-integration` | 3 並列の結合修正・全 3 幕＆墨庵通し完走・Phase 4 Gate を 1 本の PR に |

- 各並列ブランチは PR 作成前に最新の `main` を取り込み（`git merge main`）、先行マージされた他ブランチがあればその状態で自身の対象テストとスモークを再実行する。

**禁止事項**
- P4-A / P4-B / P4-C の本実装（筆スタイル 3 種の個別差別化、新強化札 16 種の効果本体、爆雷・三元共鳴のセル走査、新敵 4 種と鵺・一筆龍の AI、ロビー・墨庵・ポーズの本格描画、毛筆体フォント・外部 BGM の追加）を本タスクで先回り実装しない。
- Phase 2 / Phase 3 で凍結済みの下位 API（`fluid.h`, `ecs.h`, `meta.h`）のシグネチャを変えない。
- `core` / `fluid` / `ecs` / `game` / `audio` に `raylib.h` を include しない。

---

## 1. ファイルと所有

### 1.1 本タスク（P4-Pre）で変更・新規作成するファイル

| ファイル | 操作 | 内容 | Pre 後の扱い |
| :--- | :---: | :--- | :--- |
| `src/game/game_ids.h` | 更新 | P1 の敵 10 種・弾 5 種・強化札 24 種・初期大遺物 9 種・エリート変異フラグ・`ENEMY_FLAG_SUMMONED`・ギミック種別・`DMG_CATACLYSM`（§2.1） | **凍結** |
| `src/game/game_events.h` | 更新 | `EVF_REWARD_RELIC` 追加、Phase 4 イベントペイロード契約確定（§2.2） | **凍結** |
| `src/game/game.h` | 更新 | `RunProgress` に `relics[]` / `relic_count` 追加、`game_has_relic` / `game_is_boss_enemy` / `game_debug_start_act` 宣言（§2.3） | **凍結** |
| `src/game/game_contract.c` | 更新 | 24 種の `k_upgrade_meta`、`game_has_relic`、`game_is_boss_enemy` 実装（`game_run_result` は既存の `acts_cleared >= GAME_MAIN_ACT_COUNT` を維持）（§2.3） | **凍結** |
| `src/game/game_internal.h` | 更新 | `#include "ecs.h"` 追加、P4-A 用・P4-B 用の内部フィールドおよび `RunModifiers` 全フィールドを自己完結定義（§2.4） | **凍結** |
| `src/game/hash.c` | 更新 | `RunProgress.relics` / `relic_count` と拡張 `GameInternal` の全フィールドをハッシュへ反映（§2.4） | **凍結** |
| `src/game/game.c` | 更新 | 3 幕進行、`rewards_pick_reward` 呼出、第 1・2 幕ボス後の社 HP +3 ＆大遺物報酬 → 次幕遷移、段 6 ギミック、段 7e ヒント配線（§2.5） | **凍結** |
| `src/game/debug_api.c` | 更新 | `game_debug_start_act(g, act_index)` 追加（§2.3） | **凍結** |
| `src/game/roguelike/relics.h` / `relics.c` | 新規 | 大遺物付与・加算/乗算修飾子適用・7 フック（`upgrades.h` の `*HookArgs` 再利用）の骨格（§2.6） | P4-A へ移譲 |
| `src/game/roguelike/hints.h` / `hints.c` | 新規 | 初回操作・反応ヒント発火判定の骨格（§2.6） | P4-A へ移譲 |
| `src/game/roguelike/upgrades.h` / `upgrades.c` | 更新 | 24 札分のテーブル枠（新規 16 札は no-op 骨格）、`RunModifiers` P0 互換既定値初期化、`relics_apply_modifiers_*` / `relics_fire_on_*` 呼び出し、`fluid.cfg.dye_damp[0..1]` 同期（§2.6） | P4-A へ移譲 |
| `src/game/roguelike/rewards.h` / `rewards.c` | 更新 | `rewards_generate_relic_offer`、`rewards_pick_reward`（`rewards_pick_upgrade` を統合置換）、`rewards_reroll`（遺物対応）、ボス後 `REWARD_KIND_FALLBACK` の次幕遷移（§2.6） | P4-A へ移譲 |
| `src/game/elements/elements.c` | 更新 | `RunModifiers`（`ink_mark_duration` / `burn_base_dmg` / `storm_zap_cd` / `burning_proj_pierce` / `deflected_max_speed_mul`）、`game_is_boss_enemy`、`ELITE_INK_REPEL`、`ELITE_STORMPROOF` の先行配線（§2.6） | P4-A へ移譲 |
| `src/game/roguelike/stage_gimmicks.h` / `stage_gimmicks.c` | 新規 | 第 2・3 幕の地形配置・山颪・霊柱・一時ギミック更新の骨格（§2.7） | P4-B へ移譲 |
| `src/game/roguelike/waves.h` / `waves.c` | 更新 | 3 幕配列 `k_act_defs[GAME_MAIN_ACT_COUNT]` の枠（第 2・3 幕は暫定で第 1 幕コピー）、`waves_prepare_act` の ECS クリア＋`stage_gimmicks_setup_act` 呼出、`game_is_boss_enemy` 対応（§2.7） | P4-B へ移譲 |
| `src/game/enemies/enemy_defs.h` / `enemy_defs.c` | 更新 | 10 種の `k_enemy_defs` 枠（新規 6 種は基礎パラメータ本値＋暫定 stub）、`game_is_boss_enemy` による `COMP_BOSS` ＆ `run.boss_id` 設定（§2.7） | P4-B へ移譲 |
| `src/game/combat/damage.c`、`src/game/enemies/projectiles.c`、`src/game/autoplay.c` | 更新 | `game_is_boss_enemy`、`PROJ_BOUNCE_ORB`、`RunModifiers`（P0 互換既定値）全参照、`rosary_swirl`、`barrier_shield_ready`、`ELITE_STORMPROOF` バウンス除外、余韻時一時ギミック破棄、`autoplay` の `REWARD_KIND_RELIC` 対応（§2.7） | P4-B へ移譲 |
| `src/render/ui_view.h` | 更新 | `UiScene`（ロビー・筆選択・墨庵・設定）と `UiView`（ポーズ・ヒント・メタ表示・`showcase_subpage` 枠）の拡張（§2.8） | **凍結** |
| `src/render/text_table.h` / `text_table.c` | 更新 | 新規 6 敵・16 札に対する暫定プレースホルダ（`TEXT_P1_STUB`）を返して `test_text_table` を維持、`text_act_title(1..2)` は `TEXT_NONE` のまま維持（§2.8） | P4-C へ移譲 |
| `tests/test_acts_enemies.c` | 新規 | P4-B 用の幕・大遺物報酬・次幕遷移の単体テスト骨格（§3.1） | P4-B へ移譲 |
| `tests/test_determinism_run.c` | 最小更新 | `act_count = 3` 化に伴う既存 P0 単幕テストの整合（`a->run.act_count = 1; b->run.act_count = 1;`）（§3.1） | P4-B へ移譲 |
| `tests/test_core_smoke.c` | 更新 | 24 札・9 遺物・新敵 ID・3 幕設定・`game_debug_start_act` の基本契約チェック追記（§3.2） | **凍結** |
| `CMakeLists.txt` | 追記のみ | `test_acts_enemies` の登録（§3.1） | **凍結** |
| `docs/PLAN.md`、`docs/architecture.md`、`docs/game-design.md`、`docs/qa-and-testing.md` | 更新 | Phase 4 の確定契約を同期（§4） | **凍結**（`game-design.md` / `art-direction.md` の数値調整のみ各担当） |

### 1.2 並列期間（P4-A / P4-B / P4-C）の排他所有表

**3 つの並列ブランチ間で編集対象ファイルが 1 つも重ならない**よう、以下のとおり厳密に分割する。

| 区分 | 所有ファイル |
| :--- | :--- |
| **凍結**（並列期間中は編集禁止。変更が必要な場合は Pre §1.3 の契約変更手続きに従う） | `CMakeLists.txt`、`CMakePresets.json`、`src/core/*`、`src/fluid/*`、`src/ecs/*`、`src/game/meta.*`、`src/game/game.h`、`src/game/game_ids.h`、`src/game/game_events.h`、`src/game/game_components.h`、`src/game/game_contract.c`、`src/game/game_internal.h`、`src/game/game.c`、`src/game/hash.c`、`src/game/debug_api.c`、`src/render/ui_view.h`、`tests/test_core_smoke.c`、`tests/test_fluid.c`、`tests/bench_fluid.c`、`tests/test_ecs.c`、`tests/test_app_save.c`、`tools/*`、`.github/*`、`flake.*`、`.agents/*`、`docs/PLAN.md`、`docs/architecture.md`、`docs/qa-and-testing.md` |
| **P4-A** (`wt-roguelike-p1`) | `src/game/player/*`（`player_skills.c`/`.h`）、`src/game/elements/*`（`elements.c`/`.h`）、`src/game/roguelike/upgrades.c`/`.h`、`src/game/roguelike/relics.c`/`.h`、`src/game/roguelike/rewards.c`/`.h`、`src/game/roguelike/hints.c`/`.h`、`tests/test_elements.c`、`tests/test_roguelike_meta.c` |
| **P4-B** (`wt-acts-enemies-p1`) | `src/game/combat/*`（`damage.c`/`.h`）、`src/game/enemies/*`（`enemy_defs.c`/`.h`、`archer.c`、`lantern.c`、`umbrella.c`、`kamaitachi.c`、`stone_oni.c`、`ink_leech.c`、`boss_tengu.c`、`boss_nue.c`、`boss_ryu.c`、`projectiles.c`/`.h`）、`src/game/roguelike/waves.c`/`.h`、`src/game/roguelike/stage_gimmicks.c`/`.h`、`src/game/autoplay.c`、`tests/test_acts_enemies.c`、`tests/test_determinism_run.c`、`tests/bench_game.c`、`docs/game-design.md`（数値調整のみ） |
| **P4-C** (`wt-ui-assets-p1`) | `src/render/*`（`ui_view.h` を除く全ファイル）、`src/audio/*`、`src/app/*`、`assets/*`、`README.txt`、`AGENTS.md`（`Esc` ポーズ化に伴う終了操作表記の更新のみ）、`tests/test_app_core.c`、`tests/test_audio.c`、`tests/test_text_table.c`、`docs/art-direction.md`（数値調整のみ） |

### 1.3 並列期間中の契約変更手続き
- 並列作業中に凍結ファイルの変更が不可避となった場合、当該ブランチで直接書き換えず、PR 本文または作業メモに「変更理由・影響ファイル・最小差分案」を記載して統合担当へエスカレーションする。
- 承認された契約変更は小さな単独 PR として先に `main` へマージし、他の並列 worktree が `git merge main` で取り込む。

---

## 2. 契約仕様の詳細

### 2.1 `src/game/game_ids.h`（凍結）

既存の P0 列挙値（値と順序）は一切変更せず、各 enum の `*_COUNT` の直前に P1 の要素を追記する。ビットフラグは `-Wpedantic` 準拠のため `#define`（`UINT32_C(1) << n`）で定義する。

#### (1) `EnemyDefId`（全 10 種：通常敵 7 種＋幕ボス 3 体）
```c
typedef enum {
    ENEMY_ARCHER = 0,       /* 1-1: 弓妖 */
    ENEMY_LANTERN_FIRE,     /* 1-1: 提灯妖・炎 */
    ENEMY_UMBRELLA,         /* 1-2: 唐傘妖 */
    ENEMY_BOSS_TENGU,       /* 1-Boss: 大天狗 */
    ENEMY_LANTERN_STORM,    /* 2-1: 提灯妖・雷 */
    ENEMY_KAMAITACHI,       /* 2-1: 鎌鼬 */
    ENEMY_STONE_ONI,        /* 2-2: 石鬼 */
    ENEMY_INK_LEECH,        /* 3-1: 墨吸い */
    ENEMY_BOSS_NUE,         /* 2-Boss: 鵺 */
    ENEMY_BOSS_RYU,         /* 3-Boss: 一筆龍（P1 は簡易版） */
    ENEMY_DEF_COUNT
} EnemyDefId;
```
- 任意枠の「狐符」（`game-design.md` §8.1）は Phase 5（P2 拡張）で追加する。P1 の第 3 幕第 2 ウェーブおよび第 3 幕増援表の狐符枠は、`game-design.md` §8.1 の規定（「実装しない場合は、出現表とウェーブ表の狐符を鎌鼬に置き換える」）に従い鎌鼬へ置き換える。

#### (2) エリート変異ビットフラグ・召喚識別フラグ（`EnemyComp.elite_flags` 用）
```c
#define ELITE_RIGID       (UINT32_C(1) << 0)  /* 剛体（金）：質量+1段、叩きつけ被ダメ-50%、砲台不可 */
#define ELITE_INK_REPEL   (UINT32_C(1) << 1)  /* 撥墨（白）：墨印無効、周囲48pxの墨を毎秒20%散らす */
#define ELITE_FIREPROOF   (UINT32_C(1) << 2)  /* 不燃（朱）：炎分類被ダメ-70% */
#define ELITE_STORMPROOF  (UINT32_C(1) << 3)  /* 避雷（蒼）：雷分類被ダメ-70%、雷墨連鎖対象/中継点外 */
#define ELITE_ALL_MASK    (ELITE_RIGID | ELITE_INK_REPEL | ELITE_FIREPROOF | ELITE_STORMPROOF)
#define ENEMY_FLAG_SUMMONED (UINT32_C(1) << 16) /* ボス召喚護衛（wave_kills 非計上、is_elite とは独立） */
```

#### (3) `ProjKind`（全 5 種）
```c
typedef enum {
    PROJ_ARROW = 0,
    PROJ_FIREBALL,
    PROJ_STORM_ORB,
    PROJ_WIND_SICKLE, /* 鎌風（300px/s、流体影響×1.5、偏向後20） */
    PROJ_BOUNCE_ORB,  /* 跳弾雷球（230px/s、流体影響×1.0、壁/solidで最大2回反射、偏向後20+雷撃） */
    PROJ_KIND_COUNT
} ProjKind;
```

#### (4) `UpgradeId`（P1 全 24 種：各軸 5 種 × 4 軸 ＋ 共通 4 種）
P0 の 8 種（0..7）を維持し、8..23 に 16 種を追加する（残り 4 種：`残り火`・`雷雲の筆`・`二段風斬り`・`硯の恵み` は Phase 5 P2 で 28 種へ拡張）。
```c
typedef enum {
    /* P0 既存 8 種 (0..7) */
    UPG_EMBER_TIP = 0,      /* 紅蓮: 火種の筆 */
    UPG_WILDFIRE,           /* 紅蓮: 延焼 */
    UPG_CHARGED_SEAL,       /* 雷霆: 帯電印 */
    UPG_CHAIN_LIGHTNING,    /* 雷霆: 連鎖雷 */
    UPG_GREAT_VORTEX,       /* 烈風: 大渦 */
    UPG_SLAM_SHOCKWAVE,     /* 烈風: 叩きつけ衝撃 */
    UPG_RICH_INK,           /* 墨道: 墨玉増量 */
    UPG_LASTING_SURGE,      /* 墨道: 長き昇華 */
    /* P1 追加：紅蓮 (+3 -> 計5) */
    UPG_FLAME_GALE,         /* 紅蓮: 焔の風斬り */
    UPG_INFERNO,            /* 紅蓮: 業火 */
    UPG_PHOENIX_ARROW,      /* 紅蓮: 不死鳥の矢 */
    /* P1 追加：雷霆 (+3 -> 計5) */
    UPG_THUNDER_ARROW,      /* 雷霆: 雷撃弾 */
    UPG_SWIFT_THUNDER,      /* 雷霆: 迅雷 */
    UPG_THUNDERBURST_FUSE,  /* 雷霆: 爆雷の導火 */
    /* P1 追加：烈風 (+3 -> 計5) */
    UPG_MIGHTY_STROKE,      /* 烈風: 剛腕の筆 */
    UPG_MAELSTROM_BIND,     /* 烈風: 渦縛り */
    UPG_TAILWIND,           /* 烈風: 追い風 */
    /* P1 追加：墨道 (+3 -> 計5) */
    UPG_INK_MAGNET,         /* 墨道: 吸墨 */
    UPG_CHAIN_PRIMER,       /* 墨道: 呼び水 */
    UPG_DRY_MASTERY,        /* 墨道: かすれの妙 */
    /* P1 追加：共通 (+4 -> 計4) */
    UPG_SHRINE_WARD,        /* 共通: 社の護符 */
    UPG_DEEP_INK,           /* 共通: 濃墨 */
    UPG_DEEP_MARK,          /* 共通: 墨印深化 */
    UPG_BARRIER,            /* 共通: 結界 */
    UPG_COUNT               /* 24 */
} UpgradeId;
```

#### (5) `RelicId`（P1 初期解放の大遺物 9 種）
```c
typedef enum {
    RELIC_VERMILION_INKSTONE = 0, /* 朱の硯 */
    RELIC_THUNDER_DRUM,           /* 雷神の太鼓 */
    RELIC_TENGU_FAN,              /* 天狗の羽団扇 */
    RELIC_INK_DYED_SHRINE,        /* 墨染めの社 */
    RELIC_TWIN_BRUSH,             /* 連理の筆 */
    RELIC_ROSARY_SWIRL,           /* 数珠の渦 */
    RELIC_PACT_OF_FIRE,           /* 業火の契り */
    RELIC_LINGERING_INK,          /* 残墨 */
    RELIC_WIND_RETURN,            /* 返し風 */
    RELIC_COUNT                   /* 9 */
} RelicId;
```
- **重要（墨庵解放 6 種との分離契約）**:
  - 墨庵（`meta.h` の `META_RELIC_UNLOCK_COUNT = 6`、スロット `0..5`）で購入・解放する追加 6 種（三元の印、墨嵐の極意、雷火の宝珠、鬼瓦、無尽の筆、百鬼夜行の巻物）は、P4-C の墨庵 UI で購入・セーブ可能とし、`text_table` に名前と説明を持つ。
  - ただしラン中の遺物効果の本実装は Phase 5（P2）であるため、P1 の `RelicId` は初期解放の 9 種（`RELIC_COUNT = 9`）のみとする。これにより、P1 で墨庵の遺物解放を購入しても、未実装の P2 遺物がラン中のボス報酬に混入しない。

#### (6) `GimmickKind` / `GimmickElemKind` と `DMG_CATACLYSM`
```c
typedef enum {
    GIMMICK_NONE = 0,
    GIMMICK_SPIRIT_PILLAR,     /* 第3幕 霊柱 */
    GIMMICK_LIGHTNING_STRIKE,  /* 一筆龍 雷玉の落雷予兆 */
    GIMMICK_KAMAITACHI_VORTEX, /* 鎌鼬 逆回転旋風（持続 1.0s） */
    GIMMICK_COUNT
} GimmickKind;

typedef enum {
    GIMMICK_ELEM_NONE = 0,
    GIMMICK_ELEM_FIRE,
    GIMMICK_ELEM_STORM
} GimmickElemKind;

#define DMG_CATACLYSM (UINT32_C(1) << 6) /* 三元共鳴ダメージ（一筆龍 点睛後で×2） */
```

---

### 2.2 `src/game/game_events.h`（凍結）

`EVF_REWARD_REPAIR` の直後に次を追加する：
```c
#define EVF_REWARD_RELIC (UINT32_C(1) << 13)
```

Phase 4 で発火が有効化されるイベントのペイロード契約を以下のとおり固定する：

| イベント | `x, y` | `x2, y2` | `value` | `flags` |
| :--- | :--- | :--- | :--- | :--- |
| `EV_THUNDERBURST` | 爆雷ブロック内の爆雷セル重心 (px) | `radius_px` (`72 * rmul`), `0` | ブロック内の爆雷セル数 (`>= 6`) | `0` |
| `EV_CATACLYSM` | 共鳴ブロック内の共鳴セル重心 (px) | `96.0f`, `0` | ブロック内の共鳴セル数 (`>= 8`) | `0` |
| `EV_HINT_TRIGGER` | 関連座標（なければ社の座標） | `0, 0` | `HintId` (`0..HINT_COUNT-1`) | `0` |
| `EV_WIND_TELEGRAPH` | 風上の辺の中央座標 (px) | 風向きの単位ベクトル `(dx, dy)` | 風向き番号（`0`:左→右、`1`:右→左、`2`:上→下、`3`:下→上） | `0` |
| `EV_REWARD_PICK` | `0, 0` | `0, 0` | 強化札は `UpgradeId`、大遺物は `RelicId`、社修繕は `-1` | 強化札=`0`、大遺物=`EVF_REWARD_RELIC`、修繕=`EVF_REWARD_REPAIR` |

---

### 2.3 `src/game/game.h`、`src/game/game_contract.c`、`src/game/debug_api.c`（凍結）

1. **`RunProgress` への大遺物配列追加**:
   `OwnedUpgrade upgrades[GAME_MAX_OWNED_UPGRADES]; int32_t upgrade_count;` の直後に次を追加する：
   ```c
   RelicId relics[GAME_MAX_OWNED_RELICS];
   int32_t relic_count;
   ```
2. **公開クエリ・デバッグ関数の追加**:
   ```c
   bool game_has_relic(const GameState *g, RelicId id);
   bool game_is_boss_enemy(EnemyDefId id);
   void game_debug_start_act(GameState *g, int32_t act_index);
   ```
   - `game_has_relic(g, id)`: `g != NULL && (unsigned)id < (unsigned)RELIC_COUNT` のとき、`g->run.relics[0..min(relic_count, GAME_MAX_OWNED_RELICS))` に `id` が含まれれば `true`。
   - `game_is_boss_enemy(id)`: `id == ENEMY_BOSS_TENGU || id == ENEMY_BOSS_NUE || id == ENEMY_BOSS_RYU` のとき `true`。
   - `game_debug_start_act(g, act_index)`（`debug_api.c`）: `g != NULL && act_index >= 0 && act_index < GAME_MAIN_ACT_COUNT` のとき、`g->run.act_index = act_index; g->run.wave_index = 0; g->internal.act_prepared = false; waves_prepare_act(g); g->phase = RUN_PHASE_ACT_TRANSITION; g->phase_t = 0.0f;` を設定する（範囲外は no-op）。`waves_prepare_act(g)` 内で旧幕の残存 ECS エンティティをすべてクリアした上で新幕の地形・霊柱を配置する（§2.7）。
3. **`game_contract.c` の `k_upgrade_meta[UPG_COUNT]`**:
   24 種すべての `axis` と `max_level = 3` を登録する（`UPG_FLAME_GALE` / `UPG_INFERNO` / `UPG_PHOENIX_ARROW` = `AXIS_BLAZE`、`UPG_THUNDER_ARROW` / `UPG_SWIFT_THUNDER` / `UPG_THUNDERBURST_FUSE` = `AXIS_THUNDER`、`UPG_MIGHTY_STROKE` / `UPG_MAELSTROM_BIND` / `UPG_TAILWIND` = `AXIS_GALE`、`UPG_INK_MAGNET` / `UPG_CHAIN_PRIMER` / `UPG_DRY_MASTERY` = `AXIS_INKWAY`、`UPG_SHRINE_WARD` / `UPG_DEEP_INK` / `UPG_DEEP_MARK` / `UPG_BARRIER` = `AXIS_COMMON`）。`game_run_result` は既に `res.cleared = g->run.acts_cleared >= GAME_MAIN_ACT_COUNT;` であり変更不要。

---

### 2.4 `src/game/game_internal.h` と `src/game/hash.c`（凍結）

P4-A と P4-B が `game_internal.h` や `hash.c` を編集して競合するのを防ぐため、`RunModifiers` と `GameInternal` の全フィールドを P4-Pre で確定し、`hash_run` / `hash_internal` にすべて組み込む。

- **`game_internal.h` の自己完結ヘッダ契約**: `maelstrom_bind_t[ECS_MAX_ENTITIES]` で `ECS_MAX_ENTITIES` を使うため、冒頭に `#include "ecs.h"` を追加する（`architecture.md` §1.2 の許容ヘッダ内）。

```c
#include "ecs.h"

#include <stdbool.h>
#include <stdint.h>

#define GAME_MAX_ACTIVE_CYCLONES 8
#define GAME_THUNDERBURST_BLOCKS_X 20
#define GAME_THUNDERBURST_BLOCKS_Y 12
#define GAME_THUNDERBURST_BLOCKS (GAME_THUNDERBURST_BLOCKS_X * GAME_THUNDERBURST_BLOCKS_Y)

typedef struct {
    float brush_radius_mul;
    float brush_force_mul;
    float stroke_cost_mul;
    float gale_impulse_mul;
    float gale_cost_add;
    float gale_fire_add;
    float gale_end_blast_dmg;     /* 0 = なし, >0 = 焔の風斬り Lv3 の終点爆炎基礎ダメ (20.0f) */
    float cyclone_storm_add;      /* 細筆 0.55f（帯電印の増分は on_cyclone で上乗せ） */
    float cyclone_storm_sustain;  /* 渦印持続中の毎ティック雷注入量（既定 0.0f） */
    float ink_max_add;
    float ink_regen_mul;
    bool ink_regen_no_delay;      /* P2 予備（硯の恵み Lv3、P1 既定 false） */
    float burn_rate_mul;
    float burn_base_dmg;          /* 既定 10.0f（業火で 14/18/22） */
    bool inferno_ember_on_kill;   /* 業火 Lv3 */
    float fire_dmg_mul;           /* 既定 1.0f（業火の契りで 2.0f） */
    int32_t chain_hops_add;
    float chain_range_mul;
    float cyclone_radius_mul;
    float cyclone_duration_add;
    int32_t maelstrom_bind_level; /* 0..3（渦縛り） */
    float cyclone_cd;             /* 既定 1.4f（細筆 1.1f） */
    float gale_cd;                /* 既定 1.0f */
    float surge_duration;         /* 既定 3.6f */
    float chain_grace;            /* 既定 2.65f（呼び水で 3.20/3.80/4.50） */
    float orb_pull_radius;        /* 既定 130.0f（吸墨 170/210/250、細筆 ×1.3） */
    bool cyclone_pulls_all_orbs;  /* 吸墨 Lv3 */
    float dry_force_override;     /* 0 = 既定(0.46f), >0 = かすれの妙の倍率 (0.75/1.00) */
    bool dry_zero_cost;           /* かすれの妙 Lv3 */
    float dry_deflect_dmg_mul;    /* 既定 1.0f（かすれの妙 Lv3 で 1.20f） */
    int32_t burning_proj_pierce;  /* 既定 2（不死鳥の矢で 3/4/5） */
    bool burning_proj_hit_splat;  /* 不死鳥の矢 Lv3 */
    int32_t thunder_proj_targets; /* 既定 1（雷撃弾で 2/3/3） */
    bool deflect_auto_electrify;  /* 雷撃弾 Lv3 */
    float storm_zap_cd;           /* 既定 0.54f（迅雷で 0.45/0.38/0.30） */
    float thunderburst_thresh;    /* 既定 0.30f（爆雷の導火で 0.26/0.22/0.18） */
    float thunderburst_radius_mul;/* 既定 1.0f（爆雷の導火 Lv3 で 1.30f） */
    float homing_steer_mul;       /* 既定 1.0f（追い風で 1.30/1.60/2.00） */
    float deflected_max_speed_mul;/* 既定 1.0f（追い風 Lv3 で 1.20f） */
    float ink_mark_mul;           /* 既定 2.0f（墨印深化で 2.20/2.40/2.60） */
    float ink_mark_duration;      /* 既定 3.0f（墨印深化 +1/+2/+3、残墨 +2） */
    int32_t shrine_max_hp_delta;  /* 社の護符 +1/+2/+3、業火の契り -2 */
    float shrine_invuln_add;      /* 結界 +0.30/+0.60/+0.90 */
    bool barrier_shield_enabled;  /* 結界 Lv3 */
    float stroke_ember_interval;  /* 0 = なし, >0 = 火種間隔(px) */
    float stroke_ember_mul;       /* 既定 1.0f（火種の筆 Lv3 で 1.5f） */
    float relic_ember_interval;   /* 朱の硯: 200.0f (0 = なし) */
    float ink_damp_override;      /* 既定 0.985f（朱の硯 -0.005f、残墨 +0.008f） */
    float fire_damp_override;     /* P2 予備（残り火、P1 既定 0.935f） */
    bool twin_brush;
    bool tengu_fan;
    bool thunder_drum;
    bool ink_dyed_shrine;
    bool rosary_swirl;
    bool wind_return;
} RunModifiers;
```

`GameInternal` には既存の P0 フィールドに加え、以下を定義順に持つ：
- **P4-A 用フィールド**:
  - `float relic_ember_dist;`（朱の硯の累積距離）
  - `float thunderburst_block_cd[GAME_THUNDERBURST_BLOCKS];`（8×8 ブロックごとの爆雷 CD 0.8s）
  - `float cataclysm_cd;`（三元共鳴の全体 CD 2.0s）
  - `float barrier_shield_cd;`（結界 Lv3 の 12s タイマー）
  - `bool barrier_shield_ready;`（結界 Lv3 の敵弾 1 発無効化チャージ）
  - `float wind_return_cd;`（返し風の 12s CD）
  - `uint32_t triggered_hints_mask;`（ラン内で発火済みの `HintId` ビットマスク）
  - `float maelstrom_bind_t[ECS_MAX_ENTITIES];`（渦縛りの拘束残り秒数）
- **P4-B 用フィールド**:
  - `float wind_timer;`（第 3 幕 山颪の周期タイマー）
  - `float wind_telegraph_t;`（山颪の予兆残り秒数）
  - `float wind_active_t;`（山颪の吹送残り秒数）
  - `int32_t wind_dir;`（山颪の風向き `0..3`）
  - `int32_t wave_elites_spawned;`（現ウェーブで出現したエリート累計）
  - `uint32_t last_elite_flags;`（直前に付与したエリート変異マスク。連続重複防止用）
  - `float nue_trail_x[2][2], nue_trail_y[2][2], nue_trail_t[2];`（鵺の雷帯 2 本分の始点・終点・残り秒数）
  - `float ryu_coil_t;`（一筆龍の龍巻き残り秒数）

- **`hash.c` の同期**:
  - `hash_run`: `upgrades` ハッシュの直後に `r->relic_count` と `r->relics[0..min(relic_count, GAME_MAX_OWNED_RELICS))` をハッシュする。
  - `hash_internal`: `RunModifiers` と `GameInternal` の上記全追加フィールドを定義順そのままにハッシュする（`thunderburst_block_cd[0..GAME_THUNDERBURST_BLOCKS)`、`maelstrom_bind_t[0..ECS_MAX_ENTITIES)`、`nue_trail_x/y/t` を含む）。

---

### 2.5 `src/game/game.c` の 3 幕進行・大遺物遷移・パイプライン配線（凍結）

`game.c` に `#include "roguelike/hints.h"` と `#include "roguelike/stage_gimmicks.h"` を追加し、以下を確定する：

1. **`game_init`**:
   - `g->run.act_count = GAME_MAIN_ACT_COUNT;`（3 幕）に変更する。
   - `roguelike_recompute_modifiers(g)` が `g->fluid.cfg.dye_damp[0..1]` と `g->shrines[0].max_hp`・`g->player.ink_max` を同期する（§2.6）。
2. **`handle_command`（段 1）**:
   - `CMD_PICK_REWARD`: `rewards_pick_reward(g, cmd->arg)` を呼ぶ（`REWARD_KIND_UPGRADE` と `REWARD_KIND_RELIC` の両方を受け付ける。既存の `rewards_pick_upgrade` は置換統合する）。
   - `CMD_REROLL_REWARD`: `rewards_reroll(g)` を呼ぶ（`REWARD_KIND_UPGRADE` と `REWARD_KIND_RELIC` の両方でその幕の `rerolls_left` を共有する）。
   - `CMD_REPAIR_SHRINE`: `rewards_repair_shrine(g)` を呼ぶ（強化札画面、または候補 0 件の `REWARD_KIND_FALLBACK` で有効）。
3. **ボス撃破時と `step_aftermath`（段 1 / 段 8）**:
   - 段 8 でボス撃破を検出したとき（`g->run.wave_index == GAME_BOSS_WAVE_INDEX && g->run.boss_id.index == ECS_INVALID_INDEX`）：
     - `++g->run.acts_cleared;`、`g->internal.wave_clear_is_boss = true;`、`EV_ACT_CLEAR` を発行し、`combat_begin_wave_aftermath(g)` へ入る。
     - **最終幕以外のボス撃破時**（`g->run.act_index + 1 < g->run.act_count`、すなわち第 1・2 幕ボス）：`game-design.md` §3.5 に従い、社の HP を `+3` 回復する：
       ```c
       if (g->run.act_index + 1 < g->run.act_count && g->shrine_count > 0) {
           ShrineState *sh = &g->shrines[0];
           sh->hp += 3;
           if (sh->hp > sh->max_hp) {
               sh->hp = sh->max_hp;
           }
       }
       ```
   - `step_aftermath` で 1.0 秒経過したとき：
     - `g->internal.wave_clear_is_boss` が真かつ `g->run.act_index + 1 >= g->run.act_count`（第 3 幕最終ボス撃破）なら、`RUN_PHASE_VICTORY` へ遷移し `EV_VICTORY`（`value = g->run.act_index`）を出す。
     - `g->internal.wave_clear_is_boss` が真かつ `g->run.act_index + 1 < g->run.act_count`（第 1・2 幕ボス撃破）なら、`rewards_generate_relic_offer(g)` を呼んで `RUN_PHASE_REWARD` へ遷移する。
     - 通常ウェーブクリアなら、`rewards_generate_offer(g)` を呼んで `RUN_PHASE_REWARD` へ遷移する。
4. **段 6 と段 7e の呼び出し**:
   - 段 6：`stage_gimmicks_step(g);` → `waves_step_spawns(g);`
   - 段 7e：`upgrades_fire_on_tick(g, INK_FIXED_DT);` → `hints_check_tick(g);`

---

### 2.6 P4-A へ移譲する骨格と先行配線（`relics.*`, `hints.*`, `upgrades.*`, `rewards.*`, `elements.c`）

- **`src/game/roguelike/relics.h` / `relics.c`**:
  - `relics.h` は `"roguelike/upgrades.h"` を include し、7 フックの引数構造体（`StrokeHookArgs`〜`TickHookArgs`）をそのまま共有する（`lv = 1` を渡す）。
  - 公開関数：
    ```c
    bool roguelike_grant_relic(GameState *g, RelicId id);
    void relics_apply_modifiers_add(const GameState *g, RunModifiers *mods);
    void relics_apply_modifiers_mul(const GameState *g, RunModifiers *mods);
    void relics_fire_on_stroke(GameState *g, float x, float y, float vx, float vy, float dist);
    void relics_fire_on_cyclone(GameState *g, float x, float y, float *storm_add,
                                float *storm_sustain_add);
    void relics_fire_on_gale(GameState *g, float x, float y, float dx, float dy);
    void relics_fire_on_hit(GameState *g, EntityId target, float *dmg_inout, uint32_t dmg_flags);
    void relics_fire_on_kill(GameState *g, EntityId victim, float x, float y, uint32_t dmg_flags);
    void relics_fire_on_reaction(GameState *g, ReactionKind kind, float x, float y, float intensity);
    void relics_fire_on_tick(GameState *g, float dt);
    ```
  - `roguelike_grant_relic(g, id)`: `g != NULL && (unsigned)id < (unsigned)RELIC_COUNT && !game_has_relic(g, id) && g->run.relic_count < GAME_MAX_OWNED_RELICS` なら `g->run.relics[g->run.relic_count++] = id;` を追加し、`roguelike_recompute_modifiers(g)` を呼んで `true` を返す。
  - Pre では `relics_apply_modifiers_*` と `relics_fire_on_*` の中身は no-op 骨格とし、P4-A が 9 種を本実装する。
- **`src/game/roguelike/upgrades.c`**:
  - `roguelike_recompute_modifiers(g)` で以下の **P0 互換既定値**（乗算係数が 0 にならないよう全フィールドを明示初期化）を起点にする：
    - 乗算系 `= 1.0f`: `brush_radius_mul`, `brush_force_mul`, `stroke_cost_mul`, `gale_impulse_mul`, `ink_regen_mul`, `burn_rate_mul`, `fire_dmg_mul`, `chain_range_mul`, `cyclone_radius_mul`, `dry_deflect_dmg_mul`, `thunderburst_radius_mul`, `homing_steer_mul`, `deflected_max_speed_mul`, `stroke_ember_mul`
    - 基礎定数系：`burn_base_dmg = 10.0f`, `cyclone_cd = 1.4f`, `gale_cd = 1.0f`, `surge_duration = 3.6f`, `chain_grace = 2.65f`, `orb_pull_radius = 130.0f`, `burning_proj_pierce = 2`, `thunder_proj_targets = 1`, `storm_zap_cd = 0.54f`, `thunderburst_thresh = 0.30f`, `ink_mark_mul = 2.0f`, `ink_mark_duration = 3.0f`, `ink_damp_override = 0.985f`, `fire_damp_override = 0.935f`
    - 加算/フラグ系は `0` / `false`
  - Pass 1（加算：`meta` → 所持札 → `relics_apply_modifiers_add`）→ Pass 2（乗算：筆スタイル → `meta` → 所持札 → `relics_apply_modifiers_mul`）の順で計算する。
  - 計算末尾で `g->fluid.cfg.dye_damp[0] = ink_clampf(mods.ink_damp_override, 0.90f, 0.999f);`、`g->fluid.cfg.dye_damp[1] = ink_clampf(mods.fire_damp_override, 0.90f, 0.999f);` を必ず同期する（ラン途中で大遺物を取得した瞬間にも即反映させるため）。
  - 7 つの `upgrades_fire_on_*` 関数の末尾でそれぞれ対応する `relics_fire_on_*` を呼ぶ（所持札 → 所持遺物の取得順実行を骨格段階で確定）。
- **`src/game/roguelike/hints.h` / `hints.c`**:
  - `void hints_check_tick(GameState *g);`（Pre では no-op、P4-A が 8 種の `HintId` 発火条件を実装）。
- **`src/game/roguelike/rewards.h` / `rewards.c`**:
  - `void rewards_generate_relic_offer(GameState *g);`（未所持の `RelicId` `0..RELIC_COUNT-1` から重複なしで最大 3 個を抽選し、`offer->kind = REWARD_KIND_RELIC`、`offer->repair_available = false` にする。候補 0 件なら `REWARD_KIND_FALLBACK`・`repair_available = true`）。
  - `bool rewards_pick_reward(GameState *g, int32_t choice_index);`（`rewards_pick_upgrade` を置換）：
    - `REWARD_KIND_UPGRADE`：札を付与 → `EV_REWARD_PICK`（`flags = 0`）→ `waves_start_wave(g, g->run.wave_index + 1)`。
    - `REWARD_KIND_RELIC`：遺物を付与 → `EV_REWARD_PICK`（`value = (int32_t)relic_id`, `flags = EVF_REWARD_RELIC`）→ `reward` をクリアし `g->internal.wave_clear_is_boss = false;` の上、次幕 `g->run.act_index++`, `g->run.wave_index = 0`, `g->internal.act_prepared = false`, `g->phase = RUN_PHASE_ACT_TRANSITION`, `g->phase_t = 0.0f` へ遷移する。
  - `rewards_reroll(g)`: `REWARD_KIND_UPGRADE` と `REWARD_KIND_RELIC` の両方を許可し、種別に応じた `rewards_generate_*` を呼ぶ。
  - `rewards_repair_shrine(g)`: ボス後の `REWARD_KIND_FALLBACK`（`g->internal.wave_clear_is_boss` が真）で呼ばれた場合は、`waves_start_wave(g, wave_index + 1)` ではなく上と同じ次幕 `RUN_PHASE_ACT_TRANSITION` へ遷移する（`wave_index + 1 == 4` の範囲外スタックを防ぐ）。
- **`src/game/elements/elements.c`（P4-B 単独通過のための先行配線 → P4-A へ移譲）**:
  - `elements_stage4_enemies`:
    - 墨印付与を `if ((en->elite_flags & ELITE_INK_REPEL) != 0) { st->ink_mark_t = 0.0f; } else if (ink > 0.28f) { st->ink_mark_t = g->internal.mods.ink_mark_duration; }` にする。
    - 炎上間隔を `game_is_boss_enemy(en->def_id) ? 0.55f : 0.36f`、炎上基礎ダメを `g->internal.mods.burn_base_dmg` にする。
    - 雷墨連鎖の初撃・ホップ候補から `(en->elite_flags & ELITE_STORMPROOF) != 0` を除外し、CD に `g->internal.mods.storm_zap_cd` を使う。
  - `elements_stage5_projectiles`:
    - 炎上弾の貫通初期値に `g->internal.mods.burning_proj_pierce` を使い、最高速クランプに `640.0f * (proj->deflected ? g->internal.mods.deflected_max_speed_mul : 1.0f)` を使う。

---

### 2.7 P4-B へ移譲する骨格と先行配線（`stage_gimmicks.*`, `waves.*`, `enemy_defs.*`, `damage.c`, `projectiles.c`, `autoplay.c`）

- **`src/game/roguelike/stage_gimmicks.h` / `stage_gimmicks.c`**:
  - `void stage_gimmicks_setup_act(GameState *g, int32_t act_index);`（`waves_prepare_act` から呼ぶ。Pre では `fluid_clear_solids(&g->fluid)` のみ、P4-B が第 2 幕の鳥居/岩柱・第 3 幕の岩柱/霊柱を配置）。
  - `void stage_gimmicks_step(GameState *g);`（段 6 から呼ぶ。Pre では no-op、P4-B が山颪・霊柱・落雷予兆・鎌鼬逆回転旋風を実装）。
- **`src/game/roguelike/waves.c`**:
  - `k_act1_def` を `k_act_defs[GAME_MAIN_ACT_COUNT]` に拡張する（Pre では Act 2・3 に Act 1 の表を暫定配置し、P4-B が `game-design.md` §3.3・§3.4 の本表とエリート変異抽選に置き換える）。
  - `waves_prepare_act(g)`: `fluid_reset(&g->fluid)` の前に旧幕の全残存 ECS エンティティ（`COMP_ENEMY | COMP_PROJ | COMP_INK_ORB | COMP_GIMMICK`）を破棄して `ecs_flush_destroy(&g->ecs)` し、`g->run.boss_id = {ECS_INVALID_INDEX, 0}`・ボス/ギミック系タイマー（`boss_vortex_t`, `ryu_coil_t`, `nue_trail_t[]`, `wind_telegraph_t`, `wind_active_t`）をリセットしてから `stage_gimmicks_setup_act(g, g->run.act_index)` を呼ぶ。
  - `waves_start_wave(g, wave_index)`: 既存の `COMP_ENEMY`・`COMP_PROJ` に加え、`gim->kind != GIMMICK_SPIRIT_PILLAR` の `COMP_GIMMICK` も破棄し（霊柱は幕内で保持）、`wave_elites_spawned = 0`・`last_elite_flags = 0`・`nue_trail_t[] = 0`・`ryu_coil_t = 0` をリセットする。
  - `pick_spawn_position` / `spawn_wave_enemy` の `def_id == ENEMY_BOSS_TENGU` を `game_is_boss_enemy(def_id)` に統一し、`EV_BOSS_ENTER` の `.value = (int32_t)def_id` にする。
- **`src/game/enemies/enemy_defs.c`**:
  - 新規 6 種（`ENEMY_LANTERN_STORM`, `ENEMY_KAMAITACHI`, `ENEMY_STONE_ONI`, `ENEMY_INK_LEECH`, `ENEMY_BOSS_NUE`, `ENEMY_BOSS_RYU`）のエントリ枠を追加する（Pre では基礎 HP・半径・質量・流体結合・点・墨玉数を `game-design.md` §8.1 / §8.6 / §8.7 どおりに入れ、AI 関数は暫定 stub にする）。
  - `enemy_spawn` で `game_is_boss_enemy(def_id)` が真なら `COMP_BOSS` を付与し、`g->run.boss_id = id` と `g->internal.last_boss_x/y = x/y` を設定する。
- **`src/game/combat/damage.c`（P4-A 単独通過のための先行配線 → P4-B へ移譲）**:
  - 既存の `ENEMY_BOSS_TENGU` 直書き 7 か所（`kill_enemy` のボス座標・`wave_kills`・死亡飛沫半径、`combat_apply_damage_f` の墨印倍率・雷耐性、`combat_physics_and_slams_step` の敵衝突半径）を `game_is_boss_enemy(...)` に統一する（大天狗固有の第 2 フェーズ移行分岐のみ `ENEMY_BOSS_TENGU` を維持）。
  - P0 ハードコード定数を `g->internal.mods` 参照へ置換し、P4-A の効果フラグを先行配線する（既定値では P0 と 1 ビットも変わらない）：
    - `combat_apply_damage_f`: `upgrades_fire_on_hit` の前後で `(dmg_flags & DMG_FIRE) != 0` に `mods.fire_dmg_mul`、`g->player.dry && (dmg_flags & DMG_DEFLECTED) != 0` に `mods.dry_deflect_dmg_mul`、墨印倍率に `game_is_boss_enemy(en->def_id) ? (1.5f * (mods.ink_mark_mul * 0.5f)) : mods.ink_mark_mul` を適用。
    - `kill_enemy`: `g->run.chain_t = g->internal.mods.chain_grace;`。
    - `combat_damage_shrine`: `sh->invuln_t = 0.95f + g->internal.mods.shrine_invuln_add;`。
    - `combat_orbs_step`: 吸引半径に `g->internal.mods.orb_pull_radius` を使用。通常回収時（自動回収を除く）に `if (g->internal.mods.rosary_swirl)` なら回収位置へ半径 `6.0f`・強さ `36.0f`・墨 `0.25f` の `fluid_swirl` を注入。
    - `combat_begin_wave_aftermath`: `gim->kind != GIMMICK_SPIRIT_PILLAR` の `COMP_GIMMICK`（落雷予兆・鎌鼬旋風）を即破棄し、ウェーブ終了後の遅延被弾を防ぐ。
- **`src/game/enemies/projectiles.c`（P4-A 単独通過のための先行配線 → P4-B へ移譲）**:
  - `ENEMY_BOSS_TENGU`（追尾優先・命中半径）を `game_is_boss_enemy(...)` に、`PROJ_STORM_ORB`（偏向時帯電・飛沫・軌跡・雷撃バウンス）を `(proj->kind == PROJ_STORM_ORB || proj->kind == PROJ_BOUNCE_ORB)` に拡張する。
  - `g->internal.mods` と結界 Lv3 を先行配線する：
    - 偏向成立時：`if (g->internal.mods.deflect_auto_electrify) { proj->electrified = true; }`。
    - 追尾加速度：`(proj->burning ? 840.0f : 700.0f) * g->internal.mods.homing_steer_mul`。
    - 最高速：`proj->deflected` のとき `base_max_sp * g->internal.mods.deflected_max_speed_mul`。
    - 社衝突（`!proj->deflected && dist < shrine_proj_r`）：`if (g->internal.barrier_shield_ready)` ならダメージ 0 で弾を消滅させ `g->internal.barrier_shield_ready = false; g->internal.barrier_shield_cd = 12.0f;` にする。
    - 炎上弾命中時：`if (proj->burning && g->internal.mods.burning_proj_hit_splat)` なら命中点へ半径 `3.2f`・炎 `0.65f` の `fluid_splat` を注入。
    - `trigger_electrified_bounce`: 最大 `g->internal.mods.thunder_proj_targets`（既定 1、上限 8）体の近い別敵へ順に `EV_STORM_ZAP`＋`10` 雷ダメージを飛ばす（`(en->elite_flags & ELITE_STORMPROOF) != 0` は除外）。
- **`src/game/autoplay.c`（最小 2 か所 → P4-B へ移譲）**:
  - `game_autoplay_input` の `ENEMY_BOSS_TENGU` を `game_is_boss_enemy(en->def_id)` に、`game_autoplay_command` の `REWARD_KIND_UPGRADE` 条件を `(kind == REWARD_KIND_UPGRADE || kind == REWARD_KIND_RELIC)` に拡張し、スモーク実行が大遺物画面で停滞しないようにする。

---

### 2.8 `src/render/ui_view.h`（凍結）と `src/render/text_table.*`（P4-C へ移譲）

1. **`src/render/ui_view.h`（凍結・raylib 非依存）**:
   P4-C が実装するロビー・筆スタイル選択・墨庵・設定・ポーズ・初回ヒント・見本帳 6 ページに必要な型を確定する。

```c
#ifndef INK_UI_VIEW_H
#define INK_UI_VIEW_H

#include "game_ids.h"
#include "meta.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    UI_SCENE_RUN = 0,
    UI_SCENE_SHOWCASE,
    UI_SCENE_TITLE_LOBBY,
    UI_SCENE_BRUSH_SELECT,
    UI_SCENE_BOKUAN,
    UI_SCENE_SETTINGS
} UiScene;

typedef enum {
    PAUSE_CONFIRM_NONE = 0,
    PAUSE_CONFIRM_RESTART,
    PAUSE_CONFIRM_LOBBY
} PauseConfirmKind;

#define UI_HOVER_NONE (-1)
#define UI_HOVER_REPAIR 3
#define UI_HOVER_REROLL 4

typedef struct {
    UiScene scene;
    UiScene settings_return_scene;
    int32_t hover_choice;
    bool xray;
    uint8_t shake_scale;
    bool settled;
    MetaSettleBreakdown settle;
    int32_t crystals_after;
    int32_t showcase_page;
    int32_t showcase_subpage;

    /* Phase 4 additions: Lobby / Brush Select / Bokuan / Settings / Pause / Hints */
    BrushStyleId selected_brush;
    int32_t lobby_cursor;
    int32_t shop_cursor;
    int32_t settings_cursor;
    bool paused;
    int32_t pause_cursor;
    PauseConfirmKind pause_confirm;
    int32_t confirm_cursor;
    HintId active_hint;
    float active_hint_t;
    MetaState meta_view;
} UiView;

#endif
```

2. **`src/render/text_table.h` / `text_table.c`（P4-C へ移譲）**:
   - 既存の `tests/test_text_table.c` は `0..ENEMY_DEF_COUNT-1` の全敵と `0..UPG_COUNT-1` の全札（Lv1〜3）について `!= TEXT_NONE` を検証し、かつ `text_act_title(1) == TEXT_NONE` を検証している。
   - したがって P4-Pre では `TEXT_P1_STUB`（`{.ja = "準備中", .en = "TBD"}`）を 1 件追加し、`text_enemy_name` / `text_enemy_intro` の新規 6 敵と `text_upgrade_name` / `text_upgrade_effect` の新規 16 札（`level 1..3` のみ）が `TEXT_P1_STUB` を返すようにする（範囲外 ID と `level < 1 || level > 3`、および `text_act_title(1..2)` は `TEXT_NONE` を維持）。
   - P4-C が本文字列（全 10 敵・全 24 札・全 15 遺物・第 2/3 幕等）を登録するときに `TEXT_P1_STUB` を削除し、`tests/test_text_table.c` を更新する。

---

## 3. テストターゲットの先行登録

### 3.1 `CMakeLists.txt` への `test_acts_enemies` 追加と既存テストの整合
`if(BUILD_TESTING)` ブロックの末尾（`endif()` の直前）に以下を追記する：
```cmake
    add_executable(test_acts_enemies tests/test_acts_enemies.c)
    target_link_libraries(test_acts_enemies PRIVATE unity ink_game ink_core m)
    add_test(NAME test_acts_enemies COMMAND test_acts_enemies)
```
- `tests/test_acts_enemies.c`（Pre の骨格）：
  - `game_create(NULL)` で `run.act_count == 3` であること、第 1 幕ボス撃破 → `WAVE_AFTERMATH` 1.0 秒後に `RUN_PHASE_REWARD` かつ `reward.kind == REWARD_KIND_RELIC` になり、社の HP が `+3`（上限 `max_hp`）回復していること、`CMD_PICK_REWARD 0` で `relic_count == 1` かつ `act_index == 1` の `RUN_PHASE_ACT_TRANSITION` に遷移することを確認する。
- `tests/test_determinism_run.c`（Pre の最小整合）：
  - 既存の P0 単幕テスト（`test_full_run_determinism_sanity_and_settlement`）は第 1 幕ボス撃破で `RUN_PHASE_VICTORY`・`from_acts == 30` を期待しているため、Pre では当該テスト冒頭で `a->run.act_count = 1; b->run.act_count = 1;` を設定して既存検証を維持し、P4-B で全 3 幕通し（`act_count == 3`, `from_acts == 120`, `cleared == true`）へ拡張する。

### 3.2 `tests/test_core_smoke.c` の更新（凍結）
- 全 24 種の `UpgradeId` に対し `game_upgrade_max_level(id) == 3`、`game_upgrade_axis(id) < AXIS_COUNT`、範囲外は `0`。
- `game_has_relic(g, RELIC_VERMILION_INKSTONE) == false`（範囲外 `RELIC_COUNT` も `false`）、`game_is_boss_enemy(ENEMY_BOSS_TENGU / NUE / RYU) == true`、通常敵 7 種と `ENEMY_DEF_COUNT` は `false`。
- `game_debug_start_act(g, 1)` で `act_index == 1` かつ `RUN_PHASE_ACT_TRANSITION` になり、範囲外（`-1`, `3`）は no-op であること。

---

## 4. 設計書の同期

- `docs/PLAN.md` §9.1 の Phase 4 行から、`phase4-pre-contracts.md`、`phase4-roguelike-p1.md`、`phase4-acts-enemies-p1.md`、`phase4-ui-assets-p1.md`、`phase4-integration.md` の 5 ファイルへリンクを張る。
- `docs/architecture.md` §1.2（`ui_view.h` → `game_ids.h`, `meta.h`）、§3.3（`game_debug_start_act`）、§5.3（`RunProgress.relics` / `relic_count`）、§6.3、§7.1（`EVF_REWARD_RELIC` および 4 イベントのペイロード表）を同期する。
- `docs/game-design.md` §4.3 / §9.4 / §9.5 / §12 に、P1 で実装する強化札 24 種（各軸 5 種＋共通 4 種）と初期大遺物 9 種の内訳、および第 2・3 幕の確定地形セル座標（左鳥居中心 `x=384`、右鳥居中心 `x=896` 等）を同期する。
- `docs/qa-and-testing.md` §2 のテスト一覧に `test_acts_enemies`（Phase 4：全 3 幕の地形 `solid`・山颪・霊柱・新敵 4 種・エリート変異 4 種・鵺・一筆龍簡易版）を追加する。

---

## 5. ローカル検証と完了条件

`CMakeLists.txt` と共有公開ヘッダを更新するため、ローカルでフルの ASan/UBSan + CTest（計 15 件）および `format-check` / `lint` を 1 回実行する。

```sh
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan &&
    nix develop -c ctest --preset asan --output-on-failure &&
    nix develop -c cmake --build --preset asan --target format-check lint

nix develop -c cmake --preset release && nix develop -c cmake --build --preset release &&
    ./build/release/inkstorm --headless-smoke &&
    nix develop -c ./build/release/inkstorm --smoke-frames 5 &&
    nix develop -c ./build/release/inkstorm --no-post --smoke-frames 5

find src -name '*.h' -printf '%f\n' | sort | uniq -d
```

**完了条件（Definition of Done）**
- [ ] §2 の全契約ヘッダ・`game_contract.c`・`game_internal.h`・`hash.c`・`game.c`・`debug_api.c`・`ui_view.h` と骨格・先行配線ファイルが揃い、ヘッダ名の重複がない。
- [ ] CTest 全 15 件（`test_acts_enemies`、`test_text_table`、`test_determinism_run` を含む）が ASan/UBSan（`-Werror`）で通り、`format-check` と `lint` が指摘ゼロ。
- [ ] Release の `--headless-smoke` と `--smoke-frames 5`（通常 / `--no-post`）が終了コード 0。
- [ ] PR の CI 3 ジョブがすべて緑。マージ後、`main` から 3 つの並列 worktree（`../wt-roguelike-p1`, `../wt-acts-enemies-p1`, `../wt-ui-assets-p1`）を作成できる。
