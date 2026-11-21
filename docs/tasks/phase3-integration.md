# Phase 3-Int タスク指示書：P0 の結合・WSLg 実機完走・Phase 3 完了ゲート

> **必読**: [phase3-pre-contracts.md](phase3-pre-contracts.md)、[phase3-sim-p0.md](phase3-sim-p0.md)、[phase3-render-audio-p0.md](phase3-render-audio-p0.md)、[PLAN.md](../PLAN.md) §9（特に §9.1 の Phase 3 行の完了ゲート）、[qa-and-testing.md](../qa-and-testing.md) §1〜§6、[art-direction.md](../art-direction.md) §14 / §15、[AGENTS.md](../../AGENTS.md)、[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md)、[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)、[inkstorm-demo-and-dist](../../.agents/skills/inkstorm-demo-and-dist/SKILL.md)
>
> **前提**: P3-Pre、P3-A（`feat/phase3-sim-p0`）、P3-B（`feat/phase3-render-audio-p0`）と、承認された契約変更の PR（Pre §1.3）が、それぞれ CI 全緑で `main` にマージ済みであること（Pre §0.1）。P3-A・P3-B の PR 本文（ベンチの実測値、キャプチャの一覧、フォントの確認結果、数値の変更、契約変更要求）を読んでおくこと。
> **作業場所 / ブランチ**: リポジトリルート（`full-scale-dev/Ink-storm`、作業ツリーがクリーンな状態）で、最新の `main` から `feat/phase3-integration` を作り、結合の修正と完了ゲートの記録を **1 本の PR** にする。

---

## 0. 目的と禁止事項

`main` 上で初めて一緒になった P3-A（シミュレーション）と P3-B（描画・音・入出力）を結び付け、実ウィンドウで第 1 幕（戦闘 → 強化札 3 択 → 大天狗 → リザルト）を通しで遊べる状態にする。そのうえで **Phase 3 完了ゲート**（PLAN §9.1）を確定させる。

1. 凍結契約と担当範囲が守られたまま合流したことの機械的な確認。
2. 実際のゲーム進行と結び付けたときに初めて見える不具合の修正と、完走できる難易度への調整。
3. 実機の完走、WSLg の 4 描画経路、アートのセルフチェック、性能予算、CI の確認と記録。

**禁止事項**
- P1 以降の内容（Phase 4 の範囲）を入れない。
- 修正は最小限にとどめ、理由を PR に書く。テストのしきい値や判定を緩めない。
- 契約（凍結ファイルの型・意味）を変える必要が新たに出たら、作業を止めてユーザーに確認する（承認後は本 PR の中で、凍結ファイル・呼び出し側・設計書を同じコミットで直す）。
- 難易度の調整は定義テーブルの数値だけで行い、規則を変えない。変えた値は game-design.md に反映する。

---

## 1. 変更対象

| 対象 | 操作 | 内容 |
| :--- | :---: | :--- |
| `main` の合流状態 | 確認 | §2.1 / §2.2 |
| `src/game/*`、`src/render/*`、`src/audio/*`、`src/app/*` | 最小限 | 結合の不具合の修正（§2.3） |
| 定義テーブル、`docs/game-design.md`、`docs/art-direction.md` | 数値のみ | 完走できる難易度への調整（§2.4） |
| `docs/architecture.md`、`docs/qa-and-testing.md`、`docs/PLAN.md` | 更新 | 契約変更の反映漏れの補正と Phase 3 完了の記録（§2.5） |

---

## 2. 手順

