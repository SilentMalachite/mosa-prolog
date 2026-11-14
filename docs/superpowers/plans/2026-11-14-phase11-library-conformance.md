# フェーズ11 準拠率の範囲の拡大と format の指示 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 準拠スイートの本欄（ISO）の分母に ISO の 4 ディレクトリを入れる。実装済みのライブラリ述語を別のライブラリ欄で測る。ライブラリ欄の不合格を、`format/2,3` の指示をそろえることを中心に直す。

**Architecture:**
- 範囲は `src/machine/lgt_suite.hpp` の一覧だけで決める。`suite_column` が欄（本欄かライブラリ欄）を返す。
- `src/machine/iso_suite.cpp` は欄ごとに数え、報告書に 2 つの節を書き、欄ごとの JSON で後退を判定する。
- `format` の新しい指示は `lib/format.pl` に書く。C++ が要るのは 2 つの内部述語（`'$stream_column'/2`、`'$format_radix'/4`）と、`'$format_float'/4` の書き方の追加だけである。

**Tech Stack:** C++20 標準ライブラリ、CMake 3.25、`mosatest`（`tests/pl` の `test(Name, Goal, Expect)` 行）、`tests/cpp/harness.hpp`（`mosa_test::check`）、Logtalk 準拠スイート（`tests/conformance/logtalk/prolog`）。

**Spec:** `docs/superpowers/specs/2026-11-14-phase11-library-conformance-design.md`（承認済み、2026-11-14、所有者。問い 1 と問い 2 は既定 A）

## Global Constraints

- 依存は C++20 標準ライブラリと CMake 3.25 以上だけにする。`-Wall -Wextra -Wpedantic`（MSVC は `/W4`）の警告はエラーとして扱う。macOS、Linux（GCC と Clang）、Windows（MSVC）の 3 OS で通す。
- 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロ、コンパイラ拡張を使わない。関数は 60 行以内、ファイルは 800 行以内にする。`tests/cpp/conformance_report_test.cpp`（778 行）と `tests/tests.cmake`（819 行）は、書き足す前にタスク 2 で分ける。`src/machine/internal.hpp`（800 行）と `src/builtins/write_pred.cpp`（799 行）には触らない。
- C++ のエラーは `raise_formal` と `formal_*` の族だけで投げる。`lib/format.pl` は今と同じく `throw(error(Formal, context(PI, _)))` で投げる。
- 組み込み述語の C++ は「引数の検査 → 本処理 → 束縛」の順に書く。ファイル頭のコメントには、節番号（無ければ「13211-1 に条項は無い」）、引数のモード、投げうるエラー、テストファイル名を書く。
- テストは実装の前に書き、赤を確かめてから緑にする。エラーのテストは `Formal` だけを比べる。スイートのファイルは書き換えない。期待値を mosa に合わせて変えない。
- 範囲（どのディレクトリをどの欄に入れるか）は、設計の「範囲の規則」だけで決める。合格率を見て足し引きしない。
- 識別子は英語、コメントとコミットメッセージは日本語で書く。文書は英語が正で、`*.ja.md` を同じコミットで直す。`docs/STATUS.md` は英語だけ。
- コミットは論理単位ごとに 1 つ。メッセージは 1 行で、`[phase11] 内容（確認方法）` の形にする。
- 1 セッションでは 1 タスクだけを行う。始めに `docs/STATUS.md` の「In progress」にタスクを書く。終わりに `cmake --build --preset macos-clang && ctest --preset macos-clang` を通し、完了したことと次にやることを STATUS に書く（15 行以内）。
- タスク 3 以降で、スイートの結果を変えるコミットでは `./build/mosatest --suite tests/conformance` と `ctest --preset macos-clang -L diff` を流す。`docs/conformance.md`、`docs/conformance.json`、`docs/conformance-library.json`、`docs/agreement.md`、`docs/agreement.json` を同じコミットに入れる（CI の「Check badges」がこの差分を見る）。
- `format` の指示を足すタスク（6〜9）は、足した指示を同じコミットで SPEC 第5節のライブラリ表の `library(format)` の行と、マニュアル第14章の `library(format)` の段落に書き足す（日英）。main の文書がコードと食い違う期間を作らない。
- `format` は今と同じく、出力を全部組んでから一度に書く。誤りのときは何も書かない。
- 既定の扱い（iso-notes の問い 2 つ。タスク 1 で書く）: `length_2` はライブラリ欄で数え、SPEC 第5節の文言には触れない。`lgt_setup_call_cleanup_3_26` は直さない（ADR 0016 のまま）。
- レビュアーは opus を使う。

## Review Focus

