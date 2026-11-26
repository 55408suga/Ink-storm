# INKSTORM（墨嵐）技術アーキテクチャ設計書

> **位置づけ**: 本ドキュメントは INKSTORM（墨嵐）のモジュール境界・60Hz Simulation Pipeline・Fluid Engine API・Hybrid ECS・データ駆動スキーマ・イベント/コマンド・セーブ仕様の Single Source of Truth（[PLAN.md](PLAN.md) Section 7 の詳細版）である。
> ゲーム仕様は [game-design.md](game-design.md)、世界観・水墨表現規約は [art-direction.md](art-direction.md)、検証・テスト仕様は [qa-and-testing.md](qa-and-testing.md)、モジュール依存の機械的制約は [CMakeLists.txt](../CMakeLists.txt) および [AGENTS.md](../AGENTS.md) を参照する。

---

## 1. アーキテクチャ全体像と責務境界

### 1.1 ロジック全集約と I/O 分離
- **`ink_game`（raylib 非依存）**:
  - 戦闘ランのシミュレーション（`GameState`）だけでなく、ロビーのメタ進行・墨庵ショップ・設定/操作ヒントの純粋ロジック（`MetaState`：墨晶精算・購入判定・`RunConfig` 生成・シリアライズ文字列変換）も保持する。
  - これにより、戦闘・3 択報酬・エンドレス移行・墨庵購入まで、すべて raylib なしの Unity 単体テスト＋ASan/UBSan でヘッドレス検証できる。
- **`src/app/`（`inkstorm` 実行体）**:
  - ウィンドウ管理、入力収集、シーン遷移と UI 操作のブリッジ、アセット読み込み、セーブファイルの読み書き I/O、CLI オプション（`--headless-smoke`, `--smoke-frames`, `--showcase`, `--demo`, `--snap`, `--no-post`, `--no-save`, `--save-path`）のみを担当する。
- **モジュール依存境界の検証主体**:
  - 依存境界（`core`/`fluid`/`ecs`/`game`/`audio` の raylib 禁止およびターゲット間依存）は CMake ターゲットで機械的に強制する。
  - スタンドアロン配布用の `Makefile`（`tools/dist.mk`）は全モジュールの include パスを公開するため、配布ビルド成功は「配布アーカイブの自己完結性」の検証と位置づけ、依存境界の確認は CMake ビルド側で行う。

### 1.2 演出・VFX の所有境界（`ink_game` vs `ink_render`）
- **`ink_game` が所有するもの**:
  - ゲームプレイ結果に影響する状態のみ（ヒットストップ残時間、敵・弾・墨玉・社・設置ギミック、および各ティックの `GameEvent` 配列）。
  - 日本語 UI/コールアウト文字列および表示文字列 ID（`text_id`）は `ink_game` に持たせない。定義テーブルも `text_id` を持たず、`ink_render` が内容 ID（`EnemyDefId`、`UpgradeId`、`UpgradeAxis` 等）から文字列表を引く。
  - **公開読み取り契約**: `GameState` の `internal`（`GameInternal`）以外のすべてのフィールドは `ink_render` / `ink_audio` / `app` が `const GameState *` 経由で読んでよい。`internal` は `ink_game` 専有とし、`ink_render` / `ink_audio` / `app` は参照しない。
  - **ヘッダの分割構成**:
    - `game.h` → `fluid.h`, `ecs.h`, `meta.h`, `game_ids.h`, `game_events.h`, `game_components.h`, `game_internal.h`
    - `game_internal.h` → 自己完結（`<stdbool.h>`, `<stdint.h>`, `fluid.h`, `ecs.h`, `game_ids.h`, `game_events.h` のみ）
    - `audio.h` → `struct GameState` の前方宣言のみ（`game.h` を include しない）
    - `render.h` → `raylib.h`, `game.h`, `ui_view.h`, `sprite_rules.h`
    - `ui_view.h` → `game_ids.h`, `meta.h`（raylib 非依存）
    - `cli.h` / `frame_clock.h` / `input_latch.h` / `hint_queue.h` / `save.h` / `text_table.h` / `text_layout.h` / `vfx_rules.h` → 標準ヘッダと `game.h` / `meta.h` / `game_ids.h` のみ（raylib 非依存）
    - `menu_nav.h` → 標準ヘッダと `input_map.h` / `ui_view.h` のみ（`input_map.h` も `game.h` / `input_latch.h` / `ui_view.h` だけに依存し raylib 非依存）
    - `bgm_fade.h` → `audio.h` のみ（`BgmTrack` を参照するため。raylib 非依存）
    - 全モジュールのヘッダファイル名は `src/` 全体および raylib ヘッダ名と重複させない。
- **`ink_render` が所有するもの**:
  - 純粋な見た目 VFX（火花パーティクル、雷ボルト残像、渦印リング演出、画面シェイク、日本語ポップアップ文字／コールアウト）はすべて `RenderState` 内に固定長プールとして持つ。
  - 録画・視覚回帰時の再現性を保つため、`RenderState` 専用の明示 RNG（`uint64_t vfx_rng`）を持つ。
- **複数ティック消費契約（イベント取りこぼし防止）**:
  - `app` の固定 60Hz アキュムレータループ内で 1 描画フレームあたり複数回 `game_step()` が実行される場合でも、**各 `game_step()` 呼び出しの直後に毎回 `audio_consume_events()` と `render_consume_events()` を呼び出す**。
  - `render_update_vfx()` も固定刻み `INK_FIXED_DT` でティックごとに進めることで、描画が 0 回または複数ティックをまとめたフレームでもイベントが失われない契約とする。

### 1.3 シェーダ配置規約
- GLSL シェーダは `src/render/*.c` 内の **C 文字列リテラル埋め込み** を標準とする（`tools/check_shaders.py src` による静的検証と、`tools/package_dist.py` の `src/*.c` 収録条件の両方にそのまま一致する）。
- 将来外部 `.frag` へ分離する場合は、`tools/package_dist.py` の収録規則と `CMakeLists.txt` の `check_shaders.py` 対象を同一コミットで同時更新すること。

---

## 2. メモリ配置・固定上限・決定性契約

### 2.1 フラット固定長＋トップレベル単一ヒープ確保
- `FluidState`（160×90×汎用 4ch＋`solid` ≒ 約 0.88 MB）と `EcsWorld`＋`GameComponents`（最大 512 エンティティ ≒ 約 0.35 MB）等を含む `GameState` は合計 **約 1.3〜1.5 MB** のフラット構造体とする。
- スタック溢れと可変グローバル状態を避けるため、呼び出し側（`app`・CLI スモーク・Unity テストすべて）がトップレベルで 1 回だけヒープ確保する（`game_create()` / `game_destroy()` または `calloc(1, sizeof(GameState))` ＋ `game_init()`）。
- **60Hz ループ内の `malloc` / `free` は完全ゼロ** とする。

### 2.2 固定上限定数（初期基準値）
```c
#define INK_SCREEN_W             1280
#define INK_SCREEN_H             720
#define INK_TICK_HZ              60
#define INK_FIXED_DT             (1.0f / 60.0f)

#define FLUID_W                  160
#define FLUID_H                  90
#define FLUID_N                  (FLUID_W * FLUID_H)
#define FLUID_CELL_PX            ((float)INK_SCREEN_W / (float)FLUID_W) /* 160×90 では 8.0f */
#define FLUID_MAX_CHANNELS       4     /* 0:Ink, 1:Fire, 2:Storm, 3:Reserved */
/* fluid.h の _Static_assert: FLUID_W は偶数（Red-Black 分割）、セルは画面と同じ縦横比の正方形 */

#define ECS_MAX_ENTITIES         512
#define GAME_MAX_SHRINES         4     /* 基本1基使用、複数化への拡張余白 */
#define GAME_MAX_EVENTS          128
#define GAME_MAX_REWARD_CHOICES  3
#define GAME_MAX_OWNED_UPGRADES  32
#define GAME_MAX_OWNED_RELICS    16
#define GAME_MAX_PENDING_IMPULSE 8     /* 爆雷・三元共鳴の次ティック注入予約枠 */
```

