# Phase 3-A タスク指示書：P0 ゲームシミュレーション（`ink_game`）（`wt-sim-p0`）

> **必読**: [phase3-pre-contracts.md](phase3-pre-contracts.md)（特に §0.1 の進め方、§1.2 の所有表、§2 の凍結契約）、[PLAN.md](../PLAN.md) §9、[architecture.md](../architecture.md) §2 / §3 / §5〜§7、[game-design.md](../game-design.md) §1〜§3 / §5〜§9 / §12、[qa-and-testing.md](../qa-and-testing.md) §2 / §3、[AGENTS.md](../../AGENTS.md)、[inkstorm-fluid-sim](../../.agents/skills/inkstorm-fluid-sim/SKILL.md)、[inkstorm-content-authoring](../../.agents/skills/inkstorm-content-authoring/SKILL.md)
>
> **worktree / ブランチ**: `full-scale-dev/wt-sim-p0` / `feat/phase3-sim-p0`（P3-Pre のマージ後の `main` から分岐）。完了したら単独で `main` へ PR を出す（Pre §0.1）。
> **並列ペア**: P3-B（[phase3-render-audio-p0.md](phase3-render-audio-p0.md)）。結合と完了ゲートは [phase3-integration.md](phase3-integration.md)（P3-Int）。
> **リファレンス**: プロトタイプ `../../prototypes/inkstorm/src/sim.c`（流れ・式の参考。**数値と規則は game-design.md を正とする**）

---

## 0. 目的と責務境界

P0 縦切り（game-design §12）のゲームシミュレーションを `ink_game` に実装する。対象は次のとおり。
- 固定 60Hz の 9 段パイプライン
- 3 つの技と豪筆、墨経済・一筆連殺・墨昇華、社
- 紅蓮墨嵐と雷墨連鎖
- 弓妖・提灯妖（炎）・唐傘妖・大天狗
- 第 1 幕のウェーブ表、強化札 8 種と報酬・リロール・修繕、ランの終了（勝利・敗北）

決定性は `test_determinism_run`、反応は `test_elements`、報酬は `test_roguelike_meta`（ラン部）で、`game_step < 4.0 ms/tick` は `bench_game` で実証する。

