# フェーズ13 条件付きコンパイルとライブラリの追加 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `:- if(Goal)`、`:- elif(Goal)`、`:- else`、`:- endif` を、ファイルを読むすべての道で使えるようにする。`mosac` はビルド時にも条件を評価し、実行ファイルは起動時に評価し直して、`mosa` と同じ結果を出す。`library(pairs)`、`library(assoc)`、`library(ordsets)` を Prolog で書いて `lib/` に置く。suite の `directives/conditional_compilation` の 39 件をライブラリ欄で数えて通す。

**Architecture:**
- 条件の節の状態は、`read_terms` の 1 回に 1 つ作る `CondFrames`（新しい `src/machine/load_cond.cpp`）が持つ。`read_terms` は項を読むたびに、`take_term` より前に `CondFrames::step` を呼ぶ。条件の指令はそこで扱い、読み飛ばし中の項は捨てる。だから `term_hook`（`test/3` の横取り）にも文法規則の変換にも届かない。
- 条件は `run_condition` が、一般の指令と同じ `run_parked` で読み込み中のモジュールで 1 回解く。翻訳の読み込み（`machine.translation` がある）では、例外と `halt` を「取らない」に変え、診断と `halt_now` を消す（ADR 0027）。
- `mosac` は 3 つの印で、翻訳の読み込みのほうが多く読む場合を扱う。`TranslationLog::File` の `untaken_branch`（公開の未定義を警告にとどめる）、`in_branch`（`startup_modules` から外す）、読み飛ばした指令の `skipped_goal` である。根でない単位は新しい `rt::install_loaded_unit` で付け、起動時に読まれていなければ読まない。
- ライブラリは新しく書く。`lib/*.pl` は CMake の glob（`CMakeLists.txt:234`）で翻訳済みの単位になるので、CMake は直さない。

**Tech Stack:** C++20 標準ライブラリ、CMake 3.25。テストは `mosatest`（`tests/pl` の `test(Name, Goal, Expect)` 行）と `tests/cpp/harness.hpp`（`mosa_test::check`）。`mosac` の単独実行のテストは `tests/mosac/standalone_test.cmake`。suite は `mosatest --suite tests/conformance`。

**Spec:** `docs/superpowers/specs/2026-11-16-phase13-conditional-compilation-libraries-design.md`。所有者が 2026-11-16 に実装計画を頼んだので、設計の承認とみなした。ADR 0027 も、同じ依頼で承認されたものとして扱う（これまでのフェーズと同じ。ADR の Status にそう書く）。問い 1〜3 は所有者が 3 つとも A に決めた。

## 設計との違い（計画を書くときに決めた）

1. 構造の誤り（対の無い `elif`・`else`・`endif`、`else` の後ろの `elif`、2 つ目の `else`）は、読み飛ばし中の入れ子の節の中でも読み込みを失敗させる。設計は書いていない。条件の値によって閉じ方の誤りが見えたり見えなかったりすると、別の機械で初めて壊れるからである。
2. 根でない単位は、すべて新しい `rt::install_loaded_unit` で付ける（設計は「全部か、分岐の中で初めて読んだ物だけか」を任せた）。根の単位は今の `rt::install_unit` のままにする。今の実行ファイルでは、根の読み込みが成功すれば根でない単位のファイルはどれも読み終えているので、振る舞いは変わらない。
3. `reexport(library(N))` と `reexport(library(N), _)` は、`use_module` と同じく全部の項から拾う（設計は範囲を任せた）。指令の `reexport` は今も翻訳の読み込みが辿るので、拾っても同梱は増えない。
4. 「分岐の中で初めて読んだ」は、ファイルを開いた時に、読んでいるテキストのどれかにまだ閉じていない `if` があることとする（`TranslationLog::open_conditions > 0`）。ファイルは取った分岐の中でしか開かれないので、これで足りる。
5. 「取らなかった分岐を持つファイル」は、条件で読み飛ばした項が 1 つでもあるファイルとする。空の分岐は数えない。`skipped_goal` と同じく `~FileNote` で外のファイルにも移す。include の外のファイルの公開も警告になりうるが、ビルドを止めるか警告かが違うだけで、実行の結果は変わらない。
6. 完了条件 2 の後半（分岐が同じなら取った分岐の述語だけを訳す）は、`mosac emit` を走らせる CMake の台本ではなく、`codegen_test` で `translate_program` の出力を見て確かめる。`mosac emit` が呼ぶのと同じ関数で、C++ のビルドが要らない。
7. 条件付きコンパイルの診断（対の誤りの字面、条件の誤りの Formal、`halt` の終了コード）は、新しい `tests/cpp/cond_compile_test.cpp` で確かめる。`tests/pl` の行では「読み込みが失敗する」ことしか見られない。
8. `cond_export` の単独実行のテストでは、台本の「未定義の呼び出しの警告があれば落とす」検査を外し、代わりに公開の警告があることを求める。取らなかった分岐だけが定義する述語の呼び出しは警告になる（設計の既知の制限 3）。今の 10 件の検査は変えない。
9. 設計のリスクの節が任せた `--verbose` での知らせ（起動時に分岐が違って訳したコードが付かない）は、足さない。

## Global Constraints

- 依存は C++20 標準ライブラリと CMake 3.25 以上だけにする。`-Wall -Wextra -Wpedantic`（MSVC は `/W4`）の警告はエラーとして扱う。3 OS（macOS、Linux の GCC と Clang、Windows の MSVC）で通す。
- 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロ、コンパイラ拡張を使わない。関数は 60 行以内、ファイルは 800 行以内にする。`src/machine/load.cpp`（796）にはタスク 2 で分けてから足す。`src/machine/internal.hpp`（767）には `TranslationLog` の 3 つの欄だけを足す。`src/machine/load_directive.cpp`（713）には、タスク 3 の 2 つの宣言の移動と括り出しだけを入れる。`tests/cpp/session_test.cpp`（796）には足さない。
- エラーは `throw_error`/`make_*_error` の族と今の `formal_*` の助け手だけで作る。アトムの名前を文字列で直に書かない。ライブラリの Prolog の誤りは、`lib/between.pl` と同じく `throw(error(Formal, context(Name/Arity, _)))` で投げる。
- ライブラリのファイルの頭のコメントは `lib/strings.pl` の形にする。SPEC 第5節の表を指し、テストのファイル名と、名前と誤りを合わせた SWI-Prolog のマニュアルの版（10.1.20、2026-11-16 に読んだ）を書く。SWI-Prolog のソースを写さない。
- テストを先に書き、赤を確かめてから緑にする。エラーのテストは `Formal` だけを比べる。期待値を実装に合わせて変えない。例外は設計のアンチ目標の 2 つ（`conformance_scope_test.cpp` の範囲の変更と、`lib/` のライブラリの一覧の 2 か所）だけで、どちらも範囲を変える、またはライブラリを足す同じコミットで直す。
- `tests/pl` の相対パスは、ゴールではリポジトリの根（ctest の作業ディレクトリ）から、指令では `tests/pl` から開く。読み込み先の固定のファイルは `tests/pl/fixtures/cond_*.pl` に置く。テストが書き出すファイルは `tests/tmp/` の下に置く。
- `tests/pl` の行を足すか変えるコミットでは、`ctest --preset macos-clang -L diff` を流す。`docs/agreement.md` と `docs/agreement.json` を同じコミットに入れる。タスク 3 と 5 では `./build/mosatest --suite tests/conformance` も流し、結果が変われば `docs/conformance.md`、`docs/conformance.json`、`docs/conformance-library.json` を入れる。本欄 98.4%、ライブラリ欄 98.0% を下回らず、本欄の分母は 2326 のまま。
- 決定は変えない。条件の指令になるのは `if/1`、`elif/1`、`else/0`、`endif/0` の 4 つの形だけで、`reserved_directive` には足さない。`:- if(C, T, E)` は今と同じく一般の指令として組み込みの `if/3` を呼ぶ。`:- module/2` は今と同じくファイルの最初の項でなければならない。翻訳の読み込みが呼ぶ利用者のゴールは、条件だけにする。
- 識別子は英語、コメントとコミットメッセージは日本語で書く。文書は英語が正で、`*.ja.md` を同じコミットで直す。`docs/STATUS.md` は英語だけにする。
- コミットは論理単位ごとに 1 つ。メッセージは 1 行で、`[phase13] 内容（確認方法）` の形にする。
- 1 セッション 1 タスク。始めに `docs/STATUS.md` の「In progress」にタスクを書く。終わりに `cmake --build --preset macos-clang && ctest --preset macos-clang` を通し、STATUS を直す。
- レビュアーは opus を使う。