### 2.3 決定性規約
- シミュレーション更新は常に固定刻み `INK_FIXED_DT`（60Hz）で行う。
- 乱数は `ink_core` の SplitMix64（明示状態 `uint64_t rng`）のみを使用し、`rand()` / `time()` を禁止する。
- 決定性一致の対象は **同一ビルド・同一 `RunConfig`・同一ティック入力列（`BrushInput` + `GameCommand`）** とする。
- 状態ハッシュ（`fluid_hash` / `game_hash` / `game_hash_sections`）はフィールドを固定順に `ink_hash_*`（FNV-1a 64bit）で加算し、構造体パディングや計測時間（`solve_ms`）をハッシュ対象から除外する。
- `game_hash_sections(const GameState *g, uint64_t out[GAME_HASH_SECTION_COUNT])` は次の 7 セクションを個別に算出し、`game_hash(const GameState *g)` はこれらを番号順に `ink_hash_u64` で畳み込む：
  1. `GAME_HASH_CORE`: `ticks`, `rng`, `phase`, `phase_t`, `hitstop_t`, `config`
  2. `GAME_HASH_FLUID`: `fluid_hash(&g->fluid)`
  3. `GAME_HASH_ECS`: `gen` / `comp_mask` / `alive` / `pending_destroy` の `0..high_water-1`、`free_list[0..free_count)`、各カウンタ
  4. `GAME_HASH_COMPONENTS`: 生存エンティティの、マスクを持つ Component のみ（index 昇順・宣言順）
  5. `GAME_HASH_RUN`: `player`, `shrines`, `shrine_count`, `run`, `ryu_body`
  6. `GAME_HASH_EVENTS`: `event_count`, `events[0..event_count)`, `dropped_events`, `pending_impulse_count`, `pending_impulses[0..pending_impulse_count)`
  7. `GAME_HASH_INTERNAL`: `GameInternal`（定義順）

---

## 3. 固定 60Hz Simulation Pipeline とフェーズ遷移契約

### 3.1 ラン内フェーズ一覧
```c
typedef enum {
    RUN_PHASE_PLAYING = 0,         /* 通常戦闘・ボス戦闘 */
    RUN_PHASE_WAVE_AFTERMATH,      /* 墨還り(0.6s) + 墨玉自動回収 + 流体余韻(1.0s) */
    RUN_PHASE_REWARD,              /* 強化札3択 / 大遺物3択（全シム停止） */
    RUN_PHASE_ACT_TRANSITION,      /* 幕転換題字(2.0s)・流体リセット・次幕地形配置 */
    RUN_PHASE_SHRINE_BREAK,        /* 社砕け演出(1.5s)（全戦闘停止） */
    RUN_PHASE_VICTORY,             /* 勝利リザルト（全シム停止） */
    RUN_PHASE_GAME_OVER            /* 敗北リザルト（全シム停止） */
} RunPhase;
```
- **フェーズと `ticks` / `phase_t` の契約**:
  - ラン開始は `RUN_PHASE_ACT_TRANSITION`（2.0 秒）から始まり、終了時に第 1 ウェーブを開始して `RUN_PHASE_PLAYING` へ移る。
  - `ticks` と `phase_t` は `PLAYING`（ヒットストップ中を含む）・`WAVE_AFTERMATH`・`ACT_TRANSITION`・`SHRINE_BREAK` のティックで進み、`REWARD`・`VICTORY`・`GAME_OVER` のティックでは進まない。1 回の `game_step` で `ticks` が増えるのは 0 か 1 で、0 になるのはステップ後のフェーズが `REWARD` / `VICTORY` / `GAME_OVER` のときだけである。`phase_t` は遷移時に 0 へ戻る。
  - 最終幕（`act_index == act_count - 1`、P0 では `act_count == 1`）のボスを倒すと、大遺物と社の回復なしで `WAVE_AFTERMATH` → `VICTORY` へ移る。
  - `CMD_ABORT_RUN` は `PLAYING` / `WAVE_AFTERMATH` / `REWARD` / `ACT_TRANSITION` / `SHRINE_BREAK` で有効で、直ちに `GAME_OVER` へ移り `EV_GAME_OVER`（`EVF_RUN_ABORTED`）を出す。`VICTORY` / `GAME_OVER` では no-op。社 HP 0 による敗北では、`SHRINE_BREAK` に入るティックで `EV_GAME_OVER` を出す。
  - `CMD_START_ENDLESS` と `CMD_PICK_ENDLESS_CRYSTAL` はエンドレスを実装するまで常に no-op とする。

### 3.2 `game_step` の 9 段階実行順序
`game_step(GameState *g, const BrushInput *in, const GameCommand *cmd)` は毎ティック必ず以下の固定順序で実行する。

1. **イベントクリア & コマンド処理**
   - `g->event_count = 0` にリセットし、入力（`BrushInput`・`GameCommand`）を §7.2 の境界規則で検証する。
   - **先にコマンドを処理してから**、その時点のフェーズで停止フェーズの分岐を判定する。報酬を決めたティックから次のウェーブが動き、そのティックで `ticks` も進む。
   - コマンド有効条件：`CMD_ABORT_RUN` は `PLAYING` / `WAVE_AFTERMATH` / `REWARD` / `ACT_TRANSITION` / `SHRINE_BREAK` で有効、`CMD_PICK_REWARD` / `CMD_REROLL_REWARD` / `CMD_REPAIR_SHRINE` / `CMD_PICK_ENDLESS_CRYSTAL` は `RUN_PHASE_REWARD` のみ有効、`CMD_START_ENDLESS` は `RUN_PHASE_VICTORY` のみ有効。
   - 無効なコマンド（現フェーズ非対応・範囲外 `arg`・リロール残数 0・社 HP 満タン時の修繕等）は **状態を変更せず no-op で無視** する。
   - **停止フェーズ分岐**:
     - `RUN_PHASE_REWARD` / `RUN_PHASE_VICTORY` / `RUN_PHASE_GAME_OVER`：以降のステップをスキップ（流体・タイマー・`ticks` 完全停止。ポーズ解除や報酬決定からの再開ティックでは、`app` 側がフレーム `accumulator` を 0 へリセットしてから再開する）。
     - `RUN_PHASE_SHRINE_BREAK`：演出タイマーのみ進め、1.5 秒経過で `RUN_PHASE_GAME_OVER` へ遷移（流体・戦闘は停止）。
     - `RUN_PHASE_ACT_TRANSITION`：演出タイマー（2.0 秒）を進め、開始フレームで `fluid_reset()`＋次幕の `solid` 地形配置を実施し、終了時に次幕第 1 ウェーブを開始して `RUN_PHASE_PLAYING` へ戻る。
     - `RUN_PHASE_WAVE_AFTERMATH`：新規スポーン・敵攻撃・社被弾・プレイヤー技を止め、**ステップ 4（`fluid_step`）＋墨還り消滅（0.6 秒）＋場の墨玉自動回収** のみ進める。1.0 秒経過で `RUN_PHASE_REWARD`（または最終幕ボス撃破なら `RUN_PHASE_VICTORY`）へ遷移。
2. **ヒットストップ更新**
   - `g->hitstop_t > 0` の場合は `hitstop_t -= dt` のみ進め、物理・戦闘ステップ（3〜8）をスキップ（`g->ticks++` と `g->phase_t` は進める）。
3. **保留インパルス注入 & プレイヤー入力（風筆・渦印・風斬り）＋入力系フック**
   - 前ティックの爆雷・三元共鳴で予約された放射インパルス（`pending_impulses[]`）を `fluid_add_radial_impulse()` で注入しクリア。
   - 風筆／渦印／風斬りの墨消費・かすれ筆判定・硯回復・CD 更新・墨昇華減衰。
   - 流体速度場・墨・能動属性チャンネルへ注入し、`on_stroke` / `on_cyclone` / `on_gale` フックを取得順に実行。
