# Phase 6-A タスク指示書：`--demo` / `--snap` CLI・`F1` X 線仕上げ・バランス＆性能最終調整

> **必読**: [PLAN.md](../PLAN.md) §9（特に §9.1 の Phase 6 行）、[architecture.md](../architecture.md) §1〜§4 / §8.2 / §9.2、[game-design.md](../game-design.md) §3 / §5.1 / §8〜§10 / §13、[art-direction.md](../art-direction.md) §5 / §8 / §14 / §15、[qa-and-testing.md](../qa-and-testing.md) §1〜§6、[phase5-integration.md](phase5-integration.md)、[AGENTS.md](../../AGENTS.md)、[inkstorm-fluid-sim](../../.agents/skills/inkstorm-fluid-sim/SKILL.md)、[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md)、[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)
>
> **前提**: Phase 5（`P5-Pre`〜`P5-Int`、PR #23・`main` `3c824b2`）が完了し、Phase 5 完了ゲート（全 28 札/15 遺物・狐符・多部位一筆龍・エンドレス・拡充設定・CI 3 ジョブ全緑）を通過していること。着手時に `main` が最新であることを確かめる。
> **作業場所 / ブランチ**: リポジトリルート（`full-scale-dev/Ink-storm`）または兄弟 worktree（`full-scale-dev/wt-demo-polish`）で最新の `main` から `feat/phase6-demo-polish` を作り、**1 本の PR** にする。
> **後続**: P6-B（[phase6-dist-release.md](phase6-dist-release.md)）— 本 PR が `@coderabbitai review` 完了・CI 3 ジョブ全緑で `main` にマージされた後に着手する（順次合流）。

---

## 0. 目的・Phase 6 の進め方（順次合流）・禁止事項

Phase 6（仕上げ・デモ・配布検証）は、自動デモ・視覚回帰ツール・X 線オーバーレイ・性能/バランス仕上げを固めたうえで、合流後の**同一コミット**に対して配布アーカイブと最終デモの合格判定を行うため、先行契約ブランチ（`*-Pre`）を挟まない **2 本順次合流** で進める。

### 0.1 Phase 6 の進め方（2 本順次合流）

| 段階 | 担当 | ブランチ | 主な範囲 | 合流条件 |
| :---: | :--- | :--- | :--- | :--- |
| 1 | **P6-A**（本書） | `feat/phase6-demo-polish` | `--demo` / `--snap <dir>` CLI 契約・実装・テスト、`F1` X 線仕上げ、Release 性能マージン検証・改善判断、難易度・数値バランス最終確認、設計書（`qa-and-testing.md` 等）同期 | ローカル最小テスト・Release ベンチ・WSLg `--snap` 確認・`@coderabbitai review`・CI 3 ジョブ全緑 → `main` マージ |
| 2 | **P6-B**（[phase6-dist-release.md](phase6-dist-release.md)） | `chore/phase6-dist-release` | P6-A マージ後の `main` から分岐し、同一コミットで `README.txt`・`assets/LICENSES.txt`・`dist/inkstorm-src.zip`（禁止ファイル検査＋独立展開＋素 `make` ＋ `--headless-smoke`）検証、デモ動画収録＆コンタクトシート目視確認、Phase 6 Gate と `docs/PLAN.md` 完了記録 | ローカル配布/視覚/デモ確認・`@coderabbitai review`・CI 3 ジョブ全緑 → `main` マージ |

