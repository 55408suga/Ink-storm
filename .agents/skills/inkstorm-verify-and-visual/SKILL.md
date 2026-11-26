---
name: inkstorm-verify-and-visual
description: >-
  INKSTORM の見た目（描画・キャラ・ボス・背景・シェーダ・VFX・UI）や音（BGM・筆音・SE）を変更したとき、またはビルド・GLSL・オフライン音付き動画 QA（tools/qa_run.py）と視覚・聴覚レビューを行うときに必ず使う。
---

# ビルド・視覚・音声 QA 検証

リポジトリルート（`full-scale-dev/Ink-storm/`）で作業する。

## 必須トリガー

次のいずれかを変更したタスクでは、PR 作成前に必ずオフライン音付き動画 QA（`tools/qa_run.py`）を実行し、自動合否に加えて **生成されたコンタクトシートと音付き MP4 を直接見て・聴いて確認** する：

- 描画・キャラ・ボス・敵・背景・UI・VFX・画面揺れ（`src/render/`）
- シェーダ・流体表現（`src/render/*.glsl`, `fluid_view.c`, `post_fx.c`, `scenery.c`）
- 音声・筆音・SE・BGM アセット（`src/audio/`, `src/app/audio_out.*`, `assets/audio/`）

## ローカル検証と CI 検証の役割分担

- **ローカル（最小・高速＋環境固有）**：変更モジュールの最小ビルド・関連テスト、および CI で代替できない WSLg 実機 GPU 描画・オフライン音付き動画 QA・視覚/聴覚レビュー（`flake.nix` 変更時は Nix 評価も）。
- **CI 委譲（重複実行を避ける）**：フルの ASan/UBSan + CTest、`format-check`、`lint`、`dist` は原則 GitHub Actions CI に任せる。**CI 失敗時の再現** や **`flake.nix` / `CMakeLists.txt` / `tools/` 変更時** のみローカル実行する。

## オフライン音付き動画 QA（標準フロー）

`tools/qa_run.py` は `qa_record` と `ffmpeg`（`libx264` / `aac`）を使い、ウィンドウ操作なしで 60fps の音付き MP4（和風 BGM＋筆ループ＋SE 同期ミックス）・6枚キーフレーム＋`contact_sheet.png`・計測 `report.json` / `summary.json` を生成する。

### 1. ビルド

```sh
nix develop -c cmake --build build/release --target inkstorm qa_record -j
```

### 2. 変更内容に応じたシナリオを実行

出力先は **必ずリポジトリ外**（既定は `/tmp/inkstorm-qa-<uid>`、明示するなら `--out /tmp/...`）にする。リポジトリ内へ `.mp4`・`.png`・`.wav` を置かない。

| 変更内容 | 実行コマンド |
| --- | --- |
| キャラ・ボス・敵・背景・流体シェーダ | `nix develop -c python3 tools/qa_run.py --scenario boss_showcase --scenario act1_combat --out /tmp/inkstorm-qa`（シェーダ変更時は `--no-post` も別途確認） |
| ロビー・筆演出・筆音・UI | `nix develop -c python3 tools/qa_run.py --scenario lobby_brush --out /tmp/inkstorm-qa`（UI 全画面一覧は `nix develop -c ./build/release/inkstorm --showcase --snap /tmp/inkstorm-showcase`） |
| 元素反応・VFX・画面揺れ・SE | `nix develop -c python3 tools/qa_run.py --scenario thunderburst_stress --scenario lobby_brush --out /tmp/inkstorm-qa`（SE・筆音単体確認は `--no-bgm`） |
| 横断改修・大きめの見た目/音変更 | `nix develop -c python3 tools/qa_run.py --all --out /tmp/inkstorm-qa` |
| ディスプレイ無し環境での音声・指標確認 | `nix develop -c python3 tools/qa_run.py --all --no-video --out /tmp/inkstorm-qa` |

既存4シナリオでカバーできない新しい見た目・演出・UI を追加した場合は、`--showcase --snap /tmp/...` の併用や `tests/qa_record.c` へのシナリオ追加を検討する。

### 3. 自動合否と直接の視聴レビュー

1. `tools/qa_run.py` の終了コード `0` と `<out>/summary.json` の全件 `PASS` を確認する（決定性・黒/白/静止フレーム・音割れ `clip_ratio`・BGM/SE 独立ピーク・`post_enabled`・爆雷揺れ上限/揺れ率）。
2. 各シナリオの `<out>/<scenario>/snaps/contact_sheet.png`（必要なら個別の `snap_01..06.png`）と **音付き `<out>/<scenario>/<scenario>.mp4`** を直接読み取り、以下を目視・試聴確認する：
   - **視認性**：和紙背景や濃い墨・火・雷の上でキャラ・ボス・敵弾・UI 文字が明瞭に判別できるか。コールアウトやエフェクトが操作対象を覆い隠していないか。
   - **動き・演出**：シルエット・アニメーション・軌跡の破綻、過度な画面揺れや点滅がないか。
   - **音**：和風 BGM と筆音・SE の音量バランスが自然か、連続動作で途切れ・刺さる高音・異質な電子音が混ざっていないか。
   - **フォールバック**：シェーダや描画経路を変更したときは `--no-post` でも文字と重要要素が判別できるか。
3. 発見事項や確認結果は、実行コマンド・シナリオ名・該当時刻（またはローカル出力パス）を添えて報告する。ローカルの動画・画像は Git コミットや GitHub PR に添付しない。

## WSLg 実機 GPU スモーク・補助確認

```sh
./build/release/inkstorm --headless-smoke
nix develop -c ./build/release/inkstorm --smoke-frames 5
nix develop -c ./build/release/inkstorm --no-post --smoke-frames 5
```

- WSLg では `GALLIUM_DRIVER=d3d12` の描画ログと、ポストシェーダ有効時・直接描画のログを確認する。
- `F1` デバッグ表示に関わる変更では、流速・渦度・描画時間が読めるか確認する。
- 任意の外部動画からキーフレーム一覧を作る場合は `tools/extract_snaps.sh <input.mp4> /tmp/inkstorm-snaps` を使う。

## ローカル再現・フル検証コマンド（CI 失敗時・基盤変更時）

```sh
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan &&
    nix develop -c ctest --preset asan --output-on-failure
nix develop -c cmake --build --preset asan --target format-check
nix develop -c cmake --build --preset asan --target lint
```

`format-check` は clang-format、`lint` は `compile_commands.json` を使う clang-tidy と cppcheck、`tools/check_shaders.py src`、ShellCheck を実行する。シェーダを変更した際は GLSL 検査の対象本数も確認する。
