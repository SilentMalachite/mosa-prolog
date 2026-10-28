[English](iso-notes.md) | 日本語

# ISO 解釈メモ

この文書は [英語版](iso-notes.md) の日本語訳である。食い違うときは英語版に従う。

規格と仕様が食い違うときは、条番号を付けて「A か B か」の質問を 1 つ書く。確認前に実装しない。

## 仕様第4節 𠮷野家のコードポイント

決定（2026-10-25）: Unicode のコードポイント（スカラー値）で数える。`𠮷野家` は U+20BB7、U+91CE、U+5BB6 の 3 個である。`atom_length('𠮷野家', 3)` が正しい。UTF-16 の 4 符号単位にはしない。𠮷 1 文字は `atom_length('𠮷', 1)` であり、`char_code('𠮷', 134071)` である。

## 8.3.9 callable/1

決定（2026-10-25）: B。Corrigendum 2 の 8.3.9.3 のとおり、Errors は None で、`callable(3)` と `callable(X)` は失敗する。
`call/1` はこれまでどおり、変数なら `instantiation_error`、数なら `type_error(callable, 値)` を投げる。

## 8.16 atom_number/2（非 ISO）

決定（2026-10-25）: 未決 2 は A。`atom_number/2` を登録し、両方向に変換する。両方とも変数なら `instantiation_error`。アトムから数は `number_chars` と同じ構文解析で、合わない字面は `syntax_error(読み筋)`。数からアトムは引用符なしの `write_term` の文字列。両方とも具体値なら、一致すれば成功し、違えば失敗する。

## 8.10 findall/4（非 ISO）

決定（2026-10-25）: `findall(Template, Goal, List, Tail)` は差分リストである。解が無ければ List と Tail を単一化し、Tail を `[]` には束縛しない。第4引数はリストとして検査しない。`findall/3` は末尾が `[]` の `findall/4` である。

決定（2026-10-27）: 上の「検査しない」は取り下げる。第4引数が部分リストでもリストでもなければ、収集の前に `type_error(list, Tail)` を投げる。変数の末尾は解の終わりに束縛する。空リストと真のリストも末尾のままである。検査が終わる前に末尾を束縛しない。

## 8.14 print/1 と print/2

決定（2026-10-25）: 未決 4 は B。`print/1` と `print/2` は `portray/1` を呼ばず、`write/1` と同じ文字列を出す。項の書き出しで登録した。

決定（2026-10-27）: 上の B は取り下げる。`portray/1` に節がありゴールが成功するとき、`print/1` と `print/2` はその書き出しを項の字面にする。節が無いときは `write/1` と同じ手続きである。節はあるが失敗するときは、その項は `write/1` と同じ字面で、複合項の引数だけ再び `print` にかける。`portray/1` は組込にしない。`write/1` は `portray` を見ない。失敗した `portray` の書き出しは残さない。

## 8.14 書き出しオプション

決定（2026-10-25）: `write/1,2` と `print/1,2` は `numbervars(true)`。`writeq/1,2` は `quoted(true)` と `numbervars(true)`。`write_canonical/1,2` は `quoted(true)` と `ignore_ops(true)`。`write_term` の既定は三つとも偽。同じ論理オプションの値が食い違うときは `domain_error(write_option, 後の項)`。`variable_names` は一回で、名前が変数の綴りでなければ `domain_error(write_option, その対)`。束縛済みの Var は無視する。

`lgt_write_term_3_118` から `122` は、同じオプションを 2 度書いたとき後の方が勝つことを期待する。`[quoted(true), quoted(false)]` は `A` を書き、`[numbervars(true), numbervars(false)]` は `$VAR(0)` を書き、2 つ目の `variable_names/1` を使う。`lgt_open_4_41`（今は `os::` で止まっている）は `open/4` のオプション `eof_action/1` にも同じことを期待する。上の決定は `domain_error(write_option, LaterTerm)` である（`tests/pl/write.pl` の `opt_conflict`）。調べた範囲では、規格の文面（書き出しオプションの 7.10.4、ストリームのオプションの 7.10.2.11）は、オプションを繰り返したときのことを言っていない。

A か B か: A は `domain_error(write_option, LaterTerm)` のまま（今の決定。5 行は不合格のまま）。B は後のオプションが勝つ。スイートが `write_term/3` と `open/4` に仮定するとおりにし、`opt_conflict` を決定と同じコミットで直す。

決定（2026-10-28）: B。繰り返した書き出しオプションは後の方が勝つ。どの出現も検査はするので、`[quoted(true), quoted(foo)]` は `domain_error(write_option, quoted(foo))` になる。2 つ目の `variable_names/1` は 1 つ目を置き換える。2026-10-25 の決定のうち「`domain_error(write_option, LaterTerm)`」と「`variable_names` は一回」をこれで置き換える。8.14.2.3 が `domain_error(write_option, E)` にするのは要素が正しい書き出しオプションでないときだけで、繰り返した要素はどれも正しい。`open/4` はすでに後の `type/1`、`reposition/1`、`eof_action/1` を勝たせている。テストは `tests/pl/write.pl` の `opt_repeat_*`。

## 8.14.3 op/3

決定（2026-10-26）: ゴールは `OperatorTable::define` を呼ぶ。検査は具体化、型、ドメイン、許可の順で、名前のリストは全部を確かめてから表を置き換える。拒否のときは表を変えない。0 以外の宣言は、同じ類（前置、中置、後置）の古い指定子を置き換える。優先度 0 はその指定子だけを消す。`|` の優先度 0 は Cor.2 の例どおり削除として成功する。中置で 1 以上 1000 以下、および前置と後置は `permission_error(create, operator, '|')`。コンマは `permission_error(modify, operator, ',')`。`[]` と `{}` は `permission_error(create, operator, そのアトム)`。指示の `:- op` も同じ `define` を使う。

## 8.14.4 current_op/3

決定（2026-10-26）: 検査は引数の順で、最初に当てはまるものを投げる。優先度が変数でなく、0 以上 1200 以下の整数でもなければ `domain_error(operator_priority, Priority)`。アトムや浮動小数もこのドメインエラーである。指定子が変数でなく、`xf` `yf` `xfx` `xfy` `yfx` `fx` `fy` のどれでもなければ `domain_error(operator_specifier, Op_specifier)`。整数や複合項もこのドメインエラーである。演算子が変数でもアトムでもなければ `type_error(atom, Operator)`。優先度 0 は領域に入る。該当が無ければ失敗する。候補は呼び出しのときに写し、やり直しの途中で `op/3` が表を変えても写した候補は残る。名前がアトムのときの並びは `lookup` と同じで、優先度の昇順、同じ優先度では指定子の宣言順である。名前が変数のときは表の並びである。

## 8 with_output_to/2

