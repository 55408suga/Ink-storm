# Phase 2-B タスク指示書：Hybrid ECS（`ink_ecs`）とメタ進行の純粋ロジック（`src/game/meta.*`）（`wt-ecs-meta`）

> **必読**: [PLAN.md](../PLAN.md) §2.3 / §3.2 / §9、[architecture.md](../architecture.md) §2 / §5.1 / §8、[game-design.md](../game-design.md) §5.6 / §9.5 / §10 / §11.2 / §11.3、[qa-and-testing.md](../qa-and-testing.md) §1 / §2、[AGENTS.md](../../AGENTS.md)
>
> **worktree / ブランチ**: `full-scale-dev/wt-ecs-meta` / `feat/phase2-ecs-meta`（`main` から分岐）。このブランチ単独では `main` へ PR を出さない。
> **並列ペア**: P2-A（[phase2-fluid.md](phase2-fluid.md)）。統合は [phase2-integration.md](phase2-integration.md)（P2-Int）。

---

## 0. 目的と責務境界

1. ゲーム固有の型を知らないインデックスレジストリ型の **Hybrid ECS（`ink_ecs`）** を完成させる。
2. 墨庵の購入、墨晶の精算、設定、操作ヒント、`RunConfig` の生成、`key=value` 文字列の変換を担う **メタ進行の純粋ロジック（`src/game/meta.h` / `meta.c`）** を完成させる。
3. `tests/test_ecs.c` と `tests/test_roguelike_meta.c`（メタ部）で、境界ケース・飽和演算・破損耐性を ASan/UBSan のもとで検証する。

**禁止事項**
- `src/ecs/` にゲーム固有の Component 型や `raylib.h` を持ち込まない。依存先は `ink_core` と C11 標準ライブラリだけにする。
- `meta.c` ではファイル I/O・環境変数・時刻・乱数（`fopen`、`rename`、`getenv`、`time`、`rand`）を使わない。保存先の解決と原子的な保存は Phase 3 の `src/app/save.c` の責務。
- `meta.h` は `game.h` / `fluid.h` / `ecs.h` を include しない。Phase 3 の P3-Pre で `game.h` 側から `meta.h` を include する想定。
- 担当外のファイル（`src/fluid/*`、`src/game/game.h`、`src/game/game.c`、`src/audio/*`、`src/render/*`、`src/app/*`、`src/core/*`、`tests/test_core_smoke.c`）を編集しない。`GameState` を受け取る `meta_settle_run` ラッパ（architecture §8.1）は P3-Pre で追加する。
- `malloc` / `free` と可変のグローバル状態を使わない（`static const` の価格表は可）。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/ecs/ecs.h` / `src/ecs/ecs.c` | 更新 | §2.1 |
| `src/game/meta.h` / `src/game/meta.c` | 新規 | §2.2（`ink_game` の `GLOB_RECURSE CONFIGURE_DEPENDS` で自動収録される） |
| `tests/test_ecs.c` / `tests/test_roguelike_meta.c` | 新規 | §3 |
| `CMakeLists.txt` | 追記のみ | 下記ブロックを指定位置に挿入する。既存行は変更しない |

**`CMakeLists.txt` の挿入位置**: `add_test(NAME test_cli_headless_smoke COMMAND inkstorm --headless-smoke)` の**直後**（`endif()` の前）に入れる。P2-A は `add_test(NAME test_core_smoke ...)` の直後に入れるので、マージで衝突しない。

```cmake

    add_executable(test_ecs tests/test_ecs.c)
    target_link_libraries(test_ecs PRIVATE unity ink_ecs ink_core)
    add_test(NAME test_ecs COMMAND test_ecs)
    add_executable(test_roguelike_meta tests/test_roguelike_meta.c)
    target_link_libraries(test_roguelike_meta PRIVATE unity ink_game ink_core)
    add_test(NAME test_roguelike_meta COMMAND test_roguelike_meta)
