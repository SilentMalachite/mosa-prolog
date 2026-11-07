# 順の違う id

`mosatest --manual` の見本（`manual_fixture_bad_ids`）。英語版と同じ例を違う順に置いたので、版の突き合わせは
このファイルを名指しして失敗する。

<!-- skip: second -->
```console
$ brew install something
```

<!-- check: first -->
```text
?- X = 1.
X = 1.
```
