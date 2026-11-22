# Phase 5-C タスク指示書：P2 設定画面拡充・エンドレス UI/演出・狐符＆多部位一筆龍描画・音響・文字列表（`wt-ui-settings-p2`）

> **必読**: [phase5-pre-contracts.md](phase5-pre-contracts.md)（特に §0.1 の進め方、§1.2 の排他所有表、§2 の凍結契約）、[PLAN.md](../PLAN.md) §9.1、[architecture.md](../architecture.md) §1 / §7〜§9、[art-direction.md](../art-direction.md) §5 / §6 / §9〜§15、[game-design.md](../game-design.md) §3.5 / §3.6 / §8.1 / §8.2 / §8.7 / §9.4 / §9.5 / §10.1 / §11.1〜§11.6、[qa-and-testing.md](../qa-and-testing.md) §2 / §4 / §5、[AGENTS.md](../../AGENTS.md)、[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md)、[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md)
>
> **worktree / ブランチ**: `full-scale-dev/wt-ui-settings-p2` / `feat/phase5-ui-settings-p2`（P5-Pre マージ後の `main` から分岐）。完了したら単独で `main` へ PR を出す。
> **並列ペア**: P5-A（[phase5-roguelike-p2.md](phase5-roguelike-p2.md)）、P5-B（[phase5-endless-ryu-p2.md](phase5-endless-ryu-p2.md)）。合流と完了ゲートは P5-Int（[phase5-integration.md](phase5-integration.md)）。

---

## 0. 目的と責務境界

P2 拡張の全 UI・水墨描画・音響・アプリ遷移を完成させる。

1. **設定画面拡充**（game-design §11.2、meta.h 凍結契約）：
   - 既存 6 項目に加え、**表示言語**（`自動 / 日本語 / English`）、**閃光軽減**（`ON / OFF`）、**カーソル倍率**（`100% / 125% / 150%`）の 3 設定項目と、**初回ヒントを初期化**・**設定を既定に戻す** の 2 ユーティリティ操作を追加し、すべて即時反映・保存・フィードバック通知する。
2. **エンドレス UI・遅延勝利精算・金砂子・フォールバック 2 択**（game-design §3.5 / §3.6 / §10.1、art-direction §6）：
   - **遅延勝利精算（重要）**：本編 3 幕クリア直後の `RUN_PHASE_VICTORY` では `meta_settle_run` を呼ばず内訳プレビュー（`ui.settle_preview = true`）を表示する。「エンドレスへ進む」を選んだら未精算のまま `CMD_START_ENDLESS` で第 3 幕ボス大遺物 3 択 → 第 4 幕へ進み、「ロビーへ戻る」「再出陣」または勝利画面でのウィンドウ終了時に初めて 1 回だけ精算・保存する。
   - **フォールバック 2 択報酬 UI**：`REWARD_KIND_FALLBACK` で「社を修繕（+1 / +3・超過分 1 につき墨晶 +15）」と「墨晶 +10 / +30（`CMD_PICK_ENDLESS_CRYSTAL`）」の 2 カードを表示・操作可能にする。
   - **第 4 幕以降の情景と HUD**：`act_index % 3` で地色・背景・地形を循環し、画面端に周回数に応じた **金砂子（`min(20 * n, 120)` 粒）** を加える。幕転換題字・HUD 幕名・二段風斬り 2 チャージピップ・多部位一筆龍の合計 HP バー＋3 龍玉状態印・解放 6 遺物の紋章を描く。
3. **狐符・護符結界・追尾符・多部位一筆龍の水墨描画・VFX・SE・文字列表完成**：
   - `TEXT_P2_STUB` を削除し、P2 全コンテンツの日本語／英語（印字可能 ASCII）文字列を完成する。
   - `ENEMY_OFUDA_FOX`・`StatusComp.barrier_t` 結界・`PROJ_HOMING_OFUDA`・`RyuBodyState.trail` 参照の多部位一筆龍（破壊龍玉区間の `dry` かすれ強化＋独立 `ENEMY_BOSS_RYU_ORB`＋`EV_BOSS_PART_BREAK` 破砕 VFX＆SE）を描画・合成する。

