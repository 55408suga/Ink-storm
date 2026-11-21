# INKSTORM（墨嵐）本開発 実装プラン

> **位置づけ**: 本ドキュメントは、INKSTORM（墨嵐）の本開発および環境構築を AI コーディングエージェントへ分割・委譲するためのマスタープラン（Single Source of Truth）である。
> 相談の進行に合わせて各セクションを確定（`[確定]`）へ更新し、確定したフェーズからタスクを切り出して実装する。
>
> ※ 本ファイルおよび `docs/`, `AGENTS.md`, `tests/`, `tools/`, `flake.nix` 等の開発支援ファイルは、スタンドアロン配布ソース zip（`dist/inkstorm-src.zip`）には含めない。

---

## 1. プロジェクト概要・基本方針 `[確定]`

- **概要**: 和紙キャンバス上で筆を操り、2次元非圧縮 Navier–Stokes 流体（Stable Fluids）と属性反応・ローグライク要素を融合させた C11 + raylib 6.2 水墨流体アクションゲーム。
- **リファレンス実装**: `../../prototypes/inkstorm/`（純 C11 + raylib 6.2 で動作確認済みのプロトタイプ）
- **配布・ビルド互換目標**:
  - ソース配布アーカイブ（`dist/inkstorm-src.zip`）は `src/`, `third_party/` の本体依存分, `assets/`, スタンドアロン `Makefile`, `README.txt` のみで構成し、Nix / CMake / Ninja / ネット接続なしで素の `gcc` + `make` + `raylib 6.2` だけでビルドできる状態を常時維持する。

---

## 2. アーキテクチャ方針 `[確定]`

### 2.1 基本原則
**「純 C11 + 独立 Fluid Engine + 超軽量 Hybrid ECS + ハイブリッドデータ駆動 + 厳密な一方向依存」**

1. **開発環境とスタンドアロン配布ビルドの2層分離**
   - **開発環境**: Determinate Nix (`flake.nix` + `.envrc`) ＋ CMake + Ninja + CTest + `clang-format` / `clang-tidy` / `cppcheck` + ASan/UBSan で AI 開発効率と静的検証を最大化する。
   - **配布アーカイブ**: `tools/package_dist.py`（または `cmake --build --target dist`）で、開発用ファイル・AI用doc・テストを自動除外し、**素の `Makefile` を同梱した自己完結 zip** を生成。さらに一時ディレクトリへ展開して Nix 抜きの素の `/usr/bin/gcc` + `/usr/bin/make` でビルド＆ヘッドレス動作を自動検証する。
2. **言語規格**: **C11**（`-std=c11 -Wall -Wextra -Wpedantic -Wshadow`、検証ビルドでは `-Werror -fsanitize=address,undefined`）。
3. **WSLg + Nix 共存方針**:
   - `flake.nix` は `pkgs.mkShellNoCC` を使用し、Nix から `cmake`, `ninja`, `clang-tools`（`clang-format` / `clang-tidy`）, `cppcheck`, `glslang`, `gdb`, `ccache`, `zip`, `unzip`, `shellcheck` 等の **開発ツールのみ供給** する。
   - コンパイラと raylib はホストの `/usr/bin/gcc` ＋ `${RAYLIB_DIR:-$HOME/.local/opt/raylib-6.2}` をそのまま使用することで、WSLg の GPU（`GALLIUM_DRIVER=d3d12`）および glibc ABI の不整合を回避し、標準 Linux 環境と同一のリンク挙動を維持する。
4. **決定性とヘッドレス検証**:
   - `core`, `fluid`, `ecs`, `game`, `audio` は **raylib を一切 include しない**。
   - 固定タイムステップ（60Hz）＋明示的 RNG（SplitMix64）により、同一シード＋同一入力で完全再現可能にする。
5. **外部アセット＋安全フォールバック**:
   - BGM / SE / フォント / テクスチャ等は `assets/` に配置する。
   - 実行ファイル基準のパス解決を行い、読み込み失敗時でも組み込みのプロシージャル音やデフォルトフォントへ安全にフォールバックし、クラッシュさせない。

