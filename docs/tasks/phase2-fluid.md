# Phase 2-A タスク指示書：流体エンジン `ink_fluid` の独立実装（`wt-fluid`）

> **必読**: [PLAN.md](../PLAN.md) §2.2 / §3.2 / §9、[architecture.md](../architecture.md) §2 / §4、[game-design.md](../game-design.md) §2 / §4.2 / §4.3 / §7.1、[qa-and-testing.md](../qa-and-testing.md) §1〜§3、[AGENTS.md](../../AGENTS.md)、[inkstorm-fluid-sim](../../.agents/skills/inkstorm-fluid-sim/SKILL.md)
>
> **worktree / ブランチ**: `full-scale-dev/wt-fluid` / `feat/phase2-fluid`（`main` から分岐）。このブランチ単独では `main` へ PR を出さない。
> **並列ペア**: P2-B（[phase2-ecs-meta.md](phase2-ecs-meta.md)）。統合は [phase2-integration.md](phase2-integration.md)（P2-Int）。

---

## 0. 目的と責務境界

160×90 の非圧縮 Stable Fluids ソルバを `ink_fluid` として完成させる。対象は渦度閉じ込め、Gauss-Seidel 圧力投影、セミラグランジュ移流、汎用 4ch スカラー、`solid` 障害物、壁を抜けない短歩幅レイマーチ、決定性ハッシュの各機能。品質・決定性は `test_fluid`（ASan/UBSan）で、性能予算 `< 2.0 ms/tick` は `bench_fluid`（Release）で実証する。

**禁止事項**
- ゲームルールを書かない。着火・伝導・爆雷・ダメージ・敵弾は `ink_game` の責務で、本モジュールではチャンネルを番号 `0..FLUID_MAX_CHANNELS-1` だけで扱う。
- `raylib.h` を include しない。依存先は `ink_core` と C11 標準ライブラリだけにする。
- `src/fluid/` では `malloc` / `free` と可変の `static` 変数（関数内 `static` の作業配列も含む）を使わない。作業領域はスタック上のローカル配列で持つ（`static const` の表は使ってよい）。
- 担当外のファイル（`src/core/*`、`src/ecs/*`、`src/game/*`、`src/audio/*`、`src/render/*`、`src/app/*`、`tests/test_core_smoke.c`）を編集しない。呼び出し側の同期は P2-Int が行う。
- `src/core/` を編集できないので、`INK_FIXED_DT` などの共通定数とハッシュ補助は使えない。テストとベンチでは `static const float k_dt = 1.0f / 60.0f;` を各自で定義し、FNV-1a は `fluid.c` 内の `static` 関数で実装する。

---

## 1. 変更許可ファイル

| ファイル | 操作 | 内容 |
| :--- | :---: | :--- |
| `src/fluid/fluid.h` | 更新 | 公開定数・構造体・API（§2.1） |
| `src/fluid/fluid.c` | 更新 | ソルバ本体（内部補助を分けるときは `src/fluid/*.c` を追加してよい。公開ヘッダは `fluid.h` だけ） |
| `tests/test_fluid.c` | 新規 | Unity 単体テスト（§3.1） |
| `tests/bench_fluid.c` | 新規 | Release ベンチ実行体（§3.2） |
| `CMakeLists.txt` | 追記のみ | 下記ブロックを指定位置に挿入する。既存行は変更しない |

**`CMakeLists.txt` の挿入位置**: `add_test(NAME test_core_smoke COMMAND test_core_smoke)` の**直後**に入れる。P2-B は `add_test(NAME test_cli_headless_smoke ...)` の直後に入れる。挿入位置が 1 行ずれていれば両ブランチは衝突なくマージできる（同じ位置に入れると衝突する。いずれも検証済み）。

```cmake

    add_executable(test_fluid tests/test_fluid.c)
    target_link_libraries(test_fluid PRIVATE unity ink_fluid ink_core m)
    add_test(NAME test_fluid COMMAND test_fluid)
    add_executable(bench_fluid tests/bench_fluid.c)
    target_link_libraries(bench_fluid PRIVATE ink_fluid ink_core m)
```

- `bench_fluid` は `add_test` に登録しない（ASan の CTest でベンチを走らせないため）。ただし CI の ASan ビルドは全ターゲットを作るので、`-Werror` で警告なくコンパイルできることは必須。
- `ink_fluid` のリンク行は変更しない。libm は `ink_core` の `PRIVATE m` から静的ライブラリ経由で伝播する（`libink_core.a libink_fluid.a libink_core.a -lm` の順になることを確認済み）。

