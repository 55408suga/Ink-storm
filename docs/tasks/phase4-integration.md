# Phase 4-Int タスク指示書：P1 本編の 3 並列合流・全 3 幕＆墨庵通し完走・Phase 4 完了ゲート

> **必読**: [phase4-pre-contracts.md](phase4-pre-contracts.md)、[phase4-roguelike-p1.md](phase4-roguelike-p1.md)、[phase4-acts-enemies-p1.md](phase4-acts-enemies-p1.md)、[phase4-ui-assets-p1.md](phase4-ui-assets-p1.md)、[PLAN.md](../PLAN.md) §9.1（Phase 4 完了ゲート）、[qa-and-testing.md](../qa-and-testing.md) §1〜§6、[art-direction.md](../art-direction.md) §14 / §15、[AGENTS.md](../../AGENTS.md)、[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md)、[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)、[inkstorm-demo-and-dist](../../.agents/skills/inkstorm-demo-and-dist/SKILL.md)
>
> **前提**: P4-Pre、P4-A（`feat/phase4-roguelike-p1`）、P4-B（`feat/phase4-acts-enemies-p1`）、P4-C（`feat/phase4-ui-assets-p1`）がすべて CI 全緑で `main` にマージ済みであること。各 PR の本文（ベンチ実測値、キャプチャ一覧、同梱フォント・外部 BGM のライセンス確認結果、数値調整、契約変更要求）を通読しておくこと。
> **作業場所 / ブランチ**: リポジトリルート（`full-scale-dev/Ink-storm`、作業ツリーがクリーンな状態）で最新の `main` から `feat/phase4-integration` を作り、結合修正と Phase 4 Gate の記録を **1 本の PR** にする。

---

## 0. 目的と禁止事項

3 並列で開発された P4-A（筆 3 種・爆雷/三元共鳴・24 札・9 遺物・ヒント発火）、P4-B（第 2/3 幕地形・山颪・霊柱・新敵 4 種・エリート・鵺・一筆龍簡易版）、P4-C（ロビー・筆選択・墨庵・設定・ポーズ・新描画・毛筆フォント・外部 BGM）を `main` 上で結合し、**「ロビー → 筆選択 → 全 3 幕（12 ウェーブ＋大遺物 2 回）完走 → リザルト墨晶精算 → 墨庵購入＆セーブ → 再出陣」** を実ウィンドウで実証して **Phase 4 完了ゲート（P1 本編完成）** を通過させる。

**禁止事項**
- Phase 5（P2 拡張：狐符、残り 4 札、解放遺物 6 種のラン中効果、エンドレスモード、一筆龍の多部位版）や Phase 6（`--demo` / `--snap` CLI）を先回りして実装しない。
- テストのしきい値や性能予算（`fluid_step < 2.0 ms/tick`, `game_step < 4.0 ms/tick`, `render_draw < 4.0 ms/frame`）を緩めない。
- 難易度・バランス調整は定義テーブルの数値変更のみで行い、変更した数値は `docs/game-design.md` / `docs/art-direction.md` に同期する。

---

## 1. 変更対象

| 対象 | 操作 | 内容 |
| :--- | :---: | :--- |
| `main` の 3 並列合流状態 | 確認 | §2.1 / §2.2 |
| `src/game/*`、`src/render/*`、`src/audio/*`、`src/app/*` | 最小限 | 3 並列結合時の不整合修正（§2.3） |
| 定義テーブル、`docs/game-design.md`、`docs/art-direction.md` | 数値のみ | 全 3 幕通し（6〜10 分目安）の難易度・テンポ調整（§2.4） |
| `docs/PLAN.md`、`docs/architecture.md`、`docs/qa-and-testing.md` | 更新 | Phase 4 完了の記録と最終同期（§2.5） |

---

## 2. 手順

