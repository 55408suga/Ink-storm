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
│   ├── PLAN.md               # 本ファイル（全体方針・モジュール境界・確定状況・タスク分割のハブ）
│   ├── game-design.md        # ゲーム仕様書（ラン構造・3幕・属性反応・筆スタイル・敵/ボス・ビルド・UI/アセット）
│   ├── art-direction.md      # 世界観・アートディレクション仕様書（6本柱・パレット・3幕情景・筆プリミティブ・NG集）
│   ├── architecture.md       # 技術設計書（Simulation Pipeline・Fluid API・Hybrid ECS・データスキーマ・セーブ）
│   ├── qa-and-testing.md     # 品質保証・テスト・視覚回帰・デモ録画計画
│   └── tasks/                # フェーズ・worktree 別タスク指示書
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

## 6. ゲーム仕様・ローグライト拡張設計 `[確定]`

詳細仕様は **[docs/game-design.md](game-design.md)** へ分離して管理する。確定済みサマリ：

- **ラン構造**: 全 3 幕 ×（通常 3 ウェーブ＋幕ボス 1）の幕×ウェーブ防衛型＋第 3 幕クリア後エンドレス解放。
- **空間・流体連動**: 中央の社 1 基固定を基本としつつ、幕ごとに流体 `solid` 境界障害物・環境ギミックが変化（社位置・複数化の拡張余白を維持）。
- **属性反応**: 3 属性深化（墨・炎・雷）— `墨×炎`（紅蓮墨嵐）、`墨×雷`（雷墨連鎖）、`炎×雷`（爆雷インパルス）、`墨×炎×雷`（三元共鳴）＋ビルドによる能動炎/雷注入。
- **操作・筆スタイル**: 3 操作統一（左ドラッグ 風筆・右クリック 渦印・Space 風斬り）＋初期解放の筆スタイル 3 種（豪筆 / 朱筆 / 細筆）。
- **報酬＆メタ進行**: ラン中は 2 層報酬（通常後＝強化 3 択、ボス後＝大遺物 3 択＋社回復、ショップなし）。ラン外ロビーにメタショップ（墨庵：小幅恒久強化＋追加遺物解放）を配置し、`RunConfig` 経由で決定性テスト/デモと分離。
- **コンテンツ規模**: 通常敵 6〜7 種＋エリート変異＋固有幕ボス 3 体／4 ビルド軸（紅蓮・雷霆・烈風・墨道）＋通常アップグレード 24〜30 種＋大遺物 12〜15 種。
- **幕ボス**: 大天狗（第 1 幕）／鵺（第 2 幕）／一筆龍（第 3 幕・霊峰の龍脈、多部位ボス、簡易版で代替可）。
- **実装優先度**: P0＝第 1 幕の縦切り → P1＝3 幕本編 → P2＝任意枠・エンドレス・多部位版（詳細は game-design.md Section 12）。
- **世界観・UI・アセット**: 日本語メイン UI（和風フリーフォント TTF 同梱＋ASCII フォールバック）、古典水墨様式に基づく余白と筆致・紙抜き縁・6 種筆プリミティブ（詳細は **[docs/art-direction.md](art-direction.md)**）、和楽器シンセ SE 拡張、外部ループ BGM（＋フォールバック）。

---

## 7. 技術アーキテクチャ設計 `[確定]`

詳細仕様は **[docs/architecture.md](architecture.md)** へ分離して管理する。確定済みサマリ：

- **責務境界**: `ink_game` が戦闘ラン（`GameState`）とメタ進行/墨庵/設定/ヒント（`MetaState`）を全集約し、`app` はウィンドウ・入力・UI 描画・ファイル I/O のみに徹する。見た目 VFX は `ink_render` が毎ティック直後に `events[]` から生成（専用 `vfx_rng` 付き、複数ティック時のイベント取りこぼし防止契約あり）。GLSL シェーダは `src/render/*.c` 埋め込みを標準とする。
- **メモリ・性能予算**: 約 1.3〜1.5 MB のフラット固定長構造体（160×90・ECS 512）をトップレベルで 1 回だけヒープ確保（ラン中 `malloc` ゼロ）。Release 性能予算は `fluid_step < 2.0 ms/tick`、`game_step < 4.0 ms/tick`。
- **Fluid Engine**: 汎用固定 4ch（3ch 使用＋1ch 余白）＋`solid[GN]` no-slip 境界＆法線クエリ＋薄い柱の壁横断を防ぐ短歩幅レイマーチ＋既定 12 反復＋`fluid_add_radial_impulse`（次ティック注入）＋`fluid_step()` → `elements_step()` → `fluid_sanitize_channels()` の直列連携。
- **Hybrid ECS**: インデックスレジストリ型（`ink_ecs`＝世代 ID・64bit マスク・遅延削除・削除予約中アクセス拒否・昇順走査、`ink_game`＝型付き Component 固定配列＋検証付きアクセサ）。
- **パイプライン・効果＆コマンド**: 戦闘更新の後に遅延削除フラッシュ＆遷移優先判定（敗北＞ボス撃破＞通常ノルマ）を配置。数値系は `RunModifiers` 集約＋ルール変更系は 7 限定フック。イベントは `GameEvent[128]`、UI 操作は値型 `GameCommand`（選択・リロール・社修繕・エンドレス代替・中断）へ統一し、候補不足時の代替規則・無効コマンド no-op 契約を明文化。
- **セーブ仕様**: 依存ゼロの `key=value` テキスト（`version=1`・設定/ヒント統合・飽和加算・`run_settled` 二重精算防止・破損時フォールバック・原子的 `rename` 保存）、XDG 標準保存先（`--no-save` / `--save-path` 対応）。

