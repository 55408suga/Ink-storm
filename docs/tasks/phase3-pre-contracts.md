# Phase 3-Pre タスク指示書：P0 縦切りの共通契約（ヘッダ枠・テスト登録・設計書同期）

> **必読**: [PLAN.md](../PLAN.md) §9（特に §9.1 の Phase 3 行と §9.2）、[architecture.md](../architecture.md) §1〜§3 / §5〜§9、[game-design.md](../game-design.md) §3 / §5 / §8.1 / §8.5 / §9.2 / §9.4 / §10.1 / §11 / §12、[art-direction.md](../art-direction.md) §11 / §14、[qa-and-testing.md](../qa-and-testing.md) §2〜§5、[phase2-integration.md](phase2-integration.md) §3.2、[AGENTS.md](../../AGENTS.md)
>
> **前提**: P2-Int（PR #5、マージコミット `5d4bd6b`）が `main` に入り、Phase 2 Gate を通過していること。本書の `CMakeLists.txt` の挿入位置、置き換える既存 API（§2.3）、§4 の 1〜5 は、この時点の `main` と PR #5 の本文で照合済み。着手時に `main` がさらに進んでいたら、同じ照合をやり直す。
> **作業場所 / ブランチ**: リポジトリルート（`full-scale-dev/Ink-storm`、作業ツリーがクリーンな状態）で `main` から `feat/phase3-pre-contracts` を作り、**1 本の PR** にする。
> **後続**: P3-A（[phase3-sim-p0.md](phase3-sim-p0.md)）、P3-B（[phase3-render-audio-p0.md](phase3-render-audio-p0.md)）、P3-Int（[phase3-integration.md](phase3-integration.md)）

---

## 0. 目的と禁止事項

Phase 3 では、ゲームシミュレーション（P3-A）と描画・音・入出力（P3-B）を並列で実装する。本タスクは両者の境界を `main` に固定し、どちらのブランチも**単独でビルド・CI 全緑・PR 可能**な状態を作る。

1. **境界ヘッダの凍結**: `GameState`、`GameEvent`、`GameCommand`、`BrushInput`、内容 ID、Component、`RunConfig` の受け渡し、`meta_settle_run` を確定する。P3-B が読むフィールドを「公開読み取り契約」として明示し、P3-A 専有の状態は `GameInternal` に隔離する。
2. **テストターゲットの先行登録**: Phase 3 で増えるテスト・ベンチをすべて `CMakeLists.txt` に登録し、骨格テストを置く。並列期間中は、**P3-A も P3-B も `CMakeLists.txt` を編集しない**。
3. **骨格実装**: 骨格の `game_step`、検証付きアクセサ、デバッグ用スポーン、自動入力、CLI の解析、フレーム時計を置く。`main` が常にビルドでき、CI が緑になる状態にする。
4. **設計書の同期**: Phase 2 で未反映の 5 項目（[phase2-integration.md](phase2-integration.md) §3.2）と、本書で確定する Phase 3 の契約を設計書へ書き込む（§4）。

### 0.1 Phase 3 の進め方

| 段階 | 担当 | ブランチ | PR とマージ |
| :--- | :--- | :--- | :--- |
| 1 | P3-Pre（本書） | `feat/phase3-pre-contracts` | 1 本。マージ後に下の 2 つの worktree を切る |
| 2a | P3-A | `full-scale-dev/wt-sim-p0` / `feat/phase3-sim-p0` | 完了したら単独で PR → CI 全緑 → マージ |
| 2b | P3-B | `full-scale-dev/wt-render-audio-p0` / `feat/phase3-render-audio-p0` | 完了したら単独で PR → CI 全緑 → マージ（2a と順不同） |
| 3 | P3-Int | 2a・2b のマージ後の `main` から `feat/phase3-integration` | 結合の修正・実機完走・Phase 3 Gate を 1 本の PR に |

- 2a・2b を個別の PR にするのは、レビューの単位を小さく保ち、各ブランチのフル検証（ASan/UBSan + CTest・整形・静的解析・配布 zip）を CI に任せるため（AGENTS.md のローカル検証と CI の分担）。本書の骨格により、どちらが先にマージされても `main` はビルドでき、CI は緑のままになる。
- 各ブランチは PR の前に最新の `main` を取り込み（`git merge main`）、もう一方が先に入っていれば、その状態で関連テストとスモークを実行し直す。

**禁止事項**
- ゲームの挙動（敵 AI、属性反応、報酬の抽選、ダメージ計算）を実装しない（P3-A の担当）。描画の様式、音の合成、セーブの I/O も実装しない（P3-B の担当）。骨格は §2.7〜§2.10 の範囲に留める。
- Phase 2 で確定した公開 API（`fluid.h`、`ecs.h`、`meta.h`）を変えない。変更が必要になったら作業を止めてユーザーに確認する。
- `CMakeLists.txt` の既存行を変えない（追記のみ）。モジュール間のリンク行も変えない。
- `core` / `fluid` / `ecs` / `game` / `audio` に `raylib.h` を持ち込まない。

---

## 1. ファイルと所有

### 1.1 本タスクで変更するファイル

| ファイル | 操作 | 内容 | Pre 後の扱い |
| :--- | :---: | :--- | :--- |
| `src/core/core.h` / `core.c` | 更新 | FNV-1a ハッシュ補助（§2.2） | 凍結 |
| `src/game/game.h` | 更新 | 定数・`GameState`・公開 API（§2.3） | 凍結 |
| `src/game/game_ids.h` / `game_events.h` / `game_components.h` | 新規 | 内容 ID、入力とイベント、Component（§2.4〜§2.6） | 凍結 |
| `src/game/game_contract.c` | 新規 | アクセサ、イベントの追加、内容メタ、精算（§2.6 / §2.8） | 凍結 |
| `src/game/game_internal.h` | 新規 | `GameInternal` の枠（§2.3） | P3-A へ移譲 |
| `src/game/game.c`、`hash.c`、`debug_api.c`、`autoplay.c` | 更新・新規 | 生成・初期化・骨格 `game_step`・ハッシュ・デバッグ API・自動入力（§2.7） | P3-A へ移譲 |
| `src/app/cli.h` / `cli.c`、`frame_clock.h` / `frame_clock.c` | 新規 | CLI の解析とフレーム時計の**完全な実装**（§2.9） | P3-B へ移譲 |
| `src/app/input_latch.h` / `input_latch.c` | 新規 | 入力ラッチの API と素通しの骨格（§2.9） | P3-B へ移譲 |
| `src/app/save.h` / `save.c`、`src/app/main.c` | 新規・更新 | セーブ API の枠、新 API への同期（§2.10） | P3-B へ移譲 |
| `src/audio/audio.h` / `audio.c` | 更新 | API の枠（§2.10） | P3-B へ移譲 |
| `src/render/render.h` / `render.c` / `ui_view.h` / `text_table.h` / `text_table.c` | 更新・新規 | `UiView` と `TextId` の枠、`render_draw` の引数（§2.10） | P3-B へ移譲 |
| `tools/expect_usage.sh` | 新規 | CLI の Usage エラー判定（§3.1） | 凍結 |
| `tests/test_core_smoke.c` | 更新 | 新 API・ハッシュ補助・中断の契約（§3.2） | 凍結 |
| `tests/test_app_core.c` | 新規 | CLI とフレーム時計の全ケース（§3.3） | P3-B へ移譲（既存ケースは残す） |
| `tests/test_elements.c` / `test_determinism_run.c` / `bench_game.c` | 新規 | 骨格（§3.4） | P3-A へ移譲 |
| `tests/test_audio.c` / `test_app_save.c` / `test_text_table.c` | 新規 | 骨格（§3.4） | P3-B へ移譲 |
| `CMakeLists.txt` | 追記のみ | §3.1 | 凍結 |
| `docs/architecture.md` / `game-design.md` / `qa-and-testing.md` / `art-direction.md` / `PLAN.md` | 更新 | §4 | §1.2 |

### 1.2 並列期間（P3-A / P3-B）の所有表

