# Phase 6-B タスク指示書：配布ソース zip・README/ライセンス最終検証・デモ動画収録＆視覚確認・Phase 6 完了ゲート

> **必読**: [phase6-demo-polish.md](phase6-demo-polish.md)（P6-A 仕様とマージ済み PR 本文のベンチ・視覚記録）、[PLAN.md](../PLAN.md) §1 / §2 / §9.1（Phase 6 完了ゲート）、[qa-and-testing.md](../qa-and-testing.md) §1〜§6、[art-direction.md](../art-direction.md) §14 / §15、[AGENTS.md](../../AGENTS.md)、[inkstorm-demo-and-dist](../../.agents/skills/inkstorm-demo-and-dist/SKILL.md)、[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md)、[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)
>
> **前提**: P6-A（`feat/phase6-demo-polish`）が `@coderabbitai review` 完了・CI 3 ジョブ全緑で `main` にマージ済みであること。着手時に `main` が最新かつ CI 全緑であることを確かめる。
> **作業場所 / ブランチ**: リポジトリルート（`full-scale-dev/Ink-storm`、作業ツリーがクリーンな状態）で P6-A マージ後の最新 `main` から `chore/phase6-dist-release` を作り、最終配布検証と Phase 6 Gate 記録を **1 本の PR** にする。

---

## 0. 目的・同一コミット原則・ローカル保存ポリシー・禁止事項

P6-A マージ後の最終ソースツリーに対し、**同一コミット（同一配布ツリー）** で以下を完遂し、**Phase 6 完了ゲート** を通過させて INKSTORM（墨嵐）の本開発をリリース可能状態で完了させる。

1. **`README.txt` と `assets/LICENSES.txt` の最終精査・更新**: ビルド手順、`RAYLIB_DIR` 上書き、全操作・`F1` X 線表示・CLI オプション（`--demo`, `--snap DIR` を含む）、セーブ先、同梱フォント・BGM・ライセンス表記を実物と 100% 照合する。
2. **スタンドアロン配布ソース zip（`dist/inkstorm-src.zip`）の生成・全件検査・独立展開素 `make` 検証**: `tools/package_dist.py`（`tools/dist.mk` をアーカイブ直下の `Makefile` として格納）を実行し、禁止ファイル混入ゼロ・独立展開先での Nix 抜き `/usr/bin/make` ＋ `/usr/bin/gcc` ビルド・`./inkstorm --headless-smoke`（別カレントディレクトリからの実行を含む）・`make clean`・展開先バイナリでの WSLg GPU 起動を確認する。
3. **デモ動画収録・コンタクトシート生成・最終視覚確認**: 同一ツリーの Release ビルド（1280×720）でデモ動画をローカル収録し、`ffprobe` 検査・`tools/extract_snaps.sh`（等間隔 6 枚＋3×2 `contact_sheet.png`）・`--snap`（通常 / `--no-post` 同一ティック比較 / `--showcase` / `F1` X 線 / 障害注入・アセット欠損・崩し試験 4 種）を目視確認する。
4. **Phase 6 完了ゲート記録**: `docs/PLAN.md` §9.1 の Phase 6 行を **`[完了]`** に更新し、最終検証結果を記録する。

### 0.1 同一コミット検証原則（[PLAN.md](../PLAN.md) §9.1・[qa-and-testing.md](../qa-and-testing.md) §6）
- P6-B では原則として `src/` の C コード・アセット本体を変更しない。したがって Release バイナリは P6-A マージ済み `main` と完全一致する。
- `README.txt` と `assets/LICENSES.txt` は `dist/inkstorm-src.zip` の収録対象であるため、**これら（およびレビュー対応修正）をすべて確定させた後の最終ツリー**で `tools/package_dist.py`・`unzip -l`・独立展開素 `make` 検証を実施し、検証対象のベース SHA・最終ツリーとの一致を PR 本文と `docs/PLAN.md` に記録する。
- 万一、素 `make` ビルドや配布検証・視覚確認の過程で `src/` や `tools/dist.mk` の修正が不可避となった場合は、修正後に **Release ベンチ・デモ動画/コンタクトシート・`dist/inkstorm-src.zip` のすべてを同一最終ツリーで再検証** する。