**禁止事項**
- P5-Pre §1.2 の凍結ファイル（`CMakeLists.txt`, `ui_view.h`, `src/game/*`, `assets/*`, `AGENTS.md`, `tools/*` 等）および P5-A / P5-B の所有ファイルを編集しない。
- `render` / `audio` から `GameState` を変更しない。`g->internal` を読まない。
- `src/audio/*`, `src/app/save.*`, `paths.*`, `cli.*`, `frame_clock.*`, `input_latch.*`, `src/render/text_table.*` に `raylib.h` を include しない。
- 完成形の描画に raylib の均一図形描画関数や RGB 直書き（`palette.c` 以外）を使わない（art-direction §8 / §13 / §14.4）。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/render/text_table.h` / `text_table.c` | 更新 | `TEXT_P2_STUB` 削除、狐符・龍玉、残り 4 札×Lv1..3、解放 6 遺物（名・効果・代償）、設定拡充 5 項目＋通知 2 種、エンドレス幕・フォールバック墨晶・リザルト「エンドレスへ進む」＆エンドレス/巻物ボーナス行の JA/EN 文字列完成（§2.1） |
| `src/render/render.h`、`render_internal.h`、`fonts.c`、`render.c`、`palette.c`、`paper.c` | 更新 | `SettingsLayout` 9 行＋2 操作ボタン＋戻る、`RewardLayout.crystal_rect`、`ResultLayout.endless_rect`、`render_set_lang_mode`、`act_index % 3` 地色/背景/地形循環＋エンドレス金砂子 `min(20*n, 120)` 粒（§2.2 / §2.3） |
| `src/render/ui_settings.c`、`ui_reward.c`、`ui_result.c`、`ui_banner.c`、`hud.c`、`vfx.c`、`draw_entities.c` | 更新 | 設定画面拡充 UI、フォールバック 2 択＆新 6 遺物紋章、リザルト 3 ボタン＆プレビュー＆エンドレス/ボーナス行、第 4 幕以降題字/HUD・二段風斬り 2 ピップ・カーソル倍率・多部位一筆龍合計 HP バー＆3 龍玉印、`reduce_flash` 明滅抑止、`EV_BOSS_PART_BREAK` VFX、狐符・結界・追尾符・`ryu_body.trail` 参照多部位龍＆独立龍玉描画（§2.2〜§2.4） |
| `src/audio/audio.h` / `audio.c`、`synth_voices.h` / `synth_voices.c` | 更新 | `AUDIO_VOICE_PART_BREAK`（`EV_BOSS_PART_BREAK` 玉破砕 SE）、エンドレス開始時の BGM 復帰（`BGM_VICTORY → BGM_BATTLE`）（§2.5） |
| `src/app/app_loop.c`、`input_map.h` / `input_map.c`、`input_latch.h` / `input_latch.c`、`showcase.c` | 更新 | 遅延勝利精算＆`MENU_ACT_RESULT_ENDLESS`、設定拡充アクション（`MENU_ACT_SETTINGS_RESET_HINTS` / `MENU_ACT_SETTINGS_RESET_DEFAULTS`）・起動時 `render_set_lang_mode` 適用、`RawInput.crystal_pressed` / `InputLatch.crystal_pending`（`CMD_PICK_ENDLESS_CRYSTAL`）、見本帳 P2 拡充（§2.2 / §2.3 / §2.6） |
| `README.txt` | 更新 | エンドレスモード・拡充設定・28 札/15 遺物・狐符/多部位龍の操作案内追記（§2.6） |
| `tests/test_text_table.c`、`tests/test_audio.c`、`tests/test_app_core.c` | 更新 | P2 全文字列網羅（28 札・15 遺物・12 敵・設定/エンドレス文言）、`EV_BOSS_PART_BREAK` SE＆エンドレス BGM 遷移、`CMD_PICK_ENDLESS_CRYSTAL` ラッチの単体テスト追加（§3） |
| `docs/art-direction.md` | 数値のみ | 見本帳で微調整した太さ・不透明度等の同期 |

---

## 2. 実装仕様

### 2.1 文字列表の完成（`src/render/text_table.h` / `text_table.c`）

P5-Pre で仮置きした `TEXT_P2_STUB` を削除し、以下をすべて JA（日本語）および EN（印字可能 ASCII `0x20〜0x7E` のみ）で登録する：
- **敵 2 種**：`ENEMY_OFUDA_FOX`（「狐符」 / `"Ofuda Fox"`、紹介文：「味方に結界を張り、追尾符を放つ」）、`ENEMY_BOSS_RYU_ORB`（「龍玉」 / `"Dragon Orb"`、紹介文：「一筆龍が宿す属性の核」）。
- **強化札 4 種 × (名 + Lv1..3)**：`UPG_LINGERING_EMBERS`（残り火）、`UPG_STORM_BRUSH`（雷雲の筆）、`UPG_DOUBLE_GALE`（二段風斬り）、`UPG_INKSTONE_BLESSING`（硯の恵み）。`TEXT_UPG_BARRIER_LV3` の直後に 16 エントリを連続配置すれば既存の `TEXT_UPG_EMBER_TIP_NAME + (int)id * 4` 式が 28 種すべてでそのまま成立する。
- **解放遺物 6 種（名・効果・代償）**：`RELIC_SEAL_OF_TRINITY`（三元の印・代償なし）、`RELIC_INKSTORM_SECRET`（墨嵐の極意・代償：「墨昇華がチェイン×3から」）、`RELIC_THUNDERFIRE_GEM`（雷火の宝珠・代償なし）、`RELIC_ONI_TILE`（鬼瓦・代償なし）、`RELIC_ENDLESS_BRUSH`（無尽の筆・代償：「墨ゲージ上限 -40」）、`RELIC_NIGHT_PARADE_SCROLL`（百鬼夜行の巻物・代償：「増援間隔×0.75・同時上限+2」）。
- **設定画面拡充文字列**：`TEXT_SETTINGS_LANG`（「表示言語」）、`TEXT_SETTINGS_LANG_AUTO`（「自動」）、`TEXT_SETTINGS_LANG_JA`（「日本語」）、`TEXT_SETTINGS_LANG_EN`（「English」）、`TEXT_SETTINGS_REDUCE_FLASH`（「閃光軽減」）、`TEXT_SETTINGS_CURSOR_SCALE`（「カーソル倍率」）、`TEXT_SETTINGS_RESET_HINTS`（「初回ヒントを初期化」）、`TEXT_SETTINGS_RESET_DEFAULTS`（「設定を既定に戻す」）、`TEXT_SETTINGS_NOTICE_HINTS`（「初回ヒントを初期化しました」）、`TEXT_SETTINGS_NOTICE_DEFAULTS`（「設定を既定に戻しました」）。
- **エンドレス・フォールバック・リザルト文字列**：`TEXT_REWARD_CRYSTAL_TITLE`（「墨晶を得る」）、`TEXT_REWARD_REPAIR_EXCESS`（「超過分 +15 墨晶/HP」）、`TEXT_RESULT_ENDLESS`（「エンドレスへ進む」）、`TEXT_RESULT_CRYSTAL_ENDLESS`（「エンドレス」）、`TEXT_RESULT_CRYSTAL_BONUS`（「巻物ボーナス」）、`TEXT_ENDLESS_ACT_PREFIX`（第 4 幕以降の幕名合成用）。
- `text_act_title(0..2)` / `text_act_short(0..2)` の既存契約（`0..2` 非 `TEXT_NONE`、範囲外 `TEXT_NONE`）は維持し、第 4 幕以降は `act_index % 3` の副題と幕番号/周回番号からフォーマットする。

---

### 2.2 設定画面拡充と即時反映（`ui_settings.c`, `render.h`, `fonts.c`, `vfx.c`, `hud.c`, `input_map.*`, `app_loop.c`）

1. **`SettingsLayout`（`render.h` / `ui_settings.c`）**:
   - 設定 9 行（`rows[9]`, `minus_rects[9]`, `plus_rects[9]`）＋ ユーティリティボタン 2 個（`reset_hints_rect`, `reset_defaults_rect`）＋ 戻るボタン（`back_rect`）の計 12 フォーカス項目を `panel` 内に収める（例：パネル `y = 44..686`、9 行を `row_h = 42px`, `gap = 6px`、下段に 2 ユーティリティ横並び、最下段に「戻る」＋右脇に `settings_notice_t > 0` の確認短文）。
   - 行の対応：`0` マスター音量、`1` BGM、`2` SE、`3` ポストシェーダ、`4` 画面揺れ、`5` 操作ヒント、`6` 表示言語（`AUTO / JA / EN`）、`7` 閃光軽減（`ON / OFF`）、`8` カーソル倍率（`100% / 125% / 150%`）、`9` 初回ヒントを初期化、`10` 設定を既定に戻す、`11` 戻る。
2. **言語切替（`fonts.c` / `render.h`）**:
   - `void render_set_lang_mode(RenderState *r, UiLangMode mode);` を公開する。
   - `bool want_ja = (mode == UI_LANG_EN) ? false : r->custom_fonts;`（フォント欠損時は `UI_LANG_JA` 指定でもクラッシュせず安全に英語 ASCII を維持する）。
   - `r->use_ja != want_ja` のとき `r->use_ja = want_ja;` とし、`memset(r->text_size_valid, 0, sizeof r->text_size_valid);` で計測キャッシュを破棄する。
   - `app_loop.c` の起動時（`render_init` 直後）および設定変更時に必ず `render_set_lang_mode(render, (UiLangMode)st->ui_lang)` を呼ぶ。
3. **閃光軽減（`vfx.c`）**:
   - `render_draw` で `ui->reduce_flash` が真のときは画面全体の胡粉明滅オーバーレイ（`r->flash_t`）を描画しない（`trigger_flash` も抑止可能）。墨・電撃・円相などの通常 VFX は維持する。
4. **カーソル倍率（`hud.c`）**:
   - `const float cs = (ui != NULL && ui->cursor_scale >= 100 && ui->cursor_scale <= 150) ? ((float)ui->cursor_scale * 0.01f) : 1.0f;` を用い、筆先カーソル・周囲の墨リング半径・下の渦印/風斬り準備ピップ位置と大きさを `cs` 倍で描画する。
5. **初期化操作（`input_map.*` / `app_loop.c`）**:
   - `MenuAction` に `MENU_ACT_SETTINGS_RESET_HINTS`、`MENU_ACT_SETTINGS_RESET_DEFAULTS`、`MENU_ACT_RESULT_ENDLESS` を追加する。
   - `MENU_ACT_SETTINGS_RESET_HINTS`：`ctx.meta.seen_hints_mask = 0u;`、`audio_trigger_ui_stamp(audio)`、`ctx.ui.settings_notice_kind = 1; ctx.ui.settings_notice_t = 1.8f;`、`sync_meta_view`＋`persist_meta`。
   - `MENU_ACT_SETTINGS_RESET_DEFAULTS`：`ctx.meta.settings = meta_settings_default();`（墨晶・恒久強化・解放遺物・記録・既読ヒントは一切消さない）、音量・ポスト・言語・揺れ・閃光・カーソル倍率を即時再適用、`audio_trigger_ui_stamp`、`settings_notice_kind = 2; settings_notice_t = 1.8f;`、`sync_meta_view`＋`persist_meta`。
   - `settings_notice_t` は設定画面フレームごとに減衰させる。

---

### 2.3 エンドレス UI・遅延勝利精算・金砂子・フォールバック 2 択

1. **遅延勝利精算と「エンドレスへ進む」（`app_loop.c`, `ui_result.c`, `input_map.c`）**:
   - **`app_after_tick`（重要）**:
     - `game->phase == RUN_PHASE_VICTORY && !game->run.run_settled && game->run.endless_loop == 0` のときは **`meta_settle_run` を呼ばない**。代わりに `MetaRunResult res = game_run_result(game); ctx->ui.settle = meta_calc_crystals(&res); ctx->ui.settle_preview = true; ctx->ui.settled = false;` とし、`ctx->ui.crystals_after` を現在墨晶＋プレビュー合計（上限 `META_MAX_CRYSTALS`）に設定する。
     - `RUN_PHASE_GAME_OVER`（本編敗北・エンドレス終了・中断）および `RUN_PHASE_VICTORY && game->run.endless_loop > 0`（エンドレス上限到達）では、従来どおり即座に `meta_settle_run`＋`persist_meta` を実行する（`ui.settled = true; ui.settle_preview = false;`）。
   - **確定精算ポイント（漏れ防止）**:
     - 共通ヘルパ `settle_victory_if_pending(ctx, game)` を用意し、**`MENU_ACT_RESULT_LOBBY`、`MENU_ACT_RESULT_RETRY`、および `app_finish_run_on_exit`（勝利画面のままウィンドウを閉じた場合）** の 3 か所で必ず呼び、`game->phase == RUN_PHASE_VICTORY && !game->run.run_settled` なら `meta_settle_run`＋`persist_meta` を確定させる。
   - **`MENU_ACT_RESULT_ENDLESS`**:
     - `render->victory_scroll_t >= 0.75f` かつ `game->phase == RUN_PHASE_VICTORY && !game->run.run_settled && game->run.endless_loop == 0` のとき有効。
     - `const GameCommand cmd = {.type = CMD_START_ENDLESS, .arg = 0}; game_step(game, NULL, &cmd);` を実行し、`ctx->ui.settle_preview = false; ctx->ui.settled = false;`、`render->victory_scroll_t = 0.0f;`、`audio_consume_events` / `render_consume_events` を通して大遺物報酬画面（`RUN_PHASE_REWARD`）へ遷移する。
   - **`ui_result.c` / `ResultLayout`**:
     - `render_result_layout(bool show_endless, ResultLayout *out)`: `show_endless` が真のときは下部 3 ボタン横並び（`endless_rect` [「エンドレスへ進む」・朱枠強調]、`lobby_rect`、`retry_rect`）、偽のときは従来どおり 2 ボタン。
     - 墨晶内訳の右列は `ui->settled || ui->settle_preview` で表示し、`st.from_endless > 0` なら「エンドレス」行、`st.from_bonus > 0` なら「巻物ボーナス」行を追加表示する。到達幕が第 4 幕以上なら周回表示（例 `Act 4-2 (Loop 1)` / `第4幕 2/3`）にする。
2. **フォールバック 2 択報酬 UI と入力ラッチ（`ui_reward.c`, `input_latch.*`, `input_map.*`）**:
   - `RawInput` に `bool crystal_pressed;`、`InputLatch` に `bool crystal_pending;` を追加する。`input_latch_take_command` で `RUN_PHASE_REWARD` 時に `crystal_pending` が立っていれば `CMD_PICK_ENDLESS_CRYSTAL` を返す。
   - `RewardLayout` に `Rectangle crystal_rect; bool has_crystal;` を追加する。`rw->kind == REWARD_KIND_FALLBACK` のとき、画面中央に 2 枚のカード（左 `repair_rect`：「社を修繕 +1」または「社を修繕 +3（超過 +15 墨晶/HP）」、キー `1` / `4` / クリック；右 `crystal_rect`：「墨晶 +10」または「墨晶 +30」、キー `2` / クリック、`UI_HOVER_CRYSTAL`）を並べて描く。
   - `draw_relic_emblem` に解放 6 遺物（`RELIC_SEAL_OF_TRINITY`〜`RELIC_NIGHT_PARADE_SCROLL`）それぞれの水墨紋章を追加する。
3. **エンドレス情景（金砂子）・幕題字・HUD 拡張（`palette.c`, `paper.c`, `ui_banner.c`, `hud.c`）**:
   - `pal_paper_for_act(act_index)` と `paper.c` の背景・地形描画を `stage_act = act_index >= 0 ? (act_index % GAME_MAIN_ACT_COUNT) : 0` へ更新する。
   - **金砂子（art-direction §6）**：`paper.c` の背景キャッシュ生成（`bg_cached_act != act_index` で再生成）で、`loop_n = act_index / GAME_MAIN_ACT_COUNT` が `>= 1` のとき、画面端 120px 帯の中へ決定的シードから `flecks = min(20 * loop_n, 120)` 粒の金砂子（`PAL_KIN` / `PAL_KIN_HOT` の小さな非対称多角形片、不透明度 `0.22〜0.34`）を散らす（`bg_cached_act` が幕番号そのものを持つため第 4 幕・第 7 幕で自動更新される）。
   - `ui_banner.c`：幕転換で `act_index >= 3` のとき「第 N 幕（周回 M）＋副題」を毛筆体 44px 以上で描く。
   - `hud.c`：
     - 上中央の幕名を `act_index >= 3` なら `第%d幕` / `Act %d` で表示。
     - カーソル下の風斬り準備ピップ：`g->player.gale_max_charges >= 2` のときは 2 個の並んだ墨ピップ（チャージ数に応じて濃墨／補充中は清墨塗り上げ）を描画する。
     - **多部位一筆龍のボス HP バー**：`g->ryu_body.head_id.index == g->run.boss_id.index && g->run.boss_id.index != ECS_INVALID_INDEX` のとき、分子に「龍睛 HP ＋ 生存中 3 龍玉 HP の合計」、分母に「龍睛 max_hp ＋ 3 龍玉 max_hp 相当（破壊済みは 1 玉あたり `head_max_hp / 3` で復元）」を表示し、HP バーの直下に **3 つの龍玉印（炎＝朱・雷＝蒼・風＝胡粉、破壊済みは清墨の欠け円）** を並べて現在の第 1 / 第 2 / 第 3 フェーズを一目で読めるようにする。

---

### 2.4 狐符・護符結界・追尾符・多部位一筆龍の水墨描画と VFX（`draw_entities.c`, `vfx.c`）

art-direction §10.1 / §10.3 / §10.5 / §10.6 / §11 に厳密準拠し、全登場物に紙抜き縁 `ink_knockout` を先に描く。

1. **狐符（`ENEMY_OFUDA_FOX`、`w_max = 3px`）**:
   - 胡粉（`PAL_GOFUN`）の狐面（尖った耳と細目・朱の目元ひと刷毛）＋濃墨の輪郭＋筆の払いのように揺れる大きな尾（`ink_stroke`）を描く。結界付与直前・射撃予兆は面の周りに淡い符を浮かべる。
2. **護符結界（`StatusComp.barrier_t > 0.0f`）**:
   - 結界が付いている敵の周囲に、胡粉と淡墨の細い結界弧（`ink_arc_stroke` 2 片＋小さな胡粉短冊 2 枚）を緩やかに回転描画し、ダメージ半減中であることを視認できるようにする。
3. **追尾符（`PROJ_HOMING_OFUDA`、`w_max = 3px`）**:
   - 進行方向へ傾く胡粉の短冊に濃墨の細い文字線（短い払い 2〜3 本）を描く。周囲の炎濃度が高いときは端が朱（`PAL_SHU`）に焦げる演出を加える。
4. **多部位一筆龍（`ENEMY_BOSS_RYU` ＋ `ENEMY_BOSS_RYU_ORB`）**:
   - **胴の軌跡**：`g->ryu_body.head_id.index == id.index && g->ryu_body.trail_count >= 2` のときは、`RenderState` 側の旧 24 点バッファではなく **`g->ryu_body.trail` から弧長等間隔に 24 点を再サンプル**して 1 本の `ink_stroke`（`w_max = 26px`、尾 30% は `dry = 0.6`）を描く（簡易版フォールバック時も既存 24 点バッファがそのまま動く）。
   - **破壊龍玉区間のかすれ強化（art-direction §10.5）**：`g->ryu_body.orb_broken_mask` の各ビット（0=頭側 0..33%、1=胴中央 33..66%、2=尾側 66..100%）が立っている区間では、かすれ `dry` を `+0.35` 上乗せした上書きかすれ線・鱗の欠けで「線が途切れがちになる」表現を行う。
   - **独立龍玉（`ENEMY_BOSS_RYU_ORB`）**：`boss->part_id` に応じて宝珠形＋火焔縁を描く（`0`=朱＋`PAL_SHU_HOT`、`1`=蒼＋`PAL_AO_HOT`、`2`=胡粉＋淡墨の渦文）。`boss->telegraph > 0.01f` の間は内側から明色で強く発光させる。
   - **点睛（`phase_idx >= 1`）と昇龍（`phase_idx >= 2`）**：点睛後は目に大きな朱の点と一段濃い焦墨輪郭、昇龍（第 3 フェーズ）では角・髭に金明（`PAL_KIN_HOT`）の微かな龍気ハイライトを添える。
5. **龍玉破壊 VFX（`vfx.c`）**:
   - `EV_BOSS_PART_BREAK` 消費時に、破壊された龍玉の属性色（`0`=朱, `1`=蒼, `2`=胡粉）で `ink_splatter`＋欠けた円相 `VfxStroke`＋小シェイクを生成する。

---

### 2.5 音響拡張（`src/audio/audio.h`, `audio.c`, `synth_voices.h/.c`）

- `AudioVoiceKind` に `AUDIO_VOICE_PART_BREAK` を追加し、`EV_BOSS_PART_BREAK` 消費時に澄んだ鈴・破砕音＋中太鼓の複合和楽器 SE を発音する。
- エンドレス継続で `RUN_PHASE_VICTORY → RUN_PHASE_REWARD`（`g->run.endless_loop > 0`）へ戻った際、`audio_consume_events` が `BGM_VICTORY` に張り付かず `BGM_BATTLE` へ適切にクロスフェード復帰することを確認・整備する。

---

### 2.6 見本帳 `--showcase` の P2 拡充と `README.txt` 更新

- **`showcase.c`**：既存 6 ページを維持しつつ、`Tab`（`ui.showcase_subpage`）で P2 新規要素をすべて確認できるように拡充する：
  - ページ 1：`Tab` 切替で **狐符・護符結界・追尾符・多部位一筆龍（3 龍玉付き＋1 玉破壊かすれ）** を表示。
  - ページ 3：`Tab` 切替に **解放 6 遺物を含む大遺物 3 択** と **エンドレス候補枯渇 2 択（修繕 vs 墨晶）** を追加。
  - ページ 5：`Tab` 切替に **拡充された設定画面（9 項目＋2 初期化ボタン）** を追加。
  - ページ 6：`Tab` 切替で **勝利リザルト（3 ボタン「エンドレスへ進む」＆プレビュー内訳）** と **エンドレス終了リザルト（エンドレス/巻物ボーナス行）** を切り替え表示。
- **`README.txt`**：P2 拡張（強化札 28 種・大遺物 15 種・狐符・多部位一筆龍・エンドレスモード・拡充設定の言語切替/閃光軽減/カーソル倍率/初期化）の操作案内を追記する。

---

## 3. テスト要件

1. **`tests/test_text_table.c`**:
   - 全 12 種の `EnemyDefId`、全 28 種の `UpgradeId` × Lv1..3、全 15 種の `RelicId`（名前・効果・代償 7 種：初期 4＋解放 3）、新設定項目・通知・エンドレス/リザルト文字列がすべて非 `TEXT_NONE`・非空で、英語が印字可能 ASCII（`0x20〜0x7E`）のみであることを検証する。
2. **`tests/test_audio.c`**:
   - `EV_BOSS_PART_BREAK` 消費で有効な波形（ピーク `>= 1000`、`<= 32767`、`audio_validate_finite` 真）が合成されること。
   - `RUN_PHASE_VICTORY`（`BGM_VICTORY`）から `RUN_PHASE_REWARD`（エンドレス継続）へ移ったときに `audio_bgm_track` が `BGM_BATTLE` へ復帰すること。
3. **`tests/test_app_core.c`**:
   - `RawInput.crystal_pressed` が `RUN_PHASE_REWARD` で `CMD_PICK_ENDLESS_CRYSTAL` を 1 回だけ生成し、他フェーズでは発火しないこと、`input_latch_reset_for_ui_transition` で `crystal_pending` もクリアされることを検証する。

---

## 4. ローカル検証・視覚確認・PR と完了条件

```sh
# 1. ASan/UBSan: 変更範囲のビルドと関連テスト
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan --target inkstorm ink_render ink_audio \
        test_app_core test_audio test_app_save test_text_table test_core_smoke &&
    nix develop -c ctest --preset asan --output-on-failure \
        -R '^(test_app_core|test_audio|test_app_save|test_text_table|test_cli_.*|test_core_smoke)$'