| 区分 | ファイル |
| :--- | :--- |
| **凍結**（読むだけ。変更は §1.3 の手続き） | `CMakeLists.txt`、`CMakePresets.json`、`src/core/*`、`src/fluid/*`、`src/ecs/*`、`src/game/meta.*`、`src/game/game.h`、`game_ids.h`、`game_events.h`、`game_components.h`、`game_contract.c`、`tests/test_core_smoke.c`、`test_fluid.c`、`bench_fluid.c`、`test_ecs.c`、`docs/PLAN.md`、`architecture.md`、`qa-and-testing.md`、`tools/*`、`.github/*`、`flake.*`、`AGENTS.md`、`.agents/*` |
| **P3-A** | `src/game/` の上記以外（`game.c`、`game_internal.h`、`hash.c`、`debug_api.c`、`autoplay.c`、新規サブディレクトリ）、`tests/test_elements.c`、`test_determinism_run.c`、`bench_game.c`、`test_roguelike_meta.c`（ラン部の追記のみ。メタ部の既存テストは変えない）、`docs/game-design.md`（数値調整の反映のみ） |
| **P3-B** | `src/render/*`、`src/audio/*`、`src/app/*`、`assets/*`、`README.txt`、`tests/test_app_core.c`、`test_audio.c`、`test_app_save.c`、`test_text_table.c`、`docs/art-direction.md`（数値調整の反映のみ） |

- 移譲したファイルの中身と API は、受け取った側が変えてよい。ただし、凍結ヘッダに宣言された関数（`game_debug_*`、`game_autoplay_*` など）のシグネチャと意味は P3-A が保つ。CLI の規則（§2.9 の表）と `test_app_core` の既存ケースは P3-B が保つ。
- P3-B は `GameState` の公開読み取り契約（§2.3）だけを読む。`g->internal` を読まない。

### 1.3 契約変更の手続き
- 並列期間中に凍結ファイルの変更が必要になったら、自分のブランチでは変更せず、作業メモと完了報告の「契約変更要求」欄に「変更内容・理由・代替案・影響範囲」を書いてユーザーに知らせる。
- 承認された変更は、統合担当が `main` から `fix/phase3-contract-<topic>` を切り、凍結ファイル・呼び出し側・設計書を同じコミットで直した小さな PR にする。マージ後、P3-A / P3-B はそれぞれ `git merge main` で取り込む（並列作業を止めないため、P3-Int まで溜めない）。
- P3-A は、新しく必要になった状態を `GameInternal` に置けば凍結ファイルを変えずに進められる。P3-B は、契約にない情報が必要になったら、契約の範囲から導出するか、表示を省いて変更要求に回す。

---

## 2. 実装仕様

### 2.1 ヘッダの構成と共通規則
```text
game.h ──▶ fluid.h, ecs.h, meta.h, game_ids.h, game_events.h, game_components.h, game_internal.h
game_internal.h ──▶ <stdbool.h>, <stdint.h>, fluid.h, ecs.h, game_ids.h, game_events.h のみ
audio.h ──▶ struct GameState の前方宣言のみ（game.h を include しない）
render.h ──▶ raylib.h, game.h, ui_view.h
ui_view.h ──▶ meta.h（MetaSettleBreakdown）
cli.h / frame_clock.h / input_latch.h / save.h ──▶ 標準ヘッダと game.h / meta.h のみ（raylib 非依存）
```
- `GameState` は `typedef struct GameState GameState;` とし、`struct GameState { … };` で定義する。`audio.h` は `struct GameState;` を前方宣言し、引数を `const struct GameState *` にする（`ink_audio` の利用側へ `game.h` の include を強制しないため）。
- **`game_internal.h` は自己完結にする**。`GameInternal` が使う型はこのヘッダで定義し、`src/game/` のサブディレクトリのヘッダを include しない（`game.h` を読むすべての翻訳単位に P3-A の内部ヘッダが流れ込み、P3-A の内部変更で P3-B 側まで再ビルド・結合されるのを防ぐ）。
- **ヘッダのファイル名は `src/` 全体で一意にする**。配布用 `Makefile`（`tools/dist.mk`）は全モジュールの `-Isrc/<module>` を並べるので、別モジュールに同じ名前のヘッダがあると、`#include "x.h"` が別のファイルを拾う。raylib のヘッダ名（`raylib.h`、`rlgl.h`、`raymath.h` など）とも重ねない。確認: `find src -name '*.h' -printf '%f\n' | sort | uniq -d` が空。
- ビットフラグは `enum` にしない。`-Wpedantic` は `int` を超える列挙値（`1u << 31` など）をエラーにする（gcc 13.3 の `-Werror` で失敗することを確認済み）。`#define NAME (UINT32_C(1) << n)` で定義する。
- 列挙値の範囲検査は `(unsigned)id >= COUNT` と書く（GCC では列挙型の基底型が符号なしになり、`id < 0` は `-Wtype-limits` で失敗する）。
- 空の構造体は C11 では書けない（GCC の `-Wpedantic` でエラー）。`GameInternal` の枠には 1 つメンバーを置く。
- 本書のコードブロック内の `/* … */` は契約の説明であり、そのまま実装へ写さない。単位や範囲はできるだけ名前（`_t` は秒など）と本書の表で伝え、コードのコメントは「なぜその形にしたか」が必要なものに限る。
- 列挙型は Phase 4 以降も末尾の `*_COUNT` の直前に追記し、既存の値を変えない。

### 2.2 `core` のハッシュ補助
```c
#define INK_HASH_INIT UINT64_C(0xcbf29ce484222325)

uint64_t ink_hash_bytes(uint64_t h, const void *data, size_t size); /* uint8_t 列専用（メモリ順） */
uint64_t ink_hash_u32(uint64_t h, uint32_t v);                       /* 下位バイトから 4 バイト */
uint64_t ink_hash_u64(uint64_t h, uint64_t v);                       /* 下位バイトから 8 バイト */
uint64_t ink_hash_f32(uint64_t h, float v); /* -0.0f → +0.0f、NaN → 0x7fc00000 に正規化してから u32 */
```
- FNV-1a 64bit（prime `0x100000001b3`）。`core.h` に `<stddef.h>` を加える。複数バイトの整数は `ink_hash_bytes` に渡さず、必ず u32 / u64 版を使う（構造体のパディングとエンディアンに依存させない）。
- `ink_fluid` の `fluid_hash` は Phase 2 の実装（`fluid.c` 内の `static` 関数）のまま変えない。

### 2.3 `game.h`：定数・`GameState`・公開 API（凍結）
```c
#define GAME_MAX_SHRINES         4
#define GAME_MAX_EVENTS          128
#define GAME_MAX_REWARD_CHOICES  3
#define GAME_MAX_OWNED_UPGRADES  32
#define GAME_MAX_OWNED_RELICS    16
#define GAME_MAX_PENDING_IMPULSE 8
#define GAME_MAIN_ACT_COUNT      3 /* 本編の幕数。P0 の ActDef は第 1 幕だけ */
#define GAME_BOSS_WAVE_INDEX     3 /* 0..2 が通常ウェーブ、3 がボスウェーブ */

typedef enum { RUN_PHASE_PLAYING = 0, /* … architecture §3.1 と同じ 7 値・同じ順 … */ } RunPhase;

typedef struct {
    float x, y;
    float hit_radius_proj;    /* 32（敵弾） */
    float hit_radius_contact; /* 42（敵の接触） */
    int32_t hp, max_hp;
    float invuln_t;           /* 残りの無敵時間（秒） */
} ShrineState;

typedef struct {
    float ink, ink_max;
    bool dry;                 /* かすれ筆（墨 < 20） */
    bool painting;            /* このティックに風筆を注入した */
    float stroke_speed;       /* 筆先の速さ（px/s）。筆擦れの音量に使う */
    float idle_t;             /* 筆を離してからの秒数 */
    float cyclone_cd, cyclone_cd_total;
    float gale_cd, gale_cd_total;
    float surge_t;            /* 墨昇華の残り秒数（0 = 非発動） */
    float cursor_x, cursor_y; /* 最後に受け取った筆先（画面 px） */
    float aim_x, aim_y;       /* 風斬りの向き（単位ベクトル。未確定は 0, 0） */
} PlayerState;

typedef struct { UpgradeId id; uint8_t level; } OwnedUpgrade;

typedef enum { REWARD_KIND_NONE = 0, REWARD_KIND_UPGRADE, REWARD_KIND_RELIC, REWARD_KIND_FALLBACK } RewardKind;

typedef struct {
    RewardKind kind;
    int32_t choice_count;                               /* 0..GAME_MAX_REWARD_CHOICES */
    int32_t choice_ids[GAME_MAX_REWARD_CHOICES];        /* kind に応じた UpgradeId */
    uint8_t choice_next_level[GAME_MAX_REWARD_CHOICES]; /* 取得後の Lv（1..3） */
    bool repair_available;                              /* CMD_REPAIR_SHRINE が有効 */
    uint32_t serial;                                    /* 提示を作るたびに +1（リロールを含む） */
} RewardOffer;

typedef struct {
    int32_t act_index, act_count;  /* act_index は 0 起点。P0 の act_count は 1 */
    int32_t wave_index;            /* 0..GAME_BOSS_WAVE_INDEX */
    int32_t wave_kills, wave_quota;/* ノルマに数える撃破（その幕の通常敵）。ボスウェーブの quota は 0 */
    int32_t acts_cleared;
    int32_t kills;                 /* すべての撃破（ボスを含む）の累計 */
    int32_t score;
    int32_t chain, max_chain;
    float chain_t;                 /* 一筆連殺の残り猶予（秒） */
    int32_t ignitions, storm_zaps, thunderbursts, cataclysms;
    int32_t rerolls_left;
    RewardOffer reward;
    OwnedUpgrade upgrades[GAME_MAX_OWNED_UPGRADES]; /* 取得順。同じ札は 1 要素の level を上げる */
    int32_t upgrade_count;
    EntityId boss_id;              /* 生存中のボス。いない・倒した後は {ECS_INVALID_INDEX, 0} */
    bool run_settled;
} RunProgress;

typedef struct { float x, y, rad_cells, peak_speed; } PendingImpulse;

struct GameState {
    FluidState fluid;
    EcsWorld ecs;
    GameComponents comps;
    RunConfig config;
    uint64_t rng;          /* 戦闘・報酬で共用するランの RNG（SplitMix64） */
    uint64_t ticks;
    RunPhase phase;
    float phase_t;         /* 現フェーズに入ってからの秒数 */
    float hitstop_t;
    PlayerState player;
    ShrineState shrines[GAME_MAX_SHRINES];
    int32_t shrine_count;
    RunProgress run;
    GameEvent events[GAME_MAX_EVENTS];
    int32_t event_count;
    uint32_t dropped_events;   /* ラン開始からの累計 */
    PendingImpulse pending_impulses[GAME_MAX_PENDING_IMPULSE];
    int32_t pending_impulse_count;
    GameInternal internal;     /* P3-A 専有。render / audio / app は読まない */
};
```
- **公開読み取り契約**: `internal` 以外のすべてのフィールドは P3-B が読んでよい。意味（単位・範囲・更新時期）は上の注記と §2.5〜§2.7 で固定し、P3-A はフィールド名・型・意味を変えない。値の書き込みは `ink_game` だけが行う（例外は、テストと見本帳 `--showcase` で場面を組み立てるときだけ）。
- `game_internal.h` の Pre 時点の内容は `typedef struct { uint32_t reserved; } GameInternal;` だけにする。