## Review Focus

1. **条件の選択点と束縛。** `:- if(((X = 1 ; X = 2), assertz(cond_seen(X)))).` は 1 回だけ解き、`cond_seen/1` は `1` だけを持つ。束縛は後ろの項に残らない。タスク 3 の `cond_once` で確かめる。
2. **読み飛ばした旗の指令。** 読み飛ばした `:- set_prolog_flag(double_quotes, atom).` は効かず、後ろの `"ab"` は今の既定どおり符号のリストのまま読まれる。タスク 3 の `cond_skip_flag` で確かめる。
3. **分岐が変わった読み直し。** 1 回目に取った分岐だけが定義した述語は、2 回目に別の分岐を取って読み直すと消える（ADR 0020 の「前に定義して今は定義しない述語」）。タスク 3 の `cond_reload_on` と `cond_reload_off` で確かめる。
4. **条件の中の `halt`。** `mosa` では、一般の指令と同じく読み込みがその場で止まり、終了コードが残る。`mosac build` は止まらず、後ろの節を訳す。タスク 3 の `check_cond_halt` と、タスク 4 の `check_cond_translation` の `cond_halt.pl` で確かめる。
5. **方言の分岐。** `:- if(current_prolog_flag(dialect, swi)). :- use_module(library(clpfd)). … :- else. … :- endif.` は、`mosa` では else の分岐を取る（`dialect` は `mosa`）。`mosac build` は、無い `library(clpfd)` で止まらない。タスク 3 の `cond_dialect` と、タスク 4 の `check_cond_translation` の `cond_dialect.pl` で確かめる。

---

### Task 1: フェーズを開く（文書だけ）

**Files:**
- Modify: `docs/SPEC.md` と `.ja.md`（第3節の指令の項目、第5節のライブラリの表に 3 行、第6節の読み込みと `mosac` の項、第9節の表に 13 行目）
- Modify: `docs/iso-notes.md` と `.ja.md`
- Create: `docs/adr/0027-conditional-compilation.md` と `.ja.md`。Modify: `docs/adr/README.md` と `.ja.md`（索引に 1 行）
- Modify: `README.md` と `.ja.md`（バッジ `phases-0--12_done-green` を `phase-13_in_progress-yellow` に、フェーズの表に 13 行目を `In progress` で）
- Modify: `docs/STATUS.md`

- [ ] **Step 1: SPEC を書く（日英）。**
  - 第3節: ISO 外の指令として `if/1`、`elif/1`、`else/0`、`endif/0` を足す。意味は設計の「条件付きコンパイルの意味」の表と箇条（読み飛ばす部分の扱い、構文の誤りを黙って読み飛ばすこと、ファイルごとの対、4 つの形だけであること）。
  - 第5節: ライブラリの表に `library(pairs)`、`library(assoc)`、`library(ordsets)` の 3 行。述語は設計の表のとおり。
  - 第6節: 翻訳の読み込みは条件を呼ぶ（翻訳が呼ぶ唯一の利用者のゴール。例外と halt は取らない分岐）。実行ファイルは起動時に評価し直す。ずれの扱い（設計の「`mosac`」の節の 4 つ）と既知の制限 3 つ。
  - 第9節の 13 行目: 完了条件は設計の「完了条件」1〜5 を写す。ただし 2 の後半は「`translate_program` の出力が取った分岐の述語だけを訳す（ctest の `codegen`）」にする（「設計との違い」6）。
- [ ] **Step 2: iso-notes を書く（日英）。**
  - 新しい節「Conditional compilation（13211-1 に無い）」に、問い 1〜3 と決定（3 つとも A、2026-11-16、所有者）を書く。
  - 同じ節に、条件の例外で読み込みを失敗させる理由（SWI-Prolog 4.3.1.2 は表示して偽とする。Mosa は iso-notes 7.4.2 の一般の指令に揃える）と、構造の誤りを読み飛ばし中でも失敗にすること（「設計との違い」1）を書く。
  - 新しい節「library(assoc) `del_min_assoc/4` and `del_max_assoc/4` on an empty assoc」に、空では失敗にする理由を書く。
- [ ] **Step 3: ADR 0027 を書く（日英）。** Context、Decision、Rejected alternatives、Consequences を各 3 行以内にする。中身は設計の「ADR 0027」の節のとおり。Status は `Approved (2026-11-16, owner; the owner's request for the phase 13 plan counts as approval)`。
- [ ] **Step 4: README と STATUS を直す。** STATUS の 1 行目は「Phases 0–12 done, phase 13 open (conditional compilation and libraries, opened 2026-11-16, owner decisions A, A, A)」で始める。
- [ ] **Step 5: 確かめる。** `./build/mosatest --manual docs/manual --all-tables` が 0 failed。`grep -n 'phase-13\|phase 13' README.md README.ja.md docs/STATUS.md` が 3 ファイルに当たる。
- [ ] **Step 6: Commit.** `[phase13] フェーズ13を開き SPEC 第3・5・6節と第9節の 13 行目、iso-notes の問い 3 つ、ADR 0027 を日英で書く（文書だけ、manual 0 failed）`

---

### Task 2: `load.cpp` から翻訳の書き留めを分ける（振る舞いは変えない）

**Files:**
- Create: `src/machine/load_note.cpp`。`load.cpp` の `note_site`、`mark_opaque`、`struct FileNote`（436〜495 行、コメントごと）を移す。`CMakeLists.txt` の `mosa_lib` の源に足す。
- Modify: `src/machine/load_internal.hpp`（3 つの宣言。頭のコメントの「load.cpp と load_directive.cpp だけが共有する」を、共有するファイルの並びに直す）

**Interfaces:**
- Produces（`load_internal.hpp`）:
  - `void note_site(Impl& machine, PredKey key, std::size_t line);`
  - `[[nodiscard]] bool mark_opaque(TranslationLog* log);`
  - `struct FileNote`。欄（`log`、`outer`）と、`tolerate_syntax()`、`first_term(const ReadTerm&)` の宣言をヘッダに置き、コンストラクタ、デストラクタ、2 つの関数の本体を `load_note.cpp` に置く。タスク 4 はこのファイルの `FileNote` に印を足す。