1. **行の途中から始まる `~N`。** 出力先の流れがすでに行の途中なら、`~N` は改行を出す。`with_output_to(atom(A), (write(x), format("~Nb", [])))` は `A = 'x\nb'`。タスク 6 の `fmt_N_midline` で確かめる。
2. **多倍長と負の数の `~r`・`~D`。** `format("~16r", [2^100])` は `'10000000000000000000000000'`、`format("~16r", [-255])` は `'-ff'`、`format("~D", [-1234567])` は `'-1,234,567'`、`format("~D", [2^70])` は `'1,180,591,620,717,411,303,424'`、`format("~2D", [5])` は `'0.05'`。タスク 7 の `fmt_r_big`、`fmt_r_neg`、`fmt_D_neg`、`fmt_D_big`、`fmt_D_small` で確かめる。
3. **誤りのときに出力が残らないこと。** `with_output_to(atom(A), catch(format("ab~r", [foo]), _, true))` は `A = ''`。タスク 7 の `fmt_r_atomic` で確かめる。
4. **全角の字と `~+`。** 桁は東アジア幅で数える。`format("~a~t~8+|", ['日本'])` は `'日本    |'`、`format("~t~a~6+|", ['日a'])` は `'   日a|'`。タスク 9 の `japanese.pl` の `fmt_plus_wide_*` で確かめる。
5. **`~g` と浮動小数点数の端。** `X is inf, format("~g", [X])` は `'1.0Inf'`、`format("~g", [2^2000])` は `evaluation_error(float_overflow)`（iso-notes 8.7 の既定 A）。タスク 8 の `fmt_g_inf`、`fmt_g_big` で確かめる。

---

### Task 1: フェーズを開く（文書だけ）

**Files:**
- Modify: `docs/iso-notes.md` と `.ja.md`（節を 3 つ足す）
- Modify: `README.md` と `.ja.md`（フェーズのバッジを `phases-0--10_done` から `phase-11_in_progress` へ。フェーズの表に 11 行目）
- Modify: `docs/STATUS.md`（1 行目を「Phases 0–10 done, phase 11 open (conformance scope and `format`, opened 2026-11-14, owner decisions A and 3)」で始める）

SPEC 第9節の 11 行目は、計画のコミットですでに書いてある（日英）。

- [ ] **Step 1: iso-notes に 3 つの節を足す（日英）。**
  - 「Section 5 `length/2`: ISO or not」。SPEC 第5節は「ISO core」、マニュアル第14章は節番号「—」。A: マニュアルが正しい。SPEC の文言を直し、`length_2` はライブラリ欄（既定）。B: SPEC が正しい。マニュアルに節番号を書き、`length_2` は本欄。確認までは SPEC の文言に触れない。
  - 「Section 7 Stage G `setup_call_cleanup/3`: bindings of a cleanup that a cut runs」。`lgt_setup_call_cleanup_3_26` と、試験器の `(本体, !), (確かめ)` と、ADR 0016 の関係を書く。A: ADR 0016 のまま、1 件は不合格（既定）。B: ADR 0016 を改め、束縛を戻す。
  - 「Section 5 `format/2,3`: reference for the phase 11 directives」。参照は同梱スイートの `format_2`・`format_3`。件が決めない点は SWI-Prolog の文書に従い、食い違えばスイートに従う（例: `~r` の既定の基数は 8）。`~r` の基数が 2〜36 の外なら `domain_error(radix, N)`。`~t~|` と `~+` の桁は `format` の呼び出しの始まりを 0 として数える（流れの今の桁を使わない。既知の制限）。`portray_1` ディレクトリは範囲の外（`portray/1` は組み込みにしない。8.14 の決定のまま）。
- [ ] **Step 2: README のバッジと表、STATUS の 1 行目を直す（日英）。**
- [ ] **Step 3: 確かめる。** `grep -n 'phase-11\|phase 11\|フェーズ11\|フェーズ 11' README.md README.ja.md docs/STATUS.md` が 3 ファイルに当たる。`./build/mosatest --manual docs/manual/en --all-tables` と `docs/manual/ja` が通る。
- [ ] **Step 4: Commit.** `[phase11] フェーズ11を開き iso-notes に length/2 と後始末の束縛の問い、format の参照の決め方を書く（文書だけ）`

---

### Task 2: 800 行を越えるファイルを分ける（振る舞いは変えない）

**Files:**
- Create: `tests/cpp/conformance_scope_test.cpp`（`conformance_report_test.cpp` の `expect_scope`、`expect_all`、`expect_children`、`check_scope` を移す）
- Create: `tests/conformance_tests.cmake`（`tests/tests.cmake` から、`conformance_*_test` と `suite_*_test` の `add_executable`・`add_test`・`set_tests_properties` を移す）
- Modify: `tests/cpp/conformance_report_test.cpp`、`tests/tests.cmake`（移した場所で `include(${CMAKE_CURRENT_LIST_DIR}/conformance_tests.cmake)`）

