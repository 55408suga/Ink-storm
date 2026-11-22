# Phase 5-B タスク指示書：P2 狐符・エンドレス周回スケール・一筆龍多部位版（`wt-endless-ryu-p2`）

> **必読**: [phase5-pre-contracts.md](phase5-pre-contracts.md)（特に §0.1 の進め方、§1.2 の排他所有表、§2 の凍結契約）、[PLAN.md](../PLAN.md) §9.1、[architecture.md](../architecture.md) §3〜§6、[game-design.md](../game-design.md) §3.3〜§3.6 / §6.4 / §8.1〜§8.7、[qa-and-testing.md](../qa-and-testing.md) §2 / §3、[AGENTS.md](../../AGENTS.md)、[inkstorm-content-authoring](../../.agents/skills/inkstorm-content-authoring/SKILL.md)、[inkstorm-fluid-sim](../../.agents/skills/inkstorm-fluid-sim/SKILL.md)、[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)
>
> **worktree / ブランチ**: `full-scale-dev/wt-endless-ryu-p2` / `feat/phase5-endless-ryu-p2`（P5-Pre マージ後の `main` から分岐）。完了したら単独で `main` へ PR を出す。
> **並列ペア**: P5-A（[phase5-roguelike-p2.md](phase5-roguelike-p2.md)）、P5-C（[phase5-ui-settings-p2.md](phase5-ui-settings-p2.md)）。合流と完了ゲートは P5-Int（[phase5-integration.md](phase5-integration.md)）。

---

## 0. 目的と責務境界

P2 拡張の戦闘コンテンツ・高難度エンドレスループ・最終ボス多部位化を完成させ、エンドレス周回を含む全決定性と Release 性能予算（`game_step < 4.0 ms/tick`）を実証する。

1. **狐符（`ENEMY_OFUDA_FOX`）と追尾符（`PROJ_HOMING_OFUDA`）**（game-design §3.3 / §3.4 / §8.1 / §8.2 / §8.3）：
   - 3-2 初期編成および第 3 幕増援表への正式編入（P1 の鎌鼬代替を解除）、同時上限 1、エリート対象外。
   - 外周維持 AI、6 秒ごとの味方 1 体への護符結界（`StatusComp.barrier_t = 4.0f`、被ダメ -50%）、3 秒ごとの追尾符発射（140px/s、流体結合 0.6、社へ毎秒 90°旋回、炎 > 0.30 で焼失）。
2. **エンドレスモード（第 4 幕以降の周回スケール）**（game-design §3.6）：
   - 周回数 `n = act_index / 3`（上限 `ENDLESS_MAX_LOOP_N = 100`）、地形・ウェーブ編成・ボス・山颪は `act_index % 3` で第 1 → 2 → 3 幕を循環。
   - 通常敵 HP `×(1 + 0.25n)`、ボス（および龍玉）HP `×(1 + 0.35n)`、ノルマ `+2n`、増援間隔 `×0.95^n`（下限 0.6s → その上で `spawn_interval_mul` 適用）、エリート出現率 `+4n%`（上限 30%）、ボス攻撃間隔 `×0.9^n`（下限 ×0.7、`>= tele_dur + 0.10f` ガード付き）。
3. **一筆龍の多部位 ECS 版**（game-design §8.7、architecture §5.2）：
   - 龍の頭部（親 `ENEMY_BOSS_RYU`：龍睛 HP 900）＋ 3 つの龍玉（子 `ENEMY_BOSS_RYU_ORB`：炎玉・雷玉・風玉 各 HP 300、合計 HP 1800）を独立 ECS エンティティとして表現。
   - `GameState.ryu_body` の 128 点リングバッファで頭部軌跡を保持し、弧長 `0px / 450px / 900px` の位置へ 3 龍玉を毎ティック追従配置。
   - 3 フェーズ進行：**第 1 フェーズ「三つの龍玉」**（頭部無敵＆標的除外・龍玉が 2.2s ごとに交代攻撃・破壊ごとに残り龍玉の攻撃間隔 `×0.85`・`EV_BOSS_PART_BREAK` 発行）→ **第 2 フェーズ「点睛」**（3 玉全破壊で 1.5s 点睛演出・`EV_BOSS_PHASE value=2`・龍睛弱点化・速度 ×1.3・山颪 6s・雷火の息/龍巻き/召喚）→ **第 3 フェーズ「昇龍」**（龍睛 HP 40% 以下・`EV_BOSS_PHASE value=3`・攻撃間隔 ×0.8・**三元共鳴ダメージ ×2**）。

