# Phase 3-B タスク指示書：P0 の描画・音・入出力（`ink_render` / `ink_audio` / `app`）（`wt-render-audio-p0`）

> **必読**: [phase3-pre-contracts.md](phase3-pre-contracts.md)（特に §0.1 の進め方、§1.2 の所有表、§2.3 の公開読み取り契約、§2.5 のイベント表、§2.9 の app の純粋モジュール）、[PLAN.md](../PLAN.md) §9、[architecture.md](../architecture.md) §1 / §7 / §8.2 / §9、[art-direction.md](../art-direction.md) 全体（特に §5 / §8 / §9 / §11〜§15）、[game-design.md](../game-design.md) §3.2 / §5.1 / §8 / §11 / §12、[qa-and-testing.md](../qa-and-testing.md) §2 / §4 / §5、[AGENTS.md](../../AGENTS.md)、[inkstorm-verify-and-visual](../../.agents/skills/inkstorm-verify-and-visual/SKILL.md)
>
> **worktree / ブランチ**: `full-scale-dev/wt-render-audio-p0` / `feat/phase3-render-audio-p0`（P3-Pre のマージ後の `main` から分岐）。完了したら単独で `main` へ PR を出す（Pre §0.1）。
> **並列ペア**: P3-A（[phase3-sim-p0.md](phase3-sim-p0.md)）。結合と完了ゲートは [phase3-integration.md](phase3-integration.md)（P3-Int）。
> **リファレンス**: プロトタイプ `../../prototypes/inkstorm/src/render.c` / `sfx.c` / `main.c`（音の合成と音声ストリームの接続の参考。**見た目は art-direction.md を正とし、プロトタイプの図形描画は移植しない**）

---

## 0. 目的と責務境界

P0 縦切りの「見る・聴く・操作する・保存する」を実装する。

1. **`ink_render`**: 次を実装する。
   - 和紙の地、流体の可視化、パレット、6 種の筆描画プリミティブ、ポストシェーダ
   - P0 の登場物（社・弓妖・提灯妖・炎・唐傘妖・大天狗・敵弾・墨玉）と VFX プール
   - HUD、幕題字・ボス題字・敵紹介、報酬画面、リザルト、最小の `F1` 表示
   - 文字列表と日本語フォント（ASCII フォールバック）
2. **`ink_audio`**: イベント駆動のプロシージャル和楽器 SE と、プロシージャル BGM（外部 BGM がない場合の既定経路）。
3. **`app`**: 次を実装する。
   - 入力ラッチ、固定 60Hz ループと毎ティックのイベント消費、音声ストリームの接続
   - セーブ I/O と、ラン終了時・ラン中の終了時の精算
   - 本番の `--headless-smoke`、見本帳 `--showcase`、障害の注入の配線
4. **テスト**: `test_app_core`（入力ラッチの追加分）、`test_audio`、`test_app_save`、`test_text_table`、本番の `test_cli_headless_smoke`。

**禁止事項**
- ゲーム状態を書き換えない。`render` / `audio` は `const GameState *` だけを受け取り、`g->internal` を読まない（Pre §2.3）。app が `GameState` に直接書き込むのは見本帳（§2.8）だけ。それ以外はすべて `game_step` / `game_init` を通す。
- `ink_game` の内部ヘッダや凍結ファイル（Pre §1.2）を編集・include しない。使うのは `game.h` とそこから include されるヘッダだけ。
- `src/audio/*`、`src/app/save.*`、`paths.*`、`cli.*`、`frame_clock.*`、`input_latch.*`、`src/render/text_table.*`、`ui_view.h` で `raylib.h` を include しない（これらは raylib なしでテストにコンパイルされる）。
- 色の RGB 直書きはパレットの定義（§2.3）だけ。raylib の均一な描画関数（`DrawLine*`、`DrawCircle*`、`DrawRectangle*`、`DrawRing`、`DrawPoly*`、`DrawTriangleStrip`、`DrawSplineCatmullRom`）は、`F1` 表示・デバッグ表示・プリミティブの内部でだけ使う（art-direction §8 / §14.4）。
- 可変のグローバル状態を使わない。`RenderState` / `AudioState` などの状態は app が起動時に 1 回 `calloc` し、終了時に解放する。表示用の乱数は `RenderState.vfx_rng`、音の乱数は `AudioState` の RNG だけ。`g->rng` を読まない。
- ポーズ、ロビー、墨庵、設定画面、操作ヒント、外部 BGM、毛筆体フォントは Phase 4。`Esc` は終了のまま（ラン中なら §2.7 の精算をしてから終了する）。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/render/*` | 更新・新規 | §2.1〜§2.5 |
| `src/audio/*` | 更新・新規 | §2.6 |
| `src/app/*` | 更新・新規 | §2.7〜§2.9（`cli.c` / `frame_clock.c` は Pre の実装を使い、規則を変えない） |
| `assets/fonts/ui_mincho.ttf`、`assets/fonts/OFL.txt`、`assets/LICENSES.txt` | 新規 | 本文用の明朝体とライセンス（§2.5） |
| `README.txt` | 更新 | 操作方法・CLI・セーブ先（§2.10） |
| `tests/test_app_core.c` | 追記 | 入力ラッチのテスト（§3.3）。Pre の CLI・フレーム時計のケースは変えない |
| `tests/test_audio.c` / `test_app_save.c` / `test_text_table.c` | 更新 | §3 |
| `docs/art-direction.md` | 数値のみ | 見本帳で調整した太さ・不透明度などの値 |

**推奨構成**（ヘッダ名は `src/` 全体で一意にし、raylib のヘッダ名とも重ねない）

| パス | 責務 |
| :--- | :--- |
| `src/render/render.h` / `render.c`、`render_internal.h` | 公開 API とフレームの組み立て、`render` 内で共有する型（`BrushProfile`、`InkTone` など） |
| `src/render/palette.h` / `palette.c` | パレットのトークン（`(Color){` を書く唯一の場所） |
| `src/render/ink_prims.h` / `ink_prims.c` | 6 種の筆描画プリミティブ（rlgl） |
| `src/render/paper.c`・`fluid_view.c`・`post_fx.c` | 和紙の地・流体テクスチャ・ポストシェーダ |
| `src/render/draw_entities.c`、`vfx.c` | 社・敵・ボス・敵弾・墨玉、VFX プール・イベント消費・更新 |
| `src/render/hud.c`・`ui_reward.c`・`ui_result.c`・`ui_banner.c`・`xray.c` | HUD、報酬、リザルト、題字・紹介、`F1` |
| `src/render/fonts.c`、`text_table.h` / `text_table.c` | フォントと文字列表 |
| `src/audio/audio.c`・`synth_voices.c` / `.h`・`bgm_proc.c` | 公開 API・SE のボイス・BGM |
| `src/app/main.c`・`app_loop.c` / `.h`・`input_map.c` / `.h`・`audio_out.c` / `.h`・`headless.c` / `.h`・`showcase.c` / `.h` | 起動とモードの分岐、ウィンドウループ、raylib の入力から `RawInput`、音声出力、ヘッドレス、見本帳 |
| `src/app/cli.*`・`frame_clock.*`・`input_latch.*`・`save.*`・`paths.*` | raylib 非依存の純粋モジュール（テスト対象） |

> **ブランチ単独のビルド状態**: P3-A が先にマージされていなければ、`game_step` は P3-Pre の骨格（ドラッグで墨が流れる、`CMD_ABORT_RUN` が効く）。敵・報酬・リザルトは、見本帳（§2.8）でデバッグ API と公開フィールドを使って組み立てて開発する。実際のゲーム進行と結び付けた確認は P3-Int で行う。

---

## 2. 実装仕様

### 2.1 公開 API と毎ティックの契約（architecture §1.2 / §9）
```c
typedef struct {
    bool use_post;
    uint32_t fault_mask; /* APP_FAULT_SHADER / APP_FAULT_FONT（Pre §2.9） */
} RenderOptions;

