# フェーズ5 準拠率と日本語対応 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 同梱 Logtalk スイートの合格率をフェーズ4終了時の基準値より上げ、`tests/pl/japanese.pl` を SPEC 第4節の最小集合まで広げて全件通す。

**Architecture:** 前半は不合格の仕分けと修正である。不合格は「試験器の不足（S）」「ISO 実装の誤り（I）」「持ち主の判断待ち（Q）」「SPEC 外の非 ISO（N）」「後のフェーズ（X）」に分け、S と I だけをコードで直す。後半は日本語対応で、全角記号モードは字句解析の入口で 1 文字を置き換えるだけにし、メッセージは C++ から `lib/messages_{ja,en}.pl` の事実へ移す。OS 分岐は `src/platform/` に閉じる。

**Tech Stack:** C++20、CMake 3.25 以上、CTest、標準ライブラリのみ。

**Spec:** `docs/SPEC.md` 第4節（日本語と Unicode）、第7節（エラー体系）、第9節（フェーズ5の行）。未決の解釈は `docs/iso-notes.md`。

## 開始条件

- フェーズ4の完了条件（`tests/pl/modules.pl` が通る、`library(lists)` を使う既存テストが壊れない）を満たしていること。2026-10-28 時点ではフェーズ4は未着手であり、この計画はフェーズ4の終了後に着手する
- 完了条件の「合格率を上げる」は、Task 1 で測るフェーズ4終了時の基準値より上であることとする。固定の目標値は置かない（2026-10-28 持ち主の決定: A）
- 下の「現時点の不合格の見取り図」は 2026-10-28 の `docs/conformance.md`（分母 2171、合格 2028、93.4%）から作った。`between/3`、`format/3`、`atomic_list_concat/2,3`、`forall/2` などはフェーズ4の ADR 0006 と iso-notes `Section 5 Autoload` の決定しだいで消えるので、数はタスク 1 で取り直す

## Global Constraints

- 言語は C++20。依存は標準ライブラリと CMake だけ。`-Wall -Wextra -Wpedantic`（MSVC は `/W4`）を警告エラーで通す
- 1 関数 60 行以内、1 ファイル 800 行以内。`lgt_io.cpp`（778 行）、`lgt_judge.cpp`（718 行）、`lgt_exec.cpp`（708 行）、`lgt_file.cpp`（703 行）に足すときは、先に分割する
- 識別子は英語。コメント、コミットメッセージは日本語。文書は英語が正で `*.ja.md` を同じコミットで直す。`docs/STATUS.md` は英語だけで 15 行以内
- `#ifdef _WIN32` などの OS 分岐は `src/platform/` だけに書く
- エラーは `throw_error(m, Formal, PI)` と `make_*_error` 系だけで投げる。原子名を文字列リテラルで書かない
- 組み込み述語は「引数検査 → 本処理 → 束縛」の順。途中で束縛してから失敗しない
- Cell（ADR 0001、0002）と IR（ADR 0003）は変えない。`include/mosa/mosa.hpp` の API を増やさない
- `tests/conformance/logtalk/` は改変しない
- 期待値を実装に合わせて書き換えない。ADR で形を変えた期待値だけを、その ADR のコミットで直す
- 解釈が一つに決まらない不合格は推測で実装せず、`docs/iso-notes.md`（と `.ja.md`）に条項番号付きで「A か B か」を 1 つ書いて、そのタスクを終える
- 任意精度整数（unbounded）、GC、循環項はフェーズ7以降。この計画では触らない
- `iso` 旗が真のときの振る舞いは常に ISO。SWI に寄せない
- コミットは 1 論理単位ずつ、`[phase5] 内容（確認方法）` の日本語 1 行
- 各タスクの終わりに `cmake --build build && ctest --test-dir build` を通し、`docs/STATUS.md` を更新する
- 版は最後のタスクまで `mosa 0.0.1 (phase 4)`（フェーズ4終了時の値）のまま

## Review Focus

1. 全角記号モードが偽のとき、`、` と `。`（どちらも `Po` なので記号文字）を含む既存のソースが今までと同じ項に読めること。タスク 10 の `fw_off_symbol` で固定する
2. 全角記号モードが真でも、引用原子、文字列、`0'` 文字、コメントの中の全角記号は置き換えないこと。タスク 10 の `fw_quoted_kept` と `fw_char_code_kept` で固定する
3. `:- set_prolog_flag(japanese_punctuation, true).` を書いたファイルでは、その次の節から全角記号が効くこと。字句解析器が先読みしたトークンにも新しい値が届くこと。タスク 10 の `fw_directive_next_clause` で固定する
4. `messages_*.pl` が見つからない、または鍵が欠けているときも、例外の表示が落ちずに鍵と項を出すこと。タスク 11 の `message_missing_key` で固定する
5. 日本語を含む行の構文エラーの列が、同じ形の ASCII の行と同じ数になること（列は符号位置で数える）。タスク 12 の `syntax_col_japanese` で固定する

---

## 現時点の不合格の見取り図（2026-10-28、143 件）

