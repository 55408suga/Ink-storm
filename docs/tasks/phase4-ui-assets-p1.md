# Phase 4-C タスク指示書：P1 UI/シーン（ロビー・筆選択・墨庵・ポーズ・ヒント）・第 2/3 幕描画・毛筆フォント・外部 BGM（`wt-ui-assets-p1`）

> **必読**: [phase4-pre-contracts.md](phase4-pre-contracts.md)（特に §0.1 の進め方、§1.2 の排他所有表、§2 の凍結契約）、[PLAN.md](../PLAN.md) §9.1、[architecture.md](../architecture.md) §1 / §7〜§9、[art-direction.md](../art-direction.md) 全体（特に §5〜§15）、[game-design.md](../game-design.md) §4.3 / §5.1 / §5.6 / §8 / §10 / §11、[qa-and-testing.md](../qa-and-testing.md) §2 / §4 / §5、[phase3-render-audio-p0.md](phase3-render-audio-p0.md)、[AGENTS.md](../../AGENTS.md)、[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md)
>
> **worktree / ブランチ**: `full-scale-dev/wt-ui-assets-p1` / `feat/phase4-ui-assets-p1`（P4-Pre マージ後の `main` から分岐）。完了したら単独で `main` へ PR を出す。
> **並列ペア**: P4-A（[phase4-roguelike-p1.md](phase4-roguelike-p1.md)）、P4-B（[phase4-acts-enemies-p1.md](phase4-acts-enemies-p1.md)）。合流と完了ゲートは P4-Int（[phase4-integration.md](phase4-integration.md)）。

---

## 0. 目的と責務境界

P1 本編の画面遷移（タイトル/ロビー・筆スタイル選択・墨庵・設定・ポーズ・大遺物報酬・リザルト遷移・初回操作ヒント）、第 2・3 幕の情景と新敵/ボス/エフェクトの水墨描画、題字用の毛筆体フォントと外部 BGM 音源（＋安全フォールバック）を実装する。

1. **シーン遷移と UI 画面群**（game-design §10 / §11.1〜§11.3、art-direction §12.2）：
   - **タイトル / ロビー**（`UI_SCENE_TITLE_LOBBY`）：「出陣」「墨庵」「設定」「終了」、所持墨晶・最高到達幕・最高スコア・累計クリア数、背景で流体だけがゆっくり動きマウスドラッグで墨を描いて遊べる。
   - **筆スタイル選択**（`UI_SCENE_BRUSH_SELECT`）：豪筆・朱筆・細筆の 3 枚のカード（特徴・修正値・推奨軸）と自動の試し書き演出。選択でその筆の `RunConfig` を組んでランを開始。
   - **墨庵（メタショップ）**（`UI_SCENE_BOKUAN`）：恒久強化 4 種（社の加護・大きな硯・早墨・再考の札）と大遺物解放 6 枠を掛け軸/短冊で並べ、`meta_buy_upgrade` / `meta_buy_relic_unlock` と落款効果音・即時セーブ（`save_write`）を行う。
   - **設定画面**（`UI_SCENE_SETTINGS`）：マスター音量・BGM・SE（`0..100%`）、ポストシェーダ ON/OFF（実行時即時切替）、画面揺れ（`100% / 50% / 0%`）、操作ヒント ON/OFF。ロビーとポーズメニューの両方から開け、変更時に即時反映・終了時にセーブへ保存する。
   - **ポーズメニュー**（`UI_SCENE_RUN` かつ `ui.paused == true`）：戦闘中に `Esc` で開閉。「再開」「最初から」「設定」「ロビーへ戻る」＋現在の所持強化札・大遺物一覧。「最初から」と「ロビーへ戻る」は確認ダイアログを出し、承認時に `CMD_ABORT_RUN` で敗北精算・セーブ（ラン開始直後の第 1 幕 `ACT_TRANSITION` 中を除く）を行う。
   - **`Esc` 仕様変更に伴う文書更新**（PLAN §9.1）：`AGENTS.md` と `README.txt` の終了・ポーズ・ロビー/墨庵操作の説明を更新する。
   - **大遺物 3 択報酬・リザルト拡張・初回操作ヒント短冊**：`REWARD_KIND_RELIC` の巻物・名品 3 択描画、HUD の幕名動的表示・社灯火上限拡張・右下大遺物アイコン、リザルトの取得札/遺物表示と「ロビーへ戻る / 再出陣」ボタン、`EV_HINT_TRIGGER` 受信時の上部胡粉短冊（FIFO キュー・2.0 秒・ゲーム非停止・`meta_mark_hint_seen` 保存）。
2. **第 2・3 幕の情景、筆 3 種、新敵 4 種・エリート・鵺・一筆龍・爆雷/三元共鳴の描画**（art-direction §5 / §6 / §10 / §11）：
   - 第 2 幕（薄柿の地色、左右端の竹と上端の烏、弁柄の鳥居柱と濃墨の笠木、没骨＋皴法の岩柱）。
   - 第 3 幕（冷灰の地色、上端の霊峰龍脈筆線と飛白・墨雲・斜め雨線、胡粉＋紋＋属性内光の霊柱、山颪予兆の紙はためきと清墨の払い矢印 3 本）。
   - 筆 3 種の筆致とカーソル意匠（豪筆＝太い濃墨、朱筆＝朱混じりと火の粉、細筆＝細い淡墨と蒼い火花）。
   - 提灯妖・雷、鎌鼬、石鬼、墨吸い、エリート 4 種の色縁（金・胡粉・朱・蒼）、鵺（猿面・狸胴・虎脚・蛇尾・黒雲と蒼雷・停止時の太輪郭）、一筆龍簡易版（頭部の軌跡履歴 24 点から 1 本の `ink_stroke` で胴と鱗・龍玉・点睛・龍巻き予兆円を描く）、鎌風・跳弾雷球、爆雷（欠けた円相と放射払い）・三元共鳴（金縁の円相と落款「三元」・金砂子）。
