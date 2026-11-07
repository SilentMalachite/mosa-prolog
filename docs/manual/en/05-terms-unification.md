English | [日本語](../ja/05-terms-unification.md)

# 5. Terms and unification

In this chapter you meet the kinds of data that Prolog works with, called terms, and learn how `=` makes two terms
equal.

## Steps

1. Open a terminal or PowerShell, move into the folder `prolog` with the `cd` line of chapter 4, and start `mosa`.
   Give `X` a number. A number is a term: `42` and `-7` are integers, and `3.14`, which has a decimal point, is a
   floating-point number, or float for short.

<!-- check: numbers -->
```text
?- X = 42.
X = 42.
?- X = -7.
X = -7.
?- X = 3.14.
X = 3.14.
```

2. Write an atom in single quotes. In chapter 3 an atom started with a lowercase letter. In single quotes it can
   also start with a capital letter and hold spaces or periods, as `'family.pl'` did in chapter 4. The quotes are
   not part of the name, so `'hello'` and `hello` are the same atom. Mosa shows the quotes only when the atom needs
   them.

<!-- check: quoted -->
```text
?- X = 'Hello world'.
X = 'Hello world'.
?- 'hello' = hello.
true.
```

3. Make a compound term. A compound term is a name followed by arguments in parentheses, written like the facts of
   chapter 4: `point(1, 2)` has the name `point` and the arguments `1` and `2`. Here it is only a piece of data, not
   a question, and Mosa puts it in `X`. Mosa writes it without a space after each comma.

<!-- check: compound -->
```text
?- X = point(1, 2).
X = point(1,2).
```

4. Take a compound term apart with two built-in predicates. `functor/3` gives its name and its number of arguments,
   and `arg/3` gives one argument, counted from 1. The number of arguments is called the arity. The name and the
   arity together, written `point/2`, are the functor of the term. You have seen this way of writing in
   `atom_length/2`.

<!-- check: functor-arg -->
```text
?- functor(point(1, 2), Name, Arity).
Name = point, Arity = 2.
?- arg(2, point(1, 2), X).
X = 2.
```

5. Make two compound terms equal. `=` looks for values of the variables that make its two sides the same term. This
   is called unification. Both sides of `point(1, Y) = point(X, 2)` have the functor `point/2`, so Mosa unifies the
   arguments pair by pair: `1` with `X`, then `Y` with `2`. Mosa lists the variables in the order in which they
   first appear in the query, so `Y` comes first.

<!-- check: unify -->
```text
?- point(1, Y) = point(X, 2).
Y = 2, X = 1.
```

6. Try terms that do not unify. Two compound terms unify only when they have the same functor and every pair of
   arguments unifies, so a different name or a different arity gives `false.`. A variable that appears twice must
   get the same value in both places, and `X` cannot be both `1` and `2`. An integer and a float are different
   terms, even when they are equal as numbers.

<!-- check: no-unify -->
```text
?- point(1, 2) = line(1, 2).
false.
?- point(1, 2) = point(1, 2, 3).
false.
?- point(X, X) = point(1, 2).
false.
?- 1 = 1.0.
false.
```

7. Put a term inside a term. An argument can itself be a compound term, and Mosa unifies the inner terms in the same
   way.

<!-- check: nested -->
```text
?- date(2026, month(11), D) = date(Y, M, 8).
D = 8, Y = 2026, M = month(11).
```

8. Unify two variables. After `X = Y`, the two variables stand for the same value, even though that value is not
   known yet. Mosa answers `X = Y.`. When one of them gets a value, the other has it too.

<!-- check: two-variables -->
```text
?- X = Y.
X = Y.
?- X = Y, Y = hello.
X = hello, Y = hello.
```

9. Look at a value that holds another variable of your query. Mosa writes that variable by its name and shows its
   value on its own, so read the answer below as "`X` is `f(2)`".

<!-- check: named-inside -->
```text
?- X = f(Y), Y = 2.
X = f(Y), Y = 2.
```

10. Use the anonymous variable `_`. It means "some term here, whose value I do not need". Each `_` is a new
    variable, and Mosa does not show it in the answer. When a value holds a variable that has no name, Mosa makes
    up a name for it that starts with `_G`, such as `_G1`. That is also the `_G1` in the warning of chapter 3.

<!-- check: anonymous -->
```text
?- point(X, _) = point(1, 2).
X = 1.
?- X = point(1, _).
X = point(1,_G1).
```

11. Use compound terms in facts. Create a file `lives.pl` in the folder `prolog`, as you created `family.pl` in
    chapter 4. Each fact says where a person lives, as an address with a city and a country:

<!-- file: lives-file lives.pl -->
```prolog
lives(tom, address(tokyo, japan)).
lives(ann, address(paris, france)).
lives(bob, address(osaka, japan)).
```

12. Load the file, and ask who lives in Japan, whatever the city. Press `;` after the first answer. Then ask in which
    city Ann lives.

<!-- check: lives
consult('lives.pl').
lives(Who, address(_, japan)).
;
lives(ann, address(City, _)).
-->
```text
?- consult('lives.pl').
true.
?- lives(Who, address(_, japan)).
Who = tom;

Who = bob.
?- lives(ann, address(City, _)).
City = paris.
```

13. Type `halt.` to quit.

## What happened

Everything that a Prolog program works with is a term. There are four kinds:

- A number: an integer such as `42` or `-7`, or a float such as `3.14`.
- An atom: a name that starts with a lowercase letter, such as `hello`, or any text in single quotes, such as
  `'Hello world'`.
- A variable: a name that starts with a capital letter or with `_`, such as `X`, `Who` and `_`.
- A compound term: a name with arguments in parentheses, such as `point(1, 2)`. Each argument is a term again. The
  functor of `point(1, 2)` is `point/2`: its name and its arity.

`=` unifies its two sides by these rules:

- A variable that has no value yet unifies with any term, and takes that term as its value.
- An atom unifies only with the same atom, and a number only with the same number, so `1` and `1.0` do not unify.
- Two compound terms unify when they have the same functor and their arguments unify, pair by pair from left to
  right.

When one pair cannot be unified, the whole unification fails, and Mosa answers `false.`.

Mosa uses unification to answer every query, not only those with `=`. The facts of chapter 4 are compound terms,
and so is the query `parent(tom, X)`. Mosa unifies the query with each fact in turn: with `parent(tom, bob)` it
gives `X = bob`. In step 12, `lives(Who, address(_, japan))` unified with the first and the third fact, and not with
the second, because `france` and `japan` are different atoms.

In an answer, Mosa shows each variable of your query that has a value, in the order in which the variables first
appear in the query. A variable of your query inside a value is shown by its name (`X = f(Y)`), and a variable
without a name as `_G1`, `_G2` and so on.

## Exercises

1. What does Mosa answer to `f(X, b) = f(a, Y)`? And to `f(X, X) = f(a, a)`?
2. With `lives.pl`, ask who lives in Paris.

### Answers

1. In the first, `X` unifies with `a` and `b` with `Y`. In the second, the first pair gives `X = a`, and then the
   second pair is `a = a`, which holds:

<!-- check: exercise-unify -->
```text
?- f(X, b) = f(a, Y).
X = a, Y = b.
?- f(X, X) = f(a, a).
X = a.
```

2. Put `paris` in the city and leave the country to `_`:

<!-- check: exercise-paris -->
```text
?- consult('lives.pl').
true.
?- lives(Who, address(paris, _)).
Who = ann.
```

---

[Index](README.md) · Previous: [4. Facts and rules](04-facts-rules.md) · Next: [6. Arithmetic](06-arithmetic.md)
