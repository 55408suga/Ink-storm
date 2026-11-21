---
name: inkstorm-verify-and-visual
description: >-
  INKSTORM のビルド、ASan/UBSan・CTest・静的解析、GLSL 検証や描画・録画の目視確認に使う。
---

# ビルドと描画の検証

リポジトリルートで作業する。**ローカル検証と CI 検証の役割分担**を守る：
- **ローカル（最小・高速）**：変更したモジュールのピンポイントビルド・関連テスト、および CI で代替できない項目のみ（WSLg 実機 GPU 描画・視覚確認、`flake.nix` 変更時の Nix 評価）。
- **CI 委譲（重複実行を避ける）**：フルの ASan/UBSan + CTest、`format-check`、`lint`、`dist`（配布ソース zip 検証）は原則 GitHub Actions CI に任せる。**CI が失敗したときの再現** や **`flake.nix` / `CMakeLists.txt` / `tools/` の変更時** にのみ以下をローカル実行する。

## ローカル再現・フル検証コマンド（CI 失敗時・基盤変更時）

```sh
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan &&
    nix develop -c ctest --preset asan --output-on-failure
nix develop -c cmake --build --preset asan --target format-check
nix develop -c cmake --build --preset asan --target lint
```

`format-check` は clang-format、`lint` は `compile_commands.json` を使う clang-tidy と cppcheck、
`tools/check_shaders.py src`、ShellCheck を実行する。シェーダを変更した際は GLSL 検査の対象本数も確認する。

## WSLg 実機 GPU スモーク・視覚確認（ローカル固有）

```sh
nix develop -c cmake --preset release && nix develop -c cmake --build --preset release
./build/release/inkstorm --headless-smoke
nix develop -c ./build/release/inkstorm --smoke-frames 5
nix develop -c ./build/release/inkstorm --no-post --smoke-frames 5
```

WSLg では `GALLIUM_DRIVER=d3d12` の描画ログと、ポストシェーダ有効時および直接描画のログを確認する。
通常起動では表示を見て ESC または閉じるボタンによる終了を確認する。

## キャプチャ確認

- `--snap` が実装されたビルドは、その CLI の指定に従って変更場面の PNG を取得する。
  最大6枚を時系列に `build/snaps/snap_01.png` から保存し、下のコマンドで一覧にする。
- 動画からは `tools/extract_snaps.sh demo.mp4 build/snaps` で既定の6枚と
  `build/snaps/contact_sheet.png` を生成できる。ffmpeg と ffprobe を使う。
- スナップを採ったときは時刻、ビルド、シードまたは入力、シェーダ設定を記録する。
- 5秒以内にデモの見どころが伝わるか、和紙背景の上で敵や弾が判別できるか、UI が操作対象を隠さないかを確認する。
- シェーダを外した状態でも文字と重要な要素を判別できるか確認する。
- `F1` が利用できるビルドでは流速、渦度、計算時間が読めるか確認する。
- 発見事項は再現手順と該当時刻、またはキャプチャのパスとともに報告する。

```sh
ffmpeg -y -pattern_type glob -i 'build/snaps/snap_*.png' \
    -vf 'scale=640:-1,tile=3x2:padding=6:margin=6' -frames:v 1 build/snaps/contact_sheet.png
```