- [ ] **Step 1: 移す前の件数を控える。** `ctest --preset macos-clang -N | tail -1`
- [ ] **Step 2: 移す。** 文とコメントは変えない。`load_note.cpp` の頭のコメントに、分けた理由（800 行の上限と、タスク 3 で `read_terms` に足す分）を書く。
- [ ] **Step 3: 確かめる。** ビルドに警告が無い。ctest の件数が同じで、全部通る。`wc -l src/machine/load.cpp` が 745 以下になる。
- [ ] **Step 4: Commit.** `[phase13] load.cpp から翻訳の書き留め（note_site、mark_opaque、FileNote）を load_note.cpp に分ける（振る舞いは変えない。ctest 全件）`

---

### Task 3: 条件付きコンパイルをローダーに入れる

**Files:**
- Create: `src/machine/load_cond.cpp`（`CondFrames` と `run_condition`）。`CMakeLists.txt` の `mosa_lib` の源に足す。
- Modify: `src/machine/load_internal.hpp`（下の宣言）、`src/machine/load_directive.cpp`（`run_parked` を無名の名前空間から出す。`run_directive_goal` の `M:Goal` の組み立てを `in_load_module` に括り出す）、`src/machine/load.cpp`（`read_terms` だけ）
- Modify: `lib/messages_ja.pl`、`lib/messages_en.pl`（3 つの鍵）
- Create: `tests/pl/cond_compile.pl`、`tests/pl/fixtures/cond_*.pl`（下の一覧）、`tests/cpp/cond_compile_test.cpp`。Modify: `tests/tests.cmake`（`load_throw_test` と同じ形で `cond_compile_test` を登録。名前は `cond_compile`）、`tests/pl/japanese.pl`

**Interfaces:**
- Consumes: タスク 2 の `load_internal.hpp`。
- Produces（`load_internal.hpp`、定義は `load_cond.cpp`。2 つ目と 3 つ目は `load_directive.cpp`）:
  - `[[nodiscard]] int run_condition(Impl& machine, Cell goal);` 1 は真、0 は偽、-1 は読み込みの失敗（診断は書いた）。`in_load_module` で包んで `run_parked` で解く。`Exception` は -1（`run_aside` が Formal の診断を書く）。`Halt` は `machine.load_kept = true` にして -1（`run_directive_goal` と同じ）。翻訳の読み込みの扱いはタスク 4 で足す。
  - `[[nodiscard]] StopKind run_parked(Impl& machine, Cell goal);`（今の定義を名前空間の外へ）
  - `[[nodiscard]] Cell in_load_module(Impl& machine, Cell goal);` 読み込み中のモジュールが `user` でなければ `M:Goal` を返す。
  - `class CondFrames`:
    - `explicit CondFrames(Impl& machine);`
    - `[[nodiscard]] int step(Cell term);` `term` が `(:-)/1` で中身が `if/1`、`elif/1`、`else/0`、`endif/0` のどれかなら扱って 1 か -1。そうでなく読み飛ばし中なら項を捨てて 1。どちらでもなければ 0（`take_term` へ渡す）。読み飛ばし中の `if` は、条件を呼ばずに節を積む。
    - `[[nodiscard]] bool skipping() const;`
    - `[[nodiscard]] bool close();` テキストの終わり。閉じない `if` があれば `cond_unclosed` の診断を書いて `false`。
  - `read_terms` の変更は 3 か所だけにする。構文の誤りは `skipping()` なら診断を出さずに今の読み飛ばしの道へ進む（`note.tolerate_syntax()` より先に見て、印を付けない）。テキストの終わりでは `scope.close` の前に `close()`。`take_term` の前に `step()`。`first` は捨てた項と条件の指令の後でも `false` にする。
  - メッセージの鍵（日 / 英）。`~w` は指令を `write_term` で書いた字面（`:-endif` など）。
    - `cond_unmatched`: `'対応する if が無い: ~w'` / `'no matching if: ~w'`
    - `cond_after_else`: `'else の後ろには置けない: ~w'` / `'not allowed after else: ~w'`
    - `cond_unclosed`: `'endif で閉じていない if がある'` / `'if without a matching endif'`

- [ ] **Step 1: 読み込み先を作る（`tests/pl/fixtures/`）。** 各ファイルの中身は次のとおり（`\n` は改行）。
  - 失敗するもの: `cond_stray_endif.pl` は `a.\n:- endif.\n`。`cond_stray_else.pl` は `:- else.\n`。`cond_stray_elif.pl` は `:- elif(true).\n`。`cond_elif_after_else.pl` は `:- if(fail).\n:- else.\n:- elif(true).\n:- endif.\n`。`cond_else_twice.pl` は `:- if(true).\n:- else.\n:- else.\n:- endif.\n`。`cond_unclosed.pl` は `:- if(true).\na.\n`。`cond_open_part.pl` は `:- if(true).\n`。`cond_unclosed_include.pl` は `:- include('cond_open_part.pl').\n:- endif.\n`。`cond_endif_part.pl` は `:- endif.\n`。`cond_endif_in_include.pl` は `:- if(true).\n:- include('cond_endif_part.pl').\n`。`cond_throw.pl` は `:- if(throw(cond_ball)).\n:- endif.\n`。`cond_var.pl` は `:- if(X).\n:- endif.\n`。`cond_number.pl` は `:- if(1).\n:- endif.\n`。`cond_taken_syntax.pl` は `:- if(true).\ncond_t(1r3).\n:- endif.\n`。`cond_swallowed_endif.pl` は `:- if(fail).\ncond_s(1r3 :- endif.\n`。`cond_skipped_nested_bad.pl` は `:- if(fail).\n:- if(true).\n:- else.\n:- else.\n:- endif.\n:- endif.\n`（「設計との違い」1）。
  - 読めるもの: `cond_module.pl` は `:- module(cond_module, [cond_where/1]).\ncond_here.\n:- if(cond_here).\ncond_where(module).\n:- else.\ncond_where(user).\n:- endif.\n`。`cond_reload.pl` は `:- if(cond_switch).\ncond_only_on.\n:- endif.\ncond_always.\n`。