> **ブランチ単独のビルド状態**: `fluid_init()` のシグネチャ変更と `fluid_step_smoke()` の廃止で、このブランチでは `ink_game`・`inkstorm`・`test_core_smoke` が P2-Int まで壊れたままになる（想定どおり）。ビルドは `--target test_fluid bench_fluid` に限定し、`all` と `lint` ターゲットは使わない（§4）。

---

## 2. 実装仕様

### 2.1 公開ヘッダ（`architecture.md` §2.2 / §4.1 / §4.2 と一字一句一致）
- 定数は `FLUID_W 160`、`FLUID_H 90`、`FLUID_N (FLUID_W * FLUID_H)`、`FLUID_CELL_PX 8.0f`、`FLUID_MAX_CHANNELS 4`。
- `FluidConfig`、`FluidStats`、`FluidState` は §4.1 のフィールド名・型・宣言順をそのまま使う。公開関数は §4.2 の 18 個で、追加も省略もしない。
- include は `<stdbool.h>` と `<stdint.h>` だけにする。`core.h` は include しない（`ink_fluid` → `ink_core` は `PRIVATE` リンクなので、利用側には core の include パスが伝播しない）。
- `fluid_config_default()` の既定値は次のとおり。

| フィールド | 既定値 | 根拠 |
| :--- | :--- | :--- |
| `project_iters` | `12` | architecture §4.1 |
| `vorticity_eps` / `vorticity_thresh` | `9.5f` / `0.35f` | architecture §4.1 |
| `max_vel` / `max_dye` | `125.0f` / `2.5f` | architecture §4.1、game-design §2 |
| `vel_damp` | `0.985f` | プロトタイプ `sim.c` |
| `dye_damp` | `{0.985f, 0.935f, 0.905f, 0.950f}` | game-design §7.1（ch3 は予備） |
| `active_channels` | `3` | architecture §4.1 |

### 2.2 座標系と単位（全 API 共通）
- **格子座標** `g = px / FLUID_CELL_PX`。セル `(x, y)` は `g ∈ [x, x+1) × [y, y+1)`、つまり画面の `[8x, 8x+8) × [8y, 8y+8)` px を占め、中心は `(8x+4, 8y+4)` px にある。点が属するセルは `(floorf(gx), floorf(gy))`。
- **中心座標** `s = g - 0.5f`（セル `(x, y)` の中心が `s = (x, y)` になる）。バックトレース、バイリニア補間、注入距離はすべて `s` で計算する。`s` 上の点が属するセルは `floor(s + 0.5f)` になる。
- この規約は描画テクスチャの texel（中心 `(x+0.5)*8`）と `fluid_is_solid()` の `floorf` 判定にそろえたもの。プロトタイプ `sim.c` は `px / 8` をそのまま中心座標として扱っており半セルずれているため、移植時に注意する。
- 速度 `u, v, fx, fy, strength, peak_speed` と `rad_cells` は格子単位（セル/s、セル）。**`fluid_sample()` もセル/s で返す**（プロトタイプは `× CELL_PX` して px/s で返していた。px/s への換算は `ink_game` 側で行う）。`dye` は無次元の `[0, max_dye]`。
- `-Wpedantic` と C11 では `M_PI` に頼らず、`static const float k_pi = 3.14159265f;` のようなローカル定数を使う。

### 2.3 入力検証・NULL 契約

| 対象 | 規則 |
| :--- | :--- |
| `f == NULL` | 全関数で何もしない。戻り値は `false` / `0` / `NULL`（`fluid_is_solid` と `fluid_solid_normal` も `false`、`fluid_hash` は `0`） |
| 注入系・`fluid_set_solid_*` の数値引数 | `NaN` / `±Inf` が 1 つでもあれば呼び出し全体を拒否し、状態を一切変えない。`dye_add` はアクティブチャンネル分の要素を検査する |
| 半径・幅高 | `rad_cells <= 0`、`radius_px <= 0`、`pw <= 0`、`ph <= 0` は拒否する。`rad_cells > FLUID_W + FLUID_H`（250）は 250 にクランプする（`r²` のオーバーフローで `Inf/Inf = NaN` が入るのを防ぐ） |
| 有限で範囲外の座標 | エラーにしない。注入は格子内部 `1..W-2 × 1..H-2` と交差するセルだけに適用する |
| float → int 変換 | 必ず float のまま有効範囲へクランプしてから変換する。`(int)floorf(1e30f)` は未定義動作 |
| `fluid_step(f, dt)` | `!isfinite(dt) \|\| dt <= 0.0f` なら何もしない（`steps` と `stats` も変えない）。dt に上限はない（§2.5 の終点クランプでループ長が有界になる） |
| `fluid_sample` | 座標が非有限なら、非 NULL の出力すべてに `0.0f` を書いて返す |
| `dye_add` / `out_dye` | `NULL` を許容する。非 NULL なら要素数 `FLUID_MAX_CHANNELS` 以上の配列を前提とし、`0..active_channels-1` を読み書きする。`out_dye` の非アクティブ要素には `0.0f` を書く |
| `out_u` / `out_v` / `nx` / `ny` | 個別に `NULL` を許容する |
| `fluid_channel_const/mut(f, ch)` | `0 <= ch < FLUID_MAX_CHANNELS` ならアクティブかどうかに関係なくポインタを返し、範囲外は `NULL` を返す。借用期限は次の `fluid_step` / `fluid_reset` まで |

