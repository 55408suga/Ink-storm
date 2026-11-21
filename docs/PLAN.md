# INKSTORM（墨嵐）本開発 実装プラン

> **位置づけ**: 本ドキュメントは、INKSTORM（墨嵐）の本開発および環境構築を AI コーディングエージェントへ分割・委譲するためのマスタープラン（Single Source of Truth）である。
> 相談の進行に合わせて各セクションを確定（`[確定]`）へ更新し、確定したフェーズからタスクを切り出して実装する。
>
> ※ 本ファイルおよび `docs/`, `AGENTS.md`, `tests/`, `tools/`, `flake.nix` 等の開発支援ファイルは **提出用 zip には含めない**（パッケージングスクリプトで自動除外する）。

---

## 1. プロジェクト概要・ゴール・制約 `[確定]`

### 1.1 目的とスケジュール
- **対象授業**: プログラミング第1同演習（河野先生 / sslab）最終課題
- **目標**: ソースコード提出＋90秒以内のデモ動画提出により **試験免除** を獲得する
- **締切想定**: **2027年1月前半**（開発期間：約6〜7週間）
- **リファレンス実装**: `../../prototypes/inkstorm/`（純 C11 + raylib 6.2、2,780行で動作確認済みのプロトタイプ）

### 1.2 提出要件と運用制約
- **提出物 1（ソース一式 zip）**:
  - `README.txt` 必須（コンパイル方法、ゲームの遊び方、アピールポイント）
  - コンパイルおよび実行に必要な全ファイル（`src/`, `third_party/` の本体依存分, `assets/`, 提出用 `Makefile`）を同梱
  - 採点環境（KIC Linux 想定）に **Nix / CMake / Ninja / ネット接続を要求しない**（素の `gcc` + `make` + `raylib` でオフライン一発ビルド）
- **提出物 2（デモ動画）**:
  - zip とは別に提出（1分30秒以内、mp4/mov、操作説明＋実デモ操作）
  - 終盤 5〜7 秒で `F1` X線オーバーレイ（流体速度ベクトル・渦度ヒートマップ・計算時間 ms）を表示し、裏の Navier-Stokes 技術を視覚証明する
- **未確認事項（授業で随時確認し本欄を更新）**:
  - [ ] 正確な提出締切日時
  - [ ] KIC Linux の raylib バージョン・インストールパス・標準コンパイルコマンド
  - [ ] 外部素材（BGM / SE / フォント / 画像）の著作権・利用ルールと zip サイズ上限
  - [ ] 個人課題かグループ課題か、AI 利用の開示ルール

---

## 2. アーキテクチャ方針 `[確定]`

### 2.1 基本原則
**「純 C11 + 独立 Fluid Engine + 超軽量 Hybrid ECS + ハイブリッドデータ駆動 + 厳密な一方向依存」**

1. **開発環境と提出パッケージの2層分離**
   - **開発環境**: Determinate Nix (`flake.nix` + `.envrc`) ＋ CMake + Ninja + CTest + `clang-format` / `clang-tidy` / `cppcheck` + ASan/UBSan で AI 開発効率と静的検証を最大化する。
   - **提出パッケージ**: `tools/package_submission.py`（または `cmake --build --target dist`）で、開発用ファイル・AI用doc・テストを自動除外し、**素の `Makefile` を同梱した自己完結 zip** を生成。さらに一時ディレクトリへ展開して Nix 抜きの素の `/usr/bin/gcc` + `/usr/bin/make` でビルド＆ヘッドレス動作を自動検証する。
2. **言語規格**: **C11**（`-std=c11 -Wall -Wextra -Wpedantic -Wshadow`、検証ビルドでは `-Werror -fsanitize=address,undefined`）。
3. **WSLg + Nix 共存方針**:
   - `flake.nix` は `pkgs.mkShellNoCC` を使用し、Nix から `cmake`, `ninja`, `clang-tools`（`clang-format` / `clang-tidy`）, `cppcheck`, `gdb`, `ccache`, `zip`, `unzip` 等の **開発ツールのみ供給** する。
   - コンパイラと raylib はホストの `/usr/bin/gcc`（or `/usr/bin/clang`）＋ `${RAYLIB_DIR:-$HOME/.local/opt/raylib-6.2}` をそのまま使用することで、WSLg の GPU（`GALLIUM_DRIVER=d3d12`）および glibc ABI の不整合を回避し、KIC Linux に近いリンク挙動を維持する。
