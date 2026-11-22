# Phase 5-Int タスク指示書：P2 拡張の 3 並列合流・全カタログ＆エンドレス実機完走・Phase 5 完了ゲート

> **必読**: [phase5-pre-contracts.md](phase5-pre-contracts.md)、[phase5-roguelike-p2.md](phase5-roguelike-p2.md)、[phase5-endless-ryu-p2.md](phase5-endless-ryu-p2.md)、[phase5-ui-settings-p2.md](phase5-ui-settings-p2.md)、[PLAN.md](../PLAN.md) §9.1（Phase 5 完了ゲート）、[qa-and-testing.md](../qa-and-testing.md) §1〜§6、[art-direction.md](../art-direction.md) §6 / §10 / §14 / §15、[AGENTS.md](../../AGENTS.md)、[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md)、[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)、[inkstorm-demo-and-dist](../../.agents/skills/inkstorm-demo-and-dist/SKILL.md)
>
> **前提**: P5-Pre、P5-A（`feat/phase5-roguelike-p2`）、P5-B（`feat/phase5-endless-ryu-p2`）、P5-C（`feat/phase5-ui-settings-p2`）がすべて `@coderabbitai review` 完了・CI 3 ジョブ全緑で `main` にマージ済みであること。各 PR の本文（ベンチ実測値、キャプチャ一覧、数値調整、契約変更要求）を通読しておくこと。
> **作業場所 / ブランチ**: リポジトリルート（`full-scale-dev/Ink-storm`、作業ツリーがクリーンな状態）で最新の `main` から `feat/phase5-integration` を作り、結合修正と Phase 5 Gate の記録を **1 本の PR** にする。

---

## 0. 目的と禁止事項

3 並列で開発された P5-A（強化札 28 種・墨庵解放遺物 6 種＝全 15 種・候補枯渇 2 択）、P5-B（狐符＋護符結界＋追尾符・エンドレス周回スケール・一筆龍の多部位 ECS 版）、P5-C（設定画面拡充・遅延勝利精算＆エンドレス UI・金砂子・狐符＆多部位一筆龍の水墨描画・SE）を `main` 上で結合し、**「墨庵で解放遺物購入 → 拡充設定変更 → 本編 3 幕（狐符・多部位一筆龍 3 フェーズ撃破）→ 勝利画面からエンドレス継続 → 第 4 幕以降（金砂子・周回スケール・候補枯渇 2 択）→ エンドレス終了 1 回精算」** を実ウィンドウで実証して **Phase 5 完了ゲート（P2 拡張完成）** を通過させる。

**禁止事項**
- Phase 6（`--demo` / `--snap` CLI、`F1` X 線仕上げ、最終デモ録画・配布 zip 仕上げ）を先回りして実装しない。
- テストのしきい値や性能予算（`fluid_step < 2.0 ms/tick`, `game_step < 4.0 ms/tick`, `render_draw < 4.0 ms/frame`）を緩めない。
- 難易度・バランス調整は定義テーブルの数値変更のみで行い、変更した数値は `docs/game-design.md` / `docs/art-direction.md` に同期する。

---

## 1. 変更対象

| 対象 | 操作 | 内容 |
| :--- | :---: | :--- |
| `main` の 3 並列合流状態 | 確認 | §2.1 / §2.2 |
| `src/game/*`、`src/render/*`、`src/audio/*`、`src/app/*` | 最小限 | 3 並列結合時の不整合修正（§2.3） |
| 定義テーブル、`docs/game-design.md`、`docs/art-direction.md` | 数値のみ | 全 28 札/15 遺物・多部位一筆龍・エンドレス周回テンポの微調整（§2.4） |
| `docs/PLAN.md`、`docs/architecture.md`、`docs/qa-and-testing.md` | 更新 | Phase 5 完了の記録と最終同期（§2.5） |

---

## 2. 手順