### 2.4 初期化・リセット・`solid` 操作
- **`fluid_init(f, cfg)`**: `memset(f, 0, sizeof *f)` でゼロ化する。`*f = (FluidState){0}` は 0.88 MB の一時オブジェクトがスタックに作られうるので使わない。続いて `cfg` をコピーし（`NULL` なら既定値）、下表のとおりサニタイズしてから `fluid_clear_solids(f)` を呼ぶ。

| フィールド | サニタイズ |
| :--- | :--- |
| `project_iters` | `[0, 64]` にクランプ（0 は投影なし。§3.1 の壁抜けテストで使う） |
| `active_channels` | `[0, FLUID_MAX_CHANNELS]` にクランプ |
| `vorticity_eps`、`vorticity_thresh` | 非有限または `< 0` なら既定値 |
| `max_vel`、`max_dye` | 非有限または `<= 0` なら既定値 |
| `vel_damp`、`dye_damp[ch]` | 非有限なら既定値にしたうえで `[0, 1]` にクランプ |

- **`fluid_reset(f)`**: `u, v, u0, v0, div, p, curl, dye, dye0, stats` を 0 にし、`steps = 0` にする。`cfg` と `solid[]` は保持する。
- **`fluid_clear_solids(f)`**: 内部セルを `solid = 0`、外周 1 セル（`x == 0 || x == W-1 || y == 0 || y == H-1`）を `solid = 1` にし、外周の `u, v, dye` を 0 にする。
- **`fluid_set_solid_rect(f, px, py, pw, ph, solid)` / `fluid_set_solid_circle(f, px, py, radius_px, solid)`**:
  - 対象は**セル中心が図形の内側にある内部セル**とする。矩形は半開区間 `[px, px+pw) × [py, py+ph)`、円は中心からの距離 `<= radius_px` で判定する。8 の倍数にそろえた矩形はちょうど `pw/8 × ph/8` セルになる（例: `(640, 320, 16, 16)` はセル `(80..81, 40..41)` の 4 セル）。
  - 外周 1 セルは `solid == false` を渡されても `1` のまま維持する。
  - `solid = 1` にしたセルの `u, v` と全 4ch の `dye` は即座に `0.0f` にする。
- **`fluid_is_solid(f, px, py)`**: 座標が非有限、または `g` が `[0, W) × [0, H)` の外なら `true`（壁扱い）。それ以外は所属セルの `solid != 0` を返す。
- **`fluid_solid_normal(f, px, py, nx, ny)`**（流体側を向く単位法線）:
  1. 座標が非有限なら `false` を返す。
  2. 所属セル `c` を求め、格子範囲 `[0, W-1] × [0, H-1]` にクランプする。
  3. `n = Σ d`。和は 3×3 近傍（中心を除く 8 方向のオフセット `d`）のうち、範囲内かつ `solid == 0` のセルについてとる。範囲外のセルは壁として扱う。これは「壁から離れる向き」と等価で、整数演算なので決定的になる。
  4. `n == (0, 0)` かつ `c` 自体が `solid` のとき（壁の奥に埋まっているとき）は、5×5 近傍（中心を除く）で同じ和をとり直す。
  5. それでも `n == (0, 0)` なら `false` を返す（壁に接していない場合と、対称に囲まれた場合）。そうでなければ正規化して `true` を返す。`false` のときは非 NULL の `nx` / `ny` に `0.0f` を書く。