4. **Fluid Engine ステップ（`fluid_step`）**
   - 渦度閉じ込め → 圧力投影（`solid` no-slip 境界適用） → 速度移流 → 圧力投影 → アクティブチャンネル（0..2）のスカラー移流＆減衰 → 統計更新。
5. **属性反応ステップ（`elements_step`）**
   - [game-design.md](game-design.md) §7.2 の順序に厳密準拠：
     1. セル単位反応（紅蓮墨嵐の燃焼・墨上の雷成長）
     2. 三元共鳴の判定（成立ブロックはそのティックの爆雷判定から除外）
     3. 爆雷の判定
     4. 敵への状態付与と属性ダメージ（墨印・炎上・帯電・雷墨連鎖）
     5. 敵弾の属性昇格・加速（炎上弾・雷撃弾・未偏向弾の雷加速）
   - 炎・雷の最大濃度がいずれも 0.14 以下のティックは 1〜3 の全格子走査を省く（どの規則も 0.14 超を要するため結果は同一）。反応フックや三元共鳴の後は最大値を無効化して通常どおり走査する。
   - 完了後に必ず `fluid_sanitize_channels(&g->fluid)` を呼び、`solid` セル 0 固定・`[0, max_dye]` クランプ・非有限値除去を保証。
   - 爆雷・三元共鳴の放射インパルスは `pending_impulses[]` へ積み、次ティックのステップ 3 で速度クランプ付き注入する。
   - 反応ごとに `on_reaction` フック実行および `GameEvent` 発行。
6. **ステージギミック & 増援スポーン**
   - 第 3 幕の環境風（山颪）予告/吹送・霊柱の点灯/注入更新。
   - 増援クールダウンと同時出現上限に基づく敵スポーン（撃破遷移判定はまだ行わない）。
7. **ECS 戦闘システム群（index 昇順の決定性走査）**
   - **7a. 敵 AI & 幕ボス行動**：流体サンプリング、移動、射撃、ボス予兆・フェーズ遷移・特殊技。
   - **7b. 弾更新**：流体偏向、偏向追尾、社・敵・`solid` 衝突（跳弾雷球のみ反射）。
   - **7c. 物理・叩きつけ判定**：外周壁および `fluid_is_solid()` / `fluid_solid_normal()` による柱叩きつけ、敵同士衝突、`on_hit` / `on_kill` フック。
   - **7d. 墨玉（InkOrb）**：流体移流・`solid` スライド・筆近傍吸引・回収。
   - **7e. 設置ギミック & `on_tick` フック**。
8. **遅延削除フラッシュ & 遷移優先判定（戦闘確定）**
   - `ecs_flush_destroy(&g->ecs)` で削除予約済みエンティティを一括回収し `gen++`。
   - **同一ティック同時発生の遷移優先順位**（上にある条件が勝つ）：
     1. **社 HP == 0（敗北最優先）**：全戦闘を停止し `RUN_PHASE_SHRINE_BREAK`（1.5 秒）へ遷移（`EV_GAME_OVER` 発行）。
     - ※ 各幕開始（`waves_prepare_act`）および各ウェーブ開始（`waves_start_wave`）時に `hitstop_t` と `pending_impulse_count` をリセットし、前ウェーブ／前幕終了時のヒットストップや保留インパルスを持ち越さない。
     2. **ボス撃破**（ボスウェーブ中）：墨玉 8＋社 HP +3（第 1・2 幕、または第 3 幕は勝利リザルトへ）。残存敵/敵弾へ墨還り開始し `RUN_PHASE_WAVE_AFTERMATH` へ遷移（`EV_WAVE_CLEAR` / `EV_ACT_CLEAR`）。
     3. **通常ウェーブノルマ到達**：残存敵/敵弾へ墨還り開始し `RUN_PHASE_WAVE_AFTERMATH` へ遷移（`EV_WAVE_CLEAR`）。
   - `g->ticks++`。
9. **ティック後（呼び出し側による毎ティック読み取り専用消費）**
   - `audio_consume_events(audio, g)`
   - `render_consume_events(render, g)` → `render_update_vfx(render, g, INK_FIXED_DT)`

### 3.3 デバッグ API と自動入力（テスト・開発用）
テスト・ベンチ・ヘッドレススモーク・`--smoke-frames`・見本帳（`src/app/showcase.c`）・自動デモおよびスナップ撮影（`src/app/demo.c`）用に次の API を `game.h` に定義する。通常プレイ経路（`src/app/app_loop.c`）からは呼ばない。

| 関数 | 役割 |
| :--- | :--- |
| `game_debug_spawn_enemy(g, def, x, y)` | `EnemyDef` の生成関数で通常のスポーンと同じ Component・初期値を付与（位置の抽選のみ省略）。ボスなら `run.boss_id` も設定する |
| `game_debug_spawn_projectile(g, kind, x, y, vx, vy)` | 通常の発射と同じ初期化で敵弾を生成する |
| `game_debug_spawn_orb(g, x, y)` | 墨玉のドロップと同じ初期化で生成する |
| `game_debug_start_act(g, act_index)` | 指定幕（`0..META_MAX_BEST_ACT-1`）へ切り替え、`endless_loop = act_index / GAME_MAIN_ACT_COUNT` を同期し、旧幕の残存 ECS エンティティを破棄した上で `waves_prepare_act(g)` を実行し `RUN_PHASE_ACT_TRANSITION` へ移す（範囲外は no-op） |
| `game_debug_start_wave(g, wave_index)` | 現在の幕のウェーブ `wave_index` を通常のウェーブ開始と同じ処理で開始する（範囲外は no-op） |
| `game_debug_force_wave_clear(g)` | `PLAYING` のみ有効。通常ウェーブはノルマ到達扱い、ボスウェーブは通常のダメージ経路でボスへ致死ダメージを与える（遷移は次ティックの段 8） |
| `game_autoplay_input(g)` / `game_autoplay_command(g)` | 状態のみから決まる決定的な自動入力を返す（`g` を変更せず `g->rng` も消費しない） |

---

## 4. Fluid Engine API 設計（`src/fluid/`）

`ink_fluid` はゲーム属性の意味を一切知らず、汎用スカラーチャンネルと障害物境界を持つ純粋な Stable Fluids ソルバとして設計する。

### 4.1 設定・状態構造体と単位契約
- **座標・距離単位**: `px, py, pw, ph, radius_px` は画面論理 px（`0..1280`, `0..720`）。流体セル `(x, y)` は `[c·x, c·x+c)` px（`c = FLUID_CELL_PX`、160×90 では 8px）を占め、中心は `(c·x + c/2, c·y + c/2)` とする。`rad_cells`・速度 `(u, v, fx, fy, strength, peak_speed)` は格子単位（セル / セル/s）。ゲーム側での px/s 換算は `× FLUID_CELL_PX`。`dye` は無次元 `[0, max_dye]`。
- **解像度非依存の規則（格子を 240×135 などへ変えても破綻しないこと）**: ゲーム側のセル探索窓は px 半径から `fluid_cell_reach(radius_px)`（`ceil(r / FLUID_CELL_PX)`）で求める。爆雷・三元共鳴のブロックは画面 64px 四方（`GAME_REACTION_BLOCK_PX`、20×12 ブロック）でセル中心の px 座標から決める。セル数の閾値（紅蓮 24・雷墨連鎖 10・三元 8・爆雷 6）は 8px セル基準で、セル面積比 `GAME_CELL_COUNT_SCALE`（160×90 では厳密に `1.0f`）を掛けて比較する。反応・ギミックが流体へ渡す半径と速度（三元共鳴・爆雷の注入、霊柱・落雷・鵺の軌跡・龍の渦・鎌鼬・山颪）は px と px/s で定義し、`GAME_PX_TO_CELLS` で格子単位へ換算する（160×90 では従来の値と完全に一致）。
- **非有限値・範囲外入力の契約**:
  - 注入系・障害物設定系の引数に `NaN` / `Inf` または非正半径が渡された場合は安全に no-op 拒否する。
  - 範囲外座標は、`fluid_sample` では有効格子域へクランプ、`fluid_splat` / `fluid_swirl` / `fluid_add_radial_impulse` では格子と交差する有効セル範囲のみ適用する。