### 0.2 ローカル保存限定ポリシー＆公開表現規約（厳守）
- **ローカル保存のみ（コミット・PR 貼付禁止）**:
  - デモ動画（例：`build/snaps/demo.mp4`）、スナップショット PNG（`build/snaps/**/snap_*.png`）、コンタクトシート（`build/snaps/**/contact_sheet.png`）、および生成アーカイブ（`dist/inkstorm-src.zip`）は、すべて `.gitignore` 対象のローカル領域のみに保存する。
  - **絶対に `git add` / コミットしない**こと。
  - **PR 本文・レビューコメントへスクリーンショットやコンタクトシート画像を貼り付けない**こと（テキストによる検証環境・SHA・シード・操作順・解像度・再生時間・目視チェック所見のみを記載する）。
- **公開リポジトリ表現規約の厳守**:
  - `README.txt`、`assets/LICENSES.txt`、`docs/PLAN.md`、PR 本文・コメントには、特定の外部組織・個人・評価基準・既存商業作品名・不採用案の名称などの私的または外部固有名詞を一切書かない（「デモ動画」「配布要件」等の一般表現を用いる）。

### 0.3 禁止事項
- `dist/inkstorm-src.zip` へ開発専用ファイル（`AGENTS.md`, `CLAUDE.md`, `docs/`, `tests/`, `tools/`, `.github/`, `.coderabbit.yaml`, `.agents/`, `.claude/`, `flake.nix`, `flake.lock`, `.envrc`, `CMakeLists.txt`, `CMakePresets.json`, `third_party/unity/`, `.gitkeep`, 動画・PNG・ビルド成果物）を 1 件でも混入させない。
- 新機能追加や仕様変更を紛れ込ませない（実装と調整は P6-A で完了済み）。
- テストのしきい値や性能予算を緩めない。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `README.txt` | 更新 | コンパイル手順（`make` / `RAYLIB_DIR` / `make clean`）、全操作説明、`F1` X 線表示説明、CLI オプション（`--demo` / `--snap DIR` 追記）、セーブ先と無効オプション、フォント・BGM・ライセンス案内を実物と 100% 一致させる（§2.2） |
| `assets/LICENSES.txt` | 監査・必要時更新 | `assets/` 配下の全実ファイルと 1 対 1 照合し、版・権利・出典・未改変表記の漏れや齟齬があれば修正（§2.2） |
| `docs/PLAN.md` | 更新 | §9.1 の Phase 6 行を `[完了]` に更新し、`[P6-A](tasks/phase6-demo-polish.md)`・`[P6-B](tasks/phase6-dist-release.md)` への相対リンクと最終完了ゲート実績（性能・描画・視覚・配布 zip・CI 結果）を記録（§2.5） |
| `docs/qa-and-testing.md` | 必要時最小限 | 最終配布・視覚検証で追記が必要な契約事項がある場合のみ同期 |
| `tools/dist.mk`, `tools/package_dist.py` | 条件付き | 配布ビルド・禁止ファイル検査で不具合が見つかった場合のみ最小修正（原則変更なし） |

※ リポジトリルートに `Makefile` は置かない。スタンドアロン用 `Makefile` の原本は `tools/dist.mk` であり、`tools/package_dist.py` が `dist/inkstorm-src.zip` のルートへ `Makefile` として格納する構成を維持する。

---

## 2. 作業手順

### 2.1 P6-A マージ後の `main` 確認とベース SHA 固定

```sh
git switch main && git pull --ff-only && git switch -c chore/phase6-dist-release
git rev-parse HEAD
gh run list --branch main --limit 3
```

- P6-A のマージコミットに対する `main` の CI 3 ジョブ（`build-asan-and-test`, `static-analysis`, `dist-verify`）がすべて緑であることを確認し、検証基準 SHA を手元に控える。

---

### 2.2 `README.txt` と `assets/LICENSES.txt` の実物照合・最終更新