void render_init(RenderState *r, const RenderOptions *opt); /* 呼ぶ前に InitWindow 済みであること */
void render_reset_vfx(RenderState *r, uint64_t seed);       /* ランの開始時。vfx_rng とプールを初期化 */
void render_consume_events(RenderState *r, const GameState *g);
void render_update_vfx(RenderState *r, const GameState *g, float dt);
void render_draw(RenderState *r, const GameState *g, const UiView *ui);
void render_close(RenderState *r);
```
- `render_init` はシェーダ・フォント・テクスチャのどれが失敗しても、ログを出して通常描画で続けられる状態にする（戻り値で中断させない）。`fault_mask` の障害は、本物の失敗と**同じ分岐**を通す（障害専用の別経路を作らない）。
- app のループ（§2.7）は **`game_step` を 1 回呼ぶたびに** `audio_consume_events` → `render_consume_events` → `render_update_vfx(r, g, INK_FIXED_DT)` を呼ぶ。描画フレームで 0 ティックや複数ティックになっても、イベントを取りこぼさない（architecture §1.2）。
- REWARD・VICTORY・GAME_OVER でも app は固定刻みで `game_step` を呼び続ける（ゲーム側は止まり、`ticks` も進まない）。報酬札の描き出しや落款などの UI アニメーションは、`render_update_vfx` で進む `RenderState` 側のタイマーで動かす。

### 2.2 イベントから演出・音へ（art-direction §11、Pre §2.5）
| イベント | 描画 | 音 |
| :--- | :--- | :--- |
| `EV_KILL` | 衝撃の向きへ `ink_splatter`、輪郭がほどけて墨溜まりになり紙に吸われる（0.6 秒）。叩きつけ撃破なら「叩きつけ！」 | 撃破（和太鼓の短打）。`g->run.chain >= 2` なら連殺（チェインが上がるほど陽音階を上げた琴） |
| `EV_DEFLECT` | 金の縁と伸びる尾、小さな打ち込みの飛沫（0.2 秒） | 偏向（竹の打音） |
| `EV_SLAM` | 壁・相手側に潑墨の飛沫と太い払い 3 本（0.3 秒） | 叩きつけ（低い太鼓） |
| `EV_IGNITE` | 「紅蓮墨嵐！」のコールアウト | 着火（炎の唸り） |
| `EV_STORM_ZAP` | `(x, y)` → `(x2, y2)` の 6〜10 節のジグザグ、白い芯は 1px（0.22 秒） | 雷撃 |
| `EV_CYCLONE` / `EV_GALE` | 朱の小印と螺旋 2 本（0.72 秒）/ 太く入ってかすれる 1 本の龍脈筆線（0.36 秒） | 渦 / 突風 |
| `EV_ORB_PICKUP` | 筆先へ吸い込まれる短い払いと金の 1 点（`EVF_ORB_AUTO` は筆先へ飛ぶ軌跡を付ける） | 水滴（自動回収は 1 ティックに 1 回だけ鳴らす） |
| `EV_SHRINE_HIT` | 灯の揺らぎと紙のひび（0.4 秒）、画面揺れ（小） | 鉦 |
| `EV_SURGE_START` | 墨リングが金に変わる（`player.surge_t > 0` の間） | 墨昇華（上行の琴） |
| `EV_BOSS_ENTER` / `EV_BOSS_PHASE` | 毛筆の題字「紅風の大天狗」と HP バー / 潑墨の弾け（1.5 秒） | 大太鼓 / 大太鼓＋鉦 |
| `EV_WAVE_CLEAR` / `EV_ACT_CLEAR` | 落款「祓」 / 大きな払いで墨を拭い落款「討」 | ウェーブクリア / 幕クリア |
| `EV_REWARD_PICK` / `EV_REWARD_REROLL` | 札に落款（白文）/ 札が墨に還り新しい札を描き出す（0.4 秒） | 報酬の決定 / 紙をめくる音 |
| `EV_ENEMY_INTRO` | 画面上部に名前と一言説明を 2 秒（同時に複数あれば順に） | — |
| `EV_SKILL_DENIED` | カーソル下の該当ピップを揺らす | — |
| `EV_GAME_OVER` / `EV_VICTORY` | 灯が消え墨が上から垂れる（1.5 秒）/ 巻物が広がりリザルトへ | — |

- 大きな演出（ボス登場・フェーズ移行・幕クリア・敗北・勝利）は同時に 2 つまで（art-direction §7）。画面全体の明滅は 1 秒に 3 回まで。画面揺れは `UiView.shake_scale` に従う。
- VFX プールの上限は architecture §9.2（`sparks[512]`、`bolts[32]`、`seals[16]`、`callouts[24]`）。満杯なら最も古いものを再利用し、メモリを確保しない。
- 筆擦れ（風筆）は `player.painting` と `player.stroke_speed` から連続音として鳴らす。

### 2.3 描画の様式（art-direction 準拠）
- **パレット**: §5 の全トークンを `PaletteId` と `Color pal_color(PaletteId)` で定義する。属性色は「外側に基本色・芯に明色」の 2 層で描く。
- **筆描画プリミティブ**: §8 の 6 種を `rlBegin(RL_TRIANGLES)` と頂点ごとの `rlColor4ub` で実装する。太さの比 2.5 倍以上、入りと払い、最小 1.2px、輪郭の切れ目 1〜2 か所（§9.1）。形のシードは `EntityId` の `index`・`gen` と部位番号から `ink_hash_u32` で作り、ボイルは `g->ticks / 6` で切り替える（§9.3）。粒子だけが `vfx_rng` を使う。1 本で数千頂点になる筆線の前には `rlCheckRenderBatchLimit(頂点数)` を呼び、1 本の途中でバッチが切れないようにする。
- **流体**: 160×90 の RGBA テクスチャを CPU で毎フレーム合成し、`UpdateTexture` とバイリニアで拡大する。濃度の対応曲線と墨溜まりは §9.4 のとおり。墨は乗算合成、属性色は通常のアルファ合成、加算は雷の芯と金の小さな光だけ（§9.7）。
- **和紙の地**: 起動時に `GenImagePerlinNoise` と繊維の線で 1 回だけテクスチャを作り、ポストシェーダの有無に関係なく敷く（§14.1）。第 1 幕の端の背景（下端の芒 3〜5 本・上端の霞）は幕の開始時に `RenderTexture` へ描いてキャッシュする（§6 / §15）。
- **描画順**: §9.5 のとおり。HUD・UI は必ずポストシェーダの後に画面へ直接描く。
- **登場物**: 社（§10.2）、弓妖・提灯妖・唐傘妖（§10.3）、大天狗（§10.5）、矢・火球・雷球・偏向弾・炎上弾・雷撃弾・墨玉（§10.6）。全員に `ink_knockout`。攻撃の構えは `EnemyComp.windup`、ボスの予兆は `BossComp.telegraph_pattern` / `telegraph`、被弾は `HealthComp.flash_t`、状態異常は `StatusComp`、墨還りは `TransformComp.dissolving` / `dissolve_t` を読む。
- **HUD**（game-design §11.5、art-direction §12.1）: カーソル周りの墨リング（20 の目盛り、墨昇華中は金）と準備ピップ 2 つ（`cyclone_cd / cyclone_cd_total`）、左上に社 HP の灯火と墨の数値、上中央に幕・ウェーブ・ノルマ（ボス戦は名前と HP バー。HP は `run.boss_id` から引き、無効なら出さない）、右上にスコアと一筆連殺（猶予ゲージ）。上端に和紙の帯（地色・不透明度 0.8）。
- **幕転換**: ACT_TRANSITION の 2.0 秒は、巻物が繰り広げられて題字「第一幕　開けた和紙の社」。
- **報酬画面**（art-direction §12.2、game-design §3.2）: 止まった戦闘画面を和紙 1 枚越しに薄く残し、その上に描く。札は縦長の胡粉の短冊（軸の印、名前、現在の Lv → 次の Lv の効果）で、`choice_count` が 0〜3 のどれでも崩れない配置にする。リロールは `kind == REWARD_KIND_UPGRADE && rerolls_left > 0` のときだけ有効の見た目。修繕は社の形の小さな札で、`repair_available` が偽なら無効の見た目。`REWARD_KIND_FALLBACK` では修繕だけを出す。
- **リザルト**: 到達した幕・ウェーブ、スコア、撃破数、最大連殺、反応回数（紅蓮墨嵐・雷墨連鎖・爆雷・三元共鳴）、取得した札、シード、墨晶の内訳（`UiView.settle`）と「クリックで再出陣」。
- **`F1`（最小版）**: `render_draw` の CPU 時間（ms）、プリミティブの頂点数、合成モードの切り替え回数、`fluid.stats.solve_ms`、生存エンティティ数、`dropped_events` を数値で出す（速度ベクトルや渦度の可視化は Phase 6）。`render_draw` の時間は描画コマンド発行の開始から `EndDrawing` の直前までを `GetTime()` で測る。

### 2.4 ポストシェーダ
- GLSL は `src/render/post_fx.c` の C 文字列リテラルに埋め込む（architecture §1.3）。**1 本のシェーダを 1 つの連結リテラル（4095 バイト以下）で書く**。`-Wpedantic` は 4095 バイトを超える文字列リテラルを `-Woverlength-strings` で警告し、`-Werror` で失敗する（gcc 13.3 で確認済み）。複数の配列に分けて実行時に連結すると、`tools/check_shaders.py` は `#version` で始まる断片だけを検査して失敗する。
- 内容は和紙の繊維、にじみ縁、ビネットまで（art-direction §14.1 の「失ってよいもの」）。形・濃淡・墨溜まりをシェーダに頼らない。
- 失敗の判定は既存の `render_init` と同じ（`IsShaderValid` と、既定シェーダの ID が返った場合）。`APP_FAULT_SHADER` のときは、読み込み結果を失敗として扱って同じ分岐へ入り、`INKSTORM: post shader unavailable; drawing directly` を出す。