### 2.1 3 並列合流状態の確認
```sh
git switch main && git pull --ff-only && git switch -c feat/phase4-integration

# P4-Pre のマージコミット SHA を取得
pre=$(gh pr view <P4-Pre の PR 番号> --json mergeCommit -q .mergeCommit.oid)

# Pre 以降の main の履歴（P4-A・P4-B・P4-C・承認済み契約変更の PR のみであること）
git log --first-parent --oneline "$pre"..main

# Pre 以降に凍結ファイルを変えたコミットの有無を確認
git log --oneline "$pre"..main -- CMakeLists.txt CMakePresets.json src/core src/fluid src/ecs \
    src/game/meta.h src/game/meta.c src/game/game.h src/game/game_ids.h src/game/game_events.h \
    src/game/game_components.h src/game/game_contract.c src/game/game_internal.h \
    src/game/game.c src/game/hash.c src/game/debug_api.c src/render/ui_view.h \
    tests/test_core_smoke.c tests/test_fluid.c tests/bench_fluid.c tests/test_ecs.c \
    tests/test_app_save.c tools .github flake.nix flake.lock .agents \
    docs/PLAN.md docs/architecture.md docs/qa-and-testing.md

# main の最新コミットに対する CI 3 ジョブがすべて緑であること
gh run list --branch main --limit 3
```

### 2.2 境界・アート規約・アセットの機械確認（いずれも結果が注記どおり）
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
P4-A・P4-B・P4-C が別々に開発されたため、結合状態で以下の連携を重点的に確認し、ずれがあれば修正する。

| 結合箇所 | 確認内容 |
| :--- | :--- |
| **筆 3 種 × 第 2・3 幕地形・霊柱** | 朱筆の火種・風斬りの炎と、細筆の帯電渦印が、第 2 幕の鳥居隙間（`x=384 / 896`）での流速加速や第 3 幕の霊柱（炎/雷 `> 0.4` で点灯）を正しく駆動すること |
| **爆雷・三元共鳴・風斬り × 新敵・エリート・ボス** | 爆雷と三元共鳴で石鬼（重量級）と停止中の鵺（`ai_state == 1`）が吹き飛び、さらに風斬りでも停止中の鵺を柱へ叩きつけて 60 ダメージ＋スタン 1.0 秒が入ること。一筆龍の点睛後（`phase_idx >= 1`）に三元共鳴のダメージが `×2.0` になること。エリート（撥墨・不燃・避雷）に対し、撥墨の墨印無効・避雷の連鎖除外・複合属性ダメージの「低い方の耐性」適用がすべて一致すること |
| **大遺物 9 種 × 戦闘・HUD・ポーズ・リザルト** | 第 1・2 幕ボス撃破後に社 HP `+3` → 1.0 秒余韻 → `REWARD_KIND_RELIC`（巻物の大遺物 3 択、修繕ボタンなし）が表示され、選択で `EV_REWARD_PICK`（`EVF_REWARD_RELIC`）が出て次幕の `ACT_TRANSITION`（巻物幕転換 2.0 秒）へ進むこと。取得した大遺物が HUD 右下・ポーズ画面・リザルト画面に正しく表示されること |
| **大遺物・強化札の代償＆フック × 流体・弾・社** | `業火の契り` で社最大 HP `-2`（HUD の灯火数も減少）、`朱の硯` / `残墨` 取得直後に `fluid.cfg.dye_damp[0]` が即変更されること、`天狗の羽団扇` で風斬りが 3 方向になり消費 `+6`・単体敵への重複吹き飛びがないこと、`連理の筆` で点対称に筆跡が描かれ消費 `×1.3`、`数珠の渦` で通常墨玉回収時に小渦が出ること、`結界 Lv3` で 12 秒ごとに未偏向敵弾 1 発のみ無効化（敵接触は非消費）されること、`返し風` で社接近弾（鎌風・跳弾雷球を含む）に自動風斬りが出ること |
| **ロビー・墨庵・設定・ポーズ・セーブ** | ロビー → 筆選択 → 出陣、ラン中の `Esc` ポーズ（再開 / 設定 / 最初から / ロビーへ戻る）、ポーズからの「最初から」「ロビーへ戻る」で敗北精算＆セーブされること、ロビー/墨庵からの終了で架空のランが精算されないこと、設定画面でポストシェーダを OFF→ON / ON→OFF の両方向へ即時切替できること、墨庵で恒久強化 4 種と遺物解放を購入すると墨晶が減ってセーブされ次の出陣の `RunConfig` に反映されること |
| **初回操作・反応ヒント** | 新規セーブ状態で第 1 幕を始めると、操作ヒント 4 種と 4 属性反応の初回発火時に上部の胡粉短冊が 2.0 秒間順次表示され（同時発火時も消失しない）、`seen_hints_mask` に保存されて 2 回目以降は出ないこと。設定で操作ヒント OFF にすると表示されないこと |
| **音（外部 BGM ＋ 新 SE ＋ ポーズ無音）** | ロビー（`BGM_LOBBY`）→ 通常戦（`BGM_BATTLE`）→ ボス戦（`BGM_BOSS`）→ 勝利（`BGM_VICTORY`、非ループ）で外部 OGG BGM がクロスフェード再生され、爆雷・三元共鳴・山颪予兆の SE と墨庵購入の落款 SE が鳴ること。風筆ドラッグ中に `Esc` ポーズしても筆擦れ音が鳴り続けないこと。アセットなし起動時にプロシージャル BGM へ無音落ちやクラッシュなく切り替わること |

