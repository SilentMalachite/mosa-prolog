English | [日本語](../ja/01-introduction.md)

# 1. Introduction

In this chapter you learn what Prolog and Mosa Prolog are, and how to read the examples in this manual.

## What Prolog is

Most programming languages ask you to write the steps a computer should follow. Prolog asks you to write what is
true instead. You state facts ("Tom is a parent of Bob") and rules ("a parent of a parent is a grandparent"), and
then you ask questions. Prolog searches through your facts and rules and answers them. A question to Prolog is
called a query.

Prolog is used to search, to check rules, to parse text and to solve puzzles. It is also a good way to learn to
think about a problem as a set of facts and rules.

## What Mosa Prolog is

Mosa Prolog is a Prolog system that follows the international standard for Prolog, ISO/IEC 13211-1. It comes with
three commands:

- `mosa` runs Prolog. You type queries and it answers them. Chapters 2 to 12 use it.
- `mosac` turns a Prolog program into an executable file that runs without `mosa`. It needs a C++ compiler.
  Chapter 13 shows it.
- `mosatest` runs tests. It is also what checks every example in this manual.

Mosa reads and writes text as UTF-8, so you can write names in Japanese and other languages. Chapter 11 shows how.

## Steps: reading an example

1. Look at this example. It is a query and Mosa's answer.

<!-- check: first-look -->
```text
?- atom_length(hello, N).
N = 5.
```

2. The first line starts with `?- `. That is the prompt Mosa shows when it waits for you. You type the text after
   it, `atom_length(hello, N).`, and press Enter. This query asks how many characters the word `hello` has.
3. The line under it, `N = 5.`, is what Mosa prints: the word has 5 characters. You do not type it.
4. A line that starts with `$ ` works the same way, for your shell: you type the text after `$ `. Chapter 2 starts
   with such lines.

## What happened

You read a query and its answer. Every example in this manual was run with Mosa 0.2.0 before the release, and the
output under each query is what Mosa printed. The command `mosatest --manual` runs every example again and fails if
a single character differs, so when you type an example, you should see exactly the same output. If you see
something else, the difference is worth a closer look: it may be a typing mistake, or a
[bug to report](https://github.com/SilentMalachite/mosa-prolog/issues).

## Exercise

1. In the example above, which line do you type, and which line does Mosa print?

### Answer

1. You type `atom_length(hello, N).` (the text after `?- `). Mosa prints `N = 5.`.

---

[Index](README.md) · Next: [2. Getting and starting Mosa](02-install.md)
