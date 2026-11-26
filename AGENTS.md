# INKSTORM 開発規約

INKSTORM（墨嵐）は C11 と raylib 6.2 で構築する 2D 水墨流体アクションゲーム。
開発計画・フェーズ進行・モジュール境界は [docs/PLAN.md](docs/PLAN.md) を起点とし、詳細は [docs/game-design.md](docs/game-design.md)（ゲーム仕様）・[docs/art-direction.md](docs/art-direction.md)（世界観・水墨表現）・[docs/architecture.md](docs/architecture.md)（技術契約）・[docs/qa-and-testing.md](docs/qa-and-testing.md)（テスト・検証基準）を参照する。
フェーズごとの実装範囲と完了条件は [docs/tasks/](docs/tasks/) の指示書で確認する。

## 作業場所と担当範囲

- Codex、Claude Code、Antigravity は `full-scale-dev/Ink-storm/` を起点にする。
- 兄弟 worktree では、各エージェントを割り当てられた worktree のルートから起動する。
- `full-scale-dev/` から起動すると Git、direnv、プロジェクトの Skill を検出できないことがある。
- 共通ルールはルートの `AGENTS.md` に集約し、Claude Code、Codex、Antigravity、CodeRabbit で参照する。
- 対応版の Claude Code は、既定設定でプロジェクトに `CLAUDE.md` がなければ `AGENTS.md` を読む。
- Skill の実体は `.agents/skills/` にあり、`.claude/skills` は `../.agents/skills` への相対リンク。
- プロトタイプは参照資料として扱い、割り当てのフェーズで指定された範囲だけを移す。

## 標準コマンド

`nix develop` は開発ツールを提供する。CMake のプリセットはホストの `/usr/bin/gcc` を使う。
raylib の既定値は `$HOME/.local/opt/raylib-6.2`。
別の配置先は `RAYLIB_DIR` 環境変数か、CMake の `-DRAYLIB_DIR=/path/to/raylib` で指定する。
`/dev/dxg` がある環境では Nix シェルが未設定の `GALLIUM_DRIVER` に `d3d12` を設定する。

| 作業 | リポジトリルートで実行 |
| --- | --- |
| ASan / UBSan | `nix develop -c cmake --preset asan` → `nix develop -c cmake --build --preset asan` |
| CTest | `nix develop -c ctest --preset asan --output-on-failure` |
| Release | `nix develop -c cmake --preset release` → `nix develop -c cmake --build --preset release` |
| Debug | `nix develop -c cmake --preset debug` → `nix develop -c cmake --build --preset debug` |
| 整形検査 | `nix develop -c cmake --build --preset asan --target format-check` |
| 静的解析 | `nix develop -c cmake --build --preset asan --target lint` |
| 配布 zip | `nix develop -c python3 tools/package_dist.py` または `nix develop -c cmake --build --preset release --target dist` |
| オフライン QA 録画 | `nix develop -c python3 tools/qa_run.py --all`（要 `ffmpeg` の `libx264` / `aac`、`--no-video` は `ffmpeg` 不要。`build/release/qa_record` を使用し、リポジトリ外へ MP4・スナップ・JSON を出力） |

```sh
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan &&
    nix develop -c ctest --preset asan --output-on-failure

nix develop -c cmake --preset release && nix develop -c cmake --build --preset release
./build/release/inkstorm --headless-smoke
nix develop -c ./build/release/inkstorm --smoke-frames 5
nix develop -c ./build/release/inkstorm --no-post --smoke-frames 5
```

通常のウィンドウ確認は `nix develop -c ./build/release/inkstorm` で起動する（タイトル/ロビー画面では ESC・「終了」または閉じるボタンで終了、ラン中は ESC でポーズメニューを開き「ロビーへ戻る」または閉じるボタンで終了する）。
`format-check` は `src/` と `tests/` を対象とする。`lint` は clang-tidy、cppcheck、GLSL 検証、ShellCheck を実行する。
`build/<preset>/compile_commands.json` はそのプリセットのコンパイル条件を静的解析やエディタに渡す。
ビルドは `build/`、配布アーカイブは `dist/` に出力する。

## モジュール境界

| ターゲット | 直接依存できる先 |
| --- | --- |
| `ink_core` | C 標準ライブラリと `m` |
| `ink_fluid` | `ink_core` |
| `ink_ecs` | `ink_core` |
| `ink_game` | `ink_core`、`ink_fluid`、`ink_ecs` |
| `ink_audio` | `ink_core`、`ink_game` の読み取り専用イベント |
| `ink_render` | `ink_core`、`ink_fluid`、`ink_ecs`、`ink_game`、raylib |
| `inkstorm` | 上の全モジュール、raylib、ホストのプラットフォームライブラリ |

- `core`、`fluid`、`ecs`、`game`、`audio` では `raylib.h` を include しない。
- `raylib` のヘッダ探索先は `ink_render`、`inkstorm`、および開発専用オフスクリーン録画ツール `qa_record`（`tests/`・配布除外）の `PRIVATE` に限定する。
- `src/` 一括の include パスや相対パスによる他モジュールのヘッダ参照で依存制限を迂回しない。
- `fluid` は汎用の数値計算を担う。ダメージや属性反応などのゲームルールは `game` に置く。
- `render` に渡すゲーム状態は `const GameState *` とし、描画側から変更しない。
- ウィンドウ、入力、raylib の音声デバイスへの接続は `app` に置く。