| 群 | 件数 | 主な中身 | 類 | タスク |
| --- | --- | --- | --- | --- |
| `suppress_text_output/0`、`text_output_contents/1,2` が無い | 約 22 | `call_1`、`catch_3_07`、`write_term_3` の 103〜113 と 043、044、065、066、117、`current_op_3_40` | S | 2 |
| `=~=` で `fail` | 約 15 | `acos`、`asin`、`atan`、`atan2`、`cos`、`exp`、`log`、`pi`、`tan`、`integer_power_2_09`、`float_fractional_part_1_02` | S | 3 |
| `ground/1` が無い | 2 | `current_input_1_06`、`current_output_1_06`。8.3.10 の述語そのものが未実装 | I | 4 |
| `write_term/3` の選択肢 | 約 10 | `quoted(false)` などを `domain_error` にする（118〜122）、`lgt_write_term_3_088`〜`096` | I | 5 |
| `discontiguous` の読み込み | 5 | `syntax/directives/discontiguous_1_01`〜`05` が `load` | I | 6 |
| 整数を受けた丸め系評価子 | 6 | `ceiling`、`floor`、`round`、`truncate`、`float_integer_part`、`float_fractional_part` に整数 → `type_error(float, 9)` の期待 | Q | 7 |
| `round/1` の負の半分 | 2 | `eclipse_round_1_07`、`08` | Q | 7 |
| 未決の iso-notes | 約 10 | 7.8.3、9.3.1.3 d、7.5.4（logical update 4 件）、8.11.8、`memberchk/2`、8.11.1 | Q | 8 |
| `if/3`、`log/2`、旗 `dialect`・`version_data`、`encoding('UTF-8')`・`bom/1`、`::/2` | 約 45 | SPEC に無い非 ISO | N | 8 |
| 9223372036854775808 などの多倍長 | 1 と別欄 111 | `sics_read_term_3_24` | X | なし |
| 残り | 約 25 | `stream_property` の `eof_action(reset)`・`mode(append)`・`position`、`setof_3_34`、`abolish_1_07`、`sub_atom_5`、`univ_2` など | I か Q | 9 |

## ファイルの責任

| パス | 責任 | タスク |
| --- | --- | --- |
| `docs/superpowers/plans/2026-10-28-phase5-conformance-japanese.md` の「仕分け表」節 | 不合格 1 件ごとの類とタスク | 1 |
| `src/machine/lgt_io.cpp` から分ける `src/machine/lgt_output.cpp` | lgtunit の出力捕捉述語 | 2 |
| `src/machine/lgt_judge.cpp` から分ける `src/machine/lgt_approx.cpp` | `=~=` | 3 |
| `src/builtins/types.cpp` | `ground/1` | 4 |
| `src/builtins/write_pred.cpp` | 書き出し選択肢の検査 | 5 |
| `src/machine/load.cpp`、`src/machine/consult.cpp` | `discontiguous` の読み込み | 6 |
| `src/builtins/arith_float.cpp` | 丸め系評価子 | 7 |
| `src/read/fullwidth.cpp`（新規）、`src/read/lexer.cpp`、`src/read/lexer_literal.cpp` | 全角記号の置き換え | 10 |
| `docs/adr/0007-messages.md`（新規、`.ja.md` も） | メッセージの置き場と `syntax_error` の Formal | 11 |
| `lib/messages_ja.pl`、`lib/messages_en.pl`（新規）、`src/machine/messages.cpp`（新規）、`src/machine/session.cpp`、`src/main_mosa.cpp`、`src/builtins/flags.cpp` | 旗 `message_language` と表示 | 11 |
| `src/read/lexer_literal.cpp`、`src/builtins/read_pred.cpp`、`tests/pl/read.pl` | 構文エラーの鍵と位置 | 12 |
| `lib/format.pl` かフェーズ4で決まる `format` の実装 | `~t~|` の東アジア幅 | 13 |
| `src/platform/console.cpp`（新規）、`include/mosa/console.hpp`（新規）、`src/main_mosa.cpp` | Windows コンソールの UTF-8 | 14 |
| `src/core/path_utf8.cpp`（`streams.cpp` の `path_from_utf8` を移す）、`src/machine/load_path.cpp`、`src/machine/suite.cpp` | 日本語のパス | 15 |
| `tests/pl/japanese.pl` | SPEC 第4節の最小集合 | 10、12、13、15、16 |

---

## 前半: 準拠率

### Task 1: 基準値を取り、仕分け表を作る

**Files:**
- Modify: `docs/superpowers/plans/2026-10-28-phase5-conformance-japanese.md`（末尾に「仕分け表」節を足す）
- Modify: `docs/STATUS.md`

**Interfaces:**
- Produces: 「仕分け表」節。1 行 1 件で `パス テスト名 類 タスク番号`。類は S、I、Q、N、X のどれか。以後のタスクはこの表の自分の行だけを直す

- [x] **Step 1:** `./build/mosatest --suite tests/conformance --report docs/conformance.md` を実行し、分母、合格数、合格率を `docs/STATUS.md` に `Baseline (phase 5 start): N/D = R%` として書く
- [x] **Step 2:** 不合格の各行を見取り図の群に当てはめ、仕分け表に書く。群に入らない行は I か Q に分け、Q には iso-notes の見出しを添える
- [x] **Step 3:** N の群ごとに 1 つずつ、`docs/iso-notes.md` に「A: SPEC 外なので実装しない（不合格のまま残す） B: SPEC に足して実装する」を書く。持ち主への質問は 1 回に 1 つ
- [x] **Step 4:** コミット `[phase5] 準拠スイートの基準値と仕分け表を書く（mosatest --suite で N/D）`

### Task 2: lgtunit の出力捕捉述語とライブラリの取り込み

**Files:**
- Create: `src/machine/lgt_output.cpp`（`lgt_io.cpp` から出力まわりを先に移す）
- Test: `tests/cpp/conformance_io_test.cpp`

**Interfaces:**
- Consumes: `lgt_io.cpp` の `^^` 一時ファイルの仕組み
- Produces: 試験器の中だけで見える `suppress_text_output/0`、`text_output_contents/1`、`text_output_contents/2`。意味は固定コミット `4decb64` の `lgtunit.lgt` に合わせ、`current_predicate/1` には出さない

