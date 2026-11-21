# Phase 4-A タスク指示書：P1 ローグライト・筆 3 種・爆雷/三元共鳴・24 札/9 遺物（`wt-roguelike-p1`）

> **必読**: [phase4-pre-contracts.md](phase4-pre-contracts.md)（特に §0.1 の進め方、§1.2 の排他所有表、§2 の凍結契約）、[PLAN.md](../PLAN.md) §9.1、[architecture.md](../architecture.md) §3 / §6 / §7、[game-design.md](../game-design.md) §5 / §6 / §7 / §9 / §11.3 / §12、[qa-and-testing.md](../qa-and-testing.md) §2、[AGENTS.md](../../AGENTS.md)、[inkstorm-fluid-sim](../../.agents/skills/inkstorm-fluid-sim/SKILL.md)、[inkstorm-content-authoring](../../.agents/skills/inkstorm-content-authoring/SKILL.md)
>
> **worktree / ブランチ**: `full-scale-dev/wt-roguelike-p1` / `feat/phase4-roguelike-p1`（P4-Pre マージ後の `main` から分岐）。完了したら単独で `main` へ PR を出す。
> **並列ペア**: P4-B（[phase4-acts-enemies-p1.md](phase4-acts-enemies-p1.md)）、P4-C（[phase4-ui-assets-p1.md](phase4-ui-assets-p1.md)）。合流と完了ゲートは P4-Int（[phase4-integration.md](phase4-integration.md)）。

---

## 0. 目的と責務境界

P1 本編のプレイヤー筆スタイル・4 属性反応・ローグライトビルド（強化札 24 種＋初期大遺物 9 種）・初回操作/反応ヒント発火を完成させる。

1. **初期「筆スタイル」3 種**（game-design §5.6）：豪筆（Heavy Brush）・朱筆（Ember Brush）・細筆（Quill Brush）の固有パラメータ、能動炎/雷注入、および同系統の強化札との重なり規則（game-design §9.4 末尾）。
2. **属性反応の拡張**（game-design §7.2 / §7.3）：`elements_step` の段 2 **三元共鳴（墨×炎×雷）** と段 3 **爆雷（炎×雷）**、8×8 セルブロック判定、三元共鳴成立ブロックの同一ティック爆雷除外、次ティック保留インパルス（`pending_impulses[]`）、4 反応すべての `on_reaction` フック発火。
3. **強化札 24 種（Lv1〜3）と初期大遺物 9 種**（game-design §9.4 / §9.5）：`RunModifiers` の一括再計算（加算→乗算）と `fluid.cfg.dye_damp` 同期、7 限定フック（`on_stroke`, `on_cyclone`, `on_gale`, `on_hit`, `on_kill`, `on_reaction`, `on_tick`）の取得順実行、大遺物の代償（墨減衰・風斬り消費増・風筆消費増・社最大 HP 減少）の正確な反映。
4. **2 層報酬ループ**（game-design §9.1 / §9.2）：通常ウェーブ後の強化札 3 択（24 種対応）と、第 1・2 幕ボス撃破後の大遺物 3 択（初期 9 種対応）、幕内のリロール回数共有、候補不足（`< 3` 件 / `0` 件）時のフォールバック。
5. **初回ヒントイベント発火**（game-design §11.3）：操作ヒント 4 種＋属性反応初発火 4 種の計 8 種（`HintId`）を検出し、`EV_HINT_TRIGGER` をラン内各 1 回発行する。

