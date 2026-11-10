# フェーズ9 ISO の穴埋めと堅牢性 設計

日付: 2026-11-11
状態: 承認待ち（所有者の「A か B か」の答えを待つ。下の「所有者への問い」）

## 意図

SPEC 第9節のフェーズは 8 で終わっており、次のフェーズは SPEC に無い（STATUS「the next phase waits for the owner」）。0.1.2 の既知の制限（CHANGELOG）のうち、ISO/IEC 13211-1 が要求する組み込み述語が無いもの（8.2.4 `subsumes_term/2`、8.3.11 `acyclic_term/1`。SPEC 第3節 6「第8章のすべての組み込み述語」に反する）と、実装限界として iso-notes 7.1 に残したもの（reader だけが入れ子 1 段ごとに再帰する。Windows では 600 段ほどで落ちる）、フェーズ8が残した文法規則の制限 3 つを、ひとつのフェーズで閉じる。

読む人は、このあと実装計画を書き、実装するエージェントである。所有者が A を選ぶまで、実装しない。

## 候補と推奨（所有者への問い）

次のフェーズの候補は 2 つある。

- **A: ISO の穴埋めと堅牢性（この文書）**。SPEC の目標と非目標を変えずに（第9節の行と第5節のライブラリ表の追記だけで）、目標 1（ISO 準拠）の穴と、公開した既知の制限を閉じる。`subsumes_term/2`、`acyclic_term/1`、繰り返しで動く reader、`Name//Arity` のエクスポート、文法規則の本体の `*->`、`library(dcg)` の `expand_term/2`。規模はフェーズ8より少し大きい（9 タスク。下の「規模と順序」）。
- **B: 多倍長整数**。SPEC 第1節は「フェーズ6以降に任意機能として検討」とし、Cell の BIG タグ（ADR 0002）を予約してある。SPEC 第1節と第3節 5（整数は 64 ビット）の改訂、ADR 0025（BIG の箱の形、GC の印付け、mosac の定数の出し方）、旗 `bounded` を `false` にして `max_integer`/`min_integer` を外すこと（7.11.1.2）、reader と writer と `number_codes/2`、評価可能な関数子のすべて、標準順序と比較、準拠スイートの `unbounded`（111 件）と別欄のビット演算 10 件ほどを分母に入れること、が要る。標準ライブラリだけで書く（GMP は依存に加えない。AGENTS.md）。規模はフェーズ6に近い（10 タスク以上）。

推奨は **A を フェーズ9、B を フェーズ10 の候補** とすること。理由は、A は SPEC を変えずに目標 1 の違反（組み込み述語の欠落）を直し、既知の制限として公開したものを減らすのに対し、B は SPEC の非目標を目標に変える決定を先に要し、A の reader の件（深い入れ子で落ちる）は B の数の読み書きにも関わるので先に片づけるほうがよいからである。

**所有者への問い: A か B か。** 答えが A なら、この文書を承認済みにして計画（`docs/superpowers/plans/2026-11-11-phase9-iso-completion.md`）へ進む。B なら、この文書は保留し、B の設計を別に書く。

## 規模と順序

9 タスク。AGENTS.md の「1 セッション 1 タスク（1 述語、1 ファイル、1 バグ）」に合わせて切る。1〜2 と 3〜5 と 6〜8 は互いに独立で、順は提案（私の判断）。

1. `acyclic_term/1`（8.3.11）と、準拠スイートの範囲表への追加
2. `subsumes_term/2`（8.2.4）と、準拠スイートの範囲表への追加
3. ADR 0011 の改訂（reader を「明示したスタックで動く歩き手」に加える）。コードより先に書き、所有者の承認を得る
4. reader の演算子の部分（`parse_expr`、`parse_prefix`、`parse_name`、`apply_prefix`、`parse_rest`）を枠のスタックで動かす
5. reader の括弧の部分（引数、リスト、丸括弧、波括弧）を同じスタックに載せ、1 MiB の下で 1,000,000 段のテストを 3 OS で通す
6. ADR 0024 の改訂 3（6〜8 をまとめて）と、エクスポート・インポートの表と `dynamic/1`・`discontiguous/1`・`multifile/1` の `Name//Arity`
7. 文法規則の本体の `*->`
8. `library(dcg)` の `expand_term/2`
9. 文書を閉じる（CHANGELOG、マニュアル第14章、SPEC 第3節と第9節、README、iso-notes 7.1、STATUS）。CI の 11 ジョブの通過を確かめ、フェーズを閉じるかを所有者に問う