1. **`README.txt` の実物照合チェックリスト**:
   - **コンパイル方法**: `tools/dist.mk` の実ターゲット（`all`, `run`, `clean`）・既定 `RAYLIB_DIR`（`$(HOME)/.local/opt/raylib-6.2`）・必要ライブラリ（OpenGL / X11 / pthread / dl / rt / m、GCC、GNU Make、raylib 6.2）と 100% 一致していること。
   - **操作方法**: ロビー・筆選択・墨庵・設定（9 項目＋2 初期化）・戦闘（左ドラッグ・右クリック/E・Space・Esc）・ポーズ・報酬（強化札 28 種 / 大遺物 15 種 / 候補枯渇 2 択）・リザルト＆エンドレス・閉じるボタンの表記が実実装（`src/app/input_map.c`・`app_loop.c`）と一致していること。
   - **`F1` X 線表示**: 「その他」欄の `F1` の説明を、P6-A 仕上げ後の実機能（流体速度ベクトル・渦度・属性セル数・描画/流体計測の切替）へ更新する。
   - **CLI オプション**: P6-A で追加された `--demo`（固定シードの自動プレイデモ起動）と `--snap DIR`（指定ディレクトリへ要所 PNG 6 枚を自動保存して終了）を追記し、既存オプション（`--no-post`, `--no-save`, `--save-path PATH`, `--headless-smoke`、必要に応じ `--showcase`）および `cli_print_usage()` と矛盾がないこと。
   - **セーブ先**: `src/app/save.c` の `save_resolve_path()` 優先順 4 段階と、セーブを読み書きしないオプション一覧（`--no-save`, `--headless-smoke`, `--demo`, `--snap`）が一致していること。
   - **フォント・BGM・ライセンス**: `assets/fonts/ui_mincho.ttf`、`assets/fonts/title_brush.ttf`、`assets/fonts/OFL.txt`、`assets/audio/bgm_*.ogg`（4 曲）、およびフォールバック挙動と `assets/LICENSES.txt` 参照先が実物と一致していること。
2. **`assets/LICENSES.txt` の 1 対 1 照合**:
   ```sh
   find assets -type f ! -name '.gitkeep' | sort
   ```
   - 現行収録ファイルの全件と `assets/LICENSES.txt` を突き合わせる：
     - `assets/fonts/ui_mincho.ttf`（Shippori Mincho Regular, SIL OFL 1.1, 未改変）
     - `assets/fonts/title_brush.ttf`（Yuji Syuku Regular, SIL OFL 1.1, 未改変）
     - `assets/fonts/OFL.txt`（SIL OFL 1.1 全文）
     - `assets/audio/bgm_lobby.ogg` / `bgm_battle.ogg` / `bgm_boss.ogg` / `bgm_victory.ogg`（CC0 1.0）
     - `assets/LICENSES.txt`
   - 未記載ファイル・不要ファイル・破損パスが 0 件であることを確認する。

---

### 2.3 スタンドアロン配布ソース zip（`dist/inkstorm-src.zip`）の生成と徹底検証

`README.txt` と `assets/LICENSES.txt` の更新完了後に以下を実行する（[inkstorm-demo-and-dist](../../.agents/skills/inkstorm-demo-and-dist/SKILL.md) 準拠）。

```sh
# 1. 配布ソース zip の生成と内蔵検証（禁止パス検査 + 一時展開 + 素 make + --headless-smoke）
nix develop -c python3 tools/package_dist.py

# 2. アーカイブ全エントリを目視確認
nix develop -c unzip -l dist/inkstorm-src.zip

# 3. 独立した一時ディレクトリへ展開し、Nix 抜きの素 /usr/bin/make + /usr/bin/gcc で完全検証
tmp_dist=$(mktemp -d)
nix develop -c unzip -q dist/inkstorm-src.zip -d "$tmp_dist"
test -f "$tmp_dist/Makefile" && test -f "$tmp_dist/README.txt" && test -f "$tmp_dist/assets/LICENSES.txt"

env -i PATH=/usr/bin:/bin HOME="$HOME" \
    make -C "$tmp_dist" RAYLIB_DIR="${RAYLIB_DIR:-$HOME/.local/opt/raylib-6.2}"

# 展開先ディレクトリからの実行（Font / TitleFont / BGM 4 曲すべて found を確認）
env -i PATH=/usr/bin:/bin HOME="$HOME" "$tmp_dist/inkstorm" --headless-smoke

# 別カレントディレクトリ (/tmp) からの実行（実行ファイル基準の assets/ 解決を確認）
(cd /tmp && env -i PATH=/usr/bin:/bin HOME="$HOME" "$tmp_dist/inkstorm" --headless-smoke)

# 展開先の素 make バイナリで WSLg / 実機 GPU スモーク & --snap 動作確認（ローカル build/snaps/ 保存のみ）
mkdir -p build/snaps/p6b_dist_snap
gl_env=""
[ -e /dev/dxg ] && gl_env="GALLIUM_DRIVER=d3d12"
env ${gl_env:+"$gl_env"} "$tmp_dist/inkstorm" --smoke-frames 30
env ${gl_env:+"$gl_env"} "$tmp_dist/inkstorm" --no-post --smoke-frames 30
env ${gl_env:+"$gl_env"} "$tmp_dist/inkstorm" --demo --snap "$(pwd)/build/snaps/p6b_dist_snap"

# make clean の動作確認と一時ディレクトリ片付け
env -i PATH=/usr/bin:/bin HOME="$HOME" make -C "$tmp_dist" clean
test ! -e "$tmp_dist/inkstorm" && test ! -e "$tmp_dist/build/obj"
rm -rf "$tmp_dist"
```

