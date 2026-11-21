---
name: inkstorm-demo-and-dist
description: >-
  INKSTORM のデモ動画を録画・確認し、スタンドアロン配布用ソース zip と README を検証する。
---

# デモ録画と配布アーカイブの確認

Release をビルドし、録画対象シーンを通しで操作する。録画時に使用したビルド、シード、操作順、解像度を記録する。

- 動画から `tools/extract_snaps.sh demo.mp4 build/snaps` でスナップショットと
  `build/snaps/contact_sheet.png` を生成し、表示と視認性を目視確認する。
- `ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 demo.mp4` で再生時間を確認する。
- `F1` デバッグ表示が利用できるビルドでは、流速・渦度・処理時間が読めることを確認する。

## 配布ソース zip と README

1. `README.txt` のコンパイル手順、`RAYLIB_DIR` の上書き方法、操作説明を実物と照合する。
2. `nix develop -c python3 tools/package_dist.py` を実行する。禁止エントリ検査と、
   一時展開先のホスト `make`、`./inkstorm --headless-smoke` が通ったアーカイブを使う。
3. `nix develop -c unzip -l dist/inkstorm-src.zip` でトップレベルの `Makefile` と `README.txt`、
   ソース、必要なアセット、登録済み依存を確認する。開発専用ファイルや動画を同梱しない。