### 2.1 合流状態の確認
```sh
git switch main && git pull --ff-only && git switch -c feat/phase3-integration

# P3-Pre のマージコミット SHA を pre に入れる（PR 番号から取得すると squash マージでも確実）
pre=$(gh pr view <P3-Pre の PR 番号> --json mergeCommit -q .mergeCommit.oid)

# Pre 以降の main の履歴（P3-A・P3-B・契約変更の PR だけが並ぶ）
git log --first-parent --oneline "$pre"..main

# Pre 以降に凍結ファイルを変えたコミット（承認された契約変更の PR だけが並ぶ）
git log --oneline "$pre"..main -- CMakeLists.txt CMakePresets.json src/core src/fluid src/ecs \
    src/game/meta.h src/game/meta.c src/game/game.h src/game/game_ids.h src/game/game_events.h \
    src/game/game_components.h src/game/game_contract.c tests/test_core_smoke.c \
    docs/architecture.md docs/qa-and-testing.md docs/PLAN.md tools .github flake.nix flake.lock

# main の最新コミットに対する CI（push 時）が 3 ジョブとも緑であること
gh run list --branch main --limit 3
```
- 想定外のコミットが凍結ファイルを変えていたら、その変更の承認の有無を確かめ、承認がなければユーザーに確認する。
- `main` への push で CI が走るので、P3-A と P3-B を合わせた状態のフル検証（ASan/UBSan + CTest 14 件・整形・静的解析・配布 zip）は、ここで CI の結果として確認できる。ローカルで重複実行しない（AGENTS.md）。

### 2.2 境界の機械的な確認（いずれも結果が注記どおり）
```sh
grep -rn 'raylib.h' src/core src/fluid src/ecs src/game src/audio src/app/save.* src/app/paths.* \
    src/app/cli.* src/app/frame_clock.* src/app/input_latch.* src/render/text_table.* src/render/ui_view.h  # 0 件
grep -rnE '(->|\.)internal\b' src/render src/audio src/app                         # 0 件
grep -rn 'P3-Pre stub' src                                                         # 0 件
grep -rn 'game_debug_' src/app | grep -v showcase.c                                # 0 件
grep -rnE 'fluid(\.|->)(u0|v0|div|p|curl|dye0)\b' src/game                         # 0 件（fluid の作業用配列を読まない）
grep -rn 'rand(\|time(' src/game                                                   # 0 件
grep -rnE '\b(malloc|calloc|realloc|free)\(' src/game                               # game_create / game_destroy だけ
grep -rnE 'Draw(Line|Circle|Rectangle|Ring|Poly|TriangleStrip|Spline)' src/render  # xray.c・デバッグ・ink_prims.c だけ
grep -rnE '\(Color\)\{' src                                                        # palette.c だけ
grep -rnE '\bGameState\s+[A-Za-z_][A-Za-z0-9_]*\s*(;|=|\[)' src tests             # 0 件（値での宣言がない）
find src -name '*.h' -printf '%f\n' | sort | uniq -d                               # 空
grep -rnE '^\s*static\s+[A-Za-z_][A-Za-z0-9_ *]*\s+\**[A-Za-z_][A-Za-z0-9_]*(\[[^]]*\])*\s*(=|;)' src \
    | grep -v 'static const'                                                       # 0 件（可変の static がない）
```

### 2.3 結合の確認ポイント
P3-B は骨格の `game_step` と見本帳で開発していることがあるので、実際のゲーム進行と結び付けて次を確かめ、ずれていれば直す。