4. **決定性とヘッドレス検証**:
   - `core`, `fluid`, `ecs`, `game`, `audio` は **raylib を一切 include しない**。
   - 固定タイムステップ（60Hz）＋明示的 RNG（SplitMix64）により、同一シード＋同一入力で完全再現可能にする。
5. **外部アセット＋安全フォールバック**:
   - BGM / SE / フォント / テクスチャ等は `assets/` に配置して zip 同梱する。
   - 実行ファイル基準のパス解決を行い、読み込み失敗時でも既存プロシージャルシンセやデフォルトフォント等へ安全にフォールバックし、クラッシュさせない。

### 2.2 独立 Fluid Simulation Engine (`src/fluid/`)
- 現行プロトタイプ（160×90 Stable Fluids：Semi-Lagrangian 移流＋Gauss-Seidel 圧力投影＋Vorticity Confinement）の数値計算ロジックを独立モジュール化する。
- **責務分離**:
  - `fluid` の責務：速度場 `(u, v)`、圧力投影、渦度閉じ込め、境界条件、汎用スカラー場（複数チャンネル：Ink / Fire / Storm / 拡張チャンネル）の移流・減衰・注入・バイリニアサンプリング、計測統計。
  - `game/elements` の責務：墨×炎の着火（Blazing Inkstorm）、墨×雷の伝導（Tempest Lightning）、新属性の反応ルール（`fluid` 内部にゲームルールを直書きしない）。
- Cellular Automata は初期導入しない（紙の残墨や焦げ等が必要になった場合は低減衰スカラー場で表現し、別シミュレーション基盤は作らない）。

### 2.3 超軽量 Hybrid ECS (`src/ecs/`)
外部巨大フレームワーク（Flecs 等）は採用せず、**300〜500行程度の純 C11・依存ゼロの超軽量 Hybrid ECS** を自作する。

- **採用対象（ECS で管理するもの）**:
  - 敵、弾（矢・火球・雷球等）、召喚物、設置ギミック／トラップ、回収物（墨玉・遺物ドロップ等）、一部持続エフェクトエンティティ。
- **非対象（ECS 外の独立モジュールで管理するもの）**:
  - Fluid Solver（セルを Entity にしない）、Game Loop / Scene Manager、Run Progression / Upgrade State、Audio、大量の軽量パーティクル（固定リングバッファ）。
- **初期内部設計**:
  - **世代付き Entity ID**: `typedef struct { uint32_t index; uint32_t gen; } EntityId;`（`ENTITY_NULL = {0, 0}`、`ecs_is_alive(w, id)` でダングリング参照を100%防止）
  - **固定長 Component 配列（SoA 的レイアウト）**: `ECS_MAX_ENTITIES`（初期案 512〜1024）に対し Component 種別ごとの連続固定配列（動的メモリ確保なし・キャッシュ局所性◎・完全決定的イテレーション）
  - **64-bit コンポーネントビットマスク**: `uint64_t comp_mask[ECS_MAX_ENTITIES]` で最大64種コンポーネントを高速判定
  - **遅延削除（Deferred Destroy）**: `ecs_destroy()` は削除フラグのみ立て、各ステップの確定フェーズ（End-of-Tick）で一括回収＋`gen++` してフリーリストへ返却
  - **将来拡張性**: 公開 API（`ecs_create`, `ecs_destroy`, `ecs_add_*`, `ecs_get_*`, `ecs_has` 等）を維持したまま、将来必要になれば内部を Sparse Set へ差し替え可能

### 2.4 ハイブリッドデータ定義（敵・アップグレード・アビリティ・ウェーブ）
- **基本（C11 構造体テーブル）**:
  - 指示付き初期化子（Designated Initializers）による `const EnemyDef[]`, `const UpgradeDef[]`, `const AbilityDef[]` と、挙動・効果フック用の関数ポインタテーブルを採用する。
  - 巨大な `switch` 文への追記を避け、新敵・新アップグレード追加時の変更範囲を局所化する。