決定（2026-10-25）: 最初の成功だけを取る。成功のとき `atom/1` と `codes/1` を束縛する。失敗と例外では束縛しない。現在出力は成功しても失敗しても例外でも戻す。ゴールは今の文脈で走る。作ったメモリの流れは終わったら閉じる（下の「8 with_output_to のメモリ」）。

## 8.8 clause/2

決定（2026-10-25）: 未決 5 は A。利用者が定義した述語は公開である。`consult` の静的な節、`:- dynamic`、`assert` で作った述語、`retract` で節を空にした動的述語を含む。`clause/2` は定義順に節を返す。事実の本体はアトム `true` である。同じスロットの変数は同じセルで、匿名変数は出現ごとに新しい参照である。本体の `false/0` はコンパイルが `fail` に畳むので、返る本体はアトム `fail` である。組込と制御は `permission_error(access, private_procedure, PI)` である。無い述語は失敗する。節の無い動的述語は失敗するが、非公開ではない。静的な利用者述語は読める。

## 8.8 current_predicate/1

決定（2026-10-25）: 未決 6 は B。`current_predicate/1` は利用者の述語だけを、定義された順に返す。`current_predicate(true/0)` は失敗であり、`permission_error` ではない。空の動的述語は成功する。`abolish` のあとは失敗する。

## 8 入出力テストのメモリストリーム

決定（2026-10-25）: 未決 7 は A。名前は `open_string/2` と `with_output_to/2`。置き場と改行は ADR 0005。

## 7.11 旗 char_conversion

決定（2026-10-25）: 未決 8 は A。旗 `char_conversion` の既定は `off`。`on` のとき読みは写像を使う。写像の初期状態は恒等のまま。

## 8.14.5 char_conversion/2

決定（2026-10-26）: 検査は、来が変数、来が 1 文字のアトムでない、先が変数、先が 1 文字のアトムでない、の順。型は `type_error(character, その引数)`。同じ文字を先にすると、その来の写像を消して恒等に戻す。別の文字へは、来ごとに一つだけ置き換える。`a` を `b` に、`b` を `c` にしても、`a` は `b` のままである。旗が `off` のあいだ、読みは写像を使わない。`on` のときは、引用の外と注釈の外の符号点だけを写す。引用と注釈の中は元の文字である。`0'` のうしろの文字は引用の外なので写す。指示 `:- char_conversion` も同じ検査で、読み込みが失敗したときは旗と写像を戻す。値は `on` と `off` である。

## 8.14.6 current_char_conversion/2

決定（2026-10-26）: 検査は来、先の順。変数は許す。1 文字のアトムでなければ `type_error(character, その引数)`。具体化エラーは投げない。恒等の組は解にしない。初期状態は解が無い。同じ変数を両引数にしても失敗する。計画にある「恒等のとき自分自身に結び付く」は採らない。旗の on と off は問い合わせに影響しない。旗が off でも登録した組は返り、読みは写さない。候補は呼び出しのときの写しで、登録順である。同じ来を置き換えると、その組は末尾に付く。やり直しの途中で写像を消しても、写した候補は残る。`a` を `b` に、`b` を `c` にした組は、そのまま二つの解である。

## 7.4.2 discontiguous/1

決定（2026-10-26）: 指示だけである。引数は述語指示子かそのリストで、`dynamic/1` と同じである。コンマで並べた列は指示子ではない。同じテキストで、隙間より前に宣言した述語だけ、節が離れてよい。宣言が無ければ診断は `述語 名前/アリティ の節が離れている` のまま。宣言は手続きを作らない。節が無ければ `current_predicate/1` は失敗し、呼び出しは `existence_error` である。宣言は次の読み込みへ残さない。次の `consult` はその述語を置き換える。別のアリティや別の述語の隙間は許さない。

`syntax/directives/discontiguous_1`（5 行）は、カンマで区切った並び `:- discontiguous((b/1, c/2)).` を書いており、上の決定がそれを拒むので（「述語指示子ではありません」）読み込みに失敗する。同じファイルの単独とリストの形は読め、`tests/pl/directives.pl` の `dc_arity2` から `dc_arity4` がその形を確かめる。私たちの読みでは、7.4.2 は `dynamic/1`、`multifile/1`、`discontiguous/1` の引数に述語指示子、述語指示子の並び、述語指示子のリストを認める。ただし条文そのものは確かめていない。`dynamic/1` と `multifile/1` も同じく並びを拒む。

A か B か: A は決定のまま（指示子かリストだけ。5 行は不合格のまま）。B は 3 つの指示すべてでカンマの並びも受け付け、各要素は同じ検査をする。確認前は A のまま。

決定（2026-10-28）: B。`dynamic/1`、`multifile/1`、`discontiguous/1` はカンマで区切った並び `(p/1, q/2)` も受け付ける。各要素は述語指示子でなければならず、同じ検査をする。並びの中のリストや誤った要素は、同じ診断で読み込みを失敗させる。2026-10-26 の `discontiguous/1` と `multifile/1` の決定のうち「コンマで並べた列は指示子ではない」をこれで置き換える。テストは `tests/pl/directives.pl` の `dc_seq_*`、`dyn_seq_*`、`mf_seq` と、`tests/cpp/machine_test.cpp` の "comma sequence"。

## 7.4.2 multifile/1

決定（2026-10-26）: 指示だけである。引数は述語指示子かそのリストで、`dynamic/1` と同じである。コンマで並べた列は指示子ではない。`モジュール:PI` はフェーズ4まで書かない。宣言は手続きを作らない。節が無ければ `current_predicate/1` は失敗し、呼び出しは `existence_error` である。宣言は次の読み込みへ残す。残った述語は、次の読み込みが節を足し、置き換えない。同じテキストを二度読むと節は倍になる。宣言が無い述語は置き換わり、解は倍にならない。この読み込みの最初の節より後の宣言は、その読み込みの置き換えを戻さない。その宣言は残り、その次の読み込みは足す。別のアリティや別の述語は足さない。失敗した読み込みは、足した節と宣言を戻す。動的のままの述語は、足しても動的のままである。隙間は許さず、診断は `述語 名前/アリティ の節が離れている` のまま。

決定（2026-10-28）: カンマの並びを受け付ける。7.4.2 discontiguous/1 を参照。

## 7.4.2 initialization/1

決定（2026-10-26）: A。節は例外のときも残す。指示だけである。ゴールはテキストを準備したあとに、ソースの順で一度実行する。引数が変数なら `instantiation_error`、callable でなければ `type_error(callable, Goal)`。この二つは読み込みを失敗させ、節を戻し、ゴールは実行しない。失敗はエラーにしない。読み込みは成功し、節は残り、後の初期化ゴールも実行する。失敗より前の副作用は残る。捕捉されない例外は、`error/2` ならその Formal、それ以外は投げた項を診断にして読み込みを失敗させる。後の初期化ゴールは実行しない。節と、例外より前の副作用は残す。