- [ ] **Step 2: `tests/pl/cond_compile.pl` を書く。** 頭に `:- dynamic(cond_seen/1).`、`:- dynamic(cond_called/1).`、`:- dynamic(cond_switch/0).` を置き、テキストの中の条件の塊と行を次のとおりにする。
  - 組み合わせ: `:- if(true). cond_pick(if). :- else. cond_pick(else). :- endif.`、`:- if(fail). cond_pick2(if). :- elif(true). cond_pick2(elif). :- else. cond_pick2(else). :- endif.`、`:- if(fail). cond_pick3(if). :- elif(fail). cond_pick3(elif). :- else. cond_pick3(else). :- endif.`、`:- if(fail). cond_pick4(if). :- endif.`。入れ子は `:- if(true). :- if(fail). cond_nest(inner_if). :- else. cond_nest(inner_else). :- endif. :- else. :- if(true). cond_nest(outer_else). :- endif. :- endif.`。各指令と節は 1 行に 1 つ書く。
    - `cond_if_then`: `findall(X, cond_pick(X), L)` は `solutions([L = [if]])`
    - `cond_elif_taken`: `cond_pick2` で `solutions([L = [elif]])`
    - `cond_else_taken`: `cond_pick3` で `solutions([L = [else]])`
    - `cond_none_taken`: `current_predicate(cond_pick4/1)` は `fail`
    - `cond_nested`: `cond_nest` で `solutions([L = [inner_else]])`
  - 呼ばない条件: `:- if(true). :- elif(assertz(cond_called(elif))). :- endif.` と `:- if(fail). :- if(assertz(cond_called(inner))). :- endif. :- endif.`
    - `cond_not_called`: `findall(X, cond_called(X), L)` は `solutions([L = []])`
  - 1 回だけ解く（Review Focus 1）: `:- if(((X = 1 ; X = 2), assertz(cond_seen(X)))). :- endif.`（`member/2` は組み込みに無いので `;` で書く）
    - `cond_once`: `findall(X, cond_seen(X), L)` は `solutions([L = [1]])`
  - 読み飛ばしは何も変えない: `:- if(fail).` の中に `:- module(cond_never, []).`（動かせばファイルの最初の項でないので失敗する）、`:- op(700, xfx, cond_op).`、`:- dynamic(cond_skip_dyn/1).`、`:- set_prolog_flag(double_quotes, atom).`、`:- include('no_such_cond_file.pl').`、`:- use_module(no_such_cond_module).`、`cond_skip_clause.`、`cond_skip_rule --> [a].`、`cond_bad_rule --> 1.`（今の `mosa` で `type_error(callable,1)` になる規則）、`test(cond_skipped_test, fail, true).` を並べ、`:- endif.` で閉じる。
    - `cond_skip_op`: `current_op(_, _, cond_op)` は `fail`
    - `cond_skip_dynamic`: `catch(cond_skip_dyn(_), error(existence_error(procedure, cond_skip_dyn/1), _), true)` は `true`
    - `cond_skip_flag`（Review Focus 2）: `(X = "ab", atom(X))` は `fail`
    - `cond_skip_clause`: `current_predicate(cond_skip_clause/0)` は `fail`
    - `cond_skip_rule`: `current_predicate(cond_skip_rule/2)` は `fail`
    - 横取りした `cond_skipped_test` が走れば失敗として数えられるので、それが無いことは件数で見る（Step 4）。
  - 構文の誤りの読み飛ばし（問い 3 の A）: `:- if(fail).`、`cond_third(1r3).`、`:- endif.`、`cond_after_syntax.`
    - `cond_skip_syntax`: `cond_after_syntax` は `true`
  - 方言（Review Focus 5）: `:- if(current_prolog_flag(dialect, swi)).`、`:- use_module(library(clpfd)).`、`cond_dialect(swi).`、`:- else.`、`cond_dialect(other).`、`:- endif.`
    - `cond_dialect`: `findall(X, cond_dialect(X), L)` は `solutions([L = [other]])`
  - 読み込み先（ゴールから。パスは `'tests/pl/fixtures/<名前>'`）:
    - 失敗の 14 件は、それぞれ `consult(…)` が `fail`。行の名前は読み込み先の名前から `.pl` を外したもの（`cond_stray_endif` など。`cond_open_part` と `cond_endif_part` は行にしない）。
    - `cond_module`: `(use_module('tests/pl/fixtures/cond_module.pl'), cond_where(X))` は `solutions([X = module])`
    - `cond_reload_on`（Review Focus 3）: `(assertz(cond_switch), consult('tests/pl/fixtures/cond_reload.pl'), cond_only_on)` は `true`
    - `cond_reload_off`: `(retractall(cond_switch), consult('tests/pl/fixtures/cond_reload.pl'), \+ current_predicate(cond_only_on/0), cond_always)` は `true`
  - `tests/pl/japanese.pl` に、`:- if(true).`、`条件の果物(りんご).`、`:- else.`、`条件の果物(みかん).`、`:- endif.` と、行 `cond_ja`: `findall(X, 条件の果物(X), L)` は `solutions([L = [りんご]])` を足す。
- [ ] **Step 3: `tests/cpp/cond_compile_test.cpp` を書く。** `Machine::load_program` の結果（`ok` と `message`）を見る。期待の字面は `mosa::message_text(impl, 鍵, {…})` で作り、言語に頼らない。
  - `check_cond_messages`: `":- endif.\n"` の `message` に `cond_unmatched` と `":-endif"` の字面。`":- if(true).\n:- else.\n:- elif(true).\n:- endif.\n"` は `cond_after_else` と `":-elif(true)"`。`":- if(true).\n:- else.\n:- else.\n:- endif.\n"` は `cond_after_else` と `":-else"`。`":- if(true).\n"` は `cond_unclosed`。
  - `check_cond_condition_errors`: `":- if(_).\n:- endif.\n"` の `message` に `instantiation_error`。`":- if(1).\n:- endif.\n"` に `type_error(callable,1)`。`":- if(throw(cond_ball)).\n:- endif.\n"` に `cond_ball`。どれも `ok` は偽。
  - `check_cond_halt`（Review Focus 4）: `":- if(halt(3)).\n:- endif.\ncond_after_halt.\n"` は `ok` が偽で、`engine_impl(*machine).halt_now` が真、`halt_code == 3`。
  - `check_cond_silent_skip`: `":- if(fail).\ncond_x(1r3).\n:- endif.\ncond_y.\n"` は `ok` が真で `message` が空。
  - `check_cond_if3`: `":- if(true, assertz(cond_if3_ran), true).\n"` は `ok` が真で、その後の `query("cond_if3_ran.")` が 1 つの解を返す（`:- if(C, T, E)` は一般の指令のまま）。
  - `check_cond_include_balance`: `tests/tmp/p13_open_part.pl` に `":- if(true).\n"` を書き、`":- include('<絶対パス>').\n:- endif.\n"` は `ok` が偽で `cond_unclosed` の字面。
- [ ] **Step 4: 赤を確かめる。** `./build/mosatest tests/pl/cond_compile.pl` はファイルの読み込みで落ちる（`existence_error(procedure,if/1)`）。`ctest --preset macos-clang -R '^cond_compile$'` が落ちる。
- [ ] **Step 5: 実装する（上の Interfaces どおり）。** 節の状態は 1 つの節につき「外が読み飛ばし中か」「この節で分岐を取ったか」「`else` を見たか」「今の分岐を読むか」を持てば足りる。読み飛ばし中の `elif` の条件と、読み飛ばし中に開いた `if` の条件は呼ばない。
- [ ] **Step 6: 緑を確かめる。**
  - `./build/mosatest tests/pl/cond_compile.pl tests/pl/japanese.pl` が全部通る。`cond_compile.pl` の件数が、書いた行の数と同じである（`cond_skipped_test` が横取りされていない）。
  - `ctest --preset macos-clang` が全部通る。`ctest --preset macos-clang -L diff` も通るので、`docs/agreement.*` を作り直す。`cond_compile.pl` の行が両側で同じ結果である（読み込み先は両側とも作業ディレクトリから読む。設計のリスクの節）。
  - suite の 2 つの欄が下がらない（39 件はまだ範囲の外）。
- [ ] **Step 7: Commit.** `[phase13] ローダーに :- if/elif/else/endif を入れる（問い 3 の A。cond_compile.pl と japanese.pl の新しい行、cond_compile の C++ の 6 件、ctest -L diff 0 差分）`

