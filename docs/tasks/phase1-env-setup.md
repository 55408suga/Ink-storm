# Phase 1 実装タスク指示書：開発基盤・CI・CodeRabbit・AGENTS/Skill・最小スモーク構築

> **必読マスタードキュメント**: `docs/PLAN.md`（特に Section 1〜5）
> **作業ディレクトリ**: `/home/g115/gakko/raylibgame/full-scale-dev/Ink-storm`

---

## 0. 絶対ルール・スコープ制限

1. **スコープ厳守（完全な最小スモーク）**:
   - 本タスクのスコープは **開発環境・ビルド・テスト・静的解析・CI・CodeRabbit・提出パッケージ検証・`AGENTS.md`/5本のSkill・最小スモークコード** の構築のみ。
   - **ゲーム本体のロジック、既存プロトタイプ（`../../prototypes/inkstorm`）の移植、Fluid 数値アルゴリズム本実装、ECS コンポーネント詳細は作らないこと。**
2. **Git 操作の制約**:
   - **`git push` / `gh pr create` / `git commit` は実行しないこと。**
   - ※注意：Git リポジトリ内で `nix develop` を動かす場合、Nix flakes は未追跡（untracked）ファイルを無視します。そのため `flake.nix` など Nix 評価に必要なファイルは必要に応じて `git add -N <file>`（intent-to-add）だけ使って評価可能にし、コミット自体は行わずユーザー確認用に残すこと。
3. **既存環境の前提**:
   - コンパイラはホストの `/usr/bin/gcc` を標準とする（C11）。
   - raylib 6.2 は `${HOME}/.local/opt/raylib-6.2`（`include/raylib.h`, `lib/libraylib.a`）に配置済み。
   - WSLg 環境（`/dev/dxg` 存在時）は `GALLIUM_DRIVER=d3d12` で GPU 描画する。

---

## 1. 実装項目と詳細要件

### 1.1 Nix 開発環境 (`flake.nix`, `flake.lock`, `.envrc`)
- `pkgs.mkShellNoCC` を使用し、**ホストの `/usr/bin/gcc` および glibc / WSLg OpenGL と衝突させずに開発ツールのみ供給**すること。
- パッケージ一覧：
  - `cmake`, `ninja`, `clang-tools`（`clang-format` / `clang-tidy`）, `cppcheck`, `glslang`（`glslangValidator`）, `gdb`, `ccache`, `zip`, `unzip`, `python3`, `shellcheck`
- `shellHook`：
  - `RAYLIB_DIR` 未設定時は `${HOME}/.local/opt/raylib-6.2` を設定
  - `/dev/dxg` 存在かつ `GALLIUM_DRIVER` 未設定時は `GALLIUM_DRIVER=d3d12` を設定
- `.envrc` に `use flake` を記述。
- `nix flake lock` / `nix develop -c ...` がエラーなく通り、`flake.lock` が生成されること。

### 1.2 CMake + Ninja + CTest (`CMakeLists.txt`, `CMakePresets.json`)
- 規格：C11（`CMAKE_C_STANDARD 11`, `CMAKE_C_STANDARD_REQUIRED ON`, `CMAKE_C_EXTENSIONS OFF`）、`-D_DEFAULT_SOURCE`、`CMAKE_EXPORT_COMPILE_COMMANDS ON`。
- 共通警告：`-Wall -Wextra -Wpedantic -Wshadow`（`third_party/unity` は `-w` 等で外部警告を隔離）。
- **7ターゲット構成と依存方向の機械強制**：
  - `ink_core` (STATIC): `src/core/` — 外部依存なし（`m` のみ）
  - `ink_fluid` (STATIC): `src/fluid/` — `ink_core` のみ
  - `ink_ecs` (STATIC): `src/ecs/` — `ink_core` のみ
  - `ink_game` (STATIC): `src/game/` — `ink_core`, `ink_fluid`, `ink_ecs` のみ
  - `ink_audio` (STATIC): `src/audio/` — `ink_core`, `ink_game` のみ
  - `ink_render` (STATIC): `src/render/` — `ink_core`, `ink_fluid`, `ink_ecs`, `ink_game`, `raylib`
  - `inkstorm` (EXECUTABLE): `src/app/` — 全モジュール + `raylib`（`-lGL -lm -lpthread -ldl -lrt -lX11`）
  - **必須要件**: `raylib` の include ディレクトリは **`ink_render` と `inkstorm` にのみ `PRIVATE` で付与**し、`ink_core` / `ink_fluid` / `ink_ecs` / `ink_game` / `ink_audio` から `#include "raylib.h"` したら **コンパイルエラーになる** 構造にすること。
  - `RAYLIB_DIR` は CMake キャッシュ変数または環境変数 `RAYLIB_DIR` から解決し、未指定時は `$ENV{HOME}/.local/opt/raylib-6.2` にフォールバックすること。