## 7.4.2 include/1

決定（2026-10-26）: 指示だけである。節は、その指示の位置で同じテキストに入る。離れた節の診断は `述語 名前/アリティ の節が離れている` のまま。入れたファイルの `op`、`discontiguous`、`initialization` も、その位置で同じテキストに効く。初期化ゴールは、テキスト全体を準備したあとに、ソースの順で実行する。相対パスは、その指示があるファイルのディレクトリから開く。文字だけの load は、渡されたパスのまま開く。同じファイルを二度 include すると節は倍になる。開き済みのファイルを再帰で開くと、診断は `パス: ファイルが循環しています` で、節は戻す。引数が変数なら `instantiation_error`、アトムでなければ `type_error(atom, File)`。この二つは読み込みを失敗させ、節を戻す。`[]` は空の指示ではなくファイル名である。開けないときは `パス: ファイルを開けません` で失敗し、節を戻す。規格の `existence_error(source_sink, File)` は使わない。入れ子の構文エラーは、そのファイルのパスと位置を診断に残す。拡張子の無い名前は、そのパスが普通のファイルでなければ、同じ名前に `.pl` を足して開く。名前のまま開けるときと、既に拡張子があるときは足さない。どちらも無ければ、診断のパスは渡された名前のままである。

## 7.4.2 ensure_loaded/1

決定（2026-10-26）: 指示だけである。引数と、開けないときの診断と、拡張子の扱いは include/1 と同じである。変数は `instantiation_error`、アトムでなければ `type_error(atom, File)`。`[]` はファイル名である。開けないときは `パス: ファイルを開けません` で失敗し、節を戻す。規格の `existence_error(source_sink, File)` は使わない。節は、その指示の位置で同じテキストに入る。同じ正規パスを二度 `ensure_loaded` しても、節も初期化も足さない。相対パスと絶対パスが同じファイルなら一度である。`include` したファイル、`consult` したファイル、今開いているファイルも準備済みである。そのあとの `ensure_loaded` は読み直さない。`include` は準備済みでも節を足す。`consult` は読み直して置き換える。自分自身や、互いに `ensure_loaded` するファイルは循環にしない。節を戻す失敗は、準備済みのパスも戻す。初期化の例外で失敗したときは節が残るので、パスも残し、次の `ensure_loaded` は例外を再実行しない。

## 7.4.2 [File]

決定（2026-10-26）: 指示だけである。本体がリストのとき、各アトムを `consult` と同じ手続きで左から読む。空リストは何もしない。要素の `[]` はファイル名である。部分リストと、要素の変数は `instantiation_error`。変数が一つでもあれば型より先である。要素がアトムでなければ `type_error(atom, その要素)`。閉じたリストでなければ `type_error(list, その項)`。`:- X.` はリストではない。相対パスは、その指示があるファイルのディレクトリから開く。文字だけの load は、渡されたパスのまま開く。開けないときは `パス: ファイルを開けません` で失敗し、節を戻す。節は親のテキストに継がない。別の述語を読んでも、親の節は離れない。`consult` は述語を置き換え、同じパスをもう一度読むと初期化ももう一度実行する。そのあとの `ensure_loaded` は読み直さない。親がすでに見た述語へは、そのあとの節を足す。親がまだ見ていない述語は、そのあとの節が置き換える。自分自身を開くと `ファイルが循環しています` で失敗し、節は戻す。初期化の例外は節を残し、後の節と初期化は読まない。リストの途中でそれ以外の失敗をしたときは、それより前に置き換えた節も戻す。

## 7.11 旗 debug

決定（2026-10-25）: 未決 9 は A。既定は `off`。`on` にしてもフェーズ3ではトレースしない。

## 7.11 旗 unknown の warning

決定（2026-10-25）: 未決 10 は B。`unknown` は `error` と `fail` だけを受け付ける。`warning` は受け付けない。

## 7.11 旗 float_overflow、float_zero_div、float_undefined

決定（2026-10-25）: 未決 11 は B。`float_overflow`、`float_zero_div`、`float_undefined` は `error` だけを受け付ける。別のアトム名は作らない。

## 第4節 旗 invalid_utf8

決定（2026-10-25）: 未決 12 は A。`invalid_utf8` の既定は `error`。`replace` のときは U+FFFD を入れる。

## 第4節 旗 japanese_punctuation

決定（2026-10-25）: 未決 13 は A。`set_prolog_flag(japanese_punctuation, true)` は値として `true` を保存する。字句解析は変えない。

## 7.11 旗 version

決定（2026-10-25）: 未決 14 は A。`version` は読み取り専用のアトムで、文字列は `version_line()` と同じである。

## 第3節 旗 iso

決定（2026-10-25）: 未決 15 は A。`iso` の既定は `true` で、読み取り専用である。フェーズ3で新しい挙動は足さない。

## 8.12 テキストの改行

決定（2026-10-25）: 未決 16 は A。テキストファイルもメモリストリームも、どの OS でも改行は U+000A 1 個。C++ のテキストモードには任せない。

## 8.11 ストリーム項

決定（2026-10-25）: ストリーム項は常に `'$stream'(N)`。`N` はストリームのベクタの添字。別名は別の名前であり、項の形を変えない。

## 8.11 標準ストリーム

決定（2026-10-25）: 添字 0 が `user_input` で stdin、1 が `user_output` で stdout、2 が `user_error` で stderr。`reposition` は false。

## 8.11 終端の次の読み

決定（2026-10-25）: 既定の `eof_action` は `eof_code`。終端の次の読みは、テキストが `end_of_file`、バイナリが `-1`。`eof_action(reset)` は別途選べる。

## 8.11 空の入力

決定（2026-10-25）: 位置が源の長さと等しいとき `end_of_stream` は `at`。空の入力を開いた直後は `at`。

## 8.11 位置の項

決定（2026-10-25）: `stream_position(CharNo, Line, LinePos)`。文字数は 0 から、行と行内位置は 1 から。

## 8.11.5.3 f ストリーム引数が具体値

決定（2026-10-25）: B。Cor.2 の 7.12.2 と 8.11.5.3 f のとおり `uninstantiation_error(Stream)`。`open_string/2` の `Stream` もモードが `-` なので同じ Formal。仕様第3節の一覧に `uninstantiation_error/1` を足した。

## 7.10.2.11 eof_action(reset)

決定（2026-10-26）: A。終端のあと `reset` で読むと、位置を先頭に戻して読み直す。テスト `reset rereads`。7.10.2.11 は、終端を越えた印を下ろして、もう一度読む、とだけ書く。ファイルは開いたときにバイト列へ写すので、印だけ下ろすと位置は末尾のままになり、次も終端で `eof_code` と同じになる。承認済みの計画は「ファイルをもう一度読む」なので、先頭へ戻す。これは 7.10.2.11 からの逸脱として残す。

