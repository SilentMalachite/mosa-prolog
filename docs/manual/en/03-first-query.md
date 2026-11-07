English | [日本語](../ja/03-first-query.md)

# 3. Your first query

In this chapter you ask Mosa questions at the `?-` prompt and learn to read its answers: `true.`, `false.`, values
of variables, and more answers with `;`.

## Steps

1. Start Mosa as in chapter 2: type `mosa` in your shell. Mosa shows `?- ` and waits.
2. Ask whether two words are the same. `=` asks Mosa to make its two sides equal. A word that starts with a
   lowercase letter, such as `hello`, is an atom: a name that stands for itself.

<!-- check: same -->
```text
?- hello = hello.
true.
?- hello = world.
false.
```

3. Ask with a variable. A name that starts with an uppercase letter, such as `X`, is a variable: a place for a
   value that Mosa looks for. Mosa answers with the value that makes the query true.

<!-- check: variable -->
```text
?- X = hello.
X = hello.
```

4. Join two goals with a comma. A goal is one question inside a query. A comma means "and": both goals must be
   true. In the second query `X` cannot be
   `hello` and `world` at once, so the answer is `false.`.

<!-- check: and -->
```text
?- X = hello, Y = world.
X = hello, Y = world.
?- X = hello, X = world.
false.
```

5. Join goals with a semicolon. A semicolon means "or": Mosa finds one answer and waits, without a period at the
   end. Type `;` and press Enter to ask for the next answer. After the last answer Mosa prints a period.

<!-- check: or-all
X = red ; X = green ; X = blue.
;
;
-->
```text
?- X = red ; X = green ; X = blue.
X = red;

X = green;

X = blue.
```

6. Ask the same query again, but this time press Enter alone after the second answer. Mosa prints a period and
   stops looking.

<!-- check: or-stop
X = red ; X = green ; X = blue.
;

-->
```text
?- X = red ; X = green ; X = blue.
X = red;

X = green
.
```

7. Use a built-in predicate. A predicate is a named question that Mosa can answer. `atom_length/2` comes with
   Mosa; the `/2` means it takes two arguments. `atom_length(hello, N)` is true when `N` is the number of characters
   in `hello`. You can also give both arguments and ask for `true.` or `false.`.

<!-- check: builtin -->
```text
?- atom_length(hello, N).
N = 5.
?- atom_length(hello, 5).
true.
?- atom_length(hello, 3).
false.
```

8. Write a query over two lines. Mosa does not answer until it reads the period, so you can press Enter in the
   middle of a query and go on typing on the next line.

<!-- check: two-lines
atom_length(hello,
N).
-->
```text
?- atom_length(hello,
N).
N = 5.
```

9. Ask about a predicate that does not exist. Mosa prints a warning. Chapter 9 explains how to read it; for now, it
   says that there is no predicate `hello` with no arguments (`hello/0`).

<!-- check: unknown -->
```text
?- hello.
Warning: goal raised exception: error(existence_error(procedure,hello/0),context(hello/0,_G1))
```

10. Type `halt.` to quit.

## What happened

Each line you typed after `?- ` was a query, and each part of a query joined by `,` or `;` is a goal. Mosa tried to
make the query true:

- `true.` means it is true, and there is nothing to report.
- `false.` means Mosa found no way to make it true.
- `X = hello.` means it is true when the variable `X` has the value `hello`. A variable keeps its value until the
  end of the query, which is why `X = hello, X = world` is false.

When a query can be true in more than one way, Mosa shows the first answer and waits, with no period after it.
`;` asks for the next answer, and after it Mosa prints an empty line. Enter alone stops, and Mosa prints the period
on a line of its own. A period right after an answer, as in `X = blue.`, means Mosa knows that there are no more
answers.

Names matter: `hello` and `world` are atoms because they start with a lowercase letter, and `X`, `Y` and `N` are
variables because they start with an uppercase letter.

## Exercises

1. How many characters does the word `prolog` have? Ask Mosa.
2. Ask for every answer of `X = cat ; X = dog`.

### Answers

1. Six:

<!-- check: exercise-length -->
```text
?- atom_length(prolog, N).
N = 6.
```

2. Type `;` once. After `dog` Mosa prints a period, because there are no more answers:

<!-- check: exercise-or
X = cat ; X = dog.
;
-->
```text
?- X = cat ; X = dog.
X = cat;

X = dog.
```

---

[Index](README.md) · Previous: [2. Getting and starting Mosa](02-install.md) · Next:
[4. Facts and rules](04-facts-rules.md)