| 確認する所 | 内容 |
| :--- | :--- |
| イベントのペイロード | Pre §2.5 の表と、P3-A の発行内容・P3-B の解釈が一致する（特に `EV_KILL` の衝撃の向き、`EV_STORM_ZAP` の始点と終点、`EV_ENEMY_INTRO` の重なり、`EV_SURGE_START` が延長で出ないこと、自動回収の `EV_ORB_PICKUP` で音が鳴りすぎないこと） |
| 1 フレーム複数ティック | ウィンドウを掴んで動かすなどで描画を止めた後も、溜まったティック分のイベントが演出と音に反映され、押下が失われない |
| 報酬 | `1`〜`3` / クリック / `R` / `4` が `GameCommand` になり、無効なとき（リロール 0 回、満タンでの修繕）は何も起きない。札をクリックした後、次ウェーブの開始時に筆跡が出ない。戦闘中に押した数字キーが報酬画面で効かない |
| リザルト | VICTORY / GAME_OVER で 1 回だけ精算・保存され、内訳とシードが表示される。再出陣で新しいシードのランが始まり、前のランの演出が残らない |
| ラン中の終了 | 戦闘中・報酬画面・社砕けの最中に `Esc` で終了すると、敗北として精算・保存される。起動直後の幕転換中の終了では精算しない |
| 演出と状態 | 大天狗の予兆（`telegraph`）が技の 0.6 秒以上前から見える。墨還り（`dissolving`）が 0.6 秒で消える。HUD のピップが `cyclone_cd_total` / `gale_cd_total` で満ちる。ボス撃破後に HP バーが消える |
| 音 | BGM が通常戦 → ボス戦 → 勝利（または風音）と切り替わる。筆擦れの音量が筆の速さに追従する。プツプツと途切れない |
| CLI | `--headless-smoke` が本番の `game_step` で 600 ティック通り、`--smoke-frames` と見本帳が `game_autoplay_*` / 本実装のデバッグ API で動く |

### 2.4 完走できる難易度への調整
- 目標: メタ強化なしの豪筆で、操作に慣れた人が第 1 幕を 4〜7 分（報酬選択を含む）でクリアでき、操作を誤れば負けることがある（game-design §3.1 のテンポ）。
- 調整してよいのは、定義テーブルの数値（敵の HP・射撃間隔・増援間隔・ボスの攻撃間隔など）だけ。変えた値は前後の値と理由を PR に書き、game-design.md を更新する。調整後に `test_determinism_run`（ASan）と `bench_game`（Release）をローカルで再実行する。

### 2.5 設計書
- 承認された契約変更が architecture.md / qa-and-testing.md に反映されていることを確かめ、漏れがあれば補う。
- P3-A / P3-B が反映した数値の変更（game-design.md / art-direction.md）と、本タスクの調整が、`main` の実装と一致していることを確かめる。
- PLAN §9.1 の Phase 3 の行に完了を記録し、Phase 4（P4-Pre）へ進めることを書く。

---

## 3. 検証と Phase 3 完了ゲート

### 3.1 ローカル検証
`main` の CI（§2.1）で全体の検証は済んでいる。ローカルでは、本タスクで変えた範囲のビルド・関連テストと、CI で代替できない項目（性能・実機の描画・音）だけを行う。

```sh
# 1. 本タスクで変えた範囲（例: 定義テーブルを変えたら test_determinism_run と test_roguelike_meta）
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan &&
    nix develop -c ctest --preset asan --output-on-failure -R '<変えた範囲のテスト>'

# 2. Release: 性能ゲート（CI にはない）と CLI・実ウィンドウのスモーク
nix develop -c cmake --preset release &&
    nix develop -c cmake --build --preset release &&
    ./build/release/bench_fluid &&
    ./build/release/bench_game &&
    ./build/release/inkstorm --headless-smoke &&
    nix develop -c ./build/release/inkstorm --smoke-frames 300 &&
    nix develop -c ./build/release/inkstorm --no-post --smoke-frames 300 &&
    nix develop -c ./build/release/inkstorm --showcase --smoke-frames 30
```