# 2. Release と WSLg 描画経路（通常 / --no-post / shader 失敗 / font 欠損 / 併用）
nix develop -c cmake --preset release && nix develop -c cmake --build --preset release &&
    ./build/release/inkstorm --headless-smoke &&
    nix develop -c ./build/release/inkstorm --smoke-frames 30 &&
    nix develop -c ./build/release/inkstorm --showcase --smoke-frames 30 &&
    nix develop -c ./build/release/inkstorm --showcase --no-post --smoke-frames 30
mkdir -p build/snaps
nix develop -c ./build/release/inkstorm --showcase
nix develop -c ./build/release/inkstorm --showcase --no-post
INKSTORM_DEBUG_FAULTS=shader nix develop -c ./build/release/inkstorm --showcase
INKSTORM_DEBUG_FAULTS=font nix develop -c ./build/release/inkstorm --showcase
INKSTORM_DEBUG_FAULTS=shader,font nix develop -c ./build/release/inkstorm --showcase

# 3. 境界・アート規約・排他所有の機械確認
grep -rn 'raylib.h' src/audio src/app/save.* src/app/paths.* src/app/cli.* src/app/frame_clock.* \
    src/app/input_latch.* src/render/text_table.* src/render/ui_view.h                          # 0 件
grep -rnE '(->|\.)internal\b' src/render src/audio src/app                                     # 0 件
grep -rnE 'Draw(Line|Circle|Rectangle|Ring|Poly|TriangleStrip|Spline)' src/render              # xray.c・ink_prims.c のみ
grep -rnE '\(Color\)\{' src/render src/app                                                     # palette.c のみ
git diff --stat main -- CMakeLists.txt CMakePresets.json src/core src/fluid src/ecs src/game \
    src/render/ui_view.h tests/test_core_smoke.c tests/test_fluid.c tests/bench_fluid.c \
    tests/test_ecs.c tests/test_app_save.c tests/test_elements.c tests/test_roguelike_meta.c \
    tests/test_acts_enemies.c tests/test_determinism_run.c tests/bench_game.c assets tools .github \
    flake.nix flake.lock .agents AGENTS.md docs/PLAN.md docs/architecture.md docs/game-design.md \
    docs/qa-and-testing.md                                                                     # 空