- [x] **Step 1:** 小さな `tests.lgt` を 3 つ書いた失敗するテストを足す。`output_suppressed`: `test(a, error(instantiation_error)) :- ^^suppress_text_output, write_term(_, [quoted(_)])` が合格に数えられる。`output_contents`: `test(b, true(Cs == [f,o,o])) :- ^^set_text_output(''), write(foo), ^^text_output_contents(Cs)` が合格。`output_contents_stream`: 2 引数版で同じ
- [x] **Step 2:** `ctest --test-dir build -R conformance_io_test` が 3 件失敗することを見る
- [x] **Step 3:** 移動だけのコミットを先に作り、次に 3 述語を足す
- [x] **Step 4:** テストとスイートを流し、`write_term_3` の 103〜113 が合格に移ったことを報告書で見る
- [x] **Step 5:** コミット `[phase5] 試験器に suppress_text_output と text_output_contents を足す（conformance_io_test、合格 +N）`
- [x] **Step 6:** iso-notes `Section 5 Autoload`（決定 A）に従い、処理系は変えず、試験器が各 `tests.lgt` の読み込み前にテストのモジュールへ `lists`、`apply`、`between`、`format`、`strings`、`japanese` を取り込む。失敗するテスト `suite_imports_libs`: `test(b, true) :- between(1, 2, _)` が合格に数えられる。`mosa` の `user` では `between/3` が `existence_error` のまま（`lists_needs_use` と同じ形）
- [x] **Step 7:** コミット `[phase5] 試験器がテストのモジュールへ 6 ライブラリを取り込む（conformance_io_test、合格 +N）`

 `=~=` を lgtunit の定義に合わせる

**Files:**
- Create: `src/machine/lgt_approx.cpp`（`lgt_judge.cpp` の `approx` を移す）
- Test: `tests/cpp/conformance_report_test.cpp`

**Interfaces:**
- Produces: `bool approx(const Heap& heap, Cell left, Cell right)`（名前と型は今のまま）

- [ ] **Step 1:** 失敗するテスト `approx_pi`: `test(p, true(X =~= 3.1415927)) :- X is pi` と `approx_list`: `true([1.0, 2.0] =~= [1.0000001, 2.0])` が合格、`approx_far`: `true(1.0 =~= 1.1)` が不合格
- [ ] **Step 2:** 固定コミット `4decb64` の `lgtunit.lgt` にある `=~=` の定義（相対誤差と絶対誤差の閾値、リストと複合項の扱い）をコメントに条項代わりに出典として書き、同じ式で実装する
- [ ] **Step 3:** テストとスイートを流し、見取り図の `=~=` 群が合格に移ったことを見る。残ったものは評価子の誤差なので仕分け表で I にしてタスク 9 へ回す
- [ ] **Step 4:** コミット `[phase5] =~= を lgtunit と同じ閾値にする（conformance_report_test、合格 +N）`

### Task 4: `ground/1`（8.3.10）

**Files:**
- Modify: `src/builtins/types.cpp`（先頭の注釈に 8.3.10、`ground(?term)`、エラーなし、`tests/pl/types.pl`）
- Test: `tests/pl/types.pl`

- [ ] **Step 1:** 8.3.10.4 の Examples を `tests/pl/types.pl` に 1 行 1 テストで写す。足りなければ `ground_atom`: `ground(a)` 成功、`ground_var`: `ground(f(_))` 失敗、`ground_nested`: `ground(f(g(a), [b]))` 成功
- [ ] **Step 2:** `ctest -R prolog_tests` で `existence_error(procedure, ground/1)` を見る
- [ ] **Step 3:** 束縛しない述語として実装する。深い項で C++ のスタックを使い切らないよう、再帰ではなく明示のスタックで辿る
- [ ] **Step 4:** テストを流す。コミット `[phase5] ground/1 を実装（8.3.10 の Examples 全通過）`

### Task 5: `write_term/3` の選択肢の検査（8.14.2.3）

**Files:**
- Modify: `src/builtins/write_pred.cpp`
- Test: `tests/pl/write.pl`

- [ ] **Step 1:** 失敗するテストを足す。`wt_opt_var`: `write_term(a, [_])` → `instantiation_error`。`wt_opt_tail`: `write_term(a, [quoted(true)|_])` → `instantiation_error`。`wt_opt_quoted_fail`: `write_term(a, [quoted(fail)])` → `domain_error(write_option, quoted(fail))`。`wt_opt_vn_bad`: `write_term(a, [variable_names(a)])` → `domain_error(write_option, variable_names(a))`。`wt_opt_vn_tail`: `write_term(a, [variable_names(['A'=_|a])])` → `domain_error(write_option, variable_names(['A'=_|a]))`。`wt_opt_ok`: `write_term(a, [quoted(false), numbervars(true), variable_names(['Used'=_])])` 成功
- [ ] **Step 2:** 失敗を見る。`lgt_write_term_3_118`〜`122` の入力をスイートから写し、どの組み合わせで正しい選択肢が拒まれるかをテスト名 `wt_opt_lgt_118` などで固定する
- [ ] **Step 3:** 選択肢リストの検査を、書き出しを始める前に全部終える関数 `std::optional<WriteOptions> parse_write_options(Impl& machine, const Goal& goal, Cell list)` にまとめる
- [ ] **Step 4:** テストとスイートを流す。コミット `[phase5] write_term/3 の選択肢を書き出す前に検査する（8.14.2.3、write.pl と合格 +N）`

### Task 6: `discontiguous` の読み込み（7.4.2.3）

**Files:**
- Modify: `src/machine/load.cpp` か `src/machine/consult.cpp`（原因が分かってから決める）
- Test: `tests/pl/directives.pl`

