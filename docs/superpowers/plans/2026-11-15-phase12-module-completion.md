# フェーズ12 モジュールの仕上げ Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `use_module/1,2` と `ensure_loaded/1` をゴールで呼べるようにする。ゴールの読み込みが失敗したら、その場で診断を 1 回だけ出す。`reexport/1,2` と、`strip_module/3`、`context_module/1`、`module_property/2`、`import/1` を入れる。インタプリタと `mosac` の結果を同じにする。

**Architecture:**
- ゴールの読み込みは、`load.cpp` の新しい入口 `load_goal_term` で動かす。指令と同じ `apply_use`・`apply_file` を、外側の読み込み 1 回として呼ぶ（ADR 0026）。組み込みの側は `consult.cpp` の `run_load_goal` が受け持つ。中身は引数の検査、文脈モジュール、親の印、診断である。
- 失敗の診断は `Impl::load_report` に書く。空なら `std::cerr`、セッションなら `Echo::warn` に行く。トップレベルは、ゴールが書いた診断をもう一度書かない。
- 公開の一覧の項目を `Export` にする。持つのは公開名、項数、定義したモジュール、そこでの名前である。再公開は、取り込んだ項目をこの形で足すだけにする。`resolve_pred` は変えない。
- `mosac` は、翻訳の読み込みの間に字面の `library(Name)` を `TranslationLog` に集める。読み終えたら、どこにも取り込まない形（`use_module(library(Name), [])`）で読み、同梱に入れる。

**Tech Stack:** C++20 標準ライブラリ、CMake 3.25。テストは `mosatest`（`tests/pl` の `test(Name, Goal, Expect)` 行）と `tests/cpp/harness.hpp`（`mosa_test::check`）、`mosa::run_session`、`StdCapture`（`src/machine/std_capture.hpp`）。`mosac` の単独実行のテストは `tests/mosac/standalone_test.cmake`。

**Spec:** `docs/superpowers/specs/2026-11-15-phase12-module-completion-design.md`。所有者が 2026-11-15 に実装計画を頼んだので、設計の承認とみなした。問い 1〜3 は所有者が A に決めた。ADR 0026 はまだ承認されていない。タスク 4 と 6 の実装は、その承認の後に始める。

## 設計との違い（計画を書くときに決めた）

1. 今のテストのうち 2 件が「`ensure_loaded/1` はゴールでない」ことを確かめている。`tests/pl/directives.pl` の `ensure_goal` と、`tests/pl/japanese.pl` の `ensure_ha` である。どちらも `existence_error(procedure, ensure_loaded/1)` を期待する。設計は「既存のテストの成功と失敗は変わらない」と書いたが、この 2 件を見落としていた。iso-notes「7.4.2 ensure_loaded/1」の改訂（タスク 1）に従って、タスク 4 で期待を `fail` に変える（ファイルが無いので診断を出して失敗する）。コメントも改訂に合わせる。`include_goal`・`include_ha` は変えない。
2. マニュアル第12章の `<!-- check: no-library -->` の塊は、`--manual` が打って比べる。この塊はプロンプトの `use_module/1` が警告になると書いているので、タスク 4 で直す（設計ではタスク 9）。第14章の表は `--manual --all-tables` が `builtin_procedures()` と突き合わせる。そのため、表の行は述語を足すタスク（4、7、8）で日英とも足す。
3. 完了条件 3 のライブラリは `library(apply)` にする。`translate_program` の `load_catalogs` が `messages_*` → `format` → `lists` を必ず同梱するので、`lists` では直す前もテストが通ってしまう（赤にならない）。
4. `module_property/2` の性質を持つのは、`module/2` で作ったモジュールだけにする。`user` と `system` は性質を持たない（`module_property(user, P)` は失敗する）。誤りを確かめる順は、`M` の型、`P` の領域、`M` があるか、の順にする。
5. `import/1` の引数に `M:` が無ければ、文脈モジュールから取り込むとみなす。自分からの取り込みなので、何も変えずに成功する（`apply_imports` の `from == into` と同じ）。`M:` があれば、`M` が変数なら `instantiation_error`、原子でなければ `type_error(atom, M)` にする。`M1:M2:PI` は内側の `M2` から取り込む。
6. ゴールの読み込みの診断の字面。開いたファイルの中の誤りは、`mosa F` と同じ字面にする（`F: 理由`、構文の誤りは `F:行:桁: 理由`。`F` は開いたパス）。読んだ後の誤り（モジュールでない、取り込みの衝突、公開していない）も `F: 理由` にする。ファイルを探す前の誤り（ライブラリが見つからない）は、`Spec` を書いた字面を付ける（`library(nope): existence_error(source_sink,library(nope))`）。開けないファイルは、今の `run_consult` と同じく `cannot_open_file` の文面だけにする。
7. `mosac` が字面のライブラリを探す範囲は、読んだ項の全体の部分項にする。制御構造だけには絞らない。余分に見つけた名前は、ライブラリがあれば同梱が増えるだけで、無ければ黙って飛ばす。翻訳は止めない。`--diff` の翻訳（`units.cpp` の `translate_one`）は変えない。差分の訳した側は、ゴールで読むライブラリを同じディスクの `lib/` から読むので、結果は同じになる。

## Global Constraints