```

**完了条件（Definition of Done）**
- [ ] §2 の全機能（設定画面拡充＋即時反映＋初期化、遅延勝利精算＆エンドレス 3 ボタン、フォールバック 2 択 UI、エンドレス金砂子＆HUD/幕題字、狐符・結界・追尾符・多部位一筆龍描画＆SE、文字列表完成、`README.txt` 更新）が完了している。
- [ ] 凍結ファイルおよび P5-A / P5-B の担当ファイルに差分がない。
- [ ] `test_text_table`、`test_audio`、`test_app_core`、`test_app_save`、`test_cli_*`、`test_core_smoke` が ASan/UBSan（`-Werror`）で通る。
- [ ] 見本帳の WSLg 描画経路確認（併用 `shader,font` 含む）・崩し試験 4 種・`F1` 描画予算（`< 4.0 ms/frame`、頂点 `<= 60,000`、ブレンド切替 `<= 12`）を確認し、結果を PR に記載した。
- [ ] PR 作成直後に `@coderabbitai review` を投稿し、全指摘を 4 分類トリアージして必要な修正と返信を完了した（[inkstorm-pr-and-coderabbit](../../.agents/skills/inkstorm-pr-and-coderabbit/SKILL.md) 準拠）。
- [ ] PR の CI 3 ジョブ（`build-asan-and-test`, `static-analysis`, `dist-verify`）がすべて緑。
