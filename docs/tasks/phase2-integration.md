# Phase 2-Int タスク指示書：P2-A / P2-B の統合・`GameState` のヒープ確保化・Phase 2 完了ゲート

> **必読**: [PLAN.md](../PLAN.md) §9（特に §9.2）、[architecture.md](../architecture.md) §2.1 / §2.2 / §4 / §5 / §8、[qa-and-testing.md](../qa-and-testing.md) §1〜§3 / §6、[AGENTS.md](../../AGENTS.md)、[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md)、[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)
>
> **前提**: [phase2-fluid.md](phase2-fluid.md)（P2-A / `feat/phase2-fluid`）と [phase2-ecs-meta.md](phase2-ecs-meta.md)（P2-B / `feat/phase2-ecs-meta`）の完了報告（ローカル検証の結果、P2-A のベンチ実測値）を受け取っていること。
> **作業場所 / ブランチ**: リポジトリルート（`full-scale-dev/Ink-storm`、作業ツリーがクリーンな状態）で `main` から `feat/phase2-integration` を作る。両ブランチを `--no-ff` で取り込んで統合コミットを積み、**1 本の PR** にまとめる。P2-A のブランチは単独では全体ビルドが通らないため、個別には `main` へ入れない。

---

## 0. 目的と禁止事項

P2-A（`ink_fluid`）と P2-B（`ink_ecs`・`src/game/meta.*`）を統合し、新しい `fluid_init` / `fluid_step` の契約に呼び出し側を合わせる。あわせて `GameState` をヒープ確保に移し、**Phase 2 完了ゲート（CI 全緑と、Release での `fluid_step` の平均・p95 < 2.0 ms/tick）** を確定させる。

**禁止事項**
- Phase 3 の内容を先取りしない。`GameState` へのフィールド追加（`run_settled`、`RunConfig` など）、`game_step`、`meta_settle_run`、セーブ I/O、敵、属性反応は P3-Pre 以降で行う。
- P2-A / P2-B の実装に不具合を見つけた場合は最小限の修正にとどめ、理由を PR に書く。指示書の契約（API・数値・判定基準）を変える必要がある場合は、ユーザーに確認する。

---

## 1. 変更対象ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `CMakeLists.txt` | 確認（衝突時のみ編集） | 両ブランチの追記を取り込む（§2.1） |
| `src/core/core.h` | 更新 | 共通定数（§2.2） |
| `src/game/game.h` / `src/game/game.c` | 更新 | 新しい流体 API への同期と `game_create` / `game_destroy`（§2.3） |
| `src/app/main.c` | 更新 | ヒープ確保化と共通定数の参照（§2.4） |
| `tests/test_core_smoke.c` | 更新 | ヒープ確保化と流体の健全性確認（§2.5） |

---

## 2. 統合手順

### 2.1 ブランチの取り込みと `CMakeLists.txt`
```sh
git switch -c feat/phase2-integration main
git merge --no-ff feat/phase2-fluid
git merge --no-ff feat/phase2-ecs-meta
```
- 担当ファイルが重ならず、`CMakeLists.txt` の挿入位置も分けてあるので、衝突しない想定。衝突した場合は両方のブロックを残す。マージ直後は `game.c` などが旧 API のままなのでビルドできない（§2.2〜§2.5 で解消する）。
- 取り込み後の `if(BUILD_TESTING)` は次の 6 ターゲットになる。並べ替えは不要。

| ターゲット | リンク | `add_test` |
| :--- | :--- | :---: |
| `test_core_smoke`（既存） | `unity ink_core ink_fluid ink_ecs ink_game ink_audio` | ○ |
| `test_fluid` | `unity ink_fluid ink_core m` | ○ |
| `bench_fluid` | `ink_fluid ink_core m` | ×（実行体のみ） |
| `test_cli_headless_smoke`（既存） | `inkstorm --headless-smoke` を実行 | ○ |
| `test_ecs` | `unity ink_ecs ink_core` | ○ |
| `test_roguelike_meta` | `unity ink_game ink_core` | ○ |

- 依存方向を確認する。`grep -rn 'raylib.h' src/core src/fluid src/ecs src/game src/audio` が 0 件であり、モジュール間のリンク行（`target_link_libraries` と `target_include_directories` の各行）が `main` から変わっていないこと。