**Interfaces:**
- Produces: ctest の `conformance_scope_test`（資源のロックは要らない。スクラッチを使わない）。タスク 3 はここに欄の検査を足す。

- [ ] **Step 1: 移す前の数を取る。** `ctest --preset macos-clang -N | tail -1` の件数と、`ctest --preset macos-clang -R 'conformance|suite'` の結果を控える。
- [ ] **Step 2: 移す。** 関数の本体は変えない。`conformance_scope_test.cpp` の `main` は `check_scope()` を呼んで `return mosa_test::failures();` で終わる（`conformance_report_test.cpp` と同じ）。
- [ ] **Step 3: 確かめる。** ctest の件数が 1 つ増え、`-R 'conformance|suite'` がすべて通る。`wc -l` で 4 つのファイルが 800 行以内。
- [ ] **Step 4: Commit.** `[phase11] conformance_report_test から範囲の検査を、tests.cmake から準拠スイートのテストの登録を分ける（振る舞いは変えない。ctest が 1 件増えて全部通る）`

---

### Task 3: 範囲とライブラリ欄の報告書

**Files:**
- Modify: `src/machine/lgt_suite.hpp`（一覧 2 つ、`SuiteColumn`、`suite_column`）
- Modify: `src/machine/iso_suite.cpp`（欄ごとの集計、報告書の節、JSON、後退）
- Modify: `lib/messages_ja.pl`、`lib/messages_en.pl`（`library_conformance_regressed`）
- Modify: `.github/workflows/ci.yml`（Job Summary の 4 か所、「Check badges」）
- Modify: `README.md`、`README.ja.md`（2 つ目のバッジ）、`docs/SPEC.md`、`docs/SPEC.ja.md`（第8節の「数は 2 つ」を 3 つに）
- Test: `tests/cpp/conformance_scope_test.cpp`、`tests/cpp/conformance_badge_test.cpp`、`tests/cpp/conformance_report_test.cpp`
- 作り直す: `docs/conformance.md`、`docs/conformance.json`、`docs/conformance-library.json`（新しい）、`docs/agreement.md`、`docs/agreement.json`

**Interfaces:**
- Produces（`lgt_suite.hpp`）:
  - `enum class SuiteColumn { Iso, Library };`
  - `[[nodiscard]] std::optional<SuiteColumn> suite_column(std::string_view relative_posix);`
  - `suite_in_scope(p)` は `suite_column(p).has_value()` を返す（`mosatest --diff` は両方の欄を流す）。
  - `iso_scope::kIsoExtraScope` = `sort_2`、`keysort_2`、`ground_1`、`not_unifiable_2`（`predicates/` の直下）。
  - `iso_scope::kLibraryScope` = `atomic_list_concat_2`、`atomic_list_concat_3`、`between_3`、`format_2`、`format_3`、`forall_2`、`length_2`、`msort_2`、`numbervars_3`、`plus_3`、`predicate_property_2`、`setup_call_cleanup_3`、`succ_2`。各行に置き場所の注釈（`// system`、`// library(between)` など）。
- Produces（報告書）: 今の節（`分母:`、`合格数:`、`合格率:`、`不合格:`、`別欄:`）は本欄のまま。その後ろに `ライブラリ欄の分母: N`、`ライブラリ欄の合格数: N`、`ライブラリ欄の合格率: R`、`ライブラリ欄の不合格:` と不合格の行を足す。ライブラリ欄の別欄の件は今の `別欄:` の一覧にまとめる。
- Produces（JSON）: 報告書の隣に `<報告書の名前>-library.json`（同梱スイートなら `docs/conformance-library.json`）。形は `conformance.json` と同じ 1 行で、ラベルは `library conformance`、色の閾値も同じ。
- 終了コード: 本欄の 90% の下限（今の `suite_status`）、本欄の後退、ライブラリ欄の後退、書けなかったファイルのどれかで 1。ライブラリ欄には 90% の下限を掛けない（タスク 3 の時点で 65% 前後）。後退した欄の JSON は書き換えない（今の本欄と同じ）。

- [ ] **Step 1: 失敗するテストを書く。**
  - `conformance_scope_test.cpp` の `check_columns`: `suite_column("predicates/sort_2") == SuiteColumn::Iso`。`keysort_2`、`ground_1`、`not_unifiable_2`、`atom_length_2` も `Iso`。`predicates/format_2`、`length_2`、`between_3` は `Library`。`predicates/call_nth_2`、`clause_references`、`evaluable_property_2`、`atomic_concat_3`、`portray_1`、`directives/conditional_compilation` は `std::nullopt`。`kLibraryScope` の 13 個すべてが `Library` になる。
  - `conformance_badge_test.cpp` の `check_library_gate`: `predicates/atom_length_2` に合格 9・不合格 1、`predicates/between_3` に合格 1・不合格 1 を置いて流すと、終了コードは 0。`gate.json` は `90.0% (9/10)`、`gate-library.json` はラベル `library conformance` で `50.0% (1/2)`、色は `red`。次にライブラリ欄を合格 0・不合格 2 にすると、終了コードは 1 で、`gate-library.json` は `50.0% (1/2)` のまま残る。本欄だけが同じ値なら `gate.json` は書き換わる。
  - `conformance_report_test.cpp` の `check_library_section`: 上と同じ置き方で、報告書に `ライブラリ欄の分母: 2`、`ライブラリ欄の合格数: 1`、`ライブラリ欄の合格率: 50.0` の行があり、`ライブラリ欄の不合格:` の後ろに `predicates/between_3/tests.lgt` の行がある。先頭の `^合格率:` の行は本欄の `90.0`。