**禁止事項**
- 凍結ファイル（Pre §1.2）を編集しない。必要になったら Pre §1.3 の契約変更要求に回し、当面は `GameInternal` で代替する。
- `raylib.h` を include しない。日本語・英語の表示文字列を持たない（表示はすべて内容 ID と `GameEvent` で `ink_render` に渡す）。
- `game_step` の中で `malloc` / `free` しない。可変の `static`（関数内の作業用 `static` 配列を含む）を使わない（`static const` の表は可）。作業用の配列はスタックか `GameInternal` に置く。
- `rand()` / `time()` / 実時間でゲーム状態を変えない。乱数は `g->rng`（`ink_rng_next` / `ink_rng_f01`）だけ。
- `fluid` の作業用配列（`u0`、`v0`、`div`、`p`、`curl`、`dye0`）を読まない（`fluid_step` の内部の都合で中身の意味が変わりうる）。読んでよいのは `u`、`v`、`solid`、`dye`（`fluid_channel_const/mut` 経由）、`cfg`、`stats` だけ。
- P1 以降の内容（爆雷・三元共鳴の判定、第 2・3 幕、他の敵、エリート、大遺物、朱筆・細筆の固有挙動、山颪、操作ヒントの発火、エンドレス）を実装しない。パイプライン上の位置（elements の段 2・3 など）は空けておく。
- P3-B の担当（`src/render/*`、`src/audio/*`、`src/app/*`、`assets/*`、`README.txt`）を編集しない。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/game/game.c` | 更新 | 生成・初期化・9 段パイプラインと遷移（§2.1 / §2.10） |
| `src/game/game_internal.h` | 更新 | `GameInternal`（`RunModifiers`、各種タイマー・CD、累積距離など）。自己完結にする（Pre §2.1） |
| `src/game/hash.c` | 更新 | `game_hash_sections`（§2.11） |
| `src/game/debug_api.c` / `autoplay.c` | 更新 | 本実装（§2.11） |
| `src/game/<subdir>/*.c` / `*.h` | 新規 | 下の推奨構成 |
| `tests/test_elements.c` / `test_determinism_run.c` / `bench_game.c` | 更新 | §3 |
| `tests/test_roguelike_meta.c` | 追記 | ラン部のテスト（§3.3）。P2-B のメタ部のテストは変えない |
| `docs/game-design.md` | 数値のみ | 実プレイで数値を調整したら、その値へ更新する（規則は変えない） |

**推奨構成**（ヘッダ名は `src/` 全体で一意にする。追加したら `find src -name '*.h' -printf '%f\n' | sort | uniq -d` が空であることを確認する）

| パス | 責務 |
| :--- | :--- |
| `src/game/player/player_skills.c` / `.h` | 風筆・渦印・風斬り、墨経済、一筆連殺、墨昇華、社の被弾後処理 |
| `src/game/elements/elements.c` / `.h` | `elements_step`（紅蓮墨嵐・雷墨連鎖・状態付与・弾の属性変化） |
| `src/game/combat/damage.c` / `.h` | ダメージ計算、状態異常の更新、吹き飛び・叩きつけ |
| `src/game/enemies/enemy_defs.c` / `.h` | `EnemyDef` 表と生成関数の登録 |
| `src/game/enemies/archer.c`・`lantern.c`・`umbrella.c`・`boss_tengu.c`・`projectiles.c` | 敵ごとの行動と敵弾 |
| `src/game/roguelike/waves.c` / `.h` | `ActDef`・ウェーブ表・増援スポーン |
| `src/game/roguelike/rewards.c` / `.h` | 報酬の提示・リロール・修繕 |
| `src/game/roguelike/upgrades.c` / `.h` | `UpgradeDef`、`RunModifiers` の再計算、7 フックの呼び出し |

- サブディレクトリの `.c` は `GLOB_RECURSE`（CMake）と `find`（`tools/dist.mk`）で自動収録される。別のサブディレクトリのヘッダは `#include "elements/elements.h"` のように `src/game` からの相対で書く（include パスは `src/game` だけ）。
- 定義テーブルは `const` の指示付き初期化子にし、内容ごとの挙動を巨大な `switch` に足さない（inkstorm-content-authoring）。

> **ブランチ単独のビルド状態**: P3-Pre の骨格のおかげで、このブランチは全ターゲットがビルドでき、CI も緑にできる。`inkstorm` の描画は Pre の暫定版（または先にマージされた P3-B の本実装）。`--headless-smoke` は `game_autoplay_input` で本ブランチの `game_step` を通る。

---

## 2. 実装仕様

### 2.1 9 段パイプラインの P0 対応（architecture §3.2 準拠）

| 段 | P0 で実装する内容 | 詳細 |
| :---: | :--- | :--- |
| 1 | `event_count = 0` → 入力の検証（Pre §2.5）→ コマンド処理（下表）→ 停止フェーズ分岐。**分岐はコマンド処理後のフェーズで判定する**（Pre §2.7）。WAVE_AFTERMATH・ACT_TRANSITION・SHRINE_BREAK の各分岐の中身は §2.10 | §2.8 / §2.10 |
| 2 | ヒットストップ（叩きつけ 0.05 秒）。残っている間は段 3〜8 を飛ばし、`ticks` と `phase_t` は進める | |
| 3 | 保留インパルスの注入（P0 に発生源はないが、注入とクリアは実装する）、風筆・渦印・風斬り、墨経済、`on_stroke` / `on_cyclone` / `on_gale` | §2.3 |
| 4 | `fluid_step(&g->fluid, INK_FIXED_DT)` | |
| 5 | `elements_step`（段 1・4・5。段 2・3 は Phase 4）→ `fluid_sanitize_channels` | §2.4 |
| 6 | 増援スポーン（第 1 幕にギミックはない） | §2.7 |
| 7 | 7a 敵 AI・大天狗 → 7b 敵弾 → 7c 物理・叩きつけ・`on_hit` / `on_kill` → 7d 墨玉 → 7e `on_tick` | §2.5 / §2.6 |
| 8 | `ecs_flush_destroy`（倒したボスなら `run.boss_id` を無効 ID に戻す）→ 遷移の優先判定（敗北 ＞ ボス撃破 ＞ ノルマ到達）→ `ticks++` | §2.10 |

| コマンド | 有効な条件 | 効果 |
| :--- | :--- | :--- |
| `CMD_PICK_REWARD` | REWARD、`kind == REWARD_KIND_UPGRADE`、`0 <= arg < choice_count` | 札の Lv を +1（未所持なら `run.upgrades[]` の末尾に追加）→ `RunModifiers` を再計算 → `EV_REWARD_PICK` → 次ウェーブを開始 |
| `CMD_REROLL_REWARD` | REWARD、`kind == REWARD_KIND_UPGRADE`、`rerolls_left > 0` | `rerolls_left--` → 3 枚とも引き直す（`serial++`）→ `EV_REWARD_REROLL` |
| `CMD_REPAIR_SHRINE` | REWARD、`reward.repair_available` | 社 HP +1（最大まで）→ `EV_REWARD_PICK`（`value = -1`、`EVF_REWARD_REPAIR`）→ 次ウェーブを開始 |
| `CMD_ABORT_RUN` | Pre §2.7 のフェーズ | GAME_OVER へ（`EVF_RUN_ABORTED`） |
| `CMD_START_ENDLESS` / `CMD_PICK_ENDLESS_CRYSTAL` / 範囲外の種別 | なし | 常に no-op |

- 無効なコマンドは状態を一切変えない（architecture §3.2）。RNG も消費しない。
- `repair_available` は、`kind == REWARD_KIND_UPGRADE` なら `hp < max_hp`、`kind == REWARD_KIND_FALLBACK`（候補 0 件）なら常に `true`（社が満タンでも回復量 0 で進める。他に有効なコマンドがなく、進行不能になるのを防ぐ）。

### 2.2 単位・決定性・数値の規則
- **単位**: 位置と距離は画面 px、流体への注入は格子単位（セル、セル/s）。`fluid_sample` はセル/s を返すので、ゲーム側の px/s へは `× FLUID_CELL_PX` で換算する（プロトタイプの `fluid_sample` は px/s を返していた。式を移植するときに二重に掛けない）。プロトタイプの注入式（風筆の `mdx * 0.58` など）は、もともとセル/s で注入しているのでそのまま使える。
- **時間**: タイマーは秒の `float` で持ち、`INK_FIXED_DT` ずつ減らす（増やす）。秒数の到達判定は `t + 0.5f * INK_FIXED_DT >= duration` のように半ティックの余裕をつける（`1/60` の累積誤差で 1 ティックずれないようにする。例: 2.0 秒の幕転換がちょうど 120 ティックで終わる）。一筆連殺の猶予と墨昇華は、流体が動くフェーズ（PLAYING と WAVE_AFTERMATH）でだけ減る。
- **乱数**: `g->rng` だけを使う。乱数を引く箇所と順序は決定的にする（条件によって引いたり引かなかったりするのはよいが、条件自体が状態だけで決まること）。`game_autoplay_*` は `g->rng` を使わない。
- **走査**: ECS は index 昇順。各システムの開始時に `const uint32_t hw = g->ecs.high_water;` を取り、その上限で回す（走査中にスポーンしたものは次のシステムから処理する。Pre §4 の 3）。
- **容量**: `game_spawn_entity` が無効 ID を返したら（512 体の満杯）、その生成だけを諦めて処理を続ける（敵・弾・墨玉のどれでも、クラッシュや状態の破損を起こさない）。
- **数え方**: `run.kills` はボスを含むすべての撃破、`run.wave_kills` はノルマの対象（その幕の通常敵）だけを数える。墨還りで消えたものはどちらにも数えない。
- **浮動小数点**: `-ffast-math` に頼らない。ベクトルの正規化は長さ `> 1e-6f` を確かめてから割り、0 の場合の向きを決めておく（NaN を作らない。UBSan は浮動小数点の 0 除算を検出しない）。
- **整数**: float から int への変換は、float のまま範囲へクランプしてから行う（範囲外の変換は未定義動作で、UBSan の `float-cast-overflow` が検出する）。ダメージの四捨五入は `[1, 1e6]` にクランプしてから `(int32_t)floorf(v + 0.5f)`。スコアは `int64_t` で計算して `[0, META_MAX_SCORE]` に飽和させる。
- **ハッシュの比較**: ゴールデン値（固定のハッシュ定数）をテストに書かない。比較は常に同一プロセス内の 2 インスタンス間で行う（プリセット間で浮動小数点の結果が一致する保証はない）。
- **数値の正**: game-design.md の値を使う。プロトタイプとの違い（社 HP 6 → 8、硯の回復 14/24 → 15/26、HP とダメージ ×10 など）に注意する。

### 2.3 プレイヤーの技・墨経済・豪筆（game-design §5）
- 風筆・渦印・風斬り・墨経済・一筆連殺・墨昇華・社は game-design §5.2〜§5.5 のとおり実装する。式の形（風筆の点数 `clamp(mdist / 5, 1, 18)`、かすれ筆の 1 点おきの間引き、墨量の配分など）はプロトタイプ `sim.c` の該当箇所を移植する。
- **発動の判定**: 墨がコスト以上で CD が 0 のときだけ発動する。満たさないときは発動せず、`EV_SKILL_DENIED`（`value = SkillId`）を出す。
- **`PlayerState` の更新**: `painting`（そのティックに風筆を注入したか）、`stroke_speed`（`mdist × 60`）、`cyclone_cd_total` / `gale_cd_total`（`RunModifiers` 適用後の CD）を毎ティック書く（P3-B の HUD と音が読む）。
- **豪筆**: 筆の半径 ×1.25、力 ×1.2、風筆の消費 ×1.15、風斬りの衝撃 ×1.25。`config.brush_style` が豪筆以外でも、P0 では豪筆として扱う（Pre §4 の 6）。
- **連殺の雷**: チェインが 3 の倍数に達した撃破地点に、半径 6.2 セルで墨 0.55・雷 1.15。P0 で雷を起こす主な源なので、雷墨連鎖のテストに使う。

### 2.4 属性反応 `elements_step`（game-design §7、P0 は墨×炎・墨×雷）
処理順は game-design §7.2 に厳密に従う。段 2・3（三元共鳴・爆雷）は Phase 4 で入れるので、段 1 → 段 4 → 段 5 の順に呼ぶ。

| 段 | P0 の内容 | 実装上の注意 |
| :---: | :--- | :--- |
| 1 | 紅蓮墨嵐の燃焼（炎 > 0.14 かつ墨 > 0.20、燃焼量 = `min(墨, 炎) × 0.030 × (1 + min(2.4, \|渦度\| × 0.28)) × 延焼の倍率`、炎 += 燃焼量 × 1.15、墨 -= 燃焼量 × 0.14）。雷の成長（雷 > 0.14 かつ墨 > 0.24 で、雷 += `min(墨, 雷) × 0.015`）。燃焼セル（炎 > 0.34）と雷セル（> 0.24）の数を数える | 渦度は、条件を満たしたセルについてだけ、現在の `u`・`v` から中心差分（`0.5f × ((v[R] - v[L]) - (u[D] - u[U]))`）で求める（`fluid.curl` は読まない）。染料は同じセルの値だけを読み書きするので、1 パスでその場更新してよい。燃焼セルが 24 以上になったら `EV_IGNITE`（3.2 秒間隔）と `run.ignitions++` |
| 4 | 敵への状態付与（墨印 > 0.28 で 3.0 秒、炎上 > 0.42、帯電 > 0.25 で 2.0 秒）。炎上ダメージ（0.36 秒ごとに 10、ボス 0.55 秒）。雷墨連鎖（発動条件・初撃・ホップ・同じ敵 1 回まで・CD 0.54 秒） | 敵の位置の値は `fluid_sample` のバイリニア補間。雷墨連鎖の「周囲 195px 以内で最も強い雷セル」は、敵ごとに半径 25 セルの範囲だけを走査する（全格子を敵の数だけ回さない）。同点は index の小さいセルを採る。各ホップで `EV_STORM_ZAP` を出し、`run.storm_zaps` は雷墨連鎖の発動（ホップ 0）および雷撃弾の追加雷撃発動ごとに 1 回数える（ホップ数で水増ししない） |
| 5 | 炎上弾（偏向済み、または墨 > 0.16 の上の弾が炎 > 0.36 を通過）、雷撃弾（偏向済みが雷 > 0.25 を通過）、偏向前の雷加速（1 回だけ ×1.3。使ったら `accelerated` を立てる） | 速度の上限は game-design §8.2 |

- `elements_step` の後に必ず `fluid_sanitize_channels(&g->fluid)` を呼ぶ。`fluid_channel_mut` の借用は次の `fluid_step` までに限る。

### 2.5 敵・敵弾・大天狗（game-design §8.1 / §8.2 / §8.4 / §8.5）

| 敵 | 実装の要点 |
| :--- | :--- |
| 弓妖 | 砲台型。外周帯（画面端から 76〜165px）を巡回し、1.15〜1.85 秒ごとに社へ矢。射る前の 0.35 秒で `EnemyComp.windup` を 0 → 1 に進める（P3-B が弓を引く姿を描く） |
| 提灯妖・炎 | 砲台型。1.55〜2.25 秒ごとに火球（射る前の 0.35 秒で `windup`）。出現時と撃破時に炎を撒く |
| 唐傘妖 | 軽量。社へ 96px/s で跳ねながら突進し、当たると跳ね返る。流速 × 3.8 /s で流される。風斬り・強い流れ（> 105px/s）で吹き飛ぶ |
| 大天狗 | HP 400、叩きつけ無効、画面の上半分を 8 の字に巡回。パターン A → B → C の繰り返し、HP 50% 以下で第 2 フェーズ（攻撃間隔 ×0.8、パターン D 追加）。フェーズ移行時は 1.5 秒の無敵（`HealthComp.invuln_t`）と `EV_BOSS_PHASE` |

- **大天狗の予兆**: すべての技の前に 0.6 秒以上（D は 0.8 秒）の予兆を置き、その間 `BossComp.telegraph_pattern` に次の技、`telegraph` に 0 → 1 の進行度を書く。予兆は game-design §8.5 の「次の攻撃まで」の間隔の**最後の部分に含める**（攻撃のテンポを変えない）。第 2 フェーズで間隔が ×0.8 になっても、予兆は 0.6 秒を下回らせない。
- 敵弾（矢・火球・雷球）: 流体の影響（流速 px/s × 5.0 × 影響係数 /s の加速）、偏向の判定、偏向後の追尾（ボスは距離 ×0.65 で優先）、速度の範囲、寿命 7.5 秒、画面外 45px で消滅。偏向した瞬間に `EV_DEFLECT`。軌跡は `ProjComp.trail` に Pre §2.6 の順序規約で積む（満杯なら古い方を捨てる）。
- 火球は軌跡に炎を残す（墨の上で着火する）。大天狗の敵風（紅風）は流体への注入で表し、敵弾・墨玉・唐傘妖も運ぶ。
- 墨還り（ウェーブクリア時）の対象になった敵と敵弾は、`TransformComp.dissolving = true` にして行動・当たり判定を止め、`dissolve_t` が 0.6 秒に達したら破棄予約する（撃破扱いにせず、スコアも墨玉もない）。

### 2.6 ダメージ・状態異常・叩きつけ（game-design §6）
- **計算順**: 基礎値 → 札の補正（加算してから乗算）→ 墨印（×2.0、ボス ×1.5）→ 耐性（ボスの雷 −50%）→ 四捨五入（最低 1、§2.2）。`on_hit` フックは補正の段階で `dmg_inout` を書き換える。
- **叩きつけ**: 吹き飛び中に 215px/s 超で画面端の壁へ当たると成立（唐傘妖は即撃破）。敵同士は 205px/s 超・判定半径 34px（ボス 54px）で、当たられた側に 40、飛ばされた側にも叩きつけと同じダメージ。成立時に `EV_SLAM` とヒットストップ 0.05 秒。即撃破でも `DMG_SLAM` 付きのダメージ適用経路（`on_hit` フック呼び出し）を通し、「叩きつけ衝撃」札が確実に発動するようにする。叩きつけで倒したら `EV_KILL` の `flags` に `DMG_SLAM`。
- **撃破**: `EV_KILL`、スコア（基礎点 × max(チェイン, 1)）、墨玉のドロップ（§8.1 の数）、チェインの更新、`on_kill`。破棄は `ecs_destroy` の予約で行い、回収はステップ 8 に任せる。
- **派生ダメージの再入防止**: 叩きつけ衝撃の衝撃波のように、フックが起こすダメージには `DMG_SECONDARY` を付け、`DMG_SECONDARY` 付きのダメージでは `on_hit` / `on_kill` の札効果を再び起こさない（無限の連鎖を防ぐ）。

### 2.7 ウェーブとスポーン（game-design §3.2〜§3.4）
- 第 1 幕の 4 ウェーブ（ノルマ・同時上限・増援間隔・初期編成）と増援の重み（弓妖 45、提灯妖・炎 30、唐傘妖 25（第 2 ウェーブから））を `ActDef` の表に持つ。
- **ウェーブ開始**: 墨を満タンにし、初期編成を出し、その種類がランで初めて出るなら `EV_ENEMY_INTRO`（種類ごとに 1 回、表の順）。ボスウェーブなら大天狗と護衛を出して `run.boss_id` を設定し、`EV_BOSS_ENTER`。
- **出現位置**: 4 辺から抽選し、既存の敵から 88px 以上離す（最大 10 回試行、全滅なら最後の候補）。社から 200px 以内には出さない。抽選は `g->rng` を使い、試行回数も決定的にする。
- 幕が始まるとき（ACT_TRANSITION の開始ティック）に `fluid_reset()` と地形の配置（第 1 幕はなし）を行い、`rerolls_left` をその幕の値に戻す。

### 2.8 報酬の提示（game-design §9.1 / §9.2）
WAVE_AFTERMATH から REWARD へ入るとき（§2.10）、次の手順で提示を作る（リロールも同じ手順）。

1. 候補 = `level < game_upgrade_max_level(id)` の札。候補が 0 件なら `kind = REWARD_KIND_FALLBACK`、`choice_count = 0` で終わる。
2. 3 枠を順に埋める。各枠で候補を次の順に絞り込む。**絞り込んだ結果が空になる制限は適用しない**（その前の候補のまま次へ進む）。
   1. すでに選んだ札を除く。
   2. ランの最初の提示の 1 枠目は、推奨軸（`game_is_recommended_axis`）の札に限る。
   3. 3 枠目で、それまでの 2 枚が同じ軸なら、別の軸の札に限る。
   4. `ink_rng_f01` を 1 回引き、`< 0.4` なら所持中（Lv1〜2）の札に限る。
   5. 重みを推奨軸なら 3、それ以外は 2 として、`ink_rng_next % 合計` で 1 枚を引く。
3. 候補が尽きたら、その時点の枚数で終わる（1〜2 枚の提示もありうる）。
4. `choice_next_level` に取得後の Lv を書き、`repair_available` を §2.1 の規則で決め、`serial++`。

- 乱数は各枠で 2.4 の 1 回と 2.5 の 1 回を必ずこの順に引く（制限で候補が 1 枚でも引く）。同じシードと同じ入力列で同じ提示になる。
- 選択後に始まる次ウェーブは `wave_index + 1`。第 3 ウェーブの報酬の後はボスウェーブになる。

### 2.9 強化札 8 種（game-design §9.4、architecture §6.1）

| 札 | 実装 | Lv1 / Lv2 / Lv3 |
| :--- | :--- | :--- |
| 火種の筆 | `on_stroke`: 筆跡の累積距離が間隔に達するごとに、炎 0.5 × 量の倍率・半径 2.5 セルの火種。かすれ筆の間は累積しない | 160 / 120 / 80px、量 ×1 / ×1 / ×1.5 |
| 延焼 | `RunModifiers.burn_rate_mul` | ×1.25 / ×1.5 / ×1.8 |
| 帯電印 | `on_cyclone`: 発動時の `fluid_swirl` の `dye_add` の雷（ch2）に加える。Lv3 は、渦を持続させる間（最初の 55%）の毎ティックの注入にも加える | 0.3 / 0.45 / 0.6 |
| 連鎖雷 | `chain_hops_add`、Lv3 は `chain_range_mul` | +1 / +2 / +3、Lv3 で距離 ×1.3 |
| 大渦 | `cyclone_radius_mul`、`cyclone_duration_add` | +15% / +30% / +45%、+0.2 / +0.4 / +0.6 秒 |
| 叩きつけ衝撃 | `on_hit`（`DMG_SLAM`）: 衝撃波（`DMG_SECONDARY`）。Lv3 は周囲を吹き飛ばす | 64px・20 / 80px・30 / 96px・40 |
| 墨玉増量 | `on_kill`: 確率で墨玉 +1（`ink_rng_f01` を所持時だけ 1 回引く） | 33% / 66% / 100% |
| 長き昇華 | `surge_duration` | 4.6 / 5.6 / 6.6 秒 |

- **`RunModifiers` の再計算**（`roguelike_recompute_modifiers`）: 既定値 → 筆スタイル → メタ強化（`config.meta_upgrade_lv` の硯の回復 +10%/段など）→ 所持札（取得順）の順に、加算をすべて適用してから乗算を適用する。札の取得・ラン開始のときだけ呼ぶ（毎ティック呼ばない）。
- **フック**: 7 フックは `const` の引数構造体 1 つで受け取る（Pre §4 の 9。`GameState *g` と `lv` などを構造体に持つ）。所持札の取得順に呼び、未登録は `NULL` で飛ばす。P0 では同じフック種別を持つ札が 1 枚ずつなので、順序の検証は Phase 4 で追加する（実装は最初から取得順のループにする）。

### 2.10 遷移・ランの終了・墨還り
- **優先順位**（ステップ 8、architecture §3.2）: 社 HP 0（SHRINE_BREAK、`EV_GAME_OVER`）＞ ボス撃破（`EV_ACT_CLEAR`、`run.acts_cleared++`）＞ ノルマ到達（`EV_WAVE_CLEAR`）。ボスの撃破スコア（基礎点 1500 × `max(チェイン, 1)`）と墨玉 8 個ドロップは §2.6 の通常の敵撃破処理（`EnemyDef` の `score = 1500`, `orb_count = 8`）で 1 回だけ処理し、ステップ 8 で二重計上しない。後の 2 つは、残った敵と敵弾を墨還りさせ、場の墨玉（ボスの墨玉 8 を含む）を自動回収して（スコア +25 ずつ、`EV_ORB_PICKUP` に `EVF_ORB_AUTO`）WAVE_AFTERMATH へ移る。
- **WAVE_AFTERMATH**（1.0 秒）: 段 1 の分岐で、`fluid_step`・墨還り・`phase_t` だけを進め、新規スポーン・敵の攻撃・社の被弾・プレイヤーの技は止める。1.0 秒に達したティックの最後に、通常ウェーブなら報酬の提示を作って REWARD へ、最終幕のボスなら VICTORY（`EV_VICTORY`）へ移る。P0 では第 1 幕が最終幕なので、大遺物と社の回復はない（Pre §2.7）。
- **ACT_TRANSITION**（2.0 秒）: 開始ティックに §2.7 の幕の準備、2.0 秒に達したティックに第 1 ウェーブを開始して PLAYING へ。
- **SHRINE_BREAK**（1.5 秒）→ GAME_OVER。
- 社の被弾: 無敵 0.95 秒、チェインを 0 に戻し、社の周囲に小さな渦を注入する。`EV_SHRINE_HIT`。

### 2.11 状態ハッシュ・デバッグ API・自動入力
- **`game_hash_sections`**: Pre §2.3 のセクション定義に従い、フィールドを宣言順に `ink_hash_u32` / `u64` / `f32` で加える。`bool` は `uint8_t` として、列挙型は `(uint32_t)` で加える。構造体を丸ごと `ink_hash_bytes` に渡さない（パディングを含むため）。`GAME_HASH_COMPONENTS` は `0..high_water-1` の生存エンティティについて、持っているマスクの Component だけを加える。`fluid.stats` は加えない。
- **デバッグ API**: Pre §2.7 の「本実装の意味」のとおりに置き換え、`/* P3-Pre stub */` の行を消す。
- **自動入力 `game_autoplay_input` / `_command`**: プロトタイプの `sim_demo_input` を参考に、状態だけから決まる入力を返す（社の周りを回る風筆で矢を逸らす、唐傘妖が壁の方向にいるとき風斬り、火球が墨の近くに来たら渦印、墨が少なければ筆を離す）。コマンドは、REWARD で「社 HP が 4 以下で修繕できるなら修繕、その幕で未使用ならリロール、それ以外は `ticks % choice_count` の札」。勝てる強さは求めないが、§3.2 の健全性の条件を満たすこと。

### 2.12 性能（Release、architecture §4.3）
- 予算は `game_step` 全体で平均・p95 `< 4.0 ms/tick`（うち `fluid_step` 約 1.3 ms）。
- 全格子のパス（elements の段 1）は 1 回にまとめる。敵ごとの格子走査は §2.4 の局所範囲に限る。敵と弾の総当たりは P0 の規模（敵 ≤ 10、弾 ≤ 64 程度）なら許容するが、ループの内側で `fluid_sample` を重ねて呼ばない。

---

## 3. テスト・ベンチ要件

全テストは `game_create` でヒープに確保し、最後に `game_destroy` する。内部関数（`elements_step` など）を直接呼ぶテストは、`#include "elements/elements.h"` のように内部ヘッダを include してよい。この Unity 構成では `double` の比較マクロは使えない。

