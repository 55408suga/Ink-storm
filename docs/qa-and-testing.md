# INKSTORM（墨嵐）品質保証・テスト・視覚回帰計画

> **位置づけ**: 本ドキュメントは INKSTORM（墨嵐）の自動テスト、性能ベンチマーク、CLI 契約、視覚回帰、デモ録画および配布ビルド検証の基準（[PLAN.md](PLAN.md) Section 8 の詳細版）を定義する。
> 技術契約は [architecture.md](architecture.md)、ゲーム仕様は [game-design.md](game-design.md)、水墨表現規約は [art-direction.md](art-direction.md)、日常の検証コマンド手順は [inkstorm-verify-and-visual](../.agents/skills/inkstorm-verify-and-visual/SKILL.md) を参照する。

---

## 1. ローカル検証 vs CI 検証の役割分担

| 区分 | 実施内容 | タイミング |
| :--- | :--- | :--- |
| **ローカル（最小・高速）** | 変更モジュールのターゲットビルド＋該当する Unity テストのみ | 毎実装・PR 作成前 |
| **ローカル固有（CI 代替不可）** | WSLg 実機 GPU 描画（通常 / `--no-post` / シェーダ失敗フォールバック / `--snap` 目視）、`flake.nix` 変更時の Nix 評価、Release 性能ベンチマーク | 描画・流体・基盤変更時 |
| **GitHub Actions CI（自動ゲート）** | `build-asan-and-test`（ASan/UBSan + 全 CTest）、`static-analysis`（`clang-format` / `clang-tidy` / `cppcheck` / GLSL / ShellCheck）、`dist-verify`（禁止ファイル検査 + 展開先素 `make` + `--headless-smoke`） | 全 PR / `main` push 時 |

- 現行 CI には Release 性能ベンチマークジョブを含めていないため、性能予算の確認はローカルの Release ビルドで行う。
- モジュール依存境界の強制は CMake ビルド（`build-asan-and-test`）が担い、`dist-verify` は配布ソース zip の自己完結性（素 `make` + `gcc` + raylib 6.2）を確認する。

---

## 2. 自動テストスイート（CTest / Unity）

すべての Unity テストは `asan` プリセット（`-Werror -fsanitize=address,undefined`）で通過することを必須とする。`GameState` はテスト内でもヒープ確保（`calloc` / `game_create()`）を用い、スタック溢れを防ぐ。

| テストターゲット | 導入 Phase | 検証対象と必須境界ケース |
| :--- | :---: | :--- |
| `test_core_smoke`（既存） | Phase 1（P2 でヒープ化、P3 で契約追加） | SplitMix64 RNG、クランプ、FNV-1a ハッシュ補助、初期化・中断・Component アクセサ・イベント上限の契約 |
| `test_cli_headless_smoke`（既存） | Phase 1（P3 で本番化） | `./inkstorm --headless-smoke` の正常終了。Phase 3 以降はスタブではなく本番 `game_step` を 60Hz で複数ティック通過させる |
| `test_cli_usage_*`（3 件） | Phase 3 | `tools/expect_usage.sh` による CLI Usage エラーの実行ファイル配線・終了コード 1・`Usage: ` 行の検証（未知オプション・セーブ排他・ヘッドレス排他） |
| `test_app_core` | Phase 3 | CLI オプション解析（全正常・全 Usage エラー・排他・重複・`PATH_MAX`・`INKSTORM_DEBUG_FAULTS`）、`FrameClock`（60Hz/144Hz/頭打ち/非有限入力）、`InputLatch` の規則 |
| `test_text_table` | Phase 3 | 文字列表の全エントリの非空・英語 ASCII 限定（0x20〜0x7E）・内容 ID 対応関数と範囲外検査 |
| `test_fluid` | Phase 2 | 静止場・中心/端/四隅/強入力連続での NaN・Inf ゼロ＆クランプ、圧力投影後の発散低下、`solid` no-slip 境界と法線クエリ、**薄い柱（2 セル幅）の壁横断防止＆角（コーナー）回帰**、`fluid_add_radial_impulse` の速度クランプ、非有限/範囲外入力の安全拒否、同一入力の `fluid_hash` 完全一致 |
| `test_ecs` | Phase 2 | `spawn`/`destroy` による世代インクリメント、無効/旧世代 `EntityId` 拒否、**満杯（512）時のスポーン失敗（`ECS_INVALID_INDEX`）**、**削除予約中エンティティへのアクセス拒否（`ecs_is_alive == false`）**、遅延削除フラッシュ後の再利用、昇順ビットマスク走査 |
| `test_roguelike_meta` | Phase 2（メタ）→ Phase 3/4（報酬） | `MetaState` の購入残高チェック、`int32_t` 上限付近での**飽和加算（符号付きオーバーフローなし）**、**精算の二重適用防止（`run_settled`）**、`key=value` シリアライズ往復・未知キー無視・破損入力デフォルト復帰・設定/ヒント保存；Phase 3/4 で `RunModifiers` 再計算・7 フック取得順実行・3 択/リロール/修繕・**候補不足（3 件未満/0 件）時の代替選択**・無効コマンド no-op を追加 |
| `test_app_save` | Phase 3 | 一時ディレクトリを用いた `src/app/save.c` の原子的保存テスト（正常読み書き、ディレクトリ自動生成、**書き込み/`rename` 失敗時に既存 `save.txt` が破損・消失しないこと**、`--no-save` の隔離） |
| `test_elements` | Phase 3（紅蓮/雷墨）→ Phase 4（爆雷/三元） | 4 属性反応の発火条件・順序（**三元共鳴優先→成立ブロックを爆雷から除外**）、`fluid_sanitize_channels` 後の有限性・`solid=0` 保証、放射インパルスの次ティック適用、`GameEvent` 発行 |
| `test_acts_enemies` | Phase 4 | 全 3 幕の地形 `solid`・山颪・霊柱・新敵 4 種・エリート変異 4 種・鵺・一筆龍簡易版、および 3 幕進行・大遺物報酬・次幕遷移（全 15 CTest に統合済み） |
| `test_determinism_run` | Phase 3（P0 縦切り）→ Phase 4/5 拡張 | 同一ビルド・同一 `RunConfig`・同一ティック入力列（戦闘→3 択/修繕→ボス戦）を 2 インスタンスで実行し、`game_hash()` ビット完全一致を検証（**不一致時は最初に差が出たティックとフィールドを出力**）。あわせて墨消費・撃破・反応・ボス交戦の健全性、**128 件超イベントの安全ドロップ**、**1 フレーム複数ティック時のイベント全件消費**を検証 |
| `test_audio` | Phase 3 | `GameEvent` 消費からのプロシージャル PCM 合成（NaN/オーバーフローなし）、BGM 状態遷移（ロビー/通常戦/ボス戦/勝利） |