3. **毛筆体フォント（`assets/fonts/title_brush.ttf`）と外部 BGM（`assets/audio/*.ogg`）と `assets/LICENSES.txt`**（game-design §11.4 / §11.6）：
   - 40px 以上の題字・落款・大反応名専用の OFL 1.1 毛筆体フォント同梱（未改変）＋明朝体 / ASCII への多段安全フォールバック。
   - ロビー・通常戦・ボス戦の外部ループ BGM と勝利ジングル（OGG）の同梱＋`ink_audio` プロシージャル BGM（`BGM_LOBBY` 追加）への自動フォールバック、および爆雷・三元共鳴・山颪予兆のプロシージャル SE 追加。

**禁止事項**
- P4-Pre §1.2 の凍結ファイル（`CMakeLists.txt`, `ui_view.h`, `src/game/*`, `tools/*` 等）および P4-A / P4-B の所有ファイルを編集しない。
- `render` / `audio` から `GameState` を変更しない。`g->internal` を読まない。
- `src/audio/*`, `src/app/save.*`, `paths.*`, `cli.*`, `frame_clock.*`, `input_latch.*`, `src/render/text_table.*` に `raylib.h` を include しない。
- 完成形の描画に raylib の均一図形描画関数や RGB 直書き（`palette.c` 以外）を使わない（art-direction §8 / §13 / §14.4）。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/render/text_table.h` / `text_table.c` | 更新 | `#include "meta.h"` / `<stdint.h>` 追加、`TEXT_P1_STUB` 削除、P1 の全文字列（第 2・3 幕題字＆短縮幕名、新敵 4 種＋鵺＋一筆龍、全 24 札、全 15 遺物（初期 9＋墨庵解放 6）、筆 3 種、ロビー/墨庵/設定/ポーズ/リザルト、ヒント 8 種、爆雷/三元共鳴、落款「三元」「墨嵐」）と対応関数（§2.1） |
| `src/render/fonts.c`、`render.h`、`render_internal.h`、`render.c`、`post_fx.c`、`xray.c` | 更新 | `title_brush.ttf` 読み込みと二重解放しない多段フォールバック、`collect_codepoints` オーバーフロー修正、シーン分岐＆Layer 3 地形描画、実行時 `render_set_post_enabled`、新 UI レイアウト関数宣言、`F1` 表記更新（§2.2 / §2.3 / §2.6） |
| `src/render/paper.c` | 更新 | 無彩色グレースケール和紙テクスチャ化（第 2・3 幕の二重乗算暗化防止）、第 2 幕・第 3 幕の背景キャッシュ、Layer 3 地形描画 `render_paper_draw_terrain`（鳥居＋笠木・岩柱・霊柱）、山颪の端のはためき（§2.3） |
| `src/render/draw_entities.c`、`vfx.c`、`hud.c`、`ui_banner.c` | 更新 | 新敵 4 種・エリート縁・鵺・一筆龍（頭部軌跡リングバッファ 24 点）・新弾 2 種（跳弾円相検知含む）・落雷予兆・筆 3 種カーソル・爆雷/三元共鳴 VFX（大コールアウト `>= 40px`）・HUD 幕名/灯火上限 16/連殺猶予ゲージ/右下遺物/非ラン時抑止・ヒント短冊・見本帳 6 ページ（§2.4 / §2.5） |
| `src/render/ui_reward.c`、`ui_result.c` | 更新 | `REWARD_KIND_RELIC`（巻物と名品 3 択、修繕非表示、リロール中央配置、見出し `>= 40px`）、リザルトの所持札（最大 9 枚2列）＆大遺物表示と「ロビーへ戻る / 再出陣」ボタン（§2.5） |
| `src/render/ui_lobby.c`、`ui_shop.c`、`ui_pause.c`、`ui_settings.c` | 新規 | タイトル/ロビー（背景流体デモ付き）、筆スタイル選択（試し書き付き）、墨庵、ポーズ＋確認ダイアログ、設定画面の描画と共通レイアウト矩形計算（§2.6） |
| `src/audio/audio.h` / `audio.c`、`synth_voices.h` / `synth_voices.c`、`bgm_proc.c` | 更新 | `EV_THUNDERBURST` / `EV_CATACLYSM` / `EV_WIND_TELEGRAPH` の SE ボイス、`BGM_LOBBY` プロシージャル環境音、`audio_set_lobby_mode` / `audio_set_external_bgm` / `audio_set_paused` / `audio_trigger_ui_stamp`（§2.7） |
| `src/app/app_loop.c`、`input_map.h` / `input_map.c`、`input_latch.h` / `input_latch.c`、`audio_out.h` / `audio_out.c`、`showcase.c`、`headless.c` | 更新 | `SetExitKey(KEY_NULL)` による `Esc` ポーズ制御、ロビー/筆選択/墨庵/設定/ポーズ/リザルトの状態遷移、終了時精算のラン中限定ガード、初回ヒント FIFO キュー＆既読保存、外部 OGG BGM ストリーム再生とプロシージャル BGM フォールバック、見本帳 `--showcase` 6 ページ化、`headless.c` の任意アセット有無ログ（§2.6〜§2.8） |
| `assets/fonts/title_brush.ttf`、`assets/audio/bgm_*.ogg`、`assets/LICENSES.txt` | 新規・更新 | 題字用毛筆体フォント（OFL 1.1・未改変）と外部 BGM 4 曲の同梱、ライセンス記録（§2.2 / §2.7） |
| `AGENTS.md`、`README.txt` | 更新 | `Esc` のポーズメニュー化・ロビー/墨庵/リザルト操作・同梱アセットライセンスの更新（§2.9） |
| `tests/test_text_table.c`、`tests/test_audio.c`、`tests/test_app_core.c` | 更新 | P1 文字列網羅（第 2・3 幕含む）、新 SE・`BGM_LOBBY`・外部 BGM/ポーズ制御、ポーズ/UI クリック時のラッチ規則のテスト追加（§3） |
| `docs/art-direction.md` | 数値のみ | 見本帳で微調整した太さ・不透明度等の同期 |

---

## 2. 実装仕様

