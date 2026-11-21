---
name: inkstorm-submission-demo
description: >-
  INKSTORM のマイルストーンを録画し、90秒以内の授業デモ動画と提出用ソース zip・README を最終確認する。
---

# デモ動画と提出物の確認

Release をビルドし、実際に録画する場面を通しで操作する。録画時に使用したビルド、シード、操作順、解像度を記録する。
動画は MP4 または MOV とし、ソース zip と別の提出物として扱う。

| 時間の目安 | 内容 |
| --- | --- |
| 0–5秒 | 和紙上で墨と属性が反応する見どころを見せる。 |
| 5–20秒 | 基本操作とプレイ目標を説明する。 |
| 20–57秒 | ビルドの選択と属性コンボを実際の操作で見せる。 |
| 57–83秒 | ボス風合戦を見せる。 |
| 終盤の5–7秒 | `F1` X線オーバーレイで流体速度ベクトル、渦度ヒートマップ、処理時間を見せる。 |

`ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 demo.mp4` で90秒以内か確認する。
`tools/extract_snaps.sh demo.mp4 build/snaps` のコンタクトシートと動画終盤を目視し、操作説明、文字、
`F1` 表示が読めることを確認する。各場面の撮影は対応する機能が実装されたビルドで行う。

## 提出 zip と README

1. `README.txt` のコンパイル手順、`RAYLIB_DIR` の上書き方法、遊び方、アピールポイントを実物と照合する。
   最終提出では操作・アピール欄を埋め、授業の環境条件を確認する。
2. `nix develop -c python3 tools/package_submission.py` を実行する。禁止エントリ検査と、
   一時展開先のホスト `make`、`./inkstorm --headless-smoke` が通った zip を使う。
3. `nix develop -c unzip -l dist/inkstorm-submission.zip` でトップレベルの `Makefile` と `README.txt`、
   ソース、必要なアセット、登録済み依存を確認する。開発専用ファイルや動画を同梱しない。