### 2.2 独立 Fluid Simulation Engine (`src/fluid/`)
- 160×90 Stable Fluids（Semi-Lagrangian 移流＋Gauss-Seidel 圧力投影＋Vorticity Confinement）の数値計算ロジックを独立モジュール化する。
- **責務分離**:
  - `fluid` の責務：速度場 `(u, v)`、圧力投影、渦度閉じ込め、境界条件、汎用スカラー場（複数チャンネル：Ink / Fire / Storm / 拡張チャンネル）の移流・減衰・注入・バイリニアサンプリング、計測統計。
  - `game/elements` の責務：墨×炎の着火（Blazing Inkstorm）、墨×雷の伝導（Tempest Lightning）、新属性の反応ルール（`fluid` 内部にゲームルールを直書きしない）。
- Cellular Automata は初期導入しない（残墨や焦げ表現が必要になった場合は低減衰スカラー場で表現する）。

### 2.3 超軽量 Hybrid ECS (`src/ecs/`)
外部巨大フレームワークは採用せず、**300〜500行程度の純 C11・依存ゼロの超軽量 Hybrid ECS** を自作する。

- **採用対象（ECS で管理するもの）**:
  - 敵、弾（矢・火球・雷球等）、召喚物、設置ギミック／トラップ、回収物（墨玉・遺物ドロップ等）、一部持続エフェクトエンティティ。
- **非対象（ECS 外の独立モジュールで管理するもの）**:
  - Fluid Solver（セルを Entity にしない）、Game Loop / Scene Manager、Run Progression / Upgrade State、Audio、大量の軽量パーティクル（固定リングバッファ）。
- **初期内部設計**:
  - **世代付き Entity ID**: `typedef struct { uint32_t index; uint32_t gen; } EntityId;`
  - **固定長 Component 配列（SoA 的レイアウト）**: `ECS_MAX_ENTITIES`（初期案 512〜1024）に対し Component 種別ごとの連続固定配列
  - **64-bit コンポーネントビットマスク**: `uint64_t comp_mask[ECS_MAX_ENTITIES]`
  - **遅延削除（Deferred Destroy）**: `ecs_destroy()` は削除フラグのみ立て、各ステップの確定フェーズ（End-of-Tick）で一括回収＋`gen++`

### 2.4 ハイブリッドデータ定義（敵・アップグレード・アビリティ・ウェーブ）
- **基本（C11 構造体テーブル）**: 指示付き初期化子による `const EnemyDef[]`, `const UpgradeDef[]`, `const AbilityDef[]` と、挙動・効果フック用の関数ポインタテーブルを採用する。
- **拡張（外部パラメータオーバーライド余地）**: バランス調整をリコンパイルなしで回したくなった段階で、数値フィールドだけ外部データで上書きできる構造を許容する。

---

## 3. ディレクトリ構成とモジュール境界 `[確定]`

