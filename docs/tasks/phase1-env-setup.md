# Phase 1 実装タスク指示書：開発基盤・CI・CodeRabbit・AGENTS/Skill・最小スモーク構築

> **必読マスタードキュメント**: `docs/PLAN.md`
> **作業ディレクトリ**: `/home/g115/gakko/raylibgame/full-scale-dev/Ink-storm`

---

## 0. スコープと前提

1. **スコープ（完全な最小スモーク）**:
   - 開発環境・ビルド・テスト・静的解析・CI・CodeRabbit・スタンドアロン配布 zip 自動検証・`AGENTS.md` / 5本の Skill・最小スモークコードの構築のみ。
   - ゲーム本体のロジック、プロトタイプ移植、Fluid 数値アルゴリズム本実装、ECS コンポーネント詳細は含めない。
2. **環境前提**:
   - コンパイラはホストの `/usr/bin/gcc`（C11）。
   - raylib 6.2 は `${HOME}/.local/opt/raylib-6.2`（`include/raylib.h`, `lib/libraylib.a`）。
   - WSLg 環境（`/dev/dxg` 存在時）は `GALLIUM_DRIVER=d3d12` で GPU 描画する。

---

## 1. 実装項目

### 1.1 Nix 開発環境 (`flake.nix`, `flake.lock`, `.envrc`)
- `pkgs.mkShellNoCC` で開発ツールのみ供給（`cmake`, `ninja`, `clang-tools`, `cppcheck`, `glslang`, `gdb`, `ccache`, `zip`, `unzip`, `python3`, `shellcheck`）。
- `shellHook` で `RAYLIB_DIR` と WSLg 時の `GALLIUM_DRIVER=d3d12` を未設定時デフォルト設定。

### 1.2 CMake + Ninja + CTest (`CMakeLists.txt`, `CMakePresets.json`)
- C11、`-D_DEFAULT_SOURCE`、`CMAKE_EXPORT_COMPILE_COMMANDS ON`、`-Wall -Wextra -Wpedantic -Wshadow`。
- 7ターゲット（`ink_core`, `ink_fluid`, `ink_ecs`, `ink_game`, `ink_audio`, `ink_render`, `inkstorm`）の依存方向を CMake で強制し、`raylib` ヘッダは `ink_render` / `inkstorm` の `PRIVATE` のみ。
- `CMakePresets.json` に `debug` / `asan` / `release` の3プリセット。
- カスタムターゲット：`format-check`, `lint`, `dist`。

### 1.3 最小スモークコード (`src/*`, `assets/.gitkeep`)
- 各モジュール最小公開ヘッダ・スタブ実装、`src/render/` 埋め込み GLSL `#version 330` シェーダ＋フォールバック、`src/app/paths.c` 実行ファイル基準アセットパス解決（`NULL` / `..` 防御）、`src/app/main.c` 固定 60Hz accumulator＋`--headless-smoke` / `--smoke-frames N` / `--no-post`。

### 1.4 単体テスト (`third_party/unity/`, `tests/`)
- `ThrowTheSwitch/Unity` `v2.6.0` を `third_party/unity/` に配置し、CTest に `test_core_smoke` と `test_cli_headless_smoke` を登録。

### 1.5 フォーマット・静的解析・Git 設定
- `.clang-format`, `.clang-tidy`, `.gitignore`。

### 1.6 スタンドアロン配布アーカイブ＆検証 (`tools/package_dist.py`, `tools/dist.mk`, `README.txt`)
- `tools/package_dist.py` でホワイトリスト zip（`dist/inkstorm-src.zip`）を生成し、開発専用ファイル未混入をアサート後、一時展開先で `PATH=/usr/bin:/bin` の素の `make` + `./inkstorm --headless-smoke` を自動検証する。
- `tools/dist.mk` を zip 内ルート直下の `Makefile` として同梱し、`README.txt` にビルド方法と操作方法を記載する。

### 1.7 検証スクリプト (`tools/check_shaders.py`, `tools/extract_snaps.sh`)
- C 埋め込みシェーダ検査（文字列リテラル間コメント対応）および動画コンタクトシート生成スクリプト。

### 1.8 GitHub Actions CI (`.github/workflows/ci.yml`) & CodeRabbit (`.coderabbit.yaml`)
- CI：`ubuntu-24.04`、raylib 6.2 固定 SHA ビルド＆キャッシュ、`build-asan-and-test` / `static-analysis` / `dist-verify` の3ジョブ。
- CodeRabbit：Schema v2、`language: "ja-JP"`、`profile: "assertive"`、`cppcheck` / `shellcheck` / `github-checks`、モジュール別 `path_instructions`。

### 1.9 `AGENTS.md` と プロジェクト専用 Skill (`.agents/skills/` + `.claude/skills`)
- `AGENTS.md`（120〜150行、ローカル最小検証 vs CI 委譲ルール含む）と 5 本の Skill（`inkstorm-verify-and-visual`, `inkstorm-fluid-sim`, `inkstorm-content-authoring`, `inkstorm-pr-and-coderabbit`, `inkstorm-demo-and-dist`）、`.claude/skills -> ../.agents/skills` 相対シンボリックリンク。