```

> **ブランチ単独のビルド状態**: `ecs_init` / `ecs_alive_count` のシグネチャは互換のまま保つ。このブランチは全ターゲット（`render.c` が使う `ecs_alive_count` を含む）がそのままビルドできる状態を保つ。既存の `ecs.c` は引数名が `world` なので、新しいヘッダの引数名（例 `w`）に宣言と定義の両方でそろえる（cppcheck `funcArgNamesDifferent` 対策）。

---

## 2. 実装仕様

### 2.1 Hybrid ECS（architecture §2.2 / §5.1 準拠）
- `ecs.h` は `<stdbool.h>` と `<stdint.h>` を include し、`ECS_MAX_ENTITIES 512` と `ECS_INVALID_INDEX UINT32_MAX` を定義する。`EntityId` と `EcsWorld` は §5.1 のフィールド名・型・宣言順をそのまま使う（`EcsWorld` は約 9 KB）。
- 公開関数は §5.1 の 9 個に、走査補助の `EntityId ecs_entity_at(const EcsWorld *w, uint32_t index);` を加える（§5.1 への追加分）。
- **不変条件**: `gen[i] >= 1`（世代 0 は無効なので、ゼロ初期化した `EntityId` が誤って一致しない）。`alive_count` は `ecs_is_alive` が真になる数と等しい。`free_count` と「`alive` または `pending_destroy` のスロット数」の和は `ECS_MAX_ENTITIES`。

| 関数 | 仕様（`w == NULL` は全関数で何もしない。戻り値は `false` / `0` / 無効 ID） |
| :--- | :--- |
| `ecs_init` | `memset` で 0 にし、全スロットを `gen = 1` にする。`free_list[k] = ECS_MAX_ENTITIES - 1 - k`（降順）、`free_count = ECS_MAX_ENTITIES` とする。これで初回のスポーンは 0, 1, 2, … の順になる |
| `ecs_spawn` | `free_count == 0` なら `(EntityId){ECS_INVALID_INDEX, 0}` を返し、状態を変えない。それ以外は `idx = free_list[--free_count]` とし、`alive = 1`、`pending_destroy = 0`、`comp_mask = 0`、`alive_count++`、`high_water = max(high_water, idx + 1)` として `{idx, gen[idx]}` を返す |
| `ecs_is_alive` | `id.index < ECS_MAX_ENTITIES && alive && !pending_destroy && gen == id.gen` |
| `ecs_destroy` | `ecs_is_alive` が真のときだけ `pending_destroy = 1`、`alive_count--` とする。2 回目の呼び出しや旧世代の ID では何もしない。削除予約中のスロットは `ecs_flush_destroy` まで `free_list` に戻さない |
| `ecs_flush_destroy` | `i = high_water - 1` から 0 へ**降順**に走査し、予約中のスロットを `alive = 0`、`pending_destroy = 0`、`comp_mask = 0`、`gen++`（0 に回り込んだら 1）にして `free_list[free_count++] = i` で戻す。最後に `while (high_water > 0 && !alive[high_water - 1]) high_water--;` で縮める。降順に積むので、同じ flush で空いた番号は小さい順に再利用される。ループは `for (uint32_t i = w->high_water; i-- > 0;)` と書く（`i >= 0` は符号なしでは常に真になり、`-Wtype-limits` と `-Werror` で失敗する） |
| `ecs_add_mask` / `ecs_remove_mask` | 生存していなければ `false`。生存していれば `comp_mask \|= mask` / `&= ~mask` として `true`（`mask == 0` でも `true`） |
| `ecs_has_mask` | 生存していて `(comp_mask & mask) == mask` なら `true`（`mask == 0` は生存していれば `true`） |
| `ecs_alive_count` | `alive_count` |
| `ecs_entity_at` | `index < high_water && alive && !pending_destroy` なら `{index, gen[index]}`、それ以外は無効 ID |

- **決定的な走査の書き方**: `for (uint32_t i = 0; i < w->high_water; ++i) { const EntityId id = ecs_entity_at(w, i); if (id.index == ECS_INVALID_INDEX) continue; … }`。走査中にスポーンしたエンティティを同じティックで処理するかどうか（ループ上限を固定するか毎回読むか）は Phase 3 のシステム側で決める。

### 2.2 メタ進行（architecture §8.1 / §8.2、game-design §10 / §11.2 / §11.3 準拠）

#### 定数・型
```c
#define META_SAVE_VERSION       1
#define META_MAX_CRYSTALS       9999999
#define META_MAX_SCORE          999999999
#define META_MAX_COUNTERS       9999999
#define ENDLESS_MAX_LOOP_N      100
#define META_MAX_ENDLESS_ACTS   (ENDLESS_MAX_LOOP_N * 3)
#define META_MAX_BEST_ACT       (3 + META_MAX_ENDLESS_ACTS) /* 本編 3 幕 + エンドレス最大 300 幕 */
#define META_RELIC_UNLOCK_COUNT 6                           /* 墨庵で解放する大遺物（bit 0..5） */
#define META_HINT_BITS          32
#define META_SERIALIZE_CAP      512 /* 全 18 キーと NUL が必ず収まる（全項目が最長の値でも 366 バイト） */
```
- `MetaUpgradeKind`、`SettingsState`、`MetaState` は architecture §8.1 のフィールド名・型・宣言順をそのまま使う。
- `meta.h` は `<stdbool.h>`、`<stddef.h>`、`<stdint.h>` を include する。追加する型は次のとおり。

```c
typedef enum {
    BRUSH_STYLE_HEAVY = 0, /* 豪筆 */
    BRUSH_STYLE_EMBER,     /* 朱筆 */
    BRUSH_STYLE_QUILL,     /* 細筆 */
    BRUSH_STYLE_COUNT
} BrushStyleId;