- `CMakePresets.json`：
  - `configurePresets` / `buildPresets` / `testPresets` を `debug`, `asan`, `release` の3系統で用意：
    - `debug`: Ninja, `-O0 -g`, `build/debug`, `CMAKE_C_COMPILER=/usr/bin/gcc`
    - `asan`: Ninja, `-O1 -g -Werror -fsanitize=address,undefined -fno-omit-frame-pointer`, `build/asan`, `CMAKE_C_COMPILER=/usr/bin/gcc`
    - `release`: Ninja, `-O2 -DNDEBUG`, `build/release`, `CMAKE_C_COMPILER=/usr/bin/gcc`
- カスタムターゲット（`CMakeLists.txt`）：
  - `format-check`: `src/` と `tests/` 配下の `.c`/`.h` に対し `clang-format --dry-run --Werror`
  - `lint`: `clang-tidy`（`compile_commands.json` 参照、`src/` 対象）＋ `cppcheck` ＋ `tools/check_shaders.py src` ＋ `shellcheck`
  - `dist`: `python3 tools/package_submission.py` 実行

### 1.3 最小スモークコード (`src/*`, `assets/.gitkeep`)
ゲームロジックは書かず、モジュール境界・リンク・シェーダ検証・アセットパス解決・ヘッドレス実行が通る最小骨格のみ実装する：
- `src/core/core.h`, `src/core/core.c`: バージョン定数、`ink_clampf`, SplitMix64 (`ink_rng_next`, `ink_rng_f01`)
- `src/fluid/fluid.h`, `src/fluid/fluid.c`: 最小スタブ（`FluidState`, `fluid_init`, `fluid_step_smoke`）
- `src/ecs/ecs.h`, `src/ecs/ecs.c`: 最小スタブ（`EntityId`, `EcsWorld`, `ecs_init`, `ecs_alive_count`）
- `src/game/game.h`, `src/game/game.c`: 最小スタブ（`GameState`, `game_init`, `game_step_smoke`、内部で `core`/`fluid`/`ecs` を呼ぶ）
- `src/audio/audio.h`, `src/audio/audio.c`: 最小スタブ（`AudioState`, `audio_init`, `audio_render_silence`、`raylib` 非依存）
- `src/render/render.h`, `src/render/render.c`:
  - 最小の埋め込み GLSL `#version 330` ポストシェーダ（`tools/check_shaders.py` が実際に1本検証できるようにする）＋ `!IsShaderValid()` / `--no-post` 時の安全な非シェーダフォールバック。
  - 和紙色背景＋最小ステータス文字描画（`const GameState *` の読み取り専用参照）。
- `src/app/paths.h`, `src/app/paths.c`:
  - 実行ファイルの位置（Linux `/proc/self/exe`）基準で `assets/` への相対パスを解決し、存在確認できる関数（`app_resolve_asset_path`）。実行ディレクトリが変わっても落ちない設計。
- `src/app/main.c`:
  - CLI オプション：
    - `--headless-smoke`: ウィンドウを開かずに `game` + `audio` + `app_resolve_asset_path` の疎通・数ティック実行を行い exit 0
    - `--smoke-frames N`: ウィンドウを非表示（`FLAG_WINDOW_HIDDEN`）で開き、`N` フレーム描画（シェーダ＋フォールバック経路含む）して正常終了（自動 GPU スモーク検証用）
    - `--no-post`: ポストシェーダ無効化
    - 引数なし：通常ウィンドウ起動（1280x720, ESC / 閉じるで終了）
- `assets/.gitkeep` を配置。

### 1.4 単体テスト基盤 (`third_party/unity/`, `tests/`)
- `ThrowTheSwitch/Unity` タグ **`v2.6.0`** の `unity.c`, `unity.h`, `unity_internals.h`, `LICENSE.txt` を `third_party/unity/` に配置する（`third_party/unity/` は提出 zip から除外）。
- CTest に最低 2 本登録：
  1. `test_core_smoke`: Unity を使い `ink_core`, `ink_fluid`, `ink_ecs`, `ink_game`, `ink_audio` を検証（raylib 非依存でリンク）
  2. `test_cli_headless_smoke`: `inkstorm --headless-smoke` が exit 0 を返すことを検証

### 1.5 フォーマット・静的解析・Git 設定
- `.clang-format`: C11 向け（4スペース、列幅 100、Linux/Attach 等の統一スタイル、`third_party/` は対象外）
- `.clang-tidy`: `compile_commands.json` ベースで `bugprone-*`, `clang-analyzer-*`, `misc-*` 等からノイズが少なく実用的なルールを設定（`HeaderFilterRegex: 'src/.*'`）
- `.gitignore`: `build/`, `dist/`, `*.mp4`, `*.wav`, `.direnv/`, `.cache/`, `compile_commands.json`, `*.o`, `*.a` 等

