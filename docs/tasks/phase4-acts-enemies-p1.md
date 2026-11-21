# Phase 4-B タスク指示書：P1 全 3 幕ステージ・ギミック・新敵 4 種・エリート・鵺・一筆龍簡易版（`wt-acts-enemies-p1`）

> **必読**: [phase4-pre-contracts.md](phase4-pre-contracts.md)（特に §0.1 の進め方、§1.2 の排他所有表、§2 の凍結契約）、[PLAN.md](../PLAN.md) §9.1、[architecture.md](../architecture.md) §3〜§6、[game-design.md](../game-design.md) §3 / §4 / §6 / §8 / §12、[qa-and-testing.md](../qa-and-testing.md) §2 / §3、[AGENTS.md](../../AGENTS.md)、[inkstorm-fluid-sim](../../.agents/skills/inkstorm-fluid-sim/SKILL.md)、[inkstorm-content-authoring](../../.agents/skills/inkstorm-content-authoring/SKILL.md)
>
> **worktree / ブランチ**: `full-scale-dev/wt-acts-enemies-p1` / `feat/phase4-acts-enemies-p1`（P4-Pre マージ後の `main` から分岐）。完了したら単独で `main` へ PR を出す。
> **並列ペア**: P4-A（[phase4-roguelike-p1.md](phase4-roguelike-p1.md)）、P4-C（[phase4-ui-assets-p1.md](phase4-ui-assets-p1.md)）。合流と完了ゲートは P4-Int（[phase4-integration.md](phase4-integration.md)）。

---

## 0. 目的と責務境界

P1 本編のステージ空間（全 3 幕のウェーブ表・`solid` 障害物・山颪・霊柱）、新通常敵 4 種と敵弾 2 種、エリート変異 4 種、第 2 幕ボス「鵺」および第 3 幕ボス「一筆龍（簡易版）」を実装し、全 3 幕の決定性と Release 性能予算（`game_step < 4.0 ms/tick`）を実証する。

1. **第 2 幕『鳥居と岩柱』・第 3 幕『嵐の霊峰』の地形とギミック**（game-design §4.2 / §4.3）：
   - 第 2 幕：鳥居 2 基（`2×2` セルの柱 × 4 本、柱間 6 セル隙間）＋岩柱 4 本（半径 3 セル円柱）。
   - 第 3 幕：岩柱 2 本＋霊柱 3 本（半径 2 セル `solid` 円柱、炎/雷 `> 0.4` で 6 秒点灯・0.5 秒ごとに半径 4 セルへ `0.5` 注入）＋山颪（12 秒周期・一筆龍点睛後で 6 秒周期、1.5 秒前に `EV_WIND_TELEGRAPH` 予告、3 秒間風上 20 セル帯を `96px/s` へ加速）。
2. **新通常敵 4 種＋新敵弾 2 種**（game-design §8.1 / §8.2）：
   - 提灯妖・雷（`ENEMY_LANTERN_STORM`）、鎌鼬（`ENEMY_KAMAITACHI`＋鎌風 `PROJ_WIND_SICKLE`＋逆回転旋風 `GIMMICK_KAMAITACHI_VORTEX`）、石鬼（`ENEMY_STONE_ONI`、重量級叩きつけ 80 ダメージ＋スタン 1.0 秒、社接触 2 ダメージ＋1.2 秒後退）、墨吸い（`ENEMY_INK_LEECH`、墨吸引と HP 回復・炎上最大 ×2・撃破時濃墨放出）。
3. **エリート変異 4 種**（game-design §8.3）：
   - 第 2 幕 8%（同時 1・ウェーブ最大 2）、第 3 幕 12%（同時 2・ウェーブ最大 3）。HP ×2.5、半径 ×1.2、墨玉 5、スコア ×3。剛体（`ELITE_RIGID`）・撥墨（`ELITE_INK_REPEL`）・不燃（`ELITE_FIREPROOF`）・避雷（`ELITE_STORMPROOF`）。
4. **第 2 幕ボス：鵺（`ENEMY_BOSS_NUE`）＋跳弾雷球（`PROJ_BOUNCE_ORB`）**（game-design §8.6）：
   - HP 700。0.5 秒予兆 → 600px/s ダッシュ（雷帯を残す）→ 1.5 秒停止（停止中のみ叩きつけ有効：60 ダメージ＋スタン 1.0 秒）。跳弾雷球 3 連射、鎌鼬 2 体召喚（12 秒 CD）、HP 50% 以下で第 2 フェーズ（雷帯 2 本＋夜鳴きの放射衝撃波）。
5. **第 3 幕ボス：一筆龍・簡易版（`ENEMY_BOSS_RYU`）**（game-design §8.7 簡易版 P1）：
   - 単体（HP 1500）、叩きつけ無効。上半分 S 字遊泳＋胴の微風注入。HP 100%〜66%（炎玉攻撃）、66%〜33%（雷玉攻撃：落雷予兆と墨避雷を含む）、33% 以下で点睛（`EV_BOSS_PHASE`・山颪周期 6 秒化・風玉攻撃＋雷火の息＋龍巻き＋召喚・三元共鳴ダメージ ×2）。

