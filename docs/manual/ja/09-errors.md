[English](../en/09-errors.md) | 日本語

# 9. エラーと例外

この章では、ゴールがエラーを起こしたときに Mosa が出す警告を読み、`catch/3` でエラーを捕まえ、`throw/1` で自分の例外を投げます。

## 手順

1. 第 4 章の `cd` の行でフォルダ `prolog` に移り、`mosa` を起動します。0 で割ってみます。Mosa は値を計算できないので、問い合わせを止め、答えの代わりに警告を出します。

<!-- check: zero -->
```text
?- X is 1 / 0.
警告: ゴールは例外を送出しました: error(evaluation_error(zero_divisor),context((is)/2,_G1))
```

警告は左から順に読みます。

- 「ゴールは例外を送出しました」は、問い合わせの中のゴールが例外を投げたことを表します。例外は、ゴールが答えの代わりに返す項で、問い合わせを止めます。
- その後ろに、その項が続きます。組み込みの述語のエラーは `error(Formal, Context)` の形をしています。
- 1 つ目の引数 `Formal`（ここでは `evaluation_error(zero_divisor)`）は、何が起きたかを表します。この例では、0 で割ろうとしたために（zero divisor）、算術評価ができませんでした。
- 2 つ目の引数 `Context`（ここでは `context((is)/2,_G1)`）は、どこで起きたかを表します。ここでは `is/2` の中です。`is` は演算子なので、第 6 章の `(+)` と同じように、かっこで囲まれています。`context` の 2 つ目の引数には、ほかに知らせることがあればそれが入ります。ここでは知らせることがないので、値のない変数が入っています。Mosa はその変数を `_G1` と書きます。

2. エラーをあと 3 つ起こし、それぞれの 1 つ目の引数を読みます。`instantiation_error` は、値が要るところで、引数が値のない変数だったことを表します。第 6 章にも出てきました。`type_error(atom,123)` は、引数の型が違ったことを表します。`atom_length/2` には原子が要りますが、`123` は数です。`existence_error(procedure,こんにちは/0)` は、述語 `こんにちは/0` がないことを表します。第 3 章に出てきた警告です。procedure（手続き）は、述語の別の呼び方です。

<!-- check: three-errors -->
```text
?- atom_length(X, N).
警告: ゴールは例外を送出しました: error(instantiation_error,context(atom_length/2,_G1))
?- atom_length(123, N).
警告: ゴールは例外を送出しました: error(type_error(atom,123),context(atom_length/2,_G1))
?- こんにちは.
警告: ゴールは例外を送出しました: error(existence_error(procedure,こんにちは/0),context(こんにちは/0,_G1))
```

3. `catch/3` でエラーを捕まえます。`catch(Goal, Catcher, Recovery)` はゴール `Goal` を実行します。`Goal` が投げた例外が、捕まえる形 `Catcher` と単一化できると、Mosa は警告を出す代わりにゴール `Recovery` を実行し、問い合わせを続けます。ここでの `Catcher` は変数 `E` で、どんな例外とも単一化できるので、`E` にエラーの項が入ります。`Recovery` は何もしない `true` です。`X = X` は、第 8 章と同じように、`X` に値がないことを表します。`is/2` が `X` に値を入れる前に、エラーが起きたからです。

<!-- check: catch-all -->
```text
?- catch(X is 1 / 0, E, true).
X = X, E = error(evaluation_error(zero_divisor),context((is)/2,_G1)).
```

4. 1 種類のエラーだけを捕まえます。`Catcher` もふつうの項で、Mosa はそれを例外と単一化します（第 5 章）。下の `Catcher` は算術評価のエラーに合い、その理由を `Why` に入れます。`Recovery` は `X` に `なし` を入れます。

<!-- check: catch-one -->
```text
?- catch(X is 1 / 0, error(evaluation_error(Why), _), X = なし).
X = なし, Why = zero_divisor.
```

5. 例外を通します。例外が `Catcher` と単一化できないと、`catch/3` はそれを捕まえず、Mosa は `catch/3` がないときと同じように警告を出します。下の `Catcher` は型のエラーを待っていますが、ゴールが投げるのは `instantiation_error` です。

<!-- check: catch-miss -->
```text
?- catch(atom_length(X, N), error(type_error(_, _), _), true).
警告: ゴールは例外を送出しました: error(instantiation_error,context(atom_length/2,_G1))
```

6. `throw/1` で、自分で例外を投げます。`throw(B)` は、項 `B` を例外として投げます。値のない変数を除けば、項は何でもかまいません。`throw(_)` は、代わりに `instantiation_error` を投げます。自分で投げる項に、`error(…)` の形は要りません。どこでも捕まえられなければ、警告にはその項がそのまま出ます。