### 1.6 提出パッケージ自動生成＆検証 (`tools/package_submission.py`, `tools/submission.mk`, `README.txt`)
- `tools/package_submission.py`:
  1. **ホワイトリスト方式**で `dist/inkstorm-submission.zip` を生成：
     - `src/` 配下の全 `.c` / `.h`
     - `assets/` 配下の実ファイル（`.gitkeep` は除外してよいが、空でも落ちないこと）
     - `third_party/` のうち、スクリプト内の `SUBMISSION_THIRD_PARTY = []`（現時点は空）に登録されたもののみ
     - `tools/submission.mk` → zip 内ルート直下の `Makefile` として配置
     - `README.txt` → zip 内ルート直下に配置
  2. 生成 zip のエントリ一覧を検査し、禁止ファイル（`AGENTS.md`, `CLAUDE.md`, `docs/`, `tests/`, `tools/`, `.github/`, `.coderabbit.yaml`, `.agents/`, `.claude/`, `flake.nix`, `flake.lock`, `.envrc`, `CMakeLists.txt`, `CMakePresets.json`, `third_party/unity/`）が **1つも含まれないこと** を自動アサート。
  3. 一時ディレクトリに zip を展開し、**`PATH=/usr/bin:/bin`（Nix ツール抜き）で `make RAYLIB_DIR=<raylib_dir>`** を実行し、生成された `./inkstorm --headless-smoke` が exit 0 で完了することまで検証する（`--no-verify` オプションでスキップ可能にしてもよいが、デフォルトは検証ON）。
- `tools/submission.mk`（提出用 `Makefile`）：
  - `RAYLIB_DIR ?= $(HOME)/.local/opt/raylib-6.2`
  - `GL_ENV ?= $(if $(wildcard /dev/dxg),GALLIUM_DRIVER=d3d12,)`
  - `src/` 配下のサブディレクトリ（`core`, `fluid`, `ecs`, `game`, `audio`, `render`, `app` および将来の配下サブディレクトリ）の `.c` を分割コンパイル（`build/obj/*.o`）して `./inkstorm` を生成。
  - ターゲット：`all`, `run`, `clean`。純粋な GNU Make + `gcc` のみで動作すること（CMake/Ninja/Python/Nix 不要）。
- `README.txt`：授業提出要件（コンパイル方法、`RAYLIB_DIR` 上書き方法、操作方法プレースホルダ、アピールポイントプレースホルダ）を記載した雛形。

### 1.7 既存検証ツールの取り込み (`tools/`)
- `../../.agents/skills/raylib-c11-dev/scripts/check_shaders.py` と `extract_snaps.sh` を `tools/` へコピーし、実行権限を付与、`tools/check_shaders.py src` および `shellcheck tools/extract_snaps.sh` が通ることを確認する。

### 1.8 GitHub Actions CI (`.github/workflows/ci.yml`)
- トリガ：`push`（`main`）、`pull_request`
- ランナー：`ubuntu-24.04`
- **raylib 6.2 の構築＆キャッシュ**：
  - `raysan5/raylib` のリポジトリを調査し、ローカルの `6.2`（`RAYLIB_VERSION "6.2"`）に対応する正確なタグまたは固定コミット SHA を裏取りして指定すること（存在しないタグを推測で書かないこと！もし 6.2 が特定コミットまたは `master` 由来なら、確実にビルド可能な固定 SHA を取得して明記する）。
  - `actions/cache` で `$HOME/.local/opt/raylib-6.2` をキャッシュ。
- **3ジョブ構成**：
  1. `build-asan-and-test`: CMake `asan` プリセット（ASan/UBSan/`-Werror`）ビルド → `ctest --preset asan --output-on-failure`
  2. `static-analysis`: `clang-format --dry-run --Werror`, `clang-tidy`, `cppcheck`, `python3 tools/check_shaders.py src`, `shellcheck tools/*.sh`
  3. `submission-verify`: `python3 tools/package_submission.py`（禁止ファイル混入チェック＋素の `make` ビルド＋`./inkstorm --headless-smoke`）