### 0.2 禁止事項
- **メディア成果物のローカル保存限定**: `--snap <dir>` で採取した PNG・コンタクトシート・動画は `build/snaps/` 等（`.gitignore` 対象）へ**ローカル保存のみ**とする。**絶対に git へ追加・コミットしない**こと。**PR 本文やコメントへ画像を貼り付けない**こと（検証条件・時刻・シード・解像度・所見をテキストで記録する）。
- **公開リポジトリ表現規約の厳守**: コード・コメント・設計書・PR 本文には、特定の外部組織・個人・評価基準・既存商業作品名・不採用案の名称などの私的または外部固有名詞を一切書かない（「デモ動画」「配布要件」等の一般表現を用いる）。
- **P6-B の範囲を先回りしない**: `README.txt` の最終化、`assets/LICENSES.txt` 監査、`dist/inkstorm-src.zip` 最終合格判定、`docs/PLAN.md` §9.1 の Phase 6 `[完了]` 記録は P6-B で行う。
- **品質基準・数値契約を緩めない**: 流体格子（`160×90`）、圧力投影反復（`FluidConfig.project_iters = 12`）、壁横断防止レイマーチ（`trace_walls`）、性能予算（`fluid_step < 2.0 ms/tick`、`game_step < 4.0 ms/tick`、`render_draw < 4.0 ms/frame`、`prim_verts <= 60,000`、`blend_switches <= 12`）、`-ffast-math` 不使用、および既存 CTest の検証強度を緩めない。
- **モジュール境界・アート規約の厳守**:
  - `src/core`, `src/fluid`, `src/ecs`, `src/game`, `src/audio`, `src/app/cli.*`, `save.*`, `paths.*`, `frame_clock.*`, `input_latch.*`, `src/render/text_table.*`, `ui_view.h` に `raylib.h` を include しない。
  - `src/render`, `src/audio`, `src/app` から `g->internal` を読まない。
  - `game_debug_*` の呼び出しはテスト・ベンチ・`src/app/showcase.c`・`src/app/demo.c` のみに限定し、通常プレイ経路（`src/app/app_loop.c`）から呼ばない（[architecture.md](../architecture.md) §3.3）。
  - `DrawLine*` / `DrawCircle*` / `DrawRectangle*` / `DrawRing` / `DrawPoly*` / `DrawTriangleStrip` / `DrawSpline*` は `src/render/xray.c` と `src/render/ink_prims.c` 以外で使わない。`(Color){...}` の直書きは `src/render/palette.c` のみとする。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/app/cli.h`, `src/app/cli.c` | 更新 | `APP_MODE_DEMO`、`CliOptions.snap_dir` 追加、`--demo` / `--snap DIR` の解析・排他・重複・パス検証・セーブ無効化・`cli_print_usage` 更新（§2.1） |
| `src/app/main.c` | 更新 | `APP_MODE_DEMO` を `app_run_demo()` へ接続（§2.1） |
| `src/app/demo.h`, `src/app/demo.c` | 新規 | 固定シード自動プレイ `--demo` ループ、6 場面決定的 PNG 採取（`--snap DIR` / `--demo --snap DIR`）、ディレクトリ作成・PNG 書き出し検証（§2.2） |
| `src/app/showcase.c` | 更新 | `--showcase --snap DIR` による 6 枚自動採取モードを追加（対話 `1-6` / `Tab` / `F1` / `F12` は維持）（§2.2） |
| `src/render/xray.c` | 更新 | `F1` X 線オーバーレイ仕上げ（速度ベクトル場・渦度可視化・内部 `solid` 枠・属性セル数集計・計測パネル刷新）（§2.3） |
| `src/render/render.h`, `src/render/render.c`, `src/render/render_internal.h` | 必要時最小限 | `--snap DIR` 向けの 1280×720 オフスクリーン描画ヘルパ（例：`render_draw_to_texture`）および `render_draw_xray` の引数調整（§2.2 / §2.3） |
| `src/fluid/fluid.h`, `src/fluid/fluid.c`, `src/fluid/solver.c` | 条件付き | §2.4 の計測・原因切り分けの結果、`bench_fluid` p95 の安全マージン確保に最適化が必要と判断した場合のみ改修（公開 API・決定性・`FLT_MAX` 境界・壁横断防止を維持） |
| `src/game/autoplay.c` | 条件付き | `--demo` の見栄え・安定性向上に最小調整が必要な場合のみ（`test_determinism_run` の全条件を維持） |
| `src/game/enemies/enemy_defs.c`, `src/game/roguelike/waves.c`, `upgrades.c`, `relics.c`, `src/game/player/player_skills.c`, `src/game/elements/elements.c`, `src/game/meta.c` | 数値のみ（条件付き） | §2.5 の実プレイ検証で明確な不具合・違和感が見つかった場合のみ定義テーブル数値を調整し、`docs/game-design.md` へ同時反映 |
| `CMakeLists.txt` | 更新 | `--demo` / `--snap` の CLI Usage テスト（`tools/expect_usage.sh`）を CTest に追加（§3）。なお `src/app/demo.c` は `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)` および `tools/dist.mk` の `find src -type f -name '*.c'` で自動検出される |
| `tests/test_app_core.c` | 更新 | `--demo` / `--snap DIR` の正常系・異常系・排他・重複・パス長・セーブ隔離の単体テスト追加（§3） |
| `tests/test_fluid.c` | 条件付き | `src/fluid/` を最適化した場合のキャッシュ無効化・数値回帰テスト追加（§2.4 / §3） |
| `docs/qa-and-testing.md`, `docs/architecture.md`, `docs/game-design.md` | 更新 | `--demo` / `--snap` 確定契約・CTest 一覧・`game_debug_*` 境界・§13 数値最終確認の同期（§2.6）。`docs/art-direction.md` は数値調整があった場合のみ |

---

## 2. 実装・検証仕様

### 2.1 `--demo` / `--snap DIR` CLI 契約（`src/app/cli.h`, `cli.c`, `main.c`）

`src/app/cli.h` は raylib 非依存を維持し、次を確定する。

```c
typedef enum {
    APP_MODE_WINDOW = 0,
    APP_MODE_HEADLESS,
    APP_MODE_SHOWCASE,
    APP_MODE_DEMO
} AppMode;