typedef struct {
    uint64_t seed;
    BrushStyleId brush_style;
    uint8_t meta_upgrade_lv[META_UPG_COUNT];
    uint64_t unlocked_relic_mask; /* bit 0..META_RELIC_UNLOCK_COUNT-1 */
    int32_t endless_loop;         /* 0 = 本編、n >= 1 = エンドレス周回 n（game-design §3.6） */
} RunConfig;

typedef struct {
    int32_t acts_cleared;         /* 本編でクリアした幕数 0..3（エンドレス分は含めない） */
    int32_t endless_acts_cleared; /* エンドレスでクリアした幕数 */
    int32_t reached_act;          /* 到達した幕番号（best_act の更新用、第 4 幕以降はエンドレス） */
    int32_t kills;
    int32_t max_chain;            /* 最大の一筆連殺のチェイン数 */
    int32_t thunderbursts;
    int32_t cataclysms;
    int32_t score;
    bool cleared;                 /* 第 3 幕をクリアした */
    bool crystal_bonus_x15;       /* 大遺物「百鬼夜行の巻物」（game-design §9.5） */
} MetaRunResult;

typedef struct {
    int32_t from_acts, from_kills, from_chain, from_reactions, from_endless;
    int32_t from_bonus;     /* ×1.5 による加算分（リザルトの内訳表示用） */
    int32_t total_crystals; /* [0, META_MAX_CRYSTALS] */
} MetaSettleBreakdown;
```

#### 公開 API
```c
void meta_init_default(MetaState *meta);
SettingsState meta_settings_default(void);

uint8_t meta_upgrade_max_level(MetaUpgradeKind kind);                        /* 不正な kind は 0 */
int32_t meta_upgrade_next_cost(const MetaState *meta, MetaUpgradeKind kind); /* 最大段階・不正は -1 */
bool meta_buy_upgrade(MetaState *meta, MetaUpgradeKind kind);
int32_t meta_relic_unlock_cost(uint32_t slot); /* slot < META_RELIC_UNLOCK_COUNT なら 100、それ以外は -1 */
bool meta_is_relic_unlocked(const MetaState *meta, uint32_t slot);
bool meta_buy_relic_unlock(MetaState *meta, uint32_t slot);

MetaSettleBreakdown meta_calc_crystals(const MetaRunResult *res); /* res == NULL は全 0 */
bool meta_settle_result(MetaState *meta, const MetaRunResult *res, bool *run_settled_flag,
                        MetaSettleBreakdown *out_breakdown);

RunConfig meta_build_run_config(const MetaState *meta, BrushStyleId style, uint64_t seed);
RunConfig meta_run_config_preset(BrushStyleId style, uint64_t seed);

