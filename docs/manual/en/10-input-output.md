English | [日本語](../ja/10-input-output.md)

# 10. Input and output

In this chapter you print terms with `write/1` and `nl/0`, read the terms that you type with `read/1`, and write a
file and read it back.

## Steps

1. Move into the folder `prolog` with the `cd` line of chapter 4, and start `mosa`. Print a word. `write/1` prints
   a term, and `nl/0` starts a new line. Then Mosa prints its answer, `true.`, as usual.

<!-- check: write -->
```text
?- write(hello), nl.
hello
true.
```

2. Print a quoted atom and the value of a variable. `write/1` prints an atom without its quotes. In the second
   query, `42` is what `write/1` printed, and `X = 42.` is Mosa's answer.

<!-- check: write-more -->
```text
?- write('Hello, world'), nl.
Hello, world
true.
?- X is 6 * 7, write(X), nl.
42
X = 42.
```

3. Leave out `nl`. Nothing starts a new line after `hello`, so Mosa's answer follows on the same line.

<!-- check: no-nl -->
```text
?- write(hello).
hellotrue.
```

4. Read a term that you type. `read(X)` waits for you to type a term, and unifies `X` with it. Mosa shows no prompt
   while it waits. End the term with a period, as you end a query, and press Enter. If you press Enter before the
   period, Mosa goes on waiting, as it does for a query: type the period and press Enter again, as in the second
   query below. Type the period as the ordinary half-width `.`. A full-width `。` or `．`, which an input method for
   Japanese or Chinese may type, does not end the term, so Mosa goes on waiting and shows nothing. Type `.` and press
   Enter: Mosa prints a warning with `syntax_error`, and you can type the query again.

<!-- check: read
read(X).
hello.
read(X).
hello
.
-->
```text
?- read(X).
hello.
X = hello.
?- read(X).
hello
.
X = hello.
```

5. Read a number and compute with it. Type `21.` when Mosa waits.

<!-- check: read-number
read(X), Y is X * 2.
21.
-->
```text
?- read(X), Y is X * 2.
21.
X = 21, Y = 42.
```

6. Write a program that asks a question. Create a file `greet.pl` in the folder `prolog` with this rule. `greet`
   prints a question, reads a name, and prints a greeting with that name.

<!-- file: greet-file greet.pl -->
```prolog
greet :- write('What is your name?'), nl, read(Name), write('Hello, '), write(Name), nl.
```

7. Load the file and run `greet`. When it asks, type `tom.` and press Enter. Then run it again and type `Tom.` with
   a capital T. A name that starts with a capital letter is a variable (chapter 5), so `read/1` gives `Name` a new
   variable, and `write/1` prints the made-up name `_G1`. To read a name with a capital letter, put it in quotes.

<!-- check: greet
consult('greet.pl').
greet.
tom.
greet.
Tom.
greet.
'Tom'.
-->
```text
?- consult('greet.pl').
true.
?- greet.
What is your name?
tom.
Hello, tom
true.
?- greet.
What is your name?
Tom.
Hello, _G1
true.
?- greet.
What is your name?
'Tom'.
Hello, Tom
true.
```

8. Write a file and read it back. Mosa reads and writes a file through a stream: a connection to a place where text
   comes from or goes to, such as a file or your terminal. `open(File, Mode, S)` opens the file `File` and gives `S`
   a stream for it. `Mode` is `write` to write the file, or `read` to read it. `write/2` and `nl/1` work like
   `write/1` and `nl/0`, and `read/2` like `read/1`, but they take the stream as their first argument and use it
   instead of the terminal. `close(S)` closes the stream when you are done. Create a file `notes.pl` in the folder
   `prolog` with these two rules. `save_note` opens the file `notes.txt` for writing, writes the term
   `hello(world)` and a period into it, ends the line, and closes the file. `load_note(T1, T2)` opens the same file
   for reading, reads two terms from it, and closes it.

<!-- file: notes-file notes.pl -->
```prolog
save_note :- open('notes.txt', write, S), write(S, hello(world)), write(S, '.'), nl(S), close(S).
load_note(T1, T2) :- open('notes.txt', read, S), read(S, T1), read(S, T2), close(S).
```