- [ ] **Step 1:** `syntax/directives/discontiguous_1/tests.lgt` の 01〜05 と同じ形（引数 1〜4 の述語を他の述語の節で分断する）を `tests/pl/directives.pl` に `dc_arity1`〜`dc_arity4` として足し、失敗を見る。失敗しなければ試験器側の原因なので、`tests/cpp/conformance_report_test.cpp` に同じ形の `tests.lgt` を足して S として直す
- [ ] **Step 2:** 原因を直す。テストとスイートを流す
- [ ] **Step 3:** コミット `[phase5] discontiguous の述語を読み込めるようにする（directives.pl、合格 +5）`

### Task 7: 丸め系評価子（9.1.1、9.1.6）

**Files:**
- Modify: `docs/iso-notes.md`、`docs/iso-notes.ja.md`、`src/builtins/arith_float.cpp`
- Test: `tests/pl/arith.pl`

- [ ] **Step 1:** iso-notes に 2 問を書き、持ち主の答えを待つ。問 1「`floor(9)` などで整数を受けたとき A: `type_error(float, 9)`（`iso` 旗が真） B: 整数をそのまま返す」。問 2「`round(-2.5)` は A: -3（0 から遠い方） B: -2（`floor(x + 1/2)`）」。答えが出るまでこのタスクのコードは書かない
- [ ] **Step 2:** 答えに合わせた失敗するテスト `round_neg_half`、`floor_int`、`ceiling_int`、`truncate_int`、`fip_int`、`ffp_int` を `tests/pl/arith.pl` に書く
- [ ] **Step 3:** 実装し、テストとスイートを流す。コミット `[phase5] 丸め系評価子を ISO に合わせる（9.1.6、arith.pl と合格 +N）`

### Task 8: 判断待ちの解消

**Files:**
- Modify: `docs/iso-notes.md`、`docs/iso-notes.ja.md`、答えが B のときだけ該当コードと ADR

- [ ] **Step 1:** iso-notes の未決 6 問（7.8.3、9.3.1.3 d、7.5.4、8.11.8、`memberchk/2` と `iso_stream_property_2_01`、8.11.1）とタスク 1 の N の問いを、1 回の報告に 1 つずつ持ち主に聞く
- [ ] **Step 2:** 答えを `Decision (日付): A.` の形で書く。7.5.4 が B なら ADR 0004 を置き換える ADR 0008 を先に書いて承認を得てから実装する。それ以外で B のものは、その述語 1 つを 1 セッションで、タスク 9 と同じ手順で実装する
- [ ] **Step 3:** A のままの不合格は仕分け表で「解釈の違い（iso-notes の見出し）」と書く

### Task 9: 残りの I を 1 述語ずつ

**Files:** 述語ごとに `src/builtins/` の 1 ファイルと `tests/pl/` の対応ファイル

- [ ] **Step 1:** 仕分け表で I のまま残る行のうち、件数が最も多い述語を 1 つ選ぶ
- [ ] **Step 2:** スイートの入力をそのまま `tests/pl/` に写して失敗を見る
- [ ] **Step 3:** 直す。テストとスイートを流し、仕分け表から消す
- [ ] **Step 4:** コミット `[phase5] <述語> の <内容> を直す（<条項>、<テスト>、合格 +N）`
- [ ] **Step 5:** 仕分け表に I が残る限り、次のセッションでこのタスクを繰り返す

---

## 後半: 日本語対応

### Task 10: 全角記号モード

**Files:**
- Create: `src/read/fullwidth.cpp`
- Modify: `src/read/lexer.cpp`、`src/read/lexer_literal.cpp`、`docs/iso-notes.md`（`Section 4 Flag japanese_punctuation` の決定を「フェーズ5で字句解析に効かせる」に更新）、`docs/iso-notes.ja.md`
- Test: `tests/pl/japanese.pl`、`tests/cpp/lexer_test.cpp`

**Interfaces:**
- Produces: `std::optional<char32_t> halfwidth_punctuation(char32_t c)`。`（）［］，、：` を `()[],,:` に写し、それ以外は `std::nullopt`。`。` と `「」` は文脈が要るので字句解析器が個別に扱う
- Consumes: 旗 `japanese_punctuation`（`flags.cpp` の case 8）

- [ ] **Step 1:** 失敗するテストを `tests/pl/japanese.pl` に書く。旗の切り替えは各テストの中で行い、最後に `false` に戻す
  - `fw_same_term`: 旗を真にして `親（太郎，一郎）。` を読んだ項が、`親(太郎,一郎).` を読んだ項と `==`
  - `fw_list`: `X = ［1，2］。` が `X == [1,2]`
  - `fw_touten`: `f（a、b）。` が `f(a,b)`
  - `fw_colon`: `a：b。` が `:(a,b)`
  - `fw_kagi`: `X = 「東 京」。` が `X == '東 京'`、`writeq` の出力が `'東 京'`
  - `fw_kuten_not_decimal`: `X = 1。` が `X == 1` で、`1。5` は小数にならない（構文エラー）
  - `fw_off_symbol`: 旗が偽のとき `X = 、` が `X == '、'`、`。` は記号原子として今と同じ項になる
  - `fw_quoted_kept`: 旗が真でも `'（）'` と `"，"` の中身は全角のまま
  - `fw_char_code_kept`: 旗が真でも `0'（` は `（` の符号位置
  - `fw_directive_next_clause`: 一時ファイル `tests/tmp/全角.pl` に `:- set_prolog_flag(japanese_punctuation, true).` と `全（1）。` を書いて `consult` し、`全(1)` が呼べる
- [ ] **Step 2:** 失敗を見る
- [ ] **Step 3:** 字句解析器が文字を 1 つ取り出す入口で、引用の外のときだけ `halfwidth_punctuation` を通す。`。` は後ろが配置文字か入力の終わりのときだけ終止符にする。`「` は引用原子の開始とし、終わりは `」`。中の規則（`'` を普通の文字にするか、`\` のエスケープを使うか）は iso-notes に「A: `'…'` と同じエスケープで、`'` は普通の文字 B: エスケープなし」を書いて決めてから実装する
- [ ] **Step 4:** `ctest` を流す。`fw_off_symbol` と既存の `syntax.pl` の往復が変わっていないことを見る
- [ ] **Step 5:** コミット `[phase5] 全角記号モードを字句解析に効かせる（SPEC 第4節、japanese.pl の fw_* 10 件）`