### 3.2 WSLg 実機の完走（通常経路）
- 隔離したセーブ先で遊ぶ（実際の `$HOME` のセーブに触れない）。
  ```sh
  tmp=$(mktemp -d)
  nix develop -c ./build/release/inkstorm --save-path "$tmp/save.txt"   # 1 ラン目：第 1 幕を最後まで
  cat "$tmp/save.txt"                                                    # runs_played=1、墨晶が内訳の合計
  nix develop -c ./build/release/inkstorm --save-path "$tmp/save.txt"   # 2 ラン目：わざと負ける
  nix develop -c ./build/release/inkstorm --save-path "$tmp/save.txt"   # 3 ラン目：第 2 ウェーブ中に Esc
  cat "$tmp/save.txt"                                                    # runs_played=3、墨晶が加算されている
  XDG_DATA_HOME="$tmp/xdg" nix develop -c ./build/release/inkstorm      # 既定の解決 → $tmp/xdg/inkstorm/save.txt
  nix develop -c ./build/release/inkstorm --no-save                      # どこにも書かない
  ```
- 1 ラン目は「幕転換の題字 → 第 1〜3 ウェーブ（各ウェーブ後に強化札 3 択。リロールと社の修繕を少なくとも 1 回ずつ使う）→ 大天狗（第 2 フェーズまで到達）→ 勝利リザルト → 再出陣」を通す。2 ラン目は敗北（社砕け → リザルト）を、3 ラン目はラン中の終了による精算を通す。
- 次の表を PR 本文に記録する（キャプチャは `build/snaps/` に置き、コミットしない）。

| 項目 | 記録 |
| :--- | :--- |
| ビルド・環境 | コミット、Release、CPU / OS / GPU（`GALLIUM_DRIVER=d3d12` のログ） |
| シード・筆 | リザルトに表示されたシード、豪筆 |
| 結果・時間 | 勝利 / 敗北、報酬選択を含む所要時間 |
| 報酬 | 各ウェーブ後の選択（札と Lv、リロール・修繕の使用） |
| 大天狗 | 第 2 フェーズへの到達、撃破までの時間 |
| 反応・連殺 | 紅蓮墨嵐と雷墨連鎖の回数、最大の一筆連殺 |
| 精算 | 3 ラン分の墨晶の内訳と合計、セーブの内容 |
| 発見事項 | 再現手順と時刻、キャプチャのパス |

### 3.3 WSLg の 4 描画経路（qa-and-testing §5.1）

| 経路 | 起動 | 確認 |
| :--- | :--- | :--- |
| 1 通常 | `nix develop -c ./build/release/inkstorm` | §3.2 の完走 |
| 2 `--no-post` | `… --no-post` | 第 1 ウェーブ → 報酬 → 第 2 ウェーブの開始まで。経路 1 と形・濃淡・視認性の順位が同じ |
| 3 シェーダ失敗 | `INKSTORM_DEBUG_FAULTS=shader nix develop -c ./build/release/inkstorm` | `post shader unavailable; drawing directly` のログ。表示が経路 2 と同じ構成 |
| 4 フォント欠損 | `INKSTORM_DEBUG_FAULTS=font nix develop -c ./build/release/inkstorm` | `font unavailable; using built-in ASCII font` のログ。HUD・報酬・リザルトが英語で読め、落款が印の形だけになる |

- 追加で、**アセットが丸ごとない状態**も確かめる。実行ファイルだけを一時ディレクトリへ複製し、`--no-save` で起動して、フォント欠損と同じ表示で第 1 ウェーブまで遊べ、`--headless-smoke` も通ることを確かめる（配布物から `assets/` が欠けた場合の確認）。
- 各経路で `--showcase` の 4 ページも `F12` で撮る（phase3-render-audio-p0.md §4）。経路ごとにコンタクトシートを作る。
  ```sh
  ffmpeg -loglevel error -y -pattern_type glob -i "build/snaps/<経路名>/*.png" \
      -vf 'scale=640:-1,tile=3x2:padding=6:margin=6' -frames:v 1 "build/snaps/<経路名>/contact_sheet.png"
  ```

