---
name: inkstorm-verify-and-visual
description: >-
  INKSTORM の見た目（描画・UI・シェーダ・VFX）や音（BGM・筆音・SE）、ゲームプレイ・操作感・ビルド基盤を変更したとき、影響起点の QA 設計・オフライン音付き動画 QA（tools/qa_run.py）・GLSL/静的解析・視覚/聴覚レビューを行うときに必ず使う。
---

# 影響起点 QA・視覚・聴覚・ビルド検証

作業はリポジトリルート（`full-scale-dev/Ink-storm/` または割り当て worktree ルート）で行う。

## QA の 3 原則

1. **自動 PASS は最低ライン（天井ではない）**: クラッシュなし・黒画なし・音割れなし・有限値は健全性の下限に過ぎない。常に **初見プレイヤー目線** で一次情報から視認性・手触り・公平性・水墨世界観との調和を評価する。
2. **アンチマスキング原則**: 検証したい欠陥を覆い隠す便宜設定を信用しない（無敵や HP 下限補助で理不尽な被弾を隠す、短すぎる録画尺で行動周期を切る、BGM で不快・異質な SE を覆う、古いビルドを使い回す等）。
3. **影響起点の自己設計 QA**: 固定コマンド表を機械的に回して終わりにしない。自分の差分から「何が壊れ得るか／違和感が出得るか」を逆算し、そのリスクを確実に露出させる最小の検証・録画条件を組み立てる（既存シナリオで十分に映らない場合は条件調整や一時確認、シナリオ追加を検討する）。

## 4 ステップの影響起点 QA ループ

### Step 1: 影響軸のマッピング

差分から影響を受ける品質軸を最初に明示する：

1. **ルール・バランス・公平性・決定性**: 戦闘テンポ、予兆と対処余地、勝敗の納得感、同条件の再現性
2. **操作・入力感・境界ケース**: ポインター／キーボード競合、画面遷移・ポーズ復帰、連打や端付近の挙動
3. **視覚・図地分離・UI・フォールバック**: 背景や濃い墨・エフェクト上での主従と可読性、文字・配置、シェーダ／アセット欠損時の見た目
4. **音響・世界観適合・バランス・反復快適性**: 水墨・和風の世界観に馴染む音色か、BGM と筆音・SE の主従、連続再生で刺さる音・途切れがないか
5. **性能・メモリ/UB 安全性・モジュール境界・配布衛生**: フレーム予算、ASan/UBSan、依存方向、配布 zip への開発専用ファイル非混入

### Step 2: 的を絞った検証・録画の設計と実行

- 影響軸に合わせて、単体／回帰テスト、変更前後比較、`tools/qa_run.py`（シナリオと `--no-bgm` / `--no-post` / `--frames` / `--keep-wav` の組合せ）、`--showcase --snap`、WSLg 実ウィンドウ確認を組み合わせる。
- 同一シナリオを複数条件（例: 通常と `--no-bgm` や `--no-post`）で録画する場合は、後続実行で `<name>.mp4`・スナップ・`report.json` が上書きされないよう条件ごとに別の `--out` ディレクトリを指定する。
- 録画前に必ず対象バイナリ（`inkstorm` / `qa_record`）を再ビルドし、古いビルドの検証を避ける。
- 実機オーディオ出力経路（`src/app/audio_out.*`）やインタラクティブな操作感はオフライン録画を経由しないため、必ず実ウィンドウ（`nix develop -c ./build/release/inkstorm`）で実機再生・遅延・音切れ・操作手触りを確認する。
- 既存シナリオで変更挙動が明確に映らない場合は、対象状況が露出する録画条件や専用シナリオ・補助確認を設計して補う。

### Step 3: 一次情報の批判的レビュー（自己改善）

- 複数条件で録画した場合は各条件の出力ごとに、`summary.json` / `report.json` の自動合否に加え、生成された `contact_sheet.png`（必要時は個別 `snap_*.png`）を直接目視し、音付き `.mp4`（音声変更時は `--no-bgm` の SE・筆音単体も）を直接試聴する。
- 「合格にする理由」ではなく、**迷い・視認性低下・理不尽さ・情報過多・世界観から浮いた音・不自然な間** を能動的に探す。
- スコープ内の欠陥はその場で修正して再検証する。スコープ外で発見した課題は後続の改善チケットとして切り出す。

### Step 4: 根拠の報告とレビュー用キャプチャの保全

