# フェーズ13 条件付きコンパイルとライブラリの追加 設計

日付: 2026-11-16
状態: 草稿（所有者の承認待ち。問い 1〜3 は 2026-11-16 に所有者が 3 つとも A に決めた）

## 意図

フェーズ 0〜12 は閉じ、0.3.0 を公開した。SPEC 第9節に 13 行目は無い。所有者は 2026-11-16 に次のフェーズの案を求めた。私の出した案 A〜G のうち、所有者は A（条件付きコンパイル `:- if/elif/else/endif`）と C（ライブラリの追加 `pairs`、`assoc`、`ordsets`）を選んだ。どちらも、フェーズ 12 の設計が非目標に挙げて「次のフェーズの候補として残す」とした物である。項の展開（`term_expansion/2`、`goal_expansion/2`）はこのフェーズに入れない。

2026-11-16 に、HEAD a0cf9b1 の `build/mosa` とソースで次のことを確かめた。

- `:- if(true). p(1). :- else. p(2). :- endif.` のファイルを consult すると、`cond.pl: existence_error(procedure,if/1)` で読み込みが失敗する。`:- if(G)` は一般の指令 `:- G` として呼ばれている（`run_directive_goal`、`src/machine/load_directive.cpp`）。
- `use_module(library(pairs))` は `existence_error(source_sink,library(pairs))` になる。`lib/` には `apply`、`between`、`dcg`、`format`、`japanese`、`lists`、`strings` と 2 つのメッセージの表しか無い。`CMakeLists.txt` の 234 行目は `lib/*.pl` を glob で集め、ライブラリごとに翻訳済みの単位を作る。ライブラリを足しても CMake を直す必要は無い。
- suite の `directives/conditional_compilation`（39 件）は、`excluded_tree`（`src/machine/lgt_suite.hpp`）でどちらの欄からも外れている。試験器は `tester.lgt` の `'$lgt_load_prolog_file'` で名指しされた `file.pl` を `Machine::consult` で読む（`lgt_io.cpp`）。`file.pl` は `f01/0`〜`f31/0` を動的と宣言する。しかし節の名前は `f1`〜`f9` と `f10`〜`f31` で、`f13` の節はどこにも無く、`f15` の試験は `{f01}` を呼ぶ。そのため、`f01`〜`f09`、`f13`、`f15` の 11 件は、読み飛ばしが壊れていても通る。
- 試験器は、`.lgt` の中の `:- if` を自分で扱っている。`tests.lgt` の条件は `lgt_cond.cpp` が評価する。`tester.lgt` に直接書いた節を読む `tester_program`（`lgt_io.cpp`）は、条件の節の中を読まない。どちらもローダーを通らないので、このフェーズの変更は届かない。
- `mosac` の翻訳の読み込みは、宣言だけを実行する。一般の指令 `:- G` は呼ばず、`skipped_goal` の印だけを付ける。実行ファイルは、起動時に埋め込んだ原文を同じローダーで読み直す。そのうえで、訳したコードを、`(module, name, arity, fingerprint)` が一致する静的な述語にだけ付ける（ADR 0009）。合わない述語は IR のまま動き、結果は変わらない。
- 今の実行ファイルは、翻訳の読み込みが読んだ物を、起動時の読み込みも必ず読むことを前提にしている。条件付きコンパイルは、翻訳の読み込みのほうが多く読む初めての道になる。前提に頼っている所は 2 つある。
  - `plan_units`（`src/codegen/project.cpp` 57〜70 行目）は、翻訳の読み込みが読んだプログラムのモジュールのファイルごとに単位を作る。`main_body`（`src/codegen/project_main.cpp` 75〜77 行目）は、その全部に `install_unit` を呼ぶ。`load_unit`（`src/machine/native.cpp` 123〜130 行目）は、埋め込んだファイルが起動時にまだ読まれていなければ、そのファイルを単独で読む。だから、ビルド時の分岐だけが読んだモジュールのファイルも、起動時に読まれる。その指令と初期化のゴールが動き、公開述語が取り込まれる。`mosa` はどれもしない。
  - `startup_modules`（`project.cpp` 288〜293 行目）は、翻訳の読み込みの後のモジュールの一覧である。`qualified_goal`（`src/codegen/emit_body.cpp` 271〜283 行目）は、この一覧にあるモジュールへの `M:G` をその場で結ぶ。指紋はその述語の節と `meta_predicate` しか見ない（`native.cpp` 152〜166 行目）。だから、起動時に `M` が読まれないと、`mosa` なら `existence_error(module, M)` になる呼び出しが、違う結果になる。