### 2.5 `fluid_step(f, dt)` の処理順（固定）
処理順はプロトタイプ `sim.c` の実績順を採用し、Phase 2 では本書を正とする（architecture §3.2 ステップ 4 の要約順とは異なる。設計書側の更新は別途扱う）。共通規則として、ループは y を外側・x を内側にして内部 `1..H-2 × 1..W-2` を昇順に回し、`solid` セルは更新せず 0 のまま保つ。添字は `i = x + y * FLUID_W` とし、隣接は `L = i-1`、`R = i+1`、`U = i-W`、`D = i+W`。

0. `clock_gettime(CLOCK_MONOTONIC, &t0)` で計測を始める。宣言は CMake と `tools/dist.mk` 共通の `-D_DEFAULT_SOURCE` で有効になる。ファイル内で機能マクロを定義しない。
1. **渦度閉じ込め**: まず全セルの `curl` を 0 にし、流体セルで `curl = 0.5f * ((v[R] - v[L]) - (u[D] - u[U]))` を計算する。`stats.max_vort` は `max |curl|`。次に別パスで、`|curl| >= vorticity_thresh` の流体セルについて `gx = 0.5f * (|curl[R]| - |curl[L]|)`、`gy = 0.5f * (|curl[D]| - |curl[U]|)`、`len = sqrtf(gx*gx + gy*gy) + 1e-4f` を求め、`u += eps * (gy/len) * curl * dt`、`v -= eps * (gx/len) * curl * dt` を加えて `±max_vel` にクランプする。
2. **圧力投影 1**（共通関数 `project`）:
   - 内部セルで `p = 0` にする（ウォームスタートしない）。流体セルで `div = -0.5f * (u[R] - u[L] + v[D] - v[U])`、`solid` セルは `div = 0`。
   - Gauss-Seidel を `project_iters` 回、その場（in-place）で昇順に回す。`p[i] = (div[i] + Σ_{流体隣接} p[j]) / n_i`（`n_i` は流体隣接数。`n_i == 0` なら `p = 0`）。これは Neumann 境界（`solid` 隣接の `p[j]` を `p[i]` で代用）を解いた形になる。
   - 勾配減算は `u -= 0.5f * (pR - pL)`、`v -= 0.5f * (pD - pU)`。`solid` 隣接の圧力は `p[i]` で代用し、結果を `±max_vel` にクランプする。
3. **速度移流**: `u0 ← u`、`v0 ← v` とする。流体セルごとに `q = B(x, y, u0[i], v0[i])` を求め、`u[i] = clamp(S(u0, q) * vel_damp)`、`v` も同様に更新する。
4. **圧力投影 2**: `project` をもう一度実行する。
5. **スカラー移流**（`ch < active_channels`）: `dye0[ch] ← dye[ch]` とする。流体セルごとに**ステップ 4 後の** `(u[i], v[i])` で `q = B(...)` と重みを 1 回だけ求め、全アクティブチャンネルで共有する。`dye[ch][i] = clamp(S(dye0[ch], q) * dye_damp[ch], 0, max_dye)`。非アクティブチャンネルは移流も減衰もしない。
6. **統計**: `stats.max_vel = max sqrtf(u² + v²)`（流体セル上の速さ。成分ごとのクランプ値 `cfg.max_vel` の最大 √2 倍になりうる）、`stats.max_dye[ch]`（非アクティブは 0）、`steps++`、`stats.solve_ms`（ms）を更新する。

**バックトレース `B(x, y, uu, vv)`（壁抜け防止レイマーチ）**
- 始点 `a = (x, y)`（中心座標）と生の終点 `(x - dt*uu, y - dt*vv)` を求め、終点を `[0.5, W-1.5] × [0.5, H-1.5]` にクランプして `e` とする。クランプによりステップ数は `2 * ceil(hypot(W, H))` 以下に収まり、追加の上限は要らない。
- `n = ceil(2 * |e - a|)` として（`ceilf` を使わず整数演算で求めてよい）、`k = 1..n` で点 `a + (e - a) * k / n` を順に調べる。**最終ステップは `e` そのもの**を使う（`k * (1/n)` の丸めで終点がずれるのを防ぐ）。ステップ長は 0.5 セル以下になる。
- 点の所属セル `floor(s + 0.5)` が `solid` ならその直前の点を返す（`k == 1` なら `a`）。最後まで `solid` に入らなければ `e` を返す。`n == 0` なら `a` を返す。

**サンプル `S(field, q)`（`solid` 除外バイリニア）**
- `q >= 0.5` なので `x0 = (int)qx` は `floor` と一致し、`floorf` は要らない。4 近傍の重みのうち `solid` セルの分を 0 にし、1 つでも 0 にしたときだけ残りの和で割って正規化する。
- `q` の所属セルは必ず流体で、その重みは 0.25 以上なので、分母が 0 になることはない。
- `solid` を含まないときは正規化しない（これで後述の高速パスと結果がビット一致する）。