### 3.4 アートのセルフチェック（結合後）
- 見本帳は本実装のスポーン（Pre §2.7）で描かれるようになったので、P3-B のセルフチェック（phase3-render-audio-p0.md §4 の 5 項目）を、結合後の見本帳と §3.2 の戦闘中のキャプチャでやり直す。
- 戦闘中のキャプチャには、少なくとも「矢の偏向」「紅蓮墨嵐」「雷墨連鎖」「叩きつけ」「大天狗の予兆」「報酬画面」「勝利リザルト」「敗北リザルト」を含める。
- 崩し試験（グレースケール・5px ぼかし・25% 縮小・墨 6 割）で、社・敵・弾・墨玉・ボスの予兆が判別できる。

### 3.5 性能
| 項目 | 基準 | 確認方法 |
| :--- | :--- | :--- |
| `fluid_step` | 平均・p95 `< 2.0 ms/tick` | `bench_fluid` |
| `game_step` | 平均・p95 `< 4.0 ms/tick` | `bench_game`（5 試行の中央値。負荷行も記録） |
| `render_draw` | `< 4.0 ms/frame`、頂点 `<= 60,000`、合成モードの切り替え `<= 12` | 大天狗戦で画面の半分以上に墨を描いた状態の `F1` |

- p95 だけがホストの揺らぎで超える場合は、しきい値を変えずに実測値とノイズの状況を添えてユーザーの判断を仰ぐ。

### 3.6 CI と PR
- PR の作成・push はユーザーの許可を得てから、inkstorm-pr-and-coderabbit の手順で行う。
- PR の CI 3 ジョブ（`build-asan-and-test`、`static-analysis`、`dist-verify`）がすべて緑であること。`dist-verify` のログで、展開先の素の `make` と本番の `./inkstorm --headless-smoke` が通り、`Font: assets/fonts/ui_mincho.ttf found` が出ていること。
- PR 本文には次を書く。
  - §2.1・§2.2 の確認結果（Pre 以降の履歴と凍結ファイルの変更コミット）
  - §2.3 で直した不具合と §2.4 の調整（前後の値と理由）
  - §3.1 の実行結果、`bench_fluid` / `bench_game` の記録（CPU / OS / コンパイラ、5 試行の各値、中央値、負荷行）
  - §3.2 の完走記録、§3.3 の 4 経路とアセットなしの確認、コンタクトシートのパス、§3.4 の結果、§3.5 の `F1` の値
  - CI に任せた項目（`build-asan-and-test`、`static-analysis`、`dist-verify`）

### 3.7 完了条件（Phase 3 Gate）
- [ ] §2.1・§2.2 の確認がすべて注記どおり。承認のない契約変更がない。
- [ ] CTest 14 件（`test_core_smoke`、`test_fluid`、`test_ecs`、`test_roguelike_meta`、`test_elements`、`test_determinism_run`、`test_app_core`、`test_audio`、`test_app_save`、`test_text_table`、`test_cli_headless_smoke`、`test_cli_usage_*` 3 件）が CI の ASan/UBSan（`-Werror`）で通り、LeakSanitizer の報告もない。
- [ ] **`test_determinism_run` が緑**（2 インスタンスのビット一致、健全性、複数ティック消費、イベントの上限、不正な入力、容量の満杯）。
- [ ] **実ウィンドウで第 1 幕を完走**した（戦闘 → 3 択 × 3 → 大天狗 → 勝利リザルト）。敗北とラン中の終了の流れ、精算・セーブの読み書き・`--no-save` の隔離も確認した（§3.2）。
- [ ] **WSLg の 4 経路**（通常・`--no-post`・シェーダ失敗・フォント欠損）とアセットなしの起動を確認し、アートのセルフチェックを通した（§3.3・§3.4）。
- [ ] 性能予算（§3.5）を満たし、測定環境と値を PR に記録した。
- [ ] **CI の 3 ジョブがすべて緑**。
- [ ] ユーザーの許可を得て `main` へマージしたら、P4-Pre（追加のイベント・敵 / 札 / 遺物の ID・`UiView` の拡張枠、ポーズ導入に伴う `Esc` と終了時の精算の扱い）へ進める。
