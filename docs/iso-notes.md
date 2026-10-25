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

決定（2026-10-25）: 未決 4 は B。`print/1` と `print/2` は `portray/1` を呼ばず、`write/1` と同じ文字列を出す。

## 8.8 clause/2

決定（2026-10-25）: 未決 5 は A。利用者が定義した述語は公開である。`consult` の静的な節、`:- dynamic`、`assert` で作った述語、`retract` で節を空にした動的述語を含む。`clause/2` は定義順に節を返す。事実の本体はアトム `true` である。同じスロットの変数は同じセルで、匿名変数は出現ごとに新しい参照である。本体の `false/0` はコンパイルが `fail` に畳むので、返る本体はアトム `fail` である。組込と制御は `permission_error(access, private_procedure, PI)` である。無い述語は失敗する。節の無い動的述語は失敗するが、非公開ではない。静的な利用者述語は読める。

## 8.8 current_predicate/1

決定（2026-10-25）: 未決 6 は B。`current_predicate/1` は利用者の述語だけを、定義された順に返す。`current_predicate(true/0)` は失敗であり、`permission_error` ではない。空の動的述語は成功する。`abolish` のあとは失敗する。

## 8 入出力テストのメモリストリーム

決定（2026-10-25）: 未決 7 は A。名前は `open_string/2` と `with_output_to/2`。置き場と改行は ADR 0005。

## 7.11 旗 char_conversion

決定（2026-10-25）: 未決 8 は A。旗 `char_conversion` の既定は `off`。`on` のとき読みは写像を使う。写像の初期状態は恒等のまま。

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