**禁止事項**
- P4-Pre §1.2 の凍結ファイル、および P4-A / P4-C の所有ファイルを編集しない（`elements.c` 側の `ELITE_INK_REPEL`・`ELITE_STORMPROOF`・`game_is_boss_enemy` は P4-Pre で先行配線済み）。
- `raylib.h` の include、60Hz ループ内の `malloc`/`free`、可変 `static` 変数、`rand()`/`time()` の使用は厳禁。
- 一筆龍の多部位 ECS エンティティ版や、狐符、エンドレスモードは Phase 5（P2）の範囲であり、本タスクでは実装しない。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/game/roguelike/stage_gimmicks.h` / `stage_gimmicks.c` | 更新 | 第 2・3 幕の `solid` 地形配置、霊柱エンティティ生成と点灯/注入、山颪の予兆/吹送、`GIMMICK_LIGHTNING_STRIKE` と `GIMMICK_KAMAITACHI_VORTEX` の更新（§2.1） |
| `src/game/roguelike/waves.h` / `waves.c` | 更新 | 全 3 幕（計 12 ウェーブ）の編成・増援重み・種類別同時上限・ボスウェーブ護衛数判定・エリート変異抽選（§2.2 / §2.5） |
| `src/game/combat/damage.h` / `damage.c` | 更新 | エリート耐性（剛体・不燃・避雷・複合属性の低耐性規則）・エリート撃破時の墨玉 5 ＆スコア ×3・`ENEMY_FLAG_SUMMONED` 非計上・提灯妖雷と墨吸いの死亡飛沫・墨吸いの炎上倍率・石鬼の社接触 2 ダメージ＆後退・`solid` 接線スライド・石鬼と停止中の鵺の叩きつけスタン・鵺/一筆龍のフェーズ移行・一筆龍点睛後の三元共鳴 ×2（Pre 配線済みの `mods.*` / `rosary_swirl` は維持）（§2.3〜§2.7） |
| `src/game/enemies/enemy_defs.h` / `enemy_defs.c` | 更新 | 全 10 種の `EnemyDef` 本登録、エリート変異スポーン初期化、撥墨エリートの周囲墨散らし（§2.3 / §2.5） |
| `src/game/enemies/archer.c`、`umbrella.c`、`boss_tengu.c` | 必要時更新 | `solid` 障害物のある第 2・3 幕での移動・スライド整合 |
| `src/game/enemies/lantern.c` | 更新 | `lantern_storm_spawn` / `lantern_storm_step`（提灯妖・雷）を追加（§2.3） |
| `src/game/enemies/kamaitachi.c` | 新規 | 鎌鼬のジグザグ外周移動・`GIMMICK_KAMAITACHI_VORTEX` 生成・鎌風発射（§2.3） |
| `src/game/enemies/stone_oni.c` | 新規 | 石鬼の社への前進・`ai_state == 1` の 1.2 秒後退（§2.3） |
| `src/game/enemies/ink_leech.c` | 新規 | 墨吸いの濃墨追従・半径 64px 毎秒 30% 墨吸引・HP 回復・膨張（§2.3） |
| `src/game/enemies/boss_nue.c` | 新規 | 第 2 幕ボス「鵺」と雷帯持続注入（§2.6） |
| `src/game/enemies/boss_ryu.c` | 新規 | 第 3 幕ボス「一筆龍（簡易版）」と落雷予兆生成・龍巻き（§2.7） |
| `src/game/enemies/projectiles.h` / `projectiles.c` | 更新 | 鎌風（`PROJ_WIND_SICKLE`）、跳弾雷球（`PROJ_BOUNCE_ORB` の壁/`solid` 最大 2 回反射・反射後の自己偏向防止）（Pre 配線済みの `barrier_shield_ready` / `mods.*` は維持）（§2.4） |
| `src/game/autoplay.c` | 更新 | 第 2・3 幕の地形・霊柱・新敵・鵺・一筆龍・大遺物報酬に対応した自動入力の拡張（§2.8） |
| `tests/test_acts_enemies.c` | 更新 | 全 3 幕の地形・山颪・霊柱・新敵 4 種・新弾 2 種・エリート 4 種・鵺・一筆龍の単体テスト（§3.1） |
| `tests/test_determinism_run.c` | 更新 | 全 3 幕通し（大天狗→大遺物→鵺→大遺物→一筆龍→VICTORY）の決定性と健全性テスト（§3.2） |
| `tests/bench_game.c` | 更新 | `solid` 障害物・属性反応・多数の敵弾が稼働する実戦闘負荷での `< 4.0 ms/tick` ベンチマーク（§3.3） |
| `docs/game-design.md` | 数値のみ | バランス調整で定義テーブルの数値を変えた場合の同期 |

---

## 2. 実装仕様

### 2.1 第 2 幕・第 3 幕の地形座標とステージギミック（`stage_gimmicks.c`、game-design §4.2 / §4.3）

すべての `solid` 障害物は次の不変条件を満たすよう固定座標で配置する：
- 社 `(640, 360)` から `120px` 以内に `solid` を置かない。
- 砲台型（弓妖・提灯妖）の外周巡回帯（画面端から `76〜165px`）を塞がない。
- `solid` 同士の隙間を **6 セル（`48px`）以上** 空け、凸形状のみとして袋小路を作らない。

#### (1) 第 1 幕『開けた和紙の社』（`act_index == 0`）
- `fluid_clear_solids(&g->fluid);` のみ（内部 `solid` なし）。

#### (2) 第 2 幕『鳥居と岩柱』（`act_index == 1`）
`fluid_clear_solids(&g->fluid);` の後、以下を配置する：
- **鳥居 2 基**（社の左右 `±256px` → 左鳥居 中心 `x = 384px` `[376, 392)`、右鳥居 中心 `x = 896px` `[888, 904)`。それぞれ `2×2` セル＝`16×16 px` の柱 2 本、柱間に縦 6 セル＝`48px` の隙間 `[336, 384)` を空けて隙間を社へ向ける。笠木は描画のみで `solid` にしない）：
  - 左鳥居・上柱：`fluid_set_solid_rect(f, 376.0f, 320.0f, 16.0f, 16.0f, true)`（セル `x = 47..48, y = 40..41`、中心 `(384, 328)`）
  - 左鳥居・下柱：`fluid_set_solid_rect(f, 376.0f, 384.0f, 16.0f, 16.0f, true)`（セル `x = 47..48, y = 48..49`、中心 `(384, 392)`）
  - 右鳥居・上柱：`fluid_set_solid_rect(f, 888.0f, 320.0f, 16.0f, 16.0f, true)`（セル `x = 111..112, y = 40..41`、中心 `(896, 328)`）
  - 右鳥居・下柱：`fluid_set_solid_rect(f, 888.0f, 384.0f, 16.0f, 16.0f, true)`（セル `x = 111..112, y = 48..49`、中心 `(896, 392)`）
- **岩柱 4 本**（社から `(±300px, ±170px)`、半径 3 セル＝`24.0f px`）：
  - 左上：`fluid_set_solid_circle(f, 340.0f, 190.0f, 24.0f, true)`
  - 右上：`fluid_set_solid_circle(f, 940.0f, 190.0f, 24.0f, true)`
  - 左下：`fluid_set_solid_circle(f, 340.0f, 530.0f, 24.0f, true)`
  - 右下：`fluid_set_solid_circle(f, 940.0f, 530.0f, 24.0f, true)`

#### (3) 第 3 幕『嵐の霊峰』（`act_index == 2`）
`fluid_clear_solids(&g->fluid);` の後、以下を配置する：
- **岩柱 2 本**（第 2 幕の上側 2 本と同位置、半径 3 セル＝`24.0f px`）：
  - `(340.0f, 190.0f)`、`(940.0f, 190.0f)`
- **霊柱 3 本**（社 `(640, 360)` を中心に半径 `230px` の正三角形の頂点：上 `-90°`・左下 `+150°`・右下 `+30°`、半径 2 セル＝`16.0f px` の `solid` 円柱）：
  - 上：`Fluid solid` 中心 `(640.0f, 130.0f)`、半径 `16.0f`
  - 左下：`Fluid solid` 中心 `(441.0f, 475.0f)`、半径 `16.0f`
  - 右下：`Fluid solid` 中心 `(839.0f, 475.0f)`、半径 `16.0f`
  - 各霊柱は `COMP_TRANSFORM | COMP_GIMMICK` を持つ ECS エンティティとしても生成する（`tr->x, tr->y, tr->radius = 16.0f`、`gim->kind = GIMMICK_SPIRIT_PILLAR, gim->elem_kind = GIMMICK_ELEM_NONE, gim->timer = 0.0f, gim->param_a = 0.0f`）。`waves_start_wave` では `gim->kind != GIMMICK_SPIRIT_PILLAR` だけを破棄し、霊柱は幕を通じて保持する。
- **霊柱の点灯・注入ロジック（段 6 `stage_gimmicks_step`）**:
  - 各霊柱の中心から距離 `(16.0f, 28.0f]` px にある非 `solid` 表面隣接セルの `fire`（ch1）と `storm`（ch2）の最大値 `max_fire`, `max_storm` を調べる。
  - `max_fire > 0.40f || max_storm > 0.40f` のとき：
    - `incoming = (max_fire >= max_storm && max_fire > 0.40f) ? GIMMICK_ELEM_FIRE : GIMMICK_ELEM_STORM`。
    - 消灯中または別属性からの切り替え時は、パルス間隔タイマー `gim->param_a = 0.50f` に初期化し、直ちに初回の `0.5f` 注入を行う。同属性の継続浴びせ時は `gim->timer = 6.0f` に戻す。
    - `gim->elem_kind = incoming; gim->timer = 6.0f;`
  - 点灯中（`gim->timer > 0.0f`）：
    - `gim->timer -= INK_FIXED_DT; gim->param_a -= INK_FIXED_DT;`
    - `gim->param_a <= 1e-4f` ごとに `gim->param_a += 0.50f` し、半径 `4.0f セル`（`32.0f px`）へその属性を `0.50f` 注入する（`fluid_splat(&g->fluid, tr->x, tr->y, 4.0f, 0.0f, 0.0f, dye)`）。
    - `gim->timer <= 1e-4f` になったら `gim->timer = 0.0f; gim->elem_kind = GIMMICK_ELEM_NONE; gim->param_a = 0.0f;` に戻す。
- **一時ギミックの更新（段 6 `stage_gimmicks_step`）**:
  - `GIMMICK_KAMAITACHI_VORTEX`：毎ティック `fluid_swirl(&g->fluid, tr->x, tr->y, 10.0f, -42.0f, NULL)` を注入し、`gim->timer -= INK_FIXED_DT` で `<= 1e-4f` になったら `ecs_destroy` する。
  - `GIMMICK_LIGHTNING_STRIKE`：`gim->timer -= INK_FIXED_DT` し、`<= 1e-4f` で §2.7 の避雷・社被弾判定を実行して `ecs_destroy` する（ボス撃破後も独立して安全に後始末される）。
- **山颪（環境風）のロジック（段 6 `stage_gimmicks_step`）**:
  - 第 3 幕（`g->run.act_index == 2`）でのみ動作する。
  - 周期 `period` は通常 `12.0f` 秒、一筆龍が点睛後（`boss->phase_idx >= 1`）の間は `6.0f` 秒（game-design §8.7）。待機中に `period` が短縮された場合は `if (g->internal.wind_timer > period - 4.5f) { g->internal.wind_timer = period - 4.5f; }` で即座に追従する。
  - 幕開始時に `g->internal.wind_timer = period - 1.5f`、`wind_telegraph_t = 0.0f`、`wind_active_t = 0.0f` に初期化する。
  - 通常待機中（`wind_telegraph_t <= 0 && wind_active_t <= 0`）：`wind_timer -= INK_FIXED_DT`。`wind_timer <= 1e-4f` になったら `g->rng` で 4 方向 `dir = (int32_t)(ink_rng_next(&g->rng) % 4)`（`0`:左端から右向き `+x`、`1`:右端から左向き `-x`、`2`:上端から下向き `+y`、`3`:下端から上向き `-y`）を選び、`wind_dir = dir`、`wind_telegraph_t = 1.5f` に設定して `EV_WIND_TELEGRAPH` を発行する。
  - 予兆中（`wind_telegraph_t > 0`）：`wind_telegraph_t -= INK_FIXED_DT`。`<= 1e-4f` で `wind_telegraph_t = 0.0f; wind_active_t = 3.0f;` へ移行。
  - 吹送中（`wind_active_t > 0`）：`wind_active_t -= INK_FIXED_DT`。風上の辺から幅 **20 セル** の **非 `solid` セルのみ**（`g->fluid.solid[i] == 0` 必須：`dir==0`: `x=1..20`, `dir==1`: `x=139..158`, `dir==2`: `y=1..20`, `dir==3`: `y=69..88`）に対し、流速を `96.0f px/s = 12.0f セル/s` へ近づける力（`u[i] = ink_clampf(u[i] + (target_u - u[i]) * 0.22f, -max_vel, max_vel)`、`v[i]` 同様）を毎ティック加える。3.0 秒終了時に `wind_active_t = 0.0f; wind_timer = period - 4.5f;` に戻す。

---

### 2.2 全 3 幕のウェーブ表と増援重み（`waves.c`、game-design §3.3 / §3.4）

`game-design.md` §3.3 の全 12 ウェーブ（第 1〜3 幕 × 4 ウェーブ）と §3.4 の増援重み・種類別同時上限を `k_act_defs[3]` に定義する：
- 第 3 幕の狐符枠は `game-design.md` §8.1 に従い **鎌鼬** に置き換える：
  - 3-2 初期編成：`弓妖 2・唐傘 1・鎌鼬 2・石鬼 1`
  - 第 3 幕増援重み：`弓妖 20 / 炎提灯 12 / 雷提灯 12 / 唐傘 16 / 鎌鼬 20 (12+8) / 石鬼 10 / 墨吸い 10`（計 100）
- **種類ごとの同時上限**（game-design §3.4）：
  - 鎌鼬：同時上限 `2`（第 2・3 幕共通）
  - 石鬼：第 2 幕は同時上限 `1`、第 3 幕は同時上限 `2`（第 2 幕では第 2 ウェーブから出現）
  - 墨吸い：同時上限 `2`（第 3 幕）
  - 同時上限に達している敵種別は、そのティックの増援抽選プールから重みを 0 として除外する。
- **ボスウェーブの護衛同時上限（4 体）**: `wave_index == GAME_BOSS_WAVE_INDEX` では `!game_is_boss_enemy(en->def_id)`（ボスを除いた護衛の生存数）で `max_alive = 4` を判定し、`waves_step_spawns` とボス召喚（鵺・一筆龍）で同じ上限を共有する。
- ボスが召喚した護衛には `en->elite_flags |= ENEMY_FLAG_SUMMONED;` を付与し、`kill_enemy` で `(en->elite_flags & ENEMY_FLAG_SUMMONED) != 0` の個体は通常ウェーブのノルマ撃破数 `run.wave_kills` に数えない（累計撃破 `run.kills` には数える、game-design §3.2）。

---

### 2.3 新通常敵 4 種の仕様（game-design §8.1）

| 敵 | `EnemyDef` 基本値 | 行動・`EnemyComp` 利用規約・流体相互作用・特殊規則 |
| :--- | :--- | :--- |
| **提灯妖・雷** (`ENEMY_LANTERN_STORM`) | HP `20`, 半径 `17.0f`, `MASS_TURRET`, 結合 `0.0f`, 点 `140`, 墨玉 `2` | 提灯妖・炎と同じ外周巡回と射撃間隔（`1.55〜2.25s`、前 `0.35s` で `windup`）。社へ雷球（`PROJ_STORM_ORB`, `210px/s`）を撃つ。出現時に墨 `0.42`・雷 `0.35`（半径 `3.6 セル`）、撃破時（`damage.c`）に墨 `1.05`・雷 `1.35`（半径 `6.2 セル`）を撒く |
| **鎌鼬** (`ENEMY_KAMAITACHI`) | HP `30`, 半径 `16.0f`, `MASS_MEDIUM`, 結合 `1.5f`, 点 `160`, 墨玉 `1` | 外周（社から距離 `190〜290px`）を `160px/s` でジグザグに周回する。`en->fire_cd`（`2.5s` 周期）で社へ鎌風（`PROJ_WIND_SICKLE`, `300px/s`）を撃つ。`en->absorbed_ink` を逆回転旋風 CD（`4.0s` 周期）として使い、稼働中の渦印または最も墨の濃い非 `solid` 領域の位置に `GIMMICK_KAMAITACHI_VORTEX`（`timer = 1.0f`、§2.1）を生成してプレイヤーの渦を打ち消す |
| **石鬼** (`ENEMY_STONE_ONI`) | HP `90`, 半径 `24.0f`, `MASS_HEAVY`, 結合 `0.3f`, 点 `300`, 墨玉 `3` | `en->ai_state == 0` で社へ `40px/s` で歩く（`damage.c` で `solid` 衝突時は法線接線方向へ滑らせ、正面衝突時は決定的接線バイアスで詰まりを防ぐ）。社接触時（`damage.c`）は社へ **2 ダメージ** を与え、社外へ押し出した上で `en->ai_state = 1; en->fire_cd = 1.2f;` にし、`1.2s` 間 `55px/s` で後退してから `ai_state = 0` へ戻す。吹き飛び中に壁/`solid`/他敵へ叩きつけられると **80 ダメージ＋スタン 1.0 秒**（`st->stun_t = fmaxf(st->stun_t, 1.0f)`）。※注：風斬り単発の `×0.35`（豪筆 `245px/s`・朱筆/細筆 `196px/s`）と叩きつけ閾値 `215px/s` の関係は、単体テストでは豪筆または爆雷/三元共鳴/直設定速度で検証し、朱筆・細筆での体感は P4-Int §2.4 で確認する |
| **墨吸い** (`ENEMY_INK_LEECH`) | HP `40`, 半径 `19.0f`, `MASS_MEDIUM`, 結合 `1.5f`, 点 `220`, 墨玉 `3` | 周囲（前後左右 `48px`）の墨濃度勾配の濃い方向（墨がなければ社方向）へ `60px/s` で漂う。毎ティック半径 `64px`（`8 セル`）以内の非 `solid` セルの墨（ch0）を **毎秒 30%**（`× (1.0f - 0.30f * INK_FIXED_DT)`）吸い取り、吸い取った墨量の合計（セル平均×係数）を `en->absorbed_ink` と回復端数カウンタ `en->fire_cd` に加算する。`en->fire_cd >= 1.0f` ごとに `en->fire_cd -= 1.0f` し HP を `5` 回復（`max_hp` 上限）し、`tr->radius = 19.0f + fminf(10.0f, en->absorbed_ink * 2.5f)` で膨張する。**炎上ダメージ倍率**（`damage.c` で `DMG_FIRE` 適用時）が `1.0f + fminf(1.0f, en->absorbed_ink * 0.25f)`（最大 `×2.0`）に増え、撃破時に `0.8f + fminf(1.7f, en->absorbed_ink * 0.6f)` の濃墨（半径 `8.5 セル`）を一気に放出する |

---

### 2.4 新敵弾 2 種の仕様（`projectiles.c`、game-design §8.2）

1. **鎌風（`PROJ_WIND_SICKLE`）**:
   - 初速 `300px/s`、社ダメージ `1`、流体の影響係数 `vel->fluid_coupling = 1.5f`（`projectiles_spawn` で `kind == PROJ_WIND_SICKLE` のとき設定）、偏向後の基礎ダメージ `20`（物理）。
2. **跳弾雷球（`PROJ_BOUNCE_ORB`）**:
   - 初速 `230px/s`、社ダメージ `1`、流体の影響係数 `1.0f`、生成時に `proj->bounces_left = 2`、軌跡に雷（`PROJ_STORM_ORB` と同じ）を残す。偏向後の基礎ダメージ `20`＋雷撃弾の追加雷撃効果。
   - **反射規則**: 移動前の位置 `(prev_x, prev_y)` を保存し、移動後に外周の壁（`x < FLUID_CELL_PX || x >= INK_SCREEN_W - FLUID_CELL_PX || y < FLUID_CELL_PX || y >= INK_SCREEN_H - FLUID_CELL_PX`）または内部 `solid`（`fluid_is_solid(&g->fluid, tr->x, tr->y)`）に当たったとき、`proj->bounces_left > 0` であれば消滅せず、`proj->bounces_left--` し、外周壁法線または `fluid_solid_normal` の `(nx, ny)` で速度 `(vx, vy)` を鏡面反射（`vn = vx*nx + vy*ny; if (vn < 0) { vx -= 2*vn*nx; vy -= 2*vn*ny; }`）させて直前位置へ押し戻す。`bounces_left == 0` の状態で壁または `solid` に当たったら消滅する。
   - **反射後の自己偏向防止（重要）**: 跳弾雷球が壁や柱で反射した直後は `toward_center < 0.28f` になり得るため、未偏向の `PROJ_BOUNCE_ORB` の偏向判定は流体の実押し出し（`fsp > 48.0f && toward_center < 0.90f`）を必須とし、流体なしでの `toward_center < 0.28f` による誤偏向を防ぐ（風斬りによる直接偏向は `cast_gale` 側で成立）。

---

### 2.5 エリート変異 4 種（game-design §8.3）

1. **出現抽選（`waves.c` の増援スポーン時）**:
   - 初期編成には適用せず、通常ウェーブ（`wave_index < GAME_BOSS_WAVE_INDEX`）の増援スポーン時のみ抽選する。
   - 第 2 幕：確率 `8%`、同時生存エリート上限 `1` 体、1 ウェーブ累計上限 `2` 体（`g->internal.wave_elites_spawned`）。
   - 第 3 幕：確率 `12%`、同時生存エリート上限 `2` 体、1 ウェーブ累計上限 `3` 体。
2. **共通強化と変異の割り当て**:
   - `en->is_elite = true`、`hp->max_hp = hp->hp = (int32_t)floorf((float)def->base_hp * 2.5f + 0.5f)`、`tr->radius = def->radius * 1.2f`。撃破時（`damage.c` の `kill_enemy`）は墨玉 `5` 個、基礎スコア `def->score * 3`。
   - 変異の数：第 2 幕は **1 つ**、第 3 幕は **1〜2 つ**（50% で 1 つ、50% で重複なしの 2 つ）。
   - 候補フラグ：`ELITE_RIGID`（砲台型 `MASS_TURRET` には付与しない）、`ELITE_INK_REPEL`、`ELITE_FIREPROOF`、`ELITE_STORMPROOF`。同じウェーブで直前のエリート（`g->internal.last_elite_flags`）と完全同一の `elite_flags & ELITE_ALL_MASK` が連続しないよう抽選する。
3. **各変異の効果**:
   - **剛体（`ELITE_RIGID`）**: 生成時に `vel->mass_tier` を 1 段重くする（`MASS_LIGHT → MASS_MEDIUM`、`MASS_MEDIUM → MASS_HEAVY`）。叩きつけ（`DMG_SLAM`）による被ダメージを `×0.5`（-50%）にする。
   - **撥墨（`ELITE_INK_REPEL`）**: `elements.c`（P4-Pre 先行配線済み）および三元共鳴で墨印が付かない。また `enemies_ai_step`（`enemy_defs.c`）で毎ティック自身の周囲半径 `48px`（`6 セル`）以内の非 `solid` セルの墨（ch0）を毎秒 `20%`（`× (1.0f - 0.20f * INK_FIXED_DT)`）減衰させる。
   - **不燃（`ELITE_FIREPROOF`）**: 炎分類（`DMG_FIRE`）のダメージを `×0.3`（-70%）にする。
   - **避雷（`ELITE_STORMPROOF`）**: 雷分類（`DMG_STORM`）のダメージを `×0.3`（-70%）にする。また雷墨連鎖（`elements.c`、P4-Pre 先行配線済み）および雷撃弾バウンス（`projectiles.c`、P4-Pre 先行配線済み）の対象・中継点から除外する。
   - **複合属性（`DMG_FIRE | DMG_STORM`：爆雷・三元共鳴）の耐性規則（`damage.c`）**:
     - `game-design.md` §6.4「低い方の耐性を適用する」に従い、`m_fire = (en->elite_flags & ELITE_FIREPROOF) ? 0.3f : 1.0f`、`m_storm = (en->elite_flags & ELITE_STORMPROOF) ? 0.3f : (is_boss ? 0.5f : 1.0f)` とする。
     - `DMG_FIRE` と `DMG_STORM` の両方を持つ場合は `fmaxf(m_fire, m_storm)` を乗算する（例：ボス本体は `fmaxf(1.0f, 0.5f) = 1.0f`、不燃のみ持つエリートは `fmaxf(0.3f, 1.0f) = 1.0f`、不燃＋避雷の両方を持つエリートは `0.3f`）。片方のみの場合は該当倍率を乗算する。
     - さらに一筆龍が点睛後（`en->def_id == ENEMY_BOSS_RYU && boss->phase_idx >= 1`）かつ `(dmg_flags & DMG_CATACLYSM) != 0` のとき、耐性適用後に `dmg *= 2.0f` を乗算する（三元共鳴本体 `80 → 160`、同落雷 `30 * 0.5 * 2.0 = 30`）。

---

### 2.6 第 2 幕ボス：鵺（`boss_nue.c`、game-design §8.6）

- **基本値**: `ENEMY_BOSS_NUE`、HP `700`、半径 `40.0f`、`MASS_BOSS`、点 `1500`、墨玉 `8`。
- **コンポーネント共有契約（P4-A / P4-C と一致させる）**:
  - `en->ai_state`: `0` = ダッシュ予兆（`0.5s`）、`1` = **停止・攻撃中（`1.5s`、叩きつけ有効）**、`2` = ダッシュ中（`600px/s`）。
  - `en->phase`: 目標ウェイポイント番号（`0..7`）。
  - `boss->gale_dx, boss->gale_dy`: ダッシュ予兆中・ダッシュ中の進行単位ベクトル（P4-C の雷光予兆線描画と共有）。
  - `boss->pattern_timer`: 「群れ呼び」12 秒 CD。`en->fire_cd`: 現在の状態タイマー。
- **移動サイクル（ダッシュ＆停止）**:
  - 鳥居と岩柱の間の 6〜8 個のウェイポイント（`solid` と社から離れた座標）を持つ。
  - **予兆状態（`ai_state == 0`、`0.5s`）**: 次のウェイポイントへの進路単位ベクトルを `boss->gale_dx/dy` に入れ、`boss->telegraph_pattern = boss->pattern; boss->telegraph = 0..1` を進める。
  - **ダッシュ状態（`ai_state == 2`）**: `600px/s` で目標ウェイポイントへ直進する（パターン A「雷走り」：ダッシュ経路の始点・終点を `g->internal.nue_trail` に登録し、`2.0s` の間、毎ティック経路上のサンプル点へ雷 `0.6f`・半径 `2.8 セル` を注入し続ける。第 2 フェーズでは直近 2 本の雷帯を同時に維持する）。
  - **停止・攻撃状態（`ai_state == 1`、`1.5s`）**:
    - **停止中のみ叩きつけ有効**: `damage.c` の `combat_physics_and_slams_step` で `en->def_id == ENEMY_BOSS_NUE && en->ai_state == 1 && st->knockback_t > 0.0f` の個体は `MASS_BOSS` 除外を通過させ、速度統合と壁/`solid` 叩きつけ（215px/s 超）を判定する。叩きつけ成立時は **60 ダメージ（`DMG_PHYSICAL | DMG_SLAM`）＋スタン 1.0 秒（`st->stun_t = fmaxf(st->stun_t, 1.0f)`）** を与える。ダッシュ中（`ai_state != 1`）は吹き飛び・叩きつけ無効。
    - パターン B「跳弾雷球」：停止中に社へ向けて `PROJ_BOUNCE_ORB`（`230px/s`）を 3 発扇状（`±0.28 rad` 程度）または連射する（射撃前 `0.6s` 以上の予兆）。
    - パターン C「群れ呼び」：12 秒に 1 回まで（`boss->pattern_timer <= 0`）、ボス除き護衛数 `< 4` なら鎌鼬（`ENEMY_FLAG_SUMMONED` 付与）を最大 2 体召喚する。
- **第 2 フェーズ（HP 50% 以下：`hp->hp * 2 <= hp->max_hp`）**:
  - `damage.c` で `boss->phase_idx = 0 → 1`、`hp->invuln_t = 1.5f`、`EV_BOSS_PHASE`（`value = 2`）を発行する。
  - 雷帯を 2 本同時維持し、ダッシュ到着後にパターン D「夜鳴き」を追加：`0.6s` の予兆の後、鵺の位置から半径 `200px`（`25 セル`）に外向きの衝撃波（`fluid_add_radial_impulse(&g->fluid, tr->x, tr->y, 25.0f, 58.0f)`）を放ち、周囲の渦と墨を吹き散らす。

---

### 2.7 第 3 幕ボス：一筆龍・簡易版（`boss_ryu.c`、game-design §8.7 簡易版 P1）

- **基本値**: `ENEMY_BOSS_RYU`、単体 HP `1500`、頭部の当たり判定半径 `46.0f`、`MASS_BOSS`、叩きつけ常時無効、点 `1500`、墨玉 `8`。スポーン位置は上の霊柱 `(640, 130)` を避けた `(260.0f, 150.0f)`（または `(640.0f, 96.0f)`）。
- **遊泳と胴の微風**:
  - 頭部は画面上半分（`y = 96〜510px`、社から `200px` 以内に入らない、ただし「龍巻き」中を除く）を滑らかな S 字軌道で泳ぐ（点睛後は遊泳速度 `×1.3`）。
  - 移動方向に沿って毎ティック弱い風（速度 `16 セル/s`・半径 `4.2 セル`・清墨 `0.06f`）を流体へ注入し、プレイヤーの墨をかき混ぜる。
- **HP 区間による龍玉属性と点睛（簡易版 P1）**:
  - 現在の属性は `boss->part_id` に保持して描画側（P4-C）へ公開する：
    - `HP > 1000`（`> 66%`）：`part_id = 0`（炎玉）、`phase_idx = 0`
    - `500 < HP <= 1000`（`33% < HP <= 66%`）：`part_id = 1`（雷玉）、`phase_idx = 0`
    - `HP <= 500`（`<= 33%`）：`part_id = 2`（風玉）＋**点睛移行**（`damage.c` で `boss->phase_idx == 0 → 1`、`hp->invuln_t = 1.5f`、`EV_BOSS_PHASE` `value = 2` を発行）
  - 攻撃間隔は基本 `2.2s`（点睛後は `×0.8`、攻撃前 `0.6s` 以上は `boss->telegraph` を `0 → 1` に進める）：
    - **炎玉（`part_id == 0`）**: 扇状の火球 5 発（`205px/s`）と、社方向への炎の息（直線 8 点に炎 `0.45f`・速度 `38 セル/s` を `0.7s` 注入）。
    - **雷玉（`part_id == 1`）**: 雷球 3 発（`215px/s`）を撃ち、社の周囲 3 か所（うち 1 か所は社の当たり判定内 `(sx + offset_x, sy + offset_y)`、距離 `<= 28px`）に **落雷予兆エンティティ**（`COMP_TRANSFORM | COMP_GIMMICK`、`gim->kind = GIMMICK_LIGHTNING_STRIKE, gim->timer = 0.8f`）を置く。`0.8s` 経過時に `stage_gimmicks_step` が落雷点から半径 `40px` 以内の非 `solid` セルの最大墨濃度を調べ、**墨 `> 0.30f` があれば避雷成功**（社は無傷で、落雷点に雷 `1.1f`・半径 `4.0 セル` を注入し最も近い生存中の敵へ `EV_STORM_ZAP`＋`20` 雷ダメージを走らせる）。墨 `<= 0.30f` で落雷点が社の半径 `32px` 以内なら `combat_damage_shrine(g, lx, ly, 1)` で社に 1 ダメージを与えて破棄する。
    - **点睛後（`phase_idx >= 1`、`part_id == 2`）**: 簡易版 P1 では点睛と同時に第 2・3 フェーズを統合し、遊泳速度 `×1.3`、攻撃間隔 `×0.8`、山颪周期 `6.0s`、かつ **`DMG_CATACLYSM` 被ダメージ `×2.0`**（§2.5）を有効化し、以下をローテーションする：
      1. **風玉の大逆風＋鎌風 3 発**（`telegraph_pattern = 0`）：`1.5s` の間、社へ向けた強い逆風を注入し、`PROJ_WIND_SICKLE` を 3 発撃つ。
      2. **雷火の息**（`telegraph_pattern = 1`）：火球 3 発と雷球 3 発を交互に並べた扇状弾幕を放つ（軌跡の交差部で爆雷が誘発される）。
      3. **龍巻き（`telegraph_pattern = 3`、15 秒に 1 回まで）**：`0.8s` の予兆（P4-C が `telegraph_pattern == 3` で社周囲半径 `240px` の朱破線円を描画）の後、`2.0s`（`g->internal.ryu_coil_t = 2.0f`）の間、社 `(sx, sy)` を中心とする半径 `240px`（`30 セル`）に渦印と逆回りの流れ（`fluid_swirl` 強さ `-44.0 セル/s`）を毎ティック注入する。
      4. **召喚（10 秒ごと）**：ボス除き護衛数 `< 4` なら、石鬼 1 体または墨吸い 2 体（いずれも `ENEMY_FLAG_SUMMONED` 付与）を召喚する。

### 2.8 自動入力（`src/game/autoplay.c`）の 3 幕対応
- `game_autoplay_input` / `game_autoplay_command` は、第 2・3 幕の `solid` 障害物を避けつつ社周辺の敵弾偏向（新弾含む）・渦印・風斬り（唐傘・石鬼・停止中の鵺）を実行し、大遺物報酬（`REWARD_KIND_RELIC`）と `REWARD_KIND_FALLBACK` でも安全にコマンドを返せるようにする。
- `test_determinism_run` の全 3 幕通しおよび `--headless-smoke` / `--smoke-frames` でスタックせず決定的に進行することを確認する。

---

## 3. テスト・ベンチ要件

### 3.1 `tests/test_acts_enemies.c`
1. **第 2・3 幕の地形 `solid` 不変条件**:
   - `stage_gimmicks_setup_act(g, 1)`（第 2 幕）と `(g, 2)`（第 3 幕）のそれぞれで、社 `(640, 360)` から半径 `120px` 以内に `fluid_is_solid` が真のセルが 1 つもないこと、鳥居柱（`(384, 328)`・`(384, 392)`・`(896, 328)`・`(896, 392)`）と岩柱（`(340, 190)` 等）・霊柱（`(640, 130)` 等）の中心が `solid` であり、左鳥居の隙間中心 `(384, 360)` と右鳥居の隙間中心 `(896, 360)` が非 `solid` であること。
2. **第 3 幕の霊柱と山颪**:
   - 霊柱の表面隣接セルに炎 `0.5` を書くと `GIMMICK_ELEM_FIRE` で点灯（`timer == 6.0f`）し、0.5 秒（30 ティック）ごとに周囲の炎が増え、点灯中に雷 `0.6` を浴びせると `GIMMICK_ELEM_STORM` に切り替わって `timer == 6.0f` に戻ること。
   - 第 3 幕で時間を進めると `EV_WIND_TELEGRAPH` が発火し、その 1.5 秒（90 ティック）後から 3.0 秒間、風上 20 セル帯に風向きの流速が生じ、`fluid_validate_finite(&g->fluid)` が常に真であること。
3. **新通常敵 4 種と新敵弾 2 種**:
   - 提灯妖・雷が `PROJ_STORM_ORB` を撃ち撃破時に雷を撒くこと。
   - 鎌鼬が `PROJ_WIND_SICKLE` を撃ち `GIMMICK_KAMAITACHI_VORTEX` で逆回転の渦を起こすこと。
   - 石鬼が社接触で 2 ダメージを与えて後退し、壁/`solid` への叩きつけで 80 ダメージ＋スタン 1.0 秒を受けること。
   - 墨吸いが周囲の墨を吸って HP を回復・半径が膨張し、炎上ダメージが増えて撃破時に濃墨を放出すること。
   - 跳弾雷球（`PROJ_BOUNCE_ORB`）が `solid` や外周壁で 2 回まで反射（反射直後に流体なしで自己偏向しないこと）し、3 回目の衝突で消滅すること。
4. **エリート変異 4 種**:
   - エリート個体の HP が `×2.5`、撃破時の墨玉が `5`、スコアが `×3` であること。
   - 剛体（質量 +1 段、叩きつけダメージ -50%）、撥墨（墨印無効、周囲の墨減衰）、不燃（炎ダメージ -70%）、避雷（雷ダメージ -70%、雷墨連鎖対象外）、および `DMG_FIRE | DMG_STORM` に対する「低い方の耐性」適用を検証する。
5. **鵺と一筆龍（簡易版）**:
   - 鵺：ダッシュ中は叩きつけ無効、停止中（`ai_state == 1`）に `solid` へ叩きつけると 60 ダメージ＋スタン 1.0 秒が入ること、ダッシュ経路に雷が注入されること、HP 50% 以下で `EV_BOSS_PHASE` が出ること。
   - 一筆龍（簡易版）：HP 1500、66% 以下で雷玉（`part_id == 1`）に移行し落雷予兆ギミック（墨 `> 0.3` で避雷成功・墨なしで社被弾）が動くこと、33% 以下で点睛（`EV_BOSS_PHASE`、`part_id == 2`、`phase_idx == 1`）し `combat_apply_damage(g, ryu, 80, DMG_FIRE | DMG_STORM | DMG_CATACLYSM, 0, 0)` が `160` ダメージ（`×2.0`）になること。

### 3.2 `tests/test_determinism_run.c`
- **全 3 幕通し決定性シナリオ**:
  - 2 インスタンスで第 1 幕（3 ウェーブ＋大天狗）→ 社 HP +3 ＆大遺物選択 → 第 2 幕（幕転換＋3 ウェーブ＋鵺）→ 社 HP +3 ＆大遺物選択 → 第 3 幕（幕転換＋3 ウェーブ＋一筆龍）→ `RUN_PHASE_VICTORY` まで通しで実行し、毎ティックの `game_hash_sections` 全 7 セクションが完全一致すること。
  - `meta_settle_run` で `acts_cleared == 3`、`cleared == true`、`from_acts == 120`（30 + 40 + 50）が精算されること。
- 既存の複数ティックイベント消費・128 件イベント上限・不正入力・容量 512 満杯テストもすべて維持する。

### 3.3 `tests/bench_game.c`（Release 性能ベンチ）
- `solid` 障害物がある第 2 幕または第 3 幕のボス戦（および敵 16 体＋弾 48 発の追加負荷行）で 120 ウォームアップ＋600 計測ティック × 5 試行を回し、試行中央値の mean / p95 が **`< 4.0 ms/tick`** であることを確認する。

---

## 4. ローカル検証・PR と完了条件

```sh
# 1. ASan/UBSan: 変更範囲のビルドと関連テスト
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan --target ink_game inkstorm \
        test_acts_enemies test_determinism_run test_elements test_roguelike_meta test_core_smoke bench_game &&
    nix develop -c ctest --preset asan --output-on-failure \
        -R '^(test_acts_enemies|test_determinism_run|test_elements|test_roguelike_meta|test_core_smoke|test_cli_headless_smoke)$'