### 2.5 文字列表とフォント（game-design §11.4、art-direction §9.6 / §12.4）
- **文字列表**（`text_table.c`、raylib 非依存）: `TextId` ごとに `{ja, en}` を `static const` の表に持つ。`en` は印字可能 ASCII だけにする。内容 ID からの対応関数（`text_enemy_name(EnemyDefId)`、`text_enemy_intro`、`text_boss_epithet`、`text_upgrade_name(UpgradeId)`、`text_upgrade_effect(UpgradeId, int level)`、`text_axis_name(UpgradeAxis)`、`text_act_title(int act_index)` など）を同じファイルに置く。範囲外の ID は `TEXT_NONE` を返す。
- **P0 の文字列**: 次のすべてを表に入れる。フレーバーは 1 行 30 字以内（art-direction §4）。
  - タイトル、第一幕の題字、HUD の見出し
  - 4 種の敵の名前と一言説明、大天狗の異名
  - コールアウト（「紅蓮墨嵐！」「雷墨連鎖」「叩きつけ！」「一筆連殺 ×」「墨昇華」）
  - 報酬画面（見出し・リロール・社を修繕・Lv）、8 種の札の名前と Lv1〜3 の効果、軸の名前
  - リザルトの項目と墨晶の内訳、落款の文字（祓・討）、「クリックで再出陣」