typedef struct {
    AppMode mode;
    int smoke_frames;
    bool use_post;
    SaveMode save_mode;
    const char *save_path;
    const char *snap_dir; /* --snap DIR 指定時のみ非 NULL */
} CliOptions;
```

1. **引数検証**:
   - 既存の `is_valid_save_path()` を汎用の `is_valid_cli_path()` として `--save-path` と `--snap` の双方で使う（非 `NULL`、非空、先頭 `"--"` 禁止、`strlen(p) < (size_t)PATH_MAX`）。
   - `--snap DIR` についてはさらに `"<DIR>/snap_06.png"`（末尾 13 バイト + NUL）が `PATH_MAX` 未満に収まること（`strlen(p) + 16 < (size_t)PATH_MAX`）を検証し、超過時はパース失敗とする。
   - **重複禁止**: `--demo --demo` および `--snap a --snap b` を含むすべての同一オプション重複を拒否する。
2. **モード決定と排他マトリクス**:
   - 単独の `--snap DIR`（`--showcase` も `--demo` も未指定）は **`APP_MODE_DEMO`** として扱い、決定的デモの 6 枚採取を実行して終了する。
   - `--showcase --snap DIR` は **`APP_MODE_SHOWCASE`**＋`snap_dir = DIR` とし、見本帳の 6 枚採取を実行して終了する。

| オプション | 排他（同時指定は Usage エラー・終了コード 1） | 併用可能 | セーブ I/O |
| :--- | :--- | :--- | :---: |
| `--headless-smoke` | `--smoke-frames`, `--showcase`, `--demo`, `--snap`, `--save-path` | `--no-post`, `--no-save` | 無効 |
| `--smoke-frames N` | `--headless-smoke`, `--save-path`, **`--snap`** | `--showcase`, `--demo`, `--no-post`, `--no-save` | 無効 |
| `--showcase` | `--headless-smoke`, `--save-path`, **`--demo`** | `--smoke-frames`, `--snap`, `--no-post`, `--no-save` | 無効 |
| `--demo` | `--headless-smoke`, `--showcase`, `--save-path` | `--smoke-frames`, `--snap`, `--no-post`, `--no-save` | **無効** |
| `--snap DIR` | `--headless-smoke`, `--save-path`, `--smoke-frames` | `--demo`, `--showcase`, `--no-post`, `--no-save` | **無効** |
| `--save-path PATH` | `--no-save`, `--headless-smoke`, `--smoke-frames`, `--showcase`, **`--demo`**, **`--snap`** | `--no-post` | 指定先 |
| `--no-post` | （重複のみ） | 全モード | モード依存 |
| `--no-save` | `--save-path` | 全モード | 無効 |

3. **セーブ隔離・Usage 出力・ディスパッチ**:
   - `seen_headless || seen_smoke_frames || seen_showcase || seen_demo || seen_snap || seen_no_save` のいずれかが真なら `out->save_mode = SAVE_MODE_DISABLED; out->save_path = NULL;` とし、セーブファイルの読み書きを一切行わない。
   - `cli_print_usage()` の出力は `tools/expect_usage.sh` 契約（`^Usage: ` 始まり・終了コード 1）を守り、次へ更新する：
     ```c
     fprintf(stream,
             "Usage: %s [--headless-smoke | --smoke-frames N | --snap DIR] "
             "[--showcase | --demo] [--no-post] [--no-save | --save-path PATH]\n",
             prog);
     ```
   - `src/app/main.c` で `opts.mode == APP_MODE_DEMO` のとき `app_run_demo(game, audio, render, &opts, fault_mask)` を呼び出す（失敗時は `EXIT_FAILURE`、正常時は `EXIT_SUCCESS`）。

---

### 2.2 `--demo` 実行ループと `--snap DIR` 自動撮影（`src/app/demo.h`, `demo.c`, `showcase.c`, `render.*`）

1. **共通の決定的初期化**:
   - 固定プリセット `const RunConfig cfg = meta_run_config_preset(BRUSH_STYLE_HEAVY, k_demo_seed);`（`k_demo_seed` は `src/app/demo.c` の定数として固定し、起動時に `TraceLog(LOG_INFO, "INKSTORM: demo mode seed=0x%016" PRIx64 " post=%s ...", ...)` を出力する）。
   - `MetaState` は既定値（`meta_init_default`）とし、デモ視認性を損なわないよう `hints_on = false`, `shake_scale = 100`, `reduce_flash = false`, `cursor_scale = 100` を用いる。セーブ I/O は一切行わない。
   - `render_init` 後に `render_reset_vfx(render, cfg.seed)`、`audio_init(audio, UINT64_C(1))`、`audio_set_lobby_mode(audio, false)` を呼ぶ。
   - 複数ティック消費契約（[architecture.md](../architecture.md) §1.2）：毎 `game_step()` 直後に必ず `audio_consume_events()` → `render_consume_events()` → `render_update_vfx(render, game, INK_FIXED_DT)` を実行する。

2. **ライブデモモード（`./inkstorm --demo`、`opts->snap_dir == NULL`）**:
   - `opts->smoke_frames > 0` のときのみ `SetConfigFlags(FLAG_WINDOW_HIDDEN)` とし、`audio_out_init` を省略する。通常 `--demo` は 1280×720 の可視ウィンドウと音声出力（`audio_out_init` / `audio_out_pump` / `audio_out_close`）を有効にする。
   - **開始 5 秒以内の見どころ提示**（[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md)）：`game_init` 直後に `game_debug_start_wave(game, 0)` で第 1 幕第 1 ウェーブの戦闘へ直行するか、幕転換 2.0 秒後に即座に濃い筆致・偏向・着火が見られる固定シードを選定し、起動から 5 秒以内に墨流体と戦闘の見どころが画面に現れるようにする。
   - 60Hz 固定刻み（可視時は `FrameClock`、`--smoke-frames > 0` 時は 1 フレーム 1 ティック）で `game_autoplay_input(game)` を供給する。
   - `RUN_PHASE_REWARD` では、巻物展開アニメーション完了後に **約 1.5 秒（90 ticks）** 保持して 3 択札・大遺物を目視可能にしてから `game_autoplay_command(game)` を発行する（`--smoke-frames > 0` のときは待たずに発行してよい）。
   - **ライブ操作**：`F1` で X 線オーバーレイ（`ui.xray`）を随時トグル可能とし、`Esc` またはウィンドウ閉じるボタンで即座に `EXIT_SUCCESS` 終了する。勝利・敗北リザルト到達時は約 3.0 秒保持して正常終了する（`--smoke-frames N` 指定時は `N` フレーム描画後に終了）。

3. **自動スナップショット採取モード（`--snap DIR` / `--demo --snap DIR`）**:
   - **ディレクトリ準備と失敗時契約**:
     - `DIR` が既存ファイル（非ディレクトリ）ならエラーログを出して `EXIT_FAILURE` で終了する。
     - `DIR` が存在しない場合は親ディレクトリを含めて `mkdir`（`0755`、`save.c` の `mkdir_parents` 相当）で作成する。作成失敗またはいずれかの PNG 書き出し失敗時は警告/エラーを `stderr` へ出力し **`EXIT_FAILURE`** を返す。
   - **出力ファイル名（最大 6 枚・時系列固定）**:
     - `<DIR>/snap_01.png` 〜 `<DIR>/snap_06.png`（[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md) の `ffmpeg -pattern_type glob -i '.../snap_*.png' -vf 'scale=640:-1,tile=3x2:padding=6:margin=6'` と完全一致）。
   - **完全決定的ティック進行（`--no-post` 同一ティック比較保証）**:
     - 壁時計 `GetFrameTime()` を一切使わず、常に固定刻み `INK_FIXED_DT` で決定的ティック数だけ `game_step` を進める。これにより `./inkstorm --demo --snap dir_a` と `./inkstorm --demo --no-post --snap dir_b` が **全 6 枚でビット完全一致のシミュレーション状態（同一ティック）** を撮影する（[qa-and-testing.md](../qa-and-testing.md) §5.2）。
   - **撮影する 6 場面（全場面、実 `game_step` + `game_autoplay_input` で流体・敵弾・反応が稼働した状態を撮影）**:
     1. `snap_01.png`: **第 1 幕通常戦**（風筆・墨渦・弾偏向・紅蓮墨嵐／雷墨連鎖が活発な戦闘フレーム）
     2. `snap_02.png`: **報酬選択画面**（`RUN_PHASE_REWARD`、`reward_anim_t >= 1.0f` で強化札 3 択・リロール・社修繕が明瞭に読める状態）
     3. `snap_03.png`: **第 2 幕『鳥居と岩柱』**（`solid` 鳥居・岩柱周りの流体回り込み＋鵺または属性反応）
     4. `snap_04.png`: **第 3 幕『嵐の霊峰』通常戦**（霊柱・山颪・狐符/追尾符・属性反応）
     5. `snap_05.png`: **第 3 幕ボス『一筆龍』多部位戦**（3 龍玉・一筆胴体軌跡・上中央ボス HP バー＋3 龍玉印）
     6. `snap_06.png`: **高負荷戦闘＋`F1` X 線オーバーレイ ON**（直前 1 フレームを計測描画してから撮影し、速度ベクトル・渦度・属性セル数・非ゼロの `render_draw` / `prim_verts` / `blend_sw` / `fluid_solve` が読める状態）
     - ※ 各場面間の切り替えには [architecture.md](../architecture.md) §3.3 で `--demo` に許可された `game_debug_start_act` / `game_debug_start_wave` / `game_debug_force_wave_clear` を用いて、数秒以内に全 6 枚を安定採取できるようにする。

4. **見本帳自動撮影（`--showcase --snap DIR`、`src/app/showcase.c`）**:
   - `opts->snap_dir != NULL` のとき、`DIR` を作成し、見本帳の代表 6 場面を `<DIR>/snap_01.png`〜`<DIR>/snap_06.png` へ保存して `EXIT_SUCCESS` で終了する：
     - `snap_01.png`: Page 1 Sub 0（P1 全敵・ボス・弾・地形・6 筆プリミティブ・パレット見本）
     - `snap_02.png`: Page 1 Sub 1（P2 狐符・護符結界・追尾符・多部位一筆龍・エンドレス金砂子）
     - `snap_03.png`: Page 2（墨 6 割充填キャンバス＋4 属性反応・紙抜き縁確認）
     - `snap_04.png`: Page 3 Sub 0（強化札 3 択報酬 UI）
     - `snap_05.png`: Page 5 Sub 2（拡充設定画面 9 項目＋2 初期化）または Page 4 Sub 0（タイトル/ロビー）
     - `snap_06.png`: Page 6 Sub 0（勝利リザルト 3 ボタン・未精算内訳プレビュー）
   - `opts->snap_dir == NULL` の対話モード（`1-6`、`Left`/`Right`、`Tab`、`F1`、`F12` による `build/snaps/showcase_p*.png` 保存）および `--showcase --smoke-frames N` の挙動は維持する。

5. **WSLg オフスクリーン読み出しの安全性（黒画面防止）**:
   - X11 / WSLg D3D12 環境では `FLAG_WINDOW_HIDDEN` ウィンドウに対して `LoadImageFromScreen()` を呼ぶと黒画像になる場合がある。
   - **推奨実装**: `src/render/render.c` の描画本体（`draw_world_layers` → ポストシェーダ適用 → `draw_ui_layers` → `r->metrics` 更新）を共通化し、1280×720 の専用オフスクリーン `RenderTexture2D`（`target.id != r->scene.id`、呼び出し側で `LoadRenderTexture(INK_SCREEN_W, INK_SCREEN_H)`／終了時 `UnloadRenderTexture`）へ描画する関数（例：`void render_draw_to_texture(RenderState *r, const GameState *g, const UiView *ui, RenderTexture2D target);`）を `src/render/render.h` に追加する。`--snap DIR` では `LoadImageFromTexture(target.texture)` → `ImageFlipVertical(&img)` → `ExportImage(img, path)` → `UnloadImage(img)` で書き出すことで、隠しウィンドウでも WSLg 実機で確実に 1280×720・非黒・ピクセル正確な PNG を得られる（通常経路・`--no-post`・`INKSTORM_DEBUG_FAULTS=shader` / `font` / `shader,font` すべて同一経路を通ることを検証する）。

---

### 2.3 `F1` X 線オーバーレイ仕上げ（`src/render/xray.c`）

現行 `src/render/xray.c` は暫定ラベル `"F1 X-RAY METRICS (P1)"` の数値パネルのみで、[game-design.md](../game-design.md) §5.1・[qa-and-testing.md](../qa-and-testing.md) §5.2・[art-direction.md](../art-direction.md) §8 / §15 が定める **流体速度ベクトル・渦度・属性セル数** が未描画である。本タスクで以下を仕上げる。

1. **流体速度ベクトル場（Velocity Vector Overlay）**:
   - `FLUID_W × FLUID_H`（`160×90`）を 4 セル間隔（32px ごと、約 `39×22` 標本点）で走査する。
   - `g->fluid.solid[idx] == 0` かつ速度大きさ `speed = hypotf(u, v) > 0.5f`（セル/s）の点について、セル中心 `((x + 0.5f) * FLUID_CELL_PX, (y + 0.5f) * FLUID_CELL_PX)` から方向ベクトルを長さクランプ（例：`4.0f..26.0f` px）で `DrawLineV`（または `DrawLineEx`）＋先端小点 `DrawCircleV` で描画する。色は `PAL_GOFUN` / `PAL_KIN_HOT` の半透明（`pal_color_alpha`）で速度強度を表現する。
2. **渦度可視化＆内部 `solid` 境界表示**:
   - 各標本点（`1 <= x < FLUID_W - 1`, `1 <= y < FLUID_H - 1`）で隣接 `u`/`v` の中心差分から渦度 `vort = 0.5f * ((v[i+1] - v[i-1]) - (u[i+FLUID_W] - u[i-FLUID_W]))` を算出する（`g->fluid.curl` 内部配列に依存せず、現在の速度場と一致させる）。
   - `fabsf(vort) >= g->fluid.cfg.vorticity_thresh`（既定 `0.35f`）の標本点に、回転方向別の渦度マーカー（正＝`PAL_SHU`、負＝`PAL_AO` の半透明 `DrawCircleLines` / 小円、半径・不透明度を `|vort|` に応じてクランプ）を描画する。
   - 外周壁を除く内部障害物セル（`1 <= x < FLUID_W - 1`, `1 <= y < FLUID_H - 1` かつ `g->fluid.solid[idx] != 0`）に薄い枠線（`DrawRectangleLines`, `PAL_KIN` 半透明）を重ね、鳥居・岩柱・霊柱の `solid` 格子境界を直感的に視認できるようにする。
3. **属性セル数集計（`ui->xray` ON 時のみ計算）**:
   - `fluid_channel_const(&g->fluid, 0/1/2)` を走査し、以下を集計する（OFF 時は走査ゼロ）：
     - `ink_cells`: `ink[i] > 0.20f`
     - `fire_cells`: `fire[i] > 0.14f`
     - `storm_cells`: `storm[i] > 0.14f`
     - `react_cells`: 反応条件該当セル（紅蓮 `ink > 0.20f && fire > 0.14f`、雷墨 `ink > 0.24f && storm > 0.14f`、爆雷 `fire > 0.30f && storm > 0.30f`、三元 `ink > 0.30f && fire > 0.30f && storm > 0.30f` のいずれか）
4. **計測パネルの刷新**:
   - 左上パネルの見出しを `"F1 X-RAY"` に改め（`(P1)` 表記を撤去）、上端 HUD 帯（`y = 0..56`）と重ならない位置に ASCII 表記（`DrawText`、フォント欠損 `INKSTORM_DEBUG_FAULTS=font` 時もそのまま判読可能）で次を明瞭表示する：
     - `render_draw : %.2f ms  (budget < 4.0)`
     - `prim_verts  : %" PRIu32 " (<= 60000)   blend_sw: %" PRIu32 " (<= 12)`
     - `fluid_solve : %.2f ms  (< 2.0)   max_v: %.1f  max_vort: %.1f`
     - `cells       : ink=%d  fire=%d  storm=%d  react=%d`
     - `entities    : %" PRIu32 "  dropped_ev: %" PRIu32 "  act:%d-%d`（`act_index + 1`, `wave_index + 1`）
     - `post_shader : %s   font: %s`
5. **規約・性能確認**:
   - `xray.c` は `pal_color*` / `pal_lerp` のみ使い `(Color){...}` を一切直書きしない。`g->internal` を読まない。
   - 戦闘画面（特に多部位一筆龍戦）で `F1` ON にしても `render_draw < 4.0 ms/frame`、`prim_verts <= 60,000`、`blend_switches <= 12` に収まることを確認する。

---

### 2.4 Release 性能マージン検証と流体・ゲーム性能最終調整

Phase 5-Int（PR #23、Intel Core Ultra 9 185H / WSLg Linux / GCC 13.3.0）の Release ベンチ実測値は次のとおりであり、**`bench_fluid` の p95 が `1.9998 ms`（予算上限 `2.0 ms/tick` まで残り `0.0002 ms`）**、さらに `bench_game` の Act 6 エンドレスでは **`fluid_p95 = 2.055 ms`** と上限付近にあった：

| ベンチ項目（Phase 5-Int 実測） | mean | p95 | 予算 | 状況 |
| :--- | :---: | :---: | :---: | :--- |
| `bench_fluid`（160×90, `project_iters=12`, 障害物 8） | `1.163 ms` | **`1.9998 ms`** | `< 2.0 ms` | **上限ギリギリ（要検証・改善判断）** |
| `bench_game` Act 3 多部位一筆龍 | `1.311 ms`（fluid `1.176`） | `2.186 ms`（fluid `1.911`） | `< 4.0 ms` | PASS（流体 p95 余白小） |
| `bench_game` Act 6 エンドレス多部位一筆龍＋狐符 | `1.302 ms`（fluid `1.161`） | `2.334 ms`（fluid `2.055`） | `< 4.0 ms` | PASS（流体 p95 2.0ms 超過） |
| `bench_game` Act 2 ストレス（+16 敵 / +48 弾） | `1.200 ms` | `2.251 ms` | `< 4.0 ms` | PASS |
| 多部位一筆龍戦 `F1` `render_draw` | `1.065 ms` | `1.647 ms` | `< 4.0 ms` | PASS（頂点 `18,309`・切替 `2`） |

本タスクでは次の手順で**計測 → 原因切り分け → 安全な最適化の検討・実施判断 → 回帰検証**を行う。

1. **ベースライン再計測とステージ別内訳の切り分け**:
   - Release ビルド（`-O2 -DNDEBUG`）で `./build/release/bench_fluid`（`noise floor` p50/p95 を含む）と `./build/release/bench_game` を計測する。
   - `src/fluid/solver.c` の各ステージ（`prepare_geometry`, `confine`, `project` ×2, `advect_velocity`, `advect_scalars`）の寄与を確認し、p95 押し上げ要因が「計算ホットスポット」か「WSLg / ハイブリッド CPU の周波数・コア遷移揺らぎ」かを切り分ける（計測用の一時コードはコミットに残さない）。
2. **安全な最適化候補（優先順）**:
   - **候補 A：`prepare_geometry` の `solid` 変更時キャッシュ化（`src/fluid/fluid.h`, `fluid.c`, `solver.c`）**:
     - 現行 `fluid_step` は毎ティック 72 KB（`inv_neighbors[FLUID_N]`＋`near_wall[FLUID_N]`）をスタック確保して全格子を再走査しているが、`solid` 配列は幕転換時（`fluid_init` / `fluid_clear_solids` / `fluid_set_solid_rect` / `fluid_set_solid_circle`）以外変化しない。
     - `FluidState` に固定長値フィールドとして内部キャッシュ配列と `geometry_dirty` フラグを持たせ、変更時のみ再構築する（ヒープポインタを持たないため `tests/bench_fluid.c` の `memcpy(snapshot, f, sizeof *f)` とも完全互換、メモリ増分約 72 KB は総予算 1.3〜1.5 MB の範囲内、`fluid_hash` / `fluid_reset` / `fluid_validate_finite` 契約は不変）。`solid` 動的切替後にキャッシュが確実に再構築される回帰テストを `tests/test_fluid.c` へ追加する。
   - **候補 B：通常値域での不要な `double` 変換・分岐削減（`src/fluid/solver.c`）**:
     - 現行の `interpolate()`・`confine()`・`project()` は `FLT_MAX` 付近のオーバーフロー防止のために毎セル `double` 変換と `limit_magnitude(..., FLT_MAX)` を通している。
     - 通常のゲーム設定（例：`f->cfg.max_vel <= 1e30f` かつ `f->cfg.max_dye <= 1e30f`、既定は `125.0f` / `2.5f`）では `float` 積和でオーバーフローし得ないため、`fluid_sample()` と極端な設定値（`> 1e30f`）のみ既存 `double` 経路を残し、通常シミュレーション時の `advect_velocity` / `advect_scalars` / `confine` / `project` を軽量 `float` 経路へ分離する（`test_fluid.c` の `FLT_MAX` 境界テストを含む全テストが 100% 通ること）。
   - **候補 C：ループ不変条件の巻き上げ・`restrict`・内側非 `near_wall` 高速パス**:
     - `vel_damp`・`max_vel`・`dye_damp[ch]` をローカル変数へ巻き上げ、`advect_scalars` の 3 チャンネルを同一 `Stencil` で連続処理する際の `restrict` 修飾等で GCC `-O2` の自動ベクトル化・コード生成を改善する。
3. **改善判断基準と記録**:
   - **必須基準**: `bench_fluid` の `median mean < 2.0 ms/tick` かつ **`median p95 < 2.0 ms/tick`**、`bench_game` 全 3 ケースの `game_mean < 4.0`・`game_p95 < 4.0 ms/tick`、`test_fluid`・`test_determinism_run` 全緑。
   - **目標マージン**: `bench_fluid` の **`median p95 <= 1.70 ms/tick`**、かつ `bench_game` の `fluid_p95 < 2.0 ms/tick` を目安とする。
   - 最適化を実施した場合は、変更前後の実測表（mean / p50 / p95 / max / `noise floor` / 決定性ハッシュ一致）と、最適化不要と判断した場合でも計測根拠を PR 本文へ明記する。

---

### 2.5 難易度・数値バランス最終確認

Phase 5-Int（PR #23）のメタ強化ゼロ自動完走実績は、3 筆すべて本編 3 幕 `2.74〜3.04 min`（実プレイ換算 6〜8 分）、多部位一筆龍 `22.6〜27.1 s`（実プレイ換算 55〜80 秒帯）、獲得墨晶 `196〜197`（目標 `180〜230` 帯）、エンドレス第 6 幕到達と設計目標によく収まっている。本タスクでは実ウィンドウの手動プレイで以下を最終確認する：

1. **筆 3 種の手触り**：豪筆（偏向・叩きつけ）、朱筆（間欠火種と墨の残存バランス）、細筆（帯電渦印と墨玉回収テンポ）。
2. **幕・ボス・エンドレスのテンポ**：第 1〜3 幕の通常ウェーブ・大天狗・鵺・多部位一筆龍（3 龍玉→点睛→昇龍）の予兆の読みやすさ・緊張感、第 4 幕エンドレス突入直後の難易度段差。
3. **経済バランス**：墨ゲージ枯渇〜硯回復〜墨玉回収の循環、1 ラン獲得墨晶と墨庵価格帯（全購入 1,920 墨晶）。
4. **調整ルール**：
   - 明白な破綻・ストレスがなければ **現行数値で確定** とし、確認結果を PR 本文と `docs/game-design.md` §13 へ記録する。
   - 調整が必要な場合は定義テーブル（`enemy_defs.c`, `waves.c`, `upgrades.c`, `relics.c`, `player_skills.c`, `elements.c`, `meta.c`）の数値変更のみに留め、変更した全数値を `docs/game-design.md`（および必要時 `docs/art-direction.md`）へ同期したうえで `test_acts_enemies`, `test_roguelike_meta`, `test_elements`, `test_determinism_run`, `bench_game` をすべて再確認する。

---

### 2.6 設計書同期（P6-A 範囲）

- `docs/qa-and-testing.md`:
  - §2：新規追加した `test_cli_usage_*` CTest ターゲットと `test_app_core` の `--demo` / `--snap` 検証範囲を反映する。
  - §4：`--demo` と `--snap <dir>` の注記「Phase 6 まで未知オプション」を削除し、確定した排他マトリクス（`--snap` と `--smoke-frames` の排他、`--demo` と `--showcase` の排他等）と単独 `--snap <dir>` の挙動を反映する。
- `docs/architecture.md`:
  - §1.1 / §3.3 / §8.2：`--demo` / `--snap` のセーブ隔離と `game_debug_*` の許容範囲（`showcase.c`・`demo.c`）が実コードと 100% 一致していることを確認・同期する。
- `docs/game-design.md`:
  - §13：「本書の数値を実プレイで調整する」項目に `[Phase 6 確定]` として最終確認（または調整内容）を記録する。
- ※ `docs/PLAN.md` §9.1 の Phase 6 `[完了]` 記録は **P6-B** で行うため、本 PR では変更しない。

---

## 3. テスト要件

1. **`tests/test_app_core.c`**:
   - 既存 `test_cli_usage_errors` の `--demo` / `--snap d` を削除し、`test_cli_valid_options` へ以下を追加する：
     - `--demo`（`mode == APP_MODE_DEMO`, `save_mode == SAVE_MODE_DISABLED`, `snap_dir == NULL`, `use_post == true`）
     - `--demo --no-post --no-save`
     - `--demo --smoke-frames 10`（`mode == APP_MODE_DEMO`, `smoke_frames == 10`）
     - `--demo --snap build/snaps/out`（`mode == APP_MODE_DEMO`, `snap_dir == "build/snaps/out"`, `save_mode == SAVE_MODE_DISABLED`）
     - `--snap build/snaps/out`（単独指定で `mode == APP_MODE_DEMO`, `snap_dir == "build/snaps/out"`, `save_mode == SAVE_MODE_DISABLED`）
     - `--snap build/snaps/out --no-post`
     - `--showcase --snap build/snaps/sc`（`mode == APP_MODE_SHOWCASE`, `snap_dir == "build/snaps/sc"`, `save_mode == SAVE_MODE_DISABLED`）
   - `test_cli_usage_errors` へ以下を追加する：
     - 引数不足・不正パス：`--snap`、`--snap ""`、`--snap --no-post`、`--snap <PATH_MAX 相当長パス>`
     - 重複：`--demo --demo`、`--snap a --snap b`
     - 排他：`--demo --headless-smoke`、`--demo --showcase`、`--demo --save-path x`、`--snap a --headless-smoke`、`--snap a --save-path x`、`--snap a --smoke-frames 1`
2. **`CMakeLists.txt`（`tools/expect_usage.sh` 配線 CTest 追加：全 15 件 → 18 件）**:
   - 既存 3 件（`test_cli_usage_unknown`, `test_cli_usage_save_exclusive`, `test_cli_usage_headless_exclusive`）に加え、以下 3 件を追加する：
     - `test_cli_usage_demo_exclusive`（`--demo --showcase`）
     - `test_cli_usage_snap_exclusive`（`--snap out --smoke-frames 1`）
     - `test_cli_usage_snap_missing_arg`（`--snap`）
3. **`tests/test_fluid.c`**（`src/fluid/` を最適化した場合）:
   - `fluid_set_solid_rect` / `fluid_set_solid_circle` / `fluid_clear_solids` で障害物を動的に追加・撤去した直後の `fluid_step` がキャッシュ無効化により正しく壁反射・非壁通過を切り替えること、および `FLT_MAX` 境界・壁横断防止・決定性リプレイが全件通ることを検証する。

---

## 4. ローカル検証・視覚確認・機械確認・完了条件（P6-A DoD）

### 4.1 最小ローカル検証・ベンチ・WSLg スナップ確認コマンド

```sh
# 1. ASan/UBSan: 変更範囲のビルドと関連テスト
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan --target inkstorm test_app_core test_fluid \
        test_determinism_run test_acts_enemies test_roguelike_meta test_elements test_core_smoke &&
    nix develop -c ctest --preset asan --output-on-failure \
        -R '^(test_app_core|test_cli_.*|test_fluid|test_determinism_run|test_acts_enemies|test_roguelike_meta|test_elements|test_core_smoke)$'

