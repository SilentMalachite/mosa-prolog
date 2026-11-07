English | [日本語](../ja/06-arithmetic.md)

# 6. Arithmetic

In this chapter you compute with numbers using `is/2`, compare numbers, and use arithmetic in a rule.

## Steps

1. Move into the folder `prolog` with the `cd` line of chapter 4, and start `mosa`. Unify `X` with a sum. `=` does
   not compute anything: `1 + 2` is a compound term with the name `+` and the arguments `1` and `2`, so its functor
   is `+/2`. A name such as `+`, written between its two arguments, is called an operator. When Mosa writes an
   operator alone, it puts it in parentheses, as in `(+)`.

<!-- check: not-computed -->
```text
?- X = 1 + 2.
X = 1+2.
?- X = 1 + 2, functor(X, Name, Arity).
X = 1+2, Name = (+), Arity = 2.
```

2. Compute with `is/2`. `X is 1 + 2` evaluates the right side, that is, computes its value, and then unifies the
   left side with that value. This is called arithmetic evaluation. `*` multiplies, and `-` subtracts from the left:
   `10 - 4 - 3` is `(10 - 4) - 3`.

<!-- check: is -->
```text
?- X is 1 + 2.
X = 3.
?- X is 7 * 6.
X = 42.
?- X is 10 - 4 - 3.
X = 3.
```

3. Divide. `/` always gives a float, even when the division comes out even. `//` divides integers and drops the
   fraction, and `mod` gives the remainder.

<!-- check: divide -->
```text
?- X is 7 / 2.
X = 3.5.
?- X is 8 / 2.
X = 4.0.
?- X is 7 // 2.
X = 3.
?- X is 7 mod 2.
X = 1.
```

4. Mind the order. `*`, `/`, `//` and `mod` are computed before `+` and `-`, as in school arithmetic. Use parentheses
   to change the order.

<!-- check: order -->
```text
?- X is 1 + 2 * 3.
X = 7.
?- X is (1 + 2) * 3.
X = 9.
```

5. Use arithmetic functions. Inside the right side of `is/2`, `max(A, B)` is the larger of two numbers, `abs(A)` is
   the value without its sign, and `sqrt(A)` is the square root, always as a float.

<!-- check: functions -->
```text
?- X is max(3, 8).
X = 8.
?- X is abs(-5).
X = 5.
?- X is sqrt(2).
X = 1.4142135623730951.
```

6. Use a result in the next goal. Once `X` has the value `3`, `X * 10` can be evaluated.

<!-- check: chain -->
```text
?- X is 1 + 2, Y is X * 10.
X = 3, Y = 30.
```

7. Compare numbers. `<` (less than), `>` (greater than), `=<` (less than or equal), `>=` (greater than or equal),
   `=:=` (equal) and `=\=` (not equal) evaluate both of their sides and compare the values. Compare this with `=`,
   which does not evaluate: `1 + 2` and `3` are different terms, but they have the same value. And `=:=` compares
   values, so the integer `2` and the float `2.0` are equal.

<!-- check: compare -->
```text
?- 3 < 5.
true.
?- 5 < 3.
false.
?- 3 =< 3.
true.
?- 1 + 2 =:= 3.
true.
?- 1 + 2 = 3.
false.
?- 2 =:= 2.0.
true.
```

8. Make two common mistakes on purpose. "Less than or equal" is `=<`. If you type `<=`, Mosa cannot read the query
   and names the column where it stopped. And the right side of `is/2` must have a value when Mosa evaluates it:
   goals run from left to right, so in the second query below `X` has no value yet when `Y is X * 2` runs. Mosa
   then prints a warning with `instantiation_error`, which means that a variable had no value where one was needed.
   Chapter 9 shows how to read such a warning. Put `X = 3` first, and the query works.