- **配列長・NULL 契約**:
  - `dye_add` / `out_dye` は `NULL` 許容。非 `NULL` の場合、呼び出し側は要素数 `FLUID_MAX_CHANNELS` 以上の配列を渡すこと。`fluid_sample` は `out_u`, `out_v`, `out_dye` いずれも個別に `NULL` 許容。

```c
typedef struct {
    int project_iters;                    /* 既定: 12（調整可能） */
    float vorticity_eps;                  /* 既定: 9.5f */
    float vorticity_thresh;               /* 既定: 0.35f */
    float max_vel;                        /* 既定: 125.0f */
    float max_dye;                        /* 既定: 2.5f */
    float vel_damp;                       /* 速度減衰 */
    float dye_damp[FLUID_MAX_CHANNELS];   /* チャンネル別減衰率 */
    int active_channels;                  /* 既定: 3（最大 FLUID_MAX_CHANNELS） */
} FluidConfig;

typedef struct {
    float max_vel;
    float max_vort;
    float max_dye[FLUID_MAX_CHANNELS];
    double solve_ms;                      /* 計測専用（決定性ハッシュ対象外） */
} FluidStats;

typedef struct {
    FluidConfig cfg;
    FluidStats stats;
    uint64_t steps;
    float u[FLUID_N], v[FLUID_N];
    float u0[FLUID_N], v0[FLUID_N];
    float div[FLUID_N], p[FLUID_N], curl[FLUID_N];
    float dye[FLUID_MAX_CHANNELS][FLUID_N];
    float dye0[FLUID_MAX_CHANNELS][FLUID_N];
    uint8_t solid[FLUID_N];               /* 0=流体, 1=障害物/外周壁 */
    /* 内部ジオメトリキャッシュ（solid 変更時のみ再構築、決定性ハッシュ対象外） */
    float inv_neighbors[FLUID_N];
    float inv_rb[2][FLUID_N / 2];
    uint8_t near_wall[FLUID_N];
    uint8_t geometry_valid;
} FluidState;
```

### 4.2 公開関数契約
```c
FluidConfig fluid_config_default(void);
void fluid_init(FluidState *f, const FluidConfig *cfg);
void fluid_reset(FluidState *f);          /* 速度・染料を0初期化（solidは維持） */

/* 障害物（solid 境界）操作・クエリ */
void fluid_clear_solids(FluidState *f);   /* 外周1セルのみ壁に戻す */
void fluid_set_solid_rect(FluidState *f, float px, float py, float pw, float ph, bool solid);
void fluid_set_solid_circle(FluidState *f, float px, float py, float radius_px, bool solid);
bool fluid_is_solid(const FluidState *f, float px, float py);
bool fluid_solid_normal(const FluidState *f, float px, float py, float *nx, float *ny);

/* 数値ステップ・注入・サンプリング */
void fluid_step(FluidState *f, float dt);
void fluid_splat(FluidState *f, float px, float py, float rad_cells,
                 float fx, float fy, const float *dye_add);
void fluid_swirl(FluidState *f, float px, float py, float rad_cells,
                 float strength, const float *dye_add);
void fluid_add_radial_impulse(FluidState *f, float px, float py,
                              float rad_cells, float peak_speed);
void fluid_sample(const FluidState *f, float px, float py,
                  float *out_u, float *out_v, float *out_dye);

/* game/elements・render 向けの直列2フェーズ連携アクセサ */
const float *fluid_channel_const(const FluidState *f, int ch);
float *fluid_channel_mut(FluidState *f, int ch); /* 非アクティブなチャンネルは NULL。借用期限: 次回 fluid_step / fluid_reset まで */
const float *fluid_u_const(const FluidState *f);   /* 読み取り専用の速度 x [セル/秒] */
const float *fluid_v_const(const FluidState *f);   /* 読み取り専用の速度 y [セル/秒] */
const uint8_t *fluid_solid_const(const FluidState *f);
int fluid_cell_reach(float radius_px);             /* ceil(radius_px / FLUID_CELL_PX)、[0, FLUID_W] */
void fluid_sanitize_channels(FluidState *f);     /* elements_step 後にアクティブなチャンネルの solid=0・[0,max_dye]・有限性を保証 */

/* 決定性・健全性検証 */
uint64_t fluid_hash(const FluidState *f);
bool fluid_validate_finite(const FluidState *f);
```

### 4.3 `solid` 境界条件・壁横断防止と性能予算
- **no-slip & ノイマン境界**:
  - `solid[i] != 0` のセルでは `u[i] = v[i] = 0` および全 `dye[ch][i] = 0` を強制する。
  - 圧力投影の Gauss-Seidel 反復では、隣接セルが `solid` の場合に自セルの圧力 `p[i]` を代用（`dp/dn = 0`）し、流れの反射・回り込み・渦剥離を再現する。
- **薄い障害物の壁横断（トンネリング）防止**:
  - Semi-Lagrangian 移流のバックトレースは終点 1 点のみを `solid` 判定せず、**始点から終点までステップ長 `<= 0.5` セルの短いレイマーチ** を行い、途中で `solid` セルへ入った場合は直前の非 `solid` 座標で停止してサンプリングする（2 セル幅の鳥居柱の反対側から染料や速度が漏れるのを防ぐ）。
- **作業領域**: 圧力投影（Red-Black Gauss-Seidel）の作業配列は `FluidState` 内の `rb_div` / `rb_p`（決定性ハッシュ対象外）に置き、112.5 KiB をスタックに確保しない。非アクティブなチャンネルはソルバ・注入・可変アクセサのいずれからも書けないため、`fluid_sanitize_channels` はアクティブなチャンネルだけを走査する。
- **性能予算（Release ビルド・160×90・既定 12 反復・実戦闘負荷）**:
  - `fluid_step`: **< 2.0 ms / tick**
  - `game_step` 全体: **< 4.0 ms / tick**

---

## 5. Hybrid ECS 設計（`src/ecs/` ＋ `src/game/`）

### 5.1 インデックスレジストリ型 `ink_ecs`（依存: `ink_core` のみ）
`ink_ecs` はゲーム固有の型を知らず、エンティティ寿命・世代検証・64bit コンポーネントマスク・遅延削除のみを管理する。

```c
#define ECS_INVALID_INDEX UINT32_MAX

typedef struct {
    uint32_t index;
    uint32_t gen;
} EntityId;

typedef struct {
    uint32_t gen[ECS_MAX_ENTITIES];
    uint64_t comp_mask[ECS_MAX_ENTITIES];
    uint8_t alive[ECS_MAX_ENTITIES];
    uint8_t pending_destroy[ECS_MAX_ENTITIES];
    uint32_t free_list[ECS_MAX_ENTITIES];
    uint32_t free_count;
    uint32_t alive_count;
    uint32_t high_water;                  /* 走査上限の最適化用 */
} EcsWorld;

void ecs_init(EcsWorld *w);
EntityId ecs_spawn(EcsWorld *w);          /* 満杯時は {ECS_INVALID_INDEX, 0} */
bool ecs_is_alive(const EcsWorld *w, EntityId id); /* alive && !pending_destroy && gen一致 */
void ecs_destroy(EcsWorld *w, EntityId id); /* 遅延削除フラグのみセット */
void ecs_flush_destroy(EcsWorld *w);      /* 一括回収 + gen++ + comp_mask=0 */
bool ecs_add_mask(EcsWorld *w, EntityId id, uint64_t mask);
bool ecs_remove_mask(EcsWorld *w, EntityId id, uint64_t mask);
bool ecs_has_mask(const EcsWorld *w, EntityId id, uint64_t mask);
uint32_t ecs_alive_count(const EcsWorld *w);
EntityId ecs_entity_at(const EcsWorld *w, uint32_t index);
```