- 依存は C++20 標準ライブラリと CMake 3.25 以上だけにする。`-Wall -Wextra -Wpedantic`（MSVC は `/W4`）の警告はエラーとして扱う。3 OS（macOS、Linux の GCC と Clang、Windows の MSVC）で通す。
- 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロ、コンパイラ拡張を使わない。関数は 60 行以内、ファイルは 800 行以内にする。`src/machine/internal.hpp`（800 行）には、タスク 2 で分けてから足す。次のファイルには足さない: `src/builtins/write_pred.cpp`（799）、`src/builtins/database.cpp`（762）、`tests/cpp/stream_test.cpp`（785）、`tests/cpp/machine_test.cpp`（778）。`tests/cpp/session_test.cpp`（792）には、タスク 3 と 4 の入口関数（`check_goal_load_reports()` と `check_goal_use_session()`）の宣言と呼び出しの 4 行だけを足す。個々の確かめは `tests/cpp/session_goals_test.cpp` に書き、入口関数から呼ぶ。
- エラーは `raise_formal` と `formal_*` の族だけで作る。アトムの名前を文字列で直に書かない（今の `formal_type(machine, "atom", …)` の形はよい）。
- 組み込み述語の C++ は、引数の検査、本体、束縛の順に書く。ファイルの頭のコメントには、節番号、引数のモード、投げうる誤り、テストのファイル名を書く。新しい述語の節番号は「13211-1 に条項は無い。13211-2 での位置は未確認」と書く。
- テストを先に書き、赤を確かめてから緑にする。エラーのテストは `Formal` だけを比べる。期待値を実装に合わせて変えない（例外は「設計との違い」1 の 2 件だけ）。
- `tests/pl` の相対パスは、ゴールではリポジトリの根（ctest の作業ディレクトリ）から、指令では `tests/pl` から開く。新しく書き出すファイルは `tests/tmp/` の下に置く。
- 組み込みを足すタスク（4、7、8）は、同じコミットで `engine.cpp` の `kBuiltins` とマニュアル第14章（日英）の表の行を足す。SPEC 第5節はタスク 1 で全部書く。
- `tests/pl` の行を足すか変えるコミットと、`--diff` が捕まえる標準エラーを変えるコミット（タスク 3）では、`ctest --preset macos-clang -L diff` を流す。`docs/agreement.md` と `docs/agreement.json` を同じコミットに入れる。タスク 3、4、6 では `./build/mosatest --suite tests/conformance` も流す。結果が変われば `docs/conformance*.md`・`*.json` を入れる。ISO 欄 98.4%、ライブラリ欄 98.0% を下回らないこと。
- 決定は変えない。自動読み込みはしない。モジュールを作るのは `module/2` だけ。`use_module` が受け付けるのはモジュールのファイルだけ。選択点の下の読み直しは断る（`take_over`、ADR 0017）。`check_consult_failure_reports` の期待値も変えない。
- 識別子は英語、コメントとコミットメッセージは日本語で書く。文書は英語が正で、`*.ja.md` を同じコミットで直す。`docs/STATUS.md` は英語だけにする。
- コミットは論理単位ごとに 1 つ。メッセージは 1 行で、`[phase12] 内容（確認方法）` の形にする。
- 1 セッション 1 タスク。始めに `docs/STATUS.md` の「In progress」にタスクを書く。終わりに `cmake --build --preset macos-clang && ctest --preset macos-clang` を通し、STATUS を 15 行以内で直す。
- レビュアーは opus を使う。

## Review Focus

1. **字句の旗の持ち出し。** `:- set_prolog_flag(double_quotes, atom).` を持つモジュールのファイルをゴールで読んでも、呼んだ側の `double_quotes` は変わらない。タスク 4 の `goal_use_flags` と `goal_use_flags_module` で確かめる。
2. **取り込みの衝突の後の巻き戻し。** ファイルを読み終えた後で衝突したゴールは、モジュールも準備済みの印も残さない。直した指定でもう一度呼べば読める。タスク 4 の `goal_use_conflict`、`goal_use_conflict_left`、`goal_use_conflict_retry` で確かめる。
3. **診断の回数。** `\+ consult(M)` は 1 行を出して成功する。`(\+ consult(M), \+ consult(M))` は 2 行を出す。問い合わせ全体が失敗しても、もう 1 行は出ない。埋め込み（セッション無し）では `std::cerr` に出る。タスク 3 の `check_goal_load_report_count`、`check_goal_load_report_once`、`check_goal_load_report_embedded` で確かめる。
4. **深い本体の翻訳。** 字面のライブラリの検出は、深さ 1,000,000 の項でも C++ の再帰で落ちない。タスク 5 の `check_goal_library_deep` で確かめる。
5. **訳したモジュールの節の中の文脈。** `mosac` の生成コードの中でも、`context_module/1` はその節のモジュールを、`strip_module/3` はそのモジュールを既定にして返す。タスク 7 の `cm_module`、`sm_in_module` を `mosatest --diff` に通して確かめる。

---

### Task 1: フェーズを開く（文書だけ）

**Files:**
- Modify: `docs/SPEC.md` と `.ja.md`（第9節の表に 12 行目。第5節にゴールの読み込み、`reexport/1,2`、内省の 4 つの述語）
- Modify: `docs/iso-notes.md` と `.ja.md`
- Create: `docs/adr/0026-goal-load-and-reexport.md` と `.ja.md`。Modify: `docs/adr/README.md` と `.ja.md`（索引に 1 行）
- Modify: `README.md` と `.ja.md`（バッジ `phases-0--11_done` を `phase-12_in_progress` に、フェーズの表に 12 行目）
- Modify: `docs/STATUS.md`

- [ ] **Step 1: SPEC を書く（日英）。** 第9節の 12 行目の完了条件は、設計の「完了条件」1〜6 を写す。ただし 3 のライブラリは `library(apply)` にする（「設計との違い」3）。第5節には次を書く。ゴールの読み込みの規則（相対パスは作業ディレクトリから、取り込み先は文脈モジュール、引数の誤りは例外、ほかの失敗は診断を出して失敗、診断の字面は「設計との違い」6）、`reexport/1,2`、4 つの述語の表（設計の表に「設計との違い」4・5 を加える）、新しく `system` に入る名前。
- [ ] **Step 2: iso-notes を書く（日英）。**
  - 問い 1〜3 を、決定（3 つとも A、2026-11-15、所有者）と一緒に書く。問い 1 の前提の訂正も書く。
  - 「7.4.2 ensure_loaded/1」の「指令だけ」を改める。ゴールでも呼べる。拡張子と、1 回だけ読むことの決定は残す。`ensure_goal`・`ensure_ha` の期待が `fail` に変わることも書く。
  - 「Section 5 Conflict between two imports」に、取り込み元は定義したモジュールで比べる、という読みを足す。
  - 新しい節「Section 5 `@/2` and `add_import_module/3`」に、入れない理由を書く（ADR 0006 の枠、既定の取り込み元の連鎖）。
  - 新しい節「7.4.2 Consulting a path again under a choice point」に、`take_over` の断りを残すこと（問い 3 の A）を書く。
  - 新しい節「Section 5 `module_property/2` `file(F)` under mosac」に、同梱したファイルの `F` が同梱の鍵になることを書く。
- [ ] **Step 3: ADR 0026 を書く（日英）。** Context、Decision、Rejected alternatives、Consequences を各 3 行以内にする。中身は設計の「ADR 0026」の節のとおり。Status は、所有者が承認していれば `Approved (日付, owner)`、まだなら `Proposed` にする。
- [ ] **Step 4: README と STATUS を直す。** STATUS の 1 行目は「Phases 0–11 done, phase 12 open (module completion, opened 2026-11-15, owner decisions A, A, A)」で始める。Doubts の 2 件（選択点の下の読み直しの黙った失敗、ゴールの `consult/1` の黙った失敗）は、問い 3 と問い 1 の決定を指す文に置き換える。
- [ ] **Step 5: 確かめる。** `./build/mosatest --manual docs/manual --all-tables` が 0 failed。`grep -n 'phase-12\|phase 12' README.md README.ja.md docs/STATUS.md` が 3 ファイルに当たる。
- [ ] **Step 6: Commit.** `[phase12] フェーズ12を開き SPEC 第9節の 12 行目と第5節、iso-notes の問い 3 つと ensure_loaded の改訂、ADR 0026 を日英で書く（文書だけ、manual 0 failed）`