<!-- check: mistakes -->
```text
?- 3 <= 5.
Syntax error: line 1 column 3: unexpected token <=
?- Y is X * 2, X = 3.
Warning: goal raised exception: error(instantiation_error,context((is)/2,_G1))
?- X = 3, Y is X * 2.
X = 3, Y = 6.
```

9. Use arithmetic in a rule. Create a file `shop.pl` in the folder `prolog` with these lines. The facts give the
   price of one item, and the rule says: the cost of `Count` pieces of `Item` is `Total` if the price of `Item` is
   `Price` and `Total` is `Price * Count`.

<!-- file: shop-file shop.pl -->
```prolog
price(apple, 120).
price(bread, 250).
price(milk, 180).

cost(Item, Count, Total) :- price(Item, Price), Total is Price * Count.
```

10. Load the file and ask what three apples cost. Then ask which items cost less than 400 for two pieces, and press
    `;` after the first answer.

<!-- check: shop
consult('shop.pl').
cost(apple, 3, T).
cost(Item, 2, T), T < 400.
;
-->
```text
?- consult('shop.pl').
true.
?- cost(apple, 3, T).
T = 360.
?- cost(Item, 2, T), T < 400.
Item = apple, T = 240;

Item = milk, T = 360.
```

11. Type `halt.` to quit.

## What happened

`is/2` and the comparisons evaluate arithmetic expressions. An expression is a number, or a compound term built from
numbers with operators such as `+`, `-`, `*`, `/`, `//` and `mod`, and functions such as `max`, `abs` and `sqrt`. A
variable can stand in for a number, if it has a value by the time the expression is evaluated. Anywhere else,
`1 + 2` is just a compound term, as in step 1.

`is/2` evaluates only its right side, and then unifies. So `3 is 1 + 2` is true, but `1 + 2 is 3` is false: the
value `3` does not unify with the term `1 + 2`.

<!-- check: is-one-side -->
```text
?- 3 is 1 + 2.
true.
?- 1 + 2 is 3.
false.
```

In step 10, Mosa tried `price(Item, Price)` with each fact in turn, as in chapter 4. For `apple` the cost was `240`,
which is less than `400`, so that was the first answer. For `bread` it was `500`, so `T < 400` failed and Mosa
backtracked to the next fact. For `milk` it was `360`. `milk` is the last fact, so Mosa knew that there was nothing
more to try and put a period after the answer.

Numbers have limits. An integer is between -9223372036854775808 and 9223372036854775807, and a result outside that
range gives a warning with `int_overflow`. A float is stored in binary with about 16 significant digits, so a
decimal fraction such as `0.1` is not stored exactly, and a sum can show a tiny difference:

<!-- check: limits -->
```text
?- X is 9223372036854775807 + 1.
Warning: goal raised exception: error(evaluation_error(int_overflow),context((is)/2,_G1))
?- X is 0.1 + 0.2.
X = 0.30000000000000004.
```

## Exercises

1. How many seconds are there in a day? Ask Mosa to compute 24 times 60 times 60.
2. Add a rule `cheap/1` to `shop.pl`, so that `cheap(Item)` is true when the price of `Item` is less than 200, and
   ask for every cheap item.

### Answers

1. 86400:

<!-- check: exercise-seconds -->
```text
?- X is 24 * 60 * 60.
X = 86400.
```

2. Look up the price, then compare it. Add this rule at the end of `shop.pl`:

<!-- file: shop-cheap shop.pl -->
```prolog
price(apple, 120).
price(bread, 250).
price(milk, 180).

cost(Item, Count, Total) :- price(Item, Price), Total is Price * Count.

cheap(Item) :- price(Item, Price), Price < 200.
```

Then load the file again and ask, pressing `;` once:

<!-- check: exercise-cheap
consult('shop.pl').
cheap(Item).
;
-->
```text
?- consult('shop.pl').
true.
?- cheap(Item).
Item = apple;

Item = milk.
```

---

[Index](README.md) · Previous: [5. Terms and unification](05-terms-unification.md) · Next: 7. Lists and recursion
(not written yet)