### 3.1 `tests/test_elements.c`
`game_create` → `game_debug_start_wave(g, 0)` で PLAYING にし、流体は `fluid_channel_mut` で直接書いて条件を作る。

| # | ケース | 判定 |
| :---: | :--- | :--- |
| 1 | 紅蓮墨嵐のセル反応 | 速度 0 のセル（炎 0.2・墨 0.3）で、1 回の `elements_step` 後に炎・墨が §2.4 の式どおり（許容誤差 `1e-6f`）。炎 0.13 のセルは変わらない。延焼 Lv1 で燃焼量が ×1.25。周囲に渦（`u`・`v` を直接書く）があると燃焼量が増え、倍率は `1 + 2.4` を超えない |
| 2 | 着火の表示条件 | 燃焼セル 23 では `EV_IGNITE` なし、24 で 1 回。その後 3.2 秒（192 ティック）未満は出ず、`run.ignitions` は 1 |
| 3 | 雷の成長 | 雷 0.15・墨 0.25 のセルで雷 += `min(墨, 雷) × 0.015`。墨 0.23 では変わらない |
| 4 | 雷墨連鎖 | 雷セル 9 では発動しない。10 で発動し、墨印の敵へ `EV_STORM_ZAP`（ホップ 0）と 20 ダメージ（10 × 墨印 2.0）。160px 以内の帯電した敵へホップ 1（×0.7）。同じ敵は 1 回まで。0.54 秒の CD。連鎖雷 Lv1 でホップが 1 増える |
| 5 | 状態付与 | 墨 0.29 で `ink_mark_t = 3.0`、0.27 では付かない。炎 0.43 の上で 0.36 秒ごとに 10 ダメージ、ボスは 0.55 秒ごと。雷 0.26 で `charge_t = 2.0` |
| 6 | 弾の属性変化 | 偏向済みの弾が炎 0.37 を通ると `burning`（貫通 2）、雷 0.26 で `electrified`。偏向前の弾が雷 0.26 を通ると速度 ×1.3 が 1 回だけ（2 回目は変わらない） |
| 7 | 浄化と有限性 | `elements_step` の前に `NaN`、`-1`、`99` を書いたセルが、段 5 後の `fluid_sanitize_channels` で 0、0、`max_dye` になり、`fluid_validate_finite` が真 |
| 8 | 処理順 | 同じティックで燃焼と雷撃が起きる配置で、`events[]` の中で `EV_IGNITE` が `EV_STORM_ZAP` より前に並ぶ |
| 9 | 保留インパルス | `pending_impulses[0]` を直接積んだティックの `fluid_step` 前には速度が変わらず（注入は次のティックの段 3）、次のティックで中心の右側が `u > 0`・左側が `u < 0`、`pending_impulse_count == 0`、速度は `max_vel` 以内 |