- **同梱フォント**（Phase 3 で同梱する。題字用の毛筆体は Phase 4）

| 項目 | 決定 |
| :--- | :--- |
| 採用 | しっぽり明朝（Shippori Mincho）Regular。SIL Open Font License 1.1、Google Fonts と公式リポジトリ（`fontdasu/ShipporiMincho`）で配布。築地活版系の古典的な明朝で、水墨の画面と馴染む |
| 代替 | Zen Old Mincho Regular（SIL OFL 1.1、`googlefonts/zen-oldmincho`）。採用候補が下の確認を通らないときに使う |
| 配置 | `assets/fonts/ui_mincho.ttf`（ファイル名だけを変え、中身は配布物のまま）、`assets/fonts/OFL.txt`（配布元の OFL 原文）、`assets/LICENSES.txt`（書体名・版・著作権表示・ライセンス・入手元 URL・「未改変」） |
| 改変 | **サブセット化を含めて改変しない**。OFL では改変版に Reserved Font Name の制約がかかるため、未改変で同梱してこの問題を避ける |
| 採用前の確認 | 配布元の `OFL.txt` と TTF の name テーブル（`fc-query` などで著作権・ライセンスの記載）を読み、OFL 1.1 であること・同梱と再配布が許されることを確かめる。ファイルは 10 MiB 以下（配布 zip と Git 履歴を重くしすぎないため）。どれかを満たさなければ代替へ切り替え、両方とも満たさない場合やネットワーク制約等で取得できない場合はユーザーに確認する |

- **読み込み**: `app_resolve_asset_path("fonts/ui_mincho.ttf")` で解決し、全 `ja` 文字列から `LoadCodepoints` で集めたコードポイントに ASCII（0x20〜0x7E）を足し、`qsort` で重複を除いて `LoadFontEx` に渡す（本文用と 40px 以上の題字用の 2 サイズ。P0 の題字も明朝体）。`LoadCodepoints` の結果は `UnloadCodepoints` で解放する。`SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR)`。
- **フォールバック**: 解決に失敗、`APP_FAULT_FONT`、または `IsFontValid(font) && font.texture.id != 0` でない（`IsFontValid` は GPU テクスチャを確かめない）とき、`GetFontDefault()` と `en` 文字列へ切り替え、`INKSTORM: font unavailable; using built-in ASCII font` を出す。既定フォントは `UnloadFont` しない。落款はフォント欠損時に印の形だけを押す。
- 数字は桁ごとに固定幅で描き、値が変わっても揺れないようにする。文字の下には和紙の帯か胡粉の紙抜きを敷く。

### 2.6 音（architecture §9.1、game-design §11.6）
```c
typedef struct { /* P3-B が定義：ボイスのプール、BGM の状態、音量、RNG など */ } AudioState;
void audio_init(AudioState *audio, uint64_t seed);
void audio_set_volumes(AudioState *audio, uint8_t master, uint8_t bgm, uint8_t se); /* 0..100（超過はクランプ） */
void audio_consume_events(AudioState *audio, const struct GameState *game);         /* game == NULL は no-op */
void audio_render(AudioState *audio, int16_t *samples, size_t sample_count);        /* 44100Hz・モノラル */
BgmTrack audio_bgm_track(const AudioState *audio);
bool audio_validate_finite(const AudioState *audio);
```
- SE は §2.2 の表のとおり。ボイスは固定長のプール（例 24 個）で持ち、満杯なら最も古いものを止めて使う。和楽器の音色はプロトタイプ `sfx.c` の合成（和太鼓・陽音階の琴・風・炎・雷・鉦）を移植して広げる。
- **BGM の状態**（毎回 `game` の状態から決める。イベントの取りこぼしに左右されない）

| 状態 | `BgmTrack` |
| :--- | :--- |
| ACT_TRANSITION / PLAYING / WAVE_AFTERMATH / REWARD で、`run.wave_index < GAME_BOSS_WAVE_INDEX` | `BGM_BATTLE` |
| PLAYING / WAVE_AFTERMATH で、`run.wave_index == GAME_BOSS_WAVE_INDEX` | `BGM_BOSS` |
| VICTORY | `BGM_VICTORY`（ジングルを 1 回鳴らし、その後は余韻の環境音） |
| SHRINE_BREAK / GAME_OVER | `BGM_NONE`（風音だけ） |

- 曲の切り替えは 0.5 秒のクロスフェードにする。`BGM_LOBBY` は P0 では使わない。外部 BGM（`assets/audio/`）の読み込みは Phase 4（`.gitignore` が `*.wav` を除外している点も Phase 4 で扱う）。
- **数値の安全**: ミックスは `float` で行い、ソフトクリップしてから `[-32767, 32767]` にクランプして `int16_t` に変換する（範囲外の float から整数への変換は未定義動作）。合成状態に NaN・Inf を残さない（`audio_validate_finite` で検査できるようにする）。
- `audio` は `malloc` しない。WAV 書き出し（PLAN §3.2 の表）は Phase 6 のデモ収録で扱う。