## 目標

- `subsumes_term/2` と `acyclic_term/1` が `system` にあり、規格の Examples（8.2.4.4、8.3.11.4）を `tests/pl/` に写した行がすべて通る。準拠スイートの `predicates/subsumes_term_2` と `predicates/acyclic_term_1` がディレクトリとして範囲表に入り、合格率が 98.4% を下回らない。分母に入るのは `subsumes_term_2` の 14 件だけである（全通過なら 2150/2185）: 両ディレクトリの STO の件（`subsumes_term_2` の 15〜17、`acyclic_term_1` の 02〜03）は旗 `coinduction` の if 側と else 側（`- test`）の両方に書かれ、ランナーはこの旗をいつも偽にするので、`arg_3` や `copy_term_2` の同じ件と同じく件ごとに別欄に残る。`acyclic_term_1` の 01 は `succeeds/1` の形で、ランナーは `test/2,3` しか数えない。ランナーを広げること（`succeeds/1` を数える、`coinduction` を supported にする）は非目標。
- ランナーが `subsumes_term/2` を自前で判定するのをやめる。`src/machine/lgt_exec.cpp` の `runner_functor` と `lgt_judge.cpp` の `check_subsumes` は、mosa に組み込みが無かったのでテスト本体の `subsumes_term(...)` を `lgt_shape.cpp` の `lgt_subsumes` で判定している。組み込みができたら `subsumes_term` だけ横取りを外し、mosa の組み込みに判定させる（`variant`、`subsumes`、`=~=` は Logtalk の判定語なので残す）。これで `get_char_2`、`op_3`、`predicate_property_2`、`functions/power_2`、`control/catch_3` など 10 ファイルほどの判定が組み込みを通る。合格数が 1 件でも減れば組み込みの誤りとして直す（横取りに戻さない）。
- reader が入れ子の深さで C++ のスタックを使わない。`f(f(...))`、`[[...]]`、前置演算子の連なり、右結合（`xfy`）の連なり、丸括弧の連なりのそれぞれ 1,000,000 段を、1 MiB のスタックの下で読んで書き戻せる（ctest `stack_deep_read`、`tests/stack/small_stack.cmake`）。所要時間は長さに比例する。
- 既存の読み方は 1 バイトも変わらない。`tests/pl/syntax.pl`、`read.pl`、`operators.pl`、準拠スイートの `syntax/` と `read_term`、`mosatest --diff` の差分 0 が守る。
- `:- module(m, [greet//0])` と `use_module(library(x), [a//1])` と `:- dynamic(a//1).` が通り、`greet/2` を意味する。
- 文法規則の本体の `(C *-> T ; E)` と `(C *-> T)` がソフトカットとして訳される。
- `library(dcg)` の `expand_term(Rule, Clause)` が、`-->` の項をローダーと同じ節に訳し、それ以外の項はそのまま返す。
- インタプリタと mosac が訳したコードで同じに動く。差分テストの一致を崩さない。

## 非目標

- 多倍長整数（B。フェーズ10 の候補）。
- `term_expansion/2` と `goal_expansion/2` のフック。`assertz/1` が `-->` を訳すこと（ADR 0024 の決定のまま。SWI も GNU も訳さない。利用者は `expand_term/2` を挟む）。
- 準拠スイートのランナーを広げること（`succeeds/1`・`fails/1`・`throws/2` を数える、旗 `coinduction` を supported にする）。どちらも 30 ファイル規模で分母が動き、別のフェーズの仕事である。
- 準拠スイートの不合格 35 件の掘り下げ。`unicode/builtins` の 21 件は決定 A（iso-notes 第4節）、`::/2` の 4 件は Logtalk 専用、`unknown=warning` と `domain_error(stream, …)` は iso-notes の決定どおり。`ignore/1` は非 ISO で、1 件のために入れない。
- portray フックと `with_output_to/2` と指令の入れ子の `run_loop`（iso-notes 7.1 の 2 段落目）。アトム表の GC。
- `ctest -j` での時間の比の不安定さ（STATUS「Tests」）。Windows の日本語ロケールの未検証事項。
- Cell の表現（ADR 0001/0002）と IR（ADR 0003/0009）の変更。
- このフェーズでの公開（0.1.3 か 0.2.0 か）。公開は所有者の依頼で別に行う。