- 翻訳の読み込みは、構文の誤りを、飛ばした指令のゴールの後でだけ許す（`mark_opaque`、`load.cpp` 448〜452 行目）。そのファイルの述語を IR のまま動かす印を付け、判定を起動時の読み込みに任せる。
- `system` には組み込みの `if/3` がある（`src/builtins/control.cpp` 276 行目）。`:- if(true, (write(yes), nl), true).` は今も動き、`yes` を書く。
- 翻訳の読み込みは、読んだ項の中に字面で書かれた `use_module(library(N))` と `use_module(library(N), _)` を `note_goal_libraries` で拾う（`take_term` の中、`src/machine/goal_libraries.cpp`）。読み込みの最後に `load_goal_libraries` で読み、読めなかったライブラリは黙って落とす（フェーズ 12 の問い 2 の決定 A）。
- `src/machine/load.cpp` は 796 行、`src/machine/load_directive.cpp` は 713 行ある。

この文書を読むのは、このあと実装計画を書き、実装するエージェントである。所有者がこの文書を承認したあとに、SPEC 第9節の 13 行目、ADR 0027 と実装計画を書く。承認までは計画も実装もしない。

## 目標

- `:- if(Goal)`、`:- elif(Goal)`、`:- else`、`:- endif` を、ファイルを読むすべての道で使える。対象は、コマンド行と `consult/1`、`[File]`、ゴールの `use_module/1,2` と `ensure_loaded/1`、`include/1`、ライブラリ、`Machine::load_program`、`mosac build` の実行ファイルの起動時の読み込みである。
- `mosac` は、ビルドの時点で条件を評価して、訳す節と同梱するファイルを選ぶ。実行ファイルは起動時にも条件を評価し直す（問い 1 の決定 A）。どの条件でも、`mosa` と実行ファイルの結果は同じになる。例外は、問い 1 の決定 A が既知の制限として受けた 1 つだけである（ビルド時に取らなかった分岐の中の素のパスのファイルを、実行時に作業ディレクトリから読むこと）。
- suite の `directives/conditional_compilation` の 39 件をライブラリ欄で数え、すべて通す（問い 2 の決定 A）。
- `library(pairs)`、`library(assoc)`、`library(ordsets)` を Prolog で書いて `lib/` に置く。名前、引数の順、文書にある誤りは SWI-Prolog のマニュアルに合わせる。`mosac build` の実行ファイルは、この 3 つを同梱から見つける。

## 非目標

- `term_expansion/2`、`goal_expansion/2`、条件の `expand_goal/2`。SWI-Prolog のマニュアル（4.3.1.2 Conditional compilation、2026-11-16 に `man?section=conditionalcompilation` を読んだ。SWI-Prolog 10.1.20 の版）は "For enhanced portability, Goal is processed by expand_goal/2 before execution." と書く。Mosa にはゴールの展開が無いので、条件をそのまま呼ぶ。
- 前回の案の D〜G（ISO 外の関数と述語、節の参照、デバッガ、性能と負債の整理）。
- `if/1`、`elif/1`、`else/0`、`endif/0` をゴールとして呼ぶこと。この 4 つは読み込むテキストの指令としてだけ働く。プロンプトで `?- if(true).` と打てば、今と同じく `existence_error(procedure, if/1)` になる。
- 1 つのファイルで開いた `:- if` を、`include/1` した別のファイルで閉じること。
- 試験器の `.lgt` の条件の扱い（`lgt_cond.cpp`、`tester_program`）を変えること。
- `ord_intersect/3`（SWI-Prolog のマニュアルが非推奨とする）、`library(rbtrees)`、`library(aggregate)`、`library(option)` などのほかのライブラリ。
- 文字コードと BOM の決定（iso-notes「Section 4 Stream encoding and BOM」、2026-10-28 の決定 A）を変えること。
- このフェーズでの公開。公開は所有者の依頼で別に行う。

## アンチ目標（技術的に動いても、これをやったら失敗）

- 条件のどんな値でも、`mosa` と `mosac build` の実行ファイルの結果が違うこと（目標の節の既知の制限 1 つを除く）。
- 読み飛ばした部分が何かを変えること。演算子、旗、節、モジュール、取り込み、`mosatest` が横取りする `test/3`、文法規則の変換の誤り、のどれも変えない。
- 取った分岐の後ろの `elif` の条件や、読み飛ばしている部分の中の条件を呼ぶこと。
- 本欄（ISO）の分母を変えること。条件付きコンパイルの 39 件を本欄に入れること。
- テストの期待値を実装に合わせて書き換えること。例外は 2 つだけで、どちらもテストを弱めることではない。
  - `conformance_scope_test.cpp` の 3 か所（問い 2 の決定 A による範囲の変更）。範囲を変える同じコミットで直す。
  - `lib/` のライブラリの一覧を書いた 2 か所（`tests/cpp/manual_lists_test.cpp` 104 行目と `tests/manual_tests.cmake` 96〜97 行目）。ライブラリを足す同じコミットで、足した名前を加える。
- SWI-Prolog のライブラリのソースを写すこと。コードは新しく書く。

## 制約