## 7.8.3 call/1 と節の無い multifile・discontiguous

Logtalk の `lgt_call_1_17` と `lgt_call_1_18` は、節の無い `fenix/1`（multifile）と `scattered/2`（discontiguous）の `call/1` を失敗と期待する。7.4.2 の決定と `tests/pl/directives.pl` は、宣言だけでは手続きを作らず、`unknown=error` では `existence_error(procedure, PI)` である。`dynamic/1` だけが空の手続きを作る、と 7.4.2 では読んだ。

A か B か: A は 7.4.2 のまま `existence_error`。B は指示だけで手続きがあり、節が無ければ `call/1` は失敗する。確認前に B は実装しない。

## 8.11.1.3 current_input のドメイン

決定（2026-10-26）: A。`current_input/1`、`current_output/1`、`stream_property/2` に流れでも別名でもない項を渡すと `domain_error(stream_or_alias, S)`。テスト `input_bad`、`output_bad`、`prop_bad_stream`。別名 `user_input` は `current_input(user_input)` で成功する。領域に別名が入るので、Formal は `stream` ではなく `stream_or_alias`。8.11.1.3 a と 8.11.8.3 a の `domain_error(stream, S)` からの逸脱として残す。`set_input/1` など、もとから `stream_or_alias` の述語は変えない。

## 8 with_output_to のメモリ

決定（2026-10-26）: A。`atom/1` と `codes/1` のために作ったメモリの流れは、成功でも失敗でも例外でも停止でも閉じる。成功のときはバイトを束縛してから閉じる。閉じたスロットは再利用しない。その後、`stream_property` は列挙で拾わない。その項を渡すと `existence_error(stream, S)`。呼んだ側が渡した流れは閉じない。ゴールの途中は開いたままなので、中の `current_output` と `stream_property` は見える。

## 準拠スイート

決定（2026-10-26）: 許可は待たない。`inriasuite.pl` は再配布の許諾が無いので同梱しない。フェーズ3の準拠スイートは Logtalk の `tests/prolog` を、コミット `4decb64b598feff542a926a085eef01ec51f5a72` のまま `tests/conformance/logtalk/prolog` に置く。ライセンスはリポジトリと同じ Apache-2.0 で、著作権表示は残す。ファイルは改変しない。仕様第9節の 90% はこのスイートで測る。合格率はこの決定では出さない。

決定（2026-10-28）: A。試験器が 6 ライブラリを取り込むようになったが、別欄のディレクトリ（`between_3`、`succ_2`、`plus_3`、`format_2`、`format_3`、`atomic_list_concat_2`、`atomic_list_concat_3`）はフェーズ5の間は実行しない。分母は 2171 のままにし、フェーズ5の出口は同じ基準値（2041）と比べる。これらの述語は `tests/pl/` のライブラリのテストが確かめる。

## ライセンス

決定（2026-10-26）: リポジトリは Apache-2.0 に揃える。本文は `LICENSE`。Mosa の著作権表示は `Copyright 2026 Mosa Prolog contributors` で、`NOTICE` に置く。Logtalk のテストも同じライセンスで、著作権は Paulo Moura の表示のままである。

## 9.3.1.3 d (**)/2 の 0 の負べき

`lgt_power_2_14` は `**(0, -1)` を `evaluation_error(undefined)` か `evaluation_error(zero_divisor)` と期待する。ECLiPSe の IsoErrata は、9.3.1.3.d の文面が undefined であり、無限大が定義されるので zero_divisor であるべきだと書く。Corrigendum 2 の 9.3.10.3 d は `(^)/2` の同じ形を undefined のまま残している。今は `std::pow(0, 負)` が無限大なので `evaluation_error(float_overflow)` である。

A か B か: A は 9.3.1.3.d の文面どおり `evaluation_error(undefined)`。B はエラータのとおり `evaluation_error(zero_divisor)`。確認前に `float_overflow` は変えない。

## 7.5.4 開いた走査と retract

`logical_update_semantics_retract` は、走査の途中で `retract` しても、呼んだときの節を最後まで試すことを期待する。`retractall` と `abolish` の同じ形も同じである。ADR 0004 は、retract が以後の候補からその節を外し、今の節の残りは残す、と決めている。`tests/pl/database.pl` の `retract_open_later` は `[X = 1, X = 2]` であり、消した `later(3)` は出てこない。

A か B か: A は ADR 0004 のまま、開いた走査から消した節を外す。B は 7.5.4 のとおり、その呼出しは最初の列を試す。確認前に A のままにする。

## 8.11.8 reposition(false) の position

`sics_set_stream_position_2_06` は `reposition(false)` で開いた流れから `stream_property(Stream, position(Position))` を取り、`set_stream_position(Stream, Position)` が `permission_error(reposition, stream, Stream)` になることを期待する。ファイルを写したあとの実際は `fail` である。`stream_property/2` は戻せない流れに `position/1` を出さない（`tests/cpp/stream_test.cpp` の "no position"）。`set_stream_position/2` は、同じ流れに位置の項を渡すとすでに `permission_error(reposition, stream, Stream)` を投げる（同ファイルの "reposition formal"）。

A か B か: A は性質を出さない（今の実装。ゴールは失敗する）。B は `position/1` を出してから `permission_error` にする。確認前は A のまま。B は実装しない。

## memberchk/2 と iso_stream_property_2_01

`memberchk/2` は `library(lists)` であり、フェーズ 3 の組込にはしない。`iso_stream_property_2_01` の表明は、テストファイルにある補助節を、その時点の利用者の節として実行する。行を `fail` のままにする、という読みは撤回する。

A か B か: A は組込の `memberchk/2` を足さない。B は組込として足す。確認前は A。B は実装しない。

## 8.11.1 閉じた流れの current_input と current_output

`sics_current_input_1_04` は、閉じた入力流れ `S` に `current_input(S)` を呼び、`domain_error(stream, S)` を期待する。実際は `existence_error(stream, '$stream'(3))`。`sics_current_output_1_04` は閉じた出力流れへの `current_output(S)` で、同じ食い違いである。8.11.1.3 と 8.11.2.3 が書く `domain_error(stream, Stream)` は、変数でも流れでもない項である。閉じた流れの項は流れの項のままなので、開いていない流れの `existence_error(stream, S)`（`set_input/1` の 8.11.3.3 と同じ Formal）にしている。`tests/pl/stream_prop.pl` の `set_closed` と `out_closed`、読み書きの閉じた流れも同じ Formal である。流れでない項の `stream_or_alias`（上の 8.11.1.3 の決定）は変えない。