**禁止事項**
- P5-Pre §1.2 の凍結ファイル、および P5-A / P5-C の所有ファイルを編集しない（`elements.c` 側の `DMG_REACTION`・`game_enemy_is_targetable`・`game_is_boss_part`・未偏向追尾符ガードは P5-Pre で先行配線済み）。
- `raylib.h` の include、60Hz ループ内の `malloc`/`free`、可変 `static` 変数、`rand()`/`time()` の使用は厳禁。
- 既存の P1 第 1〜3 幕の決定性・性能予算（`< 4.0 ms/tick`）を劣化させない。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/game/enemies/ofuda_fox.c` | 更新 | 狐符の AI 本実装（外周維持、6s 護符結界、3s 追尾符発射）（§2.1） |
| `src/game/enemies/projectiles.h` / `projectiles.c` | 更新 | `PROJ_HOMING_OFUDA`（結合 0.6、最低速 140px/s 例外、毎秒 90°社旋回、炎 > 0.30 焼失）、Pre 先行配線（`game_enemy_is_targetable`・`ENEMY_BOSS_RYU_ORB` 半径 36px/優先 ×0.65）の維持（§2.1） |
| `src/game/enemies/enemy_defs.h` / `enemy_defs.c` | 更新 | エンドレス周回 HP スケール（通常 `×(1+0.25n)`、ボス/龍玉 `×(1+0.35n)`、エリート `×2.5` と合成）、`ENEMY_OFUDA_FOX` エリート除外維持（§2.1 / §2.2） |
| `src/game/roguelike/waves.h` / `waves.c` | 更新 | 3-2 初期編成＆第 3 幕増援表への狐符復帰（同時上限 1・エリート除外）、`act_index % 3` 循環、エンドレス周回スケール（ノルマ・間隔・エリート率）、`ryu_body` の幕/ウェーブ初期化クリア、Pre 配線（`spawn_interval_mul`・`max_alive_add`）維持（§2.1 / §2.2） |
| `src/game/roguelike/stage_gimmicks.h` / `stage_gimmicks.c` | 更新 | `act_index % 3` による第 1〜3 幕地形と山颪の循環、落雷避雷ターゲットの `game_enemy_is_targetable` 対応（§2.2） |
| `src/game/enemies/boss_tengu.c`、`boss_nue.c` | 更新 | エンドレス周回ボス攻撃間隔 `× max(0.7, 0.9^n)` と予兆保護下限の適用（§2.2） |
| `src/game/enemies/boss_ryu.c` | 更新 | 多部位 ECS 版（3 龍玉子エンティティ生成・`RyuBodyState` 軌跡更新＆弧長配置・3 フェーズ遷移・エンドレス攻撃間隔スケール）（§2.3） |
| `src/game/combat/damage.h` / `damage.c` | 更新 | `ENEMY_BOSS_RYU_ORB` 破壊時の `EV_BOSS_PART_BREAK`・`orb_broken_mask` 更新・全破壊時の点睛移行、親頭部撃破時の残存子エンティティ後始末、一筆龍第 3 フェーズ「昇龍」（HP <= 40% → `phase_idx = 2`・`EV_BOSS_PHASE value=3`・三元共鳴 `×2.0`）、Pre 配線（`mods.*`・`barrier_t`・`oni_tile`）の維持（§2.3） |
| `src/game/autoplay.c` | 更新 | `gale_charges` 判定、追尾符対処、`game_enemy_is_targetable` による多部位龍玉狙い、`REWARD_KIND_FALLBACK` の 2 択コマンド対応（§2.4） |
| `tests/test_acts_enemies.c` | 更新 | 狐符・護符結界・追尾符・第 3 幕編成、エンドレス周回スケール全 6 項目、多部位一筆龍 3 フェーズ・軌跡配置・点睛・昇龍の単体テスト追加（§3.1） |
| `tests/test_determinism_run.c` | 更新 | 全 3 幕 → `CMD_START_ENDLESS` → 第 4〜6 幕（周回 1 完了）の通し決定性・1 回精算テスト追加（§3.2） |
| `tests/bench_game.c` | 更新 | 多部位一筆龍＋狐符＋エンドレス負荷での `< 4.0 ms/tick` ベンチマーク追加・確認（§3.3） |
| `docs/game-design.md` | 数値のみ | バランス調整で定義テーブルの数値を変えた場合の同期 |

---

## 2. 実装仕様

### 2.1 狐符（`ENEMY_OFUDA_FOX`）と追尾符（`PROJ_HOMING_OFUDA`）（game-design §3.3 / §3.4 / §8.1〜§8.3）

1. **第 3 幕ウェーブ表と増援重みの復帰（`waves.c`）**:
   - **3-2 初期編成**：`弓妖 2・唐傘 1・鎌鼬 1・石鬼 1・狐符 1`（`initial_entries = 5`）。
   - **第 3 幕増援重み**（全 4 ウェーブ共通）：`弓妖 20 / 炎提灯 12 / 雷提灯 12 / 唐傘 16 / 鎌鼬 12 / 石鬼 10 / 墨吸い 10 / 狐符 8`（計 100）。
   - **種類別同時上限**：`per_type_cap` に `if (def_id == ENEMY_OFUDA_FOX) { return 1; }` を追加する。
   - **エリート除外**：`waves_step_spawns` のエリート抽選条件に `chosen != ENEMY_OFUDA_FOX` を加える（`enemy_apply_elite` 側も Pre で除外済み）。
2. **狐符の行動（`src/game/enemies/ofuda_fox.c`）**:
   - `EnemyDef` 基本値：HP `30`, 半径 `17.0f`, `MASS_MEDIUM`, 流体結合 `1.5f`, 点 `200`, 墨玉 `2`。
   - `en->fire_cd`：追尾符の射撃 CD（初期 `1.8s`、以後 `3.0s` 周期、射撃前 `0.4s` で `en->windup` を `0 → 1` へ進める）。
   - `en->absorbed_ink`：護符結界の付与 CD（初期 `2.5s`、以後 `6.0s` 周期）。
   - **移動**：`st->stun_t > 0` は停止。`st->knockback_t <= 0` の間、社 `(sx, sy)` から距離 `260〜340px` の外周帯を保つように速度 `90px/s * slow` で緩やかに周回移動し、通常どおり流体流速（`vel->fluid_coupling = 1.5f`）の加速を受ける。
   - **護符結界（6 秒ごと）**：`en->absorbed_ink -= INK_FIXED_DT * slow` が `<= 0` になったら、自身を除く生存中（`!tr->dissolving`）・標的可能・非ボス（`!game_is_boss_enemy && !game_is_boss_part`）の味方敵のうち、自身から半径 `360px` 以内で最も近い個体（優先：未結界 `barrier_t <= 0.0f` の個体、全員結界中なら最短距離）を 1 体選び、`target_st->barrier_t = 4.0f;` を付与して `en->absorbed_ink = 6.0f` に戻す。周囲に対象がいなければ CD を `1.0f` にして再探索する。
     - 結界中（`st->barrier_t > 0.0f`）の被ダメージ `-50%` は `damage.c` で適用（Pre 配線済み、`elements.c` で毎ティック減衰済み）。
   - **追尾符発射（3 秒ごと）**：`en->fire_cd -= INK_FIXED_DT * slow` が `<= 0` になったら、社方向へ `projectiles_spawn_dir(g, PROJ_HOMING_OFUDA, tr->x, tr->y, dir_x, dir_y, 140.0f)` を放ち、`en->fire_cd = 3.0f` に戻す。
3. **追尾符の物理・旋回・焼失（`src/game/enemies/projectiles.c`）**:
   - `projectiles_spawn` で `kind == PROJ_HOMING_OFUDA` のとき `vel->fluid_coupling = 0.6f;` を設定する。
   - **未偏向時の炎上焼失（最優先）**：`!proj->deflected && proj->kind == PROJ_HOMING_OFUDA && dye[1] > 0.30f` のとき、社へダメージを与えず・偏向イベントも出さずに即 `ecs_destroy(&g->ecs, id); continue;` で燃え尽きさせる（`elements.c` 側で未偏向追尾符が炎上弾へ昇格しないガードは Pre 配線済み）。
   - **未偏向時の社追尾旋回（毎秒 90°まで）**：`!proj->deflected && proj->kind == PROJ_HOMING_OFUDA` のとき、現在速度角 `cur_ang = atan2f(vel->vy, vel->vx)` と社方向角 `want_ang = atan2f(tcy, tcx)` の正規化角度差 `diff`（`[-pi, pi]`）を `max_turn = 1.5707963f * INK_FIXED_DT`（`90°/s`）でクランプして回転させ、速さ `psp` を維持したまま `(vel->vx, vel->vy)` を更新する（流体加速による押し出しは通常どおり効くため、風筆・渦印で逸らして偏向できる）。
   - **最低速度クランプ例外（重要）**：速度クランプの下限を `const float min_sp = (!proj->deflected && proj->kind == PROJ_HOMING_OFUDA) ? 140.0f : 180.0f;` とする（未偏向の追尾符が 1 ティック目で 180px/s に跳ね上がるバグを防ぐ）。偏向後は通常弾と同じ `180〜550px/s`・ダメージ `20`（物理）。

---

### 2.2 エンドレスモード（第 4 幕以降の周回スケール、`game-design.md` §3.6）

周回数 `loop_n` と循環ステージ幕 `stage_act` を共通規則として全システムへ一貫適用する：
```c
static int32_t endless_loop_n(const GameState *g)
{
    int32_t n = g->run.act_index / GAME_MAIN_ACT_COUNT;
    if (n < 0) {
        n = 0;
    } else if (n > ENDLESS_MAX_LOOP_N) {
        n = ENDLESS_MAX_LOOP_N;
    }
    return n;
}
```
- **循環マッピング（`act_index % GAME_MAIN_ACT_COUNT`）**：
  - `waves.c`：`current_act_def(g)` と `per_type_cap(act_index, def_id)`（`stage_act >= 2` で石鬼同時上限 2）。
  - `stage_gimmicks.c`：`stage_gimmicks_setup_act(g, act_index)` と `step_mountain_wind(g)`（`act_index % 3 == 2` の幕で第 3 幕地形・霊柱・山颪を稼働）。
- **周回スケール 6 項目の厳密式**：

| 項目 | 適用箇所 | 式（`n = endless_loop_n(g)`、`n == 0` は P1 と完全同一） |
| :--- | :--- | :--- |
| 通常敵の HP | `enemy_defs.c` (`enemy_spawn` / `enemy_apply_elite`) | `hp = (int32_t)floorf((float)def->base_hp * (1.0f + 0.25f * (float)n) + 0.5f)`（エリート時はさらに `× 2.5f`：`floorf(base_hp * (1.0f + 0.25f * n) * 2.5f + 0.5f)`） |
| ボスの HP | `enemy_defs.c` (`enemy_spawn`) | `game_is_boss_enemy(def_id) || game_is_boss_part(def_id)` のとき `hp = (int32_t)floorf((float)def->base_hp * (1.0f + 0.35f * (float)n) + 0.5f)`（多部位一筆龍の龍睛 900 と各龍玉 300 の双方に適用） |
| ノルマ | `waves.c` (`waves_start_wave`) | 通常ウェーブ（`wave_index < GAME_BOSS_WAVE_INDEX`）のみ `wdef->quota + 2 * n` |
| 増援間隔 | `waves.c` (`waves_start_wave` / `waves_step_spawns`) | `base_iv = fmaxf(0.6f, wdef->spawn_interval * powf(0.95f, (float)n))`、さらに `fmaxf(0.35f, base_iv * g->internal.mods.spawn_interval_mul)` |
| エリート出現率 | `waves.c` (`waves_step_spawns`) | 通常ウェーブで `stage_act = act_index % 3` の基礎率（`0: 0.00f`, `1: 0.08f`, `2: 0.12f`）に `+ 0.04f * (float)n`（上限 `0.30f`）。`prob > 0.0f` のとき抽選（同時上限：`stage_act == 2 || n >= 2 ? 2 : 1`、ウェーブ上限：`stage_act == 2 || n >= 2 ? 3 : 2`）。変異数 2 の解禁は既存どおり `g->run.act_index >= 2`（第 3 幕および全エンドレス幕） |
| ボスの攻撃間隔 | `boss_tengu.c`, `boss_nue.c`, `boss_ryu.c` | `cd_scale = fmaxf(0.70f, powf(0.90f, (float)n))` を各ボスの攻撃間隔へ乗算する。ただし **`game-design.md` §8.4 の「予兆 0.6 秒以上」を守るため、各攻撃の `fire_cd` は `fmaxf(cd, tele_dur + 0.10f)` で下限保護**する |

---

### 2.3 一筆龍の多部位 ECS 版（`boss_ryu.c`, `damage.c`, `enemy_defs.c`, `waves.c`、`game-design.md` §8.7）

P1 の簡易単体版（HP 1500）を、`game-design.md` §8.7・`architecture.md` §5.2 に定義された **多部位 ECS 版（総 HP 1800 = 龍玉 300×3 ＋ 龍睛 900）** へ置き換える。

1. **エンティティ構成と `EnemyDef`**:
   - **親（頭部・龍睛）**: `ENEMY_BOSS_RYU`、`base_hp = 900`（P1 の 1500 から変更。エンドレス時は `×(1 + 0.35n)`）、`radius = 46.0f`、`MASS_BOSS`、`score = 1500`、`orb_count = 8`。
   - **子（3 つの龍玉）**: `ENEMY_BOSS_RYU_ORB` × 3 体（`base_hp = 300`、エンドレス時は `×(1 + 0.35n)`、`radius = 30.0f`、`MASS_BOSS`、`score = 300`、`orb_count = 2`）。
     - 各子エンティティは `COMP_TRANSFORM | COMP_VELOCITY | COMP_HEALTH | COMP_STATUS | COMP_ENEMY | COMP_BOSS` を持ち、`boss->parent_id = head_id`、`boss->part_id = 0`（炎玉・頭 0px）/`1`（雷玉・胴 450px）/`2`（風玉・尾 900px）を設定する。
2. **`GameState.ryu_body` 頭部軌跡リングバッファと弧長配置（`boss_ryu.c`）**:
   - `waves_prepare_act` / `waves_start_wave` で `g->ryu_body` をクリアする（`head_id = orb_ids[0..2] = {ECS_INVALID_INDEX, 0}`, `orb_broken_mask = 0`, `active_orb_slot = -1`, `trail_head = 0`, `trail_count = 0`）。
   - `boss_ryu_spawn`：
     - 親頭部のスポーン座標 `(tr->x, tr->y)` から左後方へ 12px 間隔で 80 点（全長 `960px`）の初期軌跡を `g->ryu_body.trail` へ埋めておき、出現直後から全長 900px の胴と 3 龍玉が重ならずに綺麗に並ぶようにする。
     - `g->ryu_body.head_id = id`、`orb_broken_mask = 0`、`active_orb_slot = 0` をセットし、3 体の `ENEMY_BOSS_RYU_ORB` を生成して `g->ryu_body.orb_ids[0..2]` へ格納する。
   - 毎ティック（`boss_ryu_step` の頭部移動直後）：
     - 頭部が直前サンプル点から `>= 8.0f px` 動いたらリングバッファの先頭（`trail_head = (trail_head + 1) % GAME_RYU_TRAIL_CAP`）へ新座標 `(tr->x, tr->y)` を追加する（`trail_count < GAME_RYU_TRAIL_CAP` なら増加）。
     - 弧長サンプラ `ryu_sample_arc_pos(&g->ryu_body, target_dist_px, &ox, &oy)` で、最新頭位置から過去方向へ線分長を積算・線形補間して `0.0f px`（炎玉）・`450.0f px`（雷玉）・`900.0f px`（風玉）の座標を求め、生存中の各 `orb_ids[s]` の `TransformComp`（`x, y, facing`）を更新する。
     - 胴の微風注入：頭（0px）・胴中央（450px）・尾（900px）の 3 点のうち、移動方向沿いに弱い風（既存どおり速度 16 セル/s・清墨 0.06f）を注入し、胴が社から 200px 以内に入らない制約（龍巻き中を除く）を維持する。
3. **3 フェーズ仕様**:
   - **第 1 フェーズ「三つの龍玉」（`boss->phase_idx == 0`）**:
     - 親頭部（`ENEMY_BOSS_RYU`）は無敵かつ標的除外（`game_enemy_is_targetable` が `false` を返すため、偏向弾・雷墨連鎖・爆雷・三元共鳴は自動的に生存中の龍玉を狙う）。
     - 生存中の龍玉（`orb_broken_mask` で未破壊のスロット `0..2`）が `active_orb_slot` として 1 つずつ交代で攻撃する。
     - 攻撃間隔：`const int32_t broken = popcount(orb_broken_mask & 0x7u);` とし、`2.2f * powf(0.85f, (float)broken) * endless_boss_cd_mul`（下限 `tele_dur + 0.10f`）。攻撃前 `0.65s` の間、担当龍玉の `orb_boss->telegraph` を `0 → 1` に進める（描画側でその龍玉が発光する）。
     - 各龍玉の攻撃は**その龍玉の現在座標 `(orb_tr->x, orb_tr->y)` から**発射・発動する：
       - **炎玉（`part_id == 0`）**：扇状の火球 5 発（`205px/s`）＋炎の息（`0.7s`）。
       - **雷玉（`part_id == 1`）**：雷球 3 発（`215px/s`）＋社周囲 3 か所の落雷予兆（`GIMMICK_LIGHTNING_STRIKE`, `0.8s`、うち 1 か所は社当たり判定内）。
       - **風玉（`part_id == 2`）**：尾の位置から社への大逆風（`1.5s`）＋鎌風 3 発（`300px/s`）。
     - **龍玉破壊時（`damage.c` `kill_enemy`）**:
       - `EV_BOSS_PART_BREAK`（`x, y = 龍玉座標`, `value = part_id`, `flags = dmg_flags`）を発行し、`g->ryu_body.orb_broken_mask |= (uint8_t)(1u << part_id);`、`g->ryu_body.orb_ids[part_id] = {ECS_INVALID_INDEX, 0};` を更新する。
       - 3 つすべて破壊された瞬間（`(orb_broken_mask & 0x7u) == 0x7u`）：親頭部がまだ `phase_idx == 0` であれば直ちに **第 2 フェーズ「点睛」** へ移行する（`parent_boss->phase_idx = 1`, `parent_hp->invuln_t = 1.5f`, `g->ryu_body.active_orb_slot = -1`, `EV_BOSS_PHASE value = 2`, 墨渦演出）。
   - **第 2 フェーズ「点睛」（`boss->phase_idx == 1`）**:
     - 親頭部（龍睛 HP 900、エンドレススケール済み）が弱点・標的可能になる。
     - 頭の遊泳速度 `×1.3`、山颪周期 `6.0s`（`stage_gimmicks.c` の既存 `bb->phase_idx >= 1` 判定で自動作動）。
     - 行動ローテーション（攻撃間隔 `2.2f * endless_boss_cd_mul`）：雷火の息（火球 3＋雷球 3 交互扇）・龍巻き（15 秒 CD、`0.8s` 予兆、`2.0s` 社周囲逆渦）・召喚（10 秒 CD、石鬼 1 または墨吸い 2、護衛上限 `4 + mods->max_alive_add`）。
   - **第 3 フェーズ「昇龍」（`boss->phase_idx == 2`、龍睛 HP 40% 以下：`hp->hp * 5 <= hp->max_hp * 2`）**:
     - `damage.c` で `phase_idx == 1 → 2` へ移行し、`hp->invuln_t = 1.5f`、`EV_BOSS_PHASE`（`value = 3`）と墨渦演出を発行する。
     - 攻撃間隔をさらに `×0.8`（`2.2f * 0.8f * endless_boss_cd_mul`、下限 `tele_dur + 0.10f`）。
     - **`DMG_CATACLYSM` 被ダメージ `×2.0`**（`damage.c` の既存 `boss_c->phase_idx >= 1` を `boss_c->phase_idx >= 2` へ更新：第 3 フェーズ「昇龍」のクライマックス仕様 `game-design.md` §8.7 に厳密一致させる）。
   - **親頭部撃破時の後始末（`damage.c`）**: 万一デバッグ強制クリア等で生存中の `ryu_body.orb_ids[0..2]` が残っていれば `ecs_destroy` し、`ryu_body.head_id = {ECS_INVALID_INDEX, 0}` にリセットする。

---

### 2.4 自動入力（`src/game/autoplay.c`）の P2 対応
- 風斬り発動可能判定を `g->player.gale_charges > 0` に合わせる。
- 追尾符（`PROJ_HOMING_OFUDA`）を偏向・迎撃対象に含める。
- 一筆龍第 1 フェーズで無敵頭部を無駄に狙わず、`game_enemy_is_targetable(g, id)` を使って生存中の龍玉を優先的に狙う。
- `REWARD_KIND_FALLBACK` のとき、社 HP が最大未満なら `CMD_REPAIR_SHRINE`、満タンなら `CMD_PICK_ENDLESS_CRYSTAL` を返すようにし、エンドレス長期オートテストでスタックしないようにする。

---

## 3. テスト・ベンチ要件

### 3.1 `tests/test_acts_enemies.c`（追記・更新）
既存の P1 テスト（一筆龍テストは多部位 3 フェーズ仕様へ更新）に加え、以下を検証する：
1. **狐符と追尾符・第 3 幕編成**:
   - 第 3 幕 3-2 初期編成に `ENEMY_OFUDA_FOX` が 1 体含まれ、同時上限 1・エリート除外が守られること。
   - 狐符が 6 秒周期で近くの味方へ `barrier_t == 4.0f` を張り、結界中の敵へのダメージが `-50%` になること。
   - 追尾符（`PROJ_HOMING_OFUDA`）が 140px/s を維持し（180px/s へ強制加速されない）、社方向へ旋回し、炎 `> 0.30f` のセルを通過した瞬間に社無傷で焼失すること（偏向後は通常どおり敵へ 20 ダメージを与えること）。
2. **エンドレス周回スケール（全 6 項目）**:
   - `act_index = 3`（周回 `n = 1`、第 4 幕）と `act_index = 6`（周回 `n = 2`、第 7 幕）で、地形・ボスが `act_index % 3` で循環すること（第 5 幕＝鳥居/岩柱/鵺、第 6 幕＝霊柱/山颪/一筆龍）。
   - 通常敵 HP（`×1.25`, `×1.50`）、ボス/龍玉 HP（`×1.35`, `×1.70`）、ノルマ（`+2`, `+4`）、増援間隔（`×0.95^n`, 下限 0.6s）、エリート率（`+4n%`, 上限 30%）、ボス攻撃間隔（`×0.9^n`, 下限 ×0.7 かつ `>= tele_dur + 0.10f`）を検証する。
3. **一筆龍の多部位 3 フェーズ**:
   - スポーン時に親頭部（HP 900）＋ 3 龍玉（各 HP 300、`part_id = 0, 1, 2`、`parent_id == head_id`）が生成され、弧長 0 / 450 / 900px 付近に配置されること。
   - 第 1 フェーズで親頭部が `game_enemy_is_targetable == false`・ダメージ 0 であり、龍玉破壊ごとに `EV_BOSS_PART_BREAK`・`orb_broken_mask` 更新・攻撃間隔短縮（`×0.85`）が起きること。
   - 3 龍玉全破壊で第 2 フェーズ「点睛」（`phase_idx == 1`, `EV_BOSS_PHASE value=2`, 山颪 6s、頭部標的可能・HP 900）へ移行し、龍睛 HP `<= 40%`（`<= 360`）で第 3 フェーズ「昇龍」（`phase_idx == 2`, `EV_BOSS_PHASE value=3`）へ移行し、`DMG_CATACLYSM` が第 2 フェーズでは等倍、第 3 フェーズで `×2.0`（`80 → 160`）になること。

### 3.2 `tests/test_determinism_run.c`（追記）
- **本編 3 幕 → `CMD_START_ENDLESS` → エンドレス 1 周（第 4〜6 幕）通し決定性シナリオ**:
  - 2 インスタンスで本編全 3 幕クリア（`RUN_PHASE_VICTORY`、この時点では未精算）→ `CMD_START_ENDLESS`（社 HP `+3`・第 3 幕ボス大遺物選択）→ 第 4 幕（大天狗）→ 第 5 幕（鵺）→ 第 6 幕（多部位一筆龍）クリアまで通しで実行し、毎ティックの `game_hash_sections` 全 7 セクションが完全一致すること。
  - 最後に `CMD_ABORT_RUN` → `meta_settle_run` を 1 回だけ実行し、`acts_cleared == 3`、`endless_acts_cleared == 3`、`from_acts == 120`、`from_endless >= 120`、`cleared == true`、`best_act >= 6` が 1 回のみ精算されること。

### 3.3 `tests/bench_game.c`（Release 性能ベンチ）
- 多部位一筆龍（3 龍玉＋128 点軌跡）＋狐符/追尾符＋エンドレス周回負荷を含む行で 120 ウォームアップ＋600 計測ティック × 5 試行を回し、試行中央値の mean / p95 が **`< 4.0 ms/tick`** かつ 5 試行ハッシュ一致・有限であることを確認する。

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
- [ ] §2 の仕様（狐符＋護符結界＋追尾符＋3-2 復帰、エンドレス周回スケール 6 項目、多部位一筆龍 3 フェーズ＋`RyuBodyState`、`autoplay` 対応）をすべて満たし、凍結ファイルおよび P5-A / P5-C の担当ファイルに差分がない。
- [ ] `test_acts_enemies`、`test_determinism_run`（本編 3 幕＋エンドレス 1 周通し）、`test_core_smoke`、`test_cli_headless_smoke` が ASan/UBSan（`-Werror`）で通る。
- [ ] `bench_game` が Release で 5 試行のハッシュ一致・有限性を満たし、mean / p95 ともに `< 4.0 ms/tick`。
- [ ] PR 作成直後に `@coderabbitai review` を投稿し、全指摘を 4 分類トリアージして必要な修正と返信を完了した（[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md) 準拠）。
- [ ] PR の CI 3 ジョブ（`build-asan-and-test`, `static-analysis`, `dist-verify`）がすべて緑。