- AGENTS.md のとおり。
  - C++20 標準ライブラリと CMake だけを使う。
  - 関数は 60 行以内、ファイルは 800 行以内にする。
  - 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロを使わない。
  - エラーは `throw_error`/`make_*_error` の族と、今の `formal_*` の助け手だけで作る。アトムの名前を文字列で直に書かない。
  - 識別子は英語で書く。コメントとコミットメッセージは日本語で書く。文書は英語が正で、`*.ja.md` を同じコミットで直す。
- `src/machine/load.cpp`（796 行）には足さない。条件の状態は新しいファイルに置く。`take_term` を変えるために `load.cpp` に手を入れるなら、その前に振る舞いを変えずに分ける。
- 翻訳の読み込みで条件を呼ぶのは、`mosac` の振る舞いの決まりを変える。今は「翻訳では宣言だけを実行する」。AGENTS.md の規則どおり、実装の前に ADR 0027 を書く。所有者が設計を承認して実装を頼めば、それを ADR の承認とみなす（これまでのフェーズと同じ扱い）。ADR の Status にそう書く。
- テストを先に書く（TDD）。条件付きコンパイルのテストは `tests/pl/` の新しいファイルに置き、読み込み先は `tests/pl/fixtures/` の下に置く。ライブラリのテストは `tests/pl/lib_pairs.pl`、`lib_assoc.pl`、`lib_ordsets.pl` に置く。日本語の名前のテストは `tests/pl/japanese.pl` に書く（`mosatest --diff` に必ず入る）。エラーのテストは `Formal` だけを比べる。新しく作るファイルは `tests/tmp/` の下に置く。
- ライブラリの述語は ISO の述語ではないので、規格の Examples は無い。SWI-Prolog のマニュアルにある例（`group_pairs_by_key/2`、`ord_symdiff/3`）は、そのまま 1 行ずつテストにする。

## 所有者への問い（iso-notes に「A か B か」と決定を書く。2026-11-16 に所有者が 3 つとも A に決めた）

1. **`mosac` は条件をどう扱うか**
   - A: ビルド時に評価して、訳す節と同梱するファイルを選ぶ。実行ファイルは起動時にも評価し直す（`mosa` と同じ意味）。条件のゴールはビルド時にも動く。結果が食い違っても、答えは `mosa` と同じになる（合わない述語は IR のまま動く）。ずれが残るのは、ビルド時に取らなかった分岐の中の素のパスのファイルだけで、これは実行時に作業ディレクトリから読む。ライブラリは、どちらの分岐にあっても、存在すれば同梱する。
   - B: ビルド時の結果を実行ファイルに固定する（C のプリプロセッサと同じ形）。同梱のずれは起きない。代わりに、`argv`、環境変数、ファイルの有無、翻訳器が飛ばす先の `:- G` で条件が変わると、`mosa` と違う結果になる（`:- assertz(f). :- if(f).` で、`mosa` は then の分岐を、実行ファイルは else の分岐を取る）。
   - 決定: A（所有者、2026-11-16）。私の推した理由: SPEC 第1節の目標 5（インタプリタと訳したプログラムの結果が常に同じ）を守れる。ビルド時に評価しない案は、問いの前に外した。翻訳器が、どの `use_module` と `include` を辿って同梱するかを決められず、`:- if(Dialect == swi). :- use_module(library(clpfd)). :- endif.` のような書き方でビルドが止まるからである。
2. **suite の 39 件をどちらの欄で数えるか**
   - A: ライブラリ欄。条件付きコンパイルは 13211-1 に無い。フェーズ 11 の範囲の規則では、Mosa が提供する ISO 外の物はライブラリ欄で測る。
   - B: 本欄（ISO）。`iso_column` は `directives/` の直下を本欄に入れるので、除外を外すだけだとこちらになる。
   - 決定: A（所有者、2026-11-16）。`library_column` が `directives/conditional_compilation` の木を受けるように直す（`excluded_tree` と同じく `path_is_or_under` で見る。私の判断）。`iso_column` は今と同じく偽を返す。`conformance_scope_test.cpp` では、`kRuleFalse` の `directives/conditional_compilation` と `directives/conditional_compilation/nested`（160〜161 行目。範囲に入るので `suite_in_scope` が真になる）を外し、252 行目の `expect_column(..., std::nullopt)` をライブラリ欄に変える。ライブラリ欄の分母は 566 から 605 になる。