## アンチ目標（これをやると技術的に動いても失敗）

- reader の書き換えで、構文エラーの位置やトークン（`syntax(stream(S, Line, LinePos, CharNo), Token)`、ADR 0007）が 1 件でも変わること。
- 1,000,000 段を通すために、テストの段数を下げたり、CTest の子のスタック上限に頼ること（`small_stack.cmake` の 1 MiB が物差し）。
- 準拠率を守るために、`subsumes_term_2` をディレクトリごと別欄に残したまま、またはランナーの横取りを残したまま（組み込みを一度も通さずに）実装したと言うこと。

## 制約

- AGENTS.md のとおり。C++20 標準ライブラリと CMake だけ。関数 60 行、ファイル 800 行。生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロを使わない。エラーは `throw_error`/`make_*_error` の族だけ。`src/platform/` の外に OS 依存を書かない。
- 規格の Examples を `tests/pl/` に先に写してから実装する（TDD）。エラーのテストは `Formal` だけを比べる。
- 識別子は英語、コメントとコミットメッセージは日本語。文書は英語が正で `*.ja.md` を同じコミットで直す。
- 変えにくい決定（reader の枠の形、`//` の意味）は ADR の改訂として先に書く。

## 決めたこと

### 1. `acyclic_term/1`（8.3.11、Cor.2）

- モード `acyclic_term(@term)`。項が無限でない（循環していない）とき真。エラーは無い（8.3.11.3）。
- `src/builtins/types.cpp` にすでにある `acyclic_terms(machine, roots)`（明示したスタックで歩く）をそのまま使う。1 ファイルの追加は要らない。ファイル頭のコメントに節番号・モード・エラー・テストファイル名を足す。
- テストは `tests/pl/types.pl` に 8.3.11.4 の Examples を写す（`acyclic_term(a)` 真、`X = f(X), acyclic_term(X)` 偽、など規格の行のまま）。
- `src/machine/lgt_suite.hpp` の `kPredicateScope` に `"acyclic_term_1"` を足す。別欄から分母に移るのはこの表による（「一覧に無い子は別欄」）。このディレクトリは 01 が `succeeds/1`、02〜03 が STO なので分母に入る件は 0 で、実装の証拠は `tests/pl/types.pl` の行である。表に入れるのは、ディレクトリごと別欄に置く理由（述語が無い）が消えるからで、以後は件ごとの別欄の行として見える。

### 2. `subsumes_term/2`（8.2.4、Cor.2）