### 3.2 `tests/test_determinism_run.c`（qa-and-testing §2）
**シナリオ**（2 インスタンス、`meta_run_config_preset(BRUSH_STYLE_HEAVY, 0x1234)`、入力と命令は `game_autoplay_*` と下表の命令）

| 区間 | 内容 |
| :--- | :--- |
| 幕転換 | ACT_TRANSITION が PLAYING になるまで（120 ティックで切り替わることも確認） |
| 第 1 ウェーブ | 480 ティック → `game_debug_force_wave_clear` → REWARD まで進める → `CMD_REROLL_REWARD` → `CMD_PICK_REWARD 0` |
| 第 2 ウェーブ | 480 ティック → 強制クリア → `CMD_REPAIR_SHRINE`（無効なら no-op を確認してから `CMD_PICK_REWARD 1`） |
| 第 3 ウェーブ | 480 ティック → 強制クリア → `CMD_PICK_REWARD 2`（`choice_count` 未満に丸める） |
| ボス | 720 ティック → 強制クリア（ボスへ致死ダメージ）→ VICTORY まで |

| # | ケース | 判定 |
| :---: | :--- | :--- |
| 1 | ビット一致 | 毎ティック `game_hash_sections` の全セクションが一致する。不一致なら**最初のティックとセクション名・両方の値**を出力して失敗する |
| 2 | 健全性 | 全区間で `fluid_validate_finite`。ラン全体で `EV_KILL`・`EV_DEFLECT`・`EV_CYCLONE`・`EV_GALE` が各 1 回以上、`EV_IGNITE` か `EV_STORM_ZAP` が 1 回以上、`EV_BOSS_ENTER` が 1 回、`EV_REWARD_PICK` が 3 回、`EV_VICTORY` が 1 回。墨が `ink_max` 未満になったティックがある。ボスの HP が強制クリア前に減っている。各 `game_step` で `ticks` の増分が Pre §2.7 の規則（0 か 1、0 は停止フェーズのときだけ）に従う |
| 3 | 精算 | VICTORY 後の `meta_settle_run` が `true` で、墨晶が `meta_calc_crystals(game_run_result(g))` と一致（`from_acts == 30`）。2 回目は `false` |
| 4 | シードの違い | seed だけ変えたインスタンスは、第 1 ウェーブ中に `GAME_HASH_COMPONENTS` か `GAME_HASH_FLUID` が分かれる |
| 5 | 1 フレーム複数ティック | 一方は 1 ティックずつ、他方は「0, 2, 1, 3, 0, 1 ティック」を繰り返すフレームで進める。どちらも `game_step` の直後に毎回テスト内の集計関数で `events[0..event_count)` を数える。同じティック数でハッシュと種類別の累計が一致する |
| 6 | イベントの上限 | 1 ティックに 130 件を積むと 128 件が残り、`dropped_events` が 2 増え、次の `game_step` で `event_count` が 0 から始まる（`dropped_events` は累計のまま） |
| 7 | 中断と優先順位 | PLAYING で `CMD_ABORT_RUN` → GAME_OVER・`EVF_RUN_ABORTED`、GAME_OVER での再送は no-op。社 HP を 0、`wave_kills = wave_quota` にした同じティックでは SHRINE_BREAK が勝ち、1.5 秒（90 ティック）後に GAME_OVER |
| 8 | 不正な入力 | 座標に `NAN` / `INFINITY` を含む `BrushInput` は入力なしと同じハッシュになる。`±1e30f` の座標、`(GameCommandType)99` のコマンドでもクラッシュせず有限のまま |
| 9 | 容量の満杯 | `game_debug_spawn_orb` で無効 ID が返るまで詰めた状態で、敵を倒す・弾を撃つ処理を含む 120 ティックを進めても、クラッシュせず有限で、`ecs_alive_count <= ECS_MAX_ENTITIES` |