### 2.1 文字列表（`text_table.h` / `text_table.c`）
`text_table.h` に `#include "meta.h"` と `#include <stdint.h>` を追加（raylib 非依存を維持）し、P4-Pre の `TEXT_P1_STUB` を削除した上で、P0 の既存関数に加えて以下の対応関数を公開・登録する（範囲外はすべて `TEXT_NONE`）：
```c
TextId text_act_short(int act_index); /* 0..2: 「第一幕」「第二幕」「第三幕」 / "Act 1".."Act 3" */
TextId text_relic_name(RelicId id);
TextId text_relic_effect(RelicId id);
TextId text_relic_tradeoff(RelicId id); /* 代償なしは TEXT_NONE または「代償なし」 */
TextId text_shop_upgrade_name(MetaUpgradeKind kind);
TextId text_shop_upgrade_desc(MetaUpgradeKind kind);
TextId text_shop_relic_name(uint32_t slot); /* 0..META_RELIC_UNLOCK_COUNT-1（解放枠 6 種） */
TextId text_shop_relic_desc(uint32_t slot);
TextId text_brush_name(BrushStyleId style);
TextId text_brush_desc(BrushStyleId style);
TextId text_brush_stats(BrushStyleId style);
TextId text_hint_message(HintId id);
```
- `text_enemy_name` / `text_enemy_intro` は全 10 種の `EnemyDefId`、`text_boss_epithet` は `ENEMY_BOSS_TENGU`（「紅風の大天狗」）・`ENEMY_BOSS_NUE`（「雷雲の鵺」）・`ENEMY_BOSS_RYU`（「霊峰の龍脈」）、`text_upgrade_name` / `text_upgrade_effect` は全 24 種の `UpgradeId` × Lv1〜3、`text_act_title` は `act_index = 0, 1, 2`（「第一幕　開けた和紙の社」「第二幕　鳥居と岩柱」「第三幕　嵐の霊峰」）で非 `TEXT_NONE` を返す。

---

### 2.2 題字用毛筆体フォント（`assets/fonts/title_brush.ttf`）と多段フォールバック

| 項目 | 仕様 |
| :--- | :--- |
| 採用書体 | **Yuji Syuku Regular（佑字 粛 Regular）** または **Yuji Mai Regular（佑字 舞 Regular）**（いずれも SIL Open Font License 1.1、Google Fonts `ofl/yujisyuku` / `ofl/yujimai`、10 MiB 以下、**サブセット化を含め未改変**） |
| 配置と記録 | `assets/fonts/title_brush.ttf` に配置し、`assets/LICENSES.txt` に書体名・版・著作権表示・OFL 1.1（`assets/fonts/OFL.txt` 参照）・入手元 URL・「未改変」を追記する |
| 使い分け（art-direction §9.6） | `font_title`（`title_brush.ttf`）は **40px 以上** の題字（タイトル「墨嵐」、幕題字、ボス登場題字、報酬見出し、「三元共鳴！」「爆雷！」等の大コールアウト、落款の文字）にのみ使う。既存の `ui_reward.c` 見出し（`36px → 42px`）と `vfx.c` 大コールアウト（`36px → 40px`）を 40px 以上に揃える。18〜36px の本文・数値・短冊説明・設定項目には必ず `font_body`（`ui_mincho.ttf`）を使う |
| `collect_codepoints` の上限修正（重要） | P0 の `fonts.c` は重複を含む生文字を `out[2048]` へ詰めた後で `qsort` 重複除去しているため、P1 の全文字列表（延べ 3,000 文字超）では後半の文字が欠落する。**追加時に逐次重複排除しながらユニーク配列（上限 2048〜4096）へ格納する**方式へ改修し、全題字・落款・ヒント・札・遺物のグリフ欠落（豆腐化）を防ぐ |
| 多段フォールバックと所有管理（二重解放防止） | `RenderOptions` に `const char *title_font_path;` を追加し、`RenderState` は `bool owns_body_font; bool owns_title_font;` を個別に持つ。<br>① `APP_FAULT_FONT` 指定時、または `ui_mincho.ttf` 読み込み失敗時：従来どおり `GetFontDefault()` と英語 ASCII（`use_ja = false`）へフォールバックし `INKSTORM: font unavailable; using built-in ASCII font` を出す。<br>② `ui_mincho.ttf` は成功したが `title_brush.ttf` のみ不在/失敗した場合：クラッシュさせず、`ui_mincho.ttf` から `FONT_TITLE_PX`（48px）を別インスタンスとして読み込んで `font_title` にする（または `owns_title_font = false` として `font_body` を共有し、`render_fonts_close` で絶対に二重 `UnloadFont` しない）ことで日本語表示（`use_ja = true`）を鮮明に維持する |

---

### 2.3 第 2・3 幕の情景と地形の描画（`paper.c`, `render.c`、art-direction §6 / §7）

- **和紙地色の正確な発色（重要修正）**:
  - P0 の `paper.c` は `paper_tex` に `PAL_PAPER_ACT1` を焼き込んだ上で第 2・3 幕で `pal_color(paper_pal)` を乗算していたため、第 2・3 幕の地色が暗く黄ばむ二重乗算になる。`render_paper_init` で `paper_tex` を **無彩色の白ベース（明度 ~248±7 の微細グレイン＆繊維線）** として生成し、`render_paper_draw_base` で全 3 幕とも `pal_color(pal_paper_for_act(act_index))` を乗算することで、第 1 幕（生成 `#EDE4D0`）・第 2 幕（薄柿 `#EADCC4`）・第 3 幕（冷灰 `#DAD8CF`）の正確な色と明度 `L* >= 80` を保つ。
- **Layer 3 地形描画（`render_paper_draw_terrain(r, g, act)`）**:
  - `render.c` の `draw_world_layers` で、Layer 2（背景キャッシュ）と Layer 4（流体）の間に呼び出す（`art-direction.md` §9.5 準拠）。
- **第二幕『鳥居と岩柱』（`act_index == 1`）**:
  - 端の背景キャッシュ（画面端 120px 帯のみ、不透明度 `<= 0.35`）：左右端に竹（縦の一筆 `ink_stroke` と節の短い打ち込み）、上端に烏の群れ（小さな払い 5〜9 個）。
  - 地形：
    - 鳥居 2 基（`solid` 座標に厳密一致：左鳥居 中心 `x = 384`、右鳥居 中心 `x = 896`）：上下の柱（中心 `(384, 328)`・`(384, 392)` と `(896, 328)`・`(896, 392)`）を `PAL_BENGARA`（弁柄、炎の朱と紛れさせない）の没骨と濃墨の締めで描き、2 本の柱を跨ぐ笠木と貫を濃墨の力強い `ink_stroke` で描く（笠木は描画のみ）。
    - 岩柱 4 本（`(340, 190)`, `(940, 190)`, `(340, 530)`, `(940, 530)`、半径 24px）：淡墨の `ink_wash` 2 層の上に皴法（短い `ink_stroke` 2〜3 本）と濃墨の輪郭一筆を入れる。