---

## 3. 性能ベンチマーク（Release ビルド）

- **対象ビルド**: `cmake --preset release && cmake --build --preset release`（`-O2 -DNDEBUG` 相当）
- **計測コマンド**: 専用ベンチマーク実行体（Phase 2 の `bench_fluid`：流体単体ベンチ、Phase 3 の `bench_game`：`game_step` 全体ベンチ。いずれも CTest 非登録とし Release で実行する）。
- **固定計測条件**:
  - 解像度・格子：160×90、`FluidConfig.project_iters = 12`、`solid` 障害物あり
  - 固定シード・固定入力列（筆ストローク＋渦印＋風斬り＋属性反応＋敵弾が稼働する実戦闘区間。空盤面や報酬停止区間で薄めない）
  - ウォームアップ：120 ticks → 本計測：600 ticks
- **合格基準（単位：`ms/tick` / `ms/frame`）**:
  - `fluid_step`: 平均および p95 **< 2.0 ms/tick**
  - `game_step` 全体: 平均および p95 **< 4.0 ms/tick**
  - `render_draw`（CPU コマンド発行・1280×720 Release）: 目安 **< 4.0 ms/frame**（筆プリミティブ頂点数とあわせて `F1` オーバーレイで監視）
- **記録項目**: 測定環境（CPU / OS / コンパイラ）、シード、ウォームアップ/計測ティック数、平均・p95・最大値、変更前後の差分。

---

## 4. CLI 契約と終了コード

| オプション | 役割 | セーブ I/O |
| :--- | :--- | :---: |
| `--headless-smoke` | ウィンドウなしで本番 `game_step`＋`audio`＋アセット解決を検証して終了 | 無効 |
| `--smoke-frames N` | 隠しウィンドウで `N` フレーム描画して終了（`N` は 10 進数字のみ `1..INT_MAX`） | 無効 |
| `--showcase` | 見本帳シーン（全登場物・反応・UI の一覧確認用）を起動 | 無効 |
| `--no-post` | 和紙ポストシェーダを無効化し直接描画経路を使用 | 通常どおり |
| `--no-save` | セーブファイルの読み込み・書き込みを完全に抑止 | 無効 |
| `--save-path <path>` | セーブファイルパスを明示指定（空・`--` 開始・`PATH_MAX` 以上は不可） | 指定先を使用 |
| `--demo`（Phase 6 で導入） | 固定シード・自動入力によるデモプレイを起動（Phase 6 まで未知オプション） | 無効 |
| `--snap <dir>`（Phase 6 で導入） | 指定ディレクトリへ要所 PNG を自動保存（Phase 6 まで未知オプション） | 無効 |