# 2. Release ビルド・性能ベンチ・CLI スモーク
nix develop -c cmake --preset release &&
    nix develop -c cmake --build --preset release &&
    ./build/release/bench_fluid &&
    ./build/release/bench_game &&
    ./build/release/inkstorm --headless-smoke &&
    nix develop -c ./build/release/inkstorm --demo --smoke-frames 120 &&
    nix develop -c ./build/release/inkstorm --demo --no-post --smoke-frames 120

# 3. WSLg 実機 --snap 自動採取＆コンタクトシート生成（build/snaps/ へローカル保存のみ・未コミット）
nix develop -c ./build/release/inkstorm --demo --snap build/snaps/p6a_post
nix develop -c ./build/release/inkstorm --demo --no-post --snap build/snaps/p6a_nopost
nix develop -c ./build/release/inkstorm --showcase --snap build/snaps/p6a_showcase
INKSTORM_DEBUG_FAULTS=shader nix develop -c ./build/release/inkstorm --demo --snap build/snaps/p6a_shader_fault
INKSTORM_DEBUG_FAULTS=font nix develop -c ./build/release/inkstorm --demo --snap build/snaps/p6a_font_fault
INKSTORM_DEBUG_FAULTS=shader,font nix develop -c ./build/release/inkstorm --showcase --snap build/snaps/p6a_both_fault