- **第三幕『嵐の霊峰』（`act_index == 2`）**:
  - 端の背景キャッシュ：上端に霊峰の稜線を 1 本の長い龍脈筆線（`ink_stroke`、`dry = 0.45` の飛白）で描き、上端 15% にうねる墨雲（`ink_wash`）。さらに動的レイヤとして清墨の斜め雨線を毎秒 `60px` で流す。
  - 地形：
    - 上側 2 本の岩柱（第二幕と同じ）。
    - 霊柱 3 本（`COMP_GIMMICK` の `GIMMICK_SPIRIT_PILLAR`、または固定座標 `(640, 130)`, `(441, 475)`, `(839, 475)`）：胡粉（`PAL_GOFUN`）の柱面に淡墨の渦紋を描き、`gim->elem_kind == GIMMICK_ELEM_FIRE` の間は内側から `PAL_SHU` / `PAL_SHU_HOT`、`GIMMICK_ELEM_STORM` の間は `PAL_AO` / `PAL_AO_HOT` の 2 層の灯と残り時間リングを灯す。
  - **山颪の予兆（`EV_WIND_TELEGRAPH` 受信から 1.5 秒間）**:
    - `RenderState` に `float wind_telegraph_t; int32_t wind_telegraph_dir;` を持ち、`EV_WIND_TELEGRAPH` 消費時に `1.5f` と `ev->value`（`0..3`：左・右・上・下）をセットする。予兆中、風上の画面端の背景帯を短冊状に波打たせ、風向きへ走る清墨の払い矢印を 3 本（`ink_stroke` の `reveal` アニメーション）描く。

---

### 2.4 新登場物・エリート・ボス・筆カーソルの意匠（`draw_entities.c`、art-direction §10）

すべての登場物に必ず紙抜き縁 `ink_knockout` を先に描く。

1. **通常敵 4 種と新敵弾 2 種**:
   - **提灯妖・雷（`ENEMY_LANTERN_STORM`）**: 提灯妖・炎と同じ破れ提灯の骨格で、中の灯を 2 層の蒼（`PAL_AO`＋芯 `PAL_AO_HOT`）と短い稲妻の明滅で描く。
   - **鎌鼬（`ENEMY_KAMAITACHI`）**: `w_max = 3px`。鎌形の鋭い払い線 3 本で体を作り、移動の後ろに淡墨の残像曲線を 0.3 秒引く。
   - **石鬼（`ENEMY_STONE_ONI`）**: `w_max = 6px`（通常敵で最太）。重墨の `ink_wash` 2 層と雪舟風の角張った皴法・割れ線でずんぐりした岩の鬼を描く。
   - **墨吸い（`ENEMY_INK_LEECH`）**: 輪郭線（`ink_outline`）を引かない唯一の例外。紙抜き縁の内側に `ink_wash` 3 層のにじみ面と胡粉の目 2 点だけを描き、`tr->radius`（または `en->absorbed_ink`）に応じてにじみ径を広げる。
   - **鎌風（`PROJ_WIND_SICKLE`）**: 三日月形の払い（淡墨に胡粉の縁）。
   - **跳弾雷球（`PROJ_BOUNCE_ORB`）**: 蒼の宝珠形＋周囲の短い稲妻。`RenderState` で各スロットの直前 `bounces_left` を追跡し、減少を検知した瞬間に小さな欠けた円相の VFX を生成する。
2. **エリート変異の縁（`en->is_elite`、art-direction §10.4）**:
   - 紙抜き縁のすぐ外側に、途切れのない細い実線（破線にしない）で変異色の縁を `2px` 足す：剛体＝`PAL_KIN`（金）、撥墨＝`PAL_GOFUN`（胡粉）、不燃＝`PAL_SHU`（朱）、避雷＝`PAL_AO`（蒼）。2 つ持つ場合は内周・外周または半周ずつ両方の色を見せる（`ENEMY_FLAG_SUMMONED` は無視して `ELITE_ALL_MASK` のみ見る）。
3. **第 2 幕ボス：鵺（`ENEMY_BOSS_NUE`、art-direction §10.5）**:
   - 淡墨の黒雲（`ink_wash`）と蒼い稲妻をまとい、胡粉の猿面・淡墨の胴・濃墨の虎縞の脚・細い払いの蛇尾を描く。
   - 予兆中（`boss->telegraph > 0.01f`）は `boss->gale_dx, boss->gale_dy` 方向に蒼い雷光の進路予兆を描き、ダッシュ軌跡に清墨の龍脈筆線と蒼い雷帯を残す。
   - **停止中（`en->ai_state == 1`）** は輪郭の太さを `1.3 倍` に太らせ身を沈めた姿勢にし、「今だけ叩きつけが効く」ことを視覚で伝える。
4. **第 3 幕ボス：一筆龍・簡易版（`ENEMY_BOSS_RYU`、art-direction §10.5）**:
   - `RenderState` 内に頭部の位置履歴リングバッファ `Vector2 ryu_trail[24]; int32_t ryu_trail_n; EntityId ryu_trail_id;`（`render_update_vfx` で毎ティック先頭へ追加、ボス不在または ID 変更時にクリア）を持つ。
   - 頭から尾までを途切れない **1 本の `ink_stroke`**（`w_max = 26px`、頭側が太く尾の 30% は `dry = 0.6` でかすれさせる）で描き、胴に沿って皴法の短い鱗の打ち込みを 40〜50 本置く。
   - 頭部に角 2 本・髭 2 本の細い払いと龍玉（`boss->part_id == 0`: 朱の炎玉、`1`: 蒼の雷玉、`2`: 胡粉＋渦文の風玉。`boss->telegraph > 0` の間は明色で強く発光）を描く。
   - 目は、点睛前（`boss->phase_idx == 0`）の間は胡粉の空白の輪郭円にし、点睛（`EV_BOSS_PHASE`）の 1.5 秒で大きな朱の点を描き入れ、以降は全身の墨を一段濃い焦墨へ締める。
   - 龍巻き予兆（`boss->telegraph > 0.01f && boss->telegraph_pattern == 3`）は社周囲半径 `240px` の朱の破線円を描く。
   - 雷玉の落雷予兆（`COMP_GIMMICK` の `GIMMICK_LIGHTNING_STRIKE`）は、半径 `32px` の朱の破線の円と中心の短い打ち込みで描く。