bool meta_is_hint_seen(const MetaState *meta, uint32_t hint_bit);
void meta_mark_hint_seen(MetaState *meta, uint32_t hint_bit);

size_t meta_serialize(const MetaState *meta, char *buf, size_t buf_size);
bool meta_deserialize(MetaState *meta, const char *text);
```
- `MetaState *` を受け取る関数は、`meta == NULL` のとき何もせず `false` / `-1` / `0` を返す。

#### ビジネスルール
1. **既定値**: `version = META_SAVE_VERSION` で、`ink_crystals`、`upgrade_lv`、`unlocked_relic_mask`、`best_score`、`best_act`、`runs_played`、`clears`、`seen_hints_mask` はすべて 0。`settings` は `{80, 70, 85, 100, true, true}`（master / bgm / se / shake / post_shader / hints）。
2. **墨庵**（game-design §10.2。全部買うと合計 1,920 墨晶）:

| 種別 | 最大段階 | 価格（現在の段階 0, 1, 2 から次へ） |
| :--- | :---: | :--- |
| `META_UPG_SHRINE_HP` | 3 | 80 / 140 / 200 |
| `META_UPG_INK_MAX` | 3 | 60 / 100 / 140 |
| `META_UPG_REROLLS` | 2 | 100 / 200 |
| `META_UPG_INK_REGEN` | 3 | 60 / 100 / 140 |
| 大遺物の解放（slot 0..5） | 各 1 回 | 各 100 |

   - 残高不足、最大段階、解放済み、不正な kind / slot はすべて `false` を返し、`meta` を一切変えない。
   - kind の範囲検査は `(unsigned)kind >= META_UPG_COUNT` で書く。GCC では列挙型の基底型が符号なしになり、`kind < 0` は `-Wextra`（`-Wtype-limits`）で常に偽の警告になり、`-Werror` で失敗する。
   - `1ULL << slot` と `1u << hint_bit` は、範囲検査を通ってから計算する（幅以上のシフトは未定義動作）。
3. **墨晶の計算**（game-design §10.1）。途中計算はすべて `int64_t` で行う（`max_chain * 2` なども `int32_t` では UB になりうる）。負の入力は 0 として扱う。

| 内訳 | 式 |
| :--- | :--- |
| `from_acts` | `acts_cleared` を `[0, 3]` にクランプし、0 / 1 / 2 / 3 → `0 / 30 / 70 / 120` |
| `from_kills` | `kills / 5`（`INT32_MAX` でも `429,496,729` で `int32_t` に収まる） |
| `from_chain` | `min(max_chain * 2, 30)` |
| `from_reactions` | `min(thunderbursts + cataclysms * 3, 20)` |
| `from_endless` | `endless_acts_cleared` を `[0, META_MAX_ENDLESS_ACTS]` にクランプして `× 40` |
| `from_bonus` | `crystal_bonus_x15` なら `base / 2`（**切り捨て**。`base` は上 5 項の和）、そうでなければ 0 |
| `total_crystals` | `base + from_bonus` を `[0, META_MAX_CRYSTALS]` に飽和させる |

4. **精算**（`meta_settle_result`）:
   - `meta`、`res`、`run_settled_flag` のいずれかが `NULL`、または `*run_settled_flag == true` なら、何も更新せず `false` を返す。このとき `out_breakdown` が非 NULL なら 0 にする。
   - 初回は `meta_calc_crystals` の結果で次を更新し、`*run_settled_flag = true` として `true` を返す。`out_breakdown` が非 NULL なら内訳を書く。更新はすべて `int64_t` 経由で飽和させる（`ink_crystals` が壊れて `INT32_MAX` や負値になっていても UB を起こさない）。
     - `ink_crystals = clamp(ink_crystals + total, 0, META_MAX_CRYSTALS)`
     - `best_score = max(best_score, clamp(score, 0, META_MAX_SCORE))`
     - `best_act = max(best_act, clamp(reached_act, 0, META_MAX_BEST_ACT))`
     - `runs_played = clamp(runs_played + 1, 0, META_MAX_COUNTERS)`
     - `cleared` のときは `clears = clamp(clears + 1, 0, META_MAX_COUNTERS)`
5. **`RunConfig`**: `meta_build_run_config` は `upgrade_lv` と `unlocked_relic_mask & 0x3F` をコピーし、`endless_loop = 0` にする。`meta == NULL` なら `meta_run_config_preset` と同じ値を返す。`meta_run_config_preset` は強化段階 0、解放マスク 0（初期の大遺物 9 種だけ）、`endless_loop = 0` を返し、`MetaState` を一切参照しない。`style >= BRUSH_STYLE_COUNT` は `BRUSH_STYLE_HEAVY` にする。
6. **操作ヒント**: `hint_bit < META_HINT_BITS` のときだけ判定・記録する。範囲外は `false`、何もしない。`HintId` の割り当ては Phase 3 で決める。
7. **`meta_serialize`**:
   - キー順は architecture §8.2 の例と同じ 18 行で、各行は `key=value\n`（LF のみ）。コメント行と空行は出力しない。値は範囲を検証せず、そのまま出力する。
   - 書式は次のとおり。`version` は `%" PRIu32 "`。`int32_t` の項目は `%" PRId32 "`。`uint8_t` の項目は `(unsigned)` にキャストして `%u`。`bool` は `0` / `1`。`unlocked_relic_mask` は `0x%016" PRIx64 "`、`seen_hints_mask` は `0x%08" PRIx32 "`（小文字の 16 進）。`<inttypes.h>` を使う。
   - 戻り値は `snprintf` と同じ意味で、NUL を除いた必要長を返す。`buf_size > 0` なら必ず NUL 終端し、呼び出し側は `ret < buf_size` で切り詰めの有無を判定する。`buf == NULL` は `buf_size = 0` として扱う。`meta == NULL` や `snprintf` が負を返した場合は 0 を返す（`buf` があれば空文字列にする）。
   - 18 項目を **1 回の `snprintf`** で書くと、範囲外へのポインタ演算（UB）が起きない。
8. **`meta_deserialize`**: `meta_init_default(&tmp)` で作業用の `tmp` を作って読み込み、成功したら `*meta = tmp` として `true` を返す。失敗したら `meta_init_default(meta)` として `false` を返す（途中まで読んだ値は残さない）。`meta == NULL` は `false` を返し、何も書かない。

| 項目 | 規則 |
| :--- | :--- |
| 行 | `\n` で区切り、行末の `\r` を除く。前後の空白・タブを除いた結果、空行または `#` で始まる行は無視する |
| 書式 | 最初の `=` で key と value に分け、それぞれの前後の空白・タブを除く。`<ctype.h>` を使う場合は `(unsigned char)` にキャストしてから渡す |
| **失敗**（既定値へ戻して `false`） | `text == NULL`、`version` の欠損、`version != 1`、`=` を含まない非空行、空の key、既知キーの値が空・数値として解釈できない・31 バイトを超える、マスク系キーの値が `-` で始まる |
| 未知のキー | 無視する（値は検査しない） |
| 重複したキー | 後に出たほうを採用する |
| 数値の解釈 | マスク系以外は `strtoll(…, 10)`（基数 0 だと `010` が 8 進数になるので使わない）で、値の全体を読み切ることを確認する。`ERANGE` のときは飽和した値のまま下のクランプに進める。マスク系は `0x` / `0X` で始まれば `strtoull(…, 16)`、それ以外は `strtoull(…, 10)` |
| クランプ | `ink_crystals` は `[0, META_MAX_CRYSTALS]`、`upg_*` は `[0, meta_upgrade_max_level]`、`best_score` は `[0, META_MAX_SCORE]`、`best_act` は `[0, META_MAX_BEST_ACT]`、`runs_played` / `clears` は `[0, META_MAX_COUNTERS]`、`opt_*_vol` は `[0, 100]` |
| 正規化 | `unlocked_relic_mask & 0x3F`、`seen_hints_mask` は下位 32 bit。`opt_shake_scale` は `< 25` → 0、`< 75` → 50、それ以外 → 100。`opt_post_shader` / `opt_hints` は 0 なら `false`、それ以外は `true` |