---

### Task 4: `mosac` が条件をビルド時にも評価する（問い 1 の A、ADR 0027）

**Files:**
- Modify: `src/machine/internal.hpp`（`TranslationLog` に 3 つの欄）、`src/machine/load_cond.cpp`（翻訳の読み込みの扱い）、`src/machine/load_note.cpp`（`FileNote` の 2 つの印）、`src/machine/goal_libraries.cpp`（`reexport` の 2 つの形）
- Modify: `include/mosa/runtime.hpp` と `src/machine/native.cpp`（`install_loaded_unit`）、`src/codegen/project_main.cpp`（`main_body`）、`src/codegen/project.cpp`（`startup_modules` と `export_warnings` の呼び方）、`src/codegen/check.cpp` と `src/codegen/project.hpp`（`export_warnings`）
- Create: `tests/mosac/cond_late_lib.pl`、`cond_build_mod.pl`、`cond_init_mod.pl`、`cond_skip_op.pl`、`cond_export.pl`、`cond_export_mod.pl`
- Modify: `tests/mosac/standalone_test.cmake`（4 つの CASE）、`tests/tests.cmake:595`（CASE の並び）、`tests/cpp/codegen_test.cpp`

**Interfaces:**
- Consumes: タスク 3 の `run_condition`、`CondFrames`。
- Produces:
  - `TranslationLog::File` に `bool untaken_branch = false;`（このファイルか、ここから開いたファイルで、条件が項を読み飛ばした）と `bool in_branch = false;`（開いた時に、読んでいるテキストのどれかに閉じていない `if` があった）。`TranslationLog` に `std::size_t open_conditions = 0;`。
  - `CondFrames` は、積んだ節の数を `open_conditions` に足し引きする（デストラクタで残りを引く。失敗で抜けても数が残らない）。読み飛ばした項ごとに `untaken_branch` を立て、`note_goal_libraries` に渡し、指令なら `skipped_goal` も立てる。
  - `run_condition` は、`machine.translation` があるとき、`Exception` と `Halt` を 0 にする。そのとき `machine.diagnostic` を空にし、`machine.halt_now` を偽に、`machine.halt_code` を 0 に戻す。`load_kept` は立てない。
  - `FileNote` は、新しい `File` を作るときに `in_branch = log->open_conditions > 0` とし、デストラクタで `untaken_branch` を `skipped_goal` と同じく外のファイルへ移す。
  - `note_goal_libraries` は `reexport(library(N))` と `reexport(library(N), _)` も拾う（「設計との違い」3）。
  - `[[nodiscard]] bool rt::install_loaded_unit(Machine& machine, const NativeUnit& unit);` 単位のファイルが埋め込みにあり、起動時の読み込みがまだ読んでいない（`Impl::prepared` に無い）なら、読まず、付けず、`true` を返す。それ以外は `install_unit` と同じ。`main_body` は `plan.units[0]` に `install_unit`、残りに `install_loaded_unit` を出す。`diff_main.cpp` の `load_unit` の道は変えない。
  - `translate_program` の `impl.startup_modules` は、`Module::file` が `log_file` で `in_branch` のファイルを指すモジュールを除いた一覧にする。
  - `std::vector<std::string> export_warnings(Impl& machine, const TranslationLog& log, const DisplayNames& names, bool untaken);` `untaken_branch == untaken` のファイルのモジュールだけを見る。`translate_program` は `false` の結果でビルドを止め、`true` の結果を `project.warnings` の前に足す（`any_opaque` でも足す）。

- [ ] **Step 1: 失敗するテストを書く。**
  - `codegen_test.cpp` の `check_cond_translation`。`tests/tmp/codegen_cond/` に書き、`translate_program` の `muser.cpp` を見る（`project_opaque` と同じ形）。
    - `cond_emit.pl`: `":- if(true).\nthenpred.\n:- else.\nelsepred.\n:- endif.\n"` は `ok` で、`has(text, "pthenpred_0")` かつ `!has(text, "pelsepred_0")`（完了条件 2 の後半）。
    - `cond_throw.pl`: `":- if(nope).\nskippedpred.\n:- endif.\nlaterpred.\n"` は `ok`、`diagnostic` が空、`platerpred_0` があり `pskippedpred_0` が無い。
    - `cond_halt.pl`（Review Focus 4）: `":- if(halt).\nhaltedpred.\n:- endif.\nafterhalt.\n"` は `ok` で `pafterhalt_0` がある。
    - `cond_dialect.pl`（Review Focus 5）: `":- if(current_prolog_flag(dialect, swi)).\n:- use_module(library(clpfd)).\n:- else.\nmosapred.\n:- endif.\n"` は `ok` で `pmosapred_0` がある。
    - `cond_syntax.pl`: `":- if(fail).\nx(1r3).\n:- endif.\nysyntax.\n"` は `ok` で `pysyntax_0` がある。
  - `check_cond_log`。`TranslationLog` を付けた機械で `load_program` に `":- if(fail).\n:- use_module(library(strings)).\n:- reexport(library(between)).\n:- endif.\n"` を渡す。`log.goal_libraries == {"strings", "between"}`、`log.open_conditions == 0`。
  - 単独実行の 4 つ（`tests/mosac/`）。どれも頭に `:- initialization(main, main).` を置く。期待の出力は `./build/mosa <ファイル>` の出力と同じであることを、書いた後に手で確かめる。
    - `cond_late_lib`（起動時に取る分岐だけがライブラリを読む）: `:- assertz(cond_flag).`、`:- if(cond_flag).`、`:- use_module(library(strings)).`、`:- endif.`、`main :- strings:upcase_atom(abc, X), write(X), nl.`。出力は `ABC\n`、標準エラーは空。
    - `cond_build_mod`（ビルド時に取る分岐だけがモジュールのファイルを読む）: `:- dynamic(cond_skip/0).`、`:- assertz(cond_skip).`、`:- if(\+ cond_skip).`、`:- use_module(cond_init_mod).`、`:- endif.`、`main :- catch(cond_init_mod:hello, error(E, _), (write(E), nl)), catch(hello, error(F, _), (write(F), nl)).`。`cond_init_mod.pl` は `:- module(cond_init_mod, [hello/0]).`、`:- initialization((write(init_ran), nl)).`、`hello :- write(hello), nl.`。出力は `existence_error(module,cond_init_mod)\nexistence_error(procedure,hello/0)\n`。
    - `cond_skip_op`（取らなかった分岐の `op/3` を後ろの項が使う）: `:- assertz(cond_op).`、`:- if(cond_op).`、`:- op(700, xfx, ===>).`、`:- endif.`、`r(a ===> b).`、`main :- r(X), write(X), nl.`。出力は `a===>b\n`。
    - `cond_export`（公開した述語を、取らなかった分岐だけが定義する）: `:- use_module(cond_export_mod).`、`main :- answer(X), write(X), nl.`。`cond_export_mod.pl` は `:- module(cond_export_mod, [answer/1]).`、`:- assertz(cond_mod_ready).`、`:- if(cond_mod_ready).`、`answer(42).`、`:- endif.`。出力は `42\n`。build の標準エラーに `answer/1` を含む行があることを求め、未定義の呼び出しの警告の検査はこの CASE だけ外す（「設計との違い」8）。