```c
typedef enum {
    GAME_HASH_CORE = 0,   /* ticks, rng, phase, phase_t, hitstop_t, config */
    GAME_HASH_FLUID,      /* fluid_hash() */
    GAME_HASH_ECS,        /* gen / comp_mask / alive / pending_destroy の 0..high_water-1、free_list[0..free_count)、各カウンタ */
    GAME_HASH_COMPONENTS, /* 生存エンティティの、マスクを持つ Component だけ（index 昇順・宣言順） */
    GAME_HASH_RUN,        /* player, shrines, shrine_count, run */
    GAME_HASH_EVENTS,     /* event_count, events[0..count), dropped_events, pending_impulses */
    GAME_HASH_INTERNAL,   /* GameInternal（P3-A が定義順に加える） */
    GAME_HASH_SECTION_COUNT
} GameHashSection;

GameState *game_create(const RunConfig *cfg); /* calloc + game_init。cfg == NULL はプリセット（豪筆・seed 1） */
void game_destroy(GameState *g);
void game_init(GameState *g, const RunConfig *cfg);
void game_step(GameState *g, const BrushInput *in, const GameCommand *cmd); /* in / cmd の NULL は入力なし */
void game_hash_sections(const GameState *g, uint64_t out[GAME_HASH_SECTION_COUNT]);
uint64_t game_hash(const GameState *g); /* 各セクションのハッシュを番号順に ink_hash_u64 で畳み込む */
bool game_push_event(GameState *g, GameEvent ev); /* 満杯なら dropped_events++ して false */

MetaRunResult game_run_result(const GameState *g);
bool meta_settle_run(MetaState *meta, GameState *g, MetaSettleBreakdown *out_breakdown);

UpgradeAxis game_upgrade_axis(UpgradeId id);             /* 不正な id は AXIS_COMMON */
uint8_t game_upgrade_max_level(UpgradeId id);            /* P0 は全札 3。不正な id は 0 */
uint8_t game_upgrade_level(const GameState *g, UpgradeId id);
bool game_is_recommended_axis(BrushStyleId style, UpgradeAxis axis);

EntityId game_spawn_entity(GameState *g, uint64_t comp_mask);
bool game_add_components(GameState *g, EntityId id, uint64_t comp_mask);
const TransformComp *game_transform(const GameState *g, EntityId id);
TransformComp *game_transform_mut(GameState *g, EntityId id);
/* 同じ形で game_velocity / game_health / game_status / game_enemy / game_boss / game_proj /
   game_orb / game_gimmick と、それぞれの _mut（計 18 関数） */

/* テスト・開発用（§2.7）。通常のプレイ経路からは呼ばない */
EntityId game_debug_spawn_enemy(GameState *g, EnemyDefId def, float x, float y);
EntityId game_debug_spawn_projectile(GameState *g, ProjKind kind, float x, float y, float vx, float vy);
EntityId game_debug_spawn_orb(GameState *g, float x, float y);
void game_debug_start_wave(GameState *g, int32_t wave_index);
void game_debug_force_wave_clear(GameState *g);
BrushInput game_autoplay_input(const GameState *g);
GameCommand game_autoplay_command(const GameState *g);
```
- `g == NULL` は全関数で何もしない（戻り値は `NULL` / `false` / 0 / 無効 ID / 既定値）。
- `game_create` 以外で `malloc` / `free` を使わない。
- **置き換える既存 API**: Phase 2 の `game_create(uint64_t seed)`、`game_init(GameState *, uint64_t)`、`game_step_smoke` は削除し、上の API に置き換える。`main` の時点で呼び出し側は `src/app/main.c` と `tests/test_core_smoke.c` の 2 ファイルだけで、どちらも本タスクで直す（§2.10・§3.2）。`audio_render_silence` も同じ 2 ファイルから呼ばれている（§2.10）。

### 2.4 `game_ids.h`：内容 ID とフラグ（凍結）

| 型 | P0 の値（この順で 0 から） | 備考 |
| :--- | :--- | :--- |
| `GameVec2` | `{float x, y;}` | 弾の軌跡などに使う |
| `EnemyDefId` | `ENEMY_ARCHER`（弓妖）、`ENEMY_LANTERN_FIRE`（提灯妖・炎）、`ENEMY_UMBRELLA`（唐傘妖）、`ENEMY_BOSS_TENGU`（大天狗）、`ENEMY_DEF_COUNT` | |
| `MassTier` | `MASS_TURRET`、`MASS_LIGHT`、`MASS_MEDIUM`、`MASS_HEAVY`、`MASS_BOSS` | game-design §6.2 |
| `ProjKind` | `PROJ_ARROW`、`PROJ_FIREBALL`、`PROJ_STORM_ORB`、`PROJ_KIND_COUNT` | 雷球は大天狗が撃つ |
| `UpgradeId` | `UPG_EMBER_TIP`、`UPG_WILDFIRE`、`UPG_CHARGED_SEAL`、`UPG_CHAIN_LIGHTNING`、`UPG_GREAT_VORTEX`、`UPG_SLAM_SHOCKWAVE`、`UPG_RICH_INK`、`UPG_LASTING_SURGE`、`UPG_COUNT` | game-design §12 の P0 8 種 |
| `UpgradeAxis` | `AXIS_BLAZE`（紅蓮）、`AXIS_THUNDER`（雷霆）、`AXIS_GALE`（烈風）、`AXIS_INKWAY`（墨道）、`AXIS_COMMON`（共通）、`AXIS_COUNT` | |
| `ReactionKind` | `REACT_BLAZING`、`REACT_TEMPEST`、`REACT_THUNDERBURST`、`REACT_CATACLYSM`、`REACT_COUNT` | P0 で起きるのは前 2 つ |
| `HintId` | `HINT_STROKE`、`HINT_CYCLONE_FIREBALL`、`HINT_GALE_UMBRELLA`、`HINT_INK_REGEN`、`HINT_REACT_BLAZING`、`HINT_REACT_TEMPEST`、`HINT_REACT_THUNDERBURST`、`HINT_REACT_CATACLYSM`、`HINT_COUNT` | `seen_hints_mask` のビット番号。発火と UI は Phase 4 |
| `SkillId` | `SKILL_CYCLONE`、`SKILL_GALE`、`SKILL_COUNT` | `EV_SKILL_DENIED` の `value` |