- 検証条件（実行コマンド・シナリオ・フラグ）、目視・試聴で確認した事実、およびオフライン QA では証明できない残留リスクを報告に明記する。
- 人間がレビューする動画・画像は **必ずリポジトリ外**（一時 QA は `/tmp/...`、最終レビュー用に保管する場合はユーザー指定のローカル確認フォルダ）に保存し、Git コミットや PR にメディアやローカルバイナリを含めない。

## ツールボックス・リファレンス

### ビルド（`inkstorm` / `qa_record`）

```sh
nix develop -c cmake --preset release &&
    nix develop -c cmake --build build/release --target inkstorm qa_record -j
```

### オフライン音付き動画 QA（`tools/qa_run.py`）

60fps 音付き MP4・6 枚キーフレーム＋`contact_sheet.png`・`report.json` / `summary.json` をリポジトリ外（既定 `/tmp/inkstorm-qa-<uid>`）へ出力する。

```sh
nix develop -c python3 tools/qa_run.py --scenario <NAME> --out /tmp/inkstorm-qa
nix develop -c python3 tools/qa_run.py --all --out /tmp/inkstorm-qa
```

- **主要フラグ**:
  - `--all` / `--scenario NAME`（複数指定可）: 全シナリオまたは対象シナリオのみ実行
  - `--frames N`: 既定フレーム数を上書き（行動周期や遷移が切れない尺を確保）
  - `--no-bgm` / `--keep-wav`: SE・筆音単体の試聴や生 WAV 確認（BGM によるマスキングを排除）
  - `--no-post`: ポストシェーダ無効時の描画・可読性フォールバック比較
  - `--no-video`: ヘッドレス音声＋指標のみ検証（`ffmpeg`・ディスプレイ不要）
  - `--out /tmp/...`: 出力先を明示（必ずリポジトリ外を指定）
- **既存の基準シナリオ例**（一覧は `./build/release/qa_record --list-scenarios`。**これらは出発点であり上限ではない**）:
  `lobby_brush`、`thunderburst_stress`、`act1_combat`、`boss_showcase`

### 補助ツール・スモーク・フォールバック・性能計測

```sh
nix develop -c ./build/release/inkstorm --showcase --snap /tmp/inkstorm-showcase
./build/release/inkstorm --headless-smoke
nix develop -c ./build/release/inkstorm --smoke-frames 5
nix develop -c ./build/release/inkstorm --no-post --smoke-frames 5
```

- **障害注入フォールバック**: `INKSTORM_DEBUG_FAULTS=shader` / `font` / `sprites`（カンマ併用可）で欠損・読込失敗時の安全切替を確認する。
- **任意動画の分解**: `tools/extract_snaps.sh <input.mp4> /tmp/inkstorm-snaps` で等間隔キーフレームとコンタクトシートを生成する。
- **性能ベンチ（Release）**: `bench_fluid`・`bench_game` で更新性能を確認する（描画 CPU 時間や流体指標は実機 `F1` オーバーレイも併用）。

## ローカル検証と CI 委譲の切り分け

- **ローカル優先**: 変更範囲の最小ビルド・関連テスト、CI で代替できない WSLg 実機 GPU 描画（`GALLIUM_DRIVER=d3d12`）・オフライン動画 QA・視聴レビュー、`flake.nix` 変更時の Nix 評価。
- **CI 委譲**: フル ASan/UBSan + CTest、`format-check`、`lint`、`dist` は原則 GitHub Actions CI に任せ、**CI 失敗の再現時** や **`flake.nix` / `CMakeLists.txt` / `tools/` 変更時** のみローカル実行する。

ローカルでフル検証を回す際の要点・落とし穴：

- **ビルド鮮度**: `tools/qa_run.py` は既定で `build/release/qa_record`（無ければ `build/debug/qa_record`）を参照する。変更後は必ず該当ターゲットを再ビルドしてから録画する。
- **ASan/UBSan + CTest**: `nix develop -c cmake --preset asan && nix develop -c cmake --build --preset asan && nix develop -c ctest --preset asan --output-on-failure`
- **整形・静的解析**: `asan` 構成後に `--target format-check`（`src/`・`tests/` の clang-format）と `--target lint`（`compile_commands.json` 参照の clang-tidy / cppcheck、`tools/check_shaders.py src`、ShellCheck）。シェーダ変更時は GLSL 検証の対象本数と `--no-post` フォールバックを併せて確認する。
- **配布衛生（`dist`）**: `tools/package_dist.py` / `--target dist` の展開物に開発専用物（`AGENTS.md`、`docs/`、`tests/`、`tools/`、`third_party/unity/`、`.agents/`、`.claude/` 等）や録画・ビルド成果物を混入させない。