- **拡張（外部パラメータオーバーライド余地）**:
  - バランス調整をリコンパイルなしで回したくなった段階で、`third_party/cJSON`（2ファイル同梱）等を使い、数値フィールドだけ `data/*.json` で上書きできる構造を許容する（初期実装では必須にしない）。

---

## 3. ディレクトリ構成とモジュール境界 `[確定]`

### 3.1 リポジトリ構成
```text
Ink-storm/
├── flake.nix                 # Nix 開発環境 (mkShellNoCC: cmake/ninja/clang-tools/cppcheck/glslang/gdb/zip)
├── flake.lock
├── .envrc                    # direnv (use flake)
├── CMakeLists.txt            # 7ターゲット定義 + 依存強制 + CTest + dist ターゲット
├── CMakePresets.json         # debug / asan / release
├── .clang-format
├── .clang-tidy
├── .gitignore
├── .coderabbit.yaml          # CodeRabbit レビュー設定 (Schema v2)
├── .github/
│   └── workflows/
│       └── ci.yml            # GitHub Actions CI (ASan/UBSan・静的解析・提出zip素make検証)
├── .agents/
│   └── skills/               # プロジェクト専用 AI スキル本体（提出除外）
├── .claude/
│   └── skills -> ../.agents/skills  # Claude Code 互換シンボリックリンク（提出除外）
├── AGENTS.md                 # AI エージェント開発規約（Claude Code/Codex/Antigravity/CodeRabbit共通・提出除外）
├── README.txt                # 提出用 README（コンパイル方法・操作・アピールポイント）
├── docs/
│   ├── PLAN.md               # 本ファイル（全体計画・要件・タスク分割）
│   └── architecture.md       # 詳細なモジュール契約・シミュレーションパイプライン（必要に応じ分離）
├── src/
│   ├── core/                 # ターゲット: ink_core
│   ├── fluid/                # ターゲット: ink_fluid
│   ├── ecs/                  # ターゲット: ink_ecs
│   ├── game/                 # ターゲット: ink_game (sim / brush / elements / combat / enemies / roguelike)
│   ├── audio/                # ターゲット: ink_audio (BGM/SE 再生インターフェース + プロシージャルシンセ)
│   ├── render/               # ターゲット: ink_render (流体テクスチャ / シェーダ / キャラ / UI / VFX / X線)
│   └── app/                  # ターゲット: inkstorm (main.c / シーン / 入力 / 録画 / アセット管理)
├── assets/                   # BGM / SE / フォント / テクスチャ（提出同梱）
├── third_party/              # ソース同梱ライブラリ（提出同梱対象と tests 専用を明確に分離）
│   └── unity/                # C単体テスト用 Unity v2.6.0（tests のみ使用・提出除外）
├── tests/                    # 単体・流体数値・ECS・決定性回帰・ベンチマーク（提出除外）
└── tools/                    # package_submission.py / check_shaders.py / extract_snaps.sh / submission.mk（提出除外）
```

### 3.2 モジュール依存ルール（CMake ターゲットで機械的に強制）

```text
app ──┬──▶ render ──▶ game ──┬──▶ fluid ──▶ core
      │                      ├──▶ ecs   ──▶ core
      │                      └──▶ core
      ├──▶ audio  ──▶ game (読み取り専用イベント) / core
      └──▶ game / fluid / ecs / core
```

| モジュール | 責務 | 依存可能先 | raylib 依存 |
| :--- | :--- | :--- | :---: |
| `ink_core` | 共通型、数学、SplitMix64 RNG、タイマー/ハッシュ補助 | 標準Cライブラリのみ | **不可** |
| `ink_fluid` | Stable Fluids 数値計算、多チャンネルスカラー移流、注入/サンプリング API | `ink_core` | **不可** |
| `ink_ecs` | 世代付き Entity、固定長 Component ストレージ、64bit マスク、遅延削除 | `ink_core` | **不可** |
| `ink_game` | 固定60Hzパイプライン、筆・墨経済、属性反応、戦闘、敵AI、ローグライク進行 | `ink_core`, `ink_fluid`, `ink_ecs` | **不可** |
| `ink_audio` | プロシージャル和楽器/風音シンセ、WAV書き出し、BGM/SEトリガ定義 | `ink_core`, `ink_game`（イベント参照） | **分離**（※raylib AudioStream 接続は `app` 側または薄いアダプタに隔離） |
| `ink_render` | 和紙ポストシェーダ、流体可視化、妖怪・筆・UI・VFX・`F1` X線オーバーレイ | `ink_core`, `ink_fluid`, `ink_ecs`, `ink_game`（すべて `const` 参照）, `raylib` | 可 |
| `app` (`inkstorm`) | `main.c`、ウィンドウ・入力収集、アセットロード、シーン遷移、録画/デモ/CLI | 全モジュール, `raylib` | 可 |