5. **筆スタイル 3 種の筆致とカーソル（game-design §5.6）**:
   - `g->config.brush_style` に応じて、筆先カーソルと風筆の軌跡演出を変える（豪筆＝太い濃墨の穂先、朱筆＝穂先に朱が混じり火の粉が散る、細筆＝細く鋭い穂先に蒼い火花が散る）。

---

### 2.5 爆雷・三元共鳴 VFX、大遺物 UI、HUD、リザルト、初回ヒント短冊

1. **爆雷（`EV_THUNDERBURST`）と三元共鳴（`EV_CATACLYSM`）（art-direction §11）**:
   - `EV_THUNDERBURST`（0.4 秒、「大」演出）：欠けを 1 か所持つ禅画の円相（`ink_stroke`）と、外向きの太い放射払い 6〜10 本、朱と蒼が混ざった `ink_splatter`、毛筆体 40px 以上の「爆雷！」コールアウト、小シェイク。
   - `EV_CATACLYSM`（0.6 秒、「大」演出）：金（`PAL_KIN` / `PAL_KIN_HOT`）で縁取った大円相と落款「三元」（`seal_stamp`）、中心へ吸い込まれてから外へ払われる筆線、金の砂子の散らし、毛筆体 44px 以上の大コールアウト「三元共鳴！」、大シェイク。
2. **大遺物 3 択報酬（`ui_reward.c`）**:
   - `render_reward_layout(g, &layout)`: `rw->kind == REWARD_KIND_RELIC` のときは `out->has_repair = false;` とし、`reroll_rect` を中央下に配置する。
   - `render_draw_reward_ui`: 見出し文字サイズを `42.0f`（`>= 40px`）にし、`can_reroll` を `(rw->kind == REWARD_KIND_UPGRADE || rw->kind == REWARD_KIND_RELIC) && g->run.rerolls_left > 0` とする。`REWARD_KIND_RELIC` では広げた巻物の上に 3 つの名品の淡墨意匠と題簽（`text_relic_name`）、効果（`text_relic_effect`）、代償（`text_relic_tradeoff`、代償があるものは朱で明示）を並べる。
3. **HUD 更新（`hud.c`）**:
   - 非戦闘シーン（`UI_SCENE_TITLE_LOBBY`, `UI_SCENE_BRUSH_SELECT`, `UI_SCENE_BOKUAN`, `UI_SCENE_SETTINGS`）および見本帳のページ `3..6` では HUD を描画しない。
   - 上中央の幕名を `text_act_short(g->run.act_index)` に変更する（P0 の固定 `TEXT_HUD_ACT` を廃止）。
   - 左上の社 HP 灯火の表示上限を `12 → 16`（最大 14 に対応、12 超時は間隔を詰める）に引き上げる。
   - 右上の連殺猶予ゲージ分母を `game_upgrade_level(g, UPG_CHAIN_PRIMER)` に応じた `2.65 / 3.20 / 3.80 / 4.50f` に合わせる。
   - 右下に所持大遺物（`g->run.relics[0..relic_count)`）の小さな落款風アイコンと略称を並べる。
4. **リザルト画面（`ui_result.c`）**:
   - 所持強化札を最大 9 枚（2 列コンパクト配置）まで表示し、さらに所持大遺物（`g->run.relics[0..relic_count)`）も並べて表示する。
   - 下部に **「ロビーへ戻る」**（既定、`Enter` / `Esc` / クリック）と **「再出陣」**（クリック）の 2 ボタンを設け、`render_result_layout` を `render.h` で公開して `input_map.c` と共有する。
5. **初回操作・反応ヒント（`EV_HINT_TRIGGER`、game-design §11.3、art-direction §11）**:
   - `AppRunContext` 内に小さな FIFO キュー `HintId hint_queue[HINT_COUNT]` を持ち、同一ティックに複数ヒントが発火しても上書き消失させずに順次 2.0 秒ずつ表示する。
   - `EV_HINT_TRIGGER`（`HintId id = (HintId)ev.value`）受信時、`ctx.meta.settings.hints_on` が真かつ `!meta_is_hint_seen(&ctx.meta, (uint32_t)id)` であればキューへ積み、`meta_mark_hint_seen(&ctx.meta, (uint32_t)id)` と `persist_meta(&ctx)` を行う。
   - `active_hint_t > 0.0f` の間（ゲームは止めない）、画面上中央（敵紹介バナーと重ならない位置、例 `y = 66px`、敵紹介表示中はその下）に垂れる胡粉の短冊に明朝体 18px で `text_hint_message(active_hint)` を描画する。

---

### 2.6 ロビー・筆スタイル選択・墨庵・設定・ポーズ画面と入力遷移（`app_loop.c`, `input_map.c`, `render.c`, `post_fx.c`, `ui_*.c`）

すべてのメニュー画面で **マウスホバー＋左クリック** と **キーボード（上下左右 / `1`〜`4` / `Enter` / `Esc`）** の両方で操作できるようにする。描画側の矩形計算関数（`render_lobby_layout`, `render_brush_select_layout`, `render_bokuan_layout`, `render_pause_layout`, `render_settings_layout`, `render_result_layout`）を `render.h` に公開し、`input_map.c` と `ui_*.c` で共有する。また `ctx.meta` が変わるたびに `ctx.ui.meta_view = ctx.meta;` を同期する。

1. **起動時の初期シーン**:
   - `opts->smoke_frames > 0` のときは、自動スモークテストが従来どおり戦闘画面を検証できるよう `UI_SCENE_RUN` で直接ランを開始する。
   - 通常のウィンドウ起動（`opts->smoke_frames == 0 && opts->mode == APP_MODE_WINDOW`）では **`UI_SCENE_TITLE_LOBBY`** から開始する。