### 2.2 `src/core/core.h` の共通定数（architecture §2.2）
```c
#define INK_SCREEN_W 1280
#define INK_SCREEN_H 720
#define INK_TICK_HZ  60
#define INK_FIXED_DT (1.0f / 60.0f)
```
`FLUID_*` と `ECS_*` は各モジュールのヘッダで定義済みなので、`core.h` には重複して定義しない。

### 2.3 `game.h` / `game.c`
- Phase 2 の `GameState` は、`FluidState`（約 0.88 MB）と `EcsWorld`（約 9 KB）からなる約 0.89 MB の構造体になる（Phase 3 以降で 1.3〜1.5 MB に増える）。Phase 2 ではフィールド構成を変えない。
- `game.h` に `GameState *game_create(uint64_t seed);` と `void game_destroy(GameState *game);` を追加する。既存の `game_init` / `game_step_smoke` は残す。
- `game.c` は `<stdlib.h>` を include し、次のとおり実装する。

| 関数 | 実装 |
| :--- | :--- |
| `game_create` | `calloc(1, sizeof *game)` で確保し、成功したら `game_init(game, seed)` を呼んで返す。失敗したら `NULL` を返す。解放の責任は呼び出し側が持つ |
| `game_destroy` | `free(game)`（`NULL` でもよい） |
| `game_init` | `game == NULL` なら何もしない。`fluid_init(&game->fluid, NULL)`、`ecs_init(&game->ecs)`、`game->rng = seed`、`game->ticks = 0` |
| `game_step_smoke` | `game == NULL` なら何もしない。`fluid_step(&game->fluid, INK_FIXED_DT)`、`ink_rng_next(&game->rng)`、`++game->ticks` |

- 確保は `game_create` の 1 回だけで、ティック処理の中では `malloc` / `free` しない（architecture §2.1）。

### 2.4 `src/app/main.c`
- `core.h` を include する。`enum` から `WINDOW_WIDTH`、`WINDOW_HEIGHT`、`TICK_HZ` を削除し（`HEADLESS_TICKS` は残す）、`INK_SCREEN_W`、`INK_SCREEN_H`、`INK_TICK_HZ` を使う。`step_dt` は `1.0 / (double)INK_TICK_HZ` にする。
- オプション解析が終わってから確保し、解放を `main` の 1 か所に集める。こうすると `run_window` の `!IsWindowReady()` による早期 return も含めて、すべての経路で解放される。

```c
    GameState *game = game_create(UINT64_C(1));
    if (game == NULL) {
        fputs("Failed to allocate game state.\n", stderr);
        return EXIT_FAILURE;
    }
    int status;
    if (headless) {
        AudioState audio;
        audio_init(&audio);
        status = run_headless_smoke(game, &audio);
    } else {
        status = run_window(game, use_post, smoke_frames);
    }
    game_destroy(game);
    return status;
```

- 解放し忘れると、ASan ビルドの `test_cli_headless_smoke` が LeakSanitizer で失敗する。

### 2.5 `tests/test_core_smoke.c`
- `test_game_advances_fluid_and_rng` を `GameState *game = game_create(UINT64_C(7)); TEST_ASSERT_NOT_NULL(game);` で始め、最後に `game_destroy(game);` を呼ぶ。`game.` によるアクセスはすべて `game->` に置き換える。
- 既存のアサーション（`ticks`、`fluid.steps`、RNG、`ecs_alive_count`）は残し、2 回の `game_step_smoke` の後に `TEST_ASSERT_TRUE(fluid_validate_finite(&game->fluid));` を加える。

---

## 3. 検証と Phase 2 完了ゲート

### 3.1 ローカル検証
`CMakeLists.txt` を取り込み、P2-A のブランチではプロジェクト全体の `lint` を実行できなかった。そのため、統合時にフルの ASan/UBSan + CTest と `format-check` / `lint` を 1 回だけローカルで実行する（inkstorm-verify-and-visual の「基盤を変更したとき」に当たる）。配布 zip の検証（`dist`）は CI の `dist-verify` に任せる。