### 2.7 app：ループ・入力・終了時の精算
```text
frame:
    raw = input_map_read(&render, g)            // raylib の入力 → RawInput（札・リザルトのクリックはここで UI 操作に変換）
    input_latch_feed(&latch, &raw)
    n = frame_clock_advance(&clock, GetFrameTime())
    repeat n:
        in  = input_latch_take_tick(&latch)
        cmd = input_latch_take_command(&latch, g->phase)
        game_step(g, &in, &cmd)
        audio_consume_events(audio, g); render_consume_events(render, g); render_update_vfx(render, g, INK_FIXED_DT)
        app_after_tick(&app, g)                 // 精算・セーブ（§2.9）
    if VICTORY / GAME_OVER かつ input_latch_take_confirm(&latch): 新しいランを始める
    audio_out_pump(&audio_out, audio)
    render_draw(render, g, &ui)
on exit (Esc・閉じるボタン): app_finish_run(&app, g)  // ラン中なら中断して精算・保存
```
- **入力ラッチの規則**（`input_latch.c`。Pre §2.9 の API）

| 規則 | 内容 |
| :--- | :--- |
| 移動 | フレームの最初の `take_tick` で「前回消費した位置 → 現在位置」を渡し、同じフレームの 2 回目以降は移動 0（`pmx == mx`）。前回位置は消費したときだけ更新する（ティックが 0 回のフレームが続いても移動を失わない） |
| 押下 | 渦印・風斬り・報酬の操作・決定は、ティックが消費するまで保持し、消費したら 1 回で消す。消費前の複数回の押下は 1 回にまとめる |
| 報酬の操作 | `take_command` は `phase == RUN_PHASE_REWARD` のときだけ保持中の操作をコマンドにし、それ以外のフェーズでは保持中の報酬操作を捨てる（戦闘中に押したキーが次の報酬画面で効かないように）。同時に複数あれば `pick` → `repair` → `reroll` の順で 1 つだけ採る |
| 筆の抑止 | `ui_click` のフレームから左ボタンを離すまで `paint = false`（札のクリックが、次ウェーブの最初のティックで筆跡にならないように） |

- **座標**: ウィンドウは 1280×720 の固定サイズのまま。ウィンドウ座標を論理座標（1280×720）として扱う。
- **報酬画面のクリック**: 札・修繕・リロールの矩形は `ui_reward.c` の配置関数 1 つから求め、描画（`render_draw`）と当たり判定（`input_map_read`）の両方で使う（配置を 2 か所に書かない）。ホバーは `UiView.hover_choice`。
- **新しいラン**: `game_init(g, &cfg)` で同じ確保領域を初期化し（再確保しない）、`render_reset_vfx(render, cfg.seed)` を呼ぶ。シードは `uint64_t s = UINT64_C(0x494e4b53544f524d) ^ (uint64_t)meta.runs_played;` に対する `ink_rng_next(&s)` の値とし、`time()` を使わない。`cfg = meta_build_run_config(&meta, BRUSH_STYLE_HEAVY, seed)`。
- **ラン中の終了**（Pre §4 の 15）: `WindowShouldClose()` が真になったら、セーブが有効で、フェーズが VICTORY / GAME_OVER 以外で、ラン開始直後の幕転換中（`act_index == 0` かつ ACT_TRANSITION）でなければ、`CMD_ABORT_RUN` で `game_step` を 1 回呼び、`meta_settle_run` と保存をしてから終了する。
- **設定**: 音量（`master` / `bgm` / `se`）と画面揺れは `meta.settings` から反映する。ポストシェーダは `--no-post` がなく、かつ `meta.settings.post_shader_on` のときに有効。
- **音声出力**（`audio_out.c`）: 通常のウィンドウ起動のときだけ `InitAudioDevice` を呼び、`IsAudioDeviceReady()` が偽なら無音で続ける。`LoadAudioStream` の前に `SetAudioStreamBufferSizeDefault(AUDIO_STREAM_FRAMES)`（1024）を呼び、`IsAudioStreamProcessed` が真の間（1 フレームに最大 2 回）、**毎回ちょうど `AUDIO_STREAM_FRAMES`** を `audio_render` して `UpdateAudioStream` に渡す（サブバッファより少なく渡すと途切れの原因になる）。raylib のオーディオコールバック（別スレッド）は使わない（ゲーム側の状態とのデータ競合を避けるため）。
- `--smoke-frames N` と見本帳は音声デバイスを開かない。`--smoke-frames` は 1 フレーム 1 ティックで `game_autoplay_input` / `_command` を使う。
- **障害の注入**: 起動時に `getenv("INKSTORM_DEBUG_FAULTS")` を `cli_parse_debug_faults(value, stderr)` に渡し、非 0 なら `INKSTORM: debug faults enabled: <内容>` を 1 回出して `RenderOptions.fault_mask` に渡す。

### 2.8 CLI・ヘッドレス・見本帳（qa-and-testing §4）
- CLI の解析は Pre の `cli_parse` をそのまま使う（規則は Pre §2.9 の表）。Usage エラーは `cli_print_usage(stderr, argv[0])` の後に `EXIT_FAILURE`。モードは `APP_MODE_WINDOW` / `APP_MODE_HEADLESS` / `APP_MODE_SHOWCASE`、セーブは `CliOptions.save_mode` に従う。
- **`--headless-smoke`（本番）**: `game_create(NULL)` の後、`game_autoplay_input` / `_command` で 600 回 `game_step` を呼び、毎回 `audio_consume_events` と `audio_render`（735 サンプル）を呼ぶ。