- **確認項目**:
  - `Archive check: <N> entries, no forbidden paths.` が表示され、総エントリ数と `src/app/demo.c`・`assets/*`・ルート `Makefile`・ルート `README.txt` の収録を確認した。
  - `AGENTS.md`, `CLAUDE.md`, `docs/`, `tests/`, `tools/`, `.github/`, `.coderabbit.yaml`, `.agents/`, `.claude/`, `flake.nix`, `flake.lock`, `.envrc`, `CMakeLists.txt`, `CMakePresets.json`, `third_party/unity/`, `.gitkeep`, 動画・PNG・ビルド成果物が 0 件である。
  - Nix 抜きの素 `make` が警告・エラーなく（`-std=c11 -Wall -Wextra -Wpedantic -Wshadow`）通り、展開先と `/tmp` の双方から `--headless-smoke` が全アセット `found` で成功した。

---

### 2.4 デモ動画収録・コンタクトシート生成・最終視覚確認（同一コミット・ローカル保存のみ）

1. **同一ツリーの Release ビルド＆最終性能ベンチ記録**:
   ```sh
   nix develop -c cmake --preset release && nix develop -c cmake --build --preset release
   ./build/release/bench_fluid
   ./build/release/bench_game
   ```
   - `fluid_step` 平均・p95 `< 2.0 ms/tick`、`game_step`（Act 3・Act 6 エンドレス・Act 2 ストレス）平均・p95 `< 4.0 ms/tick` を確認し、測定環境（CPU / OS / コンパイラ / GPU ドライバ）と数値を記録する。

2. **デモ動画のローカル収録（`build/snaps/demo.mp4`・git 未追跡）**:
   - Release ビルド（1280×720）を実ウィンドウで起動し、以下の見どころが通して伝わるデモ動画をローカル `build/snaps/demo.mp4` へ収録する（固定シード `--demo` または再現可能な通し操作順・シードを記録）：
     - 開始 5 秒以内に、和紙キャンバス上の筆さばき・墨の流れ・敵弾偏向が直感的に伝わる立ち上がり
     - 4 属性反応（紅蓮墨嵐・雷墨連鎖・爆雷・三元共鳴）および地形ギミック（鳥居・岩柱・霊柱・山颪）
     - 強化札 3 択・大遺物・巻物と落款の UI
     - 幕ボス戦（多部位一筆龍の 3 龍玉・点睛演出を含む）
     - `F1` X 線オーバーレイ（速度ベクトル・渦度・属性セル数・`solve_ms`・`render_draw`・`prim_verts` が判読できる区間）
   - 収録後、`ffprobe` と `tools/extract_snaps.sh` で尺・解像度・6 枚スナップ・3×2 コンタクトシートを生成し、ローカルで目視確認する：
     ```sh
     ffprobe -v error -show_entries format=duration,size \
         -show_entries stream=width,height,r_frame_rate \
         -of default=nw=1:nk=1 build/snaps/demo.mp4
     tools/extract_snaps.sh build/snaps/demo.mp4 build/snaps/video_snaps 6
     ```
   - ※ WSLg 環境の画面キャプチャ制約等により X11 ウィンドウ直接録画が利用できない場合は、回避・偽装せず環境制約を明記したうえで、`--demo --snap` / `--showcase --snap` 等の正規手段による確認結果を正直に報告する。