---

### Task 2: `internal.hpp` を分ける（振る舞いは変えない）

**Files:**
- Create: `src/machine/load_api.hpp`。`internal.hpp` の 751〜795 行（`load_source` の宣言から `redo_native` の宣言まで、コメントごと）を移す。
- Modify: `src/machine/internal.hpp`（末尾の `#include "builtin_api.hpp"` の後に `#include "load_api.hpp"`）

**Interfaces:**
- Produces: `load_api.hpp`。読み込みと同梱の宣言の置き場になる。タスク 3〜5 の新しい宣言はここに置く。`internal.hpp` が含むので、使う側の `#include` は変わらない。

- [ ] **Step 1: 移す前の件数を控える。** `ctest --preset macos-clang -N | tail -1`
- [ ] **Step 2: 移す。** 宣言の文とコメントは変えない。`load_api.hpp` の頭のコメントに、分けた理由（800 行の上限）を書く。
- [ ] **Step 3: 確かめる。** ビルドに警告が無い。ctest の件数が同じで、全部通る。`wc -l src/machine/internal.hpp` が 760 以下になる。
- [ ] **Step 4: Commit.** `[phase12] internal.hpp から読み込みと同梱の宣言を load_api.hpp に分ける（振る舞いは変えない。ctest 170/170）`

---

### Task 3: ゴールの読み込みの失敗を知らせる（`consult/1`、問い 1 の A）

**Files:**
- Modify: `src/machine/internal.hpp`（`Impl` に 2 つ。`std::function<void(std::string_view)> load_report;` と `std::string reported_diagnostic;`）
- Modify: `src/machine/consult.cpp`、`src/machine/load_api.hpp`、`src/machine/query.cpp:80`、`src/machine/session.cpp`
- Test: `tests/cpp/session_goals_test.cpp`（入口 `check_goal_load_reports()` と 3 つの確かめ）、`tests/cpp/session_test.cpp`（宣言と `main` の呼び出しの 2 行）、`tests/tests.cmake:333`（`session_test` の include に `src` を足す。`machine/std_capture.hpp` を使うため。`codegen_test` と同じ形）

**Interfaces:**
- Produces: `void report_goal_load_failure(Impl& machine);`（`load_api.hpp`、定義は `consult.cpp`）。`machine.diagnostic + "\n"` を、`load_report` があればそれへ、無ければ `std::cerr` へ書いて吐き出す。`reported_diagnostic = machine.diagnostic` にする。タスク 4 の `run_load_goal` も、失敗のたびにこれを呼ぶ。

- [ ] **Step 1: 失敗するテストを書く。** `/no/such/p12.pl` の行は `"/no/such/p12.pl: ファイルを開けません"` で数える。
  - `check_goal_load_report_count`: `run_session({"mosa", "-g", "(\\+ consult('/no/such/p12.pl'), \\+ consult('/no/such/p12.pl'))."}, "", false)` の `code == 0`、`out` に `true.`、`err` の中でその行が 2 回。
  - `check_goal_load_report_once`: `-g "consult('/no/such/p12.pl')."` の `code == 1`、行が 1 回。`tests/tmp/p12_busy.pl` に `p(1).` を書く。`-g "(consult('<path>'), findall(x, consult('<path>'), L), L == [])."` が成功し、`"<path>: 実行中の述語は置き換えられない"` が 1 回出る。
  - `check_goal_load_report_embedded`: `StdCapture` の中で `Machine::create()` の `query("\\+ consult('/no/such/p12.pl').")` が `Success`。`capture.text()` に行が 1 回出る。
- [ ] **Step 2: 赤を確かめる。** `ctest --preset macos-clang -R '^session$' --output-on-failure` で 3 つが落ちる。`check_consult_failure_reports` は通る。
- [ ] **Step 3: 実装する。**
  - `run_consult` の 2 つの失敗の道（開けない、読み込みの失敗）で、`machine.failed = true` の前に `report_goal_load_failure` を呼ぶ。
  - `query.cpp:80` で `reported_diagnostic` も空にする。
  - `session.cpp` の `play_goal` の失敗の道は、`diag == engine_impl(machine).reported_diagnostic` なら書かない。
  - `run_with_machine` と `run_prepared_session` は、`load_report` を `echo.warn` に向ける。`run_prepared_session` は、機械が呼び手のものなので、抜けるときに空へ戻す。
  - `session.cpp:318`・`:384` と `native_main.cpp:60` は、外側の読み込みの診断を書く所なので変えない（ゴールの診断と字面が違う）。
  - `load_report` も `std::cerr` も、`user_error` の流れの桁を進めない。`Echo::warn` と同じで、タスク 9 で既知の制限に書く。
- [ ] **Step 4: 緑を確かめる。** `ctest --preset macos-clang -R '^session'` が全部通る。
- [ ] **Step 5: 差分とスイートを流す。** `ctest --preset macos-clang -L diff` が通る。`failed_load.pl` の行は両側で同じ標準エラーになる。`docs/agreement.*` を作り直す。`./build/mosatest --suite tests/conformance` の 2 つの欄が下がらない。
- [ ] **Step 6: Commit.** `[phase12] ゴールの consult/1 の失敗で診断をその場で 1 回だけ出し、トップレベルは繰り返さない（問い 1 の A。session の新しい 3 件、ctest -L diff 0 差分）`

---

### Task 4: `use_module/1,2` と `ensure_loaded/1` をゴールにする（ADR 0026 の承認の後）

**Files:**
- Modify: `src/machine/load.cpp`（`load_source` の前後を `run_load` に括り出し、`load_goal_term` と `check_use_spec` を足す）、`src/machine/consult.cpp`（`run_load_goal`）、`src/machine/load_api.hpp`、`src/machine/engine.cpp`（`kBuiltins` に 3 つ、`run_builtin` で `consult` の隣に振り分け）
- Create: `tests/pl/modules/goal_ctx.pl`（`:- module(goal_ctx, []).`）、`goal_pick.pl`（`:- module(goal_pick, [gp_a/1, gp_b/1]).` と `gp_a(1). gp_b(2). gp_hidden.`）、`goal_flags.pl`（`:- module(goal_flags, [gf/1]).`、`:- set_prolog_flag(double_quotes, atom).`、`gf("x").`）、`goal_conflict.pl`（`:- module(goal_conflict, [gx_c/1, gx_d/1]).` と `gx_c(module). gx_d(1).`）、`goal_plain.pl`（`gpl_fact(1).`）
- Modify: `tests/pl/modules.pl`、`tests/pl/directives.pl:139-142`、`tests/pl/japanese.pl:57`、`tests/cpp/session_goals_test.cpp`、`docs/manual/{en,ja}/12-modules-libraries.md`（手順 1・2 の文と `no-library` の塊）、`docs/manual/{en,ja}/14-reference.md`（3 行）