3. **読み飛ばす部分の構文の誤り**（実装者の目での確かめの後、SWI-Prolog のマニュアルを読んで分かった）
   - 事実: SWI-Prolog のマニュアル（4.3.1.2）は "SWI-Prolog accommodates syntax extensions under conditional compilation by silently ignoring syntax errors when in the _false_ branch." と書く。例は `:- if(current_prolog_flag(bounded, false)). one_third(1r3). :- endif.` で、有理数の無い処理系では `1r3` が構文の誤りになる。これが効くのは、誤りの後で終止符に戻れ、次の条件の指令が正しいときだけだ、とも書く。Mosa の読み手は、誤りの後で終止符まで読み飛ばして続けられる（`read_terms` の `reader.consumed() > before` の道）。
   - A: 読み飛ばす部分の構文の誤りは、黙って読み飛ばす（SWI-Prolog と同じ）。ほかの処理系の構文（`1r3`、ほかの処理系の演算子）を分岐に入れた移植用のプログラムが、`mosa` で読める。
   - B: ほかの場所と同じく、読み込みを失敗させる。誤りを見落とさないが、上の書き方のプログラムは `mosa` で読めない。
   - 決定: A（所有者、2026-11-16）。私の推した理由: 条件付きコンパイルの主な使い道が、処理系ごとの違いを分けることだからである。取った分岐の構文の誤りは、今と同じく読み込みを失敗させる。

## 決めたこと（私の判断）

### 条件付きコンパイルの意味

| 指令 | 意味 |
| --- | --- |
| `:- if(Goal)` | 新しい条件の節を開く。外側が読み飛ばし中でなければ `Goal` を呼び、成功すればその後ろを読み、失敗すれば次の `elif`、`else`、`endif` まで読み飛ばす |
| `:- elif(Goal)` | 同じ節のそれまでの分岐をどれも取っていなければ `Goal` を呼び、成功すればその後ろを読む。前の分岐を取っていれば、`Goal` を呼ばずに `endif` まで読み飛ばす |
| `:- else` | それまでの分岐をどれも取っていなければ、その後ろを読む |
| `:- endif` | 条件の節を閉じる |

- 条件は、一般の指令 `:- G` と同じく、読み込み中のモジュールで 1 回だけ解く。読み込み中のモジュールが `user` でなければ `M:Goal` として呼ぶ。選択点は切り、束縛は戻す（`run_parked` と同じ形）。
- 条件が失敗したら偽とする。条件が例外を投げたら、一般の指令と同じく、その Formal を診断にして読み込みを失敗させる（`run_directive_goal`）。`Goal` が変数なら `instantiation_error`、数なら `type_error(callable, _)` が投げられ、同じく読み込みが失敗する。`halt/0,1` は、一般の指令と同じく読み込みをその場で止める。
  - SWI-Prolog のマニュアル（4.3.1.2、上と同じ版）は "If an error occurs, the error is printed and processing proceeds as if Goal has failed." と書く。Mosa は自分の一般の指令の決まり（iso-notes 7.4.2 の一般の指令）に揃える。この違いは iso-notes に書く。
- 読み飛ばす部分も、今の演算子と旗で項として読む。入れ子の `if` と `endif` を数えるためである。指令（`op/3`、`dynamic/1`、`module/2`、`include/1`、`use_module/1,2` も）は動かさず、節も足さない。
- 読み飛ばす部分の構文の誤りは、問い 3 の決定 A により、診断を出さずに次の終止符まで読み飛ばし、そこから読み続ける。翻訳の読み込みでも同じにする。誤りのせいで `:- endif` が飲み込まれ、閉じないままテキストの終わりに着いたときは、閉じない `if` として読み込みを失敗させる（SWI-Prolog のマニュアルの「終止符に戻れ、次の条件の指令が正しいときだけ効く」と同じ限り）。
- 読み飛ばしの判定は、`take_term` の一番前に置く。`mosatest` の `term_hook`（`test/3` の横取り）と文法規則の変換より先である。読み飛ばす項は、どちらにも渡さない。
- `if` と `endif` の対は、ファイル（読み込むテキスト）ごとに閉じる。`include/1` したファイルも、それだけで閉じる。次の場合は、新しいメッセージの鍵の診断で読み込みを失敗させる。
  - `if` の無い `elif`、`else`、`endif`。`else` の後ろの `elif`、2 つ目の `else`。
  - 閉じないままテキストの終わりに着いた `if`。
- 条件の指令になるのは、`if/1`、`elif/1`、`else/0`、`endif/0` の 4 つの形だけである。ほかの項数（`:- if(C, T, E)` など）は、今と同じく一般の指令として呼ぶ。組み込みの `if/3` を動かす `:- if(C, T, E).` を壊さないためである。`reserved_directive` には足さない。
  - 今は利用者の `if/1`、`elif/1`、`else/0`、`endif/0` が一般の指令として呼ばれるので、これは振る舞いの変化である。CHANGELOG に書く。2026-11-16 に `tests/pl`、`lib`、`bench`、`docs/manual`、`tests/mosac`、`tests/stack` を探し、この 4 つの指令もこの名前の節も無かった。
- `:- module/2` は、今と同じくファイルの最初の項でなければならない。条件の節は、この決まりを変えない。ファイルの最初の項が `:- if` なら、その中の `:- module/2` は今と同じ誤りになる。

### `mosac`（問い 1 の決定 A、ADR 0027）