- [ ] **Step 2: 赤を確かめる。** `ctest --preset macos-clang -R 'codegen|standalone_cond'` が落ちる。`cond_late_lib` は同梱が無く読み込みが失敗する。`cond_build_mod` は `init_ran` と `hello` を書く。`cond_skip_op` と `cond_export` は build が止まる。`check_cond_translation` の `cond_throw.pl` と `cond_halt.pl` も翻訳が止まる。
- [ ] **Step 3: 実装する（上の Interfaces どおり）。**
- [ ] **Step 4: 緑を確かめる。**
  - `ctest --preset macos-clang -R 'codegen|standalone|mosac'` が全部通る。そのあと `ctest --preset macos-clang` 全体も通る（今の実行ファイルの `install_unit` の道が変わらないこと。設計のリスクの節）。
  - `MOSA_GC_STRESS=1 ctest --preset macos-clang -R 'codegen|cond_compile'` が通る（翻訳の読み込みで解いた条件の後の GC の根。設計のリスクの節）。
  - `ctest --preset macos-clang -L diff` が通る。結果が変われば `docs/agreement.*` を入れる。
- [ ] **Step 5: Commit.** `[phase13] mosac が条件をビルド時にも評価し、起動時の分岐と食い違っても mosa と同じ結果にする（ADR 0027。standalone_cond の 4 件、codegen の 2 件、ctest 全件）`

---

### Task 5: suite のライブラリ欄が条件付きコンパイルを数える（問い 2 の A）

**Files:**
- Modify: `src/machine/lgt_suite.hpp:212-215`（`library_column`）
- Modify: `tests/cpp/conformance_scope_test.cpp`（3 か所）
- Modify（作り直す）: `docs/conformance.md`、`docs/conformance.json`、`docs/conformance-library.json`

**Interfaces:**
- Consumes: タスク 3 のローダー。
- Produces: `library_column` は、今の条件に加えて `path_is_or_under(relative_posix, "directives/conditional_compilation")` で真を返す。`excluded_tree` は変えない（そのため `iso_column` は今と同じく偽を返す）。

- [ ] **Step 1: 失敗するテストを書く。** `conformance_scope_test.cpp` で、`kRuleFalse` の `"directives/conditional_compilation"` と `"directives/conditional_compilation/nested"`（160〜161 行目）を `kRuleTrue` へ移す。252 行目の `expect_column("directives/conditional_compilation", std::nullopt)` を `SuiteColumn::Library` に変え、その下に `expect_column("directives/conditional_compilation/nested", SuiteColumn::Library);` を足す。
- [ ] **Step 2: 赤を確かめる。** `ctest --preset macos-clang -R conformance_scope` が落ちる。
- [ ] **Step 3: 実装する。** 範囲の決め方を書いた頭のコメントにも、問い 2 の A を 1 行足す。
- [ ] **Step 4: 緑を確かめ、表を作り直す。**
  - `ctest --preset macos-clang -R conformance_scope` が通る。
  - `./build/mosatest --suite tests/conformance` を流す。`docs/conformance.md` の `directives/conditional_compilation` の行が 39 件全部の合格を示す。ライブラリ欄の分母が 605、合格率が 98.0% 以上。本欄の分母が 2326 のまま、合格率が 98.4% 以上。
  - `ctest --preset macos-clang` が全部通る。
- [ ] **Step 5: Commit.** `[phase13] suite のライブラリ欄が directives/conditional_compilation の 39 件を数える（問い 2 の A。39/39、ライブラリ欄 605 件、本欄 2326 件のまま）`

---

### Task 6: `library(pairs)`

**Files:**
- Create: `lib/pairs.pl`、`tests/pl/lib_pairs.pl`
- Modify: `docs/manual/en/14-reference.md` と `docs/manual/ja/14-reference.md`（`library(between)` の節の並びに `library(pairs)` の節）、`tests/cpp/manual_lists_test.cpp:104`、`tests/manual_tests.cmake:96-97`、`tests/pl/japanese.pl`

**Interfaces:**
- Produces: `:- module(pairs, [pairs_keys_values/3, pairs_keys/2, pairs_values/2, group_pairs_by_key/2, transpose_pairs/2, map_list_to_pairs/3]).` と `:- meta_predicate map_list_to_pairs(2, ?, ?).`。`transpose_pairs/2` は組を入れ替えて `keysort/2` する。`group_pairs_by_key/2` は連続して `==` の鍵だけをまとめる。

- [ ] **Step 1: 失敗するテストを書く（`tests/pl/lib_pairs.pl`、頭に `:- use_module(library(pairs)).`）。**
  - `pkv_split`: `pairs_keys_values([a-1, b-2], K, V)` は `solutions([K = [a, b], V = [1, 2]])`
  - `pkv_join`: `pairs_keys_values(P, [a, b], [1, 2])` は `solutions([P = [a-1, b-2]])`
  - `pkv_empty`: `pairs_keys_values([], K, V)` は `solutions([K = [], V = []])`
  - `pk_keys`: `pairs_keys([a-1, b-2], K)` は `solutions([K = [a, b]])`。`pv_values`: `pairs_values([a-1, b-2], V)` は `solutions([V = [1, 2]])`
  - `gpk_doc`（マニュアルの例）: `group_pairs_by_key([a-2, a-1, b-4, a-3], X)` は `solutions([X = [a-[2, 1], b-[4], a-[3]]])`
  - `gpk_doc_largest`（マニュアルの例）: `(sort(1, @>=, [a-1, b-2, c-3, a-4, a-5, c-6], Ps), group_pairs_by_key(Ps, [K-Vs|_]))` は `solutions([K = c, Vs = [3, 6]])`
  - `gpk_strict`: `group_pairs_by_key([1-a, 1.0-b], G)` は `solutions([G = [1-[a], 1.0-[b]]])`。`gpk_empty`: `group_pairs_by_key([], G)` は `solutions([G = []])`
  - `tp_sorted`: `transpose_pairs([a-2, b-1, c-2], T)` は `solutions([T = [1-b, 2-a, 2-c]])`
  - `mlp_length`: `map_list_to_pairs(atom_length, [abc, d], P)` は `solutions([P = [3-abc, 1-d]])`
  - `mlp_user`: ファイルに `pair_key(X, k(X)).` を置き、`map_list_to_pairs(pair_key, [a], P)` は `solutions([P = [k(a)-a]])`（利用者の述語を利用者のモジュールで呼ぶ）
  - `japanese.pl`: `use_module(library(pairs))` を足し、`pairs_ja`: `pairs_keys_values(P, [りんご, みかん], [1, 2])` は `solutions([P = [りんご-1, みかん-2]])`
- [ ] **Step 2: 赤を確かめる。** `./build/mosatest tests/pl/lib_pairs.pl` が `existence_error(source_sink,library(pairs))` で落ちる。
- [ ] **Step 3: 実装し、マニュアルと一覧を直す。** 第14章の節は `<!-- predicates: pairs -->` と、述語ごとに 1 行の表（日英）。一覧の 2 か所は、名前の順に `pairs` を入れる（`… lists, pairs, strings`）。
- [ ] **Step 4: 緑を確かめる。** `./build/mosatest tests/pl/lib_pairs.pl tests/pl/japanese.pl` と `./build/mosatest --manual docs/manual --all-tables` と `ctest --preset macos-clang` が全部通る。`ctest -L diff` も通るので、`docs/agreement.*` を作り直す。
- [ ] **Step 5: Commit.** `[phase13] library(pairs) を入れる（lib_pairs.pl とマニュアルの例 2 つ、japanese.pl の行、manual 0 failed、ctest -L diff 0 差分）`