**Interfaces:**
- Consumes: `report_goal_load_failure`（タスク 3）。
- Produces:
  - `[[nodiscard]] std::optional<Cell> check_use_spec(Impl& machine, Cell spec);`（`load.cpp`）。形だけを見る: 変数、`library(変数)` は `instantiation_error`、`library(非原子)` は `type_error(atom, X)`、原子でも `library(_)` でもなければ `type_error(atom, Spec)`。`locate_use` の頭もこれを呼ぶので、指令とゴールの Formal は同じになる。
  - `[[nodiscard]] LoadStatus load_goal_term(Impl& machine, Cell directive);`（`load.cpp`）。`directive` は `:-(G)`。`G` は `use_module/1,2` か `ensure_loaded/1` で、引数は検査済み。相対パスの基準は作業ディレクトリ（`directory` は空）。開いたファイルは根のテキストとして読み（`note_syntax` の `nested` を偽にする）、診断には「設計との違い」6 の字面を付ける。字句の旗（`double_quotes` と全角の記号）は、ファイルの consult と同じく戻す。取り込み先は `machine.load_module`。タスク 5 も使う。
  - `[[nodiscard]] bool run_load_goal(Impl& machine, const Goal& goal);`（`consult.cpp`）。引数の誤りは `raise_formal` で投げる（`ensure_loaded` は変数で `instantiation_error`、原子でなければ `type_error(atom, F)`。`Imports` は `parse_imports` の Formal）。そのあと `run_consult` と同じ形で読む: `RootScope` に goal と項を登録し、`load_module` を `goal.module` にし、`mark_seen`/`restore_seen` を通す。失敗なら `report_goal_load_failure` を呼んで失敗する。成功なら選択点を残さずに `ip = goal.cont`。

- [ ] **Step 1: 失敗するテストを書く（`modules.pl`。ゴールのパスは根から）。** `gx_c(local).` を `modules.pl` に置き、`:- consult('modules/goal_ctx.pl').` を先に読む。`user` には `lists` を取り込まない。`modules.pl:163` の `lists_needs_use` が、`user` に `append/3` が無いことを確かめているからである。そのため、ライブラリを読むゴールには `goal_ctx:` を付ける。
  - 振る舞い:
    - `goal_use_before`: `predicate_property(goal_ctx:last(_, _), imported_from(_))` は `fail`（`user` が `lists` を取り込んでいても、`goal_ctx` 自身の取り込みは無い）
    - `goal_use_context`: `(goal_ctx:use_module(library(lists)), predicate_property(goal_ctx:last(_, _), imported_from(M)), goal_ctx:last([1, 2], X))` は `solutions([(M = lists, X = 2)])`
    - `goal_use_det`: `findall(x, goal_ctx:use_module(library(lists)), L)` は `solutions([L = [x]])`
    - `goal_use_bound`: `(N = lists, goal_ctx:use_module(library(N), [last/2 as goal_last]), goal_ctx:goal_last([a], X))` は `solutions([X = a])`
    - `goal_use_list`: `(use_module('tests/pl/modules/goal_pick.pl', [gp_a/1]), gp_a(X))` は `solutions([X = 1])`
    - `goal_use_list_hidden`: `gp_b(_)` は `error(existence_error(procedure, gp_b/1))`
    - `goal_use_as`: `(use_module('tests/pl/modules/goal_pick.pl', [gp_b/1 as gp_bee]), gp_bee(X))` は `solutions([X = 2])`
    - `goal_use_flags`: `(current_prolog_flag(double_quotes, B), use_module('tests/pl/modules/goal_flags.pl'), current_prolog_flag(double_quotes, A), A == B)` は `true`
    - `goal_use_flags_module`: `(gf(X), atom(X))` は `true`
    - `goal_ensure_loaded`: `(ensure_loaded('tests/pl/modules/goal_plain'), ensure_loaded('tests/pl/modules/goal_plain'), findall(X, gpl_fact(X), L))` は `solutions([L = [1]])`
  - 失敗（診断を出して失敗）:
    - `goal_use_not_module`: `use_module('tests/pl/modules/goal_plain.pl')` は `fail`
    - `goal_use_not_exported`: `use_module('tests/pl/modules/goal_pick.pl', [gp_hidden/0])` は `fail`
    - `goal_use_missing`: `use_module('tests/tmp/p12_no_such.pl')` は `fail`
    - `goal_use_lib_missing`: `use_module(library(p12_no_such))` は `fail`
  - 衝突と巻き戻し:
    - `goal_use_conflict`: `use_module('tests/pl/modules/goal_conflict.pl')` は `fail`
    - `goal_use_conflict_left`: `current_module(goal_conflict)` は `fail`
    - `goal_use_conflict_retry`: `(use_module('tests/pl/modules/goal_conflict.pl', [gx_d/1]), gx_d(X))` は `solutions([X = 1])`
  - 引数の誤り（`error(...)`）:
    - `goal_use_var`: `use_module(_)` は `instantiation_error`
    - `goal_use_type`: `use_module(1)` は `type_error(atom, 1)`
    - `goal_use_lib_var`: `use_module(library(_))` は `instantiation_error`
    - `goal_use_lib_type`: `use_module(library(1))` は `type_error(atom, 1)`
    - `goal_use_imports_var`: `use_module(library(lists), _)` は `instantiation_error`
    - `goal_use_imports_type`: `use_module(library(lists), foo)` は `type_error(list, foo)`
    - `goal_use_pi_type`: `use_module(library(lists), [foo])` は `type_error(predicate_indicator, foo)`
    - `goal_use_alias_type`: `use_module(library(lists), [last/2 as 1])` は `type_error(atom, 1)`
    - `goal_ensure_var`: `ensure_loaded(_)` は `instantiation_error`
    - `goal_ensure_lib`: `ensure_loaded(library(lists))` は `type_error(atom, library(lists))`
  - 静的: `goal_use_static` は `assertz(use_module(x))` で `error(permission_error(modify, static_procedure, use_module/1))`。`goal_ensure_static` は `ensure_loaded/1` で同じ形。
  - 期待を変える 2 件（「設計との違い」1）: `directives.pl` の `ensure_goal` と `japanese.pl` の `ensure_ha` を `fail` にし、上のコメント（「指示だけ」）を改訂に合わせる。
