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
  戦闘中:
    左ドラッグ          風筆（風と墨を描き、敵弾を偏向する）
    右クリック / E      渦印（渦を起こし、敵弾・墨・墨玉を集める）
    Space               風斬り（直線の突風で敵を吹き飛ばし叩きつける）

  報酬画面:
    1 〜 3 / クリック   強化札を選ぶ
    R / クリック        強化札を引き直す（リロール）
    4 / クリック        社を修繕する

  リザルト画面:
    クリック / Enter    再出陣

  その他:
    F1                  計測表示（描画時間・頂点数・流体計算時間など）の切替
    Esc / 閉じるボタン  終了（ランの途中なら敗北として精算・保存して終了）

CLI オプション
  --no-post             和紙ポストシェーダを無効にして直接描画する
  --no-save             セーブファイルを読み書きせずに起動する
  --save-path PATH      セーブファイルの保存先を明示指定する
  --headless-smoke      ウィンドウを開かずに動作確認を行う

セーブ先
  次の優先順で保存先を決定します（--no-save / --headless-smoke では読み書きしません）。
    1. --save-path PATH で指定したパス
    2. $XDG_DATA_HOME/inkstorm/save.txt（絶対パスの場合）
    3. $HOME/.local/share/inkstorm/save.txt
    4. 実行ファイルと同じディレクトリの inkstorm_save.txt

フォントとライセンス
  日本語表示には assets/fonts/ui_mincho.ttf を使用します。
  フォントファイルが見つからない、または読み込めない場合は、
  組み込みフォントによる英語（ASCII）表示へ自動的に切り替わります。
  同梱フォントのライセンスおよび出典は assets/LICENSES.txt と
  assets/fonts/OFL.txt を参照してください。