## C11、メモリ、決定性

- `-std=c11` でビルドする。GNU 言語拡張を前提にしない。
- プロジェクトの警告は `-Wall -Wextra -Wpedantic -Wshadow`。検証時は `-Werror` を追加する。
- 固定長領域を優先し、`malloc` は必要な場所に絞る。確保・解放する担当と借用の寿命を明示する。
- 状態は明示的に初期化し、可変グローバル状態や未初期化値への依存を避ける。
- ゲームの更新は固定 60 Hz。描画や実時間の変動でシミュレーションの刻み幅を変えない。
- `rand()` と `time()` でゲーム状態を変えない。状態を受け取る SplitMix64 を使う。
- 同じ初期状態・シード・入力列で同じ状態遷移を再現する。

## ECS、流体、アセット

- ECS の Entity ID に含まれる index と世代を検証してからコンポーネントを読む。
- 走査中の破棄は予約し、ティックの確定処理で回収して世代を進める。
- 流体セルは格子上に保持し、セルを Entity に変換しない。
- 流体の境界と添字を検証し、値の発散や NaN・Inf はテストで検出する。
- アセットは実行ファイルを基準に `assets/` から解決する。起動時の作業ディレクトリに依存させない。
- アセットやシェーダを利用できないときは組み込みのアセットや通常描画へ切り替える。

## Git、PR、配布パッケージ

- 原則として 1 タスク 1 ブランチ。コミットがタスクに含まれる場合は Conventional Commits を使う。
- 並列作業は `full-scale-dev/wt-fluid` などの兄弟 worktree に分け、担当ファイルと統合担当を共有する。
- 共有の定義表、登録箇所、CMake 設定を並列編集するときは、先に担当を決める。
- 新規の flake ファイルを Nix が認識するために必要な場合は、対象ファイルを `git add -N` で登録できる。
- Git フックの検査を迂回しない。スタンドアロン配布ビルドは `make` と `gcc` で完結させる。
- PR 前のローカル検証は変更範囲の最小ビルド・関連テストと、CI で代替できない項目（WSLg GPU 描画・視覚確認、`flake.nix` 変更時の Nix 評価）に留める。
- フルの ASan / UBSan + CTest、整形、静的解析、配布 zip 検証（`dist`）は GitHub Actions CI に委譲し、ローカルで毎回重複実行しない（CI 失敗時や該当ツール変更時のみローカルで再現する）。
- push、PR 作成、レビュー返信はユーザーから許可された範囲で行う。
- PR 作成直後は `@coderabbitai review` で CodeRabbit レビューを起動し、CI・コードレビュー・CodeRabbit の指摘確認と必要な修正を完了してからマージする（詳細手順は [inkstorm-pr-and-coderabbit](.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)）。
- 配布ソース zip は `tools/package_dist.py` で生成する。ルートにはスタンドアロン用 `Makefile` と `README.txt` を置く。
- `AGENTS.md`、`docs/`、`tests/`、`tools/`、`third_party/unity/`、`.agents/`、`.claude/` は開発専用。
- AI 向け文書、CI 設定、CMake、Nix、テスト、録画やビルド成果物を配布 zip に混入させない。
- 配布構成に影響する変更では、CI（または必要時のローカル `dist`）で展開先の `make` と `./inkstorm --headless-smoke` が通ることを確認する。

## タスク完了前

- [ ] 差分と仕様を照合し、他の担当範囲への変更や一時的な検証コードが入っていない。
- [ ] 変更範囲の最小ビルド・関連テストを通した（フル ASan/CTest・`format-check`・`lint`・`dist` は CI 委譲、失敗時やツール変更時のみローカル実行）。
- [ ] 見た目（描画・キャラ/ボス・背景・シェーダ・VFX・UI）や音（BGM・筆音・SE）を変更した場合は `tools/qa_run.py`（必要時 `--no-post`・`--no-bgm`・`--showcase --snap` 併用）で該当シナリオをリポジトリ外へ録画し、自動合否と `contact_sheet.png`・音付き MP4 を直接確認した。
- [ ] `flake.nix` を変更した場合はローカルで `nix develop` 評価を確認した。
- [ ] 実行した確認の結果と、CI に委譲した項目・環境上の制約を報告する。

## プロジェクト専用 Skill

- 見た目・音の変更時のオフライン音付き動画 QA（`tools/qa_run.py`）、ビルド、静的解析、シェーダ検証、視覚・聴覚レビューに使用する。[inkstorm-verify-and-visual](.agents/skills/inkstorm-verify-and-visual/SKILL.md)
- 流体、属性反応、格子境界、決定性や性能予算の変更に使用する。[inkstorm-fluid-sim](.agents/skills/inkstorm-fluid-sim/SKILL.md)
- 敵、ボス、遺物、アップグレード、アビリティ、ウェーブの追加に使用する。[inkstorm-content-authoring](.agents/skills/inkstorm-content-authoring/SKILL.md)
- PR の準備、CodeRabbit のコメント取得とトリアージに使用する。[inkstorm-pr-and-coderabbit](.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)
- デモ録画とスタンドアロン配布ソース zip・README の確認に使用する。[inkstorm-demo-and-dist](.agents/skills/inkstorm-demo-and-dist/SKILL.md)