- [ ] **Step 2: 走らせて失敗を確かめる。** `ctest --preset macos-clang -R 'conformance_(scope|badge|report)_test'` が失敗する。
- [ ] **Step 3: 実装する。** `Pending` に `SuiteColumn column` を足し、`run_file` の集計を欄ごとの構造（`passed`、`failed`、`failures`）にする。`write_badge` はラベルを引数に取る。ライブラリ欄の後退は `library_conformance_regressed`（ja: `mosatest: ライブラリ欄の合格率が下がりました: ~w -> ~w`、en: `mosatest: library conformance pass rate went down: ~w -> ~w`）で知らせる。
- [ ] **Step 4: 走らせて通ることを確かめる。** 上の 3 つのテストと ctest 全体が通る。
- [ ] **Step 5: 同梱スイートを本来の場所で流す。** `./build/mosatest --suite tests/conformance` が 0 で終わり、本欄が `2291/2326`、ライブラリ欄が `369/566` になる（設計の付録 A）。数が違えば、物差しを作り直す前に止めて原因を調べる。
- [ ] **Step 6: 差分テストを流す。** `ctest --preset macos-clang -L diff` が通り、`docs/agreement.md` の違いが 0 件。時間が `TIMEOUT 3000` に近ければ、`diff_conformance` の制限時間を実測に合わせて上げる。
- [ ] **Step 7: CI と文書を直す。** Job Summary の 4 か所に `grep -m 1 '^ライブラリ欄の合格率:' docs/conformance.md >> "$GITHUB_STEP_SUMMARY"` を足す。「Check badges」の `git diff --exit-code` に `docs/conformance-library.json` を足す。README（日英）に 2 つ目のバッジ（`docs/conformance-library.json` を読む shields.io の endpoint、リンク先は `docs/conformance.md`）。SPEC 第8節（日英）の「2 つの数値」を「本欄の合格率、ライブラリ欄の合格率、一致率の 3 つ」にする。
- [ ] **Step 8: Commit.** `[phase11] 準拠スイートの本欄に ISO の 4 ディレクトリを入れ、ライブラリ欄の報告書と JSON と後退の判定を足す（本欄 2291/2326、ライブラリ欄 369/566、差分 0、conformance_scope・badge・report のテスト）`

---

### Task 4: 試験器の不足を直す

**Files:**
- Modify: `src/machine/lgt_io.cpp`（`lgt_load_tester`）と、調べて原因があった試験器のファイル（`lgt_exec.cpp`、`lgt_judge.cpp`、`lgt_file.cpp` のどれか）
- Create: `tests/cpp/suite_tester_test.cpp`（`tests/conformance_tests.cmake` に登録し、`RESOURCE_LOCK suite_scratch` を付ける）
- Modify: `tests/cpp/suite_assert_test.cpp`

**Interfaces:**
- Consumes: タスク 3 のライブラリ欄（直した件がそこで数えられる）。

- [ ] **Step 1: 失敗するテストを書く。**
  - `suite_tester_test.cpp` の `check_tester_clauses`: `tester.lgt` に `:- multifile(portray/1).`、`:- dynamic(portray/1).`、`portray(X) :- X == zz, write(yy).`、`:- initialization((logtalk_load(lgtunit(loader)), halt)).` を書き、`tests.lgt` に `test(t1, true) :- {clause(portray(zz), _)}.` と `test(t2, true) :- {predicate_property(portray(_), dynamic)}.` を置く。2 件とも合格し、`initialization` の中身は走らない（`halt` しない）。
  - `suite_assert_test.cpp` の `check_outside_bindings`: スイートの 3 件を写す。`T = a(_X,_Y,_Z), {numbervars(T, 0, N)}, ^^assertion(ground(T)), ^^assertion(N == 3)`（`true`）、`{length(List, 3)}, ^^variant(List, [_, _, _])`（`deterministic`）、`^^set_text_output(''), Term = a(_), {numbervars(Term, 0, _)}, {format("~w", [Term])}, ^^text_output_assertion('a(A)', Assertion)`（`true(Assertion)`）。3 件とも合格する。
