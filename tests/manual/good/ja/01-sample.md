# 見本の章

`mosatest --manual` の見本（`tests/tests.cmake` の `manual_fixture_good`）。ここの例はどれも通る。

問い合わせと答え。入力は `?- ` の後ろの字。

<!-- check: first-query -->
```text
?- X = 親.
X = 親.
```

次の答え。入力は印の中に、読者が打つ行ごとに書く。

<!-- check: more-answers
(X = 1 ; X = 2 ; X = 3).
;

-->
```text
?- (X = 1 ; X = 2 ; X = 3).
X = 1;

X = 2
.
```

エラーの文面はその版の言語で出る。

<!-- check: error-message -->
```text
?- X is 1/0.
警告: ゴールは例外を送出しました: error(evaluation_error(zero_divisor),context((is)/2,_G1))
```

ファイルを書き、章のディレクトリでそれを読むコマンドを動かす。`mosatest` は `mosa` の隣のもの。ファイルを読んで
書き戻したものが同じなら何も出さない。

<!-- file: family-file 家族.pl -->
```prolog
親(太郎, 一郎).
親(太郎, 花子).
```

<!-- check: two-commands -->
```console
$ mosa 家族.pl -g "親(太郎, 花子)" -t halt
true.
$ mosa -g "write('a b'), nl" -t halt
a b
true.
$ mosatest --roundtrip 家族.pl
```

<!-- check: console-session -->
```console
$ mosa 家族.pl
?- 親(X, 花子).
X = 太郎.
?- halt.
```

検査しない。読者の機械による。

<!-- skip: install -->
```console
$ brew install something
```