### 2.1 3 並列合流状態の確認
```sh
git switch main && git pull --ff-only && git switch -c feat/phase5-integration

# P5-Pre のマージコミット SHA を取得
pre=$(gh pr view <P5-Pre の PR 番号> --json mergeCommit -q .mergeCommit.oid)

# Pre 以降の main の履歴（P5-A・P5-B・P5-C・承認済み契約変更の PR のみであること）
git log --first-parent --oneline "$pre"..main

# Pre 以降に凍結ファイルを変えたコミットの有無を確認
git log --oneline "$pre"..main -- CMakeLists.txt CMakePresets.json src/core src/fluid src/ecs \
    src/game/meta.h src/game/meta.c src/game/game.h src/game/game_ids.h src/game/game_events.h \
    src/game/game_components.h src/game/game_contract.c src/game/game_internal.h \
    src/game/game.c src/game/hash.c src/game/debug_api.c src/render/ui_view.h \
    tests/test_core_smoke.c tests/test_fluid.c tests/bench_fluid.c tests/test_ecs.c \
    tests/test_app_save.c assets tools .github flake.nix flake.lock .agents AGENTS.md \
    docs/PLAN.md docs/architecture.md docs/qa-and-testing.md

# main の最新コミットに対する CI 3 ジョブがすべて緑であること
gh run list --branch main --limit 3
```

### 2.2 境界・アート規約・静的品質の機械確認（いずれも結果が注記どおり）
```sh
grep -rn 'raylib.h' src/core src/fluid src/ecs src/game src/audio src/app/save.* src/app/paths.* \
    src/app/cli.* src/app/frame_clock.* src/app/input_latch.* src/render/text_table.* src/render/ui_view.h  # 0 件
grep -rnE '(->|\.)internal\b' src/render src/audio src/app                         # 0 件
grep -rn 'game_debug_' src/app | grep -v showcase.c                                # 0 件
grep -rnE 'fluid(\.|->)(u0|v0|div|p|curl|dye0)\b' src/game                         # 0 件
grep -rn 'rand(\|time(' src/game                                                   # 0 件
grep -rnE '\b(malloc|calloc|realloc|free)\(' src/game                              # game_create / game_destroy のみ
grep -rnE 'Draw(Line|Circle|Rectangle|Ring|Poly|TriangleStrip|Spline)' src/render  # xray.c・ink_prims.c のみ
grep -rnE '\(Color\)\{' src                                                        # palette.c のみ
grep -rnE '\bGameState\s+[A-Za-z_][A-Za-z0-9_]*\s*(;|=|\[)' src tests \
    | grep -v 'typedef struct GameState'                                           # 0 件
find src -name '*.h' -printf '%f\n' | sort | uniq -d                               # 空
grep -rnE '^\s*static\s+[A-Za-z_][A-Za-z0-9_ *]*\s+\**[A-Za-z_][A-Za-z0-9_]*(\[[^]]*\])*\s*(=|;)' src \
    | grep -v 'static const'                                                       # 0 件
```

### 2.3 3 並列結合の確認ポイント
P5-A・P5-B・P5-C が並列開発されたため、結合状態で以下の連携を重点的に確認し、ずれがあれば修正する。

| 結合箇所 | 確認内容 |
| :--- | :--- |
| **墨庵解放遺物 6 種 × 戦闘・HUD・精算** | 墨庵で解放したスロット `0..5` だけがラン中の大遺物 3 択に出現し、未解放は混入しないこと。`三元の印`（0.22/1.2s）・`墨嵐の極意`（チェイン 3〜＆4 反応 ×1.5）・`雷火の宝珠`（爆雷墨玉 1.5s CD）・`鬼瓦`（中量＆石鬼の即撃破叩きつけ・鵺は 60 維持）・`無尽の筆`（回復 ×2.5＆上限 -40）・`百鬼夜行の巻物`（増援 ×0.75/+2・墨玉 +1・リザルト「巻物ボーナス」×1.5）のすべてが戦闘・HUD・リザルトで一致すること |
| **強化札 28 種 × 筆 3 種 × 二段風斬り HUD** | 残り 4 札（`残り火`・`雷雲の筆`・`二段風斬り`・`硯の恵み`）が 3 択・ポーズ・リザルトに正しい JA/EN で表示され、`二段風斬り` Lv3 でカーソル下の風斬り準備ピップが 2 個表示されて 2 連射・順次補充できること |
| **狐符・護符結界・追尾符 × 属性・描画** | 第 3 幕 3-2 に狐符が 1 体出現し（エリート化しない）、味方への護符結界（胡粉弧＆被ダメ -50%）と追尾符（140px/s 社旋回・炎 > 0.30 で焼失・偏向後は 20 ダメージ）が戦闘・描画で一致すること |
| **多部位一筆龍 3 フェーズ × 戦闘・HUD・VFX・SE** | 第 1 フェーズで無敵頭部が偏向弾・雷墨連鎖・爆雷に吸われず 3 龍玉を狙えること。龍玉破壊ごとに `EV_BOSS_PART_BREAK` の破砕 VFX・SE が出て、その区間の龍の筆線がかすれ、上中央 HUD の合計 HP バーと 3 龍玉印が同期すること。3 玉全破壊で第 2 フェーズ「点睛」（目へ朱点・山颪 6s）、龍睛 HP 40% 以下で第 3 フェーズ「昇龍」（三元共鳴 ×2）へ進むこと |
| **エンドレス周回 × 遅延精算 × 金砂子 × 枯渇 2 択** | 第 3 幕勝利直後は未精算プレビューで 3 ボタンが出ること。「エンドレスへ進む」で社 HP +3 ＆大遺物 3 択 → 第 4 幕（金砂子 20 粒・周回スケール 1）へ進み、第 4〜6 幕の地形/ボス/山颪が循環すること。候補枯渇時に 2 択（通常 +1/+10、ボス +3 超過変換/+30）が操作でき、エンドレス終了時に 1 回だけ精算され二重計上が起きないこと。勝利画面でロビー/再出陣/ウィンドウ閉じを選んだ場合も 1 回だけ精算されること |
| **設定画面拡充 × セーブ互換 × ライブ反映** | 9 項目＋2 初期化ボタンがロビー・ポーズ両方から動作し、表示言語（自動/日本語/English）・閃光軽減・カーソル倍率（100/125/150%）が即時反映されること。初回ヒント初期化でヒントが再表示され、設定初期化で墨晶や解放状態が消えずに設定だけ戻ること。P1 形式の旧 `save.txt`（新 3 キーなし）が破損扱いにならず既定値補完で正常ロードされること |
| **音響と見本帳** | 勝利画面（`BGM_VICTORY`）からエンドレス継続した際に `BGM_BATTLE` へ正常復帰すること。`EV_BOSS_PART_BREAK` の SE が鳴り、アセット欠損起動でもプロシージャル BGM＋ASCII フォントでエンドレスまでクラッシュなく動くこと |