- [ ] **Step 2: 走らせて失敗を確かめ、原因を 1 件ずつ書き出す。** 原因が mosa の側（試験器ではない）と分かった件は、このタスクのテストから外してタスク 5 に回し、STATUS に書く。
- [ ] **Step 3: 試験器を直す。** `lgt_load_tester` は、名指しのファイルを読んだあとで `tester.lgt` の節と `dynamic`・`multifile` の指令を `user` に読む。`initialization` と `logtalk_load` は今と同じく読まない。ほかの不足は、Logtalk の lgtunit が同じ件をどう流すかに合わせて直す。
- [ ] **Step 4: 確かめる。** 上のテストと ctest 全体が通る。スイートを流すと、`numbervars_3` の `commons_numbervars_3_08`〜`12`、`length_2` の `commons_length_2_06`、`format_2`・`format_3` の `write_numbervars` が合格に変わる。本欄の数は変わらない（変わったら止めて調べる）。
- [ ] **Step 5: Commit.** `[phase11] 試験器が tester.lgt の節を読み、{} の外の束縛を引き継ぐ（suite_tester_test、suite_assert_test、ライブラリ欄 N/566）`。N は実際の値にする。

---

### Task 5: ライブラリ述語の誤りを直す

**Files:**
- Modify: `src/builtins/terms.cpp`（`run_numbervars`）と、タスク 4 で回ってきた件の場所
- Test: `tests/pl/utilities.pl`

- [ ] **Step 1: 失敗するテストを書く。** `utilities.pl` の numbervars の行の後ろに足す。
  - `numbervars_end_type`: `numbervars(_, 1, a)` は `error(type_error(integer, a))`。`numbervars(f(_), 1, a)` も同じ。
  - `numbervars_end_bound`: `numbervars(f(_), 0, 1)` は成功し、`numbervars(f(_), 0, 2)` は失敗する。
- [ ] **Step 2: 走らせて失敗を確かめる。** `./build/mosatest tests/pl/utilities.pl` が `numbervars_end_type` で落ちる。
- [ ] **Step 3: 実装する。** `run_numbervars` で、`Start` の検査の後、変数を集める前に、`End` が変数でも整数でもなければ `type_error(integer, End)` を投げる。
- [ ] **Step 4: 確かめる。** `./build/mosatest tests/pl` と ctest 全体、`-L diff` が通る。スイートの `commons_numbervars_3_15` が合格に変わる。
- [ ] **Step 5: Commit.** `[phase11] numbervars/3 の End が整数でも変数でもなければ型の誤りにする（utilities.pl の numbervars_end_*、ライブラリ欄 N/566）`

---

### Task 6: format 1 — `~i`、`~k`、`~W`、`~p`、`~N`

**Files:**
- Create: `src/builtins/format_support.cpp`（`'$stream_column'/2`。タスク 7 で `'$format_radix'/4` も置く）
- Modify: `src/machine/builtin_api.hpp`（`[[nodiscard]] bool run_format_support(Impl& machine, const Goal& goal);`）、`src/machine/engine.cpp`（名前の表に `{"$stream_column", 2}`、呼び出しの鎖に `run_format_support`）、`CMakeLists.txt`（ソースの一覧）
- Modify: `lib/format.pl`、SPEC 第5節とマニュアル第14章（日英）
- Test: `tests/pl/lib_format.pl`

**Interfaces:**
- Produces: `'$stream_column'(+Stream, -Column)`。Column は最後の改行から書いた字の数（`Stream::line_pos - 1`、0 から）。Stream は流れの項か別名で、`put_char/2` と同じ検査をする（`src/builtins/chars.cpp` の `select_stream` を使う。ファイルの中に閉じていれば `builtin_api.hpp` に出す）。入力の流れなら `permission_error(output, stream, S)`。
- Produces（`lib/format.pl`）: `format/2` は `current_output/1` の流れで、`format/3` は引数の流れで、始まりの桁を読む。`~N` は「始まりの桁が 0 で、まだ何も組んでいない」か「組んだ出力の最後の字が改行」なら何も出さず、そうでなければ改行を出す。`~t`・`~|` の桁は今と同じく 0 から数える（非目標）。