3. **`--snap` による最終視覚回帰＆4 経路・アセット欠損・崩し試験（ローカル `build/snaps/` 保存のみ）**:
   ```sh
   nix develop -c ./build/release/inkstorm --demo --snap build/snaps/final_post
   nix develop -c ./build/release/inkstorm --demo --no-post --snap build/snaps/final_nopost
   nix develop -c ./build/release/inkstorm --showcase --snap build/snaps/final_showcase
   INKSTORM_DEBUG_FAULTS=shader nix develop -c ./build/release/inkstorm --demo --snap build/snaps/final_shader_fault
   INKSTORM_DEBUG_FAULTS=font nix develop -c ./build/release/inkstorm --demo --snap build/snaps/final_font_fault
   INKSTORM_DEBUG_FAULTS=shader,font nix develop -c ./build/release/inkstorm --showcase --snap build/snaps/final_both_fault

   for d in final_post final_nopost final_showcase; do
       ffmpeg -y -loglevel error -pattern_type glob -i "build/snaps/$d/snap_*.png" \
           -vf 'scale=640:-1,tile=3x2:padding=6:margin=6' -frames:v 1 "build/snaps/$d/contact_sheet.png"
   done

   # アセット丸ごと欠損起動確認（実行ファイル単体コピー + --no-save、--snap で ASCII 視認性検証画像を保存）
   tmp_noassets=$(mktemp -d)
   cp ./build/release/inkstorm "$tmp_noassets/inkstorm"
   nix develop -c "$tmp_noassets/inkstorm" --no-save --demo --snap "$(pwd)/build/snaps/final_noassets"
   nix develop -c "$tmp_noassets/inkstorm" --no-save --demo --smoke-frames 30
   rm -rf "$tmp_noassets"
   ```
   - [qa-and-testing.md](../qa-and-testing.md) §5.2 の全項目を目視確認する：
     - [ ] 開始 5 秒以内に見どころが伝わるか
     - [ ] 和紙背景上で、敵・敵弾・墨・炎（朱＋火の粉）・雷（蒼＋稲妻）のコントラストと形状区別が明確か
     - [ ] ボスの攻撃予兆（朱破線・構え・山颪予告）と HP バーが視認できるか
     - [ ] 報酬選択（3 択札・リロール・社修繕）およびロビー/墨庵/設定 UI が読め、操作対象を隠さないか
     - [ ] `--no-post` 同一ティック比較（`final_post` vs `final_nopost`）で形・濃淡・視認性の順位が保たれているか
     - [ ] `INKSTORM_DEBUG_FAULTS=shader` / `font` / `shader,font` およびアセット丸ごと欠損起動で重要情報と英語 ASCII 文字が判別できクラッシュしないか
     - [ ] `F1` X 線オーバーレイ（`snap_06.png` およびデモ動画該当区間）で速度ベクトル・渦度・`solve_ms`・属性セル数・`render_draw`・`prim_verts`・`blend_sw` が読め、描画予算（`< 4.0 ms/frame`, `<= 60,000`, `<= 12`）に収まっているか
     - [ ] 見本帳（`final_showcase/contact_sheet.png`）でトーン・太さ・主従が揃っているか
     - [ ] 崩し試験 4 種（グレースケール・5px ぼかし・25% 縮小・墨 6 割充填）で社・敵・弾・墨玉が識別できるか

---

### 2.5 `docs/PLAN.md` Phase 6 完了記録と同一コミット整合の再確認

1. `docs/PLAN.md` §9.1 の Phase 6 行を **`[完了]`** に更新する：
   - 並列 worktree / タスク列へ `[P6-A](tasks/phase6-demo-polish.md)`（完了）と `[P6-B](tasks/phase6-dist-release.md)`（完了）の相対リンクを張る。
   - 完了ゲート列へ「**達成済み**」と最終検証実績（基準 SHA、Release 性能ベンチ実測値、`F1` 描画予算実測値、デモ動画＆コンタクトシート・4 経路・崩し試験 OK、配布 zip エントリ数＆素 `make` + `--headless-smoke` 合格、CI 3 ジョブ全緑）を簡潔に記録する。
2. **レビュー修正後の再検証ルール**:
   - CodeRabbit や CI の指摘対応で `README.txt`・`assets/LICENSES.txt`・`tools/dist.mk`・`src/` のいずれかを修正した場合は、最終 push 前に必ず §2.3（`tools/package_dist.py`＋展開先素 `make`）を再実行する（`src/` または `assets/` を変えた場合は §2.4 のベンチ・スナップ/デモ確認も再実行する）。