2. **タイトル / ロビー（`UI_SCENE_TITLE_LOBBY`、`ui_lobby.c`）**:
   - 背景では `game_step` を呼ばず、**`game->fluid` のみ**を緩やかな自律渦＋マウス左ドラッグの墨描きで `fluid_step` / `fluid_sanitize_channels` 更新し（敵やランタイマーを動かさない）、中央〜右側に毛筆体（`>= 48px`）で題字「墨嵐」と朱の落款「墨嵐」を大きく配する。
   - メニュー 4 項目：「出陣」（→ `UI_SCENE_BRUSH_SELECT`）、「墨庵」（→ `UI_SCENE_BOKUAN`）、「設定」（→ `UI_SCENE_SETTINGS`、`settings_return_scene = UI_SCENE_TITLE_LOBBY`）、「終了」（ループ終了フラグを立てて正常終了）。
   - 画面下部に `ui->meta_view` から所持墨晶（`ink_crystals`）、最高到達（`best_act`）、最高スコア（`best_score`）、累計クリア数（`clears`）を表示する。
   - ロビーでの `Esc` キーは「終了」と同じくアプリケーションを終了する。
3. **筆スタイル選択（`UI_SCENE_BRUSH_SELECT`、`ui_lobby.c`）**:
   - 豪筆・朱筆・細筆の 3 枚の縦長カードを並べ、各カードに名前・特徴・修正値・推奨軸と、カード内で自動周回する筆の試し書き軌跡（豪筆＝太い濃墨、朱筆＝朱と火の粉、細筆＝細い淡墨と蒼雷）を描く。
   - `1`〜`3` キー、左右キー＋`Enter`、またはカードのクリックで `selected_brush` を確定し、`meta_build_run_config(&ctx.meta, selected_brush, seed)` で新しいランを初期化して `UI_SCENE_RUN` へ遷移する（`input_latch_init`＋クリック後の `paint_suppressed` を引き継ぎ、開始直後の暴発筆跡を防ぐ）。`Esc` でロビーへ戻る。
4. **墨庵（`UI_SCENE_BOKUAN`、`ui_shop.c`、game-design §10.2）**:
   - 庵の掛け軸の上に所持墨晶を表示し、全 10 商品を並べる：
     - 恒久強化 4 種（`META_UPG_SHRINE_HP` 80/140/200、`META_UPG_INK_MAX` 60/100/140、`META_UPG_INK_REGEN` 60/100/140、`META_UPG_REROLLS` 100/200）：現在段階 / 最大段階と次段階の価格を表示。クリックまたは `Enter` で `meta_buy_upgrade(&ctx.meta, kind)` を呼び、成功したら `audio_trigger_ui_stamp(audio)` と即時セーブ（`persist_meta(&ctx)`）。
     - 大遺物の解放 6 種（スロット `0..5`、各 100 墨晶）：名前・説明・未解放（100 墨晶）/ 解放済みの落款を表示。クリックまたは `Enter` で `meta_buy_relic_unlock(&ctx.meta, slot)` を呼び、成功したら落款効果音と即時セーブ。
   - 「戻る」ボタンまたは `Esc` で `UI_SCENE_TITLE_LOBBY` へ戻る。
5. **設定（`UI_SCENE_SETTINGS`、`ui_settings.c`、`post_fx.c`、game-design §11.2）**:
   - 6 項目：マスター音量（`0..100`、10 刻み）、BGM 音量（`0..100`）、SE 音量（`0..100`）、ポストシェーダ（ON / OFF）、画面揺れ（`100% / 50% / 0%`）、操作ヒント（ON / OFF）。
   - **ポストシェーダの実行時切替契約（`post_fx.c` / `render.c`）**: `RenderState` に `bool post_enabled;` と `void render_set_post_enabled(RenderState *r, bool enabled);` を追加する。CLI `--no-post` でも `RENDER_FAULT_SHADER` でもない場合は `render_post_init` でシェーダをロードしておき、`post_enabled` の初期値を `ctx.meta.settings.post_shader_on` にする。これによりセーブ初期値が OFF の状態から設定画面で ON へ切り替えても即座に有効化される（CLI `--no-post` やシェーダ失敗時は `post_shader.id == 0` のまま直接描画を維持）。
   - 変更のたびに `ctx.meta.settings`・`audio_set_volumes`・`ctx.ui.shake_scale`・`render_set_post_enabled` へ即時反映し、「戻る」または `Esc` で `persist_meta(&ctx)` して `settings_return_scene`（ロビー、またはポーズ中の `UI_SCENE_RUN`）へ戻る。
6. **ポーズメニューと終了時精算ガード（`UI_SCENE_RUN` かつ `ui.paused == true`、`ui_pause.c`、game-design §5.1 / §11.2）**:
   - `app_run_window_loop` の `InitWindow` 直後に `SetExitKey(KEY_NULL)` を呼び、`Esc` でウィンドウが即閉じないようにする（`--showcase` は従来どおり `Esc` で終了可）。
   - `UI_SCENE_RUN` で `g->phase` が `VICTORY` / `GAME_OVER` 以外のとき、`Esc` で `ui.paused` をトグルする。ポーズ中は `game_step` を呼ばず、`audio_set_paused(audio, true)` で筆擦れ音を無音化し、ポーズ解除時に `audio_set_paused(audio, false)` と `frame_clock_reset(&clock)` で `accumulator` を 0 に戻す（クリック解除直後の誤描画を防ぐため `paint_suppressed` を維持）。
   - 画面に薄い和紙オーバーレイを掛け、左側に現在の所持強化札（Lv 付き）と所持大遺物の一覧、中央にメニュー 4 項目（「再開」「最初から」「設定」「ロビーへ戻る」）を描く。
   - 「最初から」または「ロビーへ戻る」を選んだときは `ui.pause_confirm`（`PAUSE_CONFIRM_RESTART` / `PAUSE_CONFIRM_LOBBY`）で確認ダイアログ（「現在の出陣を敗北として精算します。よろしいですか？［はい / いいえ］」）を出す。
   - 確認ダイアログで「はい」を選んだら、ラン開始直後の第 1 幕 `ACT_TRANSITION` 中でない限り `CMD_ABORT_RUN` で `game_step` を 1 回呼んで `app_after_tick`（`meta_settle_run`＋必要時 `persist_meta`）を実行した上で、「最初から」なら同じ `selected_brush` で新規ラン開始、「ロビーへ戻る」なら `UI_SCENE_TITLE_LOBBY` へ遷移する。
   - **終了時精算ガード（重要）**: ウィンドウ閉じるボタンによる `app_finish_run_on_exit` は、**ラン進行中のシーン**（`ctx.ui.scene == UI_SCENE_RUN` または `ctx.ui.scene == UI_SCENE_SETTINGS && ctx.ui.settings_return_scene == UI_SCENE_RUN`）のときにのみ実行する（ロビー・筆選択・墨庵からの終了で架空のランが精算されるのを防ぐ）。