- [ ] **Step 1: 失敗するテストを書く。** `lib_format.pl` に足す。出力はすべて `with_output_to(atom(A), …)` で比べる。
  - `fmt_i`: `format("~a~i~a", [a,b,c])` は `ac`。`fmt_i_count`: `~3i` も `ac`。`fmt_i_star`: `format("~a~*i~a", [a,3,b,c])` は `ac`。
  - `fmt_k`: `format("~k", ['A'+'B'])` は `'+(\'A\',\'B\')'`。`fmt_k_prefix`: `format("~k", [(:-a)])` は `':-(a)'`。
  - `fmt_W`: `format("~W", [a+'B',[quoted(true)]])` は `'a+\'B\''`。`fmt_W_canonical`: `[quoted(true),ignore_ops(true)]` は `'+(a,\'B\')'`。`fmt_W_bad`: `format("~W", [a, foo])` は `write_term(a, foo)` と同じ誤り（`error(type_error(list, foo))`）。
  - `fmt_p`: `format("~p", [42])` は `'42'`。`fmt_p_portray`: ファイルに `portray(fmt_portray_me) :- write(seen).` を置き、`format("~p", [f(fmt_portray_me)])` は `'f(seen)'`。
  - `fmt_N`: `format("~Nbegin~N~Nend", [])` は `'begin\nend'`。`fmt_N_count`: `~3N` でも同じ。`fmt_N_midline`: `with_output_to(atom(A), (write(x), format("~Nb", [])))` は `'x\nb'`。
  - `stream_column`: `with_output_to(atom(_), (write(abc), current_output(S), '$stream_column'(S, C)))` で `C == 3`。
- [ ] **Step 2: 走らせて失敗を確かめる。** `./build/mosatest tests/pl/lib_format.pl` が落ちる。
- [ ] **Step 3: 実装する。** `~k` は `write_canonical/1`、`~W` は `write_term/2`、`~p` は `print/1` を `with_output_to(atom(A), …)` で書いて `emit_atom` に渡す（今の `~w` と同じ形）。
- [ ] **Step 4: 確かめる。** `./build/mosatest tests/pl` と ctest 全体、`-L diff` が通る。スイートで `~i`、`~k`、`~W`、`~p`、`~N` の `domain_error(format_directive, _)` が消え、`print_portray_*` が合格する。
- [ ] **Step 5: SPEC 第5節とマニュアル第14章（日英）に 5 つの指示を足す。** `mosatest --manual` が通る。
- [ ] **Step 6: Commit.** `[phase11] format に ~i ~k ~W ~p ~N と '$stream_column'/2 を足す（lib_format.pl の fmt_i〜fmt_N_midline、ライブラリ欄 N/566、差分 0）`

---

### Task 7: format 2 — `~r`、`~R`、`~D`

**Files:**
- Modify: `src/builtins/format_support.cpp`、`src/machine/engine.cpp`（名前の表に `{"$format_radix", 4}`）
- Modify: `lib/format.pl`、SPEC 第5節とマニュアル第14章（日英）
- Test: `tests/pl/lib_format.pl`

**Interfaces:**
- Consumes: `include/mosa/bigint.hpp` の `to_text(const BigInt&, unsigned radix)`（小文字、負なら `-` が先）。int64 の値は `std::to_chars` の基数つきの形を使う。
- Produces: `'$format_radix'(+Integer, +Radix, +Case, -Atom)`。Integer は整数（Big を含む）、Radix は 2〜36 の整数、Case は `lower` か `upper`。誤りは `instantiation_error`、`type_error(integer, Integer)`、`type_error(integer, Radix)`、`domain_error(radix, Radix)`、`domain_error(letter_case, Case)`。
- Produces（`lib/format.pl`）: `~r`・`~R` は、基数（桁の数、無ければ 8）を先に確かめて `domain_error(radix, N)` を投げ、そのあとで引数を `evaluate_integer` で評価する。`~D` は `~d` と同じ桁の数の意味で、整数部を右から 3 桁ごとに `,` で区切る。符号は区切りの外に置く。

- [ ] **Step 1: 失敗するテストを書く。** `lib_format.pl` に足す。
  - `fmt_r`: `format("~16r", [127])` は `'7f'`。`fmt_R`: `format("~16R", [127])` は `'7F'`。`fmt_r_default`: `format("~r", [16])` と `format("~R", [16])` はどちらも `'20'`。`fmt_r_36`: `format("~36r", [35])` は `z`。
  - `fmt_r_big`: `format("~16r", [2^100])` は `'10000000000000000000000000'`。`fmt_r_neg`: `format("~16r", [-255])` は `'-ff'`。
  - `fmt_r_errors`: `format("~0r", [16])` と `format("~1R", [16])` は `domain_error(radix, 0)`・`domain_error(radix, 1)`、`format("~37r", [16])` は `domain_error(radix, 37)`、`format("~r", [_])` は `instantiation_error`、`format("~r", [123.0])` は `type_error(integer, 123.0)`、`format("~r", [foo(bar)])` は `type_error(evaluable, foo/1)`。
  - `fmt_r_atomic`: `with_output_to(atom(A), catch(format("ab~r", [foo]), _, true))` は `A = ''`。
  - `fmt_D`: `format("~D", [123456789])` は `'123,456,789'`。`fmt_D_places`: `format("~2D", [123456789])` と `format("~*D", [2,123456789])` は `'1,234,567.89'`。`fmt_D_short`: `format("~D", [123])` は `'123'`。
  - `fmt_D_neg`: `format("~D", [-1234567])` は `'-1,234,567'`。`fmt_D_big`: `format("~D", [2^70])` は `'1,180,591,620,717,411,303,424'`。`fmt_D_small`: `format("~2D", [5])` は `'0.05'`。
  - `fmt_D_errors`: `format("~D", [_])` は `instantiation_error`、`format("~D", [123.0])` は `type_error(integer, 123.0)`、`format("~*D", [-1,123])` は `domain_error(not_less_than_zero, -1)`。