# コンタクトシート生成（ローカル目視用）
for d in p6a_post p6a_nopost p6a_showcase; do
    ffmpeg -y -loglevel error -pattern_type glob -i "build/snaps/$d/snap_*.png" \
        -vf 'scale=640:-1,tile=3x2:padding=6:margin=6' -frames:v 1 "build/snaps/$d/contact_sheet.png"
done

# 異常系終了コード確認（既存ファイルへの --snap 指定は非 0 終了）
touch build/snaps/not_a_dir && ! nix develop -c ./build/release/inkstorm --snap build/snaps/not_a_dir
rm -f build/snaps/not_a_dir
```

### 4.2 境界・アート規約・未追跡メディアの機械確認

```sh
grep -rn 'raylib.h' src/core src/fluid src/ecs src/game src/audio src/app/save.* src/app/paths.* \
    src/app/cli.* src/app/frame_clock.* src/app/input_latch.* src/render/text_table.* src/render/ui_view.h  # 0 件
grep -rnE '(->|\.)internal\b' src/render src/audio src/app                                                    # 0 件
grep -rn 'game_debug_' src/app | grep -vE '^src/app/(showcase|demo)\.c:'                                      # 0 件
grep -rnE 'fluid(\.|->)(u0|v0|div|p|curl|dye0)\b' src/game                                                    # 0 件
grep -rn 'rand(\|time(' src/game                                                                              # 0 件
grep -rnE '\b(malloc|calloc|realloc|free)\(' src/game                                                         # game_create / game_destroy のみ
grep -rnE 'Draw(Line|Circle|Rectangle|Ring|Poly|TriangleStrip|Spline)' src/render                             # xray.c・ink_prims.c のみ
grep -rnE '\(Color\)\{' src                                                                                   # palette.c のみ
grep -rnE '\bGameState\s+[A-Za-z_][A-Za-z0-9_]*\s*(;|=|\[)' src tests \
    | grep -v 'typedef struct GameState'                                                                      # 0 件