- ASan の実行時間は全体で 90 秒以内を目安にする。超える場合は、`GAME_HASH_FLUID` だけを 8 ティックごとの比較にし、不一致が出たら直前の 8 ティックを両インスタンスで再実行して最初のティックを特定してよい（判定の条件は緩めない）。健全性の条件を満たせない場合は、シードと自動入力を調整する。

### 3.3 `tests/test_roguelike_meta.c`（ラン部の追記）

| # | ケース | 判定 |
| :---: | :--- | :--- |
| 1 | 提示の規則 | 200 シードで、3 枚が互いに異なり、2 軸以上を含み、Lv3 の札が出ない。ランの最初の提示は推奨軸（豪筆なら烈風か墨道）を 1 枚以上含む |
| 2 | 再現性 | 同じシード・同じ命令列で、各提示の `choice_ids` と `serial` が一致する |
| 3 | リロール | `rerolls_left` が減り、`serial` が増え、`EV_REWARD_REROLL`。0 回のときは no-op（基準の no-op ティックの後で `game_hash` が変わらない） |
| 4 | 修繕 | HP +1 と `EV_REWARD_PICK`（`value == -1`）。満タンなら no-op |
| 5 | 無効コマンド | `arg` が -1・`choice_count`・3、PLAYING での `CMD_PICK_REWARD`、`CMD_START_ENDLESS`、`CMD_PICK_ENDLESS_CRYSTAL` がすべて no-op（RNG も消費しない） |
| 6 | 候補不足 | 7 種を Lv3 にすると提示が 1 枚。8 種とも Lv3 なら `REWARD_KIND_FALLBACK`・0 枚で、満タンでも `CMD_REPAIR_SHRINE` で次ウェーブへ進める |
| 7 | 絞り込みの空集合 | 所持中の札が 1 枚もない状態でも 2.4 で候補が空にならず 3 枚が出る。推奨軸の札をすべて Lv3 にした状態の最初の提示でも 3 枚が出る |
| 8 | `RunModifiers` | 豪筆・メタ強化なしで大渦 Lv2 → 渦印の半径 ×1.30・持続 +0.4 秒、長き昇華 Lv3 → 6.6 秒、`META_UPG_INK_MAX` 2 段で `ink_max == 120` |