### 2.4 全 3 幕通しの難易度・テンポ調整
- **目標テンポ**（game-design §3.1 / §10）：メタ強化なしの初期状態でも、筆の使い分けとビルド構築で全 3 幕（12 ウェーブ）を **約 6〜10 分** でクリアでき、第 2 幕で敗北したランで **60〜80 墨晶**、初回 3 幕クリアで **180〜230 墨晶** を獲得できるバランスにする。
- 併せて、石鬼に対する朱筆・細筆での風斬り単発（`560 × 0.35 = 196 px/s` vs 叩きつけ閾値 `215 px/s`）の体感や、第 2・3 幕の鳥居・岩柱・霊柱周りの移動の引っかかりやすさを実機で確認し、必要なら定義テーブルの数値のみ調整する。
- 定義テーブルの数値を調整した場合は、`docs/game-design.md` / `docs/art-direction.md` の該当表を同じ値に更新し、`test_determinism_run`、`test_acts_enemies`、`bench_game` を再実行する。

### 2.5 設計書の最終同期と Phase 4 完了記録
- `docs/PLAN.md` §9.1 の Phase 4 行のステータスを完了に更新し、P4-Int の実績（性能測定値・Gate 通過）を反映する。
- `docs/architecture.md` と `docs/qa-and-testing.md` に、結合修正で生じた契約・テスト項目の差分があれば最終同期する。
- `docs/game-design.md` §13 の「毛筆体フォント・外部 BGM 音源の選定」など Phase 4 で確定した保留項目を解決済みに更新する。

---

## 3. 検証と Phase 4 完了ゲート