- 削除予約中（`pending_destroy[i] == 1`）のエンティティに対して `ecs_is_alive()` は `false` を返し、以降のコンポーネント変更や走査対象から即座に除外する。
- 走査は各システムの開始時に `high_water` を固定し、`0 .. high_water - 1` の昇順ループで決定的な更新順を保証する（走査中に生成されたエンティティは次のシステムから対象とする）。

### 5.2 `ink_game` 側 Component 配列（`GameComponents`）
型付き固定配列を `ink_game` 側に配置し、アクセサ関数が `ecs_is_alive` ＋ `comp_mask` を検証した上でポインタを返す（`void *` キャスト排除）。

| ビット | Component | 主なフィールド |
| :--- | :--- | :--- |
| `1ULL << 0` | `TransformComp` | `x, y, radius, facing, dissolving, dissolve_t` |
| `1ULL << 1` | `VelocityComp` | `vx, vy, mass_tier, drag, fluid_coupling` |
| `1ULL << 2` | `HealthComp` | `hp, max_hp, flash_t, invuln_t` |
| `1ULL << 3` | `StatusComp` | `ink_mark_t, burn_acc, charge_t, knockback_t, stun_t, barrier_t` |
| `1ULL << 4` | `EnemyComp` | `def_id, is_elite, elite_flags, phase, fire_cd, ai_state, absorbed_ink, windup` |
| `1ULL << 5` | `BossComp` | `pattern, pattern_timer, phase_idx, gale_dx, gale_dy, gale_t, part_id, parent_id, telegraph_pattern, telegraph` |
| `1ULL << 6` | `ProjComp` | `kind, deflected, burning, electrified, accelerated, pierce, bounces_left, age, trail[20], trail_n` |
| `1ULL << 7` | `InkOrbComp` | `value, age` |
| `1ULL << 8` | `GimmickComp` | `kind, elem_kind, timer, param_a, param_b`（第 3 幕霊柱・環境源等） |

- **多部位ボス（P2 拡張：一筆龍）の表現余白**: `BossComp.part_id` と `parent_id`（`EntityId`）により、龍脈本体と各部位（龍玉／頭・胴・尾等）を独立した ECS エンティティ（個別 `Transform` / `Health` / `Status`）として表現し、部位撃破で本体フェーズ遷移を駆動できる。

### 5.3 ECS 外で管理するものと `GameState` 構成
`GameState` は次のフィールドをフラットに保持し、`internal`（`GameInternal`）以外を公開読み取り契約として `ink_render` / `ink_audio` / `app` に開示する：
- `FluidState fluid`（格子配列）
- `EcsWorld ecs`・`GameComponents comps`
- `RunConfig config`・`uint64_t rng`・`uint64_t ticks`・`RunPhase phase`・`float phase_t`・`float hitstop_t`
- `PlayerState player`（墨・かすれ筆・筆先・CD・`gale_charges`/`gale_max_charges`・墨昇華）
- `ShrineState shrines[GAME_MAX_SHRINES]`・`int32_t shrine_count`（基本 1 基使用、複数化余白）
- `RunProgress run`（幕・ウェーブ・ノルマ・`endless_loop`/`endless_acts_cleared`/`bonus_crystals`・撃破・スコア・チェイン・反応回数・リロール・`RewardOffer`（`repair_amount`/`crystal_reward` 含む）・所持アップグレード・所持大遺物 `relics[]` / `relic_count`・`boss_id`・`run_settled`）
- `GameEvent events[GAME_MAX_EVENTS]`・`int32_t event_count`・`uint32_t dropped_events`
- `PendingImpulse pending_impulses[GAME_MAX_PENDING_IMPULSE]`・`int32_t pending_impulse_count`
- `RyuBodyState ryu_body`（多部位一筆龍の公開読み取り状態：`head_id`, `orb_ids[3]`, `orb_broken_mask`, `active_orb_slot`, 頭部軌跡リングバッファ `trail[GAME_RYU_TRAIL_CAP]` / `trail_head` / `trail_count`。`ink_render` が `internal` に触れずに胴の軌跡と龍玉破壊区間を描画するための公開契約）
- `GameInternal internal`（`ink_game` 専有。`RunModifiers`・各種タイマー・累積距離等を隔離し、描画・音・app は読まない）
- 描画専用 VFX プール（`RenderState` 内）

---

## 6. データ駆動定義 & ビルド効果モデル

### 6.1 ハイブリッド効果モデル（`RunModifiers` ＋ 7 限定フック）
1. **数値/倍率系（`RunModifiers`）**:
   - 筆スタイル・メタ強化・所持アップグレード・所持大遺物から `roguelike_recompute_modifiers(g)` で一括再計算する（加算を適用してから乗算を適用する規則で統一）。
2. **ルール変更系（7 種の限定イベントフック）**:
   - 所持アップグレード／大遺物の取得順（決定的）に以下のフックを呼び出す。未登録フックは `NULL` でスキップ。
   - 各フックは引数を `const` の構造体 1 つで受け取る（例：`void (*on_hit)(const HitHookArgs *a)`、構造体内に `GameState *g`, `int lv` 等を格納する）。個別のポインタ引数にすると、書き換えないフックで静的解析（cppcheck `constParameterCallback`）に抵触するためである。
     - `on_stroke(const StrokeHookArgs *a)`（`g, lv, x, y, vx, vy`）
     - `on_cyclone(const CycloneHookArgs *a)`（`g, lv, x, y`）
     - `on_gale(const GaleHookArgs *a)`（`g, lv, x, y, dx, dy`）
     - `on_hit(const HitHookArgs *a)`（`g, lv, target, dmg_inout, dmg_flags`）
     - `on_kill(const KillHookArgs *a)`（`g, lv, victim, x, y, dmg_flags`）
     - `on_reaction(const ReactionHookArgs *a)`（`g, lv, kind, x, y, intensity`）
     - `on_tick(const TickHookArgs *a)`（`g, lv, dt`）

### 6.2 報酬提示・候補不足時の代替規則
- `RewardOffer` は現在提示中の候補数 `choice_count`（`0..3`）と種類（強化札／大遺物／エンドレス代替）を保持する。
- **強化札の候補不足**: 未 Lv3 の札が 3 件未満なら出せる件数だけ提示する。0 件（全札 Lv3：エンドレス等）になった場合は「社を修繕 +1（`CMD_REPAIR_SHRINE`）」と「墨晶 +10（`CMD_PICK_ENDLESS_CRYSTAL`）」の 2 択へフォールバックする。
- **大遺物の候補不足**: 解放済み・未所持の大遺物が 3 件未満なら出せる件数だけ提示する。0 件（エンドレスで全遺物取得済み）になった場合は「社を修繕 +3（最大 HP 超過分は 1 につき墨晶 +15 へ変換）」と「墨晶 +30」の代替 2 択へフォールバックする。

### 6.3 C11 定義テーブルスキーマ
各コンテンツは `src/game/` 配下の専用ファイルへ `const` 指示付き初期化子配列として定義する。定義テーブルは表示文字列 ID（`text_id`）を持たず、`ink_render` が内容 ID（`EnemyDefId`, `UpgradeId`, `UpgradeAxis` 等）から文字列表を引く。
- **`BrushStyleDef`**: `id`, 初期 `RunModifiers` 差分、推奨軸、固有フック
- **`EnemyDef`**: `id`, `base_hp`, `radius`, `mass_tier`, `fluid_coupling`, `score`, `orb_count`, `ai_step_fn`, `spawn_fn`
- **`UpgradeDef`**: `id`, `axis`, `max_level`（3）, `weight`, `apply_mods_fn`, 7 フック（全 28 種 `UPG_COUNT = 28`：各軸 6 種 × 4 軸 ＋ 共通 4 種）
- **`RelicDef`**: `id`, `apply_mods_fn`, 7 フック（全 15 種 `RELIC_COUNT = 15`：初期解放 9 種 ＋ 墨庵解放 6 種。解放枠 `0..5` は `game_relic_unlock_slot` / `game_is_relic_unlocked` で `RunConfig.unlocked_relic_mask` と照合する）
- **`ActDef`**: `act_index`, `wave_count`（3）, `boss_def_id`, `setup_stage_fn`, ウェーブ敵重みテーブル