---

## 8. 品質保証・テスト・視覚回帰計画 `[確定]`

詳細基準は **[docs/qa-and-testing.md](qa-and-testing.md)** へ分離して管理する。確定済みサマリ：

- **検証分担**: ローカルは変更範囲の最小ビルド＆関連テスト＋WSLg GPU 視覚確認・Release ベンチマーク・Nix 評価。フル ASan/UBSan + CTest・静的解析・配布 zip 検証は GitHub Actions CI へ委譲。
- **自動テスト**: `test_core_smoke` / `test_cli_headless_smoke`（Phase 3 で本番 `game_step` 化）に加え、`test_fluid`（壁横断/角含む）・`test_ecs`（満杯/削除予約含む）・`test_roguelike_meta`（飽和加算/二重精算防止/候補不足含む）・`test_app_save`（原子的保存失敗時）・`test_elements`・`test_determinism_run`（初回差分ティック出力・複数ティックイベント消費）・`test_audio` を段階導入。
- **性能・視覚・CLI**: Release ベンチ（`fluid_step < 2.0 ms/tick`, `game_step < 4.0 ms/tick`）、CLI 終了コード/セーブ隔離契約、4 描画経路（通常/`--no-post`/シェーダ失敗/フォント欠損）とコンタクトシート目視確認を個別ゲートで管理。

---

## 9. Phase 2 以降の AI エージェント並列タスク分割表 `[確定]`

[game-design.md](game-design.md) §12 の実装優先度（**P0 縦切り → P1 本編 → P2 拡張**）と完全に整合させ、共有ファイルの競合を避けるため **「先行契約＆統合担当 → 並列 worktree → 合流ゲート」** の順で進める。

### 9.1 フェーズ別タスク分割と完了ゲート