| 確かめること | 内容 |
| :--- | :--- |
| `ticks` の契約 | 各 `game_step` での増分が 0 か 1 で、0 のときはステップ後のフェーズが REWARD / VICTORY / GAME_OVER（Pre §2.7） |
| 進行 | `fluid.steps > 0`（幕転換を抜けて戦闘が動いた） |
| 健全性 | `fluid_validate_finite` と `audio_validate_finite` |
| アセット | `Font: assets/fonts/ui_mincho.ttf found` か `Font: unavailable (ASCII fallback)` を 1 行出す（CI の `dist-verify` のログで同梱を確かめるため） |

  すべて満たせば `Headless smoke passed. ticks=<n> hash=0x…` を出して終了コード 0、どれかが偽なら理由を出して `EXIT_FAILURE`。raylib の関数（ウィンドウ・音声デバイス）とセーブ I/O を呼ばない。
- **`--showcase`（見本帳、art-direction §14.2 の P0 版）**: `game_create(NULL)` と `game_debug_spawn_*`、公開フィールドへの直接の書き込みで、静的な場面を組み立てる。`game_step` は呼ばず、ボイルを確かめるため app が毎フレーム `g->ticks` を 1 進める（見本帳だけの例外）。ページは `1`〜`4` または左右キーで切り替える。

| ページ | 内容 |
| :--- | :--- |
| 1 登場物 | 社、P0 の敵 4 種 × 3 状態（待機・構え `windup = 0.8`・被弾 `flash_t > 0`）、大天狗の予兆、全敵弾 × {通常・偏向・炎上・雷撃}、墨玉、墨・炎・雷とその重なりの流体の帯、パレットの色見本、6 種のプリミティブの見本 |
| 2 墨 6 割 | 画面の約 6 割を濃墨（1.2）で覆い、その上にページ 1 の登場物を置く（紙抜き縁の試験、art-direction §14.3） |
| 3 報酬 | REWARD 相当（3 枚の札・リロール残り・修繕）。`Tab` で札 1 枚・0 枚（FALLBACK）の配置にも切り替える |
| 4 リザルト | VICTORY 相当（内訳を含む） |

- 見本帳では `F12` で、作業ディレクトリ相対の `build/snaps/showcase_p<ページ>.png` へ `LoadImageFromScreen` → `ExportImage` で保存する（見本帳だけの開発用キー。保存先のディレクトリは事前に作る。失敗してもログだけで続ける）。

### 2.9 セーブ I/O（`src/app/save.c`、architecture §8.2）
| 項目 | 規則 |
| :--- | :--- |
| 保存先の解決 `save_resolve_path(explicit, xdg_data_home, home, exe_dir, out, size)` | 純粋関数（環境変数は呼び出し側が渡す。`exe_dir` の取得は `src/app/paths.c` に `app_executable_dir()` を追加して `app_resolve_asset_path()` と共有する）。① `explicit` ② 絶対パスの `XDG_DATA_HOME` + `/inkstorm/save.txt`（相対パスや空は無視）③ `HOME` + `/.local/share/inkstorm/save.txt` ④ `exe_dir` + `/inkstorm_save.txt`。どれも使えないか `size` に収まらなければ `false` |
| 読み込み `save_load(path, meta)` | 8 KiB を上限に読み、NUL 終端して `meta_deserialize` に渡す。ない → `SAVE_LOAD_MISSING`、上限超え・解釈失敗 → `SAVE_LOAD_CORRUPT`、読み込みエラー（ディレクトリだった場合を含む）→ `SAVE_LOAD_IO_ERROR`。成功以外では `meta` を既定値にする |
| 書き込み `save_write(path, meta, ops)` | `ops == NULL` は既定の関数群。親ディレクトリがなければ作る（0700、途中も含む。`EEXIST` は成功扱い）。`<path>.tmp` へ `meta_serialize` の内容を書き、`fwrite`・`fflush`・`fclose`・`rename` の戻り値をすべて確かめる。どれかが失敗したら `.tmp` を消し、既存の `save.txt` に触れず `false`。`fflush` が失敗しても `fclose` は 1 回だけ呼ぶ |
| app の使い方 | 起動時に 1 回読む（`MISSING` は初回として既定値、`CORRUPT` / `IO_ERROR` は `stderr` に出して既定値で続ける）。VICTORY / GAME_OVER に入って `meta_settle_run` が `true` を返したティックと、§2.7 のラン中の終了時に 1 回書く。失敗はログだけで続ける。`SAVE_MODE_DISABLED` ではファイルに一切触れない |

### 2.10 `README.txt`
- 操作方法（左ドラッグ＝風筆、右クリックまたは `E`＝渦印、`Space`＝風斬り、報酬画面の `1`〜`3` / クリック＝札、`R`＝リロール、`4`＝社を修繕、リザルトのクリック / `Enter`＝再出陣、`F1`＝計測表示、`Esc` / 閉じるボタン＝終了（ランの途中なら敗北として精算））、CLI（`--no-post`、`--no-save`、`--save-path`、`--headless-smoke`）、セーブ先（§2.9 の解決順）、フォントがない場合は英語表示になること、同梱フォントのライセンスは `assets/LICENSES.txt` を見ること。開発専用の情報（CMake、Nix、テスト、見本帳、障害の注入）は書かない。

---

## 3. テスト要件（Unity、ASan/UBSan で全通過）

### 3.1 `tests/test_audio.c`
`GameState` は `game_create(NULL)` で確保し、イベントは `events[]` と `event_count` に直接書いて用意する（テストだけの書き込み）。

| # | ケース | 判定 |
| :---: | :--- | :--- |
| 1 | 初期状態 | `audio_init` 直後は `BGM_NONE` で、`audio_render` が全サンプル 0。`audio_consume_events(a, NULL)` と `sample_count = 0` で何も起きない |
| 2 | SE | §2.2 の表で音が付く各イベントを 1 件ずつ消費し、1 秒分を合成すると、ピークが 1000 以上・32767 以下で、`audio_validate_finite` が真 |
| 3 | 負荷 | 全種類のイベントを 1 ティック 128 件ずつ 120 ティック消費しながら合成しても、有限でクリップが `[-32767, 32767]` に収まる |
| 4 | BGM の状態 | §2.6 の表の各行を `phase` と `run.wave_index` で作り、`audio_bgm_track` が一致する |
| 5 | 音量 | `master = 0` で全サンプル 0。`se = 0` で SE だけのときは 0。`200` は 100 として扱われる |
| 6 | 決定性 | 同じシード・同じイベント列の 2 つの `AudioState` の出力が `memcmp` で一致する。毎ティック消費と「0, 2, 1 ティックずつまとめて消費」で、同じティック数の後の出力が一致する |