---

### Task 7: `library(ordsets)`

**Files:**
- Create: `lib/ordsets.pl`、`tests/pl/lib_ordsets.pl`
- Modify: 第14章（日英）、`tests/cpp/manual_lists_test.cpp:104`、`tests/manual_tests.cmake:96-97`、`tests/pl/japanese.pl`

**Interfaces:**
- Produces: `:- module(ordsets, [is_ordset/1, ord_empty/1, ord_seteq/2, list_to_ord_set/2, ord_intersect/2, ord_disjoint/2, ord_intersection/2, ord_intersection/3, ord_intersection/4, ord_add_element/3, ord_del_element/3, ord_selectchk/3, ord_memberchk/2, ord_range/4, ord_subset/2, ord_subtract/3, ord_union/2, ord_union/3, ord_union/4, ord_symdiff/3]).` 順序は標準順序（`compare/3`）。`ord_intersection/4` の 4 つ目は `ord_subtract(Set2, Set1, _)`、`ord_union/4` の 4 つ目は `ord_subtract(Set2, Set1, _)`（どちらもマニュアルの説明のとおり）。`ord_range/4` は両端を含む。

- [ ] **Step 1: 失敗するテストを書く（`tests/pl/lib_ordsets.pl`）。**
  - `os_is`: `is_ordset([a, b, c])` は `true`。`os_is_unsorted`: `is_ordset([b, a])` は `fail`。`os_is_dup`: `is_ordset([a, a])` は `fail`。`os_is_term`: `is_ordset(foo)` は `fail`
  - `os_empty`: `ord_empty([])` は `true`、`os_empty_not`: `ord_empty([a])` は `fail`
  - `os_seteq`: `ord_seteq([a, b], [a, b])` は `true`、`os_seteq_not`: `ord_seteq([a], [b])` は `fail`
  - `os_from_list`: `list_to_ord_set([c, a, b, a], S)` は `solutions([S = [a, b, c]])`
  - `os_intersect`: `ord_intersect([a, b], [b, c])` は `true`、`os_intersect_not`: `ord_intersect([a], [b])` は `fail`
  - `os_disjoint`: `ord_disjoint([a], [b])` は `true`、`os_disjoint_not`: `ord_disjoint([a, b], [b])` は `fail`
  - `os_inter2`: `ord_intersection([[a, b, c], [b, c, d], [b, c]], I)` は `solutions([I = [b, c]])`。`os_inter2_empty`: `ord_intersection([], _)` は `fail`
  - `os_inter3`: `ord_intersection([a, b, c], [b, c, d], I)` は `solutions([I = [b, c]])`
  - `os_inter4`: `ord_intersection([a, b, c], [b, c, d], I, D)` は `solutions([I = [b, c], D = [d]])`
  - `os_add`: `ord_add_element([a, c], b, S)` は `solutions([S = [a, b, c]])`。`os_add_present`: `ord_add_element([a, c], c, S)` は `solutions([S = [a, c]])`
  - `os_del`: `ord_del_element([a, b, c], b, S)` は `solutions([S = [a, c]])`。`os_del_absent`: `ord_del_element([a, c], b, S)` は `solutions([S = [a, c]])`
  - `os_selectchk`: `ord_selectchk(b, [a, b, c], R)` は `solutions([R = [a, c]])`。`os_selectchk_absent`: `ord_selectchk(d, [a, b, c], _)` は `fail`
  - `os_memberchk`: `ord_memberchk(b, [a, b, c])` は `true`、`os_memberchk_not`: `ord_memberchk(d, [a, b, c])` は `fail`
  - `os_range`: `ord_range([1, 3, 5, 7], 2, 6, R)` は `solutions([R = [3, 5]])`。`os_range_ends`: `ord_range([1, 3, 5], 3, 5, R)` は `solutions([R = [3, 5]])`
  - `os_subset`: `ord_subset([a, c], [a, b, c])` は `true`、`os_subset_not`: `ord_subset([d], [a])` は `fail`
  - `os_subtract`: `ord_subtract([a, b, c], [b], D)` は `solutions([D = [a, c]])`
  - `os_union2`: `ord_union([[c], [a, b], [b, d]], U)` は `solutions([U = [a, b, c, d]])`
  - `os_union3`: `ord_union([a, c], [b, c], U)` は `solutions([U = [a, b, c]])`
  - `os_union4`: `ord_union([a, c], [b, c], U, N)` は `solutions([U = [a, b, c], N = [b]])`
  - `os_symdiff_doc`（マニュアルの例）: `ord_symdiff([1, 2], [2, 3], X)` は `solutions([X = [1, 3]])`
  - `japanese.pl`: `ordsets_ja`: `ord_union([みかん], [りんご], U)` は `solutions([U = [みかん, りんご]])`。`ordsets_ja_sort`: `list_to_ord_set([りんご, みかん], S)` は `solutions([S = [みかん, りんご]])`
- [ ] **Step 2〜4:** タスク 6 と同じ（一覧は `… lists, ordsets, pairs, strings`）。
- [ ] **Step 5: Commit.** `[phase13] library(ordsets) を入れる（lib_ordsets.pl とマニュアルの例、japanese.pl の行、manual 0 failed、ctest -L diff 0 差分）`

---

### Task 8: `library(assoc)`

**Files:**
- Create: `lib/assoc.pl`、`tests/pl/lib_assoc.pl`
- Modify: 第14章（日英。`del_min_assoc/4` と `del_max_assoc/4` が空で失敗することを書く）、`tests/cpp/manual_lists_test.cpp:104`、`tests/manual_tests.cmake:96-97`、`tests/pl/japanese.pl`

**Interfaces:**
- Produces: `:- module(assoc, [empty_assoc/1, list_to_assoc/2, ord_list_to_assoc/2, get_assoc/3, get_assoc/5, max_assoc/3, min_assoc/3, gen_assoc/3, put_assoc/4, del_assoc/4, del_min_assoc/4, del_max_assoc/4, assoc_to_list/2, assoc_to_keys/2, assoc_to_values/2, is_assoc/1, map_assoc/2, map_assoc/3]).` と `:- meta_predicate map_assoc(1, ?), map_assoc(2, ?, ?).`。木は `t` と `t(Key, Value, Balance, Left, Right)`（`Balance` は `<`、`=`、`>`）の AVL 木。`is_assoc/1` は、鍵が狭義の昇順であること、各節の `Balance` が左右の高さの差と合うこと、差が 1 以下であることを確かめる。誤りは `list_to_assoc/2` の `domain_error(unique_key_pairs, List)` と `ord_list_to_assoc/2` の `domain_error(key_ordered_pairs, List)` だけ。