- [ ] **Step 2: 走らせて失敗を確かめる。**
- [ ] **Step 3: 実装する。**
- [ ] **Step 4: 確かめる。** `./build/mosatest tests/pl`、ctest 全体、`-L diff` が通る。スイートで `~r`、`~R`、`~D` の `domain_error(format_directive, _)` が消える（`~2r~16+` の形の件は、タスク 9 まで `~+` で落ちたままでよい）。
- [ ] **Step 5: SPEC 第5節とマニュアル第14章（日英）に 3 つの指示を足す。** マニュアルに `?- format("~D~n", [1234567]).` の例を 1 つ足す（出力は `1,234,567`、`mosatest --manual` が照合する）。
- [ ] **Step 6: Commit.** `[phase11] format に ~r ~R ~D と '$format_radix'/4 を足す（lib_format.pl の fmt_r〜fmt_D_errors、多倍長も、ライブラリ欄 N/566、差分 0）`

---

### Task 8: format 3 — `~E`、`~g`、`~G`

**Files:**
- Modify: `src/builtins/format_float.cpp`（Kind に `E`、`g`、`G` を足す。見出しのコメントも）
- Modify: `lib/format.pl`、SPEC 第5節とマニュアル第14章（日英）
- Test: `tests/pl/lib_format.pl`

**Interfaces:**
- Produces: `'$format_float'(+Kind, +Digits, +Float, -Atom)` の Kind は `f`、`e`、`E`、`g`、`G`。`g` は `std::to_chars` の `chars_format::general` に精度 Digits（上限は今の `kExactDigits`）を渡す。`E` と `G` は指数の字を `E` にする。無限大と NaN は今と同じく、Kind によらず `1.0Inf`、`-1.0Inf`、`1.5NaN` のままにする。ほかの Kind は今と同じく `domain_error(format_directive, Kind)`。

- [ ] **Step 1: 失敗するテストを書く。** `lib_format.pl` に足す。
  - `fmt_E`: `format("~E", [1.333333])` は `'1.333333E+00'`。`format("~E", [1.333333+1])` は `'2.333333E+00'`。`format("~3E", [1.333333])` と `format("~*E", [3,1.333333])` は `'1.333E+00'`。`format("~0E", [1.333333])` は `'1E+00'`。
  - `fmt_g`: `format("~g", [0.00000123])` は `'1.23e-06'`。`format("~2g", [0.00000123])` は `'1.2e-06'`。`format("~g", [0.39265e+3])` は `'392.65'`。`format("~2g", [0.39265e+3])` は `'3.9e+02'`。
  - `fmt_G`: `format("~G", [0.00000123])` は `'1.23E-06'`。`format("~2G", [0.39265e+3])` は `'3.9E+02'`。
  - `fmt_g_inf`: `X is inf, format("~g", [X])` は `'1.0Inf'`。`fmt_g_big`: `format("~g", [2^2000])` は `error(evaluation_error(float_overflow))`。
  - `fmt_g_errors`: `format("~g", [_])` と `format("~*G", [5,_])` は `instantiation_error`、`format("~g", [foo(bar)])` は `type_error(evaluable, foo/1)`。
  - `format_float_kind`: `'$format_float'(x, 6, 1.0, _)` は今と同じく `domain_error(format_directive, x)`。
- [ ] **Step 2: 走らせて失敗を確かめる。**
- [ ] **Step 3: 実装する。**
- [ ] **Step 4: 確かめる。** `./build/mosatest tests/pl`、ctest 全体、`-L diff` が通る。CI の 3 OS で `fmt_g` と `fmt_E` が同じ字面になることを、push 後の CI で確かめる。
- [ ] **Step 5: SPEC 第5節とマニュアル第14章（日英）に 3 つの指示を足す。**
- [ ] **Step 6: Commit.** `[phase11] format に ~E ~g ~G を足す（'$format_float'/4 の Kind を広げる。lib_format.pl の fmt_E〜fmt_g_errors、ライブラリ欄 N/566、差分 0）`

---

### Task 9: format 4 — `~+`

**Files:**
- Modify: `lib/format.pl`（800 行を越えそうなら、桁止めの処理を別のファイルに分ける。分け方は実装者が決める）、SPEC 第5節とマニュアル第14章（日英）
- Test: `tests/pl/lib_format.pl`、`tests/pl/japanese.pl`