9. Load the file, save the note, and read it back. The first term is `hello(world)`. There is no second term, so
   `read/2` gives the atom `end_of_file`.

<!-- check: file -->
```text
?- consult('notes.pl').
true.
?- save_note.
true.
?- load_note(T1, T2).
T1 = hello(world), T2 = end_of_file.
```

The file `notes.txt` is in the folder where you started Mosa, `prolog`. Open it in your text editor, and you see one
line, `hello(world).`.

10. Try to read a file that does not exist. `open/3` raises an existence error, and `source_sink` means a file or
    another place to read from or to write to.

<!-- check: missing -->
```text
?- open('missing.txt', read, S).
Warning: goal raised exception: error(existence_error(source_sink,'missing.txt'),context(open/3,_G1))
```

11. Type `halt.` to quit.

## What happened

The terminal is a stream too. `write/1`, `nl/0` and `read/1` use it: they print in the terminal and read what you
type there. `write/2`, `nl/1` and `read/2` use the stream that you give them.

`Mode` can also be `append`, which adds to the end of the file. `write` empties the file first if it exists. Close
every stream that you open: until you do, some of the text that you wrote may not be in the file yet. The stream
stays inside the rules of `notes.pl`: `S` is a variable of each rule, so it does not appear in your answers. The name
of the file is an atom, so put it in quotes when it holds a period, as with `consult('family.pl')`.

`read/1` and `read/2` read one term, up to the period that ends it. When there is no more text, they give the atom
`end_of_file`.

`write/1` and `write/2` write an atom without its quotes, so a term that you write with them cannot always be read
back. `writeq/1` and `writeq/2` write the quotes too. To see the difference, save this file as `quoted.pl`.
`save_plain` writes the atom `'Hello world'` with `write/2`, and `save_quoted` with `writeq/2`. `load(T)` reads one
term inside `catch/3` (chapter 9). Its `Recovery` is the two goals in parentheses: when `read/2` raises an error,
`load/1` closes the stream and throws the same exception again, so the stream does not stay open.

<!-- file: quoted-file quoted.pl -->
```prolog
save_plain :- open('quoted.txt', write, S), write(S, 'Hello world'), write(S, '.'), nl(S), close(S).
save_quoted :- open('quoted.txt', write, S), writeq(S, 'Hello world'), write(S, '.'), nl(S), close(S).
load(T) :- open('quoted.txt', read, S), catch(read(S, T), E, (close(S), throw(E))), close(S).
```

After `save_plain`, the file holds `Hello world.`. `read/2` cannot read it: `Hello` is a variable, and the atom
`world` cannot follow it with only a space between them. So `load/1` raises a syntax error. The query catches it to
show its `Formal`, `syntax_error(unexpected_token)`, and passes `_` to `load/1`, because there is no term to see.
After `save_quoted`, the file holds `'Hello world'.`, and the atom comes back whole:

<!-- check: writeq -->
```text
?- consult('quoted.pl').
true.
?- save_plain.
true.
?- catch(load(_), error(E, _), true).
E = syntax_error(unexpected_token).
?- save_quoted.
true.
?- load(T).
T = 'Hello world'.
```

## Exercises

1. Read a number with `read/1` and print its square. Type `7.` when Mosa waits.
2. Write a file `print.pl` with a predicate `print_all/1` that prints each element of a list on a line of its own.
   Use recursion, as in chapter 7.

### Answers

1. `49` is what `write/1` printed, and the last line is Mosa's answer:

<!-- check: exercise-square
read(X), Y is X * X, write(Y), nl.
7.
-->
```text
?- read(X), Y is X * X, write(Y), nl.
7.
49
X = 7, Y = 49.
```

2. For `[]` there is nothing to print. For a list with the head `X` and the tail `T`, print `X`, start a new line,
   and print the elements of `T`:

<!-- file: print-file print.pl -->
```prolog
print_all([]).
print_all([X|T]) :- write(X), nl, print_all(T).
```

<!-- check: exercise-print -->
```text
?- consult('print.pl').
true.
?- print_all([a, b, c]).
a
b
c
true.
```

---

[Index](README.md) · Previous: [9. Errors and exceptions](09-errors.md) · Next: 11. Writing in Japanese (not
written yet)