---

## 7. イベントキュー & コマンド設計

### 7.1 フレームイベント（`ink_game` → `ink_audio` / `ink_render`）
- 上限 `GAME_MAX_EVENTS (128)` を超えた場合、新規イベントは安全にドロップし、`g->dropped_events` カウンタを飽和加算する（メモリ破壊・境界外書き込み禁止）。
- イベントには日本語文字列を含めず、種別 enum と数値パラメータ（`x, y, x2, y2, value, flags`）のみを載せる。同じティック内のイベントはパイプラインの実行順（§3.2）に並ぶ。

```c
typedef enum {
    EV_NONE = 0,
    EV_KILL,
    EV_DEFLECT,
    EV_SLAM,
    EV_IGNITE,
    EV_STORM_ZAP,       /* (x, y) -> (x2, y2) の電撃線 */
    EV_THUNDERBURST,    /* 炎×雷 爆雷 */
    EV_CATACLYSM,       /* 墨×炎×雷 三元共鳴 */
    EV_CYCLONE,
    EV_GALE,
    EV_ORB_PICKUP,
    EV_SHRINE_HIT,
    EV_SURGE_START,
    EV_BOSS_ENTER,
    EV_BOSS_PHASE,
    EV_WAVE_CLEAR,
    EV_ACT_CLEAR,
    EV_REWARD_PICK,
    EV_HINT_TRIGGER,    /* value = HintId（初回操作ヒント） */
    EV_WIND_TELEGRAPH,  /* 第3幕 山颪の予告 */
    EV_GAME_OVER,
    EV_VICTORY,
    EV_ENEMY_INTRO,     /* value = EnemyDefId（初登場ウェーブ開始時） */
    EV_REWARD_REROLL,   /* value = 残りリロール回数 */
    EV_SKILL_DENIED,    /* value = SkillId（墨不足または CD 中） */
    EV_BOSS_PART_BREAK, /* value = part_id（0=炎玉, 1=雷玉, 2=風玉） */
    EV_TYPE_COUNT
} GameEventType;

#define EVF_SLAM_WALL      (UINT32_C(1) << 8)
#define EVF_SLAM_ENEMY     (UINT32_C(1) << 9)
#define EVF_ORB_AUTO       (UINT32_C(1) << 10)
#define EVF_RUN_ABORTED    (UINT32_C(1) << 11)
#define EVF_REWARD_REPAIR  (UINT32_C(1) << 12)
#define EVF_REWARD_RELIC   (UINT32_C(1) << 13)
#define EVF_REWARD_CRYSTAL (UINT32_C(1) << 14)

typedef struct {
    GameEventType type;
    float x, y;
    float x2, y2;
    int32_t value;
    uint32_t flags;
} GameEvent;
```

| イベント | `x, y` | `x2, y2` | `value` | `flags` |
| :--- | :--- | :--- | :--- | :--- |
| `EV_KILL` | 撃破位置 | 衝撃の向き（単位、なければ 0） | `EnemyDefId` | 致命打の `DMG_*` |
| `EV_DEFLECT` | 弾の位置 | 偏向後の向き（単位） | `ProjKind` | 0 |
| `EV_SLAM` | 衝突点 | 壁の法線または相手への向き | `EnemyDefId`（飛ばされた側） | `EVF_SLAM_WALL` / `EVF_SLAM_ENEMY` |
| `EV_IGNITE` | 燃焼セルの重心（px） | — | 燃焼セル数 | 0（24 セル以上・3.2 秒間隔） |
| `EV_STORM_ZAP` | 始点 | 終点 | ホップ番号（0 = 初撃） | 0 |
| `EV_THUNDERBURST` | 爆雷ブロック（64px 四方）内の爆雷セル重心（px） | `radius_px`（`72 * rmul`）, `0` | ブロック内の爆雷セル数（`>= 6 × GAME_CELL_COUNT_SCALE`） | 0 |
| `EV_CATACLYSM` | 共鳴ブロック（64px 四方）内の共鳴セル重心（px） | `96.0f`, `0` | ブロック内の共鳴セル数（`>= 8 × GAME_CELL_COUNT_SCALE`） | 0 |
| `EV_CYCLONE` / `EV_GALE` | 中心 / 始点 | — / 向き（単位） | 半径（px 整数）/ 0 | 0 |
| `EV_ORB_PICKUP` | 回収位置 | — | 墨玉の価値（16） | `EVF_ORB_AUTO`（自動回収時） |
| `EV_SHRINE_HIT` | 社の位置 | 被弾元への向き（単位） | 被弾後の HP | 0 |
| `EV_SURGE_START` | 筆先 | — | チェイン数 | 0（非発動→発動の瞬間のみ） |
| `EV_BOSS_ENTER` / `EV_BOSS_PHASE` | ボスの位置 | — | `EnemyDefId` / 新フェーズ番号（多部位一筆龍：`2`=点睛、`3`=昇龍） | 0 |
| `EV_BOSS_PART_BREAK` | 破壊された龍玉の座標 (px) | `0, 0` | `part_id`（`0`=炎玉, `1`=雷玉, `2`=風玉） | 致命打の `DMG_*` |
| `EV_WAVE_CLEAR` / `EV_ACT_CLEAR` | 社の位置 / ボスの位置 | — | `wave_index` / `act_index` | 0 |
| `EV_REWARD_PICK` | — | — | 強化札は `UpgradeId`、大遺物は `RelicId`、社修繕は -1、墨晶代替は獲得墨晶数（`10`/`30`） | 強化札=0、大遺物=`EVF_REWARD_RELIC`、修繕=`EVF_REWARD_REPAIR`、墨晶代替=`EVF_REWARD_CRYSTAL` |
| `EV_HINT_TRIGGER` | 関連座標（なければ社の座標） | `0, 0` | `HintId`（`0..HINT_COUNT-1`） | 0 |
| `EV_WIND_TELEGRAPH` | 風上の辺の中央座標（px） | 風向きの単位ベクトル `(dx, dy)` | 風向き番号（`0`:左→右、`1`:右→左、`2`:上→下、`3`:下→上） | 0 |
| `EV_GAME_OVER` / `EV_VICTORY` | 社の位置 / — | — | 0 / `act_index` | `EVF_RUN_ABORTED` / 0 |
| `EV_ENEMY_INTRO` | — | — | `EnemyDefId` | 0 |
| `EV_REWARD_REROLL` | — | — | 残りリロール回数 | 0 |
| `EV_SKILL_DENIED` | 筆先 | — | `SkillId` | 0 |

### 7.2 コマンド入力（`app` / テスト → `ink_game`）
```c
typedef struct {
    float mx, my, pmx, pmy;
    bool paint, cyclone, gale;
} BrushInput;

typedef enum {
    CMD_NONE = 0,
    CMD_PICK_REWARD,          /* arg = 0 .. choice_count-1 */
    CMD_REROLL_REWARD,        /* 残りリロール > 0 のみ有効 */
    CMD_REPAIR_SHRINE,        /* 報酬画面の「社を修繕」 */
    CMD_PICK_ENDLESS_CRYSTAL, /* エンドレス候補枯渇時の墨晶代替 */
    CMD_START_ENDLESS,        /* 勝利画面からエンドレス継続 */
    CMD_ABORT_RUN             /* ラン中断（敗北扱い） */
} GameCommandType;

typedef struct {
    GameCommandType type;
    int32_t arg;
} GameCommand;
```
- `void game_step(GameState *g, const BrushInput *in, const GameCommand *cmd);`（`in == NULL` は入力なし、`cmd == NULL` は `CMD_NONE` と同義）。
- **境界検証**: `BrushInput` の 4 座標（`mx, my, pmx, pmy`）のいずれかが非有限（NaN / Inf）なら、そのティックは `in == NULL` と同じに扱う。有限なら画面外の値もそのまま使う（流体は格子と交差する範囲だけに注入する）。`GameCommand.type` が `GameCommandType` の範囲外なら `CMD_NONE` として扱う。