**Interfaces:**
- Produces: `~N+` は、前の桁止め（区切りの始まりの桁。`st/3` の第 1 引数）から N 桁（無ければ 8）先に桁止めを置く。埋め方（`~t` が無ければ後ろを空白で埋める、余りは後ろの埋め所から足す、全角の字は幅 2）は `~|` の `column_stop` と同じ。区切りの文字が桁止めを越えたときは `~|` と同じく埋めない。

- [ ] **Step 1: 失敗するテストを書く。**
  - `lib_format.pl` の `fmt_plus`: `format("~|~a~t~+", [abcd])` と `format("~|~a~t~8+", [abcd])` は `'abcd    '`。`format("~t~a~t~8+", [abcd])` は `'  abcd  '`。
  - `fmt_plus_text`: `format("left~10+next", [])` は `'left      next'`。`format("~tright~10+next", [])` は `'     rightnext'`。`format("^~|~ta~tb~tc~t~10+$", [])` は `'^ a  b  c  $'`。
  - `fmt_plus_chain`: `format("~*+.~n~3+.~n~*+.~n", [3,3])` は `'   .\n   .\n   .\n'`。`format("~t~a~10|~t~a~t~13+~a~t~10+", ['Alpha','and','Omega'])` は `'     Alpha     and     Omega     '`。
  - `fmt_plus_radix`: ``format("~`0t~2r~16+", [0xFF])`` は `'0000000011111111'`。
  - `japanese.pl` の `fmt_plus_wide_1`: `format("~a~t~8+|", ['日本'])` は `'日本    |'`。`fmt_plus_wide_2`: `format("~t~a~6+|", ['日a'])` は `'   日a|'`。
- [ ] **Step 2: 走らせて失敗を確かめる。**
- [ ] **Step 3: 実装する。**
- [ ] **Step 4: 確かめる。** `./build/mosatest tests/pl`、ctest 全体、`-L diff` が通る（`japanese.pl` は `diff_tests_pl` の `REQUIRE` に入っている）。スイートの `format_2`・`format_3` に `domain_error(format_directive, _)` の不合格が 1 件も無い。
- [ ] **Step 5: SPEC 第5節とマニュアル第14章（日英）に `~+` を足す。**
- [ ] **Step 6: Commit.** `[phase11] format に ~+ を足す（東アジア幅で数える。lib_format.pl の fmt_plus*、japanese.pl の fmt_plus_wide_*、ライブラリ欄 N/566、差分 0）`

---

### Task 10: 文書を閉じ、完了条件を確かめる

**Files:**
- Modify: `CHANGELOG.md`、`CHANGELOG.ja.md`（先頭に `## Unreleased` / `## 未リリース` の節）、`docs/iso-notes.md` と `.ja.md`（ライブラリ欄の残りの不合格の理由）、`README.md` と `.ja.md`（準拠率の文と、フェーズの段落）、`docs/STATUS.md`

- [ ] **Step 1: 残りの不合格を iso-notes に書く（日英）。** `docs/conformance.md` のライブラリ欄の不合格を 1 件ずつ挙げ、理由を書く（見込みは、Logtalk の `::` を使う `tab_table_left_aligned`・`right_aligned` の 4 件と、問い 2 の既定 A の `lgt_setup_call_cleanup_3_26` の 1 件）。
- [ ] **Step 2: CHANGELOG（日英）を書く。** 本欄に足した 4 ディレクトリ、ライブラリ欄と 2 つ目のバッジ、`format` の 12 の指示、`numbervars/3` の型の誤り、試験器が `tester.lgt` の節を読むこと。
- [ ] **Step 3: 完了条件を確かめる。** SPEC 第9節の 11 行目の 5 つ:
  - `./build/mosatest --suite tests/conformance` の本欄の分母に 4 ディレクトリがあり、本欄の合格率が 98.4% 以上。
  - ライブラリ欄に `kLibraryScope` の 13 個があり、`format_2`・`format_3` に `domain_error(format_directive, _)` が無く、残りの不合格がすべて iso-notes にあり、合格率が 98.0% 以上。
  - `./build/mosatest tests/pl/lib_format.pl tests/pl/japanese.pl` が通り、`ctest --preset macos-clang -L diff` の違いが 0。
  - `./build/mosatest --manual docs/manual/en --all-tables` と `docs/manual/ja` が通る。
  - push 後の CI が 3 OS の全ジョブで通る。
- [ ] **Step 4: STATUS を書き、Commit。** `[phase11] フェーズ11の文書を閉じる（CHANGELOG、iso-notes のライブラリ欄の残り N 件、完了条件 5 つを確かめた値）`
- [ ] **Step 5: 止まる。** 所有者に「フェーズ11を閉じるか（A）、直すか（B）」を問う。