この 2 つにより、厚さ 2 セル以上の障害物の反対側から速度・染料が漏れないことを保証する（§3.1 ケース 5）。

**性能予算 `< 2.0 ms/tick` に向けた実装方針**（指示書作成時に、リポジトリ外の参照実装を WSL2・ハイブリッドコア CPU・gcc 13.3 `-O2` で計測した値）

| 施策 | 参照計測 |
| :--- | :--- |
| 分岐を含む素朴な投影＋全セルレイマーチ | 平均 3.4 ms/tick（予算超過） |
| GS の分岐をなくす。`solid` セルの `p` は常に 0 なので、`p[i] = ((((div[i] + p[R]) + p[U]) + p[D]) + p[L]) * inv_n[i]` とする。`inv_n` は `fluid_step` の冒頭でスタック配列（`float inv_n[FLUID_N]`、`solid` は 0、流体は `1/n_i`）に 1 回だけ作る。`p[L]`（直前に更新した値）を最後に足して依存チェーンを短くする。勾配も `pR = p[R] + solid[R] * p[i]` のように分岐なしで書く | 投影 1 回 1.14 → 0.27 ms |
| バックトレースと重みはセルごとに 1 回だけ求め、`u`/`v`（または全チャンネル）で共有する。ステップごとの除算と `ceilf` を避ける | 移流 2 回で計約 1.0 ms |
| 高速パス: 変位² `<= 4` で、かつチェビシェフ距離 3 以内に `solid` が無いセル（`fluid_step` 冒頭でスタック上の `uint8_t near[FLUID_N]` に膨張マップを作る）は、レイマーチと `solid` 判定を省いて通常のバイリニアにする（結果は上記規則によりビット一致する） | 移流 → 約 0.63 ms、合計 平均 1.3 / p50 1.1 ms |
| ホットループのクランプは `ink_clampf`（別の翻訳単位にある関数の呼び出し）ではなく、`fluid.c` 内の `static inline` にする | — |

> **計測環境の注意**: 同じ環境では、全く同じ状態から同じステップを 600 回計測しても p95 が p50 の約 1.8 倍に膨らんだ（ホスト側のスケジューリング由来）。p95 < 2.0 ms を満たすには p50 ≈ 1.0 ms 以下を目標にする。ノート PC の場合は AC 電源につなぎ、Windows の電源モードを最高パフォーマンスにして計測する。

### 2.6 注入・サンプリング・浄化・ハッシュ
- 中心座標は `(cx, cy) = (px / 8 - 0.5f, py / 8 - 0.5f)`、距離は `dx = x - cx`、`dy = y - cy`、`d = sqrtf(dx² + dy²)`。セル範囲は float で `[1, W-2] × [1, H-2]` にクランプしてから整数化する。対象は流体セルだけ。
- **`fluid_splat`**: `d² <= r²` のセルに `w = expf(-d² / (0.55f * r²))` で重み付けし、`u += fx*w`、`v += fy*w`（`±max_vel` でクランプ）、`dye[ch] += dye_add[ch]*w`（`[0, max_dye]` でクランプ）を加える。
- **`fluid_swirl`**: `0.35f <= d <= r` のセルに `ring = sinf(k_pi * d / r)` で重み付けし、`u += (-dy/d) * strength * ring`、`v += (dx/d) * strength * ring` を加える。`strength > 0` は画面座標（y 下向き）で時計回りになる。`dye` には `ring²` の重みで加える。
- **`fluid_add_radial_impulse`**: `0.25f <= d <= r` のセルに `w = 1 - d / r` で重み付けし、`u += peak_speed * w * dx/d`、`v += peak_speed * w * dy/d` を加えてクランプする。染料は変えない。
- 負の `fx`、`strength`、`peak_speed`、`dye_add` は有効な入力とする（逆向きの注入・染料の除去）。結果は必ずクランプする。
- **`fluid_sample`**: `s` を `[0.5, W-1.5] × [0.5, H-1.5]` にクランプし、通常のバイリニア（`solid` は 0 の値として含める）で `u, v`（セル/s）と `dye`（アクティブ分）を返す。
- **`fluid_sanitize_channels`**: 全 4ch について、`solid` セルと非有限値を `0.0f` にし、それ以外を `[0, max_dye]` にクランプする。
- **`fluid_validate_finite`**: `u, v, u0, v0, div, p, curl, dye, dye0` がすべて有限であり、かつ `solid` セルの `u, v`、全 4ch の `dye` が `0.0f` であれば `true` を返す。
- **`fluid_hash`**: FNV-1a 64bit（offset basis `0xcbf29ce484222325`、prime `0x100000001b3`）で計算する。各値を下位バイトから 1 バイトずつ投入し、順序は次のとおり。
  1. `steps`（u64）
  2. `cfg` の各フィールドを宣言順に（`int` は `(uint32_t)`、`float` はビット列）
  3. `solid[0..N-1]`（各 1 バイト）
  4. `u[]`、`v[]`
  5. 全 4ch の `dye[ch][]`。非アクティブ ch も `fluid_channel_mut` で書き換えられるので含める

  float は `-0.0f` を `+0.0f` に正規化してから `memcpy` で `uint32_t` にする。`stats`、作業配列（`u0, v0, div, p, curl, dye0`）、パディングは対象外にする。