- モード `subsumes_term(@term, @term)`。Cor.2 の定義は `\+ \+ (term_variables(Specific, V1), unify_with_occurs_check(General, Specific), term_variables(V1, V2), V1 == V2)`。真偽はこれと同じにし、束縛は残さない（`\+ \+` と同じ）。エラーは無い（8.2.4.3）。
- 実装は `src/builtins/terms.cpp` の `functor/3` と同じ「印を取る → `do_unify` → `restore_state`」の形で、成功しても `restore_state` で戻す。`Specific` の変数の並びは `term_variables/2` の歩き手（明示したスタック）で取り、単一化のあとで同じ並びの各セルがまだ別々の未束縛の変数であることを確かめる。
- 単一化は `unify_with_occurs_check` ではなく既定の `do_unify`（出現検査なし）を使う（私の判断）。理由: 循環していない入力では両者の真偽は同じである（出現検査が落ちる束縛は `Specific` のどれかの変数を複合項に束縛するので、変数の並びの検査が同じ場合を偽にする）。循環した入力では、mosa の `unify_with_occurs_check/2` は iso-notes 8.2.2 の決定 B で「すでに循環した項への束縛も失敗」するため、Cor.2 の字面どおりに作ると `X = f(X), subsumes_term(f(_), f(X))` が偽になり、SWI とスイートの STO の行（`lgt_subsumes_term_2_15` は真を期待）に反する。`do_unify` なら真になり、歩き手は循環した項でも止まる（ADR 0011）。これは STO（7.3.3）の解釈なので、iso-notes に「8.2.4 subsumes_term/2 と循環した項」として A（8.2.2 の決定 B を継いで偽）か B（既定の単一化で真。SWI とスイートに同じ）かを書き、確認前は B で進める。
- テストは `tests/pl/unify.pl` に 8.2.4.4 の Examples を写す（`subsumes_term(a, a)` 真、`subsumes_term(f(X,Y), f(Z,Z))` 真、`subsumes_term(f(Z,Z), f(X,Y))` 偽、`subsumes_term(g(X), g(f(X)))` 偽、`subsumes_term(X, f(X))` 偽、`subsumes_term(X, Y), subsumes_term(Y, f(X))` 真）。
- `kPredicateScope` に `"subsumes_term_2"` を足す（14 件が分母に入る。上の「目標」）。ランナーの横取り（`lgt_exec.cpp` の `runner_functor`、`lgt_judge.cpp` の `check_subsumes`）から `subsumes_term` を外す。
- mosac: 組み込み述語は実行時ライブラリ側で動くので、codegen の変更は無い。`tests/pl/unify.pl` と `types.pl` は差分テストの対象に入っている。

### 3. 繰り返しで動く reader（iso-notes 7.1、ADR 0011 の改訂）

今の reader は `parse_expr → parse_prefix → {apply_prefix | parse_arguments | parse_list | parse_paren | parse_curly} → parse_expr` と `parse_rest → parse_expr` の相互再帰で、入れ子 1 段が C++ の 1 フレームである（`src/read/reader.cpp` 192〜292 行、`reader_bracket.cpp`）。

- 再帰を、`std::vector<Frame>` に載せた「枠」と 1 つのループに替える。枠は「いま何の被演算子を待っているか」を表し、種類は次の 6 つ: 前置演算子の被演算子（`op`）、中置演算子の右の被演算子（`op`、左の項）、関数表記の引数（関数子、集めた引数）、リストの要素（集めた頭、`|` の後か）、丸括弧の中、波括弧の中。各枠はその被演算子の最大優先順位を持つ。
- ループは 2 つの状態を行き来する。「優先順位 P 以下の項を待つ」では 1 つの基本項を読む（開き括弧や前置演算子なら枠を積んで続ける。`6.3.4.1` の `- 1`、`-(1)` と `- (1)` の区別、`can_start` による前置演算子かアトムかの判断は今の関数のまま）。「優先順位 Q の項を得た」では `find_follower` で中置・後置を続け、中置なら枠を積んで「待つ」へ、続きが無ければ上の枠を 1 つ降ろして還元する（引数を足して次の引数へ、閉じ括弧で複合項を作る、前置・中置の項を作る）。枠が空になれば読み終わり。
- 失敗（`Parsed::failure`）はその場で返し、枠の列は捨てる。`bad_token`、`max_arity`、`needs_minus`（2^63）、`reject_empty_comma`、`reject_headless_bar`、`take_bar_tail`、`absorb_full_stop` の判断と、エラー項の位置とトークンは今のまま。
- 置き場所は `src/read/reader.cpp`（演算子）と `reader_bracket.cpp`（括弧）。枠の型とループが入って 800 行を超えるなら `reader_frames.cpp` に分ける。関数は 60 行以内で、還元は枠の種類ごとに 1 関数。
- テストは `tests/stack/deep_read.pl`（と `.expected`）に 5 つの形を 1,000,000 段で書き、`tests/tests.cmake` に `stack_deep_read` を `small_stack.cmake`（1 MiB）で登録する。深い項は実行時に `atom_codes/2` と `read_term/2` で組み立てて読み、`write_canonical/1` で書き戻す（ファイルに 2 MB の字面を置かない）。mosac 側も同じ reader を使うので、`mosac_long_list` と同じ形で 1 つ（`f(f(...))` 100,000 段のファイルを `mosac build` で読む）を足す。
- ADR 0011 の改訂として、「reader はまだ再帰する」の文を消し、枠の形を「決定」に 3 行で書く。所有者の承認を得てから 4〜5 に入る。
- iso-notes 7.1 の 3 段落目（reader の限界）は、閉じるときに「フェーズ9で reader も繰り返しにした」に書き替える。