- 翻訳の読み込みでも、`if` と `elif` の条件を呼ぶ。これが、翻訳の読み込みが呼ぶ唯一の利用者のゴールである。一般の指令 `:- G` は、今と同じく呼ばない。
- 翻訳の読み込みでは、条件が例外を投げても `halt/0,1` を呼んでも、「その分岐を取らない」とだけ扱う。翻訳は止めない。そのとき `run_aside` が書いた診断と `halt_now` は消す。本当の扱いは、実行ファイルの起動時の読み込みが決める。そこで例外になれば、`mosa` と同じく読み込みが失敗する。
- 取った分岐の節を訳し、取った分岐の `use_module`、`include`、`ensure_loaded`、`reexport` を今と同じ道で辿って同梱の閉包に入れる。
- 読み飛ばした項も、翻訳の読み込みでは `note_goal_libraries` にかける。そこに字面で書かれた `use_module(library(N))`、`use_module(library(N), _)`、`reexport(library(N))`、`reexport(library(N), _)` のライブラリは、存在すれば同梱し、無ければ黙って落とす（フェーズ 12 の決定と同じ）。`reexport` を拾う範囲を読み飛ばした項だけにするか、全部の項に広げるかは、実装者に任せる。
- 実行ファイルの起動時の読み込みは、条件を評価し直す。翻訳と違う分岐を取ったときは、訳したコードの指紋が合わない述語と、訳していない述語が IR のまま動く（ADR 0009）。結果は `mosa` と同じになる。
- 結果を同じに保つため、翻訳の読み込みのほうが多く読む場合を次のように扱う（問い 1 の決定 A の「答えは `mosa` と同じ」から来る。意図の節の 2 つの前提）。
  - 起動時の読み込みが読まなかったモジュールのファイルは、`install_unit` が単独で読まない。訳したコードも付けない。今の実行ファイルでは、根の読み込みが成功すれば、根でない単位のファイルはどれも読み終えている。だから、この変更は今の実行ファイルの振る舞いを変えない。差分テストのインタプリタ側（`diff_main.cpp` 87 行目。埋め込みの無い単位を文字で読む道）は変えない。全部の単位に当てるか、分岐の中で初めて読んだファイルの単位にだけ当てるかは、実装者に任せる。
  - 翻訳の読み込みが条件の分岐の中で初めて読んだモジュール（プログラムのファイルもライブラリも）は、`startup_modules` に入れない。そのモジュールへの `M:G` は、生成コードがその場で結ばず、実行時に組み込みの `:/2` が確かめる。
  - 翻訳の読み込みで取らなかった分岐に指令が 1 つでもあれば、飛ばした指令のゴールと同じく `skipped_goal` の印を付ける。その分岐の `op/3` が後の項の読みを変えうるからである。後で構文の誤りに当たれば、ビルドを止めずに、そのファイルを IR のまま動かす（`mark_opaque`）。たとえば `:- if(getenv('X', _)). :- op(700, xfx, ===>). :- endif. a ===> b.` は、`X` の無い所でビルドしても止まらない。
- 既知の制限（CHANGELOG と iso-notes に書く）:
  - ビルド時に取らなかった分岐の中の、素のパスのファイル（`include(foo)`、`use_module(foo)`、`ensure_loaded(foo)`）は同梱されない。起動時にその分岐を取れば、作業ディレクトリから読む（フェーズ 12 の、ゴールで読む素のパスのファイルの制限と同じ形）。
  - 条件のゴールはビルド時にも動く。条件が何かを書けば、`mosac build` の出力にも出る。
  - ビルド時に取らなかった分岐だけが定義する述語への呼び出しは、`mosac` の未定義の呼び出しの警告になる。
- 公開した述語が未定義なら、今の `mosac` はビルドを止める（`export_warnings`、`src/codegen/project.cpp` 309 行目）。ビルド時に取らなかった分岐を持つファイルのモジュールでは、これを止めずに同じ字面の警告にする。取らなかった分岐に定義があるかもしれず、起動時にその分岐を取れば `mosa` と同じく動くからである（私の判断）。
- `initialization(main, main)` の入口は、ビルド時に決まらない。生成した `main` は、起動時の読み込みのあとで `rt::finish` を呼び、起動時の読み込みが書き留めた入口を動かす（`main_body`、`src/codegen/project_main.cpp`）。分岐の中の `initialization(main, main)` も、起動時に取った分岐のものになる。
- 2026-11-16 に `src/codegen` で、翻訳の機械の状態から節の IR の外の物を決める所を探した。見つかったのは `plan_units`、`startup_modules`、`export_warnings` の 3 つで、どれも上で扱った。呼び出しの警告（`call_warnings`）は警告だけで、ビルドを止めない。

### ライブラリ

どれも新しく書く。述語名、引数の順、文書にある誤りは、SWI-Prolog のマニュアル（2026-11-16 に読んだ `man?section=pairs`、`assoc`、`ordsets`。SWI-Prolog 10.1.20 の版）に合わせる。文書に無い使い方（リストでない引数など）の誤りは足さず、テストもしない。誤りの項は、`lib/between.pl` と同じく `throw(error(Formal, context(PI, _)))` で投げる。