### Task 11: メッセージを Prolog の事実へ移す

**Files:**
- Create: `docs/adr/0007-messages.md`、`docs/adr/0007-messages.ja.md`、`lib/messages_ja.pl`、`lib/messages_en.pl`、`src/machine/messages.cpp`
- Modify: `src/machine/session.cpp`、`src/main_mosa.cpp`、`src/builtins/flags.cpp`
- Test: `tests/cpp/session_test.cpp`、`tests/pl/flags.pl`

**Interfaces:**
- Produces: 事実 `message(Key, Format)`（モジュール `messages_ja` と `messages_en` がそれぞれ持つ。Format は `format/2` の書式原子）。C++ 側は `std::string render_message(Impl& machine, std::string_view key, Cell args)`。旗 `message_language`（値 `ja` か `en`）
- Consumes: フェーズ4の `library(...)` の探索（`$MOSA_LIB`、実行ファイルからの `lib/`）、`library(format)`

- [ ] **Step 1:** ADR 0007 を書いて持ち主の承認を得る。決めること: 事実の名前と引数、言語ごとのモジュール、旗の既定値の決め方（`LANG` が `ja` で始まれば `ja`、それ以外は `en`。`LANG` の読み取りは `src/platform/`）、カタログが読めないときの表示（`<Key>: ` の後に引数を `writeq` で並べる）。承認までコードを書かない
- [ ] **Step 2:** 失敗するテスト。`session_test` に `uncaught_ja`: 旗 `ja` で `throw(foo)` を打つと標準エラーが `警告: ゴールは例外を送出しました: foo` を含む。`uncaught_en`: 旗 `en` で `Warning: goal raised exception: foo` を含む。`message_missing_key`: カタログを読まない設定で落ちず `goal_raised: foo` を含む。`tests/pl/flags.pl` に `ml_values`: `set_prolog_flag(message_language, fr)` → `domain_error(flag_value, message_language+fr)`
- [ ] **Step 3:** `session.cpp` と `main_mosa.cpp` の `警告: …` の文字列を鍵 `goal_raised`、`heap_limit` に置き換える。REPL のヘルプに「日本語の変数は `_太郎` のように下線で始める」を鍵 `help_japanese_variables` で足す
- [ ] **Step 4:** 旗を足したので `docs/iso-notes.md` に `Section 7 Flag message_language` を Decision 付きで書く
- [ ] **Step 5:** テストを流す。コミット `[phase5] 例外の表示を messages_ja.pl と messages_en.pl へ移す（ADR 0007、session_test 3 件）`

### Task 12: 構文エラーの鍵と位置（SPEC 第4節、第7節）

**Files:**
- Modify: `src/read/lexer_literal.cpp`、`src/builtins/read_pred.cpp`、`lib/messages_ja.pl`、`lib/messages_en.pl`、`tests/pl/read.pl`
- Test: `tests/pl/read.pl`、`tests/pl/japanese.pl`、`tests/cpp/session_test.cpp`

**Interfaces:**
- Consumes: ADR 0007 で決めた Formal（`syntax_error(Key)`、Key は英語の原子）と Context（`context(PI, stream(S, Line, LinePos, CharNo))`、問題のトークンの置き場は ADR 0007 による）

- [ ] **Step 1:** `tests/pl/read.pl` の `read_syntax` と `read_incomplete` の期待値を ADR 0007 の形に直す（期待値の変更はこの ADR のコミットに限る）。`japanese.pl` に `syntax_col_japanese`: `'あいう ).'` と `'abc ).'` を読んだときの `LinePos` が等しい。`syntax_ja_shown`: `session_test` で旗 `ja` のとき `親(.` の表示が `行 1 列` と `親` を含む
- [ ] **Step 2:** 失敗を見る
- [ ] **Step 3:** C++ の日本語の理由文（`整数が範囲を超えている`、`予期しないトークン`、`項が閉じていません` など）を鍵に置き換え、文面は `messages_*.pl` に移す。`grep -rnP '"[^"]*[\x{3040}-\x{30ff}\x{4e00}-\x{9fff}]' src` が `src/machine/iso_suite.cpp` の報告書の見出しと注釈以外を出さないこと
- [ ] **Step 4:** テストを流す。コミット `[phase5] 構文エラーを鍵と位置で投げ、文面を messages へ移す（read.pl、japanese.pl、session_test）`

### Task 13: `~t~|` の東アジア幅

**Files:**
- Modify: フェーズ4で決まる `format` の実装（`lib/format.pl` か C++）
- Test: `tests/pl/japanese.pl`

**Interfaces:**
- Consumes: `library(japanese)` の `読み幅/2`（フェーズ4）、`src/unicode_tables.inc` の `k_wide_ranges`

- [ ] **Step 1:** 失敗するテスト。`fmt_wide_pad`: `with_output_to(atom(A), format("~w~t~10|~w", [日本, x]))` が `A == '日本      x'`（`日本` は幅 4、空白 6）。`fmt_ascii_pad`: 同じ書式で `ab` なら空白 8。`fmt_mixed`: `日a` は幅 3
- [ ] **Step 2:** 桁の計算を `読み幅/2` と同じ表に置き換える。テストを流す
- [ ] **Step 3:** コミット `[phase5] format の ~t~| を東アジア幅で数える（japanese.pl の fmt_* 3 件）`

### Task 14: Windows コンソールの UTF-8