### 2.4 バランス・テンポ確認と数値同期
- 多部位一筆龍（総 HP 1800：龍玉 300×3＋龍睛 900）の撃破所要時間が目標の 50〜80 秒帯に収まるか、およびエンドレス第 4〜6 幕の難易度カーブ（通常敵 HP ×1.25、ボス HP ×1.35、ノルマ +2）と新 4 札・解放 6 遺物の手触りを実機で確認する。
- 定義テーブルの数値を調整した場合は、`docs/game-design.md` / `docs/art-direction.md` の該当表を同じ値に更新し、`test_determinism_run`、`test_acts_enemies`、`test_roguelike_meta`、`bench_game` を再実行する。

### 2.5 設計書の最終同期と Phase 5 完了記録
- `docs/PLAN.md` §9.1 の Phase 5 行のステータスを完了（`[完了]`）に更新し、P5-Int の実績（性能測定値・Gate 通過）を反映する。
- `docs/game-design.md` §13 の「一筆龍の多部位版における頭の軌跡の履歴の持ち方と描画・演出の詳細」を解決済みとして更新する。
- `docs/architecture.md` と `docs/qa-and-testing.md` に、結合修正で生じた契約・テスト項目の差分があれば最終同期する。

---

## 3. 検証と Phase 5 完了ゲート

### 3.1 ローカル最小テストと Release 性能ベンチ
```sh
# 1. ASan/UBSan テスト（結合修正範囲と全主要テスト）
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan &&
    nix develop -c ctest --preset asan --output-on-failure \
        -R '^(test_determinism_run|test_acts_enemies|test_elements|test_roguelike_meta|test_audio|test_app_core|test_app_save|test_text_table|test_core_smoke)$'

# 2. Release ビルド・性能ベンチ・CLI スモーク
nix develop -c cmake --preset release &&
    nix develop -c cmake --build --preset release &&
    ./build/release/bench_fluid &&
    ./build/release/bench_game &&
    ./build/release/inkstorm --headless-smoke &&
    nix develop -c ./build/release/inkstorm --smoke-frames 300 &&
    nix develop -c ./build/release/inkstorm --no-post --smoke-frames 300 &&
    nix develop -c ./build/release/inkstorm --showcase --smoke-frames 30
```