---

### 2.7 外部 BGM（`assets/audio/*.ogg`）とプロシージャル音響拡張（`audio.h/.c`, `bgm_proc.c`, `audio_out.c`）

1. **同梱する外部 BGM 音源（4 ファイル）**:
   - `assets/audio/bgm_lobby.ogg`（ロビー・墨庵用の静かな和風環境曲、ループ）
   - `assets/audio/bgm_battle.ogg`（通常ウェーブ用の和楽器ループ）
   - `assets/audio/bgm_boss.ogg`（幕ボス戦用の緊迫した和太鼓・琴ループ）
   - `assets/audio/bgm_victory.ogg`（勝利ジングル、非ループ `looping = false`）
   - **フォーマットとライセンス**: `.gitignore` が `*.wav` を除外しているため、**OGG Vorbis（`.ogg`）** で配置する（raylib 6.2 の `LoadMusicStream` が `stb_vorbis` 経由で標準対応し、`tools/package_dist.py` が `assets/` 配下を自動収録する）。CC0 / Public Domain、または決定論的シンセスクリプトで事前生成した商用・再配布自由な和風音源（各ファイル数百 KiB〜1.5 MiB 程度）を同梱し、`assets/LICENSES.txt` に曲名・作者/生成元・ライセンスを明記する（**注意**：`tools/` は凍結中のため生成用スクリプトを `tools/` へコミットしない）。
2. **`ink_audio` の raylib 非依存 API 拡張（`src/audio/audio.h` / `audio.c` / `bgm_proc.c` / `synth_voices.c`）**:
   - 以下の制御関数を `audio.h` に追加する（既存の `audio_init` 直後は無音契約を維持し `test_audio_initial_state` を壊さない）：
     ```c
     void audio_set_lobby_mode(AudioState *audio, bool lobby);
     void audio_set_external_bgm(AudioState *audio, bool external_active);
     void audio_set_paused(AudioState *audio, bool paused);
     void audio_trigger_ui_stamp(AudioState *audio);
     ```
   - `audio_set_lobby_mode(audio, true)`: `audio->bgm = BGM_LOBBY`・`has_consumed = true`・`brush_target = 0.0f` とし、ロビー専用の穏やかなプロシージャル琴・風音トラックを `bgm_proc.c` に実装する（P0 の `BGM_BATTLE` フォールスルーを解消）。
   - `audio_set_external_bgm(audio, true)`: `bgm_proc.c` のプロシージャル楽器（太鼓・琴・ドローン）出力だけをミュートし、SE・筆擦れ・穏やかな環境風音は `AudioStream` 側から継続再生する（`audio_bgm_track(audio)` は望ましいトラックを返し続ける）。
   - `audio_set_paused(audio, true)`: `brush_target = 0.0f` にして、風筆ドラッグ中のポーズで筆擦れ音が鳴りっぱなしになるのを防ぐ。
   - `audio_trigger_ui_stamp(audio)`: 墨庵購入やメニュー決定用の落款 SE（`AUDIO_VOICE_REWARD_PICK` 相当）を即座に発音する。
   - 新イベント 3 種の SE ボイスを `synth_voices.c` に追加する：
     - `EV_THUNDERBURST`（`AUDIO_VOICE_THUNDERBURST`）：重低音の太鼓＋鋭い放電破裂音
     - `EV_CATACLYSM`（`AUDIO_VOICE_CATACLYSM`）：大太鼓＋銅鑼（鉦）の重厚な共鳴＋上行する琴の和音
     - `EV_WIND_TELEGRAPH`（`AUDIO_VOICE_WIND_HOWL`）：山颪を告げる低い笛と風の唸り
3. **`audio_out.c` での外部 BGM ストリームとプロシージャル BGM の自動切り替え**:
   - `audio_out_init` でオーディオデバイス準備成功後に `app_resolve_asset_path("audio/bgm_*.ogg")` により 4 トラックの `LoadMusicStream` を試みる（`bgm_victory.ogg` のみ `looping = false`）。`audio_out_close` では `CloseAudioDevice()` の前に全ロード済み `Music` を `StopMusicStream`＋`UnloadMusicStream` する。
   - `audio_out_pump` で `audio_bgm_track(audio)` の望ましいトラックに対応する外部 `Music` が有効なら `audio_set_external_bgm(audio, true)` の上で `UpdateMusicStream`＋0.5 秒クロスフェード＋`SetMusicVolume`（`master_vol * bgm_vol * xfade_weight`、ポーズ時は少し抑える）で再生する。
   - 該当トラックの `.ogg` が不在または読込失敗なら警告を出してクラッシュせず、`audio_set_external_bgm(audio, false)` で **`ink_audio` 組み込みのプロシージャル BGM へ自動フォールバック** する。

---

### 2.8 見本帳 `--showcase` の P1 拡張（`showcase.c`, `ui_banner.c`）

