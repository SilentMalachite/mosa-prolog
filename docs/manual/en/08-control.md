English | [日本語](../ja/08-control.md)

# 8. Control

In this chapter you steer how Mosa searches: you negate a goal, choose between two goals with if-then-else, cut
away the choices that are left, and collect every answer in a list.

## Steps

1. Move into the folder `prolog` with the `cd` line of chapter 4, and start `mosa`. Try the two simplest goals.
   `true` always succeeds, and `fail` always fails.

<!-- check: true-fail -->
```text
?- true.
true.
?- fail.
false.
```

2. Create a file `family.pl` in the folder `prolog`, with the facts of chapter 4, a fact `person/1` for each person,
   and a rule. `\+` is read "not": `\+ parent(X, _)` is true when Mosa cannot find any child of `X`. So the rule says:
   `X` has no children if `X` is a person and Mosa cannot find a child of `X`.

<!-- file: family-file family.pl -->
```prolog
parent(tom, bob).
parent(tom, liz).
parent(bob, ann).
parent(bob, pat).
parent(pat, jim).

person(tom).
person(bob).
person(liz).
person(ann).
person(pat).
person(jim).

no_children(X) :- person(X), \+ parent(X, _).
```

3. Load the file, and ask whether Liz has no children, and whether Tom has none. Then ask who has no children,
   pressing `;` until the period.

<!-- check: not
consult('family.pl').
\+ parent(liz, _).
\+ parent(tom, _).
no_children(X).
;
;
-->
```text
?- consult('family.pl').
true.
?- \+ parent(liz, _).
true.
?- \+ parent(tom, _).
false.
?- no_children(X).
X = liz;

X = ann;

X = jim.
```

4. Put the goals in the other order, and see the query fail. `\+` never gives a variable a value. Here `X` has no
   value yet, so `parent(X, _)` holds for some `X`, for example `tom`, and `\+ parent(X, _)` fails at once. Put `\+`
   after the goals that give its variables their values, as the rule in step 2 does.

<!-- check: not-order -->
```text
?- consult('family.pl').
true.
?- \+ parent(X, _), person(X).
false.
```

5. Choose with if-then-else. `( Condition -> Then ; Else )` runs `Then` if `Condition` succeeds, and `Else`
   otherwise. The parentheses around the whole are needed.

<!-- check: if-then-else -->
```text
?- X = 5, ( X > 0 -> S = positive ; S = negative ).
X = 5, S = positive.
?- X = -5, ( X > 0 -> S = positive ; S = negative ).
X = -5, S = negative.
```

6. See a rule that gives too many answers. Create a file `grade.pl` with these lines: a score of 80 or more is an
   `a`, 60 or more is a `b`, and any other score is a `c`. The `_` in the last clause means that the score does not
   matter there. Load it and ask for the grade of 90, pressing `;` until the period. A score of 90 is also 60 or
   more, and the last clause fits any score, so Mosa finds three grades.

<!-- file: grade-file grade.pl -->
```prolog
grade(Score, a) :- Score >= 80.
grade(Score, b) :- Score >= 60.
grade(_, c).
```

<!-- check: grade-many
consult('grade.pl').
grade(90, G).
;
;
-->
```text
?- consult('grade.pl').
true.
?- grade(90, G).
G = a;

G = b;

G = c.
```

7. Fix it with cuts. Change `grade.pl` as below. `!`, read "cut", always succeeds, but it throws away the choices
   that are left: once Mosa has passed the cut in a clause of `grade/2`, it tries no other clause of `grade/2` for
   this goal. So the first condition that holds decides the grade. The grade is given after the cut, by `G = a`, and
   not in the head, so a query that gives the grade itself is checked too: `grade(90, c)` fails.

<!-- file: grade-cut-file grade.pl -->
```prolog
grade(Score, G) :- Score >= 80, !, G = a.
grade(Score, G) :- Score >= 60, !, G = b.
grade(_, c).
```

<!-- check: grade-cut -->
```text
?- consult('grade.pl').
true.
?- grade(90, G).
G = a.
?- grade(70, G).
G = b.
?- grade(30, G).
G = c.
?- grade(90, c).
false.
```

8. Collect every answer in a list. `findall(X, Goal, L)` finds every answer of `Goal` and gives `L` the list of the
   values that `X` had in them. In the answer, `X = X` means that `X` itself has no value: `findall/3` takes the
   values of `X` into the list and leaves `X` free. When `Goal` has no answer, the list is `[]`.

<!-- check: findall -->
```text
?- consult('family.pl').
true.
?- findall(X, parent(tom, X), L).
X = X, L = [bob,liz].
?- findall(X, parent(liz, X), L).
X = X, L = [].
```

9. Type `halt.` to quit.

## What happened

You used four ways to control the search:

- `\+ Goal` succeeds when `Goal` has no answer, and fails when it has one. It gives no values: Mosa looks for an
  answer of `Goal` and then forgets it. This is called negation as failure, because "not" here means "Mosa cannot
  prove it from the facts and rules that it has". Liz has no child in `family.pl`, so for Mosa she has none.
- `( Condition -> Then ; Else )` uses only the first answer of `Condition`, and never comes back to try the others.
- `!` succeeds once, and removes two kinds of choice: the clauses of the same predicate below the current one, and
  the other answers of the goals before the cut in that clause.
- `findall/3` runs a goal to the end, through every answer, and gives you the answers in a list.

In step 7, `grade(90, G)` tried the first clause: `90 >= 80` held, the cut removed the other two clauses, and
`G = a` gave the answer. With no choice left, Mosa put a period after it. `grade(70, G)` failed at `70 >= 80` before
reaching the cut of the first clause, so Mosa tried the second clause. Without the cuts, the conditions would have
had to exclude each other: `Score >= 60, Score < 80` for `b`, and so on.

Mosa writes `X = X` for a variable of your query that has no value at the end, unless the answer already shows that
variable in another way, as in `X = Y` or `X = f(Y)` of chapter 5. You will meet `X = X` again in chapter 9.

## Exercises

1. Write a file `max.pl` with a predicate `max_of/3`, so that `max_of(X, Y, M)` gives `M` the larger of `X` and `Y`.
   Use if-then-else.
2. With `family.pl`, find how many people have no children. Use `findall/3` and `length/2`.

### Answers

1. If `X >= Y`, the larger is `X`, else it is `Y`:

<!-- file: max-file max.pl -->
```prolog
max_of(X, Y, M) :- ( X >= Y -> M = X ; M = Y ).
```

<!-- check: exercise-max -->
```text
?- consult('max.pl').
true.
?- max_of(3, 7, M).
M = 7.
?- max_of(9, 2, M).
M = 9.
```

2. Three: collect them with `findall/3`, then count the list:

<!-- check: exercise-count -->
```text
?- consult('family.pl').
true.
?- findall(X, no_children(X), L), length(L, N).
X = X, L = [liz,ann,jim], N = 3.
```

---

[Index](README.md) · Previous: [7. Lists and recursion](07-lists-recursion.md) · Next:
[9. Errors and exceptions](09-errors.md)
