English | [日本語](../ja/07-lists-recursion.md)

# 7. Lists and recursion

In this chapter you write lists, take them apart into a head and a tail, and write predicates that go through a list
by calling themselves.

## Steps

1. Move into the folder `prolog` with the `cd` line of chapter 4, and start `mosa`. Write a list. A list is a
   sequence of terms in square brackets, separated by commas. `[]` is the empty list, which has no elements. As with
   compound terms, Mosa writes a list without a space after each comma.

<!-- check: list -->
```text
?- X = [a, b, c].
X = [a,b,c].
?- X = [].
X = [].
```

2. Take a list apart. In `[H|T]`, the bar `|` separates the first element, called the head, from the list of the
   remaining elements, called the tail. The tail is always a list: the tail of a list with one element is `[]`. The
   empty list has no head, so it does not unify with `[H|T]`.

<!-- check: head-tail -->
```text
?- [H|T] = [a, b, c].
H = a, T = [b,c].
?- [H|T] = [a].
H = a, T = [].
?- [H|T] = [].
false.
```

3. Take more than one element from the front. Before the bar you can write several elements, separated by commas.

<!-- check: two-heads -->
```text
?- [A, B|T] = [1, 2, 3, 4].
A = 1, B = 2, T = [3,4].
```

4. Put an element in front of a list. `[a|[b, c]]` is the list with the head `a` and the tail `[b, c]`, which is the
   same list as `[a, b, c]`.

<!-- check: cons -->
```text
?- L = [a|[b, c]].
L = [a,b,c].
```

5. Count the elements with the built-in predicate `length/2`.

<!-- check: length -->
```text
?- length([a, b, c], N).
N = 3.
?- length([], N).
N = 0.
```

6. Write your own counting predicate. Create a file `mylists.pl` in the folder `prolog` with these two clauses. The
   first says: the empty list has 0 elements. The second says: a list with some head and the tail `T` has `N`
   elements if `T` has `N0` elements and `N` is `N0 + 1`. The second clause uses `count/2` itself to count the
   tail. A predicate that uses itself like this is called recursive, and this way of solving a problem is called
   recursion.

<!-- file: count-file mylists.pl -->
```prolog
count([], 0).
count([_|T], N) :- count(T, N0), N is N0 + 1.
```

7. Load the file and count a few lists.

<!-- check: count -->
```text
?- consult('mylists.pl').
true.
?- count([a, b, c], N).
N = 3.
?- count([], N).
N = 0.
```

8. Join two lists. Add the predicate `app/3` below to `mylists.pl`. `app(A, B, C)` is true when `C` is the list `A`
   followed by the list `B`. The first clause says: the empty list followed by `L` is `L`. The second says: a list
   with the head `H` and the tail `T`, followed by `L`, is the list with the head `H` and the tail `R`, if `T`
   followed by `L` is `R`.

<!-- file: app-file mylists.pl -->
```prolog
count([], 0).
count([_|T], N) :- count(T, N0), N is N0 + 1.

app([], L, L).
app([H|T], L, [H|R]) :- app(T, L, R).
```

9. Load the file again, and join `[a, b]` and `[c, d]`.

<!-- check: app -->
```text
?- consult('mylists.pl').
true.
?- app([a, b], [c, d], X).
X = [a,b,c,d].
```

10. Run `app/3` the other way. Give only the joined list, and ask which two lists make it. Press `;` until Mosa
    prints `false.`: there are three ways to split `[a, b]`.

<!-- check: app-split
consult('mylists.pl').
app(X, Y, [a, b]).
;
;
;
-->
```text
?- consult('mylists.pl').
true.
?- app(X, Y, [a, b]).
X = [], Y = [a,b];

X = [a], Y = [b];

X = [a,b], Y = [];

false.
```

11. Type `halt.` to quit.

## What happened

A list is either the empty list `[]`, or a head and a tail, `[H|T]`, where the tail is a list again. `[a, b, c]` is
a short way to write `[a|[b|[c|[]]]]`. That is why one clause for `[]` and one clause for `[H|T]` together handle
every list.

Here is how Mosa found `count([a, b, c], N)` in step 7:

1. `[a, b, c]` does not unify with `[]`, so the first clause does not fit. The second does, with `T = [b, c]`, and
   its first goal is `count([b, c], N0)`.
2. That goal, in the same way, asks for `count([c], …)`, and that one for `count([], …)`. Each of these goals is a
   new use of the clause, with variables of its own, as with a query.
3. `count([], …)` fits the first clause and gives 0. Now each goal that was waiting can finish its `is/2`: `[c]`
   has 0 + 1 = 1 element, `[b, c]` has 1 + 1 = 2, and `[a, b, c]` has 2 + 1 = 3.

So every recursive predicate needs two kinds of clause: one for a case that it can answer at once (here `[]`), and
one that brings a bigger case down to a smaller one (here, a list to its tail). Each step makes the list shorter, so
the recursion always reaches `[]`.

`app/3` works in the same way, and because Mosa only unifies, it also works backwards: in step 10 Mosa looked for
every `X` and `Y` that, joined, give `[a, b]`.

Two mistakes are easy to make here. Save this file as `wrong.pl` to see what they give. `count2/2` uses `=` instead
of `is/2`, so it builds the term `0+1+1+1` instead of computing it (chapter 6). `count3/2` has no clause for `[]`, so
when the recursion reaches `[]`, no clause fits and the whole query fails:

<!-- file: wrong-file wrong.pl -->
```prolog
count2([], 0).
count2([_|T], N) :- count2(T, N0), N = N0 + 1.

count3([_|T], N) :- count3(T, N0), N is N0 + 1.
```

<!-- check: wrong -->
```text
?- consult('wrong.pl').
true.
?- count2([a, b, c], N).
N = 0+1+1+1.
?- count3([a, b, c], N).
false.
```

## Exercises

1. Add a predicate `sum/2` to `mylists.pl`, so that `sum(L, S)` is true when `S` is the sum of the numbers in the
   list `L`. Try it with `[1, 2, 3]`.
2. Use `app/3` to find the last element of `[a, b, c]`. Use this: when the last element is `X`, the list ends with
   the list `[X]`.

### Answers

1. The sum of `[]` is 0. The sum of a list with the head `X` and the tail `T` is `X` plus the sum of `T`:

<!-- file: sum-file mylists.pl -->
```prolog
count([], 0).
count([_|T], N) :- count(T, N0), N is N0 + 1.

app([], L, L).
app([H|T], L, [H|R]) :- app(T, L, R).

sum([], 0).
sum([X|T], S) :- sum(T, S0), S is S0 + X.
```

<!-- check: exercise-sum -->
```text
?- consult('mylists.pl').
true.
?- sum([1, 2, 3], S).
S = 6.
```

2. Ask for some list, joined with `[X]`, that gives `[a, b, c]`. The `_` stands for the front part, `[a, b]`, whose
   value you do not need. After `;`, Mosa finds no other way:

<!-- check: exercise-last
consult('mylists.pl').
app(_, [X], [a, b, c]).
;
-->
```text
?- consult('mylists.pl').
true.
?- app(_, [X], [a, b, c]).
X = c;

false.
```

---

[Index](README.md) · Previous: [6. Arithmetic](06-arithmetic.md) · Next: [8. Control](08-control.md)