- [ ] **Step 2: C++ のテストを書く（`session_goals_test.cpp`、入口 `check_goal_use_session()` から呼ぶ）。**
  - `check_goal_use_at_prompt`: `run_session({"mosa"}, "use_module(library(lists)).\nlast([1, 2], X).\n", true)` の `out` に `X = 2`。ctest の `session` に `MOSA_LIB` が無ければ `tests.cmake` で `MOSA_LIB=${CMAKE_SOURCE_DIR}/lib` を足す。
  - `check_goal_use_reports_file`: `tests/tmp/p12_fail.pl` に `:- module(p12_fail, []).\n:- fail.\n` を書く。`-g "use_module('<path>')."` の `err` に `"<path>: この指示のゴールは失敗した: :-fail"` が 1 回。`tests/tmp/p12_syntax.pl` に `:- module(p12_syntax, []).\nr :- .\n` を書くと、`"<path>:2:"` で始まる行が 1 回。`-g "use_module(library(p12_none))."` は `"library(p12_none): existence_error(source_sink,library(p12_none))"` が 1 回。
  - `check_goal_ensure_busy`: `tests/tmp/p12_busy_a.pl` に `p12_p(1).`、`p12_busy_b.pl` に `p12_p(2).` を書く。`mosa <a> -g "((true ; fail), ensure_loaded('<b>'))."` の `code == 1`。`"<b>: 実行中の述語は置き換えられない"` が 1 回。
- [ ] **Step 3: 赤を確かめる。** `./build/mosatest tests/pl/modules.pl` の新しい行と、`ctest -R '^session$'` の新しい 3 件が落ちる。
- [ ] **Step 4: 実装する（上の Interfaces どおり）。** `load_source` は、括り出した `run_load` に `read_terms` を渡す形にする。振る舞いは変えない。`run_load` は 60 行以内にし、写し・旗・演算子・モジュールの退避と `reject_load` を 1 か所に残す。
- [ ] **Step 5: マニュアルを直す（日英）。**
  - 第12章の手順 1 は「プロンプトで `use_module/1` を打てば読める」にする。`no-library` の塊は、`append/3` の警告、`?- use_module(library(lists)).` の `true.`、`?- append([a, b], [c, d], X).` の `X = [a,b,c,d].` にする。
  - 手順 2 の `libs.pl` は「読み込むたびにライブラリを使うプログラムは、指令としてファイルに書く」という案内に変える。手順 3 の `check: append` の塊は変えない。
  - 第14章: 組み込みの表（193 行より後。`--all-tables` が `builtin_procedures()` と突き合わせる）に、`use_module/1`、`use_module/2`、`ensure_loaded/1` のゴールとしての行を足す。指令の表（172〜175 行）の行は残し、ゴールでも呼べることを 1 文足す。
- [ ] **Step 6: 緑を確かめる。** `./build/mosatest tests/pl/modules.pl tests/pl/directives.pl tests/pl/japanese.pl` と `./build/mosatest --manual docs/manual --all-tables` と `ctest --preset macos-clang` が全部通る。`ctest -L diff` も通るので、`docs/agreement.*` を作り直す。スイートの 2 つの欄が下がらない。
- [ ] **Step 7: Commit.** `[phase12] use_module/1,2 と ensure_loaded/1 をゴールで呼べるようにする（ADR 0026。modules.pl の新しい行、ensure_goal と ensure_ha は iso-notes 7.4.2 の改訂で fail、session 3 件、manual 0 failed、ctest -L diff 0 差分）`

---

### Task 5: `mosac` がゴールで読むライブラリを同梱する（問い 2 の A）

**Files:**
- Create: `src/machine/goal_libraries.cpp`（2 つの関数）。`CMakeLists.txt` の `mosa_lib` の源に足す。
- Modify: `src/machine/internal.hpp`（`TranslationLog` に `std::vector<std::string> goal_libraries;`）、`src/machine/load_api.hpp`、`src/machine/load.cpp`（`take_term` で、文法規則を訳した後、`include` の前に `note_goal_libraries` を呼ぶ）、`src/codegen/project.cpp`（`translate_program` で、`load_catalogs` の後、`impl.translation = nullptr` の前に `load_goal_libraries`。`options.library` のときは呼ばない）
- Create: `tests/mosac/goal_uses_apply.pl`、`tests/mosac/init_uses_apply.pl`
- Modify: `tests/mosac/standalone_test.cmake`（2 つの CASE）、`tests/tests.cmake:589`（CASE の並び）、`tests/cpp/codegen_test.cpp`

**Interfaces:**
- Consumes: `load_goal_term`（タスク 4）。
- Produces:
  - `void note_goal_libraries(Impl& machine, Cell term);` `machine.translation` が空なら何もしない。項の部分項をすべて、明示のスタック（`std::vector<Cell>`）で歩く。C++ の再帰は使わない。`use_module(library(N))` と `use_module(library(N), _)` のうち、`N` が原子のものの名前を、まだ無ければ `goal_libraries` に足す。
  - `void load_goal_libraries(Impl& machine);` `goal_libraries` を添字で前から読む。読むうちに伸びる分も読む。それぞれ `use_module(library(N), [])` を、`load_module` を `user_module` にして `load_goal_term` で読む。失敗しても止めず、診断も出さない（巻き戻しは `load_goal_term` が済ませる）。

- [ ] **Step 1: 失敗するテストを書く。**
  - `codegen_test.cpp` の `check_goal_library_names`: `TranslationLog` を付けた機械で `load_program` に次の 3 行を渡すと、`goal_libraries == {"apply", "between"}` になる。
    - `p :- catch(m:use_module(library(apply)), _, true).`
    - `q(N) :- use_module(library(N)).`
    - `:- initialization(use_module(library(between), [between/3])).`
  - `check_goal_library_deep`: ヒープに `f(f(…f(use_module(library(dcg)))…))`（深さ 1,000,000）を組む。`note_goal_libraries` が落ちずに `"dcg"` を 1 つ足す。
  - `goal_uses_apply.pl`: `:- initialization(main, main).` と `main :- use_module(library(apply)), apply:maplist(atom_length, [ab, c], L), write(L), nl.`。出力は `[2,1]\n`。
  - `init_uses_apply.pl`: `:- initialization(use_module(library(apply))).`、`:- initialization(main, main).`、`main :- apply:maplist(atom_length, [abc], L), write(L), nl.`。出力は `[3]\n`。
  - CASE は `goal_use_body` と `goal_use_init`。どちらも `--verbose` の標準エラーに `mosac: 結ぶ翻訳済みライブラリ: [^\n]*mosa_pl_apply` を求め、未定義の呼び出しの警告が無いことを求める（今の検査がそのまま効く）。