### 3.2 `tests/test_app_save.c`
一時ディレクトリは `mkdtemp`（`$TMPDIR` か `/tmp`）で作り、終わったら中身ごと消す。実際の `HOME` と `XDG_DATA_HOME` に触れない（`save_resolve_path` には値を引数で渡す）。

| # | ケース | 判定 |
| :---: | :--- | :--- |
| 1 | 保存先の解決順 | 4 段の優先順位、相対・空の `XDG_DATA_HOME` の無視、`HOME` も空なら `exe_dir`、全部なければ `false`、`size` 不足で `false`（テーブル駆動） |
| 2 | 往復 | 全キーに既定以外を入れた `MetaState` を書いて読むと、フィールドごとに一致し `SAVE_LOAD_OK` |
| 3 | ディレクトリの作成 | 存在しない `<tmp>/a/inkstorm/save.txt` に書くと、途中のディレクトリが作られる |
| 4 | 失敗の注入 | `SaveIoOps` で `fopen`・`fwrite`（短い書き込み）・`fflush`・`fclose`・`rename` を 1 つずつ失敗させると、どれも `false` で、既存の `save.txt` がバイト単位で変わらず、`.tmp` が残らない |
| 5 | 権限による失敗 | ディレクトリを `0500` にすると書き込みが `false` で既存のファイルが変わらない（`geteuid() == 0` なら `TEST_IGNORE`） |
| 6 | 読み込みの分類 | 不在 → `MISSING`、`version=2` → `CORRUPT`、8 KiB 超 → `CORRUPT`、パスがディレクトリ → `IO_ERROR`。いずれも `meta` は既定値 |

### 3.3 `tests/test_app_core.c`（入力ラッチの追加分）
Pre の CLI・フレーム時計のケースは残し、§2.7 の規則を次のケースで検証する。

| # | ケース | 判定 |
| :---: | :--- | :--- |
| 1 | 移動は最初のティックだけ | `(0, 0)` で初期化し `(100, 50)` を与えると、1 回目の `take_tick` が `pmx = 0 → mx = 100`、2 回目が `pmx == mx == 100` |
| 2 | ティック 0 回のフレーム | `(10, 0)`・`(20, 0)` を続けて与えてから取り出すと、`pmx = 0 → mx = 20`（移動を失わない） |
| 3 | 押下の保持とまとめ | 渦印を押したフレームで取り出さず、次のフレーム（押下なし）で取り出すと `cyclone == true` が 1 回だけ。消費前に 2 回押しても 1 回 |
| 4 | 報酬の操作 | PLAYING で `pick_pressed = 1` → `CMD_NONE` で、その後 REWARD で取り出しても `CMD_NONE`（捨てられている）。REWARD で `pick_pressed = 1` → `CMD_PICK_REWARD`（`arg = 1`）が 1 回。`pick` と `reroll` を同時に押すと `pick` だけ |
| 5 | 筆の抑止 | `ui_click` かつ `left_down` のフレーム以降、`left_down` が続く間は `paint == false`。一度離して押し直すと `paint == true` |
| 6 | 決定 | `confirm_pressed` が `input_latch_take_confirm` で 1 回だけ真 |

### 3.4 `tests/test_text_table.c`
- `TEXT_NONE` 以外の全 `TextId` で `ja` / `en` が非 NULL・非空で、`en` が印字可能 ASCII だけ。
- 全 `EnemyDefId`、全 `UpgradeId` × Lv1〜3、全 `UpgradeAxis`、`act_index = 0` の対応関数が `TEXT_NONE` 以外を返し、範囲外（`ENEMY_DEF_COUNT`、Lv 0 と 4 など）は `TEXT_NONE` を返す。

---

## 4. ローカル検証・視覚確認・PR と完了条件

AGENTS.md の分担に従い、ローカルでは変更範囲のビルド・関連テストと、CI で代替できない WSLg の描画・視覚確認を行う。フルの ASan/UBSan + CTest、`format-check`、`lint`（GLSL 検証を含む）、`dist` は PR の CI に任せる（CI が失敗したときだけ、該当のチェックをローカルで再現する）。

```sh
# 1. ASan/UBSan: 変更範囲のビルドと関連テスト
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan --target inkstorm ink_render ink_audio \
        test_app_core test_audio test_app_save test_text_table test_core_smoke &&
    nix develop -c ctest --preset asan --output-on-failure \
        -R '^(test_app_core|test_audio|test_app_save|test_text_table|test_cli_.*|test_core_smoke)$'

# 2. スモークがセーブ先に書かないこと
tmp=$(mktemp -d) && XDG_DATA_HOME="$tmp" HOME="$tmp" ./build/asan/inkstorm --headless-smoke &&
    test -z "$(find "$tmp" -mindepth 1 -print -quit)" && echo 'no save written' && rm -rf "$tmp"

# 3. Release と WSLg の 4 経路（見本帳と、骨格または P3-A のゲーム）
nix develop -c cmake --preset release && nix develop -c cmake --build --preset release &&
    ./build/release/inkstorm --headless-smoke &&
    nix develop -c ./build/release/inkstorm --showcase --smoke-frames 30 &&
    nix develop -c ./build/release/inkstorm --showcase --no-post --smoke-frames 30
mkdir -p build/snaps
nix develop -c ./build/release/inkstorm --showcase                                         # 経路 1 通常
nix develop -c ./build/release/inkstorm --showcase --no-post                               # 経路 2 --no-post
INKSTORM_DEBUG_FAULTS=shader nix develop -c ./build/release/inkstorm --showcase            # 経路 3 シェーダ失敗
INKSTORM_DEBUG_FAULTS=font nix develop -c ./build/release/inkstorm --showcase              # 経路 4 フォント欠損

# 4. 境界とアートの機械的な確認（結果が注記どおり）
grep -rn 'raylib.h' src/audio src/app/save.* src/app/paths.* src/app/cli.* src/app/frame_clock.* \
    src/app/input_latch.* src/render/text_table.* src/render/ui_view.h                          # 0 件
grep -rnE '(->|\.)internal\b' src/render src/audio src/app                                     # 0 件
grep -rnE 'Draw(Line|Circle|Rectangle|Ring|Poly|TriangleStrip|Spline)' src/render              # xray.c・デバッグ・ink_prims.c だけ
grep -rnE '\(Color\)\{' src/render src/app                                                     # palette.c だけ
grep -rn 'game_debug_' src/app | grep -v showcase.c                                            # 0 件
git diff --stat main -- CMakeLists.txt src/core src/fluid src/ecs src/game tools                # 空
```