### 3.1 ローカル最小テストと Release 性能ベンチ
```sh
# 1. 本タスクで修正した範囲の ASan テスト（render/audio/app を直した場合は該当テストも対象に含める）
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan &&
    nix develop -c ctest --preset asan --output-on-failure \
        -R '^(test_determinism_run|test_acts_enemies|test_elements|test_roguelike_meta|test_audio|test_app_core|test_text_table|test_core_smoke)$'

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

### 3.2 WSLg 実機の全 3 幕＆墨庵通し完走
隔離した一時セーブファイル（`--save-path "$tmp/save.txt"`）を使って以下を通しで確認し、結果を PR 本文に記録する：
```sh
tmp=$(mktemp -d)
nix develop -c ./build/release/inkstorm --save-path "$tmp/save.txt"
cat "$tmp/save.txt"
```
1. **ラン 1（全 3 幕クリア通し）**:
   - タイトル / ロビー（流体背景で墨描き確認）→「出陣」→ 筆スタイル選択（朱筆または細筆を選択）→ 第 1 幕（初回ヒント短冊の表示確認、3 ウェーブ＋大天狗撃破）→ 大遺物 3 択 → 第 2 幕（巻物幕転換、鳥居と岩柱での叩きつけ、エリート撃破、鵺撃破）→ 大遺物 3 択 → 第 3 幕（山颪予兆、霊柱点灯、爆雷・三元共鳴発火、一筆龍の点睛と撃破）→ 勝利リザルト（墨晶内訳・取得札・取得大遺物・シード確認）→「ロビーへ戻る」。
2. **墨庵購入とセーブ永続化**:
   - ロビーの「墨庵」に入り、獲得した墨晶で恒久強化（例：「大きな硯」または「社の加護」）と大遺物解放を購入し、一度ロビーから「終了」して `save.txt` に購入結果・`seen_hints_mask`・`clears=1`・`runs_played=1`（ロビー終了で二重計上されないこと）が保存されていることを確認する。
3. **ラン 2（再起動後のロード・別筆スタイル・ポーズ中断精算）**:
   - 同じ `--save-path "$tmp/save.txt"` で再起動し、墨庵の強化が反映された状態で別の筆スタイルで出陣 → 戦闘中に `Esc` でポーズメニューを開き、「設定」変更（ポストシェーダ切替・音量・画面揺れ）→ ポーズから「ロビーへ戻る」（確認ダイアログで「はい」）を選び、敗北として精算・保存（`runs_played=2`）されることを確認する。

### 3.3 WSLg の 4 描画経路＋併用・アセット欠損確認
1. **経路 1（通常）**：§3.2 の完走と、`--showcase` 全 6 ページの `F12` キャプチャ。
2. **経路 2（`--no-post`）**：`--showcase --no-post` 全 6 ページと戦闘確認。
3. **経路 3（シェーダ失敗）**：`INKSTORM_DEBUG_FAULTS=shader` でログと直接描画フォールバック確認。
4. **経路 4（フォント欠損・併用）**：`INKSTORM_DEBUG_FAULTS=font` および `INKSTORM_DEBUG_FAULTS=shader,font` でログと英語 ASCII フォールバック・落款枠のみ押印を確認。
5. **アセット丸ごと欠損確認**：一時ディレクトリへ `./build/release/inkstorm` 単体をコピー（`assets/` なし）し、`--no-save` で起動して、明朝体・毛筆体・外部 BGM がすべて欠損した状態でも内蔵 ASCII フォントとプロシージャル BGM にフォールバックしてロビー → 出陣 → 戦闘がクラッシュなく遊べることを確認する。
6. **崩し試験と `F1` 性能予算**：見本帳（特に第 2・3 幕登場物と墨 6 割ページ）および第 3 幕一筆龍戦のキャプチャで崩し試験 4 種（グレースケール・5px ぼかし・25% 縮小・墨 6 割）を通し、一筆龍戦の `F1` で `render_draw < 4.0 ms/frame`・頂点数 `<= 60,000`・ブレンド切替 `<= 12` を確認する。

### 3.4 完了条件（Phase 4 Gate）
- [ ] §2.1・§2.2 の確認がすべて注記どおりで、未承認の契約変更がない。
- [ ] CTest 全 15 件（`test_acts_enemies` を含む）が CI の ASan/UBSan（`-Werror`）で全緑。
- [ ] **実ウィンドウでロビー → 筆選択 → 全 3 幕（大天狗・鵺・一筆龍）クリア → 墨庵購入・セーブ → ポーズ中断精算を通しで完走**した（§3.2）。
- [ ] **WSLg の 4 描画経路（併用 `shader,font` 含む）** と **アセットなし起動（フォント・外部 BGM 欠損フォールバック）** を確認し、アートのセルフチェックを通した（§3.3）。
- [ ] Release 性能予算（`fluid_step < 2.0 ms/tick`、`game_step < 4.0 ms/tick`、`render_draw < 4.0 ms/frame`）をすべて達成し、測定値を PR に記録した。
- [ ] **CI の 3 ジョブ（`build-asan-and-test`, `static-analysis`, `dist-verify`）がすべて緑**（配布 zip 展開先の素 `make` と `--headless-smoke` に新規フォント・BGM アセットが正常に収録・検証されていること）。
- [ ] `docs/PLAN.md` §9.1 の Phase 4 行に完了を記録し（§2.5）、Phase 5（P2 拡張）へ進める状態にした。