| Phase（対応優先度） | ゴール | 並列 worktree / タスク | 主な担当範囲 | 完了ゲート |
| :--- | :--- | :--- | :--- | :--- |
| **Phase 2**<br>コアエンジン基盤 | 流体・ECS・メタの独立完成（2 並列＋統合） | **P2-A** (`wt-fluid`)<br>**P2-B** (`wt-ecs-meta`)<br>**P2-Int**（統合） | **A**: `src/fluid/*`, `tests/test_fluid.c`, 流体ベンチ<br>**B**: `src/ecs/*`, `src/game/meta.*`, `tests/test_ecs.c`, `tests/test_roguelike_meta.c`（メタ部）<br>**Int**: `game.c` / `main.c`（ヒープ確保化）/ `test_core_smoke.c` / `CMakeLists.txt` の新契約同期 | CI 全緑＋`fluid_step < 2.0 ms/tick`（Release） |
| **Phase 3**<br>P0 縦切り（第 1 幕プレイアブル）<br>**`[完了]`** | 第 1 幕通し（3 ウェーブ＋大天狗＋強化札 8 種＋報酬画面＋リザルト）を実機完走（契約確定→2 並列個別 PR→結合） | **[P3-Pre](tasks/phase3-pre-contracts.md)**（契約確定・完了）<br>**[P3-A](tasks/phase3-sim-p0.md)** (`wt-sim-p0`・完了)<br>**[P3-B](tasks/phase3-render-audio-p0.md)** (`wt-render-audio-p0`・完了)<br>**[P3-Int](tasks/phase3-integration.md)**（結合・ゲート・完了） | **Pre**: `game.h` / `GameState` / `GameEvent` / `GameCommand` / `UiView` 枠・テスト登録・骨格・設計書同期を `main` に確定（1 PR）<br>**A**: 60Hz パイプライン、3 技、豪筆、墨経済・連殺・墨昇華、紅蓮/雷墨、弓妖・炎提灯・唐傘・大天狗、強化札 8 種（各軸 2 種）、報酬/リロール/修繕ロジック、`test_elements` / `test_determinism_run` / `bench_game`（単独 PR → マージ）<br>**B**: 和紙シェーダ、流体描画、rlgl 筆描画プリミティブ（`ink_stroke`/`ink_knockout` 等）、VFX プール、日本語文字列表＆本文用明朝体 1 書体（OFL 1.1・未改変を `assets/fonts/` に同梱し `assets/LICENSES.txt` へ記録）/ASCII フォールバック、報酬/リザルト UI、和楽器 SE＋プロシージャル BGM、セーブ I/O（`test_app_save`）、`test_audio`、`test_text_table`、本番 `test_cli_headless_smoke`（単独 PR → マージ、2a と順不同）<br>**Int**: 2a・2b マージ後の `main` から結合修正・難易度確認・実機完走・Phase 3 Gate（1 PR） | **達成済み（Phase 4 `P4-Pre` へ進行可）**: 実ウィンドウで第 1 幕（戦闘→3 択→大天狗→勝利リザルト／敗北／途中終了・セーブ隔離）完走＋`test_determinism_run` 緑＋`fluid_step < 2.0` / `game_step < 4.0 ms/tick`＋`F1` 描画予算達成＋WSLg 4 経路・アセット欠損起動確認＋CI 全緑 |
| **Phase 4**<br>P1 本編完成<br>**`[完了]`** | 全 3 幕・筆 3 種・爆雷/三元共鳴・強化札 24 種・大遺物 9 種・ロビー墨庵・BGM 完成（契約確定→最大 3 並列個別 PR→結合） | **[P4-Pre](tasks/phase4-pre-contracts.md)**（契約確定・完了）<br>**[P4-A](tasks/phase4-roguelike-p1.md)** (`wt-roguelike-p1`・完了)<br>**[P4-B](tasks/phase4-acts-enemies-p1.md)** (`wt-acts-enemies-p1`・完了)<br>**[P4-C](tasks/phase4-ui-assets-p1.md)** (`wt-ui-assets-p1`・完了)<br>**[P4-Int](tasks/phase4-integration.md)**（結合・ゲート・完了） | **Pre**: 追加 ID・イベント・`RunProgress.relics`・`RunModifiers`/`GameInternal`・`UiView` 拡張枠・`test_acts_enemies` 登録・骨格と先行配線を確定<br>**A**: 朱筆・細筆、爆雷・三元共鳴（`elements` 拡張）、強化札 24 種・初期大遺物 9 種、`RunModifiers`＋7 フック拡充、初回ヒント発火<br>**B**: 第 2 幕（鳥居/岩柱 `solid`・鵺）、第 3 幕（山颪・霊柱・一筆龍簡易版）、雷提灯・鎌鼬・石鬼・墨吸い・エリート変異 4 種<br>**C**: タイトル/ロビー・筆選択・墨庵ショップ・設定・ポーズ（`Esc` ポーズ化に伴う `AGENTS.md`/`README.txt` の終了操作表記更新含む）・初回操作ヒント UI、第 2/3 幕情景と新敵/ボス描画、題字用の毛筆体フォント・外部 BGM・`assets/LICENSES.txt` 更新<br>**Int**: 3 並列マージ後の結合修正・全 3 幕＆墨庵通し完走・Phase 4 Gate（1 PR） | **達成済み（Phase 5 へ進行可）**: 実ウィンドウでロビー→筆選択→全 3 幕（大天狗・鵺・一筆龍）クリア→墨庵購入＆セーブ→再起動・別筆・ポーズ中断精算を完走＋全 15 CTest（ASan/UBSan `-Werror`）緑＋Release 性能予算達成（`fluid_step 1.331 / p95 1.816 ms < 2.0`、`game_step 1.495 / p95 2.060 ms < 4.0`、一筆龍戦 `F1` `render_draw 1.540 ms/frame < 4.0`・頂点 `17,571 <= 60,000`・ブレンド切替 `4 <= 12`）＋WSLg 4 描画経路（`shader,font` 併用含む）・アセット欠損起動・崩し試験 4 種 OK＋CI 3 ジョブ全緑 |
| **Phase 5**<br>P2 拡張（本編拡張）<br>**`[完了]`** | 全カタログ・エンドレス・多部位ボス・設定拡充（契約確定→3 並列個別 PR→結合） | **[P5-Pre](tasks/phase5-pre-contracts.md)**（契約確定・完了）<br>**[P5-A](tasks/phase5-roguelike-p2.md)** (`wt-roguelike-p2`・完了)<br>**[P5-B](tasks/phase5-endless-ryu-p2.md)** (`wt-endless-ryu-p2`・完了)<br>**[P5-C](tasks/phase5-ui-settings-p2.md)** (`wt-ui-settings-p2`・完了)<br>**[P5-Int](tasks/phase5-integration.md)**（結合・ゲート・完了） | **Pre**: 追加 ID・`EV_BOSS_PART_BREAK`/`EVF_REWARD_CRYSTAL`・`MetaState`/`SettingsState` 拡張・`RyuBodyState`・クロス所有先行配線・骨格と設計書同期を `main` に確定（1 PR）<br>**A**: 残り強化札 4 種（全 28 種）・墨庵解放の大遺物 6 種（全 15 種・解放マスク制御）・候補枯渇 2 択フォールバック（修繕＋墨晶変換）<br>**B**: 狐符＋護符結界＋追尾符、エンドレスモード（第 4 幕以降周回スケール・地形/ボス循環）、一筆龍の多部位 ECS 版（3 龍玉＋頭部軌跡リングバッファ＋3 フェーズ）<br>**C**: 設定画面拡充（言語・閃光軽減・カーソル倍率・ヒント初期化・設定初期化）、エンドレス UI（遅延勝利精算・勝利 3 ボタン・金砂子・第 4 幕以降 HUD）、狐符・結界・追尾符・多部位一筆龍の水墨描画と SE・見本帳拡充・`README.txt` 更新<br>**Int**: 3 並列マージ後の結合修正（多部位一筆龍フェーズ遷移時の予兆下限保証）・実機通し完走・Phase 5 Gate（1 PR） | **達成済み（Phase 6 へ進行可）**: 全 28 札/15 遺物（墨庵解放 6 種含む）・狐符＆追尾符・多部位一筆龍 3 フェーズ・エンドレスモード（遅延精算・周回スケール・金砂子・候補枯渇 2 択）・拡充設定を隔離セーブ＆実ウィンドウで完走＋ASan/UBSan 対象 CTest 全緑＋Release 性能予算達成（`fluid_step 1.163 / p95 2.000 ms < 2.0`、`game_step` Act 3 多部位一筆龍 `1.311 / p95 2.186 ms < 4.0`、Act 6 エンドレス龍＋狐符 `1.302 / p95 2.334 ms < 4.0`、多部位一筆龍戦 `F1` `render_draw 1.065 / p95 1.647 ms/frame < 4.0`・頂点 `18,309 <= 60,000`・ブレンド切替 `2 <= 12`）＋WSLg 4 描画経路（`shader,font` 併用含む）・アセット欠損起動・崩し試験 4 種 OK＋CI 3 ジョブ全緑 |
| **Phase 6**<br>仕上げ・デモ・配布検証 | 自動デモ・視覚回帰・性能調整・最終配布（順次合流） | **P6-A** (`wt-demo-polish`)<br>**P6-B**（合流後・同一コミット） | **A**: `--demo` / `--snap`、`F1` X 線仕上げ、難易度・数値バランス・性能最終調整<br>**B**: P6-A の `main` 合流後、**同一コミット**で `README.txt`・`assets/LICENSES.txt`・`dist/inkstorm-src.zip`（素 `make` + `--headless-smoke`）検証およびデモ動画収録＆コンタクトシート目視 | 性能予算達成（`< 2.0` / `< 4.0 ms/tick`）＋コンタクトシート目視 OK＋配布 zip 合格 |

