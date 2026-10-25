# ISO 解釈メモ

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

## 8.14 print/1 と print/2

決定（2026-10-25）: 未決 4 は B。`print/1` と `print/2` は `portray/1` を呼ばず、`write/1` と同じ文字列を出す。項の書き出しで登録した。

## 8.14 書き出しオプション

決定（2026-10-25）: `write/1,2` と `print/1,2` は `numbervars(true)`。`writeq/1,2` は `quoted(true)` と `numbervars(true)`。`write_canonical/1,2` は `quoted(true)` と `ignore_ops(true)`。`write_term` の既定は三つとも偽。同じ論理オプションの値が食い違うときは `domain_error(write_option, 後の項)`。`variable_names` は一回で、名前が変数の綴りでなければ `domain_error(write_option, その対)`。束縛済みの Var は無視する。

## 8.14.3 op/3

決定（2026-10-26）: ゴールは `OperatorTable::define` を呼ぶ。検査は具体化、型、ドメイン、許可の順で、名前のリストは全部を確かめてから表を置き換える。拒否のときは表を変えない。0 以外の宣言は、同じ類（前置、中置、後置）の古い指定子を置き換える。優先度 0 はその指定子だけを消す。`|` の優先度 0 は Cor.2 の例どおり削除として成功する。中置で 1 以上 1000 以下、および前置と後置は `permission_error(create, operator, '|')`。コンマは `permission_error(modify, operator, ',')`。`[]` と `{}` は `permission_error(create, operator, そのアトム)`。指示の `:- op` も同じ `define` を使う。

## 8.14.4 current_op/3

決定（2026-10-26）: 検査は引数の順で、最初に当てはまるものを投げる。優先度が変数でなく、0 以上 1200 以下の整数でもなければ `domain_error(operator_priority, Priority)`。アトムや浮動小数もこのドメインエラーである。指定子が変数でなく、`xf` `yf` `xfx` `xfy` `yfx` `fx` `fy` のどれでもなければ `domain_error(operator_specifier, Op_specifier)`。整数や複合項もこのドメインエラーである。演算子が変数でもアトムでもなければ `type_error(atom, Operator)`。優先度 0 は領域に入る。該当が無ければ失敗する。候補は呼び出しのときに写し、やり直しの途中で `op/3` が表を変えても写した候補は残る。名前がアトムのときの並びは `lookup` と同じで、優先度の昇順、同じ優先度では指定子の宣言順である。名前が変数のときは表の並びである。

## 8 with_output_to/2

決定（2026-10-25）: 最初の成功だけを取る。成功のとき `atom/1` と `codes/1` を束縛する。失敗と例外では束縛しない。現在出力は成功しても失敗しても例外でも戻す。ゴールは今の文脈で走る。

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