---

## 4. Phase 1：開発基盤・CI・CodeRabbit・最小スモーク構築仕様 `[確定]`

> **担当方針**: Phase 1 は依存関係が強いため、**1体の AI エージェントによる統合タスク**として実行する。
> **スコープ制限**: ゲーム本体のロジック・ECS コンポーネント詳細・流体アルゴリズム本実装は **含めない**（完全な最小スモークのみ）。

### 4.1 成果物一覧と詳細要件

#### (1) Nix 開発環境 (`flake.nix`, `flake.lock`, `.envrc`)
- `nixpkgs` を固定し、`pkgs.mkShellNoCC` で以下を供給する（ホスト `/usr/bin/gcc` および `~/.local/opt/raylib-6.2` を上書きしない）：
  - `cmake`, `ninja`, `clang-tools`（`clang-format` / `clang-tidy`）, `cppcheck`, `glslang`（`glslangValidator`）, `gdb`, `ccache`, `zip`, `unzip`, `python3`, `shellcheck`
- `shellHook` で以下を案内・設定：
  - `RAYLIB_DIR` が未設定なら `${HOME}/.local/opt/raylib-6.2` をデフォルト設定
  - `/dev/dxg` が存在する場合（WSLg）は `GALLIUM_DRIVER=d3d12` を設定（既存値があれば尊重）
- `.envrc` に `use flake` を記載する。

#### (2) CMake + Ninja + CTest (`CMakeLists.txt`, `CMakePresets.json`)
- **言語・コンパイラ**：C11（`CMAKE_C_STANDARD 11`, `CMAKE_C_STANDARD_REQUIRED ON`, `CMAKE_C_EXTENSIONS OFF`）、コンパイラはホスト `/usr/bin/gcc` を標準とし、`-D_DEFAULT_SOURCE` を付与。
- **`CMAKE_EXPORT_COMPILE_COMMANDS ON`** を常時有効化（`clang-tidy` / LSP / AI 支援用）。
- **警告フラグ**：共通で `-Wall -Wextra -Wpedantic -Wshadow`（`third_party/unity` には `-w` 等で外部警告を隔離）。
- **7ターゲットと依存強制**：
  - `ink_core` (STATIC): `src/core/` — 外部依存なし（`m` のみ）
  - `ink_fluid` (STATIC): `src/fluid/` — `ink_core` のみリンク
  - `ink_ecs` (STATIC): `src/ecs/` — `ink_core` のみリンク
  - `ink_game` (STATIC): `src/game/` — `ink_core`, `ink_fluid`, `ink_ecs` のみリンク
  - `ink_audio` (STATIC): `src/audio/` — `ink_core`, `ink_game` のみリンク
  - `ink_render` (STATIC): `src/render/` — `ink_core`, `ink_fluid`, `ink_ecs`, `ink_game`, `raylib` リンク
  - `inkstorm` (EXECUTABLE): `src/app/` — 上記すべて + `raylib` (`-lGL -lm -lpthread -ldl -lrt -lX11`)
  - **重要**：`raylib` の include パスは `ink_render` と `inkstorm` にのみ `PRIVATE` で付与し、`ink_core` / `ink_fluid` / `ink_ecs` / `ink_game` / `ink_audio` から `#include "raylib.h"` すると **コンパイルエラーになる** 構造を強制する。
- **`CMakePresets.json`（3プリセット）**：
  - `debug`: Ninja, `-O0 -g`, `build/debug`
  - `asan`: Ninja, `-O1 -g -Werror -fsanitize=address,undefined -fno-omit-frame-pointer`, `build/asan`
  - `release`: Ninja, `-O2 -DNDEBUG`, `build/release`