A か B か: A は閉じた流れの `current_input/1` と `current_output/1` が `existence_error(stream, S)`（今の実装）。`sics_current_input_1_04` と `sics_current_output_1_04` は残る。B は `domain_error(stream, S)`。確認前は A。B は実装しない。

## 第5節 自動取り込み

決定（2026-10-28）: A。`append/3`、`between/3`、`format/2` などのライブラリの述語は、`use_module` した後にだけ見える。起動時に `user` へ何も取り込まず、未定義の述語を呼んだときにライブラリを読むこともしない。SPEC 第5節の解決規則（自モジュール、既定の取り込み元、`system`）を保ち、利用者のプログラムが自分の `append/3` や `member/2` を取り込みの衝突なしに定義でき、フェーズ6の `mosac` が `use_module` の推移閉包だけをリンクする規則とも合う。宣言なしにこれらを呼ぶ準拠テストは、処理系を変えず、試験器がテストのモジュールへ 6 ライブラリを取り込んで扱う。

## 第5節 モジュールの中の手続きの existence_error

SPEC 第5節は、`user` 以外のモジュールで見つからない述語を `existence_error(procedure, PI)` の culprit でどう書くかを決めていない。ISO の例は `user` で動き、限定のない `Name/Arity` を期待する。

A か B か: A は文脈モジュールが `user` なら `Name/Arity`、それ以外なら `Module:Name/Arity`。B は常に `Name/Arity`。確認まで実装しない。

決定（2026-10-28）: A。`user` の中では ISO の例どおり culprit を `Name/Arity` のままにし、それ以外のモジュールでは `Module:Name/Arity` にして、どこで見つからなかったかを示す。

## 第5節 取り込みの衝突

SPEC 第5節は、自モジュールの述語と取り込んだ述語の衝突を読み込み時の `permission_error(import_into, ...)` とするが、残り 2 つの引数を決めていない。

A か B か: A は `permission_error(import_into, procedure, Name/Arity)`。B は `permission_error(import_into, procedure, Module:Name/Arity)`（`Module` は取り込み元）。確認まで実装しない。

決定（2026-10-28）: B。culprit は取り込み元のモジュールを付けた `Module:Name/Arity` とし、どの取り込みがぶつかったかを示す。上の `existence_error` で選んだモジュール付きの culprit とそろえる。

## 第5節 原子でないモジュールの Module:Goal

SPEC 第5節は未知のモジュールを `existence_error(module, M)` と決めるが、`M` が変数でも原子でもない場合（`1:true`）を決めていない。変数は `instantiation_error` である。

A か B か: A は `type_error(atom, M)`。B は `type_error(module, M)`。確認まで実装しない。

決定（2026-10-28）: A。`1:true` は `type_error(atom, 1)` とする。ISO 13211-1 に `module` という型は無いので、既存の型名の範囲に収める。

## 第5節 演算子 meta_predicate と as

SPEC 第5節の例は `:- meta_predicate 全員(1, ?).` と `[姓名分解/3 as 分解]` を書く。これを読むには `meta_predicate` と `as` が演算子でなければならないが、どちらも ISO の演算子表に無い。

A か B か: A は `op(1150, fx, meta_predicate)` と `op(700, xfx, as)` を `system` の演算子表に足し、すべてのモジュールで見えるようにする。B はどちらも足さず、`meta_predicate(...)` と `as(PI, Alias)` の標準形だけを受ける。確認まで実装しない。

決定（2026-10-28）: A。2 つの演算子を `system` の演算子表に足し、SPEC 第5節の例がどのモジュールでも書いたとおりに読めるようにする。これは ISO の演算子表（6.3.4.4）からの逸脱であり、`as` を演算子の位置で普通の原子として使うプログラムは読み方が変わる。

## 第5節 演算子 :

SPEC 第5節は `?- 家系:祖父(太郎, Z).` と書く。これを読むには `:` が中置の演算子でなければならない。`:` は ISO の演算子表（6.3.4.4）に無く、`src/read/operator_table.cpp` も意図して入れていない。

A か B か: A は `op(600, xfy, :)` を `system` の演算子表に足す。SWI-Prolog と同じで、Logtalk の判定器が一時的に足す演算子とも一致する。B は何も足さず、`:(M, G)` の標準形だけを受ける。

決定（2026-10-28）: A。`op(600, xfy, :)` を `system` の演算子表に置く。上の `meta_predicate` と `as` と同じく、ISO の演算子表（6.3.4.4）からの逸脱である。

## 第5節 Module:Goal の中のカット

SPEC 第5節は `Module:Goal` が `Goal` を `Module` の文脈で動かすと書くが、`Goal` の中のカットがどこまで届くかを決めていない。ISO 13211-1 の 7.8 では `call/1` はカットに不透明で、`,/2`、`;/2`、`->/2` は透明である。

A か B か: A は透明。`p :- member(X, L), user:!.` の `!` は `p` の節まで切る。SWI-Prolog が限定付きの本体ゴールをコンパイルするときと同じである。B は `call/1` と同じく不透明で、カットは `Module:Goal` の中だけに効く。

決定（2026-10-28）: A。`:/2` はカットに透明とする。`call/N` を通したゴールは、`call/N` 自身がカットの境界を置くので不透明のままである。

## 第5節 呼び出せないゴールの Module:Goal

SPEC 第5節は、`user:(fail, 1)` のように `Module:Goal` の `Goal` を呼び出せないときの `type_error(callable, ...)` の culprit を決めていない。ISO 13211-1 の 7.8.3.3 は `call/1` の引数を culprit とする。

A か B か: A は `Module:Goal` 全体。`call(user:(fail, 1))` は `call/1` に渡した項をそのまま示し、`call(user:1, a)` が `user:1` を示すのともそろう。B は `(fail, 1)` のように `Goal` だけ。

決定（2026-10-28）: A。culprit は `Module:Goal` 全体とする。

## 第5節 取り込み同士の衝突

SPEC 第5節は、局所の述語と取り込んだ述語が 1 つの名前空間を共有すると書き、局所の定義と取り込みがぶつかったときのエラーを決めている。1 つのモジュールが同じ `Name/Arity` を別々の 2 つのモジュールから取り込んだときは決めていない。

A か B か: A は、2 つ目の取り込みで読み込みを `permission_error(import_into, procedure, From:Name/Arity)` で失敗させる。`From` は 2 つ目の取り込み元である。同じ取り込み元からもう一度取り込むのは許す。B は、後の取り込みが前を黙って置き換える。

決定（2026-10-28）: A。1 つの名前に取り込み元が 2 つあるのは衝突であり、局所の定義と取り込みがぶつかったときと同じ Formal にする。

## 第5節 公開されていない述語の取り込み

SPEC 第5節は `use_module(File, Imports)` で取り込む述語を指定できるとするが、指定した述語が読み込んだモジュールの公開リストに無いときを決めていない。