**Files:**
- Create: `include/mosa/console.hpp`、`src/platform/console.cpp`
- Modify: `src/main_mosa.cpp`、`src/platform/README.md`、標準入力のストリームの読み取り元
- Test: `CMakeLists.txt` の `mosa_stdin` と同じ形の `mosa_utf8_pipe`

**Interfaces:**
- Produces: `void prepare_console()`（Windows では `SetConsoleCP(65001)` と `SetConsoleOutputCP(65001)`、他の OS では何もしない）。`std::optional<std::string> read_console_line()`（標準入力が端末の Windows だけ `ReadConsoleW` で UTF-16 を読み UTF-8 にして返す。他は `std::nullopt` で、呼び手は今の読み取りに戻る）

- [ ] **Step 1:** 失敗するテスト `mosa_utf8_pipe`: `親(太郎). ` を含む UTF-8 をパイプで渡し、`read(X), write(X), nl` の出力のバイト列が入力の `親(太郎)` と一致する。3 OS の CI で流す
- [ ] **Step 2:** `prepare_console` を `main_mosa.cpp` の最初で呼ぶ。`read_console_line` を標準入力の読み取り元に入れる
- [ ] **Step 3:** 端末からの `ReadConsoleW` は CI で試せないので、手で確かめる手順（`chcp` に関係なく `mosa` で `X = 親.` が読めて表示される）を `docs/STATUS.md` の Open に書き、確かめた日付を残す
- [ ] **Step 4:** コミット `[phase5] Windows のコンソールを UTF-8 で読み書きする（mosa_utf8_pipe を 3 OS で）`

### Task 15: 日本語のパス

**Files:**
- Create: `src/core/path_utf8.cpp`、`include/mosa/path_utf8.hpp`（`streams.cpp` の `path_from_utf8` を移す）
- Modify: `src/machine/load_path.cpp:18`、`src/machine/suite.cpp:258`、ほかに `std::filesystem::path{std::string...}` で作っている所すべて
- Test: `tests/pl/japanese.pl`

**Interfaces:**
- Produces: `std::filesystem::path path_from_utf8(std::string_view text)`、`std::string path_to_utf8(const std::filesystem::path& path)`

- [ ] **Step 1:** 失敗するテスト `consult_japanese_name`: `open('tests/tmp/日本語の読込.pl', write, S)` で `読込済(はい).` を書いて閉じ、`consult('tests/tmp/日本語の読込.pl')` の後で `読込済(はい)` が成功する。`prolog_tests` の作業ディレクトリがリポジトリの根でなければ `add_test` に `WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}` を足す
- [ ] **Step 2:** macOS と Linux では通り、Windows の CI で失敗することを見る（MSVC の `path(std::string)` は ANSI コードページで解釈するため）
- [ ] **Step 3:** `grep -rn 'filesystem::path{std::string' src` が 0 件になるまで `path_from_utf8` に置き換える。テストを 3 OS で流す
- [ ] **Step 4:** コミット `[phase5] パスを UTF-8 から作り、日本語のファイル名を consult できる（japanese.pl、3 OS）`

### Task 16: `japanese.pl` を SPEC 第4節の最小集合と照らす

**Files:**
- Modify: `tests/pl/japanese.pl`

- [ ] **Step 1:** SPEC 第4節「Minimum set of Japanese tests」の 5 項目それぞれに対応するテスト名を `japanese.pl` の先頭のコメントに書く（往復と `は` 演算子は `op_ha`、`語処` は `sub_atom_go_sho`、`𠮷野家` は `atom_length_yoshinoya`、日本語のファイル名と構文エラーは Task 15 と 12、全角記号は Task 10）
- [ ] **Step 2:** 欠けているものがあれば足す。日本語原子の `writeq` 往復 `wq_tokyo`: `writeq(東京)` が `東京`、`writeq('東 京')` が `'東 京'`
- [ ] **Step 3:** `mosatest tests/pl/japanese.pl` が全件通ることを見る。コミット `[phase5] japanese.pl を SPEC 第4節の最小集合にそろえる（全件通過）`

### Task 17: 出口

**Files:**
- Modify: `src/version.cpp`、`CMakeLists.txt` の `version_pass_regex`、`docs/conformance.md`、`docs/conformance.json`、`docs/STATUS.md`

- [ ] **Step 1:** `mosatest --suite` を流し、合格率が Task 1 の基準値より上であることを見る
- [ ] **Step 2:** `tests/pl/japanese.pl` の全件通過を見る
- [ ] **Step 3:** 版を `mosa 0.0.1 (phase 5)` にし、`ctest` の `mosa_version` を合わせる。3 OS の CI が緑であることを見る
- [ ] **Step 4:** `docs/STATUS.md` を `Phase: 5 done`、`Next: phase 6, mosac` にする。コミット `[phase5] フェーズ5を終える（合格率 R%、japanese.pl 全件、3 OS の CI）`

---

## 仕分け表

2026-10-28、フェーズ4終了時の基準値 2041/2171（94.0%）の不合格 130 件。`docs/conformance.md` の「不合格」節を 1 行 1 件で写した。「別欄」（実行しないディレクトリ）は含めない。Q の備考は `docs/iso-notes.md` の見出しで、「解釈の違い」は決定済みで不合格のまま残すもの。合格に移った行は表から消す。

内訳（Task 1 時点）: S 47、N 36、Q 31、I 15、X 1。
内訳（Task 2 Step 5 後、残り 111 件）: S 24、N 36、Q 35、I 15、X 1。`lgt_write_term_3_043`、`044`、`065`、`066` は出力の捕捉が出来て、変数を `_` だけで書く件として Q に移った。
内訳（Task 2 Step 7 後、残り 108 件）: S 21（`=~=` 14 件はタスク 3、試験器の追補 7 件）、N 36、Q 35、I 15、X 1。