| 内容メタ（`game_contract.c` の `static const` 表） | 値 |
| :--- | :--- |
| 札の軸 | 火種の筆・延焼 = 紅蓮、帯電印・連鎖雷 = 雷霆、大渦・叩きつけ衝撃 = 烈風、墨玉増量・長き昇華 = 墨道 |
| 札の最大 Lv | すべて 3 |
| 筆スタイルの推奨軸 | 豪筆 = 烈風・墨道、朱筆 = 紅蓮・雷霆、細筆 = 雷霆・墨道（game-design §5.6） |

```c
#define DMG_PHYSICAL  (UINT32_C(1) << 0)
#define DMG_FIRE      (UINT32_C(1) << 1)
#define DMG_STORM     (UINT32_C(1) << 2)
#define DMG_SLAM      (UINT32_C(1) << 3)
#define DMG_DEFLECTED (UINT32_C(1) << 4)
#define DMG_SECONDARY (UINT32_C(1) << 5) /* 衝撃波などの派生ダメージ。フックを再入させない */
```

### 2.5 `game_events.h`：入力・コマンド・イベント（凍結）
- `BrushInput`、`GameCommandType`、`GameCommand` は architecture §7.2 のとおり（フィールド名・型・順を変えない）。
- **入力の検証**（`game_step` の境界）: `BrushInput` の 4 つの座標のどれかが非有限なら、そのティックは `in == NULL` と同じに扱う。有限なら画面外の値もそのまま使う（流体は格子と交差する範囲だけに注入する）。`GameCommand.type` が `GameCommandType` の範囲外なら `CMD_NONE` と同じに扱う。
- `GameEventType` は architecture §7.1 の 22 値の後ろに `EV_ENEMY_INTRO`、`EV_REWARD_REROLL`、`EV_SKILL_DENIED`、`EV_TYPE_COUNT` を追記する。`GameEvent` は §7.1 のとおり。
- 同じティック内のイベントは、パイプラインの実行順（architecture §3.2）に並ぶ。

| イベント | P0 | `x, y` | `x2, y2` | `value` | `flags` |
| :--- | :---: | :--- | :--- | :--- | :--- |
| `EV_KILL` | ○ | 撃破位置 | 衝撃の向き（単位ベクトル、なければ 0） | `EnemyDefId` | 致命打の `DMG_*` |
| `EV_DEFLECT` | ○ | 弾の位置 | 偏向後の向き（単位） | `ProjKind` | 0 |
| `EV_SLAM` | ○ | 衝突点 | 壁の法線、または相手への向き | `EnemyDefId`（飛ばされた側） | `EVF_SLAM_WALL` / `EVF_SLAM_ENEMY` |
| `EV_IGNITE` | ○ | 燃焼セルの重心（px） | — | 燃焼セル数 | 0。「紅蓮墨嵐！」の表示条件（24 セル以上・3.2 秒間隔）を満たしたときだけ |
| `EV_STORM_ZAP` | ○ | 始点 | 終点 | ホップ番号（0 = セルから最初の敵） | 0 |
| `EV_THUNDERBURST` / `EV_CATACLYSM` | — | （Phase 4） | | | |
| `EV_CYCLONE` | ○ | 中心 | — | 半径（px、整数） | 0 |
| `EV_GALE` | ○ | 始点 | 向き（単位） | 0 | 0 |
| `EV_ORB_PICKUP` | ○ | 回収位置 | — | 墨玉の価値（16。実際の回復量ではない） | `EVF_ORB_AUTO`（ウェーブクリア時の自動回収） |
| `EV_SHRINE_HIT` | ○ | 社の位置 | 被弾元への向き（単位） | 被弾後の HP | 0 |
| `EV_SURGE_START` | ○ | 筆先 | — | チェイン数 | 0。非発動から発動に変わったときだけ（延長では出さない） |
| `EV_BOSS_ENTER` / `EV_BOSS_PHASE` | ○ | ボスの位置 | — | `EnemyDefId` / 新しいフェーズ番号 | 0 |
| `EV_WAVE_CLEAR` / `EV_ACT_CLEAR` | ○ | 社の位置 / ボスの位置 | — | `wave_index` / `act_index` | 0 |
| `EV_REWARD_PICK` | ○ | — | — | `UpgradeId`、修繕は -1 | `EVF_REWARD_REPAIR` |
| `EV_HINT_TRIGGER` / `EV_WIND_TELEGRAPH` | — | （Phase 4） | | `HintId` | |
| `EV_GAME_OVER` | ○ | 社の位置 | — | 0 | `EVF_RUN_ABORTED`（`CMD_ABORT_RUN` のとき） |
| `EV_VICTORY` | ○ | — | — | `act_index` | 0 |
| `EV_ENEMY_INTRO` | ○ | — | — | `EnemyDefId` | 0。その種類がランで初めて出るウェーブの開始時 |
| `EV_REWARD_REROLL` | ○ | — | — | 残りリロール回数 | 0 |
| `EV_SKILL_DENIED` | ○ | 筆先 | — | `SkillId` | 0。墨不足または CD 中に発動しようとした |

`EVF_SLAM_WALL` / `EVF_SLAM_ENEMY` / `EVF_ORB_AUTO` / `EVF_RUN_ABORTED` / `EVF_REWARD_REPAIR` は `(UINT32_C(1) << 8)` 以降に定義し、`DMG_*` の下位ビットと重ねない。

### 2.6 `game_components.h` とアクセサ（凍結）
- マスクは architecture §5.2 のビット順で `COMP_TRANSFORM`、`COMP_VELOCITY`、`COMP_HEALTH`、`COMP_STATUS`、`COMP_ENEMY`、`COMP_BOSS`、`COMP_PROJ`、`COMP_INK_ORB`、`COMP_GIMMICK`（`UINT64_C(1) << 0` 〜 `<< 8`）、`COMP_ALL_MASK` はその OR。
- `GameComponents` は各 Component の `[ECS_MAX_ENTITIES]` 配列を持つ（約 0.2 MB）。

| Component | フィールド（宣言順。通常の字は architecture §5.2 のとおり、**太字**は Phase 3 の追加） |
| :--- | :--- |
| `TransformComp` | `x, y, radius, facing`, **`bool dissolving; float dissolve_t`**（墨還り中か・経過秒） |
| `VelocityComp` | `vx, vy`, `MassTier mass_tier`, `drag, fluid_coupling` |
| `HealthComp` | `int32_t hp, max_hp`, `flash_t, invuln_t` |
| `StatusComp` | `ink_mark_t, burn_acc, charge_t, knockback_t, stun_t, barrier_t` |
| `EnemyComp` | `EnemyDefId def_id`, `bool is_elite`, `uint32_t elite_flags`, `int32_t phase`, `fire_cd`, `int32_t ai_state`, `absorbed_ink`, **`float windup`**（0 = 待機、(0, 1] = 攻撃の構えの進行度） |
| `BossComp` | `int32_t pattern`, `pattern_timer`, `int32_t phase_idx`, `gale_dx, gale_dy, gale_t`, `int32_t part_id`, `EntityId parent_id`, **`int32_t telegraph_pattern; float telegraph`**（予兆中の技と進行度 0..1） |
| `ProjComp` | `ProjKind kind`, `bool deflected, burning, electrified`, **`bool accelerated`**（偏向前の雷加速を 1 回使った）, `int32_t pierce, bounces_left`, `age`, `GameVec2 trail[PROJ_TRAIL_LEN]`（20）, `int32_t trail_n`（`trail[0]` が最古、`trail[trail_n-1]` が最新） |
| `InkOrbComp` | `value, age` |
| `GimmickComp` | `int32_t kind, elem_kind`, `timer, param_a, param_b`（P0 では使わない） |

- **アクセサ**: `game_<comp>(g, id)` は `ecs_has_mask(&g->ecs, id, COMP_<X>)` が真のときだけ `&g->comps.<x>[id.index]` を返し、それ以外は `NULL` を返す。`_mut` 版も同じ。
- **`game_spawn_entity(g, mask)`**: `mask` が 0 か `COMP_ALL_MASK` 以外のビットを含むなら何もせず無効 ID を返す。`ecs_spawn` が満杯なら無効 ID。成功したら `mask` の各 Component スロットを `memset` で 0 にしてからマスクを付ける。
- **`game_add_components(g, id, mask)`**: 生存していて `mask` が有効なときだけ、新しく付くビットのスロットを 0 にしてからマスクを足す（`ecs_remove_mask` 後の古い値を残さない）。
- **`game_push_event`**: `event_count < GAME_MAX_EVENTS` なら追加して `true`。満杯なら `dropped_events` を `UINT32_MAX` で飽和させながら加算して `false`。