`--showcase` のページ構成を P1 全コンテンツが確認できるよう **6 ページ**（`1`〜`6` キーまたは左右キー、`Tab` で `ui.showcase_subpage` 切替、`F12` で `build/snaps/showcase_p<1..6>.png` 保存、ページ表示 `"%s %d/6"`）に拡張する（`--showcase --smoke-frames 30` では 5 フレームごとにページ 1〜6 を巡回させて全ページを描画検証できると望ましい）：
- **ページ 1（全登場物・地形・プリミティブ）**：社、通常敵 7 種（弓妖・炎提灯・雷提灯・唐傘・鎌鼬・石鬼・墨吸い）＋エリート 4 変異（剛体・撥墨・不燃・避雷）、幕ボス 3 体（大天狗・鵺・一筆龍）、敵弾 5 種、第 2 幕の鳥居と岩柱、第 3 幕の霊柱（炎点灯・雷点灯）と落雷予兆。
- **ページ 2（墨 6 割充填＋4 属性反応）**：画面の 6 割を濃墨で覆った上での全登場物の紙抜き縁確認＋紅蓮墨嵐・雷墨連鎖・爆雷（円相）・三元共鳴（金円相＋落款「三元」）の 1 コマ。
- **ページ 3（報酬画面）**：強化札 3 択と、`Tab` キー（`showcase_subpage`）で大遺物 3 択（`REWARD_KIND_RELIC`）・候補 1 枚・候補 0 枚（`FALLBACK`）への切り替え。
- **ページ 4（タイトル / ロビー ＆ 筆スタイル選択）**：毛筆体題字「墨嵐」とロビーメニュー、`Tab` キー（`showcase_subpage`）で筆スタイル 3 種の選択画面へ切り替え。
- **ページ 5（墨庵 ＆ ポーズメニュー）**：墨庵の掛け軸ショップ画面、`Tab` キー（`showcase_subpage`）でポーズメニュー（所持札・遺物一覧と確認ダイアログ）へ切り替え。
- **ページ 6（勝利リザルト）**：全 3 幕クリア・4 反応カウント・所持札・所持大遺物・墨晶内訳付きの絵巻リザルト。

---

### 2.9 `AGENTS.md` と `README.txt` の更新（PLAN §9.1）

- **`AGENTS.md`**: 「標準コマンド」節の「通常のウィンドウ確認は `nix develop -c ./build/release/inkstorm` で起動し、ESC または閉じるボタンで終了する。」を、「通常のウィンドウ確認は `nix develop -c ./build/release/inkstorm` で起動する（タイトル/ロビー画面では ESC・「終了」または閉じるボタンで終了、ラン中は ESC でポーズメニューを開き「ロビーへ戻る」または閉じるボタンで終了する）。」へ更新する。
- **`README.txt`**: ロビー・筆スタイル 3 種・墨庵（メタショップ）・設定画面の操作、報酬画面（強化札・大遺物）とリザルト画面（ロビーへ戻る / 再出陣）、ラン中の `Esc` ポーズメニュー（「最初から」「ロビーへ戻る」は敗北精算されること）、同梱フォント 2 書体（明朝体・毛筆体）と外部 BGM のライセンス参照先（`assets/LICENSES.txt`）を追記する。

---

## 3. テスト要件

1. **`tests/test_text_table.c`**:
   - 全 10 種の `EnemyDefId`、3 ボスの異名、全 24 種の `UpgradeId` × Lv1〜3、全 9 種の `RelicId`（名前・効果）、墨庵の恒久強化 4 種と解放遺物 6 枠、筆スタイル 3 種、ヒント 8 種、全 3 幕の題字（`text_act_title(0..2) != TEXT_NONE`、`text_act_title(3) == TEXT_NONE` に更新）および短縮幕名（`text_act_short(0..2)`）がすべて非 `TEXT_NONE`・非空で、英語が印字可能 ASCII（`0x20〜0x7E`）のみであることを検証する。
2. **`tests/test_audio.c`**:
   - `EV_THUNDERBURST`、`EV_CATACLYSM`、`EV_WIND_TELEGRAPH` の各イベント消費で有効な波形（ピーク `>= 1000`、`<= 32767`、`audio_validate_finite` 真）が合成されること。
   - `audio_set_lobby_mode(a, true)` で `BGM_LOBBY` に遷移し有限・決定的に波形が出ること、`audio_set_external_bgm(a, true)` でプロシージャル楽器音が抑止され SE は鳴ること、`audio_set_paused(a, true)` で筆擦れ目標が 0 になること、`audio_trigger_ui_stamp(a)` で落款 SE が出ること。
3. **`tests/test_app_core.c`**:
   - ポーズ中やロビー/筆選択画面での UI クリック直後の `paint_suppressed`（左ボタンを離すまで筆跡が出ない規則）および `input_latch_init` リセットの境界ケースを検証する。

---

## 4. ローカル検証・視覚確認・PR と完了条件

```sh
# 1. ASan/UBSan: 変更範囲のビルドと関連テスト
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan --target inkstorm ink_render ink_audio \
        test_app_core test_audio test_app_save test_text_table test_core_smoke &&
    nix develop -c ctest --preset asan --output-on-failure \
        -R '^(test_app_core|test_audio|test_app_save|test_text_table|test_cli_.*|test_core_smoke)$'

# 2. Release と WSLg 描画経路（通常 / --no-post / shader 失敗 / font 欠損 / 併用 / アセットなし）
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
    tests/test_acts_enemies.c tests/test_determinism_run.c tests/bench_game.c tools .github \
    flake.nix flake.lock .agents docs/PLAN.md docs/architecture.md docs/game-design.md \
    docs/qa-and-testing.md                                                                     # 空
```

**完了条件（Definition of Done）**
- [ ] §2 の全画面（ロビー・筆選択・墨庵・設定・ポーズ・大遺物選択・リザルト・ヒント短冊）、第 2・3 幕と新敵/ボスの水墨描画、毛筆体フォント（グリフ欠落・二重解放なし）、外部 BGM（＋フォールバック）、`AGENTS.md` / `README.txt` / `assets/LICENSES.txt` 更新が完了している。
- [ ] 凍結ファイルおよび P4-A / P4-B の担当ファイルに差分がない。
- [ ] `test_text_table`、`test_audio`、`test_app_core`、`test_app_save`、`test_cli_*`、`test_core_smoke` が ASan/UBSan（`-Werror`）で通る。
- [ ] 見本帳 6 ページの WSLg 描画経路確認（併用 `shader,font` 含む）・崩し試験・`F1` 描画予算（`< 4.0 ms/frame`、頂点 `<= 60,000`、ブレンド切替 `<= 12`）を満たす。
- [ ] PR の CI 3 ジョブがすべて緑。
