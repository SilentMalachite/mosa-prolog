English | [日本語](../ja/04-facts-rules.md)

# 4. Facts and rules

In this chapter you write facts and a rule about a family in a file, load the file into Mosa, and ask about it.

## Steps

1. Make a folder for this chapter and move your shell into it. Open Terminal, a terminal or PowerShell as in
   chapter 2. On macOS and Linux, type:

<!-- skip: folder-unix -->
```console
$ mkdir ~/prolog
$ cd ~/prolog
```

On Windows, in PowerShell:

<!-- skip: folder-windows -->
```powershell
mkdir $HOME\prolog
cd $HOME\prolog
```

`mkdir` makes the folder `prolog` in your home folder, and `cd` moves your shell into it. When you open a new window
later, type only the `cd` line to come back.

2. With a text editor, create a file named `family.pl` in the folder `prolog`, write these lines in it and save it.
   Save it as plain text, because Mosa reads only plain text:
   - On macOS, in TextEdit, choose Format, then Make Plain Text, before you save. Keep the name `family.pl`; if
     TextEdit asks which extension to use, choose `.pl`.
   - On Windows, in Notepad, set "Save as type" to "All files" in the Save As dialog, so that the file does not
     become `family.pl.txt`.
   - On Linux, any plain-text editor works, such as Text Editor on Ubuntu.

   Each line of the file is a fact: it says that the first person is a parent of the second.

<!-- file: family-file family.pl -->
```prolog
parent(tom, bob).
parent(tom, liz).
parent(bob, ann).
parent(bob, pat).
parent(pat, jim).
```

3. Start Mosa with the file name. Mosa loads the file first and then shows `?- `. Ask whether Tom is a parent of
   Liz, and whether Liz is a parent of Tom, then quit.

<!-- check: start-with-file -->
```console
$ mosa family.pl
?- parent(tom, liz).
true.
?- parent(liz, tom).
false.
?- halt.
```

If Mosa prints an error instead and you are back in your shell, the file is not what Mosa expects. The error names
the file, then the line and the column where Mosa noticed the problem:

- `family.pl:1:7: unexpected token`, at line 1, column 7, means that the file was saved as rich text. TextEdit and
  WordPad start such a file with `{\rtf1\ansi`, and Mosa stops at its seventh character. Open the file and save it
  again as plain text (step 2).
- Any other line and column point at a typing mistake. Compare that line, and the line before it, with the lines
  above. Each fact ends with a period, and when a period is missing, Mosa notices only at the start of the next
  line: without the period after `parent(tom, bob)`, Mosa prints `family.pl:2:1: unexpected token`. Without the
  last period it prints `family.pl:6:1: incomplete term`, or `family.pl:5:17: incomplete term` when the file does
  not end with a line break (TextEdit and Notepad do not add one). Type parentheses, commas and periods as the
  ordinary half-width characters. An input method for Japanese, Chinese or Korean may type full-width ones such as
  `（` or `。`, which Mosa does not read as punctuation.
- `family.pl: cannot open file` means that this folder holds no file with exactly that name. List the folder to
  see the real names: type `ls` on macOS and Linux, or `dir` in PowerShell. File Explorer on Windows hides
  extensions such as `.txt`, so a file that it shows as `family.pl` may really be `family.pl.txt`.

4. Start `mosa` without a file name this time, and load the file with `consult/1`. The file name is in single
   quotes because it holds a period. Then ask who Tom's children are, and who is a parent of Jim. Press `;` after
   the first answer, as in chapter 3. The examples from here on start with `consult('family.pl')`, so that each of
   them also works after you restart Mosa. Loading the file again when it is already loaded does no harm.

<!-- check: consult
consult('family.pl').
parent(tom, X).
;
parent(X, jim).
-->
```text
?- consult('family.pl').
true.
?- parent(tom, X).
X = bob;

X = liz.
?- parent(X, jim).
X = pat.
```

5. Ask whether Tom is a parent of Bob. Mosa prints `true` without a period and waits, because it has not yet looked
   at every fact about Tom. Press Enter to stop. Then ask again and press `;` instead: Mosa looks at the rest and
   prints `false.`, because there is no other way.

<!-- check: not-sure
consult('family.pl').
parent(tom, bob).

parent(tom, bob).
;
-->
```text
?- consult('family.pl').
true.
?- parent(tom, bob).
true
.
?- parent(tom, bob).
true;

false.
```

6. Add a rule. Open `family.pl` again, add the last line below, and save it. The rule says: X is a grandparent of
   Z if X is a parent of some Y and that Y is a parent of Z. Read `:-` as "if".