| モジュール | 公開する述語 | 文書にある誤りと決め事 |
| --- | --- | --- |
| `library(pairs)` | `pairs_keys_values/3`、`pairs_keys/2`、`pairs_values/2`、`group_pairs_by_key/2`、`transpose_pairs/2`、`map_list_to_pairs/3` | 誤りは文書に無い。`group_pairs_by_key/2` は連続して `==` の鍵だけをまとめる。`transpose_pairs/2` は `keysort/2` で新しい鍵の順に並べる。`map_list_to_pairs/3` は `meta_predicate map_list_to_pairs(2, ?, ?)` |
| `library(assoc)` | `empty_assoc/1`、`list_to_assoc/2`、`ord_list_to_assoc/2`、`get_assoc/3`、`get_assoc/5`、`max_assoc/3`、`min_assoc/3`、`gen_assoc/3`、`put_assoc/4`、`del_assoc/4`、`del_min_assoc/4`、`del_max_assoc/4`、`assoc_to_list/2`、`assoc_to_keys/2`、`assoc_to_values/2`、`is_assoc/1`、`map_assoc/2`、`map_assoc/3` | `list_to_assoc/2` は鍵が重なれば `domain_error(unique_key_pairs, List)`。`ord_list_to_assoc/2` は鍵が狭義の昇順でなければ `domain_error(key_ordered_pairs, List)`。`gen_assoc/3` は鍵の昇順に列挙する。`map_assoc/2,3` は `meta_predicate` で `1` と `2` |
| `library(ordsets)` | `is_ordset/1`、`ord_empty/1`、`ord_seteq/2`、`list_to_ord_set/2`、`ord_intersect/2`、`ord_disjoint/2`、`ord_intersection/2`、`ord_intersection/3`、`ord_intersection/4`、`ord_add_element/3`、`ord_del_element/3`、`ord_selectchk/3`、`ord_memberchk/2`、`ord_range/4`、`ord_subset/2`、`ord_subtract/3`、`ord_union/2`、`ord_union/3`、`ord_union/4`、`ord_symdiff/3` | 誤りは文書に無い。`ord_intersection/2` は空のリストで失敗する。順序集合でない入力の振る舞いは決めない（文書でも未定義） |

- `assoc` の木は AVL 木にする。項の形は `t`（空）と `t(Key, Value, Balance, Left, Right)`（`Balance` は `<`、`=`、`>`）にする。マニュアルはこの形を書いていない。テストはこの形に頼らず、`is_assoc/1` と公開の述語だけで確かめる。
- `del_min_assoc/4` と `del_max_assoc/4` は、空の連想リストでは失敗する。SWI-Prolog のマニュアルは、空でも何も束縛せずに成功すると注意書きで書いている。しかしそれでは `Assoc` に何が入るかが決まらないので、Mosa は失敗にする。マニュアル第14章にこの違いを書く。
- 鍵は基底項でなければならない（SWI-Prolog の文書のとおり）。検査はしない。
- 日本語の行を `tests/pl/japanese.pl` に足す。たとえば `list_to_assoc([りんご-1, みかん-2], A), assoc_to_keys(A, K)` で `K = [みかん, りんご]`（標準順序。み は U+307F、り は U+308A）。`ord_union/3` と `pairs_keys_values/3` にも日本語の原子の行を足す。

### ADR 0027（条件付きコンパイルと翻訳時の評価）

フェーズを開くタスクで書く。中身は次のとおり。

- 背景: 翻訳の読み込みは宣言だけを実行する。実行ファイルは埋め込んだ原文を読み直し、指紋の合う述語にだけ訳したコードを付ける（ADR 0009）。`:- if` は、訳す節と同梱するファイルを選ぶために、ビルド時に分岐を決める必要がある。
- 決定: ローダーが、ファイルごとに `if/elif/else/endif` を扱う。条件は `:- G` と同じく読み込み中のモジュールで 1 回解く。翻訳の読み込みも条件を呼ぶ（翻訳が呼ぶ唯一の利用者のゴール）。そこでの例外と halt は「取らない」とだけ扱う。読み飛ばした項のライブラリも、存在すれば同梱する。起動時の読み込みは条件を評価し直す。起動時に読まなかったモジュールのファイルの単位は読まず、分岐の中で初めて読んだモジュールへの `M:G` は結ばず、取らなかった分岐の指令は飛ばしたゴールとして扱い、取らなかった分岐を持つファイルの未定義の公開は警告にとどめる。
- 退けた案: ビルド時の結果を実行ファイルに固定する案（`argv`、環境、ファイル、飛ばした `:- G` で `mosa` と結果が分かれる）。ビルド時に評価しない案（同梱するファイルを決められず、方言の分岐でビルドが止まる）。
- 結果: 条件はビルド時にも動く。分岐が食い違っても答えは同じで、遅くなるだけである。取らなかった分岐の素のパスのファイルは、実行時に作業ディレクトリから読む。