---

## 3. テスト要件（Unity、ASan/UBSan で全通過）

`EcsWorld`（約 9 KB）と `MetaState` はスタックに置いてよい。構造体の比較は `memcmp` ではなくフィールドごとに行う（`MetaState` はパディングを含み、構造体の代入ではパディングの中身が保証されない）。

### 3.1 `tests/test_ecs.c`
1. **初期状態**: `alive_count == 0`、`high_water == 0`、`free_count == 512`。`{0, 0}` と `{ECS_INVALID_INDEX, 0}` は全 API で拒否される。`w == NULL` を全 API に渡してもクラッシュしない。
2. **世代**: 最初のスポーンは `{0, 1}`。`destroy` → `flush` → `spawn` でスロット 0 が `gen == 2` で再利用され、旧 ID は `is_alive`、`add_mask`、`remove_mask`、`has_mask`、`destroy` のすべてで拒否される（`alive_count` は変わらない）。`w.gen[k] = UINT32_MAX` にしてから `destroy` → `flush` すると `gen == 1` になる。
3. **満杯**: 512 回のスポーンが 0..511 の昇順で成功し、513 回目は `{ECS_INVALID_INDEX, 0}` を返して `alive_count`、`free_count`、`high_water` が変わらない。1 体を `destroy` しても `flush` 前は満杯のままで、`flush` 後はその番号が再利用される。
4. **削除予約中**: `destroy` 直後（`flush` 前）に、`is_alive`、`add_mask`、`remove_mask`、`has_mask` がいずれも `false` になり、`ecs_entity_at` は無効 ID を返し、`alive_count` は 1 減る。2 回目の `destroy` は何もしない。
5. **マスク**: 複数ビットの設定、一部の解除、複合判定、`1ULL << 63`、`mask == 0` の意味を確認する。
6. **走査と再利用の順序**: 10 体をスポーンし、3 と 7 を `destroy` → `flush` → 3 回スポーンすると、番号は 3、7、10 の順になる。最上位の生存スロットを削除して `flush` すると `high_water` が縮み、全削除すると 0 になる。`0 .. high_water-1` の昇順走査で、生存しているものだけが列挙される。