- [ ] **Step 2: 赤を確かめる。** `ctest --preset macos-clang -R 'codegen|standalone_goal_use'` が落ちる。実行ファイルは `library(apply)` を見つけられず、診断を出して `main` が失敗する。
- [ ] **Step 3: 実装する（上の Interfaces どおり）。** `translate_one`（`--diff`）は変えない（「設計との違い」7）。
- [ ] **Step 4: 緑を確かめる。** `ctest --preset macos-clang -R 'codegen|standalone|mosac'` が全部通る。そのあと `ctest --preset macos-clang` 全体も通る。
- [ ] **Step 5: Commit.** `[phase12] mosac が節と指令に字面で書いた use_module(library(N)) のライブラリを同梱する（問い 2 の A。standalone_goal_use_body と _init、codegen の 2 件）`

---

### Task 6: `reexport/1,2`（ADR 0026 の公開の項目。承認の後）

**Files:**
- Modify: `src/module/module.hpp`（`Export` を足し、`Module::exports` を `std::vector<Export>` にする。`add_reexports` の宣言）
- Modify（`exports` の使い手を全部）: `src/module/import.cpp`（`exported`、`apply_imports` の 2 つの枝、`add_reexports` の定義）、`src/module/qualify.cpp:66-67,171`、`src/builtins/module_prop.cpp:74`（`exported_by`）、`src/codegen/check.cpp:103`（`export_warnings`）、`src/machine/manual_check.cpp:578`、`src/machine/lgt_libs.cpp:35`
- Modify: `src/machine/load.cpp`（`use_body` が `reexport/1,2` も受け、`apply_use` は取り込みの後で `add_reexports` を呼ぶ）
- Create: `tests/pl/modules/` に次を作る。
  - `rx_base.pl`: `:- module(rx_base, [rb_a/1, rb_b/1, (===>)/2, op(700, xfx, ===>)]).` と `rb_a(1). rb_b(2). X ===> X.`
  - `rx_all.pl`: `:- module(rx_all, [ra_own/1]).`、`:- reexport(rx_base).`、`ra_own(0).`
  - `rx_some.pl`: `:- module(rx_some, []).`、`:- reexport(rx_base, [rb_a/1 as rs_a]).`
  - `rx_except.pl`: `:- module(rx_except, []).`、`:- reexport(rx_base, except([rb_b/1])).`
  - `rx_clash.pl`: `:- module(rx_clash, [rb_a/1]).`、`rb_a(clash).`
  - `rx_plain.pl`: `:- reexport('元帳.pl').`（モジュールでないテキスト。`user` に取り込むだけ）
  - `元帳.pl`: `:- module(元帳, [残高/1]).`、`残高(100).`
  - `窓口.pl`: `:- module(窓口, []).`、`:- reexport('元帳.pl', [残高/1 as 照会]).`
- Create: `tests/mosac/rx_lib.pl`（`:- module(rx_lib, [lib_p/1]). lib_p(42).`）、`tests/mosac/rx_mid.pl`（`:- module(rx_mid, []). :- reexport(rx_lib).`）、`tests/mosac/uses_rx.pl`（`:- use_module(rx_mid).`、`:- initialization(main, main).`、`main :- lib_p(X), write(X), nl.`）
- Modify: `tests/pl/modules.pl`、`tests/pl/japanese.pl`、`tests/mosac/standalone_test.cmake`、`tests/tests.cmake:589`

**Interfaces:**
- Produces:
  - `struct Export { std::uint32_t name = 0; std::uint16_t arity = 0; std::uint32_t from = 0; std::uint32_t from_name = 0; };`（`module.hpp`）。`name` は公開した名前、`from`・`from_name` は定義したモジュールとそこでの名前。自分の述語は `from == Module::name`、`from_name == name` になる。
  - `apply_imports` が作る `Import` は、`name = 項目.from_name`、`from = 項目.from` にする。`alias` は `as` の別名、無ければ `項目.name`。これで `resolve_pred` は 1 段で定義に着く。`add_import` の「同じ取り込み元、同じ名前なら衝突でない」は、そのまま定義したモジュールで比べる。
  - `[[nodiscard]] std::optional<Cell> add_reexports(Machine::Impl& machine, std::uint32_t from, std::uint32_t into, const ImportSpec& spec);` 取り込みと同じ項目を `into` の `exports` に足す（同じ名前と項数の項目があれば置き換える）。`All` と `Except` では、`from` の `export_ops` も `into` の `export_ops` に足す（重ねない）。`into` が `user_module` なら何もしない。
  - `check.cpp` は、項目の定義を `PredKey{from, from_name, arity}` で確かめる。
- Consumes: タスク 4 のゴールの `use_module`（`rx_clash` の衝突のテスト）。

- [ ] **Step 1: 失敗するテストを書く。** `modules.pl` は `:- use_module('modules/rx_all.pl').`、`rx_some.pl`、`rx_except.pl` の順に読む。`rx_except` は `rb_a/1` と `===>` を 2 つ目の道で取り込むので、この読み込みが通ることが衝突でないことの確かめになる。
  - `rx_call`: `rb_a(X)` は `solutions([X = 1])`
  - `rx_call_b`: `rb_b(X)` は `solutions([X = 2])`
  - `rx_own`: `ra_own(X)` は `solutions([X = 0])`
  - `rx_imported_from`: `predicate_property(rb_a(_), imported_from(M))` は `solutions([M = rx_base])`
  - `rx_alias`: `rs_a(X)` は `solutions([X = 1])`
  - `rx_alias_from`: `predicate_property(rs_a(_), imported_from(M))` は `solutions([M = rx_base])`
  - `rx_op`: `(X = (a ===> b), X =.. L)` は `solutions([L = [(===>), a, b]])`
  - `rx_conflict`: `use_module('tests/pl/modules/rx_clash.pl')` は `fail`
  - `rx_plain`: `(consult('tests/pl/modules/rx_plain.pl'), 残高(X))` は `solutions([X = 100])`
  - `rx_goal`: `reexport(rx_base)` は `error(existence_error(procedure, reexport/1))`
  - `rx_goal_2`: `reexport(rx_base, [])` は `error(existence_error(procedure, reexport/2))`
  - `japanese.pl` は `:- use_module('modules/窓口.pl').` を読む。`再公開_別名` は `照会(X)` で `solutions([X = 100])`。`再公開_取り込み元` は `predicate_property(照会(_), imported_from(M))` で `solutions([M = 元帳])`。
  - standalone の CASE `reexport`（源は `uses_rx.pl rx_lib.pl rx_mid.pl`）: build が通り（`export_warnings` で止まらない）、未定義の呼び出しの警告が無く、出力が `42\n`。