## 規模と順序

9 タスク。AGENTS.md の「1 セッション 1 タスク」に合わせて切る。順は私の判断である。

1. フェーズを開く（文書だけ）。次を日英で書く。
    - SPEC 第3節の指令の項目に、ISO 外の `if/1`、`elif/1`、`else/0`、`endif/0`。
    - SPEC 第5節のライブラリの表に 3 行。
    - SPEC 第6節の読み込みと `mosac` の項（翻訳の読み込みが条件を呼ぶこと、起動時に評価し直すこと）。
    - SPEC 第9節の 13 行目。
    - iso-notes に、問い 1〜3 とその決定、条件の例外を SWI-Prolog と違えて読み込みの失敗にする理由、`del_min_assoc/4` と `del_max_assoc/4` の違い。
    - ADR 0027。
    - STATUS。
2. `src/machine/load.cpp` を、振る舞いを変えずに 2 つ以上のファイルに分ける。
3. 条件付きコンパイルをローダーに入れる（インタプリタ）。条件の状態は新しいファイルに置く。テストを先に書く。新しいテストのファイル（たとえば `tests/pl/cond_compile.pl`）と、`tests/pl/fixtures/` の下の読み込み先に、次の行を置く。
    - `if`、`elif`、`else` の各組み合わせと入れ子。
    - 取った分岐の後ろの `elif` の条件が呼ばれないこと。条件に副作用（`assertz/1`）を持たせて確かめる。
    - 読み飛ばした部分の `op/3`、`dynamic/1`、節、文法規則、`include/1` が何も変えないこと。
    - 対の無い `elif`、`else`、`endif`、`else` の後ろの `elif`、閉じない `if`（`include/1` したファイルの中も）で読み込みが失敗すること。
    - 例外を投げる条件と、変数の条件で読み込みが失敗すること。
    - 読み飛ばす部分の構文の誤り（`one_third(1r3).` など）は、診断を出さずに読み飛ばされ、後ろの項が読まれること。取った分岐の構文の誤りは、今と同じく読み込みを失敗させること（問い 3 の決定 A）。
    - モジュールのファイルの中の条件が、そのモジュールで呼ばれること。
    - 読み飛ばした部分の `test/3` を `mosatest` が拾わないこと。

    日本語の名前の述語を分岐に置いた行を `tests/pl/japanese.pl` に足す。メッセージの鍵を `lib/messages_ja.pl` と `lib/messages_en.pl` に足す。
4. `mosac`（問い 1 の決定 A）。次のことを入れる。
    - 翻訳の読み込みで条件を呼び、例外と halt を「取らない」に変える。
    - 読み飛ばした項のライブラリを同梱に足す。
    - 起動時に読まなかったモジュールのファイルの単位を読まない。
    - 分岐の中で初めて読んだモジュールを `startup_modules` から外す。
    - 取らなかった分岐の指令に `skipped_goal` の印を付ける。
    - 取らなかった分岐を持つファイルのモジュールでは、未定義の公開を止めずに警告にする。

    ctest で次のことを確かめる。どれも、`$MOSA_LIB` の無い、隣に `lib/` も無い場所で、実行ファイルが `mosa` と同じ出力を出すことで確かめる。
    - 先の `:- assertz(f)` で条件が変わり、起動時に取る分岐が `use_module(library(strings))` を読むプログラム（ビルド時は取らない分岐のライブラリ。`pairs` はタスク 6 まで無いので、今あるライブラリを使う）。
    - モジュールが公開する述語を、ビルド時に取らない分岐だけが定義するプログラム。ビルドが止まらないこと。
    - ビルド時に取る分岐だけがモジュールのファイルを `use_module` し、起動時はその分岐を取らないプログラム。そのモジュールの初期化のゴールが動かず、公開述語が取り込まれず、別の節の `M:G` が `mosa` と同じ `existence_error(module, M)` になること。
    - 取らなかった分岐に `op/3` があり、その演算子を後ろの項が使うプログラム。ビルドが止まらないこと。

    分岐が同じなら、取った分岐の述語が訳されることも確かめる。`mosac emit` の出力に、取った分岐の述語の関数があり、取らなかった分岐だけの述語の関数が無いことで見る。
5. suite: ライブラリ欄が `directives/conditional_compilation` を受ける（問い 2 の決定 A。`lgt_suite.hpp` と `conformance_scope_test.cpp`）。`docs/conformance.md`、`docs/conformance.json`、`docs/conformance-library.json` を作り直す。
6. `library(pairs)` と `tests/pl/lib_pairs.pl`。
7. `library(ordsets)` と `tests/pl/lib_ordsets.pl`。
8. `library(assoc)` と `tests/pl/lib_assoc.pl`。

    タスク 6〜8 のそれぞれで、同じコミットに次のことを入れる。`lib/` にファイルを置いたとたんに、3 つの ctest が赤になるからである（`manual_lists_test`、`manual_fixture_all_tables`、`manual_en_ja`）。
    - マニュアル第14章（日英）に、そのライブラリの述語の表の節を書く。`--all-tables` は、`lib/` のライブラリごとに表を求める。
    - `tests/cpp/manual_lists_test.cpp` 104 行目と `tests/manual_tests.cmake` 96〜97 行目のライブラリの一覧に、その名前を足す。
    - 日本語の行を `tests/pl/japanese.pl` に足す。