- 経路 3・4 では、`INKSTORM: debug faults enabled` と、`post shader unavailable; drawing directly` / `font unavailable; using built-in ASCII font` のログを確かめる。表示が経路 2 と同じ構成で崩れず、経路 4 では HUD・報酬・リザルトが英語で読めること。
- 各経路で見本帳の 4 ページを `F12` で保存し（経路ごとに `build/snaps/<経路名>/` へ移す）、通常のウィンドウ起動で墨を描いた戦闘画面と、`Esc` でラン中に終了したときの精算（`--save-path "$tmp/save.txt"` で `runs_played=1` が書かれる）も確かめる。撮影時にビルド・経路・ページを記録する。
- 崩し試験（art-direction §14.3）は、ページ 1 とページ 2 の画像で行う。
  ```sh
  ffmpeg -loglevel error -y -i in.png -vf format=gray     out_gray.png  # グレースケール
  ffmpeg -loglevel error -y -i in.png -vf boxblur=5:1     out_blur.png  # 5px ぼかし
  ffmpeg -loglevel error -y -i in.png -vf scale=iw/4:-1   out_25.png    # 25% 縮小
  ```
- `F1` で、見本帳ページ 1・2 と、ドラッグで画面の半分以上に墨を描いた戦闘画面の `render_draw < 4.0 ms`、頂点数 `<= 60,000`、合成モードの切り替え `<= 12` を確かめる（art-direction §15）。

**アートのセルフチェック（art-direction §14.4 の P0 版）**
- [ ] 手順 4 の `Draw*` と `(Color){` の結果が注記どおり。
- [ ] 見本帳 4 ページを 4 経路で撮り、崩し試験 4 種（グレースケール・ぼかし・縮小・墨 6 割）で社・敵・弾・墨玉が判別できる。
- [ ] 通常経路と `--no-post` の同じページを並べ、形・濃淡・視認性の順位が保たれている。
- [ ] `F1` の値が §15 の予算内。
- [ ] §13 の NG に当たるものがない。対象は、均一な太さの輪郭、正円の目、RGB 直書き、加算合成の多用、円の点の粒子、完全な円の衝撃波、縁のない黒い敵、毎フレームの乱数のふるえ、UI をポストシェーダの前に描くこと、朱の大きな面、既存作品を連想させる固有の意匠。

**PR**: 最新の `main` を取り込み（P3-A が先に入っていれば、手順 1・3 と通常起動での戦闘・報酬・リザルトを再確認）、ユーザーの許可を得てから inkstorm-pr-and-coderabbit の手順で PR を作る。PR 本文には次を書く。
- 変更概要、§3 のテスト結果、4 経路とアートのセルフチェックの結果、キャプチャの一覧（パス・ビルド・経路。画像はコミットしない）
- 同梱フォントの確認結果（書体・版・ライセンス・ファイルサイズ）
- art-direction.md へ反映した数値、契約変更要求（なければ「なし」）

**静的解析・ビルドで引っかかりやすい点**（CI の `static-analysis` で指摘されやすいもの）
- `-Woverlength-strings`（§2.4）。文字列表の 1 つの文字列やシェーダが 4095 バイトを超えない。
- `-Wshadow`: raylib の型・関数名（`Rectangle`、`Vector2`、`Color` など）と同じ名前の局所変数。
- cppcheck の `constParameterPointer`（書き換えないポインタ引数は `const` を付ける。`render_draw` の `RenderState *r` は `F1` 計測値やテクスチャ更新で書き換えるため非 `const`）、`variableScope`。clang-analyzer の `unix.Stream`（`fopen` 後のすべての経路で `fclose`）。
- LeakSanitizer: `test_app_save` の `FILE *` と一時ディレクトリ、`--headless-smoke` の `game_destroy`。raylib を使う経路は CI で動かないので、`UnloadFont` / `UnloadTexture` / `UnloadRenderTexture` / `UnloadAudioStream` / `CloseAudioDevice` と `calloc` した状態の解放の対応を目で確かめる。
- ローカル（21 / 2.18）と CI（18 / 2.13）で結果が違ったら CI を正とする。

**完了条件（Definition of Done）**
- [ ] §2 の仕様を満たし、凍結ファイルと P3-A の担当に差分がない（手順 4 の最後）。
- [ ] `test_app_core`（Pre のケース＋ラッチ 6 ケース）、`test_audio`（6 ケース）、`test_app_save`（6 ケース）、`test_text_table`、`test_cli_usage_*`、`test_cli_headless_smoke`、`test_core_smoke` が ASan/UBSan（`-Werror`）で通る。
- [ ] スモークがセーブを書かず（手順 2）、ラン中の終了で精算・保存される。
- [ ] 4 経路の確認とアートのセルフチェックを終えた。
- [ ] PR の CI 3 ジョブがすべて緑で、`dist-verify` のログに `Font: assets/fonts/ui_mincho.ttf found` が出ている（配布 zip へのフォントの収録の確認）。
