# ISO 解釈メモ

規格と仕様が食い違うときは、条番号を付けて「A か B か」の質問を 1 つ書く。確認前に実装しない。

## 8.3.9 callable/1

承認した計画は、変数なら `instantiation_error`、数なら `type_error(callable, 値)` を投げる。
Corrigendum 2 の 8.3.9.3 は Errors が None で、`callable(3)` と `callable(X)` は失敗する。

A: 計画どおり例外を投げる。
B: Corrigendum 2 どおり失敗させる。