---

## 3. 最終機械確認と Phase 6 完了ゲート（Definition of Done）

```sh
# 1. 未追跡メディア・ビルド成果物がコミット対象に入っていないこと
git status --short
git diff --cached --name-only | grep -E '\.(png|jpg|jpeg|webp|gif|mp4|mov|mkv|zip)$|^(build|dist)/' # 0 件

# 2. モジュール境界・アート規約・静的品質の最終確認
grep -rn 'raylib.h' src/core src/fluid src/ecs src/game src/audio src/app/save.* src/app/paths.* \
    src/app/cli.* src/app/frame_clock.* src/app/input_latch.* src/render/text_table.* src/render/ui_view.h  # 0 件
grep -rnE '(->|\.)internal\b' src/render src/audio src/app                                                    # 0 件
grep -rn 'game_debug_' src/app | grep -vE '^src/app/(showcase|demo)\.c:'                                      # 0 件
grep -rnE 'Draw(Line|Circle|Rectangle|Ring|Poly|TriangleStrip|Spline)' src/render                             # xray.c・ink_prims.c のみ
grep -rnE '\(Color\)\{' src                                                                                   # palette.c のみ
grep -rnE '\bGameState\s+[A-Za-z_][A-Za-z0-9_]*\s*(;|=|\[)' src tests \
    | grep -v 'typedef struct GameState'                                                                      # 0 件
find src -name '*.h' -printf '%f\n' | sort | uniq -d                                                          # 空
grep -rnE '^\s*static\s+[A-Za-z_][A-Za-z0-9_ *]*\s+\**[A-Za-z_][A-Za-z0-9_]*(\[[^]]*\])*\s*(=|;)' src \
    | grep -v 'static const'                                                                                  # 0 件
```

**Phase 6 完了ゲート（Definition of Done）**
- [ ] `README.txt` のコンパイル手順・`RAYLIB_DIR` 上書き・全操作・`F1` X 線・CLI オプション（`--demo` / `--snap DIR` 含む）・セーブ先・ライセンス参照が実物と 100% 一致している。
- [ ] `assets/LICENSES.txt` が `assets/` 配下の全実ファイルと 1 対 1 で一致し、権利・版・出典の漏れがない。
- [ ] 最終確定ツリーで `nix develop -c python3 tools/package_dist.py` が通り、`unzip -l dist/inkstorm-src.zip` で開発専用ファイル・メディア・ビルド成果物の混入が 0 件である。
- [ ] `dist/inkstorm-src.zip` を独立した一時ディレクトリへ展開し、Nix 抜きの素 `/usr/bin/make` ＋ `/usr/bin/gcc` ビルド・`./inkstorm --headless-smoke`（展開先および `/tmp` 実行の双方で全アセット `found`）・`make clean`・WSLg GPU スモークがすべて通った。
- [ ] 同一コミットの Release ビルドで性能予算（`fluid_step < 2.0 ms/tick`、`game_step < 4.0 ms/tick`、`render_draw < 4.0 ms/frame`、`prim_verts <= 60,000`、`blend_switches <= 12`）を達成し、測定値を記録した。
- [ ] デモ動画をローカル `build/snaps/` へ収録して `ffprobe`・`tools/extract_snaps.sh` 3×2 コンタクトシートを目視確認し、`--snap` による通常/`--no-post` 同一ティック比較・見本帳・4 描画経路＋併用・アセット欠損起動・崩し試験 4 種・`F1` X 線オーバーレイのすべてを合格確認した（検証記録はテキストで PR 本文に記載し、画像はコミットせず PR にも貼っていない）。
- [ ] `docs/PLAN.md` §9.1 の Phase 6 行を **`[完了]`** に更新し、P6-A / P6-B への相対リンクと最終完了ゲート実績を記録した。
- [ ] 公開リポジトリ表現規約（私的・外部固有名詞なし）および未コミット・未貼付メディア規約を満たしている。
- [ ] PR 作成直後に `@coderabbitai review` を投稿し、全指摘を 4 分類トリアージして必要な修正と返信を完了した（[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md) 準拠）。
- [ ] **CI の 3 ジョブ（`build-asan-and-test`［全 18 CTest］, `static-analysis`, `dist-verify`）がすべて緑**。