- **カスタムターゲット**：
  - `format-check` (`clang-format --dry-run --Werror`)
  - `lint` (`clang-tidy` + `cppcheck` + `tools/check_shaders.py`)
  - `dist` (`tools/package_submission.py` による提出 zip 生成＆素 `make` 検証)

#### (3) 最小スモークコード (`src/*`, `assets/.gitkeep`)
各モジュールに最小の公開ヘッダと疎通実装のみを配置する（ゲームロジックやプロトタイプ移植は Phase 2 以降）：
- `src/core/`: バージョン定数・`ink_clampf`・SplitMix64 最小関数など
- `src/fluid/`: 最小スタブ（`FluidSmoke` 初期化・1ステップ関数）
- `src/ecs/`: 最小スタブ（`EcsSmoke` 初期化関数）
- `src/game/`: 最小スタブ（`GameSmoke` 初期化・1ティック更新・スモーク状態取得）
- `src/audio/`: 最小スタブ（初期化・無音PCM生成確認）
- `src/render/`: 最小スタブ（和紙色背景クリア＋最小テキスト表示、ポストシェーダ有無のフォールバック骨格）
- `src/app/`:
  - `paths.h` / `paths.c`：実行ファイル位置基準で `assets/` のパスを解決し、存在確認できるユーティリティ（作業ディレクトリ違いに強い設計）
  - `main.c`：CLI 引数 `--headless-smoke`（ウィンドウを開かずに全非raylibモジュールの初期化＋数ティック実行して exit 0）、`--no-post`、および通常起動（空ウィンドウ描画・ESC/閉ボタンで正常終了）

#### (4) 単体テスト基盤 (`third_party/unity/`, `tests/`)
- `ThrowTheSwitch/Unity` の固定タグ **`v2.6.0`** から `unity.c`, `unity.h`, `unity_internals.h` および `LICENSE.txt` を `third_party/unity/` に配置する（提出 zip からは除外）。
- CTest に最低 2 本のテストを登録：
  1. `test_core_smoke`（`ink_core` / `ink_fluid` / `ink_ecs` / `ink_game` / `ink_audio` を Unity で検証、raylib 非依存）
  2. `test_cli_headless_smoke`（ビルド済み `inkstorm --headless-smoke` の終了コード 0 を検証）

#### (5) フォーマット・静的解析・Git 設定
- `.clang-format`：C11 向け統一スタイル（4スペースインデント、列幅 100〜110、Linux/K&R ブレース等、`third_party/` は除外）
- `.clang-tidy`：`compile_commands.json` ベースの C11 向けチェック（`bugprone-*`, `clang-analyzer-*`, `misc-*`, `readability-*` からノイズの少ない実用的セットを選定、`third_party/` は除外）
- `.gitignore`：`build/`, `dist/`, `*.mp4`, `*.wav`, `.direnv/`, `.cache/`, `compile_commands.json` 等

#### (6) 提出パッケージ自動生成＆検証 (`tools/package_submission.py`, `tools/submission.mk`, `README.txt`)
- `tools/package_submission.py` が以下を1コマンドで実行する：
  1. `dist/inkstorm-submission.zip` を生成。同梱対象は **ホワイトリスト方式**：
     - `src/`（全 `.c`/`.h`）
     - `assets/`（実アセットファイルのみ）
     - `third_party/` のうち **本体依存リスト** に登録されたもののみ（`third_party/unity/` は絶対に含めない）
     - `tools/submission.mk` をルート直下の `Makefile` として配置
     - `README.txt`
  2. 生成された zip 内に禁止ファイル（`AGENTS.md`, `CLAUDE.md`, `docs/`, `tests/`, `tools/`, `.github/`, `.coderabbit.yaml`, `.agents/`, `.claude/`, `flake.nix`, `flake.lock`, `.envrc`, `CMakeLists.txt`, `CMakePresets.json`, `third_party/unity/`）が **1つも含まれないこと** を自動アサート。
  3. 一時ディレクトリに zip を展開し、`PATH=/usr/bin:/bin` 相当（Nix ツール抜き）で `make RAYLIB_DIR=...` を実行 → 生成バイナリで `./inkstorm --headless-smoke` が終了コード 0 で通ることを自動検証。