### 9.2 共有ファイル所有・合流ルール
- **共有ファイル（`CMakeLists.txt`, `src/fluid/fluid.h`, `src/ecs/ecs.h`, `src/game/game.h`, `src/game/meta.h`, `src/render/render.h`, `src/app/main.c`）**:
  - 各 Phase の着手前に **先行契約（`*-Pre`）** として型・enum・関数シグネチャの枠・テスト登録・骨格実装を `main` に確定（1 本の PR）してから並列 worktree を切る。
  - Phase 3 以降は、`*-Pre` の骨格により各並列 worktree（例：`feat/phase3-sim-p0` / `feat/phase3-render-audio-p0`）が単独でビルド・CI 全緑・PR 可能とし、完了した順に個別 PR で `main` へマージする（PR 前に `git merge main` で最新の `main` を取り込む）。両者のマージ後に統合担当（`*-Int`）が結合修正・実機完走・完了ゲートを 1 本の PR で確定させる。
  - Phase 2 のように既存 API シグネチャ自体が変わる場合（例：`fluid_init`、`GameState` のヒープ確保化）は、呼び出し側（`game.c`, `main.c`, `test_core_smoke.c`）と `CMakeLists.txt` の同期を **統合担当（`*-Int`）** が一括実施する。
  - 並列 worktree は原則として独立した `.c` ファイル・サブディレクトリ（例：`src/game/roguelike/`, `src/game/enemies/`, `src/render/ui_*.c`）を編集する。
- **タスク指示書**: 各タスク着手前に `docs/tasks/phaseX-*.md` を切り出し、変更許可ファイル・完了条件・テスト対象を明示してからエージェントへ委譲する。
