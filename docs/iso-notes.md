# ISO 解釈メモ

規格と仕様が食い違うときは、条番号を付けて「A か B か」の質問を 1 つ書く。確認前に実装しない。

## 仕様第4節 𠮷野家のコードポイント

決定（2026-10-25）: Unicode のコードポイント（スカラー値）で数える。`𠮷野家` は U+20BB7、U+91CE、U+5BB6 の 3 個である。`atom_length('𠮷野家', 3)` が正しい。UTF-16 の 4 符号単位にはしない。𠮷 1 文字は `atom_length('𠮷', 1)` であり、`char_code('𠮷', 134071)` である。

## 8.3.9 callable/1

決定（2026-10-25）: B。Corrigendum 2 の 8.3.9.3 のとおり、Errors は None で、`callable(3)` と `callable(X)` は失敗する。
`call/1` はこれまでどおり、変数なら `instantiation_error`、数なら `type_error(callable, 値)` を投げる。

## 8.16 atom_number/2（非 ISO）

未決 2。仕様は名前だけで、モードが無い。
A: 片側がアトム、もう片側が数で、両方向に変換する。
B: モードが無いので述語は登録しない。

## 8.10 findall/4（非 ISO）

未決 3。仕様は名前だけで、引数の意味が無い。
A: 第4引数を結果リストの末尾にする。
B: 意味が無いので述語は登録しない。

## 8.14 print/1 と print/2

未決 4。仕様は 8.14 の並びに置くが、portray/1 には触れない。
A: portray/1 が定義されていればそれを使い、無ければ write/1 と同じ文字列を出す。
B: portray/1 は見ず、write/1 と同じ文字列を出す。

## 8.8 clause/2

決定（2026-10-25）: 未決 5 は A。利用者が定義した述語は公開である。`consult` の静的な節、`:- dynamic`、`assert` で作った述語、`retract` で節を空にした動的述語を含む。`clause/2` は定義順に節を返す。事実の本体はアトム `true` である。同じスロットの変数は同じセルで、匿名変数は出現ごとに新しい参照である。本体の `false/0` はコンパイルが `fail` に畳むので、返る本体はアトム `fail` である。組込と制御は `permission_error(access, private_procedure, PI)` である。無い述語は失敗する。節の無い動的述語は失敗するが、非公開ではない。静的な利用者述語は読める。

## 8.8 current_predicate/1

決定（2026-10-25）: 未決 6 は B。`current_predicate/1` は利用者の述語だけを、定義された順に返す。`current_predicate(true/0)` は失敗であり、`permission_error` ではない。空の動的述語は成功する。`abolish` のあとは失敗する。

## 8 入出力テストのメモリストリーム

未決 7。仕様第8節は open_string と with_output_to/2 相当とだけ書く。名前は ADR 0005 が決める。
A: 名前を open_string/2 と with_output_to/2 にする。
B: 別の名前にする。

## 7.11 旗 char_conversion

未決 8。写像の初期状態は恒等である。旗の既定は書いていない。
A: off。
B: on。

## 7.11 旗 debug

未決 9。既定も、on のときの実行も書いていない。
A: 既定 off。値を変えてもフェーズ3ではトレースしない。
B: 既定 on。

## 7.11 旗 unknown の warning

未決 10。error と fail は実装した。warning の文面はメッセージ目録がフェーズ5であることとぶつかる。
A: 標準エラーに「警告: 」を出してゴールを失敗させる。目録ファイルは作らない。
B: warning は受け付けない。

## 7.11 旗 float_overflow、float_zero_div、float_undefined

未決 11。error 以外の値のアトム名が書いていない。error のときは今の evaluation_error を投げる。
A: 実装者がアトム名を 1 つ決めて、そのとき IEEE 754 の値を返す。
B: error 以外への変更は、名前が決まるまで受け付けない。

## 第4節 旗 invalid_utf8

未決 12。不正な UTF-8 は representation_error(character) か、旗が replace なら U+FFFD である。既定が無い。
A: 既定 error。
B: 既定 replace。

## 第4節 旗 japanese_punctuation

未決 13。字句解析を変えるのはフェーズ5までしない。値の読み取りは false のままである。
A: 値のアトムだけ true にする。
B: 変更を拒否し、値は false のままにする。

## 7.11 旗 version

未決 14。仕様は旗の名前だけを書く。
A: アトムで、version_line() と同じ文字列。
B: 複合項にする。形が回答に含まれるまで本体は書かない。

## 第3節 旗 iso

未決 15。逸脱が iso 旗 true のとき無効だと書く。既定の true か false かは書いていない。
A: 既定 true。
B: 既定 false。

## 8.12 テキストの改行

未決 16。
A: どの OS でも U+000A 1 個を書く。
B: C++ のテキストモードに任せ、処理系の変換を許す。メモリストリームは、どちらの回答でも U+000A 1 個とする。