- `tools/submission.mk`（提出用 `Makefile`）：
  - `RAYLIB_DIR ?= $(HOME)/.local/opt/raylib-6.2`（KIC Linux で `make RAYLIB_DIR=...` 上書き可能）
  - `find src -name '*.c'` 相当（または明示サブディレクトリ wildcard）で再帰的に `.c` を分割コンパイルして `./inkstorm` を生成。
  - `all`, `run`, `clean` のシンプルかつ堅牢なターゲットを提供。
- `README.txt`：提出要件（コンパイル方法・遊び方・アピールポイント）の雛形を整備。

#### (7) 既存検証ツールの取り込み (`tools/`)
- `../../.agents/skills/raylib-c11-dev/scripts/check_shaders.py` と `extract_snaps.sh` を `tools/` へコピー・調整し、CI およびローカル検証から呼べるようにする。

#### (8) GitHub Actions CI (`.github/workflows/ci.yml`)
- トリガ：`push`（`main`）、`pull_request`
- ランナー：`ubuntu-24.04`
- **raylib 6.2 の構築＆キャッシュ**：
  - `raysan5/raylib` の **v6.2 対応タグ／固定コミット** から静的ライブラリ `libraylib.a` をビルドし、`actions/cache` で `$HOME/.local/opt/raylib-6.2` にキャッシュする（※Phase 1 実装時に正確なタグ/コミットハッシュを GitHub 上で裏取りして固定する）。
- **ジョブ構成**：
  1. `build-asan-and-test`：CMake `asan` プリセット（ASan/UBSan/`-Werror`）ビルド → `ctest --output-on-failure`
  2. `static-analysis`：`clang-format --dry-run --Werror` / `clang-tidy` / `cppcheck` / `tools/check_shaders.py src` / `shellcheck`
  3. `submission-verify`：`tools/package_submission.py` 実行（ホワイトリスト検証＋禁止ファイル混入チェック＋素の `make` ビルド＋`--headless-smoke` 実行）

#### (9) CodeRabbit 設定 (`.coderabbit.yaml`)
- **Schema v2**（`# yaml-language-server: $schema=https://coderabbit.ai/integrations/schema.v2.json`）に準拠。
- `language: "ja-JP"`
- `reviews.profile: "assertive"`（AI エージェント生成 PR の粗を厳しく拾う設定）
- `reviews.path_filters`：`!third_party/**`, `!assets/**`, `!flake.lock`, `!build/**`, `!dist/**`
- `reviews.tools`：`cppcheck.enabled: true`, `shellcheck.enabled: true`, `github-checks.enabled: true`
- `reviews.path_instructions`（モジュール境界の自動レビュー強制）：
  - `src/core/**`：外部依存禁止、副作用・グローバル状態禁止、C11 準拠確認
  - `src/fluid/**`：raylib およびゲーム固有ルール（敵・ダメージ・属性反応ロジック等）の混入禁止、境界チェック・NaN/Inf 発散対策確認
  - `src/ecs/**`：世代チェック漏れ・ループ中破壊（即時削除）・範囲外アクセス確認
  - `src/game/**`：`raylib.h` 禁止、非決定的要素（`rand()`, `time()`, 未初期化メモリ）禁止、固定タイムステップ厳守
  - `src/render/**`：`game` 状態の変更禁止（`const` 参照のみ）、シェーダ失敗時フォールバック有無確認
  - `tools/package_submission.py` / `tools/submission.mk`：開発ファイル・AI doc・テストが提出 zip に混入しないこと、Nix/CMake 非依存で素の `make` が通ること
- `knowledge_base.opt_out: false`（リポジトリルートの `AGENTS.md` を CodeRabbit にも参照させる）

#### (10) `AGENTS.md` およびプロジェクト専用 Skill (`.agents/skills/` + `.claude/skills`)
- **Section 5** の確定仕様に従って作成する。

