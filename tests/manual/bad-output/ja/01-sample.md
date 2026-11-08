# 違う出力

`mosatest --manual` の見本（`manual_fixture_bad_output`）。英語版の `wrong-answer` はわざと違う答えを書いてあり、
検査はその id を出して失敗する。こちらの版は正しい。

<!-- check: right-answer -->
```text
?- X = 1.
X = 1.
```

<!-- check: wrong-answer -->
```text
?- X = 2.
X = 2.
```

`-t halt` が無いと、`mosa` はゴールの後で促しに戻り、入力が無ければ促しを出したまま待つ。読者にもその促しが
見えるので検査器はそれを除かず、この例は失敗する。

<!-- check: waiting-prompt -->
```console
$ mosa -g "write(x), nl"
x
true.
```