find src -name '*.h' -printf '%f\n' | sort | uniq -d                                                          # 空
grep -rnE '^\s*static\s+[A-Za-z_][A-Za-z0-9_ *]*\s+\**[A-Za-z_][A-Za-z0-9_]*(\[[^]]*\])*\s*(=|;)' src \
    | grep -v 'static const'                                                                                  # 0 件
git status --short                                                                                            # build/・dist/・*.png・*.mp4 が 0 件
```

### 4.3 完了条件（Definition of Done）
- [ ] `--demo` および `--snap DIR` の CLI 解析・排他・重複・パス検証・セーブ隔離・Usage 出力が実装され、`test_app_core` および新規 `test_cli_usage_*` CTest が ASan/UBSan（`-Werror`）で全緑。
- [ ] `./build/release/inkstorm --demo`（ライブ再生・5 秒以内の見どころ・`F1` X 線切替・`Esc` 終了）と、`--demo --snap DIR` / `--snap DIR` / `--showcase --snap DIR`（6 枚 1280×720 非黒 PNG・`--no-post` 同一ティック比較・`shader`/`font`/`shader,font` 障害注入・異常パス非 0 終了）を WSLg 実機で確認した。
- [ ] `F1` X 線オーバーレイ（速度ベクトル場・渦度可視化・内部 `solid` 枠・属性セル数・刷新計測パネル）を実装し、`F1` ON 時の多部位一筆龍戦でも描画予算（`render_draw < 4.0 ms/frame`, `prim_verts <= 60,000`, `blend_switches <= 12`）を満たすことを確認した。
- [ ] Release 性能ベンチ（`bench_fluid`, `bench_game`）を計測・原因分析し、安全マージン確保の検証・改善判断（改善前後の実測表と根拠）を PR 本文に記録し、`fluid_step < 2.0 ms/tick`・`game_step < 4.0 ms/tick` を達成した。
- [ ] 実プレイで難易度・数値バランスを最終確認し、判断結果（変更した場合は定義テーブルと設計書の同期結果）を PR 本文と `docs/game-design.md` §13 へ記録した。
- [ ] `docs/qa-and-testing.md`・`docs/architecture.md` の CLI 契約・CTest 一覧を同期した（`docs/PLAN.md` Phase 6 `[完了]` と `README.txt` 最終化は P6-B へ残す）。
- [ ] スナップショット PNG・コンタクトシート・動画は `build/snaps/` へローカル保存のみで git 未コミット・PR 本文へ画像貼付なし、かつ §4.2 の機械確認がすべて注記どおりである。
- [ ] PR 作成直後に `@coderabbitai review` を投稿し、全指摘を 4 分類トリアージして必要な修正と返信を完了した（[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md) 準拠）。
- [ ] **CI の 3 ジョブ（`build-asan-and-test`, `static-analysis`, `dist-verify`）がすべて緑**（配布 zip に `src/app/demo.c` 等が自動収録され、素 `make` と `--headless-smoke` が通ること）。