### 4.2 Phase 1 完了条件（Definition of Done）
1. `nix develop -c cmake --preset asan && nix develop -c cmake --build --preset asan && nix develop -c ctest --preset asan --output-on-failure` がエラー・警告ゼロで通る。
2. `nix develop -c cmake --preset release && nix develop -c cmake --build --preset release` が通り、WSLg 上で通常起動（ウィンドウ表示・正常終了）と `--headless-smoke` の両方が成功する。
3. `clang-format --dry-run --Werror`、`clang-tidy`、`cppcheck`、`tools/check_shaders.py`、`shellcheck` がすべてパスする。
4. `ink_game` に一時的に `#include "raylib.h"` を書いてビルドが失敗する（依存強制が効いている）ことを確認し、戻す。
5. `tools/package_submission.py` が成功し、禁止ファイル混入ゼロ＋Nix 抜き素の `make` ビルド＋`--headless-smoke` が通る。
6. `.github/workflows/ci.yml` と `.coderabbit.yaml` が構文検証済みで配置されている。
7. `AGENTS.md`、`.agents/skills/`（5本）、`.claude/skills -> ../.agents/skills` が配置され、提出 zip に含まれないことが確認されている。

---

## 5. `AGENTS.md`・プロジェクト専用 Skill・エージェント運用仕様 `[確定]`

### 5.1 エージェント起動ディレクトリと並列運用ルール
- **起動ディレクトリ（必須）**: AI エージェント（Claude Code / Codex / Antigravity）は **`full-scale-dev/Ink-storm/`（リポジトリルート）** で起動する。
  - `full-scale-dev/`（親ディレクトリ）で起動すると、`.git` 未検出・`AGENTS.md`/Skill 自動発見漏れ・`direnv` 未発火・親の旧プロトタイプ用スキル（`raylib-c11-dev`）誤読み込みの原因になる。
- **並列開発時の `git worktree` 運用**:
  - 並列エージェントを走らせる場合は、`full-scale-dev/` 配下に兄弟 worktree（例：`full-scale-dev/wt-fluid`, `full-scale-dev/wt-enemies`）を作成し、各エージェントをそれぞれの worktree ルートで起動する。
- **マルチツール互換**:
  - ルール本体は `AGENTS.md` に一本化（Claude Code は `CLAUDE.md` 不在時に `AGENTS.md` をネイティブ読込、Codex / Antigravity / CodeRabbit も `AGENTS.md` を読込）。
  - Skill 本体は `.agents/skills/<name>/SKILL.md` に置き、`.claude/skills` を `../.agents/skills` への相対シンボリックリンクにして二重管理を防ぐ。

### 5.2 `AGENTS.md` 仕様（120〜150行以内で簡潔に保つ）
1. **プロジェクト要約とマスター参照先**：C11 + raylib 6.2 水墨流体アクション、`docs/PLAN.md` へのポインタ
2. **標準コマンド早見表**：
   - `nix develop -c cmake --preset asan && nix develop -c cmake --build --preset asan && nix develop -c ctest --preset asan --output-on-failure`
   - `nix develop -c cmake --preset release && nix develop -c cmake --build --preset release`
   - `format-check` / `lint` / `tools/package_submission.py`
3. **モジュール依存ルール（絶対禁止）**：
   - `core`/`fluid`/`ecs`/`game`/`audio` での `raylib.h` include 禁止
   - `fluid` へのゲームルール・ダメージ・属性反応ロジック混入禁止
   - `render` からの `game` 状態書き換え禁止（`const` 参照のみ）
4. **C11・メモリ・決定性ルール**：`-std=c11`、`malloc` 最小化＆所有権明示、`rand()`/`time()` 禁止（SplitMix64 使用）、固定 60Hz 厳守
5. **ECS / Fluid / アセット鉄則**：流体セルの Entity 化禁止、ECS は世代チェック＆遅延削除必須、アセットは実行ファイル基準パス＋フォールバック必須
6. **Git / PR / 提出衛生ルール**：1タスク1ブランチ、Conventional Commits、開発専用ファイル（`AGENTS.md`, `docs/`, `tests/`, `third_party/unity/`, `.agents/`, `.claude/` 等）を提出 zip へ混入させない
7. **ローカル検証 vs CI の役割分担（重複実行を避ける）**：
   - ローカルでは **変更範囲の最小限のビルド・関連テスト** と **CI で検証できない項目のみ**（WSLg GPU 描画・視覚確認、`flake.nix` 変更時の Nix 評価）を実行する。
   - フルの ASan/CTest・静的解析一式・提出 zip 検証は **GitHub Actions CI に委譲**し、PR 作成前後にローカルで毎回フル再実行しない（CI が落ちた場合や該当ツール自体を変更した場合のみローカル再現）。