### 2.7 フェーズの契約と、P3-A へ移譲する骨格
**`game_init(g, cfg)` の契約**（P3-A も保つ）

| 項目 | 値 |
| :--- | :--- |
| ゼロ化 | `memset(g, 0, sizeof *g)`（`*g = (GameState){0}` は 1 MB 超の一時オブジェクトをスタックに作りうるので使わない） |
| 設定 | `config = cfg ? *cfg : meta_run_config_preset(BRUSH_STYLE_HEAVY, 1)`、`rng = config.seed` |
| 下位モジュール | `fluid_init(&g->fluid, NULL)`、`ecs_init(&g->ecs)` |
| 開始フェーズ | `RUN_PHASE_ACT_TRANSITION`、`phase_t = 0`、`run.act_index = 0`、`run.act_count = 1`（P0） |
| 社 | `shrine_count = 1`、`(640, 360)`、半径 32 / 42、`max_hp = hp = 8 + min(config.meta_upgrade_lv[META_UPG_SHRINE_HP], meta_upgrade_max_level(META_UPG_SHRINE_HP))` |
| 墨 | `ink_max = ink = 100 + 10 × min(META_UPG_INK_MAX の段階, 最大段階)` |
| リロール | `rerolls_left = 1 + min(META_UPG_REROLLS の段階, 最大段階)` |
| ボス | `run.boss_id = {ECS_INVALID_INDEX, 0}` |

**フェーズと `ticks` の契約**（architecture §3.2 の補足として §4 で反映する）
- 段 1 では、**コマンドを処理してから**、その時点のフェーズで停止フェーズの分岐を判定する。報酬を決めたティックから次のウェーブが動き、そのティックで `ticks` も進む。
- `ticks` と `phase_t` は、PLAYING（ヒットストップ中を含む）、WAVE_AFTERMATH、ACT_TRANSITION、SHRINE_BREAK のティックで進み、REWARD、VICTORY、GAME_OVER のティックでは進まない。1 回の `game_step` で `ticks` が増えるのは 0 か 1 で、0 になるのはステップ後のフェーズが REWARD / VICTORY / GAME_OVER のときだけ。`phase_t` は遷移時に 0 へ戻る。
- ラン開始は ACT_TRANSITION（2.0 秒）から始まり、終了時に第 1 ウェーブを始めて PLAYING へ移る。
- P0 では最終幕（`act_index == act_count - 1`）のボスを倒すと、大遺物と社の回復なしで WAVE_AFTERMATH → VICTORY へ移る。
- `CMD_ABORT_RUN` は PLAYING / WAVE_AFTERMATH / REWARD / ACT_TRANSITION / SHRINE_BREAK で有効で、直ちに GAME_OVER へ移り `EV_GAME_OVER`（`EVF_RUN_ABORTED`）を出す。VICTORY / GAME_OVER では no-op。社 HP 0 による敗北では、SHRINE_BREAK に入るティックで `EV_GAME_OVER` を出す。
- `CMD_START_ENDLESS` と `CMD_PICK_ENDLESS_CRYSTAL` は、エンドレスを実装するまで常に no-op。

**Pre の骨格 `game_step`**（P3-A が全面的に置き換える）
1. `event_count = 0`。入力を §2.5 のとおり検証する。`CMD_ABORT_RUN` を上の契約どおりに処理する（P3-B が終了時の精算を骨格の上で確かめられるようにするため）。その後、REWARD / VICTORY / GAME_OVER なら戻る。
2. ACT_TRANSITION: `phase_t += INK_FIXED_DT`、2.0 秒で PLAYING へ。
3. PLAYING: `in->paint` のとき、`(pmx, pmy)` → `(mx, my)` の中点に `fluid_splat`（力 = 移動量 × 0.58 を ±46 にクランプ、半径 3.7、墨 0.4）。続けて `fluid_step(&g->fluid, INK_FIXED_DT)` と `fluid_sanitize_channels`。`player.cursor_*` を更新する。
4. `ecs_flush_destroy`、`ticks++`。

この骨格により、P3-B は P3-A の完成を待たずに流体の描画を実機で確かめられる。

**デバッグ API と自動入力**（Pre は暫定実装、P3-A が本実装に置き換える）

| 関数 | 本実装の意味（P3-A） | Pre の暫定実装 |
| :--- | :--- | :--- |
| `game_debug_spawn_enemy` | `EnemyDef` の生成関数で、通常のスポーンと同じ Component・初期値を付ける（位置の抽選だけ省く）。ボスなら `run.boss_id` も設定する | Transform・Velocity・Health・Status・Enemy（ボスは Boss も）を付け、半径 16（ボス 44）、HP 20（ボス 400） |
| `game_debug_spawn_projectile` | 通常の発射と同じ初期化 | Transform・Velocity・Proj |
| `game_debug_spawn_orb` | 墨玉のドロップと同じ初期化 | Transform・Velocity・InkOrb（`value = 16`） |
| `game_debug_start_wave(g, w)` | 現在の幕のウェーブ `w` を通常のウェーブ開始と同じ処理で始める（墨の回復・初期編成・`EV_ENEMY_INTRO`・ボスなら `EV_BOSS_ENTER`）。範囲外は no-op | `wave_index` とノルマ（6 / 8 / 10 / 0）を設定して PLAYING にする |
| `game_debug_force_wave_clear` | PLAYING のときだけ有効。通常ウェーブはノルマ到達として扱う。ボスウェーブはボスへ通常のダメージ経路で致死ダメージを与える。どちらも遷移は次の `game_step` のステップ 8 で起きる | `wave_kills = wave_quota` |
| `game_autoplay_input` / `_command` | 状態だけから決まる決定的な自動入力（`g` を変えず、`g->rng` も使わない）。テスト・ベンチ・ヘッドレススモーク・`--smoke-frames`・将来の `--demo` で使う | `ticks` から決まる円運動のドラッグ / REWARD なら `CMD_PICK_REWARD 0`、それ以外は `CMD_NONE` |

- 暫定実装の各関数には、P3-A が置き換えたことを確認できるよう、本体の先頭に `/* P3-Pre stub */` を 1 行入れる。

### 2.8 `meta_settle_run` と `game_run_result`（`game_contract.c`、凍結）

| `MetaRunResult` | `GameState` からの値 |
| :--- | :--- |
| `acts_cleared` | `run.acts_cleared` |
| `endless_acts_cleared` | 0（エンドレスは未実装） |
| `reached_act` | `run.act_index + 1` |
| `kills` / `max_chain` / `thunderbursts` / `cataclysms` / `score` | `run` の同名フィールド |
| `cleared` | `run.acts_cleared >= GAME_MAIN_ACT_COUNT`（P0 の勝利は本編クリアに数えない） |
| `crystal_bonus_x15` | `false`（大遺物は Phase 4） |

- `meta_settle_run(meta, g, out)`: `g == NULL` なら `out` を 0 にして `false`。それ以外は `meta_settle_result(meta, &res, &g->run.run_settled, out)` を返す。呼ぶのは VICTORY / GAME_OVER に入った後だけ（app の責務）。

### 2.9 app の純粋モジュール（raylib 非依存、`test_app_core` で検証）
**CLI（`cli.h` / `cli.c`、Pre で完全に実装する）**
```c
typedef enum { APP_MODE_WINDOW = 0, APP_MODE_HEADLESS, APP_MODE_SHOWCASE } AppMode;
typedef enum { SAVE_MODE_DEFAULT = 0, SAVE_MODE_DISABLED, SAVE_MODE_EXPLICIT } SaveMode;
#define APP_FAULT_SHADER (UINT32_C(1) << 0)
#define APP_FAULT_FONT   (UINT32_C(1) << 1)

typedef struct {
    AppMode mode;
    int smoke_frames;      /* 0 = 通常起動。1..INT_MAX = その回数だけ描画して終了 */
    bool use_post;
    SaveMode save_mode;
    const char *save_path; /* SAVE_MODE_EXPLICIT のときだけ非 NULL。argv の借用 */
} CliOptions;

bool cli_parse(int argc, char *const argv[], CliOptions *out); /* false は Usage エラー（out は未定義） */
void cli_print_usage(FILE *stream, const char *program);       /* 1 行目は "Usage: " で始める */
uint32_t cli_parse_debug_faults(const char *value, FILE *warn); /* INKSTORM_DEBUG_FAULTS の値 */
```

