English | [日本語](../ja/09-errors.md)

# 9. Errors and exceptions

In this chapter you read the warning that Mosa prints when a goal raises an error, catch errors with `catch/3`, and
raise exceptions of your own with `throw/1`.

## Steps

1. Move into the folder `prolog` with the `cd` line of chapter 4, and start `mosa`. Divide by zero. Mosa cannot
   compute the value, so it stops the query and prints a warning instead of an answer.

<!-- check: zero -->
```text
?- X is 1 / 0.
Warning: goal raised exception: error(evaluation_error(zero_divisor),context((is)/2,_G1))
```

Read the warning from left to right:

- `goal raised exception` means that a goal of your query raised an exception. An exception is a term that a goal
  hands back instead of an answer, and it stops the query.
- After the colon comes that term. An error from a built-in predicate has the form `error(Formal, Context)`.
- `Formal`, here `evaluation_error(zero_divisor)`, says what went wrong: arithmetic evaluation failed, and the
  reason is division by zero.
- `Context`, here `context((is)/2,_G1)`, says where: in `is/2`. `is` is in parentheses because it is an operator,
  as `(+)` was in chapter 6. The second argument of `context` holds more information when there is some. Here there
  is none, so it is a variable without a value, which Mosa writes as `_G1`.

2. Make three more errors, and read their first arguments. `instantiation_error` means that an argument was a
   variable without a value, where a value was needed, as in chapter 6. `type_error(atom,123)` means that an
   argument had the wrong type: `atom_length/2` needs an atom, and `123` is a number.
   `existence_error(procedure,foo/0)` means that there is no predicate `foo/0`, as in chapter 3. Procedure is
   another word for predicate.

<!-- check: three-errors -->
```text
?- atom_length(X, N).
Warning: goal raised exception: error(instantiation_error,context(atom_length/2,_G1))
?- atom_length(123, N).
Warning: goal raised exception: error(type_error(atom,123),context(atom_length/2,_G1))
?- foo.
Warning: goal raised exception: error(existence_error(procedure,foo/0),context(foo/0,_G1))
```

3. Catch the error with `catch/3`. `catch(Goal, Catcher, Recovery)` runs `Goal`. If `Goal` raises an exception
   that unifies with `Catcher`, Mosa runs `Recovery` instead of printing a warning, and the query goes on. Here
   `Catcher` is the variable `E`, which unifies with any exception, so `E` gets the error term. `Recovery` is
   `true`, which does nothing. `X = X` says that `X` has no value, as in chapter 8: the error came before `is/2`
   could give it one.

<!-- check: catch-all -->
```text
?- catch(X is 1 / 0, E, true).
X = X, E = error(evaluation_error(zero_divisor),context((is)/2,_G1)).
```

4. Catch only one kind of error. `Catcher` is a term like any other, and Mosa unifies it with the exception, as in
   chapter 5. This catcher fits an evaluation error and gives its reason to `Why`. `Recovery` gives `X` the value
   `none`.

<!-- check: catch-one -->
```text
?- catch(X is 1 / 0, error(evaluation_error(Why), _), X = none).
X = none, Why = zero_divisor.
```

5. Let an exception pass. When the exception does not unify with `Catcher`, `catch/3` does not catch it, and Mosa
   prints the warning as if `catch/3` were not there. This catcher waits for a type error, but the goal raises an
   instantiation error.

<!-- check: catch-miss -->
```text
?- catch(atom_length(X, N), error(type_error(_, _), _), true).
Warning: goal raised exception: error(instantiation_error,context(atom_length/2,_G1))
```

6. Raise an exception yourself with `throw/1`. `throw(Ball)` raises the term `Ball` as an exception. Any term will
   do: a term that you throw yourself does not need the form `error(…)`. When nothing catches it, the warning shows
   it as it is.

<!-- check: throw -->
```text
?- throw(oops).
Warning: goal raised exception: oops
?- catch(throw(oops), Ball, true).
Ball = oops.
```

7. Use `catch/3` and `throw/1` in a file. Create a file `safe.pl` in the folder `prolog` with these two rules.
   `safe_div(X, Y, Z)` divides `X` by `Y`, and gives `Z` the value `none` instead of an error when `Y` is 0.
   `check_age(Age)` succeeds when `Age` is 0 or more, and otherwise throws `bad_age(Age)`.

<!-- file: safe-file safe.pl -->
```prolog
safe_div(X, Y, Z) :- catch(Z is X / Y, error(evaluation_error(zero_divisor), _), Z = none).

check_age(Age) :- ( Age >= 0 -> true ; throw(bad_age(Age)) ).
```

8. Load the file and try both rules. The last query catches the exception of `check_age/1` and takes the age out of
   it.

<!-- check: safe -->
```text
?- consult('safe.pl').
true.
?- safe_div(7, 2, Z).
Z = 3.5.
?- safe_div(7, 0, Z).
Z = none.
?- check_age(20).
true.
?- check_age(-3).
Warning: goal raised exception: bad_age(-3)
?- catch(check_age(-3), bad_age(A), true).
A = -3.
```

9. Type `halt.` to quit.

## What happened

An exception is not a failure. When a goal fails, Mosa backtracks and tries the choices that are left. When a goal
raises an exception, Mosa stops looking for answers and goes back to the newest `catch/3` that is still running and
whose `Catcher` unifies with the exception. Every value given to a variable since that `catch/3` started is undone,
and then `Recovery` runs. Here `X = 1` is undone, so `X` has no value at the end:

<!-- check: undone -->
```text
?- catch((X = 1, throw(oops)), oops, true).
X = X.
```

When no `catch/3` catches the exception, the query ends with the warning, and Mosa shows `?-` again. The
`Syntax error:` line of chapter 6 is something else: there Mosa could not read the query at all, so no goal ran.

The errors of the built-in predicates are defined by the ISO standard for Prolog. Here are the ones you have met so
far:

| `Formal` | Meaning |
| --- | --- |
| `instantiation_error` | An argument was a variable without a value, where a value was needed. |
| `type_error(Type, Culprit)` | `Culprit` is not of the type `Type`: `atom`, `integer`, `evaluable` and so on. |
| `existence_error(procedure, Name/Arity)` | There is no predicate `Name/Arity`. |
| `evaluation_error(zero_divisor)` | Arithmetic divided by zero. |
| `evaluation_error(int_overflow)` | An integer result was too large (chapter 6). |

To catch an error, write a catcher for its `Formal` and leave its `Context` to `_`, as `safe_div/3` does. The
`Formal` says what went wrong, and the `Context` only helps you find where.

## Exercises

1. Which error does `X is foo + 1` raise, and what does it mean?
2. Write a query that runs `check_age(-3)`, and gives `Result = rejected` instead of the warning.

### Answers

1. A type error. `foo` is an atom, and in an arithmetic expression Mosa reads it as a function with the name `foo`
   and no arguments. There is no such function, so `foo/0` is not evaluable:

<!-- check: exercise-evaluable -->
```text
?- X is foo + 1.
Warning: goal raised exception: error(type_error(evaluable,foo/0),context((is)/2,_G1))
```

2. Catch `bad_age(_)`, and give `Result` its value in `Recovery`:

<!-- check: exercise-rejected -->
```text
?- consult('safe.pl').
true.
?- catch(check_age(-3), bad_age(_), Result = rejected).
Result = rejected.
```

---

[Index](README.md) · Previous: [8. Control](08-control.md) · Next: [10. Input and output](10-input-output.md)