- **排他・重複・エラー時の終了コード契約**:
  - 排他組み合わせ：`--headless-smoke` と `--smoke-frames` / `--showcase` / `--save-path`、`--save-path` と `--no-save` / `--smoke-frames` / `--showcase`。
  - 未知オプション、引数不足、不正な値、排他指定、**同一オプションの重複** は、ファイル I/O・ウィンドウ作成・メモリ確保を行わずに `stderr` へ `Usage: ` で始まる行を出力して **`EXIT_FAILURE (1)`** で終了する。
- **障害の注入（環境変数 `INKSTORM_DEBUG_FAULTS`）**:
  - カンマ区切りで `shader`（シェーダ読込失敗）・`font`（フォント欠損）を指定し、本物の失敗と同じ分岐を通して検証する（併用は `shader,font`、前後空白は無視）。未知の語は `stderr` へ `INKSTORM: unknown debug fault '<語>' ignored` を 1 行出力して無視する。

---

## 5. 視覚回帰・デモ録画の確認基準

CLI スモーク（終了コード 0）と視覚品質の合格判定は個別に管理する。

### 5.1 描画経路の個別確認（WSLg 実機）
1. **通常経路**（ポストシェーダ有効）
2. **`--no-post` 経路**（直接描画）
3. **シェーダ読込失敗フォールバック経路**（環境変数 `INKSTORM_DEBUG_FAULTS=shader` で本物の失敗と同じ分岐を通し、ログ出力＋直接描画への安全な切替を確認）
4. **フォント欠損フォールバック経路**（環境変数 `INKSTORM_DEBUG_FAULTS=font` で本物の失敗と同じ分岐を通し、和風 TTF 不在時にクラッシュせず英語 ASCII 文字列表で描画することを確認。併用確認は `INKSTORM_DEBUG_FAULTS=shader,font`）
- あわせて、**`assets/` のない場所へ実行ファイルだけを複製して起動する確認**も行い、アセット欠損時でも安全にフォールバックすることを確認する。

### 5.2 視覚チェックリスト（固定シード・1280×720・要所ティック採取）
`--snap <dir>` または録画動画からの `tools/extract_snaps.sh <video> build/snaps`（等間隔 6 枚＋3×2 コンタクトシート）で以下を目視確認する：
- [ ] 和紙背景の上で、敵・敵弾・墨・炎（朱＋火の粉）・雷（蒼＋稲妻）のコントラストと形状区別が明確か
- [ ] ボスの攻撃予兆（朱破線・構え・山颪予告）と HP バーが視認できるか
- [ ] 報酬選択（3 択札・リロール・社修繕）およびロビー/墨庵 UI が読め、操作対象を隠さないか
- [ ] `--no-post` 時およびフォント欠損時でも重要情報と文字が判別できるか
- [ ] `F1` X 線オーバーレイで速度ベクトル・渦度・計算時間（`solve_ms`）・属性セル数・筆プリミティブ頂点数が読めるか
- [ ] **見本帳シーン**：全登場物・反応・UI 部品を 1 画面に並べて `--snap` 採取し、トーン・太さ・主従が揃っているか
- [ ] **3 つの崩し試験**：スナップをグレースケール化・5px ぼかし・25% 縮小しても、社・敵・弾・墨玉が識別できるか
- [ ] **`--no-post` 同一ティック比較**：通常経路と `--no-post` を並べ、形・濃淡・視認性の順位が保たれているか
- [ ] **墨 6 割充填試験**：画面の約 6 割を濃墨で覆ったスナップでも、紙抜き縁（`ink_knockout`）により敵・弾が埋没せず判別できるか

---

## 6. スタンドアロン配布アーカイブ検証

- `tools/package_dist.py`（CI `dist-verify` および最終リリース前ローカル実行）で以下を機械検証する：
  - 開発専用ファイル（`AGENTS.md`, `docs/`, `tests/`, `tools/`, `.github/`, `.agents/`, `CMakeLists.txt`, `flake.nix`, `third_party/unity/` 等）の混入ゼロ。
  - 一時展開先で Nix 抜きの素の `/usr/bin/make` ＋ `/usr/bin/gcc` ＋ raylib 6.2（Linux OpenGL / X11 開発ライブラリ前提）によるビルド成功と `./inkstorm --headless-smoke` 成功。
- **最終リリース判定原則**: 最終デモ動画の収録と配布 zip の合格検証は、すべての調整・バグ修正が `main` へ合流した **同一コミット** に対して実施する。