**禁止事項**
- P4-Pre §1.2 の凍結ファイル（`CMakeLists.txt`, `game.h`, `game_ids.h`, `game_events.h`, `game_components.h`, `game_contract.c`, `game_internal.h`, `game.c`, `hash.c`, `debug_api.c`, `ui_view.h` 等）を編集しない。
- P4-B の所有ファイル（`src/game/combat/*`, `src/game/enemies/*`, `src/game/roguelike/waves.*`, `src/game/roguelike/stage_gimmicks.*`, `src/game/autoplay.c`, `tests/test_acts_enemies.c`, `tests/test_determinism_run.c`, `tests/bench_game.c`）を編集しない（`damage.c` / `projectiles.c` 側の `RunModifiers` 参照と `rosary_swirl`・`barrier_shield_ready` は P4-Pre で先行配線済み）。
- P4-C の所有ファイル（`src/render/*`, `src/audio/*`, `src/app/*`, `assets/*`, `README.txt`, `AGENTS.md`）を編集しない。
- `raylib.h` の include、60Hz ループ内の `malloc`/`free`、可変 `static` 変数、`rand()`/`time()` の使用は厳禁。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/game/player/player_skills.h` / `player_skills.c` | 更新 | 筆 3 種（豪筆・朱筆・細筆）、`player_skills_cast_free_gale` 公開（返し風用）、天狗の羽団扇（3 方向風斬り・重複命中防止）、連理の筆（社点対称の双筆）、停止中の鵺への風斬り吹き飛ばし、吸墨 Lv3（渦印で画面全体の墨玉吸引）、かすれの妙（力補正・消費 0）、`PROJ_BOUNCE_ORB` / `deflect_auto_electrify` 帯電対応（§2.1 / §2.3 / §2.4） |
| `src/game/elements/elements.h` / `elements.c` | 更新 | 段 2 三元共鳴、段 3 爆雷、4 反応すべての `on_reaction` フック発火、Pre 先行配線（`ELITE_INK_REPEL`・`ELITE_STORMPROOF`・`game_is_boss_enemy`・`mods.*`）の維持と拡張（§2.2） |
| `src/game/roguelike/upgrades.h` / `upgrades.c` | 更新 | 強化札 24 種すべての定義テーブル、`RunModifiers` 再計算（筆スタイル → メタ強化 → 所持札 → 所持遺物の加算・乗算＋`fluid.cfg.dye_damp[0..1]`・`shrines[0].max_hp` 同期）、7 フックの取得順呼び出し（札 → 遺物）（§2.3） |
| `src/game/roguelike/relics.h` / `relics.c` | 更新 | 初期大遺物 9 種すべての定義テーブル・付与・加算/乗算 `RunModifiers` 補正・7 フック処理（§2.4） |
| `src/game/roguelike/rewards.h` / `rewards.c` | 更新 | 24 札抽選、大遺物 3 択抽選（`REWARD_KIND_RELIC`）、幕内リロール共有、候補不足時の代替規則・ボス後次幕遷移（§2.5） |
| `src/game/roguelike/hints.h` / `hints.c` | 更新 | `HintId` 8 種の初回発火判定と `EV_HINT_TRIGGER` 発行（§2.6） |
| `tests/test_elements.c` | 更新 | 爆雷・三元共鳴・優先除外・保留インパルス・導火札の単体テスト追加（§3.1） |
| `tests/test_roguelike_meta.c` | 更新 | 筆 3 種・24 札・9 遺物・代償・重なり規則・7 フック順序・大遺物抽選・ヒント発火の単体テスト追加（§3.2） |

---

## 2. 実装仕様

### 2.1 初期「筆スタイル」3 種と札の重なり規則（game-design §5.6 / §9.4）

`roguelike_recompute_modifiers(g)` と `player_skills.c` で、`g->config.brush_style` に応じて以下の特性を適用する（P0 の「すべて豪筆扱い」を解除する）。

| 筆スタイル | 基本修正値（`RunModifiers`） | 能動属性注入・固有挙動 | 札との重なり規則（game-design §9.4） |
| :--- | :--- | :--- | :--- |
| **豪筆** (`BRUSH_STYLE_HEAVY`) | `brush_radius_mul *= 1.25f`<br>`brush_force_mul *= 1.20f`<br>`stroke_cost_mul *= 1.15f`<br>`gale_impulse_mul *= 1.25f` | なし | `火種の筆` 取得時はそのまま `160 / 120 / 80px` ごとに火種。`帯電印` 取得時はそのまま `+0.30 / +0.45 / +0.60` |
| **朱筆** (`BRUSH_STYLE_EMBER`) | `stroke_cost_mul *= 1.20f`<br>`stroke_ember_interval = 120.0f`<br>`stroke_ember_mul = 1.0f`<br>`gale_fire_add += 0.35f` | ・風筆：かすれ筆でない間、軌跡 `120px` ごとに火種（炎 `0.5 * stroke_ember_mul`・半径 `2.5 セル`）を注入<br>・風斬り：12 個の各注入点に炎 `0.35` を追加 | ・**朱筆 × 火種の筆（Lv1〜3）**: 基本間隔 `120px` に対し Lv ごとに `×0.75`（Lv1=`90.0f`、Lv2=`67.5f`、Lv3=`50.625f` px）。Lv3 で `stroke_ember_mul = 1.5f`<br>・**朱筆 × 焔の風斬り（Lv1〜3）**: 風斬り各点の炎が `0.35f + (0.25f / 0.35f / 0.50f)` になる |
| **細筆** (`BRUSH_STYLE_QUILL`) | `brush_radius_mul *= 0.80f`<br>`brush_force_mul *= 0.90f`<br>`stroke_cost_mul *= 0.70f`<br>`cyclone_cd = 1.10f`<br>`cyclone_storm_add += 0.55f`<br>`orb_pull_radius *= 1.30f` | ・渦印：発動時に常に雷 `0.55` を注入<br>・墨玉吸引：吸引半径 `×1.3`（既定 `130px → 169px`） | ・**細筆 × 帯電印（Lv1〜3）**: 基本の雷 `0.55f` に Lv ごとに `+0.15f` を上乗せ（Lv1=`0.70f`、Lv2=`0.85f`、Lv3=`1.00f`）。Lv3 では渦持続中（最初の 55%）の毎ティック注入にも雷（`0.60f`）を加える<br>・**細筆 × 吸墨（Lv1〜3）**: 吸墨の基礎半径（`170 / 210 / 250px`）を Pass 1 で設定し、Pass 2 で細筆の `×1.3` を乗算する（Lv3 で `325px`） |

- **二重カウント防止の実装規則**:
  1. **朱筆 × 火種の筆**: `roguelike_recompute_modifiers(g)` が `mods.stroke_ember_interval` と `mods.stroke_ember_mul` を一元計算し、実際の距離累積（`g->internal.ember_tip_dist`）と火種注入は `player_skills.c` の風筆処理で行う（P0 の `ember_tip_on_stroke` は二重発火を防ぐため外す）。
  2. **細筆 × 帯電印**: `cast_cyclone` で `float storm_add = mods->cyclone_storm_add; float storm_sustain = mods->cyclone_storm_sustain;` で初期化してから `upgrades_fire_on_cyclone` を呼ぶ。`charged_seal_on_cyclone` は `g->config.brush_style == BRUSH_STYLE_QUILL` なら `*storm_add += 0.15f * (float)a->lv;`、それ以外は `+0.30 / +0.45 / +0.60` を加算し、Lv3 で `*storm_sustain_add += 0.60f;` とする。
  3. **朱の硯（大遺物）と連理の筆の併用**:
     - `朱の硯`（`RELIC_VERMILION_INKSTONE`）は独立カウンタ `g->internal.relic_ember_dist`（間隔 `200.0f px`）で管理し、かすれ筆でない間に火種（炎 `0.5`・半径 `2.5 セル`）を注入する。
     - `連理の筆`（`RELIC_TWIN_BRUSH`）所持時も `upgrades_fire_on_stroke` は **1 ティック 1 回だけ** 呼び（距離カウンタの二重加算を防ぐ）、火種注入の瞬間に `mods->twin_brush` が真であれば社 `(sx, sy)` に対して点対称位置 `(2*sx - x, 2*sy - y)` にも同じ火種を同時注入する。
- **P4-B との共通契約（停止中の鵺への風斬り）**:
  - `player_skills.c` の `cast_gale` で、敵が `ENEMY_BOSS_NUE` かつ `en->ai_state == 1`（停止中）のときは `mass_mul = 0.55f` として吹き飛び（`vel->vx += gx * imp; vel->vy += gy * imp;`、`st->knockback_t = fmaxf(st->knockback_t, 0.90f)`）を付与する（通常ボスおよびダッシュ中の鵺は `0.0f`）。
  - また `cast_gale` の敵弾偏向時は、`proj->kind == PROJ_STORM_ORB || proj->kind == PROJ_BOUNCE_ORB || mods->deflect_auto_electrify` で `proj->electrified = true` にする。

---

### 2.2 属性反応：三元共鳴（段 2）と爆雷（段 3）（game-design §7.2 / §7.3）

`elements_step(GameState *g)` は毎ティック必ず **段 1 → 段 2 → 段 3 → 段 4 → 段 5** の順で実行し、直後に `game_step` 側で `fluid_sanitize_channels(&g->fluid)` を通す。P4-Pre で `elements.c` に先行配線した `ELITE_INK_REPEL`・`ELITE_STORMPROOF`・`game_is_boss_enemy`・`mods.*` 参照はすべて維持する。

- **爆発吹き飛びの質量係数（段 2・段 3 共通）**:
  - `MASS_LIGHT = 1.0f`、`MASS_MEDIUM = 0.75f`、`MASS_HEAVY = 0.55f`、停止中の鵺（`en->def_id == ENEMY_BOSS_NUE && en->ai_state == 1`）`= 0.55f`、`MASS_TURRET` およびその他のボス `= 0.0f`。
  - 敵座標が重心とほぼ一致（`d < 1e-4f`）する場合は決定的な外向き単位ベクトル `(1.0f, 0.0f)` を使う。

#### (1) 8×8 ブロック分割の定義
- 160×90 格子を `8 × 8` セルのブロック（横 `20` × 縦 `12` = 計 `240` ブロック）に分割する。
- セル `(x, y)`（`1 <= x <= FLUID_W - 2`, `1 <= y <= FLUID_H - 2`, `solid[y * FLUID_W + x] == 0`）が属するブロック番号は `bx = x / 8`（`0..19`）、`by = y / 8`（`0..11`）、`b = by * 20 + bx`（`0..239`）。
- セル中心の画面 px 座標は `cx = ((float)x + 0.5f) * FLUID_CELL_PX`、`cy = ((float)y + 0.5f) * FLUID_CELL_PX`。

#### (2) 段 2：三元共鳴（墨 × 炎 × 雷、`REACT_CATACLYSM`）
- **セル条件**: 非 `solid` かつ `ink > 0.30f && fire > 0.30f && storm > 0.30f` のセルを「共鳴セル」とする。
- **ブロック判定と全体 CD**:
  - `g->internal.cataclysm_cd`（初期 0、発動時 `2.0f` 秒、毎ティック冒頭で `INK_FIXED_DT` 減算し `<= 1e-4f` で `0.0f`）が `<= 0.0f` のときのみ判定する。
  - 共鳴セルが **8 個以上** あるブロックを探す。複数ブロックが満たす場合は「共鳴セル数が最大 → 共鳴セルの `ink + fire + storm` 合計が最大 → ブロック番号 `b` が最小」の決定的な優先順で **1 ブロックだけ** 選ぶ（game-design §7.3「全体の CD は 2.0 秒（同時に 1 か所だけ）」）。
  - 成立したブロック番号を `cataclysm_block`（不成立時は `-1`）として記録し、**そのティックの段 3（爆雷）の判定対象から除外する**（game-design §7.2）。
- **効果**:
  1. 成立ブロック内の共鳴セルの重心 `(cx_px, cy_px)` を求める。
  2. **保留インパルス予約**: `g->pending_impulse_count < GAME_MAX_PENDING_IMPULSE` なら、`g->pending_impulses[g->pending_impulse_count++] = (PendingImpulse){.x = cx_px, .y = cy_px, .rad_cells = 12.0f, .peak_speed = 90.0f}`（爆雷 `60.0f` の 1.5 倍）を積む。次ティックの段 3 で `fluid_add_radial_impulse` により速度クランプ付きで注入される。
  3. **範囲ダメージ・吹き飛び・墨印**: 中心から半径 `140.0f px` 以内の生存中の敵（`!tr->dissolving`）すべてに：
     - 吹き飛びを付与（`mass_explosion_mul > 0.0f` のとき外向きに `620.0f * mass_explosion_mul` px/s の速度を加算し `st->knockback_t = fmaxf(st->knockback_t, 1.0f)`。重量級 `MASS_HEAVY` と停止中の鵺も吹き飛ぶ）。
     - `80` ダメージ（`DMG_FIRE | DMG_STORM | DMG_CATACLYSM`）を適用する。
     - 生存していれば（`ecs_is_alive(&g->ecs, id)`）かつ `(en->elite_flags & ELITE_INK_REPEL) == 0` なら墨印を付与する（`st->ink_mark_t = g->internal.mods.ink_mark_duration`）。順序は **ダメージ適用 → 生存していれば墨印付与** で統一する。
  4. **広域落雷（30 ダメージ × 最大 4 体）**: 中心から半径 `260.0f px` 以内の生存中の敵を「中心からの距離の昇順（同距離は ECS `index` 昇順）」で最大 **4 体** 選び、それぞれに `EV_STORM_ZAP`（`(cx_px, cy_px) -> (tr->x, tr->y)`, `value = 0`）を発行し、`30` ダメージ（`DMG_STORM | DMG_CATACLYSM`）を与える。
  5. **染料消費**: 中心から半径 `12.0f` セル（`96.0f px`）以内の非 `solid` セルの染料を減衰させる：`ink *= 0.5f`, `fire *= 0.2f`, `storm *= 0.2f`。
  6. **演出・統計・フック**: `g->internal.cataclysm_cd = 2.0f`、`g->hitstop_t = fmaxf(g->hitstop_t, 0.06f)`、`if (g->run.cataclysms < META_MAX_COUNTERS) { ++g->run.cataclysms; }`、`EV_CATACLYSM`（`x = cx_px, y = cy_px, x2 = 96.0f, value = count`）発行、`upgrades_fire_on_reaction(g, REACT_CATACLYSM, cx_px, cy_px, (float)count)` 呼び出し。

#### (3) 段 3：爆雷（炎 × 雷、`REACT_THUNDERBURST`）
- **セル条件**: 非 `solid` かつ `fire > thresh && storm > thresh` のセルを「爆雷セル」とする。ここで `thresh = g->internal.mods.thunderburst_thresh`（既定 `0.30f`、`爆雷の導火` Lv1/2/3 で `0.26f / 0.22f / 0.18f`）。半径倍率 `rmul = g->internal.mods.thunderburst_radius_mul`（既定 `1.0f`、`爆雷の導火` Lv3 で `1.30f`）。
- **ブロック判定と制限**:
  - ブロックごとの CD `g->internal.thunderburst_block_cd[b]`（発動時 `0.8f` 秒、毎ティック冒頭で減算し `<= 1e-4f` で `0.0f`）が `<= 0.0f`、かつ `b != cataclysm_block`、かつ爆雷セルが **6 個以上** あるブロックを候補とする。
  - **1 ティックに最大 3 回まで**（game-design §7.3）。候補が 4 ブロック以上ある場合は「爆雷セル数が大きい順（同数はブロック番号 `b` が小さい順）」で上位 3 ブロックを発動させる。
- **効果（各発動ブロックにつき）**:
  1. ブロック内の爆雷セルの重心 `(cx_px, cy_px)` を求める。
  2. **保留インパルス予約**: `g->pending_impulse_count < GAME_MAX_PENDING_IMPULSE` なら `g->pending_impulses[g->pending_impulse_count++] = (PendingImpulse){.x = cx_px, .y = cy_px, .rad_cells = 9.0f * rmul, .peak_speed = 60.0f}` を積む。
  3. **範囲ダメージ・吹き飛び**: 中心から半径 `96.0f * rmul` px 以内の生存中の敵すべてに：
     - `mass_explosion_mul > 0.0f` のとき外向きに `520.0f * mass_explosion_mul` px/s の衝撃を加え、`st->knockback_t = fmaxf(st->knockback_t, 1.0f)` を付与する（重量級 `MASS_HEAVY` と停止中の鵺も吹き飛ぶ）。
     - `30` ダメージ（`DMG_FIRE | DMG_STORM`）を与える。
  4. **染料消費**: 中心から半径 `9.0f * rmul` セル（`72.0f * rmul` px）以内の非 `solid` セルの `fire *= 0.3f`, `storm *= 0.2f`。
  5. **演出・統計・フック**: `thunderburst_block_cd[b] = 0.8f`、`g->hitstop_t = fmaxf(g->hitstop_t, 0.03f)`、`if (g->run.thunderbursts < META_MAX_COUNTERS) { ++g->run.thunderbursts; }`、`EV_THUNDERBURST`（`x = cx_px, y = cy_px, x2 = 72.0f * rmul, value = count`）発行、`upgrades_fire_on_reaction(g, REACT_THUNDERBURST, cx_px, cy_px, (float)count)` 呼び出し。

#### (4) 段 1・段 4 での `on_reaction` フック補完
- 段 1 の `EV_IGNITE` 発火時に `upgrades_fire_on_reaction(g, REACT_BLAZING, cx, cy, (float)blazing_cells)`、段 4 の雷墨連鎖発火時に `upgrades_fire_on_reaction(g, REACT_TEMPEST, cur_x, cur_y, (float)visited_n)` を呼び、4 反応すべてで `on_reaction` フックが動作するようにする。

---

### 2.3 強化札 24 種の一覧と実装仕様（game-design §9.4）

P0 の 8 種に加え、新規 16 種を `src/game/roguelike/upgrades.c` に実装する。各 Lv の数値は**その Lv 時点の合計効果**である。

| ID | 軸 | 札名 | Lv1 / Lv2 / Lv3 の効果と実装箇所 |
| :--- | :---: | :--- | :--- |
| `UPG_EMBER_TIP` | 紅蓮 | 火種の筆 | （既存＋§2.1 朱筆重なり対応）筆跡 `160 / 120 / 80px` ごとに火種（炎 `0.5`・半径 `2.5`）、Lv3 で `stroke_ember_mul = 1.5f`（`recompute_modifiers`＋`player_skills.c` で一元実行） |
| `UPG_WILDFIRE` | 紅蓮 | 延焼 | （既存）`mods.burn_rate_mul *= 1.25f / 1.50f / 1.80f` |
| `UPG_FLAME_GALE` | 紅蓮 | 焔の風斬り | `mods.gale_fire_add += 0.25f / 0.35f / 0.50f`（風斬り 12 点の各 `fluid_splat` に炎を加算。天狗の羽団扇時は各方向 `×0.6`）。Lv3 は `mods.gale_end_blast_dmg = 20.0f` とし、`on_gale` で終点 `(x + dx * 220.0f, y + dy * 220.0f)` に爆炎（半径 `48px` 以内の敵に `20` ダメージ `DMG_FIRE | DMG_SECONDARY`＋炎 `0.8`・半径 `4.5 セル` 注入） |
| `UPG_INFERNO` | 紅蓮 | 業火 | `mods.burn_base_dmg = 14.0f / 18.0f / 22.0f`（基礎 `10` を置換）。Lv3 は `mods.inferno_ember_on_kill = true`（`on_kill` で `(a->dmg_flags & DMG_FIRE) != 0` の撃破位置に炎 `0.75`・半径 `3.5 セル` の火種を注入） |
| `UPG_PHOENIX_ARROW` | 紅蓮 | 不死鳥の矢 | `mods.burning_proj_pierce = 3 / 4 / 5`（炎上弾の `pierce` 初期値）。Lv3 は `mods.burning_proj_hit_splat = true`（Pre で `projectiles.c` に配線済み：命中ごとに炎 `0.65`・半径 `3.2 セル` 注入） |
| `UPG_CHARGED_SEAL` | 雷霆 | 帯電印 | （既存＋§2.1 細筆重なり対応）渦印に雷 `+0.30 / +0.45 / +0.60`（細筆時は `+0.15 / +0.30 / +0.45`）、Lv3 は持続中（最初の 55%）も雷 `0.60f` 注入 |
| `UPG_CHAIN_LIGHTNING` | 雷霆 | 連鎖雷 | （既存）`mods.chain_hops_add += 1 / 2 / 3`、Lv3 で `mods.chain_range_mul *= 1.30f` |
| `UPG_THUNDER_ARROW` | 雷霆 | 雷撃弾 | `mods.thunder_proj_targets = 2 / 3 / 3`（Pre で `projectiles.c` に配線済み：追加雷撃 `10` ダメージの対象数を最大 2 体 / 3 体へ拡大）。Lv3 は `mods.deflect_auto_electrify = true`（`projectiles.c` と `player_skills.c` の偏向成立時に `proj->electrified = true`） |
| `UPG_SWIFT_THUNDER` | 雷霆 | 迅雷 | `mods.storm_zap_cd = 0.45f / 0.38f / 0.30f`（雷墨連鎖の CD 既定 `0.54s` を短縮） |
| `UPG_THUNDERBURST_FUSE` | 雷霆 | 爆雷の導火 | `mods.thunderburst_thresh = 0.26f / 0.22f / 0.18f`（既定 `0.30f`）。Lv3 で `mods.thunderburst_radius_mul = 1.30f` |
| `UPG_GREAT_VORTEX` | 烈風 | 大渦 | （既存）`mods.cyclone_radius_mul *= 1.15f / 1.30f / 1.45f`、`mods.cyclone_duration_add += 0.2f / 0.4f / 0.6f` |
| `UPG_SLAM_SHOCKWAVE` | 烈風 | 叩きつけ衝撃 | （既存）叩きつけ時に半径 `64 / 80 / 96px` に `20 / 30 / 40` ダメージ（`DMG_PHYSICAL | DMG_SECONDARY`）、Lv3 で周囲吹き飛ばし |
| `UPG_MIGHTY_STROKE` | 烈風 | 剛腕の筆 | `mods.brush_force_mul *= 1.15f / 1.30f / 1.45f` |
| `UPG_MAELSTROM_BIND` | 烈風 | 渦縛り | `mods.maelstrom_bind_level = 1 / 2 / 3`。`on_tick` で全スロットの `maelstrom_bind_t[idx]` を減衰（非生存または非敵スロットは即 `0.0f` クリア）し、稼働中の渦印（`ActiveCyclone`）から半径 `140px * mods.cyclone_radius_mul` 以内の敵（Lv1 は `MASS_LIGHT` のみ、Lv2 以上は `MASS_LIGHT` と `MASS_MEDIUM`）について目標速度 `240px/s` へ向けて中心方向へ補間加速（`vel += (target - vel) * 6.0f * dt`、無制限加算しない）し、`maelstrom_bind_t[idx] = 1.0f` を更新する。`maelstrom_bind_t[idx] > 0.0f` の間は `st->stun_t = fmaxf(st->stun_t, 0.15f)` を維持して移動・射撃を止める。Lv3 では拘束中（`maelstrom_bind_t > 0`）かつ `!tr->dissolving` の敵同士が距離 `32px` 以内に接触したとき、双方の `maelstrom_bind_t` を `0.0f` に消化してから（同一ペアの連続発火防止）、生存を確認しつつ双方へ `40` ダメージ（`DMG_PHYSICAL | DMG_SECONDARY`）を与える |
| `UPG_TAILWIND` | 烈風 | 追い風 | `mods.homing_steer_mul *= 1.30f / 1.60f / 2.00f`。Lv3 で `mods.deflected_max_speed_mul *= 1.20f` |
| `UPG_RICH_INK` | 墨道 | 墨玉増量 | （既存）`on_kill` で `33% / 66% / 100%` の確率で墨玉 +1 |
| `UPG_LASTING_SURGE` | 墨道 | 長き昇華 | （既存）`mods.surge_duration += 1.0f / 2.0f / 3.0f`（合計 `4.6 / 5.6 / 6.6` 秒） |
| `UPG_INK_MAGNET` | 墨道 | 吸墨 | `mods.orb_pull_radius = 170.0f / 210.0f / 250.0f`（Pass 1 で既定 `130.0f` を置換、Pass 2 で細筆なら `×1.3`）。Lv3 は `mods.cyclone_pulls_all_orbs = true`（渦印発動時に半径 220px 制限を撤廃し、画面内の全墨玉を中心へ `360px/s` で引き寄せる） |
| `UPG_CHAIN_PRIMER` | 墨道 | 呼び水 | `mods.chain_grace = 3.20f / 3.80f / 4.50f`（一筆連殺の猶予既定 `2.65s` を延長） |
| `UPG_DRY_MASTERY` | 墨道 | かすれの妙 | `mods.dry_force_override = 0.75f / 1.00f / 1.00f`（かすれ筆中の風筆の力係数既定 `0.46f` を引き上げ）。Lv3 は `mods.dry_zero_cost = true`（`g->player.ink < 20.0f` の間は風筆の墨消費 `0`、かつ `ink <= 0.5f` でも描ける）かつ `mods.dry_deflect_dmg_mul = 1.20f`（`g->player.dry` の間、`DMG_DEFLECTED` ダメージ `×1.2`） |
| `UPG_SHRINE_WARD` | 共通 | 社の護符 | `mods.shrine_max_hp_delta += 1 / 2 / 3`。さらに `roguelike_grant_upgrade` で `roguelike_recompute_modifiers(g)` の**後**（新 `max_hp` 確定後）に社の現在 HP を `+1`（新 `max_hp` 上限）回復する |
| `UPG_DEEP_INK` | 共通 | 濃墨 | `mods.ink_max_add += 15.0f / 30.0f / 45.0f` |
| `UPG_DEEP_MARK` | 共通 | 墨印深化 | `mods.ink_mark_mul = 2.20f / 2.40f / 2.60f`（既定 `2.00f`、ボスへの倍率は `1.5f * (mods.ink_mark_mul * 0.5f)`）、`mods.ink_mark_duration += 1.0f / 2.0f / 3.0f`（既定 `3.0f`） |
| `UPG_BARRIER` | 共通 | 結界 | `mods.shrine_invuln_add += 0.30f / 0.60f / 0.90f`（社被弾後の無敵時間既定 `0.95s` に加算）。Lv3 は `mods.barrier_shield_enabled = true`（`on_tick` で `!barrier_shield_ready` のとき `barrier_shield_cd` を減算し `<= 1e-4f` で `barrier_shield_cd = 0.0f; barrier_shield_ready = true;` とする。`barrier_shield_cd` 初期値は `0.0f` なので Lv3 到達直後の最初のティックで即チャージされ、Pre で配線済みの `projectiles.c` が未偏向の敵弾 1 発を無効化した瞬間に `barrier_shield_ready = false; barrier_shield_cd = 12.0f;` を再開する） |

---

### 2.4 初期大遺物 9 種の一覧と実装仕様（game-design §9.5）

`src/game/roguelike/relics.c` に初期解放の 9 種を実装する。

| ID | 大遺物名 | 効果の実装 | 代償の実装 |
| :--- | :--- | :--- | :--- |
| `RELIC_VERMILION_INKSTONE` | 朱の硯 | `mods.relic_ember_interval = 200.0f`。`on_stroke` でかすれ筆でない間、`g->internal.relic_ember_dist` が `200px` たまるごとに火種（炎 `0.5`・半径 `2.5 セル`）を注入（`mods.twin_brush` 併用時は社点対称位置にも同時注入） | `mods.ink_damp_override -= 0.005f`（既定 `0.985 → 0.980`、`recompute_modifiers` が `fluid.cfg.dye_damp[0]` へ即同期） |
| `RELIC_THUNDER_DRUM` | 雷神の太鼓 | `on_cyclone`：渦印の中心 `(x, y)` に雷 `1.0`・半径 `4.0 セル` を `fluid_splat` 注入し、半径 `72px` 以内の生存中（`!tr->dissolving`）の敵に `40` ダメージ（`DMG_STORM | DMG_SECONDARY`）を与え、`EV_STORM_ZAP`（`(x, fmaxf(20.0f, y - 120.0f)) -> (x, y)`, `value = 0`）を発行 | なし |
| `RELIC_TENGU_FAN` | 天狗の羽団扇 | `mods.tengu_fan = true`。風斬り（`cast_gale`）が正面 `0°` と `±25°`（`±0.4363323f rad`）の 3 方向に分かれ、各方向の流体注入速度・注入墨/炎量・敵への衝撃（`560 * 0.6 = 336 px/s`）をそれぞれ `60%` にし、各方向に `EV_GALE` と `upgrades_fire_on_gale` を発行する。**注意**：`±25°` の円錐（`cos > 0.35`）は重なるため、1 回の `cast_gale` 内で同じ敵を重複吹き飛ばししないよう `uint8_t hit_enemy[ECS_MAX_ENTITIES]` で各敵 1 回に制限する | `mods.gale_cost_add += 6.0f`（基礎コスト `16 → 22`、墨昇華中は半分の `11`） |
| `RELIC_INK_DYED_SHRINE` | 墨染めの社 | `on_tick`：毎ティック社の位置 `(sx, sy)` に半径 `6.0 セル`・墨 `0.04f`（速度 0）を `fluid_splat` で注入する | なし |
| `RELIC_TWIN_BRUSH` | 連理の筆 | `mods.twin_brush = true`。風筆の各注入点 `(px, py)`・速度 `(fx, fy)` に加え、社 `(sx, sy)` を中心とした点対称位置 `(2*sx - px, 2*sy - py)` にも速度 `(-fx, -fy)`・同じ半径・同じ墨量で `fluid_splat` する（火種は §2.1 のとおり点対称にも同時注入し、`on_stroke` は 1 ティック 1 回呼び出しとする） | `mods.stroke_cost_mul *= 1.30f` |
| `RELIC_ROSARY_SWIRL` | 数珠の渦 | `mods.rosary_swirl = true`（Pre で `combat_orbs_step` に先行配線済み：墨玉を通常回収した瞬間に回収位置へ半径 `6.0 セル`・接線速度 `36.0 セル/s`・墨 `0.25f` の `fluid_swirl` を注入） | なし |
| `RELIC_PACT_OF_FIRE` | 業火の契り | `mods.fire_dmg_mul *= 2.0f`（Pre で `combat_apply_damage_f` に先行配線済み：`DMG_FIRE` を持つダメージの基礎補正で `×2.0`） | `mods.shrine_max_hp_delta -= 2`（社の最大 HP `-2`、下限 `1`。現在 HP が新 `max_hp` を超えていれば新 `max_hp` へクランプ） |
| `RELIC_LINGERING_INK` | 残墨 | `mods.ink_damp_override += 0.008f`（既定 `0.985 → 0.993`、`朱の硯` と両方所持時は `0.985 - 0.005 + 0.008 = 0.988f`）、`mods.ink_mark_duration += 2.0f` | なし |
| `RELIC_WIND_RETURN` | 返し風 | `on_tick`：`g->internal.wind_return_cd`（12.0s CD）を減算し `0` のとき、社から距離 `54.0f px` 以内に未偏向（`!proj->deflected`）かつ `!tr->dissolving` の敵弾があれば、`player_skills_cast_free_gale(g, sx, sy, tr->x - sx, tr->y - sy)`（`player_skills.h` で公開する共通ヘルパ：墨消費 0・プレイヤー CD 非消費・`EV_GALE` 発行）を放ち、`wind_return_cd = 12.0f` にする | なし |

- **社最大 HP と流体減衰の再計算契約（`roguelike_recompute_modifiers`）**:
  - `base_max_hp = 8 + clamp_meta_level(META_UPG_SHRINE_HP) + mods.shrine_max_hp_delta`（下限 `1`）を `g->shrines[0].max_hp` に反映し、`g->shrines[0].hp > g->shrines[0].max_hp` なら `hp = max_hp` にクランプする。
  - `g->fluid.cfg.dye_damp[0] = ink_clampf(mods.ink_damp_override, 0.90f, 0.999f);`、`g->fluid.cfg.dye_damp[1] = ink_clampf(mods.fire_damp_override, 0.90f, 0.999f);` を毎回同期する。

---

### 2.5 2 層報酬の抽選と遷移（`src/game/roguelike/rewards.c`）

1. **通常ウェーブ後の強化札 3 択（`rewards_generate_offer`）**:
   - P3-A §2.8 の抽選アルゴリズムをそのまま全 24 種（`UPG_COUNT = 24`）に対して実行する。共通軸（`AXIS_COMMON`）は独立した 1 軸として「3 枠中少なくとも 2 軸以上」の判定に数える（game-design §9.2）。
2. **第 1・2 幕ボス撃破後の大遺物 3 択（`rewards_generate_relic_offer`）**:
   - 未所持（`!game_has_relic(g, id)`）の大遺物（`0..RELIC_COUNT-1`）を候補プールに集める。
   - 候補が 0 件なら `offer->kind = REWARD_KIND_FALLBACK`、`choice_count = 0`、`repair_available = true`、`serial++`。
   - 候補が 1 件以上なら `offer->kind = REWARD_KIND_RELIC`、`repair_available = false`（ボス撃破時に社 HP +3 済みのため）。各枠 `slot = 0..2` で残りの候補から `ink_rng_next(&g->rng) % pool_count` で等確率に 1 つ選び、`choice_ids[slot] = (int32_t)chosen`、`choice_next_level[slot] = 1` に設定する（候補が 3 件未満なら出せる件数で終了）。`serial++`。
3. **リロール（`rewards_reroll`）**:
   - `g->phase == RUN_PHASE_REWARD` かつ `g->run.rerolls_left > 0` かつ（`kind == REWARD_KIND_UPGRADE` または `kind == REWARD_KIND_RELIC`）のとき有効。
   - `g->run.rerolls_left--` し、`kind == REWARD_KIND_UPGRADE` なら `rewards_generate_offer(g)`、`kind == REWARD_KIND_RELIC` なら `rewards_generate_relic_offer(g)` を呼び、`EV_REWARD_REROLL` を出す。
4. **選択確定（`rewards_pick_reward`）と修繕（`rewards_repair_shrine`）**:
   - `REWARD_KIND_UPGRADE`：札を付与 → `EV_REWARD_PICK`（`flags = 0`）→ `waves_start_wave(g, g->run.wave_index + 1)`。
   - `REWARD_KIND_RELIC`：遺物を付与 → `EV_REWARD_PICK`（`value = (int32_t)relic_id`, `flags = EVF_REWARD_RELIC`）→ `reward` をクリアし `wave_clear_is_boss = false` にして次幕へ移行（`g->run.act_index++`, `g->run.wave_index = 0`, `g->internal.act_prepared = false`, `g->phase = RUN_PHASE_ACT_TRANSITION`, `g->phase_t = 0.0f`）。
   - `rewards_repair_shrine`：ボス後の `REWARD_KIND_FALLBACK`（`g->internal.wave_clear_is_boss` が真）なら、社回復（上限 `max_hp`）と `EV_REWARD_PICK`（`value = -1, flags = EVF_REWARD_REPAIR`）の後に上と同じ次幕 `RUN_PHASE_ACT_TRANSITION` へ移行する。

---

### 2.6 初回ヒントの発火（`src/game/roguelike/hints.c`、game-design §11.3）

各ティックの段 7e（`hints_check_tick(g)`）で、そのラン内でまだ出していないヒント（`(g->internal.triggered_hints_mask & (UINT32_C(1) << id)) == 0`）が条件を満たしたら、ビットを立てて `EV_HINT_TRIGGER`（`value = (int32_t)id`、座標は関連位置または社座標）を発行する（セーブ済みの既読マスク `MetaState.seen_hints_mask` と `settings.hints_on` による表示フィルタは P4-C の `app` 側で行うため、`ink_game` は純粋にラン内の初回条件成立時にイベントを 1 回発行する）。

| `HintId` | 発火条件（`g->phase == RUN_PHASE_PLAYING`） |
| :--- | :--- |
| `HINT_STROKE` | 第 1 幕第 1 ウェーブ開始直後（`act_index == 0 && wave_index == 0` の最初の `PLAYING` ティック） |
| `HINT_CYCLONE_FIREBALL` | 盤面に初めて `PROJ_FIREBALL`（未偏向・`!tr->dissolving`）が存在したティック |
| `HINT_GALE_UMBRELLA` | 盤面に初めて `ENEMY_UMBRELLA`（`!tr->dissolving`）が生存しているティック |
| `HINT_INK_REGEN` | 初めて `g->player.ink < 20.0f`（かすれ筆）になったティック |
| `HINT_REACT_BLAZING` | そのティックの `g->events[]` に `EV_IGNITE` が含まれるとき |
| `HINT_REACT_TEMPEST` | そのティックの `g->events[]` に `EV_STORM_ZAP` が含まれるとき |
| `HINT_REACT_THUNDERBURST` | そのティックの `g->events[]` に `EV_THUNDERBURST` が含まれるとき |
| `HINT_REACT_CATACLYSM` | そのティックの `g->events[]` に `EV_CATACLYSM` が含まれるとき |

---

## 3. テスト要件

### 3.1 `tests/test_elements.c`（追記）
既存の P0 9 ケースを維持したまま、以下のケースを追加する：
1. **爆雷の発火・インパルス・染料減衰・ブロック CD**: 1 つの 8×8 ブロック内に `fire = 0.45, storm = 0.45` のセルを 5 個置いても発火せず、6 個置くと `EV_THUNDERBURST` が 1 件発火し、`pending_impulse_count == 1`（次ティックで外向き速度が注入される）、半径 96px 以内の敵（重量級 `ENEMY_STONE_ONI` を含む）が 30 ダメージと吹き飛びを受け、半径 9 セル内の炎が `×0.3`・雷が `×0.2` に減り、0.8 秒（48 ティック）未満は同一ブロックで再発火しないこと。1 ティックに 5 ブロック同時成立させても発火は最大 3 回であること。
2. **三元共鳴の発火と爆雷に対する優先除外**: 同一 8×8 ブロック内に `ink = 0.45, fire = 0.45, storm = 0.45` のセルを 8 個置くと、`EV_CATACLYSM` が発火し、**そのブロックでは同じティックに `EV_THUNDERBURST` が発火しない**こと。半径 140px 以内の敵に 80 ダメージ＋吹き飛び＋墨印（`ELITE_INK_REPEL` を除く）が付き、260px 以内の敵最大 4 体に `EV_STORM_ZAP` と 30 ダメージが飛び、全体 CD 2.0 秒（120 ティック）中は他のブロックでも三元共鳴が発火しないこと。
3. **爆雷の導火（`UPG_THUNDERBURST_FUSE`）**: Lv1/2/3 で閾値が `0.26 / 0.22 / 0.18` に下がり、`fire = 0.20, storm = 0.20`（通常では不発）が Lv3 で発火し、半径が `×1.3` になること。

### 3.2 `tests/test_roguelike_meta.c`（追記）
既存のメタ部・P0 ラン部を維持したまま、以下のケースを追加する：
1. **筆スタイル 3 種と重なり規則**:
   - 朱筆：風筆 120px で火種注入、かすれ筆中は火種なし、風斬りで炎 0.35 注入、`火種の筆` Lv1/2/3 で間隔が `90 / 67.5 / 50.625px` になり二重注入が起きないこと。
   - 細筆：渦印 CD `1.1s`・雷 `0.55` 注入、`帯電印` Lv1/2/3 で雷が `0.70 / 0.85 / 1.00` になり Lv3 で持続注入されること、墨玉吸引半径が `×1.3`（`吸墨` Lv3 併用で `250 * 1.3 = 325px`）になること。
2. **新規強化札 16 種の効果検証**:
   - 焔の風斬り（Lv3 終点爆炎）、業火（炎上ダメージ 14/18/22 と Lv3 炎上撃破火種）、不死鳥の矢（貫通 3/4/5 と Lv3 命中飛沫）、雷撃弾（追加対象 2/3 体と Lv3 偏向時帯電）、迅雷（CD 0.45/0.38/0.30s）、剛腕の筆、渦縛り（Lv1 軽・Lv2 中拘束・Lv3 拘束衝突 40 ダメージと再発火防止）、追い風、吸墨（Lv3 全画面渦印吸引）、呼び水、かすれの妙（Lv3 かすれ時消費 0・`ink == 0` でも描画可・偏向ダメ +20%）、社の護符（最大 HP +1/+2/+3 と即時回復）、濃墨、墨印深化、結界（無敵時間延長と Lv3 12s ごとの敵弾 1 発無効化）。
3. **初期大遺物 9 種と代償**:
   - 朱の硯（200px 火種＋`fluid.cfg.dye_damp[0] == 0.980`）、雷神の太鼓（渦印落雷 40 ダメージ）、天狗の羽団扇（3 方向風斬り・単体敵への重複吹き飛びなし＋消費 `+6`）、墨染めの社（社周囲の墨湧出）、連理の筆（点対称双筆＋消費 `×1.3`）、数珠の渦（墨玉通常回収時の小渦）、業火の契り（炎ダメージ `×2`＋社最大 HP `-2`・現在 HP クランプ）、残墨（`fluid.cfg.dye_damp[0] == 0.993`＋墨印持続 `+2s`、朱の硯との併用時は `0.988`）、返し風（社接近弾への自動風斬り・12s CD・墨非消費）。
4. **7 フック取得順と大遺物 3 択・候補枯渇**:
   - 複数の `on_hit` / `on_cyclone` 札と大遺物を異なる順序で取得したとき、取得順に決定的に実行されること。
   - ボス後の大遺物 3 択で未所持から重複なく 3 個提示され、幕のリロール回数を消費して引き直せること。8 個所持状態で残り 1 個のみ提示、9 個全所持状態で `REWARD_KIND_FALLBACK` になり `CMD_REPAIR_SHRINE` で次幕 `RUN_PHASE_ACT_TRANSITION` へ進めること。
5. **初回ヒント発火**: 8 種すべての `HintId` が条件成立時に 1 回だけ `EV_HINT_TRIGGER` を出し、同一ラン内で 2 回出ないこと。

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
- [ ] §2 の仕様（筆 3 種・爆雷・三元共鳴・強化札 24 種・初期大遺物 9 種・2 層報酬・ヒント 8 種）をすべて満たし、凍結ファイルおよび P4-B / P4-C の担当ファイルに差分がない。
- [ ] `test_elements`、`test_roguelike_meta`、`test_core_smoke`、`test_cli_headless_smoke` が ASan/UBSan（`-Werror`）で全通過する。
- [ ] PR の CI 3 ジョブ（`build-asan-and-test`, `static-analysis`, `dist-verify`）がすべて緑。