### 3.4 `tests/bench_game.c`（Release 性能ベンチ、qa-and-testing §3）
- **条件**: `meta_run_config_preset(BRUSH_STYLE_HEAVY, 0x5eed)`。幕転換の後に `game_debug_start_wave(g, GAME_BOSS_WAVE_INDEX)` で大天狗戦にし、入力は `game_autoplay_input`。ウォームアップ 120 ティックの後、600 ティックを計測する。各ティックの `game_step` を `clock_gettime(CLOCK_MONOTONIC)` で挟んで測り、`fluid.stats.solve_ms` も集める。`game_create` からやり直して **5 試行**。
- **有効性**: 計測中は毎ティック PLAYING であること（ボスを倒して抜けたら、その試行を `INVALID` と出力する）。生存エンティティ数・敵弾数・反応イベント数の平均を出力する（空の盤面で薄めていないことの記録）。
- **追加の負荷行**（判定には使わない）: `game_debug_spawn_enemy` で通常敵を計 16 体、`game_debug_spawn_projectile` で敵弾を計 48 発足した状態の値も出す。
- **出力と終了コード**: bench_fluid と同じ形（条件行、試行ごとの `mean / p50 / p95 / max`、`game_step` と `fluid_step` の両方、試行中央値、`budget(<4.0ms mean & p95): PASS|FAIL`）。終了コードが非 0 になるのは、試行間でハッシュが一致しないときと、有限性が崩れたときだけ。
- **合否**: 試行中央値の mean と p95 がともに `< 4.0 ms`。p95 だけがホストの揺らぎで超える場合は、しきい値を緩めずに実測値を添えてユーザーの判断を仰ぐ。