### 4. `Name//Arity`（エクスポート、インポート、宣言）

- `src/module/qualify.cpp` の `read_indicator` を、`N//A` を `N/(A+2)` として受けるように広げる。呼び手はエクスポートの表（`qualify.cpp`）とインポートの表（`import.cpp` の 2 箇所）だけなので、`abolish/1` と `current_predicate/1` と `retract/1` は ISO のまま `/` だけを受ける（8.9.4.3、`foo//1` は `type_error(predicate_indicator, foo//1)` のまま）。
- `dynamic/1`、`discontiguous/1`、`multifile/1` の PI の読み取りは別の場所にある（実装者が探す。`src/machine/load*.cpp` か `src/builtins/database.cpp`）。同じ広げ方をする。
- エラーは `/` と同じ順（`N` が未束縛なら `instantiation_error`、アトムでなければ `type_error(atom, N)`、`A` が整数でなければ `type_error(integer, A)`、負なら `domain_error(not_less_than_zero, A)`、`A+2 > 1024` なら `representation_error(max_arity)`）。
- `predicate_property/2`、`current_predicate/1`、`listing` は `N/(A+2)` を見せる（`//` の形は宣言の書き方であって、述語の名前ではない）。
- テストは `tests/pl/modules.pl`（エクスポートとインポート）と `dcg.pl`（`dynamic(a//1)` のあと `assertz(a(S0, S))` が通る）。

### 5. 文法規則の本体の `*->`

- `src/machine/dcg.cpp` の制御構造の訳に `*->` を足す。`(C *-> T ; E)` は `(C' *-> T' ; E')`、裸の `(C *-> T)` は `(C' *-> T')`。`S0, S` の通し方は `->` と同じ（`C` は `S0 → S1`、`T` は `S1 → S`、`E` は `S0 → S`）。`Step` に 1 つ足し、名前を選ぶ行（180 行付近）に `*->` を足す。
- テストは `dcg.pl` に、`([x] *-> [y] ; [z])` が `x y` で成功し、`z` で `E` へ行き、`C` が複数解を持つとき `T` が各解で動く行。CHANGELOG の例（非終端記号 `(*->)/4` を呼んでしまう）が消えたことを確かめる行。

### 6. `library(dcg)` の `expand_term/2`

- `expand_term(+term, ?term)`。第 1 引数（deref 後）が `H --> B` なら、ローダーが `take_term` で使うのと同じ `src/machine/dcg.cpp` の訳で `H' :- B'` を作って第 2 引数と単一化する。そうでなければ第 1 引数をそのまま単一化する。エラーは訳のエラーと同じ（`instantiation_error`、`type_error(callable, Culprit)`、`type_error(list, Culprit)`、ADR 0024）。第 1 引数が未束縛なら `instantiation_error`。
- 置き場所は `library(dcg)`（`lib/dcg.pl`、`:- module(dcg, [expand_term/2]).`）。非 ISO の述語は `library(*)` に置くのが SPEC 第3節の規則で、`system` の例外（`is_list/1`、`*->`、`phrase/2,3`）は SPEC に書き出してあるものだけにする（私の判断。`system` に置くなら SPEC 第3節に例外を足すことになる）。訳は C++ なので、`library(format)` が `'$format_float'/4` を使うのと同じ形で、`system` の隠し組み込み `'$dcg_translate'/2`（`src/builtins/dcg_translate.cpp`）を `lib/dcg.pl` から呼ぶ。SPEC 第5節のライブラリ表に `library(dcg)` の行を足す。
- `term_expansion/2` のフックは呼ばない（非目標）。CHANGELOG の制限は「`assertz/1` は訳さない。`library(dcg)` の `expand_term/2` を挟む」に書き替え、`term_expansion/2` と `goal_expansion/2` が無いことは残す。
- テストは `tests/pl/lib_dcg.pl`（`lib_strings.pl` と同じ形）に、`expand_term((a --> [x], b), C), assertz(C), phrase(a, [x, y])`（`b --> [y]`）と、`expand_term(foo(1), X)` が `X == foo(1)`、エラーの行。`tests/pl/modules.pl` に `strings_needs_use` と同じ「`use_module` 無しでは `existence_error`」の行。