### 3.2 WSLg 実機の P2 全要素＆エンドレス通し完走
隔離した一時セーブファイル（`--save-path "$tmp/save.txt"`）を使って以下を通しで確認し、結果を PR 本文に記録する：
```sh
tmp=$(mktemp -d)
nix develop -c ./build/release/inkstorm --save-path "$tmp/save.txt"
cat "$tmp/save.txt"
```
1. **拡充設定とセーブ互換の確認**：
   - ロビー →「設定」で表示言語（日本語 ↔ English）・閃光軽減 ON/OFF・カーソル倍率（100% → 125% → 150%）を即時切替し、画面文字とカーソルサイズが変わることを確認する。「初回ヒントを初期化」「設定を既定に戻す」の通知と永続化も確認する。
2. **本編 3 幕 → 多部位一筆龍 → エンドレス継続 → 終了 1 回精算**：
   - 墨庵で大遺物解放枠を購入 → 出陣 → 第 3 幕（狐符・追尾符・護符結界、多部位一筆龍の 3 龍玉破壊 → 点睛 → 昇龍 → 撃破）→ 勝利画面で未精算プレビューと「エンドレスへ進む」ボタンを確認 → 「エンドレスへ進む」を選択 → 第 3 幕ボス大遺物 3 択＆社 HP +3 → 第 4 幕（金砂子・周回 1 幕題字・強化された敵）へ突入 → 敗北（またはポーズからロビーへ戻る）で初めて 1 回だけ精算され、`save.txt` の `best_act >= 4`・`clears=1`・`runs_played=1`・`opt_lang`・`opt_reduce_flash`・`opt_cursor_scale` が正しく保存されていることを確認する。
3. **勝利画面から直接ロビーへ戻る経路の精算確認**：
   - 別ランで本編 3 幕勝利後に「ロビーへ戻る」を選び、遅延精算が 1 回だけ確定・保存されること（および勝利画面でウィンドウを閉じた場合も精算漏れしないこと）を確認する。

### 3.3 WSLg の 4 描画経路＋併用・アセット欠損確認・崩し試験
1. **経路 1〜4（通常 / `--no-post` / `shader` / `font` / `shader,font` 併用）**：`--showcase` の P2 追加サブページ（狐符・多部位龍・解放遺物・フォールバック 2 択・拡充設定・エンドレスリザルト）を全経路で確認し、`F12` キャプチャを保存する。
2. **アセット丸ごと欠損確認**：一時ディレクトリへ `./build/release/inkstorm` 単体をコピー（`assets/` なし）し、`--no-save` で起動して内蔵 ASCII フォント＋プロシージャル BGM でロビー → 拡充設定 → 戦闘がクラッシュなく動作することを確認する。
3. **崩し試験 4 種と `F1` 性能予算**：多部位一筆龍戦（第 1 フェーズ 3 龍玉＆1 玉破壊後）および第 4 幕エンドレス（金砂子）のキャプチャで崩し試験 4 種（グレースケール・5px ぼかし・25% 縮小・墨 6 割）を通し、多部位一筆龍戦の `F1` で **`render_draw < 4.0 ms/frame`・頂点数 `<= 60,000`・ブレンド切替 `<= 12`** を確認する。

### 3.4 完了条件（Phase 5 Gate）
- [ ] §2.1・§2.2 の確認がすべて注記どおりで、未承認の契約変更がない。
- [ ] CTest 全 15 件が CI の ASan/UBSan（`-Werror`）で全緑。
- [ ] **全 28 札・15 遺物（墨庵解放 6 種含む）・狐符＆追尾符・多部位一筆龍 3 フェーズ・エンドレスモード（遅延精算・周回スケール・金砂子・候補枯渇 2 択）・拡充設定を実ウィンドウで完走確認**した（§3.2）。
- [ ] **WSLg の 4 描画経路（併用 `shader,font` 含む）・アセット欠損起動・崩し試験 4 種**を通した（§3.3）。
- [ ] Release 性能予算（`fluid_step < 2.0 ms/tick`、`game_step < 4.0 ms/tick`、`render_draw < 4.0 ms/frame`）をすべて達成し、測定値を PR に記録した。
- [ ] `docs/PLAN.md` §9.1 の Phase 5 行に完了を記録し（§2.5）、Phase 6（仕上げ・デモ・配布検証）へ進める状態にした。
- [ ] PR 作成直後に `@coderabbitai review` を投稿し、全指摘を 4 分類トリアージして必要な修正と返信を完了した（[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md) 準拠）。
- [ ] **CI の 3 ジョブ（`build-asan-and-test`, `static-analysis`, `dist-verify`）がすべて緑**（配布 zip 展開先の素 `make` と `--headless-smoke` に `src/game/enemies/ofuda_fox.c` 等の新規ファイルが正常に収録・ビルド・検証されていること）。