```sh
# 1. ASan/UBSan で全ターゲットをビルドし、CTest 5 件（bench_fluid は対象外）・整形・静的解析を確認する
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan &&
    nix develop -c ctest --preset asan --output-on-failure &&
    nix develop -c cmake --build --preset asan --target format-check lint

# 2. Release: 性能ゲート（CI にはない）と、CLI・実ウィンドウのスモーク（main.c を変更したため）
nix develop -c cmake --preset release &&
    nix develop -c cmake --build --preset release &&
    ./build/release/bench_fluid &&
    ./build/release/inkstorm --headless-smoke &&
    nix develop -c ./build/release/inkstorm --smoke-frames 5 &&
    nix develop -c ./build/release/inkstorm --no-post --smoke-frames 5

# 3. GameState を値で宣言している箇所がないこと（0 件になる）
grep -rnE '\bGameState\s+[A-Za-z_][A-Za-z0-9_]*\s*(;|=|\[)' src tests
```

- 描画コード自体は変更していないので、キャプチャによる確認は不要。WSLg で `GALLIUM_DRIVER=d3d12` の描画ログが出ること、ポストシェーダの有効・無効の両方で終了コードが 0 になることを確認する。
- ローカルのツール（clang-format/clang-tidy 21、cppcheck 2.18）と CI（Ubuntu 24.04: 18、2.13）は版が違う。差が出たら CI の結果を正とし、CI に合わせて修正する。

### 3.2 設計書にまだ反映されていない決定
PR 本文にこの表を載せ、CodeRabbit から設計書との不一致を指摘されたら、この表を根拠に返信する。設計書の更新は別タスクで行う。

| 項目 | 本フェーズで採用した内容 | 関係する設計書 |
| :--- | :--- | :--- |
| `fluid_step` の処理順 | 渦度 → 投影 → 速度移流 → 投影 → スカラー移流（プロトタイプの実績順） | architecture §3.2 ステップ 4（要約順が異なる） |
| 流体セルの座標規約 | セル `(x, y)` が `[8x, 8x+8)` px を占め、中心は `(8x+4, 8y+4)` | architecture §4.1（記載なし） |
| ECS の走査補助 | `ecs_entity_at()` を追加 | architecture §5.1（API 一覧にない） |
| 精算 API | Phase 2 は `meta_settle_result(meta, res, &flag, out)`。`meta_settle_run(meta, game)` は P3-Pre でラップする | architecture §8.1 |
| 墨晶 ×1.5 の端数 | 切り捨て（`base / 2` を加算） | game-design §10.1（記載なし） |

### 3.3 PR
- PR の作成・push はユーザーの許可を得てから、inkstorm-pr-and-coderabbit の手順で行う。
- PR 本文には次を書く。
  - P2-A、P2-B、統合のそれぞれの変更概要
  - §3.1 の実行結果
  - ベンチの記録（CPU / OS / コンパイラ、5 試行の各値、試行中央値、ノイズ下限）
  - §3.2 の表
  - CI に任せた項目（`dist-verify`）

### 3.4 完了条件（Phase 2 Gate）
- [ ] CTest 5 件（`test_core_smoke`、`test_cli_headless_smoke`、`test_fluid`、`test_ecs`、`test_roguelike_meta`）が ASan/UBSan（`-Werror`）で通り、LeakSanitizer の報告もない。
- [ ] `GameState` は `main.c` と `tests/` のどこでも `game_create` / `game_destroy` で確保・解放されている（§3.1 の手順 3 が 0 件）。
- [ ] Release の `bench_fluid` が、5 試行でハッシュが一致し、`fluid_validate_finite` も満たす。試行中央値の mean と p95 がともに **`< 2.0 ms/tick`** で、測定環境と全試行の値を PR 本文に記録した。p95 だけがノイズ下限の膨張で超える場合は、しきい値を変えずにユーザーの判断を仰ぐ。
- [ ] WSLg で `--smoke-frames 5`（通常と `--no-post`）が終了コード 0 で終わる。
- [ ] GitHub Actions CI の 3 ジョブ（`build-asan-and-test`、`static-analysis`、`dist-verify`）がすべて緑。`dist-verify` で `tests/`（`bench_fluid.c` を含む）が配布 zip に入らず、`src/` の新しいファイル（`meta.c` など）は自動で収録され、素の `make` と `--headless-smoke` が通る。
- [ ] ユーザーの許可を得て `main` へマージしたら、P3-Pre（`GameState`、`GameEvent`、`GameCommand`、`RunConfig` の受け渡し、`meta_settle_run`）へ進める。