A か B か: A は、読み込みを `existence_error(procedure, Module:Name/Arity)` で失敗させる。B は、SWI-Prolog が警告付きで行うように、それでも取り込む。どちらでも、`except([...])` の中の公開されていない項目は無視する。

決定（2026-10-28）: A。取り込めるのは公開された述語だけで、無いものは `existence_error(procedure, Module:Name/Arity)` で読み込みを失敗させる。

## 第5節 モジュールでないファイルの use_module

SPEC 第5節は 1 ファイルが 1 モジュールに対応すると書くが、最初の項が `module/2` でないファイルを `use_module/1,2` がどう扱うかを決めていない。

A か B か: A は、読み込みを `permission_error(load, source_sink, File)` で失敗させ、読んだ節も戻す。SWI-Prolog もモジュールでないファイルを拒む。B は、`consult/1` と同じく文脈モジュールに読み、取り込むものは無い。

決定（2026-10-28）: A。`use_module/1,2` はモジュールのファイルだけを受ける。

## 第5節 初期化の例外で途中までしか読まれていないファイル

入れ子の読み込みの `initialization/1` のゴールが例外を投げると、それまでに読んだ節は残り、外側のファイルはその指令で読むのをやめる。外側のファイルには読み込み済みの印が残るので、後の `use_module/1,2` や `ensure_loaded/1` は残りを読まず、作りかけのモジュールが見えていた。

A か B か: A は、最後まで読んでいないファイルの印を外し、次の `use_module/1,2` か `ensure_loaded/1` で読み直す。節とモジュールは今どおり残す。B は印を残す。

決定（2026-10-28）: A。

## 第5節 未知のモジュールへの節操作

SPEC 第5節は `assertz(家系:親(a, b))` を許すが、そのモジュールがまだ無いときを決めていない。

A か B か: A は `M:G` と同じく `existence_error(module, M)`。B は、SWI-Prolog と同じくモジュールを作って節を足す。

決定（2026-10-28）: A。モジュールを作るのは `module/2` だけである。未知の `M:` を付けた `asserta/1`、`assertz/1`、`retract/1`、`retractall/1`、`clause/2`、`abolish/1` は `existence_error(module, M)` にする。

## 第5節 取り込んだ名前への節操作

SPEC 第5節は `assertz/1` などで作った述語が、宣言または追加したモジュールに属すると書くが、文脈モジュールがその名前を取り込んでいるときを決めていない。

A か B か: A は、呼び出しと同じく取り込みをたどり、取り込み元のモジュールの定義を操作する。B は文脈モジュール自身の述語だけを見て、`assertz/1` は取り込みとぶつかる。

決定（2026-10-28）: A。データベースの組み込み述語は、モジュール自身の述語、次に明示した取り込み（`use_module/1,2` と、consult したモジュールのファイルの公開）を見る。既定の取り込み元はたどらないので、モジュールの中の `assertz(p(1))` が `user:p/1` に足されることはない。存在エラーと同じく、`permission_error(modify, static_procedure, PI)` も `user` の外の述語を `M:N/A` で示す。

## 8.8.2 モジュールの中の current_predicate/1

SPEC 第5節は、`current_predicate(N/A)` と `current_predicate(M:N/A)` が何を列挙するかを決めていない。

A か B か: A は、そのモジュール自身の述語だけ。`M` が変数なら全モジュールをたどり、`M` も束縛する。B は、自身の述語と取り込んだ述語。

決定（2026-10-28）: A。限定の無い `current_predicate/1` は文脈モジュール自身の述語を列挙するので、`user` での結果はモジュールを入れる前と同じである。

## 8.4.3 sort/2

`sort/2` は ISO の組み込み（8.4.3）だが、SPEC 第7節の段階表に無く、実装もされていない。同梱スイートの `sort_2` のテストは `docs/conformance.md` の別欄にある。`keysort/2`（8.4.4）は段階 G に system として載っている。

A か B か: A はフェーズ4の Task 12 で `keysort/2` と並べて `sort/2` を `system` に作り、SPEC 第7節の段階 G に足す。B はフェーズ5へ回す。確認まで実装しない。

決定（2026-10-28）: A。`sort/2` はフェーズ4の Task 12 で `keysort/2` と並べて `system` に作り、8.4.3.4 の Examples をテストにする。SPEC 第7節の段階 G に載せた。

## 第7節 forall/2

`forall/2` は SPEC 第7節の段階 G（「非 ISO の便利述語（`library(*)`、第5節の表）」）に載り、第5節もモジュールを跨いで動くべき述語に挙げるが、第5節の表の 6 モジュールのどれも公開していない。

A か B か: A はフェーズ4の Task 9 で `forall/2` を meta 述語として `system` に作る。B はフェーズ4では作らない。確認まで実装しない。

決定（2026-10-28）: A。`forall/2` はフェーズ4の Task 9 で meta 述語として `system` に作り、呼び手の文脈モジュールで `\+ (Cond, \+ Action)` を動かす。6 モジュールの外にある段階 G の述語でフェーズ4が足すのは、これだけである。

## ISO/IEC 13211-2 7.2.2 predicate_property/2

SPEC 第5節は段階 F に `predicate_property/2` を挙げるが、頭部が変数のときと、無いモジュールを指したときを決めていない。

A か B か: A は、頭部が変数なら 13211-2 の DTR と同梱のスイート（`commons_predicate_property_2_16`）のとおり `instantiation_error`、無い `M:` なら `M:G` と同じ `existence_error(module, M)`。B は、SWI-Prolog のように見える述語を列挙し、無いモジュールでは失敗する。

決定（2026-10-28）: どちらも A。組み込み述語の性質は `defined`、`built_in`、`static` で、制御構文とゴールを受ける組み込み述語には `meta_predicate(Head)`（例: `findall(?, 0, -)`）も付く。利用者の述語は呼び出しと同じ順で引き、性質はこの順に `defined`、`dynamic` か `static`、`exported`、`imported_from(M)`（文脈モジュールの明示の取り込みのときだけ）、`multifile`、`meta_predicate(Head)`（宣言したとき）、`number_of_clauses(N)`。それ以外の性質の項は `domain_error(predicate_property, P)`。

## 第5節 モジュールの中の演算子

SPEC 第5節は、`op/3` を宣言したモジュールに閉じ、公開リストの `op(P, T, N)` を取り込み側でも効かせ、`user` の演算子をすべてのモジュールに見せると決める。モジュールの中の `op(0, T, N)` の働きと、取り込みリスト `[PI, ...]` が公開の演算子を持ち込むかは決めていない。

