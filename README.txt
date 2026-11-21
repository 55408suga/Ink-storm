INKSTORM（墨嵐）

コンパイル方法
  Linux 上でソースを展開し、Makefile のあるディレクトリで実行します。
  GNU Make、GCC、raylib 6.2 のヘッダと静的ライブラリ、OpenGL と X11 の
  開発用ライブラリが必要です。

    make
    make run

  raylib の既定の配置先は $HOME/.local/opt/raylib-6.2 です。
  別の配置先を使う場合は include/raylib.h と lib/libraylib.a を含む
  ディレクトリを指定してください。

    make RAYLIB_DIR=/path/to/raylib-6.2
    ./inkstorm --headless-smoke

  ビルド成果物は make clean で削除できます。

操作方法
  現段階ではウィンドウに動作確認用の状態を表示します。
  ESC またはウィンドウの閉じるボタンで終了します。