### 7. 準拠スイートと完了条件

完了条件は機械的に確かめられるものだけにする（SPEC 第9節）。

1. `tests/pl/types.pl` と `unify.pl` に写した 8.3.11.4 と 8.2.4.4 の行が `mosa` で通る。`mosatest --suite` が `predicates/subsumes_term_2` と `predicates/acyclic_term_1` をディレクトリとして別欄に出さず（残るのは STO の件ごとの行だけ）、`subsumes_term` の判定がランナーではなく組み込みを通り、合格率が 98.4% を下回らない（全通過なら 2150/2185）。
2. ctest `stack_deep_read` が 1 MiB の下で 3 OS で通る（5 つの形、各 1,000,000 段）。
3. `tests/pl/dcg.pl` と `modules.pl` の新しい行が通り、`mosatest --diff` の差分が 0。
4. CI の 11 ジョブが通る。

SPEC 第9節の表に 9 行目を足す: 「ISO の穴埋めと堅牢性: `subsumes_term/2`、`acyclic_term/1`、繰り返しで動く reader、`Name//Arity`、文法規則の `*->`、`library(dcg)` の `expand_term/2`」、完了条件は上の 1〜3。SPEC 第3節の「Grammar rules (phase 8)」の段落の「`term_expansion/2`、`goal_expansion/2`、`expand_term/2` は含まない」を「`expand_term/2` はフェーズ9で `library(dcg)` に」に直し、第5節のライブラリ表に `library(dcg)` を足す。

## 実装者に任せること

- 枠の型の正確な形（`std::variant` か `enum` と共用の欄か）と、還元の関数の切り方。60 行と 800 行の規則の中で決める。
- `dynamic/1` などの PI の読み取りの場所の特定と、`read_indicator` と共通化するかどうか。
- `deep_read.pl` の 5 つの形の組み立て方（`atom_codes/2` で字面を作るか、`numlist` 風に再帰で作るか）。1 MiB の下で組み立て自体が落ちないこと。
- `subsumes_term/2` の変数の並びの比べ方（セルの同一性で比べる。`==` の歩き手を使うか、`term_variables` の結果を 2 回取って要素ごとに `==` するか）。
- ランナーの横取りを外したとき、判定が組み込みに移る 10 ファイルほどの件のうち、合格が変わるものの洗い出し（変わるなら組み込みの誤り）。
- タスク 1〜2 と 6〜8 の順。3〜5 は順のまま。

## リスク

- reader の書き換えが既存の読み方を変える。守りは `syntax.pl`（第6章の例の往復）、`read.pl`、`operators.pl`、準拠スイートの `syntax/` と `read_term_3`、差分テスト。タスク 4 と 5 のレビューはこれらの通過を条件にする。
- 1,000,000 段の読みが時間の上限（CTest の既定 1500 秒には収まるが、`stack_*` の既定の上限）を超える。1 段あたり O(1) の枠の積み降ろしなら 1 秒未満。超えるなら設計の誤りであり、段数を下げない。
- `//` を `dynamic/1` で受けると、`dynamic(foo//1)` に `type_error` を期待する準拠スイートの行が落ちるかもしれない。実装者は先に `grep -rn '//' tests/conformance/logtalk/prolog/predicates/dynamic_1` などで確かめ、あれば `dynamic/1` などは広げずにエクスポートとインポートだけにする（iso-notes に書く）。