8. **利用可能 Skill 一覧と発火条件**（下記 5.3 の 5 本へのポインタ）

### 5.3 プロジェクト専用 Skill 一覧（`.agents/skills/`）

1. **`inkstorm-verify-and-visual`**
   - **用途**: 変更範囲の最小ローカル検証・視覚セルフレビュー・CI 失敗時のローカル再現
   - **内容**: 変更モジュール単位のピンポイントビルド／テスト手順、`--snap` / `demo.mp4` からのコンタクトシート生成と画像目視チェック基準（5秒フック、和紙と敵の視認性、UI被り、シェーダフォールバック、`F1` X線表示）。フル検証コマンド（ASan/lint/提出 zip）は原則 CI 委譲とし、CI 失敗時や `flake.nix` / `tools/` 変更時のみローカル実行する。
2. **`inkstorm-fluid-sim`**
   - **用途**: `src/fluid/`・`src/game/elements/` 改修、新属性・地形境界の追加
   - **内容**: Stable Fluids の数値安定性検証（発散・クランプ・NaN/Inf 禁止・境界条件）、`fluid`（純物理）と `elements`（ゲーム反応）の責務境界チェック、160×90 性能予算（<2.0ms）、決定性ハッシュ確認手順。
3. **`inkstorm-content-authoring`**
   - **用途**: 新しい敵・ボス・アップグレード・遺物・アビリティ・ウェーブの追加
   - **内容**: 巨大 `switch` を避けた C11 定義テーブル＋フック関数＋専用ファイル追加のテンプレート、ECS コンポーネント付与規約、並列開発時のファイル競合回避ルール、単体テスト＋統計バランス確認手順。
4. **`inkstorm-pr-and-coderabbit`**
   - **用途**: ブランチ作成・PR 作成・CI 確認・CodeRabbit レビュー結果の取得とトリアージ対応
   - **内容**:
     1. PR 作成前：変更範囲の最小ローカル確認（＋描画変更時は視覚確認）のみで Conventional Commits → `gh pr create`（フル検証は CI に任せる）。
     2. `gh` CLI（`gh pr checks` / `gh pr view` / `gh api`）で GitHub Actions CI と CodeRabbit のレビュー結果を取得。
     3. **盲従禁止の4分類トリアージ**：
        - **Must Fix（即修正）**：実バグ、メモリ安全性/UB、モジュール依存違反、決定性破壊、提出 zip 混入リスク
        - **Should Fix（今回対応）**：スコープ内の明確なエッジケース保護・バグ予防・可読性改善
        - **Defer / Won't Fix（見送り）**：過剰抽象化、フェーズ外要望、授業課題規模に合わない最適化、誤検知（False Positive）
        - **Verify First（要検証）**：根拠が曖昧な指摘はコード/テストで裏取りしてから判定
     4. Must / Should のみ修正 → 修正箇所の最小確認後に push して CI で全体検証。見送り／誤検知項目は理由を簡潔に PR 上で返信・記録し、テストを弱める修正は禁止。
5. **`inkstorm-submission-demo`**
   - **用途**: マイルストーン録画・90秒デモ動画制作・提出 zip 最終検査
   - **内容**: 90秒デモ構成ガイド（序盤フック→操作説明→ビルド＆属性コンボ→ボス風合戦→終盤5〜7秒の `F1` X線オーバーレイ）、`README.txt` 記載監査、`tools/package_submission.py` によるクリーン環境ビルド確認。

---

## 6. これから相談して詳細化するセクション `[相談待ち]`

- [ ] **Section 6**: ゲーム仕様・ローグライク拡張設計（ラン構造、アップグレード＆ビルド軸、新敵・新ボス・新属性、操作体系・UI/タイトル、BGM/SEアセット計画）
- [ ] **Section 7**: Simulation Pipeline・Fluid Engine API・Hybrid ECS コンポーネント設計・イベント/コマンド設計
- [ ] **Section 8**: 品質保証・テスト・視覚回帰・90秒デモ動画構成
- [ ] **Section 9**: Phase 2 以降の AI エージェント並列タスク分割表