---

## 4. ローカル検証・PR と完了条件

AGENTS.md の分担に従い、ローカルでは変更範囲のビルド・関連テストと、CI で代替できない性能ベンチだけを行う。フルの ASan/UBSan + CTest、`format-check`、`lint`、`dist` は PR の CI に任せる（CI が失敗したときだけ、該当のチェックをローカルで再現する）。

```sh
# 1. ASan/UBSan: 変更範囲のビルドと、game_step を通る関連テスト
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan --target ink_game inkstorm test_elements \
        test_determinism_run test_roguelike_meta test_core_smoke bench_game &&
    nix develop -c ctest --preset asan --output-on-failure \
        -R '^(test_elements|test_determinism_run|test_roguelike_meta|test_core_smoke|test_cli_headless_smoke)$'

# 2. Release ベンチ（CI では代替できない性能ゲート）
nix develop -c cmake --preset release &&
    nix develop -c cmake --build --preset release --target bench_game bench_fluid &&
    ./build/release/bench_game && ./build/release/bench_fluid

# 3. 担当範囲と境界（いずれも 0 件・空）
grep -rn 'raylib.h\|rand(\|time(' src/game
grep -rn 'P3-Pre stub' src/game
grep -rnE 'fluid(\.|->)(u0|v0|div|p|curl|dye0)\b' src/game
git diff --stat main -- CMakeLists.txt src/core src/fluid src/ecs src/game/meta.h src/game/meta.c \
    src/game/game.h src/game/game_ids.h src/game/game_events.h src/game/game_components.h \
    src/game/game_contract.c tests/test_core_smoke.c src/render src/audio src/app assets README.txt
```