### 3.2 `tests/test_roguelike_meta.c`（メタ部）
1. **既定値と `RunConfig` の分離**: §2.2 規則 1 の値になる。`meta_build_run_config` は強化段階と解放マスクをコピーし、`seed` を保持する。`meta_run_config_preset` はメタを強化した後も 0 のままになる。不正な `style` は `HEAVY` になる。`meta == NULL` ならプリセットと一致する。
2. **墨庵**: 残高 1,920 から 4 種の全段階と大遺物 6 枠を買い切ると、残高がちょうど 0 になる（各段階の `next_cost` も確認する）。残高が `cost - 1` なら拒否され、状態が変わらない。最大段階では `next_cost == -1` で購入は `false`。解放済みの再購入、`(MetaUpgradeKind)META_UPG_COUNT`・`(MetaUpgradeKind)99`、`slot = 6`・`UINT32_MAX` はいずれも拒否される。`meta_upgrade_max_level` は `{3, 3, 2, 3}` で、不正な kind は 0。
3. **墨晶の計算**（テーブル駆動）:

| 入力（記載のないフィールドは 0） | 期待値 |
| :--- | :--- |
| 全 0 | 全内訳 0、`total = 0` |
| `acts = 1, kills = 74, max_chain = 8`（第 2 幕で敗北した想定） | `30 / 14 / 16 / 0 / 0`、`total = 60` |
| `acts = 3, kills = 152, max_chain = 12, thunderbursts = 9, cataclysms = 4`（初クリアの想定） | `120 / 30 / 24 / 20 / 0`、`total = 194` |
| `acts = 7, kills = -5, max_chain = 20, thunderbursts = 7, cataclysms = 5, endless = -1` | `120 / 0 / 30 / 20 / 0`、`total = 170` |
| `acts = 3, kills = 200, max_chain = 15, thunderbursts = 11, bonus = true` | `base = 201`、`from_bonus = 100`、`total = 301` |
| `acts = 3, endless = 2` | `from_endless = 80`、`total = 200` |
| 全フィールド `INT32_MAX`、`bonus = true` | UB なし、`from_endless = 12000`、`total = META_MAX_CRYSTALS` |