---

## 3. テスト・ベンチ要件

### 3.1 `tests/test_fluid.c`（Unity、ASan/UBSan で全通過）
- 各ケースは `calloc(1, sizeof(FluidState))` で確保し（0.88 MB をスタックに置かない）、`fluid_init` から始めて最後に `free` する。`dt` はローカルの `k_dt = 1.0f / 60.0f` を使う。
- この Unity 構成は `UNITY_INCLUDE_DOUBLE` が未定義なので、`double` 用の比較マクロは使えない（`solve_ms` は `TEST_ASSERT_TRUE` で判定する）。

| # | ケース | 判定 |
| :---: | :--- | :--- |
| 1 | 初期化・既定値・サニタイズ・リセット | 既定値が §2.1 の表と一致する。`u, v, dye` は 0 で、外周だけ `solid == 1`。`cfg == NULL` で既定値になる。不正な `cfg`（`vorticity_eps = NAN`、`project_iters = 1000`、`active_channels = 9`、`max_vel = -1`、`vel_damp = 2`）は §2.4 どおりに直る。`fluid_reset` 後に `solid` と `cfg` が残り、`steps == 0` になる |
| 2 | 安定性・クランプ | 中心、4 辺、4 隅（例 `(4, 4)`、`(1276, 716)`）、`fx = ±1e6` の強い入力を 300 ticks 連続で注入する。毎ティック `fluid_validate_finite == true`、全セルで `\|u\|, \|v\| <= max_vel`、`dye ∈ [0, max_dye]` |
| 3 | 発散の低下 | 既定設定で `fluid_add_radial_impulse(f, 640, 360, 8, 100)` を加える。テスト内で同じ離散式（流体セルの `0.5 * (u[R]-u[L]+v[D]-v[U])`）から `\|div\|` の平均と最大を求め、1 回の `fluid_step` 後に平均が 0.5 倍以下、最大が 0.25 倍以下になる（参照計測では 0.27 倍と 0.07 倍） |
| 4 | `solid` 境界・法線 | 矩形・円の内部は注入後もステップ後も `u = v = dye = 0`。`fluid_set_solid_rect(f, 0, 0, 1280, 720, false)` の後も外周は 1 のまま。`(640, 320, 16, 16)` がちょうど 4 セルになる。`fluid_is_solid` は `(644, 324)` で true、`(636, 324)` で false、範囲外・NaN で true。法線は下表のとおり |
| 5 | 薄い壁・角を抜けない | 下記 (a)(b)。`project_iters = 0`、`vorticity_eps = 0`、**`dt = 0.05f`** を使う |
| 6 | 注入の向き | `fluid_splat` の `fx > 0` で中心の `u > 0`。`fluid_swirl` の `strength > 0` で中心の右側が `v > 0`（時計回り）。放射インパルスで右側が `u > 0`、左側が `u < 0`。`peak_speed = 1e6` でも `\|u\| <= max_vel`。半径内の `solid` セルは 0 のまま |
| 7 | 不正入力の拒否 | 各数値引数への `NAN`、`±INFINITY`、`rad <= 0`、`pw/ph <= 0`、`NaN` を含む `dye_add` で `fluid_hash` が変わらない。`±1e30f` の座標と `rad_cells = 1e30f` でも NaN が入らない。全 API への `f = NULL`、`NULL` の `dye_add/out_*/cfg/nx/ny` でクラッシュしない。`dt ∈ {0, -1, NAN, INFINITY}` で `steps` とハッシュが不変。`ch = -1, 4` で `NULL` |
| 8 | 直列 2 フェーズ連携 | `fluid_channel_mut(f, 0)` で `NAN`、`-1`、`99`、`solid` セルへの値を書くと `fluid_validate_finite == false`。`fluid_sanitize_channels` 後は true になり、各値が 0、0、`max_dye`、0 になる |
| 9 | 単位の契約 | 全流体セルに `u = 3.0f` を直接書き、`fluid_sample(f, 640, 360, ...)` が `3.0f`（24 ではない）を返す。`active_channels = 3` のとき `out_dye[3] == 0.0f` |
| 10 | 決定性 | 2 つの独立したインスタンスに同じ設定・同じ §3.2 の障害物・同じ 120 ticks の入力（splat + swirl + radial）を与え、毎ティックの `fluid_hash` が一致する。`stats.solve_ms` を書き換えてもハッシュは変わらない。1 セルの `u` を `1e-3f` 変えるとハッシュが変わる |