- [ ] **Step 2: 赤を確かめる。** `./build/mosatest tests/pl/modules.pl` は `rx_all.pl` の読み込みで止まる（`reexport` は受けない指令）。`ctest -R standalone_reexport` も落ちる。
- [ ] **Step 3: 実装する（上の Interfaces どおり）。** 型を変えるので、ほかの使い手はコンパイルの誤りでたどる。`exported_by` は `from == predicate.module && from_name == predicate.name` で見る。
- [ ] **Step 4: 緑を確かめる。** `./build/mosatest tests/pl/modules.pl tests/pl/japanese.pl` と `ctest --preset macos-clang` が全部通る。`ctest -L diff` も通るので、`docs/agreement.*` を作り直す。スイートの 2 つの欄が下がらない（`lgt_libs.cpp` を触るので）。
- [ ] **Step 5: Commit.** `[phase12] reexport/1,2 を入れ、公開の項目に定義したモジュールとそこでの名前を持たせる（ADR 0026。modules.pl と japanese.pl の新しい行、standalone_reexport、ctest -L diff 0 差分）`

---

### Task 7: `strip_module/3` と `context_module/1`

**Files:**
- Create: `src/builtins/module_goal.cpp`（`run_module_goal`。頭のコメントは Global Constraints のとおり）。`CMakeLists.txt` の源に足す。
- Create: `tests/pl/modules/ctx.pl`: `:- module(ctx, [ctx_call/1, ctx_here/1, ctx_strip/2]).`、`:- meta_predicate(ctx_call(0)).`、`ctx_call(G) :- call(G).`、`ctx_here(M) :- context_module(M).`、`ctx_strip(T, M-P) :- strip_module(T, M, P).`
- Modify: `src/machine/builtin_api.hpp`（`run_module_goal` の宣言）、`src/machine/engine.cpp`（`kBuiltins` に 2 つ、`run_builtin` の並びに `run_module_goal`）、`tests/pl/modules.pl`、マニュアル第14章（日英）

**Interfaces:**
- Produces: `[[nodiscard]] bool run_module_goal(Impl& machine, const Goal& goal);` タスク 8 が同じファイルに足す。
- `strip_module/3` は `M:` を、`M` が原子である間だけ外す。外し終えたら `Module` と `Plain` を単一化する（`M:` が無ければ `goal.module`）。誤りは無い。`context_module/1` は `goal.module` の原子と単一化する。どちらも決定的にする。

- [ ] **Step 1: 失敗するテストを書く（`modules.pl`、`:- use_module('modules/ctx.pl').` の後）。**
  - `sm_nested`: `strip_module(a:b:c(x), M, P)` は `solutions([(M = b, P = c(x))])`
  - `sm_plain`: `strip_module(foo, M, P)` は `solutions([(M = user, P = foo)])`
  - `sm_var_module`: `(strip_module(X:foo, M, P), P == X:foo)` は `solutions([M = user])`
  - `sm_number_module`: `strip_module(1:foo, M, P)` は `solutions([(M = user, P = 1:foo)])`
  - `sm_var_term`: `(strip_module(T, M, P), P == T)` は `solutions([M = user])`
  - `sm_check`: `strip_module(a:b, a, P)` は `solutions([P = b])`
  - `sm_check_fail`: `strip_module(a:b, c, _)` は `fail`
  - `sm_in_module`: `ctx_strip(x, R)` は `solutions([R = ctx-x])`
  - `sm_in_module_qualified`: `ctx_strip(q:x, R)` は `solutions([R = q-x])`
  - `cm_user`: `context_module(M)` は `solutions([M = user])`
  - `cm_module`: `ctx_here(M)` は `solutions([M = ctx])`
  - `cm_meta`: `ctx_call(context_module(M))` は `solutions([M = user])`
  - `cm_qualified`: `ctx:context_module(M)` は `solutions([M = ctx])`
  - `sm_static`: `assertz(strip_module(a, b, c))` は `error(permission_error(modify, static_procedure, strip_module/3))`
  - `cm_static`: `assertz(context_module(a))` は `error(permission_error(modify, static_procedure, context_module/1))`
- [ ] **Step 2: 赤を確かめる。** 新しい行が `existence_error` で落ちる。
- [ ] **Step 3: 実装する。** 第14章の表に 2 行を足す（日英）。
- [ ] **Step 4: 緑を確かめる。** `./build/mosatest tests/pl/modules.pl`、`--manual --all-tables`、`ctest --preset macos-clang` が全部通る。`ctest -L diff` の差分が 0（Review Focus 5）なので、`docs/agreement.*` を作り直す。
- [ ] **Step 5: Commit.** `[phase12] strip_module/3 と context_module/1 を入れる（modules.pl の新しい 15 行、manual 0 failed、ctest -L diff 0 差分）`

---

### Task 8: `module_property/2` と `import/1`

**Files:**
- Create: `tests/pl/modules/imp_ctx.pl`: `:- module(imp_ctx, []).`、`rb_a(local).`
- Modify: `src/builtins/module_goal.cpp`、`src/machine/engine.cpp`（`kBuiltins` に 2 つ）、`tests/pl/modules.pl`、`tests/pl/japanese.pl`、マニュアル第14章（日英）

**Interfaces:**
- Consumes: `Export`、`add_reexports` の結果（タスク 6）、`run_module_goal`（タスク 7）、`apply_imports`（`module.hpp`）。
- `module_property/2` の性質は `file(F)`、`exports(L)`、`exported_operators(L)` の順に出す。`file` と `exports` は `Module::file` が空でないモジュールだけ。`exported_operators` は `export_ops` が空でないときだけ出す。`L` の並びは公開した順にする。`M` が変数なら `current_module/1` の順にたどる。候補は `module_prop.cpp` の `offer` と同じく選言にする。
- `import/1` は「設計との違い」5 のとおりに `M:PI` を読む。`PI` は `read_indicator` で読む（`as` は受けない）。`apply_imports(machine, M, goal.module, ImportSpec{Only, {item}})` を呼び、Formal は投げる。