**PR**: 最新の `main` を取り込み（P3-B が先に入っていれば、手順 1 と `nix develop -c ./build/release/inkstorm --smoke-frames 300` を再実行）、ユーザーの許可を得てから inkstorm-pr-and-coderabbit の手順で PR を作る。PR 本文には、変更概要、§3 のテスト結果、`bench_game` / `bench_fluid` の記録（CPU / OS / コンパイラ、5 試行の各値、中央値、負荷行）、game-design.md へ反映した数値、契約変更要求（なければ「なし」）を書く。

**静的解析・ビルドで引っかかりやすい点**（CI の `static-analysis` で指摘されやすいもの）
- cppcheck の `constParameterCallback`（フックを個別のポインタ引数で書いたとき。§2.9）、`variableScope`、`constParameterPointer`、`knownConditionTrueFalse`。clang-tidy の `DeadStores`、`clang-analyzer-core.NullDereference`（アクセサの戻り値は必ず `NULL` を確かめる）。
- `-Wextra`: 使わないフック引数による `-Wunused-parameter`（使う引数だけを構造体から読めば起きない）、`-Wsign-compare`（`uint32_t` の index と `int32_t` の比較）、`-Wtype-limits`。
- `-Wshadow`: 局所変数と引数・ファイルスコープの名前の重複。
- `-Wpedantic`: ビットフラグの `enum`、空の構造体、`case 1 ... 3` のような範囲指定、配列の範囲指定子 `[0 ... 3]`（いずれも GNU 拡張か C11 違反）。
- UBSan: `1u << n`（`n >= 32`）、`int32_t` の加算あふれ（スコア・墨晶）、float から int への範囲外の変換。
- `lint` の cppcheck は `--check-level=exhaustive` 付き（P2-Int）。大きな関数が増えても解析上限で止まらないが、CI 失敗をローカルで再現するときは手で同じ引数を付ける。
- ローカル（21 / 2.18）と CI（18 / 2.13）で結果が違ったら CI を正とする。

**完了条件（Definition of Done）**
- [ ] §2 の仕様を満たし、凍結ファイルと P3-B の担当に差分がない（手順 3）。`P3-Pre stub` が残っていない。
- [ ] `test_elements`（9 ケース）、`test_determinism_run`（9 ケース）、`test_roguelike_meta`（メタ部＋ラン部 8 ケース）、`test_core_smoke`、`test_cli_headless_smoke` が ASan/UBSan（`-Werror`）で通る。
- [ ] `bench_game` が Release で 5 試行のハッシュ一致・有限性・全試行 `PLAYING` を満たし、試行中央値の mean と p95 がともに `< 4.0 ms/tick`。`bench_fluid` も `< 2.0 ms/tick` のまま。
- [ ] PR の CI 3 ジョブ（`build-asan-and-test`、`static-analysis`、`dist-verify`）がすべて緑。