| 規則 | 内容 |
| :--- | :--- |
| オプション | `--headless-smoke`、`--smoke-frames N`、`--no-post`、`--no-save`、`--save-path P`、`--showcase` |
| 排他 | `--headless-smoke` と `--smoke-frames` / `--showcase` / `--save-path`。`--save-path` と `--no-save` / `--smoke-frames` / `--showcase` |
| 値 | `N` は 10 進の全体が数字で `1..INT_MAX`。`P` は空でなく、`--` で始まらず、`PATH_MAX` 未満 |
| エラー | 未知のオプション（`--demo`、`--snap` は Phase 6 まで未知）、値の欠落、不正な値、排他、**同じオプションの重複** |
| セーブ | `--headless-smoke` / `--smoke-frames` / `--showcase` / `--no-save` は `SAVE_MODE_DISABLED`、`--save-path` は `SAVE_MODE_EXPLICIT`、それ以外は `SAVE_MODE_DEFAULT` |
| 解析中の副作用 | ファイル I/O・ウィンドウ・確保をしない（Usage エラーを、何も起こさずに返すため） |
| 障害の注入 | `cli_parse_debug_faults`: カンマ区切りの `shader` / `font` を対応ビットにする。前後の空白は無視、`NULL`・空は 0。未知の語は `warn` に `INKSTORM: unknown debug fault '<語>' ignored` を 1 行出して無視する（`warn == NULL` なら出さない） |

- `cli.h` は `<stdbool.h>`・`<stdint.h>`・`<stdio.h>` を include し、`cli.c` の `PATH_MAX` 検査には `<limits.h>` を使う。

**フレーム時計（`frame_clock.h` / `frame_clock.c`、Pre で完全に実装する）**
```c
typedef struct { double accumulator; } FrameClock;
void frame_clock_reset(FrameClock *c);
int frame_clock_advance(FrameClock *c, double frame_dt); /* このフレームに回すティック数（0..15） */
```
- `frame_dt` が非有限か負なら 0 として扱い、`0.25` 秒で頭打ちにする。`accumulator += dt` の後、`accumulator >= 1.0 / INK_TICK_HZ` の間だけ引き算して数える。

**入力ラッチ（`input_latch.h` / `input_latch.c`、Pre は API と素通しの骨格、規則は P3-B §2.7）**
```c
typedef struct {
    float mouse_x, mouse_y;  /* 論理座標（1280×720） */
    bool left_down;
    bool cyclone_pressed;    /* 右クリック または E */
    bool gale_pressed;       /* Space */
    int32_t pick_pressed;    /* 0..2（1〜3 キー・札のクリック）、-1 = なし */
    bool reroll_pressed;     /* R */
    bool repair_pressed;     /* 4・修繕札のクリック */
    bool confirm_pressed;    /* Enter・リザルトのクリック */
    bool ui_click;           /* このフレームの左クリックを UI 操作として使った */
} RawInput;

typedef struct {
    float prev_x, prev_y;
    float cur_x, cur_y;
    bool left_down;
    /* P3-B で押下ラッチ・抑止フラグを追加する（input_latch.h は P3-B 移譲対象） */
} InputLatch;

void input_latch_init(InputLatch *l, float x, float y);
void input_latch_feed(InputLatch *l, const RawInput *raw);          /* 描画フレームごとに 1 回 */
BrushInput input_latch_take_tick(InputLatch *l);                    /* game_step の直前に 1 回 */
GameCommand input_latch_take_command(InputLatch *l, RunPhase phase); /* 同上 */
bool input_latch_take_confirm(InputLatch *l);
```

### 2.10 P3-B へ移譲する枠
- **`audio.h`**: 次の枠を置き、`audio.c` は無音を返す暫定実装にする。`audio_render_silence` は削除する。
  ```c
  #define AUDIO_SAMPLE_RATE 44100
  typedef enum { BGM_NONE = 0, BGM_LOBBY, BGM_BATTLE, BGM_BOSS, BGM_VICTORY } BgmTrack;
  struct GameState;
  void audio_init(AudioState *audio, uint64_t seed);
  void audio_set_volumes(AudioState *audio, uint8_t master, uint8_t bgm, uint8_t se);
  void audio_consume_events(AudioState *audio, const struct GameState *game);
  void audio_render(AudioState *audio, int16_t *samples, size_t sample_count);
  BgmTrack audio_bgm_track(const AudioState *audio);
  bool audio_validate_finite(const AudioState *audio);
  ```
- **`ui_view.h`**（raylib 非依存）: `UiScene`（`UI_SCENE_RUN`、`UI_SCENE_SHOWCASE`。ロビー・墨庵・ポーズ・設定は Phase 4 で追記）、`UI_HOVER_NONE`（-1）・`UI_HOVER_REPAIR`（3）・`UI_HOVER_REROLL`（4）、`UiView { UiScene scene; int32_t hover_choice; bool xray; uint8_t shake_scale; bool settled; MetaSettleBreakdown settle; int32_t crystals_after; int32_t showcase_page; }`。
- **`render.h` / `render.c`**: 最初から P3-B §2.1 と一致する公開 API 枠へ揃えておく（P3-B でのシグネチャ変更をなくすため）：
  ```c
  typedef struct {
      bool use_post;
      uint32_t fault_mask; /* APP_FAULT_SHADER / APP_FAULT_FONT（§2.9） */
  } RenderOptions;

  void render_init(RenderState *r, const RenderOptions *opt);
  void render_reset_vfx(RenderState *r, uint64_t seed);
  void render_consume_events(RenderState *r, const GameState *g);
  void render_update_vfx(RenderState *r, const GameState *g, float dt);
  void render_draw(RenderState *r, const GameState *g, const UiView *ui);
  void render_close(RenderState *r);
  ```
  Pre の `render.c` は既存のポストシェーダ＋テキスト描画をこのシグネチャで包み、`render_reset_vfx` / `render_consume_events` / `render_update_vfx` は no-op、`render_draw` は `RenderState` 内の軽量カウンタ（例: `uint64_t frames_drawn`）を更新して非 `const` の `RenderState *r` を維持する（cppcheck の `constParameterPointer` 対策）。
- **`text_table.h` / `text_table.c`**（raylib 非依存）: `TextId`（`TEXT_NONE`、`TEXT_GAME_TITLE`、`TEXT_COUNT`）、`const char *text_get(TextId id, bool use_ja)`。
- **`save.h` / `save.c`**（raylib 非依存）: `SaveLoadResult`（`OK` / `MISSING` / `CORRUPT` / `IO_ERROR`）、`SaveIoOps`（`mkdir`、`fopen`、`fwrite`、`fflush`、`fclose`、`rename`、`remove` の関数ポインタ）、`save_resolve_path`、`save_load`、`save_write`。Pre の実装は、`save_load` が既定の `MetaState` を書いて `SAVE_LOAD_MISSING`、他は `false` を返すだけにする。
- **`main.c`**: `cli_parse` の結果で分岐する（`APP_MODE_SHOWCASE` は Pre では通常のウィンドウと同じ）。`GameState`（`game_create(NULL)`）に加えて `RenderState`・`AudioState` も起動時に `calloc` して終了時に `free` する（将来の肥大化でスタックを圧迫しないため）。`FrameClock`、`InputLatch`、`game_step(game, &in, &cmd)`、`audio_consume_events` / `render_consume_events` / `render_update_vfx` の毎ティック呼び出しへ置き換え、ヘッドレスは `game_autoplay_input` で 5 ティック、`audio_init(audio, 1)` と `audio_render` を使う。

---

## 3. テストターゲット

### 3.1 `CMakeLists.txt` への追記と `tools/expect_usage.sh`
`add_test(NAME test_roguelike_meta COMMAND test_roguelike_meta)` の**直後**（`endif()` の前）に次を入れる。P2-Int の結果で並びが違う場合は、`if(BUILD_TESTING)` ブロックの末尾に入れる。