- [ ] **Step 1: 失敗するテストを書く（`modules.pl`、`:- consult('modules/imp_ctx.pl').` の後）。**
  - `module_property/2`:
    - `mp_file`: `(module_property(rx_base, file(F)), sub_atom(F, _, _, 0, 'rx_base.pl'))` は `true`
    - `mp_exports`: `module_property(rx_all, exports(L))` は `solutions([L = [ra_own/1, rb_a/1, rb_b/1, (===>)/2]])`
    - `mp_exports_alias`: `module_property(rx_some, exports(L))` は `solutions([L = [rs_a/1]])`
    - `mp_exports_except`: `module_property(rx_except, exports(L))` は `solutions([L = [rb_a/1, (===>)/2]])`
    - `mp_ops`: `module_property(rx_base, exported_operators(L))` は `solutions([L = [op(700, xfx, ===>)]])`
    - `mp_ops_reexported`: `module_property(rx_all, exported_operators(L))` は同じ値
    - `mp_ops_none`: `module_property(rx_some, exported_operators(_))` は `fail`
    - `mp_order`: `(findall(P, module_property(rx_some, P), [file(_), exports([rs_a/1])]))` は `true`
    - `mp_user`: `module_property(user, _)` は `fail`
    - `mp_system`: `module_property(system, _)` は `fail`
    - `mp_enum`: `findall(M, (module_property(M, exports(_)), (M == rx_all ; M == user)), Ms)` は `solutions([Ms = [rx_all]])`（`user` は `lists` を取り込まないので、ライブラリの述語を使わない）
  - `module_property/2` の誤り:
    - `mp_type`: `module_property(1, _)` は `error(type_error(atom, 1))`
    - `mp_domain`: `module_property(user, foo)` は `error(domain_error(module_property, foo))`
    - `mp_domain_first`: `module_property(nomod, foo)` は `error(domain_error(module_property, foo))`
    - `mp_no_module`: `module_property(nomod, _)` は `error(existence_error(module, nomod))`
  - `import/1`:
    - `imp_basic`: `(imp_ctx:import(rx_base:rb_b/1), imp_ctx:rb_b(X))` は `solutions([X = 2])`
    - `imp_reexported`: `(imp_ctx:import(rx_some:rs_a/1), predicate_property(imp_ctx:rs_a(_), imported_from(M)))` は `solutions([M = rx_base])`
    - `imp_nested`: `imp_ctx:import(foo:rx_base:rb_b/1)` は `true`
    - `imp_unqualified`: `import(rb_a/1)` は `true`
  - `import/1` の誤り:
    - `imp_var`: `import(_)` は `error(instantiation_error)`
    - `imp_module_var`: `import(_:p/1)` は `error(instantiation_error)`
    - `imp_module_type`: `import(1:p/1)` は `error(type_error(atom, 1))`
    - `imp_no_module`: `import(nomod:p/1)` は `error(existence_error(module, nomod))`
    - `imp_pi_var`: `import(rx_base:_)` は `error(instantiation_error)`
    - `imp_pi_type`: `import(rx_base:foo)` は `error(type_error(predicate_indicator, foo))`
    - `imp_as`: `import(rx_base:(rb_a/1 as x))` は `error(type_error(predicate_indicator, rb_a/1 as x))`
    - `imp_not_exported`: `import(rx_base:nope/0)` は `error(existence_error(procedure, rx_base:nope/0))`
    - `imp_conflict`: `imp_ctx:import(rx_base:rb_a/1)` は `error(permission_error(import_into, procedure, rx_base:rb_a/1))`
  - 静的: `mp_static` と `imp_static`（`assertz` が `permission_error(modify, static_procedure, PI)`）
  - `japanese.pl`: `再公開_公開` は `module_property(窓口, exports(L))` で `solutions([L = [照会/1]])`。
- [ ] **Step 2: 赤を確かめる。** 新しい行が `existence_error` で落ちる。
- [ ] **Step 3: 実装する。** 第14章の表に 2 行を足す（日英）。
- [ ] **Step 4: 緑を確かめる。** `./build/mosatest tests/pl/modules.pl tests/pl/japanese.pl`、`--manual --all-tables`、`ctest --preset macos-clang` が全部通る。`ctest -L diff` の差分が 0 なので、`docs/agreement.*` を作り直す。
- [ ] **Step 5: Commit.** `[phase12] module_property/2 と import/1 を入れる（modules.pl と japanese.pl の新しい行、manual 0 failed、ctest -L diff 0 差分）`

---

### Task 9: 文書を閉じ、完了条件を確かめる

**Files:**
- Modify: `CHANGELOG.md` と `.ja.md`（`## Unreleased`）、`README.md` と `.ja.md`（状況の文）、`docs/iso-notes.md` と `.ja.md`（要るところだけ）、`docs/manual/{en,ja}/12-modules-libraries.md`（`reexport` と内省の述語の短い節）、`docs/STATUS.md`

- [ ] **Step 1: CHANGELOG を書く（日英）。** 書くことは次のとおり。
  - 新しい述語と `reexport/1,2`。
  - ゴールの読み込みの失敗の知らせ方。
  - 新しく `system` に入る 6 つの名前。利用者がこの名前の節を書くと `permission_error(modify, static_procedure, PI)` になる（振る舞いの変化）。
  - `mosac` の同梱が増えること。
  - 既知の制限: 診断は `user_error` の流れの桁を進めない。`module_property(M, file(F))` の `F` は、`mosac` の実行ファイルでは同梱の鍵になる。
- [ ] **Step 2: マニュアル第12章と README を直す（日英）。** 第12章には `reexport` と内省の述語の短い節を足す。README は状況の文を直す。
- [ ] **Step 3: 完了条件を流す。** 次がすべて通ること。
  - `./build/mosatest tests/pl/modules.pl tests/pl/japanese.pl`
  - `ctest --preset macos-clang -L diff` が 0 差分
  - `ctest --preset macos-clang -R 'standalone_goal_use|standalone_reexport'`
  - `ctest --preset macos-clang -R '^session$'`
  - `./build/mosatest --suite tests/conformance` の ISO 欄が 98.4% 以上、ライブラリ欄が 98.0% 以上
  - `./build/mosatest --manual docs/manual --all-tables` が 0 failed
  - `ctest --preset macos-clang` の全体
- [ ] **Step 4: Commit.** `[phase12] CHANGELOG・README・マニュアル第12章・STATUS にフェーズ12を書く（完了条件 1〜5 を macOS で確かめた）`
- [ ] **Step 5: CI。** push して CI の全ジョブが 3 OS で通ることを確かめる（push は所有者の依頼があってから）。そのあと、フェーズを閉じるかを所有者に問う。