---

## 8. メタ進行（`MetaState`）＆セーブデータ仕様

### 8.1 純粋ロジック層（`src/game/meta.h`）
設定値と操作ヒントの表示済みフラグも `MetaState` に統合し、単一のセーブファイルで管理する（[game-design.md](game-design.md) §10・§11.2・§11.3・§13 準拠）。

```c
#define META_SAVE_VERSION   1
#define META_MAX_CRYSTALS   9999999
#define META_MAX_SCORE      999999999
#define META_MAX_COUNTERS   9999999
#define ENDLESS_MAX_LOOP_N  100
#define META_SERIALIZE_CAP  640

typedef enum {
    META_UPG_SHRINE_HP = 0,     /* 最大3段階: 社最大HP +1/段 */
    META_UPG_INK_MAX,           /* 最大3段階: 墨上限 +10/段 */
    META_UPG_REROLLS,           /* 最大2段階: 各幕リロール +1/段 */
    META_UPG_INK_REGEN,         /* 最大3段階: 墨回復速度 +10%/段 */
    META_UPG_COUNT
} MetaUpgradeKind;

typedef enum {
    UI_LANG_AUTO = 0,
    UI_LANG_JA,
    UI_LANG_EN,
    UI_LANG_COUNT
} UiLangMode;

typedef struct {
    uint8_t master_vol;         /* 0..100, 既定 80 */
    uint8_t bgm_vol;            /* 0..100, 既定 70 */
    uint8_t se_vol;             /* 0..100, 既定 85 */
    uint8_t shake_scale;        /* 0 / 50 / 100, 既定 100 */
    bool post_shader_on;        /* 既定 true */
    bool hints_on;              /* 既定 true */
    uint8_t ui_lang;            /* UiLangMode (0=AUTO, 1=JA, 2=EN), 既定 UI_LANG_AUTO (0) */
    bool reduce_flash;          /* 画面全体の明滅抑止, 既定 false */
    uint8_t cursor_scale;       /* 100 / 125 / 150, 既定 100 */
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
    uint32_t seen_hints_mask;   /* 表示済みの初回操作ヒントビットマスク */
    SettingsState settings;
} MetaState;
```

- **オーバーフロー＆二重精算防止契約**:
  - 読み込み時・精算時ともに `int64_t` 経由の飽和加算で `[0, META_MAX_*]` へクランプし、符号付き整数オーバーフロー（UB）を完全に防ぐ。
  - `meta.h` の `meta_settle_result(MetaState *meta, const MetaRunResult *res, bool *run_settled_flag, MetaSettleBreakdown *out_breakdown)` が純粋な精算を担い、`game.h` の `meta_settle_run(MetaState *meta, GameState *g, MetaSettleBreakdown *out_breakdown)` が `game_run_result(g)` と `&g->run.run_settled` を渡してラップする。1 ランにつき **厳密に 1 回だけ** 精算する（勝利画面でエンドレス継続を選んだ場合は精算せず、エンドレス終了時に 1 回精算）。
  - `meta_build_run_config(const MetaState *meta, BrushStyleId style, uint64_t seed)` で `RunConfig` を生成し、テスト・デモ時は固定プリセット `RunConfig` を直接渡してメタ状態と分離する。

### 8.2 セーブ形式とファイル I/O（`src/app/save.c`）
- **フォーマット（依存ゼロの `key=value` テキスト）**:
  ```ini
  version=1
  ink_crystals=120
  upg_shrine_hp=1
  upg_ink_max=2
  upg_rerolls=1
  upg_ink_regen=0
  unlocked_relic_mask=0x0000000000000003
  best_score=14850
  best_act=3
  runs_played=5
  clears=1
  seen_hints_mask=0x0000000f
  opt_master_vol=80
  opt_bgm_vol=70
  opt_se_vol=85
  opt_shake_scale=100
  opt_post_shader=1
  opt_hints=1
  opt_lang=0
  opt_reduce_flash=0
  opt_cursor_scale=100
  ```
- **破損耐性・後方互換**: 未知のキーは無視、欠損キー（例：P1 セーブで `opt_lang` / `opt_reduce_flash` / `opt_cursor_scale` を欠く `version=1` テキスト）は既定値（`0`, `false`, `100`）、範囲外数値は安全な範囲へクランプ（`opt_cursor_scale` は `100 / 125 / 150` へ正規化）。シリアライズ容量は `META_SERIALIZE_CAP = 640`。パース不能または `version` 非互換時はクラッシュさせず既定 `MetaState` へフォールバック。
- **保存先解決順**:
  1. CLI 引数 `--save-path <path>`（明示指定時最優先）
  2. `$XDG_DATA_HOME/inkstorm/save.txt`
  3. `$HOME/.local/share/inkstorm/save.txt`
  4. フォールバック：実行ファイル基準の `inkstorm_save.txt`
- **原子的書き込みと失敗時契約**:
  - 同一ディレクトリへ `.tmp` を書き出し、`fflush`・`fclose`・`rename` の戻り値をすべて検査する。いずれかが失敗した場合は `.tmp` を削除し、**既存の `save.txt` を破壊せず** に失敗をログ出力して続行する。
  - 起動時の読み込みが破損（`SAVE_LOAD_CORRUPT`）または入出力エラー（`SAVE_LOAD_IO_ERROR`）なら、既定値で上書きする前に `save_backup_unreadable()` で退避する。同じディレクトリの一時ファイル（`mkstemp`）へコピーして閉じた後、置き換えない `link()` で `<save>.bak` として公開する（途中で失敗・中断したコピーが `.bak` に見えることはない）。既存の `.bak` と同じ内容なら退避済みとし、内容が違えば `.bak.1`〜`.bak.9` の空き名へ退避する。置き換えが起こりうる `rename()` への切り替えは行わないため、ハードリンクを作れないファイルシステムでは退避は失敗扱いになる。退避できなかった場合は、そのセッションの保存を無効にして元のファイルを守る。一時ファイルの削除に失敗したときはログに出す。
- **テスト・デモ隔離**: `--no-save`・`--headless-smoke`・`--smoke-frames`・`--showcase`・`--demo`・`--snap` 指定時はセーブファイルの読み書きを行わない（`--save-path` とこれらのオプションの同時指定は CLI 排他エラーとする）。

---

## 9. オーディオ＆描画インターフェース境界

### 9.1 `ink_audio`（`src/audio/audio.h`）
- raylib 非依存の純 C11 合成エンジンとして PCM バッファ（和太鼓・琴・風音・炎・雷・筆擦れ・叩きつけ）を生成し、`app` 層が必要なサンプル数だけ `audio_render(audio, samples, sample_count)` で取り出して raylib の `AudioStream` / `Music` へ接続する。
- `audio.h` は `struct GameState;` を前方宣言のみとし、`game.h` の include を強制しない。
- `audio_consume_events(AudioState *audio, const struct GameState *game)` で各ティックのイベントと状態を読み取り、SE トリガおよび望ましい BGM トラック状態（`BGM_NONE`, `BGM_LOBBY`, `BGM_BATTLE`, `BGM_BOSS`, `BGM_VICTORY`）を更新する。
- 外部 BGM（`assets/audio/`）や任意差し替え SE が存在しない場合は、組み込みプロシージャル和風環境音・シンセへ自動フォールバックする。
- 筆音と渦印・風斬りは、状態変数フィルタ（TPT SVF）・ピンクノイズ・窓付きグレインで合成し、聞こえる正弦波オシレータを使わない。`app` が `assets/audio/sfx_brush_flow.ogg` / `sfx_brush_swish.ogg` をデコードして `audio_set_brush_samples` で 16bit モノラル PCM を貸し出す（所有は `app`。解放前に `NULL, 0` で外す）。貸し出された PCM は前後の無音を除いて使い、無い場合は同じ音色設計のプロシージャル合成へ切り替える。
- 筆音は毎ティックの `painting` と `stroke_speed` から、ゲート（時定数で立ち上がり約 10 ms・余韻約 30 ms、ポーズと場面切替は約 11 ms）と速度の慣性（約 19 ms）で滑らかに追従する。筆を置いたまま動きの無いティックは直前の速度を約 27 ms 保持して途切れさせず、筆を離したときは保持せずすぐ余韻へ移る。無音から書き始めた筆は速度の慣性を待たずにその速さで鳴らす。
- 効果音のストリームは 1 バッファ 1024 フレーム（約 23 ms）とし、フレームの揺らぎには 1 回のポンプで最大 4 バッファを補充して備える（バッファを大きくするとすべての効果音が遅れる）。
- 公開関数：`audio_init`, `audio_set_volumes`, `audio_set_brush_samples`, `audio_consume_events`, `audio_render`, `audio_bgm_track`, `audio_validate_finite`。