4. **飽和**: 次の 5 つで、いずれも符号付きオーバーフロー（UBSan の検出対象）を起こさない。
   - `ink_crystals = META_MAX_CRYSTALS - 10` に 194 を加えると `META_MAX_CRYSTALS` になる。
   - 壊れた値 `INT32_MAX` に加えると `META_MAX_CRYSTALS` になる。
   - 負の `-100` に 194 を加えると 94 になる。
   - `runs_played = META_MAX_COUNTERS` は精算後も変わらない。
   - `score = INT32_MAX` は `best_score = META_MAX_SCORE`、`reached_act = INT32_MAX` は `best_act = META_MAX_BEST_ACT` になる。
5. **二重精算の防止**: 1 回目は `true` で `*flag == true` になる。2 回目は `false` を返し、全フィールドが変わらず、`out_breakdown` は 0 になる。`NULL` 引数では `false` を返す。
6. **操作ヒント**: ビット 0 と 31 は記録・判定できる。32 と `UINT32_MAX` は何もせず、判定は `false`（マスクも変わらない）。
7. **シリアライズ**:
   - architecture §8.2 の例の値を入れた `MetaState` から生成した文字列が、例の 18 行（各行末に `\n`）と完全に一致する（ゴールデンテスト）。
   - 全キーに既定値以外を入れて往復させると、フィールドごとに一致する。
   - `meta_serialize(meta, NULL, 0)` の戻り値は実際の長さと等しい。
   - `buf_size = 16` では切り詰められて NUL 終端され、戻り値は全体の長さになる。
8. **破損耐性**（テーブル駆動）: 次のすべてを確認する。
   - 未知のキーは無視され、欠損したキーは既定値になる。
   - 範囲外の値はクランプされる（`ink_crystals` の負値と `99999999999`、`vol = 150`、`upg = 9`）。
   - `opt_shake_scale` は 24 / 25 / 74 / 75 / 200 / -5 がそれぞれ 0 / 50 / 50 / 100 / 100 / 0 になる。
   - マスクの上位ビットは捨てられる。`010` は 10 として読まれる。`\r\n` の改行、`#` のコメント、空行、キーの前後の空白を受け付ける。重複キーは後勝ちになる。
   - `version=2`、`version` の欠損、`ink_crystals=12abc`、`=` のない行、`unlocked_relic_mask=-1`、空文字列、`NULL` は、いずれも `false` を返して既定の `MetaState` になる。

---

## 4. ローカル検証と完了条件

このブランチは単独で全ターゲットをビルドでき、統合 PR まで CI が走らない。`ecs.h` を使う `game` / `render` / `app` も再ビルドが必要なため、全体のビルドと CTest（4 テスト）をローカルで実行する。

```sh
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan &&
    nix develop -c ctest --preset asan --output-on-failure
nix develop -c cmake --build --preset asan --target format-check lint
```

- cppcheck の style チェック（`variableScope`、`constParameterPointer`、`funcArgNamesDifferent`、`knownConditionTrueFalse`）と clang-tidy の `DeadStores`、`bugprone-macro-parentheses`（`META_MAX_BEST_ACT` などのマクロは括弧で囲む）に注意する。ローカルのツール（21 / 2.18）と CI（18 / 2.13）で結果が違ったら CI を正とする。

**完了条件（Definition of Done）**
- [ ] `ecs.h` / `ecs.c` が architecture §5.1 と本書 §2.1 を満たし、`ink_core` 以外に依存しない。
- [ ] `meta.h` / `meta.c` が architecture §8 と game-design §10 を満たし、ファイル I/O、`raylib.h`、`game.h` に依存しない。
- [ ] `test_ecs`、`test_roguelike_meta`、`test_core_smoke`、`test_cli_headless_smoke` が ASan/UBSan（`-Werror`）で全通過し、`format-check` と `lint` で指摘ゼロ。
- [ ] 変更許可ファイル以外に差分がない（`git diff --stat main` で確認する）。