**ケース 4 の法線**（2×2 柱 `(640, 320, 16, 16)` と外周。判定は `|n| ∈ [0.999, 1.001]` と期待方向との内積）

| 問い合わせ点（px） | 位置 | 期待方向 | 内積 |
| :--- | :--- | :--- | :---: |
| `(636, 324)` / `(660, 332)` / `(644, 316)` / `(652, 340)` | 柱の左 / 右 / 上 / 下の面 | `(-1,0)` / `(1,0)` / `(0,-1)` / `(0,1)` | `>= 0.7` |
| `(636, 316)` | 柱の左上の角 | `(-0.707, -0.707)` | `>= 0.99` |
| `(644, 324)` | 柱の内部（埋没） | `(-0.707, -0.707)` | `>= 0.99` |
| `(12, 364)` / `(4, 364)` | 外周の左辺の内側 / 外周セル上 | `(1, 0)` | `>= 0.99` |
| `(12, 12)` | 外周の左上の隅 | `(0.707, 0.707)` | `>= 0.99` |
| `(320, 200)` | 開けた場所 | `false`、`n = (0, 0)` | — |

**ケース 5 の構成**（終点だけをサンプルする素朴な実装は `dt = 1/60` だと壁際の速度が自己移流で 0 になり漏れないため、`dt = 0.05f`（最大 6.25 セル/step）で判別する。参照計測では素朴な実装が 1 ステップ目で漏れ、§2.5 の実装は漏れなかった）
- **(a) 全高の 2 セル壁**: `fluid_set_solid_rect(f, 640, 0, 16, 720, true)`（セル 80..81、全高）を置き、`x <= 79` の流体セルに `dye[0] = 2.0f` を入れる。各ステップの前に**全流体セル**へ `u = +125, v = 0` を書き込み、10 ステップ進める。毎ステップ、`x >= 82` のすべての `dye[0]` が `0.0f` に完全一致する。
- **(b) 厚さ 2 セルの閉じた箱（角の回帰）**: 次の 4 本の矩形でセル `60..79 × 30..49` の外枠を作る: `(480, 240, 160, 16)`、`(480, 384, 160, 16)`、`(480, 240, 16, 160)`、`(624, 240, 16, 160)`。箱の外の流体セルに `dye[0] = 2.0f` を入れ、各ステップの前に全流体セルへ `u = v = +125` を書き込んで 10 ステップ進める。`u = v = -125` でも同じことを行う。どちらでも、内部 `62..77 × 32..47` の `dye[0]` が `0.0f` のままになる。

### 3.2 `tests/bench_fluid.c`（Release 性能ベンチ。qa-and-testing §3 準拠）
- **設定**: `FluidState` は `calloc` で確保し、`fluid_init(f, NULL)` を使う（160×90、`project_iters = 12`、`active_channels = 3`）。
- **障害物**（game-design §4.3 の第 2 幕相当。社は `(640, 360)`）:
  - 鳥居柱 4 本: `fluid_set_solid_rect` で `(372, 320, 16, 16)`、`(372, 384, 16, 16)`、`(892, 320, 16, 16)`、`(892, 384, 16, 16)`（2×2 セル、柱の間は 6 セル）。
  - 岩柱 4 本: `fluid_set_solid_circle` で `(340, 190)`、`(940, 190)`、`(340, 530)`、`(940, 530)`、半径 24 px。
- **毎ティックの入力**（乱数は使わず、ティック番号 `t`（ウォームアップを含めて 0 から）と `a = 0.05f * t` で決める。記録上のシードは `none`）:

| 条件 | 呼び出し |
| :--- | :--- |
| 毎ティック | `fluid_splat(f, 640 + 380cos(a), 360 + 220sin(1.3a), 4.5, -110sin(a), 90cos(1.3a), {1.2, 0, 0, 0})` |
| `t % 7 == 0` | `fluid_splat(f, 640 + 300sin(2a), 200, 3.0, 0, 60, {0.4, 0.8, 0, 0})` |
| `t % 20 == 0` | `fluid_swirl(f, 640 + 200cos(0.7a), 360, 9.0, 70, {0.3, 0, 0.7, 0})` |
| `t % 45 == 0` | `fluid_add_radial_impulse(f, 640 - 250cos(a), 360 + 150sin(a), 10.0, 110)` |

- **計測**: ウォームアップ 120 ticks の後に 600 ticks を計測し、各ティックの `stats.solve_ms` を集める。これを毎回 `fluid_init` からやり直して **5 試行**する。分位点は昇順ソート後の nearest-rank（p50 = `[299]`、p95 = `[569]`）で求める。
- **ノイズの下限**: 最終状態を別の `calloc` 領域へ退避し、復元（計測の外）→ 1 ステップ（計測）を 600 回繰り返して p50 / p95 を出す。
- **出力**（標準出力）: 条件行、試行ごとの `mean / p50 / p95 / max`、最終 `fluid_hash`、`fluid_validate_finite`、全試行の中央値（mean と p95 は別々に）、`budget(<2.0ms mean & p95): PASS|FAIL`、ノイズ下限。
- **終了コード**: 試行間でハッシュが一致しない場合、または `fluid_validate_finite == false` の場合だけ非 0 にする。性能の判定は出力行で行い、終了コードには反映しない。
- **合否**: 試行中央値の mean と p95 がともに `< 2.0 ms`。mean は満たすが p95 だけがノイズ下限の膨張で超える場合も、しきい値を緩めずに実測値を添えてユーザーに判断を仰ぐ。

---

## 4. ローカル検証と完了条件

```sh
# 1. ASan/UBSan: test_fluid を実行し、bench_fluid は -Werror でのコンパイルだけ確認する
nix develop -c cmake --preset asan &&
    nix develop -c cmake --build --preset asan --target test_fluid bench_fluid &&
    nix develop -c ctest --preset asan -R '^test_fluid$' --output-on-failure

# 2. Release ベンチ（CI では代替できない性能ゲート）
nix develop -c cmake --preset release &&
    nix develop -c cmake --build --preset release --target bench_fluid &&
    ./build/release/bench_fluid

# 3. 自分のファイルだけ整形・静的解析する（このブランチは統合 PR まで CI が走らず、
#    lint ターゲットは未同期の game.c で失敗するため）
nix develop -c clang-format --dry-run --Werror src/fluid/*.[ch] tests/test_fluid.c tests/bench_fluid.c
nix develop -c clang-tidy -p build/asan src/fluid/*.c
nix develop -c cppcheck --enable=warning,style,performance,portability --error-exitcode=1 \
    --project=build/asan/compile_commands.json "--file-filter=$PWD/src/fluid/*"
```

**静的解析で引っかかりやすい点**
- cppcheck の style チェック: `variableScope`（変数は最小のスコープで宣言する）、`constParameterPointer`、`funcArgNamesDifferent`（宣言と定義で引数名をそろえる）、`knownConditionTrueFalse`。
- clang-tidy: `clang-analyzer-deadcode.DeadStores`、`bugprone-macro-parentheses`（`#define IX(x, y) ((x) + (y) * FLUID_W)` のように括弧で囲む）。
- ローカルのツール（clang-format/clang-tidy 21、cppcheck 2.18）と CI（Ubuntu 24.04: 18、2.13）は版が違う。差が出たら CI の結果を正とする。

**完了条件（Definition of Done）**
- [ ] `fluid.h` / `fluid.c` が architecture §4 の型・18 関数と本書 §2 の契約を満たし、ゲーム固有のルール、`raylib.h`、`malloc`、可変の `static` を含まない。
- [ ] `test_fluid` が ASan/UBSan（`-Werror`）で全 10 ケース通過する。
- [ ] `bench_fluid` が Release で 5 試行のハッシュ一致と有限性を満たし、試行中央値の mean と p95 がともに `< 2.0 ms/tick` になる。完了報告に測定環境（CPU / OS / コンパイラ）、全試行の値、ノイズ下限を記載する。
- [ ] 手順 3 の整形・静的解析で指摘ゼロ。
- [ ] 変更許可ファイル以外に差分がない（`git diff --stat main` で確認する）。