<!-- check: throw -->
```text
?- throw(しまった).
警告: ゴールは例外を送出しました: しまった
?- catch(throw(しまった), B, true).
B = しまった.
?- throw(_).
警告: ゴールは例外を送出しました: error(instantiation_error,context(throw/1,_G1))
```

7. ファイルの中で `catch/3` と `throw/1` を使います。フォルダ `prolog` に `安全.pl` というファイルを作り、次の 2 つの規則を書きます。`割る(X, Y, Z)` は `X` を `Y` で割り、`Y` が 0 のときは、エラーの代わりに `Z` を `なし` にします。`年齢を確かめる(A)` は、`A` が 0 以上なら成功し、そうでなければ `年齢がおかしい(A)` を投げます。

<!-- file: safe-file 安全.pl -->
```prolog
割る(X, Y, Z) :- catch(Z is X / Y, error(evaluation_error(zero_divisor), _), Z = なし).

年齢を確かめる(A) :- ( A >= 0 -> true ; throw(年齢がおかしい(A)) ).
```

8. ファイルを読み込み、2 つの規則を試します。最後の問い合わせは、`年齢を確かめる/1` が投げた例外を捕まえ、その中から年齢を取り出します。

<!-- check: safe -->
```text
?- consult('安全.pl').
true.
?- 割る(7, 2, Z).
Z = 3.5.
?- 割る(7, 0, Z).
Z = なし.
?- 年齢を確かめる(20).
true.
?- 年齢を確かめる(-3).
警告: ゴールは例外を送出しました: 年齢がおかしい(-3)
?- catch(年齢を確かめる(-3), 年齢がおかしい(A), true).
A = -3.
```

9. `halt.` と打って終わります。

## 何が起きたか

例外は失敗とは違います。ゴールが失敗すると、Mosa はバックトラックして、残っている選択肢を試します。ゴールが例外を投げると、Mosa は答えを探すのをやめ、まだ実行中の `catch/3` のうち、`Catcher` がその例外と単一化できるいちばん新しいものまで戻ります。その `catch/3` が始まってから変数に入れた値はすべて取り消され、それから `Recovery` が実行されます。下の例では `X = 1` が取り消されるので、最後に `X` には値がありません。

<!-- check: undone -->
```text
?- catch((X = 1, throw(しまった)), しまった, true).
X = X.
```

どの `catch/3` も例外を捕まえなければ、問い合わせは警告を出して終わり、Mosa はまた `?-` を出します。第 6 章の「構文エラー:」の行は、これとは別のものです。そこでは Mosa が問い合わせをまったく読めなかったので、ゴールは 1 つも実行されていません。

組み込みの述語のエラーは、Prolog の ISO 規格で決まっています。ここまでに出てきたものを挙げます。

| `Formal` | 意味 |
| --- | --- |
| `instantiation_error` | 値が要るところで、引数が値のない変数だった。 |
| `type_error(Type, Culprit)` | `Culprit` が、型 `Type`（`atom`、`integer`、`evaluable` など）でなかった。 |
| `existence_error(procedure, Name/Arity)` | 名前が `Name` でアリティが `Arity` の述語がない。 |
| `evaluation_error(zero_divisor)` | 算術で 0 で割った。 |
| `evaluation_error(int_overflow)` | 整数の結果が大きすぎた（第 6 章）。 |

エラーを捕まえるときは、`Catcher` に `Formal` の形を書き、`Context` は `_` にします。`割る/3` もそう書いています。何が起きたかは `Formal` が表し、`Context` はどこで起きたかを探す手がかりにすぎません。

## 練習問題

1. `X is りんご + 1` はどのエラーを起こしますか。それは何を表していますか。
2. `年齢を確かめる(-3)` を実行し、警告の代わりに `R = 却下` と答える問い合わせを書いてください。

### 答え

1. 型のエラーです。`りんご` は原子です。算術式の中では、Mosa はそれを、名前が `りんご` で引数のない関数として読みます。そういう関数はないので、`りんご/0` は評価できるもの（evaluable）ではありません。

<!-- check: exercise-evaluable -->
```text
?- X is りんご + 1.
警告: ゴールは例外を送出しました: error(type_error(evaluable,りんご/0),context((is)/2,_G1))
```

2. `年齢がおかしい(_)` を捕まえ、`Recovery` で `R` に値を入れます。

<!-- check: exercise-rejected -->
```text
?- consult('安全.pl').
true.
?- catch(年齢を確かめる(-3), 年齢がおかしい(_), R = 却下).
R = 却下.
```

---

[目次](README.md) ・ 前: [8. 制御](08-control.md) ・ 次: [10. 入出力](10-input-output.md)