### 1.9 CodeRabbit 設定 (`.coderabbit.yaml`)
- **Schema v2** 準拠（1行目に `# yaml-language-server: $schema=https://coderabbit.ai/integrations/schema.v2.json`）。
- `language: "ja-JP"`
- `reviews.profile: "assertive"`
- `reviews.path_filters`: `!third_party/**`, `!assets/**`, `!flake.lock`, `!build/**`, `!dist/**`
- `reviews.tools`: `cppcheck.enabled: true`, `shellcheck.enabled: true`, `github-checks.enabled: true`
- `reviews.path_instructions`：
  - `src/core/**`: 外部依存禁止、グローバル状態・副作用禁止、C11 準拠
  - `src/fluid/**`: `raylib` およびゲーム固有ルール（敵・ダメージ・属性反応ロジック等）の混入禁止、境界チェック・NaN/Inf 発散対策
  - `src/ecs/**`: 世代チェック漏れ・ループ中破壊（即時削除）・範囲外アクセス確認
  - `src/game/**`: `raylib.h` 禁止、`rand()`/`time()` 等の非決定的API禁止、固定 60Hz タイムステップ厳守
  - `src/render/**`: `game` 状態の書き換え禁止（`const` 参照のみ）、シェーダ失敗時のフォールバック有無確認
  - `tools/package_submission.py` / `tools/submission.mk`: 開発用ファイル・AI doc・テストが提出 zip に混入しないこと、Nix/CMake 非依存で素の `make` が通ること
- `knowledge_base.opt_out: false`

### 1.10 `AGENTS.md` と プロジェクト専用 Skill (`.agents/skills/` + `.claude/skills`)
`docs/PLAN.md` の Section 5 に完全準拠して作成する：
- **`AGENTS.md`**（120〜150行以内で簡潔に保つ、`CLAUDE.md` は作らない）：
  1. プロジェクト要約・マスター参照先（`docs/PLAN.md`）
  2. 標準コマンド早見表（`nix develop -c ...` の `asan` / `release` / `ctest` / `format-check` / `lint` / `package_submission.py`）
  3. モジュール依存ルール（絶対禁止リスト）
  4. C11・メモリ・決定性ルール
  5. ECS / Fluid / アセット鉄則
  6. Git / PR / 提出衛生ルール（worktree 運用含む）
  7. タスク完了前チェックリスト
  8. 下記 5 本の Skill 一覧と発火条件
- **`.agents/skills/` 配下に 5 本の `SKILL.md`**（YAML frontmatter `name`, `description` 必須）：
  1. `inkstorm-verify-and-visual/SKILL.md`
  2. `inkstorm-fluid-sim/SKILL.md`
  3. `inkstorm-content-authoring/SKILL.md`
  4. `inkstorm-pr-and-coderabbit/SKILL.md`（PR前ゲート → `gh` で CodeRabbit コメント取得 → 盲従禁止の4分類トリアージ `Must Fix` / `Should Fix` / `Defer・Won't Fix` / `Verify First` → Must/Should のみ修正＆再検証 → 見送り理由をPRコメント返信）
  5. `inkstorm-submission-demo/SKILL.md`
- **`.claude/skills`** を `../.agents/skills` への相対シンボリックリンクとして作成する（`readlink .claude/skills` が `../.agents/skills` になること）。

---

## 2. 必須検証チェックリスト（すべて実機実行し、結果を報告すること）

以下をすべて実行してパスを確認してから完了報告してください：

1. **ASan/UBSan + CTest**：
   `nix develop -c cmake --preset asan && nix develop -c cmake --build --preset asan && nix develop -c ctest --preset asan --output-on-failure`
2. **Release ビルド + ヘッドレス＆GPU スモーク**：
   `nix develop -c cmake --preset release && nix develop -c cmake --build --preset release`
   `./build/release/inkstorm --headless-smoke`
   `GALLIUM_DRIVER=d3d12 ./build/release/inkstorm --smoke-frames 5`（および `--no-post --smoke-frames 5`）
3. **フォーマット・静的解析・シェーダ・シェルスクリプト検査**：
   `nix develop -c cmake --build --preset asan --target format-check`
   `nix develop -c cmake --build --preset asan --target lint`
4. **依存強制のネガティブテスト**：
   `src/game/game.c` に一時的に `#include "raylib.h"` を追加してビルドが失敗することを確認し、**必ず元に戻して**ビルド再通過を確認。
5. **提出パッケージ自動生成＆Nix 抜き素 `make` 検証**：
   `nix develop -c cmake --build --preset release --target dist`（または `python3 tools/package_submission.py`）が成功し、禁止ファイル混入ゼロ・一時展開先での素 `make`・`./inkstorm --headless-smoke` までパスすること。
6. **YAML 構文検証**：
   Python (`yaml.safe_load` 等) で `.github/workflows/ci.yml` と `.coderabbit.yaml` に構文エラーがないことを確認。
7. **シンボリックリンク確認**：
   `.claude/skills` が `../.agents/skills` を指し、5本の `SKILL.md` が両方から読めること。

完了時は **作成・変更ファイル一覧、各検証コマンドの実行結果サマリ、注意点や未解決事項があれば明記** して報告してください（`git commit` / `git push` は行わないこと）。