A か B か: A は、モジュールの中の `op(0, T, N)` がそのモジュールの中だけ `user` と ISO の核の同じ類の演算子を隠す（SWI-Prolog と同じ）。公開の演算子は `use_module/1`、`except(...)`、consult したモジュールのファイルで入り、取り込みリストでは入らない。B は、`op(0, T, N)` がそのモジュール自身の定義だけを消し、公開の演算子はどの取り込みでも入る。

決定（2026-10-28）: どちらも A。引く順はモジュールの固有層、次に `user` の表（ISO の核を含む）。`op/3`、`current_op/3`、`read_term/2,3` と `write` の仲間は呼び手の文脈モジュールを使う。モジュールのファイルの `:- op` 指令はそのモジュールへ入る。`user` へ取り込んだ演算子は `user` の表に入るので、すべてのモジュールから見える。

## 7.4.2.8 ファイルごとの旗と initialization/1

SPEC 第5節は、`double_quotes` と `japanese_punctuation` を読み込むファイルごとに戻す。7.4.2.8 は `initialization/1` のゴールを Prolog テキストの準備が終わった後に実行するが、そのファイルが設定した旗が見えるかは決めていない。

A か B か: A は、旗を戻す前にゴールを実行し、ファイルの旗のまま見せる。B は、先に旗を戻し、読み込み直後のクエリと同じ旗を見せる。

決定（2026-10-28）: B。ファイル自身の項はすでにその旗で読まれている。違いが出るのは、ゴールが実行中に読むものと `current_prolog_flag/2` だけである。ファイルの書式で読みたいゴールは、自分で旗を設定する。

## 第5節 負の整数を受けた succ/2

`succ/2` は規格に無い。フェーズ4の計画は SWI-Prolog と同じ `type_error(not_less_than_zero, -1)` を期待し、Logtalk のスイート（`lgt_succ_2_09`、`lgt_succ_2_10`）は `domain_error(not_less_than_zero, -1)` を期待する。

A か B か: A は `length/2` と同じ `domain_error(not_less_than_zero, N)`。B は `type_error(not_less_than_zero, N)`。

決定（2026-10-28）: A。`not_less_than_zero` は 7.12.2 c の領域であり、`length/2` もすでに領域の誤りとして投げている。

## 第5節 format/2 の誤り

`format/1,2,3` は規格に無い。Logtalk のスイート（`format_2`）は引数の誤りを ISO の形で決めている。`~c`、`~d`、`~e`、`~f`、`~n` と `~*` の回数や数の引数は `is/2` と同じく評価し、`~d` は整数の結果を要し（`type_error(integer, X)`）、`~a` は原子を要する（`type_error(atom, X)`）。引数の不足と余りには、どの `domain_error(_, _)` でも受ける。

A か B か: A は問題ごとの Formal。引数が尽きたら `domain_error(non_empty_list, [])`、残ったら `domain_error(empty_list, 残り)`、未知の指示の文字 `C` は `domain_error(format_directive, C)`（末尾の単独の `~` は `'~'`）。B は 3 つとも `domain_error(format_control, Format)` で、書式全体を罪人にする。

決定（2026-10-28）: A。`non_empty_list` は 7.12.2 c の領域であり、罪人からどこが悪いかが分かる。負の回数は `length/2` と同じく `domain_error(not_less_than_zero, N)`。

## 7.9.1 関数子と引数のどちらの誤りを先に出すか

`X is foo(bar)` では `foo/1` も `bar/0` も評価できない。7.12.1 は 2 つの誤りの条件のどちらを出すかを処理系に任せる。Mosa は引数を先に評価して `bar/0` を出していた。Logtalk のスイート（`format_2` の `foo(bar)` を渡す `~d` と `~f`）は `foo/1` を期待する。

A か B か: A は今のまま引数を先に評価する（`type_error(evaluable, bar/0)`）。B は引数を評価する前に主関数子を確かめる（`type_error(evaluable, foo/1)`）。

決定（2026-10-28）: B。7.9.1 は主関数子が評価できるかで分かれてから引数を評価する。外側の関数子が悪ければ、引数に手を付ける前に止まる。

## 第5節 結果が数の atomic_list_concat/2,3

`atomic_list_concat/2,3` は規格に無い。Logtalk のスイート（`atomic_list_concat_2`、`atomic_list_concat_3`）は結果の引数が複合項なら `type_error(atom, a(1))` を期待するが、`atomic_list_concat([1, 2], 12)` の `12` のような数の扱いは決めていない。

A か B か: A は `type_error(atom, 12)`。結果の引数は変数か原子に限る。B は文字として比べ、成功させる。

決定（2026-10-28）: A。8.16.2.3 は `atom_concat/3` の第3引数に同じ決まりを置いており、Logtalk の複合項の場合とも揃う。

## 第5節 空の区切りで分ける atomic_list_concat/3

`atomic_list_concat/3` が分けるとき（リストが決まっておらず原子が決まっているとき）、区切りが空の `''` では分け方が 1 つに決まらない。

A か B か: A は `domain_error(non_empty_atom, '')`。B は失敗させるか、1 文字ずつに分ける。

決定（2026-10-28）: A。黙って失敗するより誤りの方が何が悪いかを伝え、1 文字ずつ分けるのは `atom_chars/2` の仕事である。

## 第5節 as の別名を通した節操作

`use_module(別源, [静/1 as 静か])` の後の `assertz(静か(2))` は、別名 `静か/1` を通して静的な手続き `別源:静/1` を変えようとする。8.9.1.3 は `permission_error(modify, static_procedure, PI)` で手続きを示すが、呼び手が別名でたどり着いたときにどちらの名前を使うかは決めていない。

A か B か: A は取り込み元の手続き `別源:静/1`。別名でない取り込み（`家系:親/2`）と同じ形。B は呼び手が書いた別名 `静か/1`。

決定（2026-10-28）: A。誤りは変えられない手続きについてのもので、別名はその手続きの別の名前にすぎない。別名でない取り込みはすでに取り込み元を出しており、罪人のモジュールから、どの宣言を dynamic にすればよいかが分かる。

## 第3節 if/3

`if/3` は規格にも SPEC 第3節にも無い。`control/if_3` の 12 件はこれを呼び、`existence_error(procedure, if/3)` になる。

A か B か: A は SPEC 外なので実装せず、12 行は不合格のまま残す。B は SPEC 第3節に `if/3` を足して実装する。確認前は A のまま。

## 第3節 旗 dialect と version_data

旗 `dialect` と `version_data` は 7.11 にも SPEC 第3節にも無い。`commons_current_prolog_flag_2_16` から `18` は `domain_error(prolog_flag, dialect)` と `domain_error(prolog_flag, version_data)` になる。

A か B か: A は SPEC 外なので実装せず、3 行は不合格のまま残す。B は SPEC 第3節に `dialect`（`mosa`）と `version_data`（`mosa(Major, Minor, Patch, [])`）を足して実装する。確認前は A のまま。