### 9.2 `ink_render`（`src/render/render.h`）
- 描画対象は `const GameState *`（および `ui_view.h` で定義する raylib 非依存の軽量 `const UiView *`）のみで、ゲーム状態を変更しない。`render_init(RenderState *r, const RenderOptions *opt)` でポストシェーダ有効/無効と障害注入マスクを受け取り、`render_draw(RenderState *r, const GameState *g, const UiView *ui)` で描画する。
- 内部に固定長 VFX プール（`sparks[512]`, `bolts[32]`, `seals[16]`, `callouts[24]`）、画面シェイク状態、専用 `vfx_rng`、rlgl 筆描画プリミティブ（`ink_stroke`, `ink_wash`, `ink_splatter`, `ink_outline`, `ink_knockout`, `seal_stamp`）、和紙ポストシェーダ（C 文字列埋め込み）、流体可視化テクスチャ、和風フォント（＋文字列表の英語 ASCII フォールバック）を保持する。
- **GPU 資源の検査**: RenderTexture は ID と添付に加えて `rlFramebufferComplete` で FBO の完全性を確かめてから使い（`render_texture_usable`。背景キャッシュ・ポスト用シーン・`--snap` の描画先）、不完全なら直接描画へ切り替える。コンパイルに失敗したシェーダが持つ `locs` は `render_discard_shader` で解放する。
- **背景キャッシュ**: `bg_cache` は四辺に 16px（`RENDER_BG_PAD`）の余白を持つ不透明な RenderTexture。和紙テクスチャ（継ぎ目のない 512×512）と山水背景（`SpriteBank.bg_tex` に読み込んだ 960×540 の外周 PNG 4 枚、欠落・不正時は `scenery.c` の手続き山水）・砂子を、色は通常合成・保存先アルファは 1 固定（`BLEND_CUSTOM_SEPARATE`）で焼き込み、描き戻しでアルファが二重に掛からないようにする。キャッシュキーは `(BgKind { BG_LOBBY, BG_ACT }, act_index)`（`render_rules.h`）で、ロビーと第 1 幕を区別する。FBO を作れない環境では毎フレーム直接描画する。画面揺れでも余白により紙の端が見えない。
- **流体の描画経路**: ポスト処理が有効で、流体シェーダと和紙テクスチャ（`texture1`）が揃ったとき（`render_fluid_use_gpu`）は、`(ink, fire, storm, solid)` を `FLUID_W × FLUID_H` の RGBA8 1 枚（濃度は `sqrt(v / max_dye)` で符号化）に詰めて 1 回 `UpdateTexture` し、埋め込み GLSL 330（4095 バイト以内）で 1280×720 に描く（B スプライン双三次補間、和紙の繊維に沿ったにじみと粒状感、連続した墨のトーン曲線と勾配による墨溜まり、火・雷・金の乗算済みアルファ合成、にじみで動かさないセル位置を `texelFetch` して `solid` セルを正確に除外。復元した濃度は有限な範囲へ制限する）。流体テクスチャは 2 枚ずつ交互に更新し、前フレームが描画中のテクスチャへの転送で CPU が GPU を待たないようにする。`--no-post`・設定でのポスト無効・`INKSTORM_DEBUG_FAULTS=shader` では CPU 経路（同じトーン曲線を表引きにした墨の乗算テクスチャ＋火・雷の乗算済みアルファテクスチャ）へ切り替える。`F1` に `fluid_shader: GPU/CPU` を表示する。色計算・符号化・判定は raylib 非依存の `render_rules.c` に置き、`test_render_rules` で検証する。
- **絵の登場物・地形・背景（スプライト）**: [art-direction.md](art-direction.md) §10.7 の絵を、社・敵・ボス・龍玉・戦場地形（岩柱・鳥居）の体および外周の山水背景に使う。
  - `RenderOptions.sprite_dir` は `app` が実行ファイル基準で解決した `assets/sprites` のパスで、`render_init` の間だけ借用する。`NULL` なら手続き描画だけで描く。
  - `RenderState.sprites`（`SpriteBank`）が 26 枚（512² × 7・256² × 19、14 群）の本体と半解像度の紙抜き縁 26 枚を 1 枚の 2048² アトラスに詰めて所有し、一筆龍の帯胴体（512²）と横周期の紙抜き縁（256²）を `TEXTURE_WRAP_REPEAT` の専用テクスチャ、水墨外周背景 4 枚（`bg_tex[SPR_BG_COUNT]`、960×540）を `TEXTURE_FILTER_BILINEAR` / `TEXTURE_WRAP_CLAMP` テクスチャとして起動時に 1 回だけ読み込み、`render_close` で解放する。読み込み用の画素バッファは `render_sprites_init` の中で確保して、戻る前に解放する（60Hz 更新中の動的確保なし）。PNG は IHDR の寸法を展開前に確かめ、寸法やアルファ条件（外周余白または背景中央の余白率）に反すれば弾き、拒否したファイルの相対名だけを警告ログへ 1 行出す。
  - 左右反転の履歴（±0.25）と紙の裏返しの開始ティックは `RenderState.sprite_face*` に `EntityId` の世代付きで持つ。描画が変えるのは描画側の状態だけで、ゲーム状態は変えない。
  - `sprite_rules.{h,c}`（raylib 非依存）にマニフェスト・状態の選択・表示の大きさ・四角形とアンカーの変換・反転の履歴・紙抜き縁のマスク・アトラスの配置・背景名と背景アルファ検査・龍の背骨と胴帯幅（弧長 64 点への置き直し、首と玉で止めるうねり）を置き、`test_sprite_rules` で検証する。`sprites.{h,c}` は読み込み・四角形・背景描画および一筆龍の胴帯メッシュ描画、`draw_entities_sprite.c` はキャラクターごとの合成（一筆龍の頭・帯胴体・四肢・房尾と撃破ほどきを含む）を受け持つ。`draw_entities.c`・`paper.c`・`scenery.c` の手続き描画はフォールバックとしてそのまま残す。
  - 紙抜きの色は `ink_knockout_color(act)` に一本化し、尾や胴の筆線の縁は `ink_stroke_knockout` で描く。
  - `INKSTORM_DEBUG_FAULTS=sprites`（`APP_FAULT_SPRITES` と `RENDER_FAULT_SPRITES` は同じビット）で、スプライトと背景 PNG が無いときと同じ手続き描画の経路を通す。
- **再現性と描画順契約（[art-direction.md](art-direction.md) 準拠）**:
  - 登場物のシルエット揺らぎ（ボイル）は `EntityId`（`index`, `gen`）由来の固定シードで生成し、`vfx_rng` は粒子エフェクトのみに用いる（`--snap` 比較の再現性を維持）。
  - HUD・UI は必ずポストシェーダの**後**に描画し、文字のにじみを防ぐ。
- **描画性能予算（Release・1280×720）**: `render_draw` の CPU 描画コマンド発行時間は **< 4.0 ms/frame** を目安とし、筆プリミティブのフレーム頂点数と描画時間を `F1` X 線オーバーレイに表示する。
- 公開関数：`render_init`, `render_reset_vfx`, `render_consume_events`, `render_update_vfx`, `render_draw`, `render_close`。