# 2. Release ベンチマーク
nix develop -c cmake --preset release &&
    nix develop -c cmake --build --preset release --target bench_game bench_fluid &&
    ./build/release/bench_game && ./build/release/bench_fluid

# 3. 境界と排他所有の確認（いずれも 0 件・空）
grep -rn 'raylib.h\|rand(\|time(' src/game
grep -rnE 'fluid(\.|->)(u0|v0|div|p|curl|dye0)\b' src/game
git diff --stat main -- CMakeLists.txt CMakePresets.json src/core src/fluid src/ecs \
    src/game/meta.h src/game/meta.c src/game/game.h src/game/game_ids.h src/game/game_events.h \
    src/game/game_components.h src/game/game_contract.c src/game/game_internal.h \
    src/game/game.c src/game/hash.c src/game/debug_api.c src/game/player src/game/elements \
    src/game/roguelike/upgrades.c src/game/roguelike/upgrades.h src/game/roguelike/relics.c \
    src/game/roguelike/relics.h src/game/roguelike/rewards.c src/game/roguelike/rewards.h \
    src/game/roguelike/hints.c src/game/roguelike/hints.h tests/test_core_smoke.c \
    tests/test_fluid.c tests/bench_fluid.c tests/test_ecs.c tests/test_app_save.c \
    tests/test_app_core.c tests/test_audio.c tests/test_text_table.c tests/test_elements.c \
    tests/test_roguelike_meta.c src/render src/audio src/app assets tools .github \
    flake.nix flake.lock .agents docs/PLAN.md docs/architecture.md docs/art-direction.md \
    docs/qa-and-testing.md README.txt AGENTS.md
```

**完了条件（Definition of Done）**
- [ ] §2 の仕様（第 2・3 幕の地形とギミック、新通常敵 4 種、新敵弾 2 種、エリート 4 種、鵺、一筆龍簡易版）をすべて満たし、凍結ファイルおよび P4-A / P4-C の担当ファイルに差分がない。
- [ ] `test_acts_enemies`、`test_determinism_run`（全 3 幕通し）、`test_core_smoke`、`test_cli_headless_smoke` が ASan/UBSan（`-Werror`）で通る。
- [ ] `bench_game` が Release で 5 試行のハッシュ一致・有限性を満たし、mean / p95 ともに `< 4.0 ms/tick`。
- [ ] PR の CI 3 ジョブがすべて緑。