9. 文書を閉じる。次を日英で書く。
    - マニュアル第12章（ライブラリの一覧）と第14章の指令の表。第13章に、条件がビルド時にも動くことと既知の制限を書く。
    - CHANGELOG の `## Unreleased`（新しい指令とライブラリ、`if/1`・`elif/1`・`else/0`・`endif/0` が利用者の述語を呼ばなくなること、既知の制限）。
    - README。
    - iso-notes。
    - STATUS。

    CI の全ジョブが通ることを確かめ、フェーズを閉じるかを所有者に問う。

タスク 2 はタスク 3 より前に置く。タスク 4 と 5 はタスク 3 の後に置く。タスク 6〜8 は、タスク 1 の後なら、ほかのタスクと順を入れ替えてよい。

## 完了条件（SPEC 第9節の 13 行目にする）

1. 条件付きコンパイルの新しいテストのファイル、`tests/pl/lib_pairs.pl`、`lib_assoc.pl`、`lib_ordsets.pl`、`tests/pl/japanese.pl` の新しい行が `mosa` で通り、`mosatest --diff` の差分が 0 である。
2. ビルド時と起動時で分岐が違う次の 4 つのプログラムを `mosac build` で作る。どの実行ファイルも、`$MOSA_LIB` の無い、隣に `lib/` も無い場所で、`mosa` と同じ出力を出す（ctest）。
    - 起動時に取る分岐だけがライブラリを読むもの。
    - ビルド時に取る分岐だけがモジュールのファイルを読むもの（そのモジュールの初期化が動かず、`M:G` が `existence_error(module, M)` になる）。
    - 取らなかった分岐の `op/3` を後ろの項が使うもの。
    - 公開した述語を、取らなかった分岐だけが定義するもの。

    分岐が同じなら、`mosac emit` の出力が、取った分岐の述語だけを訳す（ctest）。
3. `mosatest --suite tests/conformance` のライブラリ欄が `directives/conditional_compilation` の 39 件を数え、すべて通る。ライブラリ欄の合格率が 98.0% を、本欄の合格率が 98.4% を下回らず、本欄の分母は 2326 のまま変わらない。
4. `mosatest --manual docs/manual --all-tables` が通る。
5. CI の全ジョブが 3 OS で通る。

## 実装者に任せること

- 条件の状態を持つ型と、それを置く新しいファイルの名前。`read_terms` の 1 回の呼び出しに 1 つ持たせる形を勧めるが、決めない。
- `load.cpp` の分け方。
- 新しいメッセージの鍵の名前と字面（日英）。
- 条件付きコンパイルのテストのファイル名と、読み込み先の置き場所。
- `reexport(library(N))` を拾う範囲（読み飛ばした項だけか、全部の項か）。
- 3 つのライブラリの中の補助の述語の名前と、AVL 木の均衡の直し方。

## リスク

- 翻訳の読み込みで利用者のゴールを呼ぶのは初めてである。`load_goal_libraries` は翻訳中に読み込みを走らせているが、利用者のゴールは走らせていない。`run_parked` を `machine.translation` のある機械で呼ぶとき、`run_aside` が例外で書く診断、`halt_now`、`skipped_goal` と `opaque` の印、GC の根が、翻訳の後の段（`translate_program`、`check.cpp`）に漏れないかをタスク 4 で確かめる。
- 起動時に分岐が違うと、訳したコードの多くが付かず、実行ファイルは遅くなる。結果は同じなので、警告は出さない。`--verbose` で知らせるかは実装者に任せる。
- `--diff` で `mosac` が訳した側を走らせるとき、条件付きコンパイルのテストが `consult/1` で読む読み込み先は、素のパスなので実行時にディスクから読む。作業ディレクトリが両側で同じであることを、タスク 3 で確かめる（フェーズ 12 のタスク 4 と同じ確かめ）。
- suite の 39 件のうち、`f01`〜`f09`、`f13`、`f15` の 11 件は読み飛ばしを試していない。読み飛ばしは、タスク 3 の自前の行で確かめる。
- 4 つの形を条件の指令にすると、利用者が `:- if(G)` で自分の `if/1` を呼んでいたプログラムは、意味が変わる。CHANGELOG に書く。
- 起動時に読まなかったモジュールのファイルの単位を読まない変更は、すべての実行ファイルの `install_unit` の道に触る。今の振る舞いが変わらないことを、タスク 4 で今の ctest 全部（`tests/mosac` と `--diff`）が通ることで確かめる。