見取り図との違い: `::/2` の 5 件はテストのコードが Logtalk のライブラリ（`os::ensure_file/1`、`lgtunit::deterministic/2`、`integer::between/3`）を呼ぶものなので、N ではなく S にした。`lgt_open_4_31` も同じ理由で S。`log/2` はフェーズ4で足したので N に残らない。

| パス | テスト名 | 類 | タスク | 備考 |
| --- | --- | --- | --- | --- |
| `control/call_1` | `lgt_call_1_17` | Q | 8 | 7.8.3 call/1 and multifile/discontiguous predicates with no clauses |
| `control/call_1` | `lgt_call_1_18` | Q | 8 | 7.8.3 call/1 and multifile/discontiguous predicates with no clauses |
| `control/catch_3` | `lgt_catch_3_13` | S | 2 追補 | 試験器に Logtalk の組込 `ignore/1` が無い |
| `control/if_3` | `if_3_01` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_02` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_03` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_04` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_05` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_06` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_07` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_08` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_09` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_10` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_11` | N | 8 | Section 3 if/3 |
| `control/if_3` | `if_3_12` | N | 8 | Section 3 if/3 |
| `control/soft_cut_2_3` | `lgt_soft_cut_2_3_22` | S | 2 追補 | 試験器が Logtalk のライブラリ（`os::`、`lgtunit::`、`integer::`）を持たない |
| `functions/acos_1` | `iso_acos_1_01` | S | 3 | `=~=` の閾値 |
| `functions/acos_1` | `iso_acos_1_02` | S | 3 | `=~=` の閾値 |
| `functions/asin_1` | `iso_asin_1_01` | S | 3 | `=~=` の閾値 |
| `functions/asin_1` | `iso_asin_1_02` | S | 3 | `=~=` の閾値 |
| `functions/atan2_2` | `iso_atan2_2_01` | S | 3 | `=~=` の閾値 |
| `functions/atan2_2` | `iso_atan2_2_02` | S | 3 | `=~=` の閾値 |
| `functions/atan_1` | `iso_atan_1_02` | S | 3 | `=~=` の閾値 |
| `functions/ceiling_1` | `lgt_ceiling_1_05` | Q | 7 | 丸め系評価子に整数（タスク 7 で iso-notes に書く） |
| `functions/cos_1` | `iso_cos_1_05` | S | 3 | `=~=` の閾値 |
| `functions/exp_1` | `iso_exp_1_02` | S | 3 | `=~=` の閾値 |
| `functions/float_fractional_part_1` | `lgt_float_fractional_part_1_02` | S | 3 | `=~=` の閾値 |
| `functions/float_fractional_part_1` | `lgt_float_fractional_part_1_04` | Q | 7 | 丸め系評価子に整数（タスク 7 で iso-notes に書く） |
| `functions/float_integer_part_1` | `lgt_float_integer_part_1_04` | Q | 7 | 丸め系評価子に整数（タスク 7 で iso-notes に書く） |
| `functions/floor_1` | `lgt_floor_1_06` | Q | 7 | 丸め系評価子に整数（タスク 7 で iso-notes に書く） |
| `functions/integer_power_2` | `iso_integer_power_2_06` | Q | 8 | 9.3.10.3 (^)/2 with a negative integer exponent |
| `functions/integer_power_2` | `iso_integer_power_2_09` | S | 3 | `=~=` の閾値 |
| `functions/log_1` | `iso_log_1_02` | S | 3 | `=~=` の閾値 |
| `functions/pi_0` | `iso_pi_0_01` | S | 3 | `=~=` の閾値 |
| `functions/power_2` | `lgt_power_2_14` | Q | 8 | 9.3.1.3 d (**)/2: negative power of 0 |
| `functions/round_1` | `eclipse_round_1_07` | Q | 7 | `round/1` の負の半分（タスク 7 で iso-notes に書く） |
| `functions/round_1` | `eclipse_round_1_08` | Q | 7 | `round/1` の負の半分（タスク 7 で iso-notes に書く） |
| `functions/round_1` | `lgt_round_1_13` | Q | 7 | 丸め系評価子に整数（タスク 7 で iso-notes に書く） |
| `functions/tan_1` | `iso_tan_1_01` | S | 3 | `=~=` の閾値 |
| `functions/truncate_1` | `lgt_truncate_1_06` | Q | 7 | 丸め系評価子に整数（タスク 7 で iso-notes に書く） |
| `logical_update_semantics` | `logical_update_semantics_retract` | Q | 8 | 7.5.4 Open scans and retract |
| `logical_update_semantics` | `logical_update_semantics_retractall` | Q | 8 | 7.5.4 Open scans and retract |
| `logical_update_semantics` | `logical_update_semantics_clause_retract` | Q | 8 | 7.5.4 Open scans and retract |
| `logical_update_semantics` | `logical_update_semantics_clause_abolish` | Q | 8 | 7.5.4 Open scans and retract |
| `logical_update_semantics` | `logical_update_semantics_negation` | S | 2 追補 | 試験器が Logtalk のライブラリ（`os::`、`lgtunit::`、`integer::`）を持たない |
| `predicates/abolish_1` | `eddbali_abolish_1_07` | Q | 8 | 7.5.4 Open scans and retract |
| `predicates/current_input_1` | `sics_current_input_1_02` | Q | なし | 解釈の違い（8.11.1.3 Domain of current_input） |
| `predicates/current_input_1` | `sics_current_input_1_04` | Q | 8 | 8.11.1 current_input and current_output on closed streams |
| `predicates/current_input_1` | `lgt_current_input_1_06` | I | 4 | `ground/1` が無い |
| `predicates/current_output_1` | `sics_current_output_1_02` | Q | なし | 解釈の違い（8.11.1.3 Domain of current_input） |
| `predicates/current_output_1` | `sics_current_output_1_04` | Q | 8 | 8.11.1 current_input and current_output on closed streams |
| `predicates/current_output_1` | `sics_current_output_1_06` | I | 4 | `ground/1` が無い |
| `predicates/current_prolog_flag_2` | `eddbali_current_prolog_flag_2_12` | Q | なし | 解釈の違い（7.11 Flag unknown: warning） |
| `predicates/current_prolog_flag_2` | `eddbali_current_prolog_flag_2_13` | Q | なし | 解釈の違い（7.11 Flag unknown: warning） |
| `predicates/current_prolog_flag_2` | `commons_current_prolog_flag_2_16` | N | 8 | Section 3 Flags dialect and version_data |
| `predicates/current_prolog_flag_2` | `commons_current_prolog_flag_2_17` | N | 8 | Section 3 Flags dialect and version_data |
| `predicates/current_prolog_flag_2` | `commons_current_prolog_flag_2_18` | N | 8 | Section 3 Flags dialect and version_data |
| `predicates/open_4` | `lgt_open_4_31` | S | 2 追補 | 試験器が `condition(os::operating_system_type(unix))` を扱わない |
| `predicates/open_4` | `lgt_open_4_41` | S | 2 追補 | 試験器が Logtalk のライブラリ（`os::`、`lgtunit::`、`integer::`）を持たない |
| `predicates/open_4` | `lgt_open_4_42` | S | 2 追補 | 試験器が Logtalk のライブラリ（`os::`、`lgtunit::`、`integer::`）を持たない |
| `predicates/open_4` | `lgt_open_4_43` | S | 2 追補 | 試験器が Logtalk のライブラリ（`os::`、`lgtunit::`、`integer::`）を持たない |
| `predicates/read_term_3` | `sics_read_term_3_24` | X | なし | 多倍長整数（フェーズ7） |
| `predicates/set_stream_position_2` | `sics_set_stream_position_2_06` | Q | 8 | 8.11.8 position with reposition(false) |
| `predicates/stream_property_2` | `sics_stream_property_2_03` | Q | なし | 解釈の違い（8.11.1.3 Domain of current_input） |
| `predicates/stream_property_2` | `sics_stream_property_2_05c` | Q | 8 | 8.11 Properties of the standard streams |
| `predicates/stream_property_2` | `sics_stream_property_2_06d` | Q | 8 | 8.11 Properties of the standard streams |
| `predicates/stream_property_2` | `lgt_stream_property_2_08b` | Q | 8 | 8.11 Properties of the standard streams |
| `predicates/stream_property_2` | `lgt_stream_property_2_09c` | Q | 8 | 8.11 Properties of the standard streams |
| `predicates/stream_property_2` | `lgt_stream_property_2_10c` | Q | 8 | 8.11 Properties of the standard streams |
| `predicates/stream_property_2` | `lgt_stream_property_2_29` | I | 9 | `reposition(true)` の流れに `position/1` が出ない |
| `predicates/stream_property_2` | `lgt_stream_property_2_30` | I | 9 | `reposition(true)` の流れに `position/1` が出ない |
| `predicates/stream_property_2` | `lgt_stream_property_2_31` | I | 9 | `reposition(true)` の流れに `position/1` が出ない |
| `predicates/write_term_3` | `lgt_write_term_3_043` | Q | 8 | 7.10.5 a Writing a variable that occurs once |
| `predicates/write_term_3` | `lgt_write_term_3_044` | Q | 8 | 7.10.5 a Writing a variable that occurs once |
| `predicates/write_term_3` | `lgt_write_term_3_065` | Q | 8 | 7.10.5 a Writing a variable that occurs once |
| `predicates/write_term_3` | `lgt_write_term_3_066` | Q | 8 | 7.10.5 a Writing a variable that occurs once |
| `predicates/write_term_3` | `lgt_write_term_3_118` | I | 5 | 書き出し選択肢の検査 |
| `predicates/write_term_3` | `lgt_write_term_3_119` | I | 5 | 書き出し選択肢の検査 |
| `predicates/write_term_3` | `lgt_write_term_3_120` | I | 5 | 書き出し選択肢の検査 |
| `predicates/write_term_3` | `lgt_write_term_3_121` | I | 5 | 書き出し選択肢の検査 |
| `predicates/write_term_3` | `lgt_write_term_3_122` | I | 5 | 書き出し選択肢の検査 |
| `syntax/directives/discontiguous_1` | `discontiguous_1_01` | I | 6 | `discontiguous` の読み込み |
| `syntax/directives/discontiguous_1` | `discontiguous_1_02` | I | 6 | `discontiguous` の読み込み |
| `syntax/directives/discontiguous_1` | `discontiguous_1_03` | I | 6 | `discontiguous` の読み込み |
| `syntax/directives/discontiguous_1` | `discontiguous_1_04` | I | 6 | `discontiguous` の読み込み |
| `syntax/directives/discontiguous_1` | `discontiguous_1_05` | I | 6 | `discontiguous` の読み込み |
| `unicode/builtins` | `lgt_unicode_current_prolog_flag_2_02` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_get_char_2_03` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_open_4_01` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_open_4_02` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_open_4_03` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_open_4_04` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_open_4_05` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_open_4_06` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_open_4_07` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_01` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_02` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_03` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_04` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_05` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_06` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_07` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_08` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_09` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_10` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_11` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/builtins` | `lgt_unicode_stream_property_2_12` | N | なし | 解釈の違い（Section 4 Stream encoding and BOM） |
| `unicode/escape_sequences` | `lgt_unicode_escape_sequence_full_05` | Q | なし | 解釈の違い（Spec Section 4: So characters are symbol characters） |