```cmake

    add_executable(test_elements tests/test_elements.c)
    target_link_libraries(test_elements PRIVATE unity ink_game ink_core m)
    add_test(NAME test_elements COMMAND test_elements)
    add_executable(test_determinism_run tests/test_determinism_run.c)
    target_link_libraries(test_determinism_run PRIVATE unity ink_game ink_core m)
    add_test(NAME test_determinism_run COMMAND test_determinism_run)
    add_executable(bench_game tests/bench_game.c)
    target_link_libraries(bench_game PRIVATE ink_game ink_core m)
    add_executable(test_audio tests/test_audio.c)
    target_link_libraries(test_audio PRIVATE unity ink_audio ink_game ink_core m)
    add_test(NAME test_audio COMMAND test_audio)
    add_executable(test_app_save tests/test_app_save.c src/app/save.c src/app/paths.c)
    target_include_directories(test_app_save PRIVATE src/app)
    target_link_libraries(test_app_save PRIVATE unity ink_game ink_core)
    add_test(NAME test_app_save COMMAND test_app_save)
    add_executable(test_app_core tests/test_app_core.c src/app/cli.c src/app/frame_clock.c
        src/app/input_latch.c)
    target_include_directories(test_app_core PRIVATE src/app)
    target_link_libraries(test_app_core PRIVATE unity ink_game ink_core)
    add_test(NAME test_app_core COMMAND test_app_core)
    add_executable(test_text_table tests/test_text_table.c src/render/text_table.c)
    target_include_directories(test_text_table PRIVATE src/render)
    target_link_libraries(test_text_table PRIVATE unity ink_game ink_core)
    add_test(NAME test_text_table COMMAND test_text_table)
    set(expect_usage sh "${CMAKE_CURRENT_SOURCE_DIR}/tools/expect_usage.sh" "$<TARGET_FILE:inkstorm>")
    add_test(NAME test_cli_usage_unknown COMMAND ${expect_usage} --bogus)
    add_test(NAME test_cli_usage_save_exclusive COMMAND ${expect_usage} --no-save --save-path x)
    add_test(NAME test_cli_usage_headless_exclusive COMMAND ${expect_usage} --headless-smoke --smoke-frames 1)
```

```sh
#!/bin/sh
set -u
if [ "$#" -lt 1 ]; then
    printf 'usage: expect_usage.sh <program> [args...]\n' >&2
    exit 2
fi
program=$1
shift
output=$("$program" "$@" 2>&1)
status=$?
if [ "$status" -ne 1 ]; then
    printf 'expected exit status 1, got %s\n%s\n' "$status" "$output"
    exit 1
fi
if ! printf '%s\n' "$output" | grep -q '^Usage: '; then
    printf 'missing Usage line\n%s\n' "$output"
    exit 1
fi
```
- `tools/expect_usage.sh` は終了コードが**ちょうど 1** で、かつ出力に `Usage: ` で始まる行があることを確かめる。ASan の異常終了も終了コード 1 になるので、出力の判定と組み合わせて区別する。`lint` の ShellCheck の対象（`tools/*.sh`）になる。CLI の全組み合わせは `test_app_core`（§3.3）で検査し、こちらは実行ファイルへの配線と終了コードだけを見る。
- `bench_game` は `add_test` に登録しない（ASan の CTest でベンチを回さない）。CI の ASan ビルドは全ターゲットを作るので、`-Werror` で警告なくコンパイルできることは必須。
- `test_app_save`、`test_app_core`、`test_text_table` は `src/` の `.c` を直接コンパイルする。そのため **`save.*`、`paths.*`、`cli.*`、`frame_clock.*`、`input_latch.*`、`text_table.*` は raylib を include してはならない**（include パスがないのでビルドが失敗する。依存境界の機械的な検査を兼ねる）。

| CTest（Pre 後、計 14 件） | 担当 |
| :--- | :--- |
| `test_core_smoke`、`test_fluid`、`test_ecs`、`test_cli_headless_smoke`、`test_cli_usage_*`（3 件） | 凍結（`test_cli_*` の中身は P3-B の `main.c`） |
| `test_roguelike_meta`、`test_elements`、`test_determinism_run` | P3-A |
| `test_app_core`、`test_audio`、`test_app_save`、`test_text_table` | P3-B |

### 3.2 `tests/test_core_smoke.c`（凍結。並列期間中も通り続ける内容に限る）
- 既存の `test_clamp` と `test_splitmix64` は残す。`test_audio_writes_silence` は削除する（音は `test_audio` へ移す）。
- **ハッシュ補助**: `ink_hash_bytes(INK_HASH_INIT, "", 0) == 0xcbf29ce484222325`、`"a"` → `0xaf63dc4c8601ec8c`、`"foobar"` → `0x85944171f73967e8`、`ink_hash_u32(INK_HASH_INIT, 0x64636261)` → `0xfc179f83ee0724dd`（`"abcd"` と一致）、`ink_hash_f32(h, -0.0f) == ink_hash_f32(h, 0.0f)`、NaN の 2 つのビット列（`0x7fc00000` と `0xffc00001`）のハッシュが一致。
- **初期化の契約**: `game_create(NULL)` が非 NULL で、`phase == RUN_PHASE_ACT_TRANSITION`、`ticks == 0`、`shrine_count == 1`、社が `(640, 360)` で `hp == max_hp == 8`、`player.ink == 100`、`run.rerolls_left == 1`、`run.boss_id.index == ECS_INVALID_INDEX`。`game_step(g, NULL, NULL)` を 2 回で `ticks == 2`、`fluid_validate_finite`。同じ設定の 2 インスタンスで `game_hash` が一致し、seed だけ変えると異なる。
- **中断の契約**: `CMD_ABORT_RUN` で `phase == RUN_PHASE_GAME_OVER`、`events[0]` が `EV_GAME_OVER` で `flags & EVF_RUN_ABORTED`、そのティックで `ticks` が進まない。もう一度送ると no-op（`game_hash` が変わらない）。
- **アクセサ**: `game_spawn_entity(g, COMP_TRANSFORM | COMP_HEALTH)` の ID で `game_transform` が非 NULL、`game_proj` が NULL。`mask = 0` と未定義ビットは無効 ID。`ecs_destroy` 後（flush 前）は NULL。
- **イベント**: `game_push_event` を 130 回 → `event_count == 128`、`dropped_events == 2`、戻り値は 129 回目から `false`。

### 3.3 `tests/test_app_core.c`（Pre は CLI とフレーム時計を網羅する）

| 引数 | 期待 |
| :--- | :--- |
| なし | `WINDOW`、`smoke_frames == 0`、`use_post`、`SAVE_MODE_DEFAULT` |
| `--no-post` / `--no-save` / `--save-path /tmp/a.txt` | `use_post == false` / `DISABLED` / `EXPLICIT` と `save_path` |
| `--headless-smoke` / `--smoke-frames 5` / `--showcase --smoke-frames 3 --no-post` | `HEADLESS`・`DISABLED` / `WINDOW`・5・`DISABLED` / `SHOWCASE`・3・`use_post == false`・`DISABLED` |
| Usage エラー（`false`） | `--bogus`、`--demo`、`--snap d`、`--smoke-frames`、`--smoke-frames 0`、`-1`、`abc`、`5x`、`2147483648`、`--save-path`、`--save-path ""`、`--save-path --no-save`、`PATH_MAX` 文字のパス、`--no-save --save-path x`、`--headless-smoke --smoke-frames 1`、`--headless-smoke --showcase`、`--save-path x --smoke-frames 1`、`--save-path x --showcase`、`--save-path x --headless-smoke`、`--no-post --no-post`、`--smoke-frames 1 --smoke-frames 2` |

- **障害の注入**: `"shader"` → `APP_FAULT_SHADER`、`" font , shader "` → 両方、`""` と `NULL` → 0、`"shader,typo"` → `APP_FAULT_SHADER` で、`tmpfile()` に書いた警告が 1 行。
- **フレーム時計**: `1/60` を 60 回で合計 60 ティック、`1/144` を 144 回で合計 59〜60、`0.25` で 15、`10.0` で 15、`-1`・`NAN`・`INFINITY` で 0 かつ `accumulator` が変わらない、`frame_clock_reset` で 0。
- 入力ラッチの規則のテストは P3-B が追加する（P3-B §3.3）。

### 3.4 骨格テスト（移譲先が置き換える）

| ファイル | Pre の内容 |
| :--- | :--- |
| `test_elements.c` | `game_debug_start_wave(g, 0)` で PLAYING にしてから入力なしで 60 ティック進め、`fluid_validate_finite` が真 |
| `test_determinism_run.c` | 同じ設定の 2 インスタンスに同じ `game_autoplay_input` を 300 ティック与え、毎ティックの `game_hash_sections` が全セクション一致 |
| `bench_game.c` | 骨格の `game_step` を 120 + 600 ティック計測して `mean / p95` を出力し、終了コード 0 |
| `test_audio.c` | `audio_init` 直後の `audio_render` が全サンプル 0、`audio_consume_events(audio, NULL)` でクラッシュしない |
| `test_app_save.c` | `save_load(NULL, &meta)` が `SAVE_LOAD_MISSING` で、`meta` が既定値 |
| `test_text_table.c` | `TEXT_NONE` 以外の全 `TextId` で `ja` / `en` が非 NULL・非空、`en` が印字可能 ASCII（0x20〜0x7E）だけ |