<!-- file: family-rule family.pl -->
```prolog
parent(tom, bob).
parent(tom, liz).
parent(bob, ann).
parent(bob, pat).
parent(pat, jim).

grandparent(X, Z) :- parent(X, Y), parent(Y, Z).
```

7. Load the file again with `consult/1`, so that Mosa reads the new rule. Loading a file again replaces what Mosa
   read from it before. Ask who Tom's grandchildren are, pressing `;` until the period, and who is a grandparent of
   Jim.

<!-- check: grandparent
consult('family.pl').
grandparent(tom, X).
;
;
grandparent(X, jim).
;
-->
```text
?- consult('family.pl').
true.
?- grandparent(tom, X).
X = ann;

X = pat;

false.
?- grandparent(X, jim).
X = bob;

false.
```

8. Type `halt.` to quit.

## What happened

Your file holds clauses. A clause is either a fact, such as `parent(tom, bob).`, which is simply true, or a rule,
such as `grandparent(X, Z) :- parent(X, Y), parent(Y, Z).`, which is true when every goal after `:-` is true. All
the clauses with the same name and the same number of arguments make one predicate: `parent/2` has five clauses
and `grandparent/2` has one. A variable such as `X` stands for the same value only inside its own clause or query,
so the `X` of your query `grandparent(tom, X)` and the `X` of the rule are two different variables.

`mosa family.pl` and `consult('family.pl')` both load the file. Mosa looks for the file in the folder where you
started it, and the name must be complete: without the `.pl`, Mosa does not find the file.

<!-- check: no-extension -->
```text
?- consult(family).
family: cannot open file
false.
```

When the file itself has a mistake, Mosa names the line and the column where it noticed it, as in the list after
step 3. You can try one on purpose without touching `family.pl`: save the facts of step 2 once more as `typo.pl`,
with the period after the first fact left out. Mosa reads `parent(tom, bob)` and goes on to the next line, where
`parent` cannot follow without a period in between. So the message names line 2, column 1, and `mosa` returns to
your shell:

<!-- file: typo-file typo.pl -->
```prolog
parent(tom, bob)
parent(tom, liz).
parent(bob, ann).
parent(bob, pat).
parent(pat, jim).
```

<!-- check: typo -->
```console
$ mosa typo.pl
typo.pl:2:1: unexpected token
```

To answer a query, Mosa tries the clauses of the predicate from top to bottom. When a clause fits, Mosa shows the
answer, and remembers that the clauses below it are still to be tried. When you press `;`, Mosa goes back to the
last place where it had a choice and tries the next clause. This going back is called backtracking.

Here is how Mosa found the answers of `grandparent(tom, X)` in step 7:

1. In the rule, `X` becomes `tom`, and `Z` stands for your `X`. The first goal of the rule, `parent(tom, Y)`, fits
   the first fact, so `Y = bob`.
2. The second goal is now `parent(bob, Z)`. It fits the fact `parent(bob, ann)`, so `Z = ann`, and Mosa shows
   the first answer, `X = ann`.
3. On `;`, Mosa backtracks to `parent(bob, Z)` and finds `Z = pat`: the second answer, `X = pat`.
4. On `;` again, no other fact has `bob` as its first argument, so Mosa backtracks further, to `parent(tom, Y)`,
   and finds `Y = liz`. No fact has `liz` as its first argument, so `parent(liz, Z)` fails. Nothing is left to try:
   `false.`.

`true` or an answer without a period means that Mosa still has clauses left to try. It does not mean there is
another answer: in step 5 there was none, so `;` led to `false.`.

## Exercises

1. Who is a parent of Pat? Load the file and ask, and press `;` once.
2. Add a rule `child/2` so that `child(X, Y)` is true when X is a child of Y, and ask for Tom's children with it.

### Answers

1. Bob. After `;`, Mosa finds no other parent:

<!-- check: exercise-parents
consult('family.pl').
parent(X, pat).
;
-->
```text
?- consult('family.pl').
true.
?- parent(X, pat).
X = bob;

false.
```

2. X is a child of Y when Y is a parent of X. Add the rule to `family.pl`:

<!-- file: family-child family.pl -->
```prolog
parent(tom, bob).
parent(tom, liz).
parent(bob, ann).
parent(bob, pat).
parent(pat, jim).

grandparent(X, Z) :- parent(X, Y), parent(Y, Z).

child(X, Y) :- parent(Y, X).
```

Then load it again and ask:

<!-- check: exercise-child
consult('family.pl').
child(X, tom).
;
-->
```text
?- consult('family.pl').
true.
?- child(X, tom).
X = bob;

X = liz.
```

---

[Index](README.md) · Previous: [3. Your first query](03-first-query.md) · Next: 5. Terms and unification (not
written yet)