- [ ] **Step 1: 失敗するテストを書く（`tests/pl/lib_assoc.pl`、頭に `:- use_module(library(assoc)).`）。** `A` を使う行は、ゴールの頭に `list_to_assoc([b-2, a-1, c-3], A), ` を置く（下では省く）。
  - `as_empty`: `(empty_assoc(E), assoc_to_list(E, L))` は `solutions([L = []])`
  - `as_from_list`: `assoc_to_list(A, L)` は `solutions([L = [a-1, b-2, c-3]])`
  - `as_from_list_dup`: `list_to_assoc([a-1, a-2], _)` は `error(domain_error(unique_key_pairs, [a-1, a-2]))`
  - `as_ord`: `(ord_list_to_assoc([a-1, b-2], O), assoc_to_keys(O, K))` は `solutions([K = [a, b]])`
  - `as_ord_unsorted`: `ord_list_to_assoc([b-1, a-2], _)` は `error(domain_error(key_ordered_pairs, [b-1, a-2]))`。`as_ord_dup`: `ord_list_to_assoc([a-1, a-2], _)` は `error(domain_error(key_ordered_pairs, [a-1, a-2]))`
  - `as_get`: `get_assoc(b, A, V)` は `solutions([V = 2])`。`as_get_absent`: `get_assoc(d, A, _)` は `fail`
  - `as_get5`: `(get_assoc(a, A, Old, A2, 9), assoc_to_list(A2, L))` は `solutions([Old = 1, L = [a-9, b-2, c-3]])`。`as_get5_absent`: `get_assoc(d, A, _, _, 9)` は `fail`
  - `as_put`: `(empty_assoc(E), put_assoc(k, E, 1, A1), put_assoc(k, A1, 2, A2), assoc_to_list(A2, L))` は `solutions([L = [k-2]])`
  - `as_put_keeps_old`: `(put_assoc(a, A, 7, _), get_assoc(a, A, V))` は `solutions([V = 1])`
  - `as_max`: `max_assoc(A, K, V)` は `solutions([K = c, V = 3])`。`as_min`: `min_assoc(A, K, V)` は `solutions([K = a, V = 1])`。`as_max_empty`: `(empty_assoc(E), max_assoc(E, _, _))` は `fail`
  - `as_gen`: `findall(K-V, gen_assoc(K, A, V), L)` は `solutions([L = [a-1, b-2, c-3]])`
  - `as_del`: `(del_assoc(b, A, V, A2), assoc_to_list(A2, L))` は `solutions([V = 2, L = [a-1, c-3]])`。`as_del_absent`: `del_assoc(d, A, _, _)` は `fail`
  - `as_del_min`: `(del_min_assoc(A, K, V, A2), assoc_to_list(A2, L))` は `solutions([K = a, V = 1, L = [b-2, c-3]])`。`as_del_max` は `K = c, V = 3, L = [a-1, b-2]`
  - `as_del_min_empty`: `(empty_assoc(E), del_min_assoc(E, _, _, _))` は `fail`。`as_del_max_empty` も `fail`（iso-notes と第14章の違い）
  - `as_keys`: `assoc_to_keys(A, K)` は `solutions([K = [a, b, c]])`。`as_values`: `assoc_to_values(A, V)` は `solutions([V = [1, 2, 3]])`
  - `as_is`: `is_assoc(A)` は `true`。`as_is_empty`: `(empty_assoc(E), is_assoc(E))` は `true`。`as_is_not`: `is_assoc(foo)` は `fail`
  - `as_balance_put`: `(empty_assoc(E), as_put_range(1, 1000, E, B), is_assoc(B), get_assoc(777, B, V))` は `solutions([V = 777])`。`as_put_range(From, To, Assoc0, Assoc)` はテストのファイルに書く補助で、`From` から `To` まで昇順に `put_assoc(K, _, K, _)` を重ねる。
  - `as_balance_del`: `(empty_assoc(E), as_put_range(1, 1000, E, B), as_del_odd(1, 999, B, C), is_assoc(C), assoc_to_keys(C, K), length(K, N))` は `solutions([N = 500])`。`as_del_odd(From, To, Assoc0, Assoc)` も補助で、`From` から `To` までの奇数の鍵を `del_assoc/4` で消す。
  - `as_map2`: `map_assoc(integer, A)` は `true`。`as_map2_fail`: `(put_assoc(d, A, x, A2), map_assoc(integer, A2))` は `fail`
  - `as_map3`: ファイルに `as_double(X, Y) :- Y is X * 2.` を置き、`(map_assoc(as_double, A, B), assoc_to_list(B, L))` は `solutions([L = [a-2, b-4, c-6]])`
  - `japanese.pl`: `assoc_ja`: `(list_to_assoc([りんご-1, みかん-2], J), assoc_to_keys(J, K))` は `solutions([K = [みかん, りんご]])`
- [ ] **Step 2〜4:** タスク 6 と同じ（一覧は `apply, assoc, between, …`）。
- [ ] **Step 5: Commit.** `[phase13] library(assoc) を AVL 木で入れる（lib_assoc.pl、1000 件の挿入と 500 件の削除の後の is_assoc、japanese.pl の行、manual 0 failed、ctest -L diff 0 差分）`

---

### Task 9: 文書を閉じる

**Files:**
- Modify: `docs/manual/{en,ja}/12-modules-libraries.md`（ライブラリの一覧に 3 つ）、`docs/manual/{en,ja}/14-reference.md`（指令の表に 4 行）、`docs/manual/{en,ja}/13-mosac.md`（`mosac build` で条件がビルド時にも動くことと、既知の制限 3 つ）
- Modify: `CHANGELOG.md` と `.ja.md`（`## Unreleased`）、`README.md` と `.ja.md`、`docs/iso-notes.md` と `.ja.md`、`docs/STATUS.md`

- [ ] **Step 1: マニュアルを直す（日英）。** 第14章の指令の表に `if/1`、`elif/1`、`else/0`、`endif/0` の 4 行と、読み飛ばす部分の扱い（構文の誤りを黙って読み飛ばす）を 1 段落。第13章の既知の制限は設計の 3 つ。
- [ ] **Step 2: CHANGELOG を書く（日英）。** 新しい指令と 3 つのライブラリ。振る舞いの変化（利用者の `if/1`、`elif/1`、`else/0`、`endif/0` は指令として呼ばれなくなる）。`mosac` の既知の制限 3 つ。
- [ ] **Step 3: 完了条件を確かめる（SPEC 第9節の 13 行目）。**
  - `./build/mosatest tests/pl/cond_compile.pl tests/pl/lib_pairs.pl tests/pl/lib_assoc.pl tests/pl/lib_ordsets.pl tests/pl/japanese.pl` が全部通り、`ctest --preset macos-clang -L diff` が 0 差分。
  - `ctest --preset macos-clang -R 'standalone_cond|codegen'` が通る。
  - `./build/mosatest --suite tests/conformance`: `directives/conditional_compilation` が 39/39、ライブラリ欄 98.0% 以上、本欄 98.4% 以上で分母 2326。
  - `./build/mosatest --manual docs/manual --all-tables` が 0 failed。
  - 完了条件 5（CI の全ジョブが 3 OS で通る）は、branch `phase13` を push して確かめる。push は外に出る操作なので、その前に所有者に確かめる。
- [ ] **Step 4: README、iso-notes、STATUS を直す。** STATUS に完了条件の結果を書き、フェーズを閉じるかを所有者に問う 1 行を「Next」に書く。
- [ ] **Step 5: Commit.** `[phase13] マニュアル第12〜14章、CHANGELOG、README、iso-notes、STATUS を日英で直す（完了条件 1〜4 を手元で確かめた。文書だけ）`