---

## 4. 設計書の同期
各設計書の該当節を直接書き換える（経緯は書かず、確定した仕様として書く）。1〜5 は P2-Int の PR #5 の本文「設計書に未反映の決定」の表と一致することを確認済み。

| # | 項目 | 反映先 | 反映する内容 |
| :---: | :--- | :--- | :--- |
| 1 | `fluid_step` の処理順 | architecture §3.2 ステップ 4 | 渦度閉じ込め → 圧力投影 → 速度移流 → 圧力投影 → スカラー移流 → 統計 |
| 2 | 流体セルの座標規約 | architecture §4.1 | セル `(x, y)` は `[8x, 8x+8)` px を占め、中心は `(8x+4, 8y+4)` |
| 3 | ECS の走査補助 | architecture §5.1 | `ecs_entity_at()` を API 一覧に追加。走査の上限は各システムの開始時の `high_water` に固定する（走査中に生まれたものは次のシステムから） |
| 4 | 精算 API | architecture §8.1 | `meta_settle_result(meta, res, &flag, out)` と、`game.h` の `meta_settle_run(meta, game, out_breakdown)`（§2.8） |
| 5 | 墨晶 ×1.5 の端数 | game-design §10.1 | 切り捨て（`base / 2` を加える） |
| 6 | フェーズ・`ticks`・P0 の終幕 | architecture §3.1 / §3.2、game-design §12 | §2.7 の「フェーズと `ticks` の契約」。P0 は第 1 幕だけで、ボス撃破後は大遺物なしで勝利リザルト。豪筆以外の筆スタイルは P0 では豪筆と同じ挙動 |
| 7 | 公開読み取り契約 | architecture §1.2 / §5.3 | `GameState` の構成（§2.3）、`internal` を描画・音・app が読まないこと、ヘッダの分割（§2.1） |
| 8 | Component の追加フィールド | architecture §5.2 | §2.6 の太字のフィールド |
| 9 | フックの引数 | architecture §6.1 | 7 フックは引数を `const` の構造体 1 つで受け取る（例 `void (*on_hit)(const HitHookArgs *a)`、構造体に `GameState *g`、`lv` などを持つ）。個別のポインタ引数にすると、値を書き換えないフックで cppcheck の `constParameterCallback`（style）が出て `lint` が失敗する（cppcheck 2.18 で確認済み） |
| 10 | 表示文字列 ID | architecture §1.2 / §6.3 | 定義テーブルは `text_id` を持たない。`ink_render` が内容 ID（`EnemyDefId`、`UpgradeId` など）から文字列表を引く |
| 11 | イベントと入力の検証 | architecture §7.1 / §7.2 | 追加の 3 種と `EV_TYPE_COUNT`、§2.5 のペイロード表、非有限の座標と範囲外のコマンド種別の扱い |
| 12 | 状態ハッシュ | architecture §2.3 | `game_hash_sections` の 7 セクションと畳み込み順 |
| 13 | 音・描画 API | architecture §9.1 / §9.2 | `BgmTrack` に `BGM_NONE`、PCM は app が必要な分だけ `audio_render` で取り出す。`render_draw` が `const UiView *` を受け取る |
| 14 | CLI と障害の注入 | qa-and-testing §4 / §5.1、art-direction §14.2 | §2.9 の CLI の表と `--showcase`（見本帳）。シェーダ失敗経路とフォント欠損経路は、環境変数 `INKSTORM_DEBUG_FAULTS=shader` / `font`（併用は `shader,font`）で、本物の失敗と同じ分岐を通して確認する。あわせて、`assets/` のない場所へ実行ファイルだけを複製して起動する確認も行う |
| 15 | ラン中の終了 | game-design §5.1 / §10.1 | P0（ポーズ導入前）は `Esc` と閉じるボタンで終了する。ランの途中なら敗北として精算・保存してから終了する（ラン開始直後の幕転換中を除く） |
| 16 | 同梱フォントと分担 | game-design §11.4 / §13、PLAN §9.1 | Phase 3 で本文用の明朝体 1 書体（OFL 1.1、未改変）を `assets/fonts/` に同梱し、採用書体・版・入手元を `assets/LICENSES.txt` に記録する。Phase 4（P4-C）は題字用の毛筆体と外部 BGM を追加する |
| 17 | テスト一覧 | qa-and-testing §2 / §3 | `test_app_core`、`test_text_table`、`test_cli_usage_*`、`bench_game`（`game_step` 全体ベンチ） |
| 18 | デバッグ API と自動入力 | architecture §3（新しい小節） | §2.7 の表。通常のプレイ経路から呼ばないこと |
| 19 | 進め方 | PLAN §9.1 / §9.2 | §0.1 の段階と PR の単位。Phase 3 の行から本書と後続 3 本へリンク |

---

## 5. ローカル検証と完了条件

`CMakeLists.txt`・`tools/`・プロジェクト全体の公開ヘッダを変えるので、フルの ASan/UBSan + CTest と整形・静的解析を 1 回ローカルで実行する（inkstorm-verify-and-visual の「基盤を変更したとき」に当たる）。配布 zip の検証は CI の `dist-verify` に任せる。

```sh
# 1. ASan/UBSan: 全ターゲット（bench_game のコンパイルを含む）と CTest 14 件、整形・静的解析
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan &&
    nix develop -c ctest --preset asan --output-on-failure &&
    nix develop -c cmake --build --preset asan --target format-check lint

# 2. Release: main.c を変えたので CLI と実ウィンドウのスモーク
nix develop -c cmake --preset release && nix develop -c cmake --build --preset release &&
    ./build/release/inkstorm --headless-smoke &&
    nix develop -c ./build/release/inkstorm --smoke-frames 5 &&
    nix develop -c ./build/release/inkstorm --no-post --smoke-frames 5

# 3. 境界の確認（いずれも 0 件・空）
grep -rn 'raylib.h' src/core src/fluid src/ecs src/game src/audio src/app/save.* src/app/paths.* \
    src/app/cli.* src/app/frame_clock.* src/app/input_latch.* src/render/text_table.* src/render/ui_view.h
find src -name '*.h' -printf '%f\n' | sort | uniq -d
git diff --stat main -- src/fluid src/ecs src/game/meta.h src/game/meta.c
```

- 通常起動（`nix develop -c ./build/release/inkstorm`）で、ドラッグした所に墨が流れること（骨格の `game_step` が流体へ注入していること）を確認する。
- 静的解析で引っかかりやすい点: cppcheck の `variableScope`、`constParameterPointer`、`constParameterCallback`、`funcArgNamesDifferent`、`knownConditionTrueFalse`。clang-tidy の `DeadStores`、`bugprone-macro-parentheses`（`COMP_*` などのマクロは括弧で囲む）。ShellCheck（`expect_usage.sh`）。`lint` の cppcheck は P2-Int で `--check-level=exhaustive` 付きになっている（既定の解析上限では `fluid.c` / `solver.c` で終了コード 1 になるため）。cppcheck を手で実行するときも同じ引数を付ける。ローカル（21 / 2.18）と CI（18 / 2.13）で結果が違ったら CI を正とする。

**完了条件（Definition of Done）**
- [ ] §2 の凍結ヘッダ・`game_contract.c`・`cli.c`・`frame_clock.c` が本書の型・フィールド順・関数・意味のとおりで、ヘッダ名の重複がない。
- [ ] CTest 14 件が ASan/UBSan（`-Werror`）で通り、`format-check` と `lint` で指摘ゼロ。
- [ ] Release の `--headless-smoke`、`--smoke-frames 5`（通常・`--no-post`）が終了コード 0。
- [ ] §4 の 19 項目を設計書へ反映した。
- [ ] `fluid` / `ecs` / `meta` の公開 API に差分がない。
- [ ] PR 本文に、§0.1 の進め方、§1.2 の所有表、§4 の表、ローカル検証の結果、CI に任せた項目（`dist-verify`）を書いた。PR の作成・push はユーザーの許可を得てから、inkstorm-pr-and-coderabbit の手順で行う。
- [ ] `main` へのマージ後、P3-Pre のマージコミット SHA（P3-Int §2.1 で参照）を控えたうえで、P3-A（`git worktree add -b feat/phase3-sim-p0 ../wt-sim-p0 main`）と P3-B（`git worktree add -b feat/phase3-render-audio-p0 ../wt-render-audio-p0 main`）を切る。