### 3.1 リポジトリ構成
```text
Ink-storm/
├── flake.nix                 # Nix 開発環境 (mkShellNoCC)
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
│       └── ci.yml            # GitHub Actions CI (ASan/UBSan・静的解析・配布zip素make検証)
├── .agents/
│   └── skills/               # プロジェクト専用 AI スキル本体（配布除外）
├── .claude/
│   └── skills -> ../.agents/skills  # Claude Code 互換シンボリックリンク（配布除外）
├── AGENTS.md                 # AI エージェント開発規約（配布除外）
├── README.txt                # ビルド方法・操作方法
├── docs/
│   ├── PLAN.md               # 本ファイル（全体計画・要件・タスク分割）
│   └── tasks/                # フェーズ別タスク指示書
├── src/
│   ├── core/                 # ターゲット: ink_core
│   ├── fluid/                # ターゲット: ink_fluid
│   ├── ecs/                  # ターゲット: ink_ecs
│   ├── game/                 # ターゲット: ink_game (sim / brush / elements / combat / enemies / roguelike)
│   ├── audio/                # ターゲット: ink_audio
│   ├── render/               # ターゲット: ink_render
│   └── app/                  # ターゲット: inkstorm (main.c / シーン / 入力 / 録画 / アセット管理)
├── assets/                   # BGM / SE / フォント / テクスチャ
├── third_party/
│   └── unity/                # C単体テスト用 Unity v2.6.0（tests のみ使用・配布除外）
├── tests/                    # 単体・流体数値・ECS・決定性回帰・ベンチマーク（配布除外）
└── tools/                    # package_dist.py / dist.mk / check_shaders.py / extract_snaps.sh（配布除外）
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
| `ink_audio` | プロシージャル和楽器/風音シンセ、WAV書き出し、BGM/SEトリガ定義 | `ink_core`, `ink_game`（イベント参照） | **分離** |
| `ink_render` | 和紙ポストシェーダ、流体可視化、妖怪・筆・UI・VFX・`F1` X線オーバーレイ | `ink_core`, `ink_fluid`, `ink_ecs`, `ink_game`（すべて `const` 参照）, `raylib` | 可 |
| `app` (`inkstorm`) | `main.c`、ウィンドウ・入力収集、アセットロード、シーン遷移、録画/デモ/CLI | 全モジュール, `raylib` | 可 |

---

## 4. Phase 1：開発基盤・CI・CodeRabbit・最小スモーク構築 `[完了]`

Phase 1（開発基盤・Nix・CMake/Ninja/CTest・Unity v2.6.0・最小スモーク・配布 zip 自動検証・GitHub Actions CI・CodeRabbit Schema v2・`AGENTS.md`・5本の Skill）は実装・検証済み。詳細は [docs/tasks/phase1-env-setup.md](tasks/phase1-env-setup.md) を参照。

---

## 5. `AGENTS.md`・プロジェクト専用 Skill・エージェント運用仕様 `[確定]`

### 5.1 エージェント起動ディレクトリと並列運用ルール
- AI エージェント（Claude Code / Codex / Antigravity）は `full-scale-dev/Ink-storm/`（リポジトリルート）で起動する。
- 並列開発時は `full-scale-dev/` 配下に兄弟 worktree（例：`full-scale-dev/wt-fluid`, `full-scale-dev/wt-enemies`）を作成し、各エージェントをそれぞれの worktree ルートで起動する。
- ルール本体は `AGENTS.md` に一本化。Skill 本体は `.agents/skills/<name>/SKILL.md` に置き、`.claude/skills -> ../.agents/skills` の相対シンボリックリンクで Claude Code 互換を確保する。

### 5.2 ローカル検証 vs CI の役割分担
- **ローカル（最小・高速）**：変更モジュールのピンポイント確認と、CI で代替できない項目のみ（WSLg 実機 GPU 描画・視覚確認、`flake.nix` 変更時の Nix 評価）。
- **CI 委譲**：フルの ASan/CTest・静的解析一式・配布 zip 検証は GitHub Actions CI に委譲し、PR 前後にローカルで重複実行しない（CI 失敗時や該当ツール変更時のみローカル再現）。

### 5.3 プロジェクト専用 Skill 一覧（`.agents/skills/`）
1. **`inkstorm-verify-and-visual`**：変更範囲の最小ローカル検証・視覚セルフレビュー・CI 失敗時のローカル再現
2. **`inkstorm-fluid-sim`**：`src/fluid/`・`src/game/elements/` 改修時の安定性・決定性・性能検証
3. **`inkstorm-content-authoring`**：敵・ボス・アップグレード・遺物・アビリティ・ウェーブのデータ駆動追加
4. **`inkstorm-pr-and-coderabbit`**：PR 作成・CI 確認・CodeRabbit レビュー取得（必要時 `@coderabbitai review` 手動トリガ）と 4 分類トリアージ対応
5. **`inkstorm-demo-and-dist`**：デモ録画・コンタクトシート目視・スタンドアロン配布ソース zip と `README.txt` 確認

---

## 6. これから相談して詳細化するセクション `[相談待ち]`

- [ ] **Section 6**: ゲーム仕様・ローグライク拡張設計（ラン構造、アップグレード＆ビルド軸、新敵・新ボス・新属性、操作体系・UI/タイトル、BGM/SEアセット計画）
- [ ] **Section 7**: Simulation Pipeline・Fluid Engine API・Hybrid ECS コンポーネント設計・イベント/コマンド設計
- [ ] **Section 8**: 品質保証・テスト・視覚回帰・デモ録画構成
- [ ] **Section 9**: Phase 2 以降の AI エージェント並列タスク分割表