## 第4節 ストリームの符号化と BOM

`unicode/builtins` の不合格 21 行は、IANA の名前 `'UTF-8'`、BOM、バイナリの流れに符号化が無いことを期待する。SPEC 第4節と第3節の 7 はすでに旗 `encoding` を `utf8` に固定し、すべての流れに既定が `utf8` の `encoding` 選択肢を与えており、Mosa はそれに従う。`current_prolog_flag(encoding, utf8)` であり、`open/4` は `encoding(utf8)` を受け付け、バイナリの流れも `encoding(utf8)` を出す。そのため `open/4` に `encoding('UTF-8')` を渡すと `domain_error(stream_option, encoding('UTF-8'))` になり、`lgt_unicode_current_prolog_flag_2_02` は名前で失敗し、`lgt_unicode_stream_property_2_07` と `09` はバイナリの流れが `encoding/1` を出すので失敗する。`open/4` の選択肢 `bom/1`、流れの性質 `bom/1`、テキストの流れを読むときの BOM の読み飛ばしは、規格（7.10.2）にも SPEC にも無い。

A か B か: A は SPEC のまま（名前は `utf8` だけ、すべての流れが `encoding/1` を出す、BOM は扱わない）で、21 行は不合格のまま残す。B は SPEC 第4節を改め、`'UTF-8'` を別名として受け付け、バイナリの流れから `encoding/1` を外し、選択肢と性質の `bom/1` を足し、テキストの入力で BOM を読み飛ばしてから実装する。確認前は A のまま。

決定（2026-10-28）: A。これらの選択肢、性質、名前は規格の外であり、SPEC はすでにすべての流れの符号化を `utf8` に固定している。21 行は解釈の違いとして不合格のまま残す。テキストの入力での BOM の読み飛ばしは、タスク 15 で別に問うことがある。

## 9.3.10.3 (^)/2 の負の整数の指数

`iso_integer_power_2_06` は `2^(-1)` が `type_error(float, 2)` になることを期待する。`src/builtins/arith.cpp` は Corrigendum 2 の 9.3.10.3 e を、指数が -1 より小さいときだけ float の型エラーと読み、`2^(-1)` は `evaluation_error(undefined)` にしている。

A か B か: A は今の読みのまま（`evaluation_error(undefined)`）。B は 1 と -1 以外の整数の底に負の整数の指数なら `type_error(float, Base)`。確認前は A のまま。

## 8.11 標準ストリームの性質

`sics_stream_property_2_05c` と `lgt_stream_property_2_08b` は、`user_input` が `eof_action(reset)` を出すことを期待する。`sics_stream_property_2_06d`、`lgt_stream_property_2_09c`、`lgt_stream_property_2_10c` は、`user_output` と `user_error` が `mode(append)` を出すことを期待する。Mosa は `eof_action(eof_code)`（「8.11 終端の次の読み」の既定）と `mode(write)` を出す。

A か B か: A は `eof_code` と `write` のまま。B は `user_input` を `eof_action(reset)`、`user_output` と `user_error` を `mode(append)` にする。確認前は A のまま。

## 仕様第4節 So の文字は記号文字

`lgt_unicode_escape_sequence_full_05` は `writeq('🀙🀚🀛🀜🀝🀞🀟🀠')` が原子を引用することを期待する。SPEC 第4節は `So` の非 ASCII 文字を記号文字にするので、この 8 文字は記号文字の原子（6.4.2）になり、`writeq`（7.10.5）は引用せずに書く。この行は解釈の違いとして不合格のまま残す。承認済みの SPEC に従うので、未決の問いは無い。

## 7.10.5 a 1 度だけ出る変数の書き方

`lgt_write_term_3_043`、`044`、`065`、`066` は、書いた変数が `_` の後に 1 文字以上続くことを期待する。Mosa は項の中に 1 度だけ出る変数を `_` だけで書き、2 度以上出る変数を `_G` と番号で書く（`f(_G1,_G1,_)`）。7.10.5 a は、変数の字面は処理系依存で `_` で始まるとだけ言う。

A か B か: A は 1 度だけ出る変数を `_` のままにする（今のライタ）。B はすべての変数に `_G1` のように `_` の後 1 文字以上の名前を付ける。確認前は A のまま。

## 9.1.6 丸め関数に整数を渡したとき

`lgt_ceiling_1_05`、`lgt_floor_1_06`、`lgt_round_1_13`、`lgt_truncate_1_06`、`lgt_float_integer_part_1_04`、`lgt_float_fractional_part_1_04` は、`X is floor(9)` などが `type_error(float, 9)` になることを期待する。9.1.6 は `floor`、`ceiling`、`round`、`truncate` の型を F → I、`float_integer_part` と `float_fractional_part` の型を F → F と定めるので、整数の引数は型の誤りになる。Mosa は前の 4 つでは整数をそのまま返し（`floor(9) = 9`）、後の 2 つでは浮動小数点数を返す（`float_integer_part(9) = 9.0`）。SPEC はこれらの評価子を挙げるが、引数の型には触れていない。

A か B か: A は 9.1.6 に従い、6 つすべてで整数の引数に `type_error(float, N)` を投げる。B は今のまま整数を受け付ける。確認前は B のまま。

決定（2026-10-28）: A。この 6 つに整数を渡すと `type_error(float, N)` を投げる。`N` は評価した後の値である（`floor(4 + 5)` は `type_error(float, 9)`）。`integer/1` はこの 6 つに含まれず、今も整数を受け付ける（`src/builtins/arith.cpp` の `float_only`、`tests/pl/arith.pl`）。Mosa にはまだ `iso` 旗が無いので、どのモードでもこの動きになる。

## 9.1.6 round/1 の負の半分

`eclipse_round_1_07` と `08` は、`round(-3.5)` が `-3`、`round(-4.5)` が `-4` になることを期待する。9.1.6 は `round(x)` を `floor(x + 1/2)` と定めるので、ちょうど半分は正の無限大の側へ丸まる。Mosa は `std::round` を使っており、半分を 0 から遠い側へ丸めるので `-4` と `-5` を返す。正の半分はどちらでも同じ結果になる（`round(2.5) = 3`）。

A か B か: A は今のまま半分を 0 から遠い側へ丸める（`round(-2.5) = -3`）。B は 9.1.6 に従い `floor(x + 1/2)` にする（`round(-2.5) = -2`）。確認前は A のまま。

決定（2026-10-28）: B。`round/1` は 9.1.6 に従い、ちょうど半分を正の無限大の側へ丸める。端数は `floor(x)` を引いて取り出す。この引き算は誤差を生まないので、`0.49999999999999994` や 2^52 を超える奇数で丸めが二重に起きない（`src/builtins/arith_float.cpp`、`tests/pl/arith.pl` の `round_*`）。
