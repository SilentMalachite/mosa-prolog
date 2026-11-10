English | [日本語](../ja/14-reference.md)

# 14. Reference

This chapter lists what Mosa 0.1.1 has, so that you can look things up: the commands and their options, the
environment variables, the flags, the directives, every built-in predicate and the predicates of each library. You
do not need to read it in order.

## Commands

All three commands print the same version line:

<!-- check: versions -->
```console
$ mosa --version
mosa 0.1.1
$ mosac --version
mosa 0.1.1
$ mosatest --version
mosa 0.1.1
```

### `mosa`

`mosa [file...] [-g goal]... [-t halt] [--heap-max MiB] [-- argument...]` starts Mosa. With no arguments, it shows
the `?-` prompt (chapter 3).

| Argument | What it does |
| --- | --- |
| `file` | Loads the file before anything else, as `consult/1` does (chapter 4). Several files load in the order given. |
| `-g goal` | After the files, runs the goal once and prints its first answer as the prompt does. Several `-g` run in order. With `-t halt`, or when the input of `mosa` is not a terminal, the first one that fails or raises an error stops the rest; at a terminal without `-t halt`, every one runs. |
| `-t halt` | Quits after the `-g` goals instead of showing the prompt. |
| `--heap-max MiB` | The most memory that terms may use, in MiB (default 4096). A query that needs more stops with a message. This wins over `MOSA_HEAP_MAX`. |
| `--version` | Prints `mosa 0.1.1` and quits. |
| `-- argument...` | Everything after `--` is not read as an option: it becomes the flag `argv`, a list of atoms. |

Put a goal in single quotes, so that your shell passes it to `mosa` as one argument:

<!-- check: goal-option -->
```console
$ mosa -g 'X is 6 * 7' -t halt
X = 42.
$ mosa -g 'current_prolog_flag(argv, A)' -t halt -- one two
A = [one,two].
```

When `mosa` ends, its exit status tells your shell how it went (`echo $?` shows it on macOS and Linux). It is 0
after `halt.`, or after `-t halt` when every `-g` goal succeeded. It is 1 when a `-g` goal failed, when a file did
not load, or when the command line is wrong. It is 2 when a goal raised an error that nothing caught, or reached the
memory limit. `halt(N)` ends with N. A file with `initialization(main, main)` (chapter 13) ends with 0, 1 or 2 in
the same way, as `main` succeeds, fails or raises an error.

### `mosac`

`mosac` turns a Prolog program into an executable (chapter 13). Its first argument says what to do:

| Command | What it does |
| --- | --- |
| `mosac build file -o name` | Translates the file, and every file it loads with `use_module/1`, into C++, compiles them and writes the executable `name` (`name.exe` on Windows). Without `-o`, the name is the file name without `.pl`. |
| `mosac run file -- argument...` | Builds as `build` does and runs the program at once, with the words after `--` as its arguments. It leaves no executable in the folder, and ends with the exit status of the program. |
| `mosac dist file -o folder` | Builds as `build` does and packs the executable, `README.txt`, `LICENSE` and `NOTICE` into one archive in the folder (`dist` without `-o`): `name-system-processor.tar.gz`, or `.zip` on Windows (chapter 13). It prints the path of the archive. |
| `mosac emit file -o folder` | Writes only the C++ files and a `CMakeLists.txt` into the folder, for reading or changing them by hand. |
| `mosac emit --library file -o folder` | Translates one library of the folder `lib`. The build of Mosa uses it. |
| `mosac emit --units folder -o folder` | Translates a folder of tests. `mosatest --diff` uses it. |
| `mosac toolchain` | Prints the compiler, CMake and the runtime library that `mosac` would use, or what to install. On Windows, when the path of the runtime library or of `.mosac-cache` has a character outside ASCII, `mosac` does not use CMake, and calls the compiler directly (chapter 13). |
| `mosac --version` | Prints `mosa 0.1.1`. |

`mosac build`, `mosac run` and `mosac dist` take these options:

| Option | What it does |
| --- | --- |
| `-o name` | `build`: the name of the executable. `dist`: the folder for the archive. |
| `--interactive` | `build` and `dist` only: the executable shows the `?-` prompt with your program loaded, instead of running `initialization(main, main)`. |
| `--debug` | Compiles without optimization, and the program prints each clause of your own files as it runs it. The libraries are not traced. |
| `--verbose` | Prints what `mosac` does, and the messages of the compiler when the generated C++ does not compile. |
| `--clean` | Deletes `.mosac-cache` first, so that everything is built again. |
| `--keep` | `run` only: keeps the temporary folder that the program ran from. |
| `--cxx compiler` | Uses this compiler instead of looking for one. `mosac toolchain` takes it too. |
| `--runtime-source` | Builds the runtime library from the source in the package, for a compiler that the package has no runtime library for (chapter 13). |

`mosac` looks for a compiler in this order: `--cxx`, then `MOSA_CXX`, then `CXX`, then the usual names (`clang++` on
macOS, `g++` and then `clang++` on Linux, Visual Studio on Windows). It exits with 0 when it succeeds and with
another status when it does not. A mistake in your program stops `mosac build` with its line and column, as in
`mosa`.

### `mosatest`

`mosatest` runs tests. The first line below is for tests of your own, and the last one checks the examples of this
manual. The others are tools for the developers of Mosa.

| Command | What it does |
| --- | --- |
| `mosatest file` | Loads the facts `test(Name, Goal, Expected)` of the file and runs each goal. `Expected` is `true`, `fail`, `error(Formal)` or `solutions(List)`, where `List` gives every solution in order. Prints each goal that does not do what is expected, and ends with 1 if there is one. A folder runs each of its `.pl` files. |
| `mosatest --suite folder --report file` | Runs the ISO conformance suite of the source tree. |
| `mosatest --diff folder --report file` | Runs each test with `mosa` and with `mosac`, and compares the two. |
| `mosatest --bench file --count n` | Times a program with `mosa` and with `mosac`. |
| `mosatest --roundtrip file` | Writes each term of the file and reads it back. |
| `mosatest --manual folder` | Runs the examples of this manual. The folder holds `en` and `ja`, as `docs/manual` of the package does. `--work folder` names where the examples run, `--mosa path` the `mosa` to check, and `--all-tables` requires every table of predicates. |

## Environment variables

An environment variable is a setting that your shell passes to each program it starts (chapter 11 sets `LANG`).

| Variable | Read by | What it does |
| --- | --- | --- |
| `LANG` | all | The language of messages: Japanese when the value starts with `ja`, English otherwise. When it is not set or empty, Mosa on Windows follows the Windows display language, and Mosa on macOS and Linux uses English (chapter 11). |
| `MOSA_HEAP_MAX` | `mosa`, executables of `mosac` | The memory limit in MiB, as `--heap-max`. A value that is not a whole number is ignored with a warning. |
| `MOSA_LIB` | `mosa`, `mosac` | A folder that `library(Name)` looks in before the folder `lib` beside the program (chapter 12). |
| `MOSA_CXX` | `mosac` | The C++ compiler to use, when `--cxx` is not given. |
| `CXX` | `mosac` | The C++ compiler to use, when neither `--cxx` nor `MOSA_CXX` is given. |
| `MOSA_NO_CMAKE` | `mosac` | With `1`, `mosac` calls the compiler itself even when CMake is installed. |
| `MOSA_WERROR` | `mosac` | With `1`, a warning of the compiler on the generated C++ counts as an error. Mosa's own tests set it. |
| `MOSA_GC_STRESS` | `mosa`, executables of `mosac` | For testing Mosa only: with a number N, memory is collected every N steps, which makes programs much slower. |
| `MOSA_TRANSCRIPT` | `mosa` | For testing Mosa only: with `1`, `mosa` reads input that is not typed at a terminal as if it were, and shows each line it reads. `mosatest --manual` sets it. |

## Flags

A flag is a setting of Mosa, read with `current_prolog_flag/2` and changed with `set_prolog_flag/2`. A flag set at
the prompt lasts until you change it or quit:

<!-- check: flags -->
```text
?- current_prolog_flag(double_quotes, X).
X = codes.
?- set_prolog_flag(double_quotes, atom).
true.
?- X = "abc".
X = abc.
```

A flag whose "Other values" says "none" cannot be changed.

| Flag | Value at start | Other values | What it means |
| --- | --- | --- | --- |
| `bounded` | `true` | none | Integers have a smallest and a largest value. |
| `max_integer` | `9223372036854775807` | none | The largest integer. A result beyond it raises `evaluation_error(int_overflow)`. |
| `min_integer` | `-9223372036854775808` | none | The smallest integer. |
| `integer_rounding_function` | `toward_zero` | none | How `//` rounds: `-7 // 2` is `-3`. |
| `max_arity` | `1024` | none | The most arguments that a compound term can have. |
| `double_quotes` | `codes` | `chars`, `atom` | What `"abc"` reads as: a list of character codes, a list of characters or an atom. |
| `unknown` | `error` | `fail` | What a call of an undefined predicate does: raise `existence_error` or fail. |
| `char_conversion` | `off` | `on` | Whether `char_conversion/2` changes characters as terms are read. |
| `debug` | `off` | `on` | ISO asks for this flag. In Mosa 0.1.1, `on` changes nothing. |
| `encoding` | `utf8` | none | Text is UTF-8. |
| `invalid_utf8` | `error` | `replace` | What reading bytes that are not UTF-8 does: raise `representation_error(character)`, or read U+FFFD instead. |
| `japanese_punctuation` | `false` | `true` | Whether full-width punctuation is read as half-width (chapter 11). |
| `message_language` | from `LANG` (on Windows without `LANG`, from the display language) | `ja`, `en` | The language of messages (chapter 11). |
| `float_overflow` | `error` | none | A float result too large raises `evaluation_error(float_overflow)`. |
| `float_zero_div` | `error` | none | A float divided by zero raises `evaluation_error(zero_divisor)`. |
| `float_undefined` | `error` | none | A float result that does not exist, as for `log(0)`, raises `evaluation_error(undefined)`. |
| `dialect` | `mosa` | none | The name of this Prolog system. |
| `version` | `101` | none | The version as one integer: 0.1.1. |
| `version_data` | `mosa(0,1,1,[])` | none | The version as a term. |
| `argv` | `[]` | none | The words after `--` on the command line of `mosa`, or after the name of an executable. |
| `trace_gc` | `false` | `true` | With `true`, Mosa prints a line each time it collects memory. |

The flags from `bounded` to `debug` are those of ISO clause 7.11, and the others are Mosa's. In a file,
`double_quotes` and `japanese_punctuation` set by a directive last to the end of that file.

## Directives

A directive is a line `:- Goal.` in a file, run while the file loads. Most of these are not predicates and work only
in a file, not at the `?-` prompt; `consult/1`, `op/3`, `char_conversion/2` and `set_prolog_flag/2` are built-in
predicates too. `dynamic/1`, `discontiguous/1`, `multifile/1`, `op/3`, `char_conversion/2`, `initialization/1`,
`include/1`, `ensure_loaded/1` and `set_prolog_flag/2` are the directives of ISO clause 7.4.2.

| Directive | What it does |
| --- | --- |
| `module/2` | `:- module(Name, Exports).` as the first line makes the file the module `Name`, which exports the predicates in the list (chapter 12). |
| `use_module/1` | `:- use_module(File).` loads a module once and imports all that it exports. `File` is `library(Name)` or a file of your own (chapter 12). |
| `use_module/2` | `:- use_module(File, Imports).` imports only the predicates in the list. `Name/Arity as New` gives one a new name, and `except(List)` imports all but those. |
| `export/1` | `:- export(Name/Arity).` adds a predicate to the exports of the module. |
| `ensure_loaded/1` | `:- ensure_loaded(File).` puts the clauses of the file here, unless it was already loaded. |
| `include/1` | `:- include(File).` puts the clauses of the file here, each time. |
| `consult/1` | `:- consult(File).` loads the file, as at the prompt. `:- [File1, File2].` loads several. |
| `initialization/1` | `:- initialization(Goal).` runs the goal once, after the file has loaded. |
| `initialization/2` | `:- initialization(main, main).` runs `main` after loading and then ends the program (chapter 13). |
| `dynamic/1` | `:- dynamic(Name/Arity).` lets `assertz/1`, `retract/1` and the others change the predicate. |
| `discontiguous/1` | `:- discontiguous(Name/Arity).` allows the clauses of the predicate to be apart in the file. |
| `multifile/1` | `:- multifile(Name/Arity).` allows clauses of the predicate in several files. |
| `meta_predicate/1` | `:- meta_predicate(Spec).` marks which arguments of a predicate are goals, so that they run in the module of the caller. |
| `op/3` | `:- op(Priority, Type, Name).` defines an operator for the rest of the file. |
| `char_conversion/2` | `:- char_conversion(A, B).` reads the character A as B, when the flag `char_conversion` is `on`. |
| `set_prolog_flag/2` | `:- set_prolog_flag(Flag, Value).` sets a flag. |

Any other `:- Goal.` runs the goal once at that point of the file. If it fails or raises an error, the file does not
load.

## Built-in predicates

The built-in predicates work in every module, with no `use_module`. The tables below list all of them. The column
ISO gives the clause of ISO/IEC 13211-1, the standard that defines Prolog, or of its part 2 on modules
(13211-2). A dash means that the predicate is not in ISO. `Name/Arity` names a predicate by its name and its number
of arguments.

### Control

These run goals and choose between them (chapters 8 and 9). Part 2 of the standard does not list `:/2` among its
built-in predicates; the row gives 6.5.2, the clause that turns a module-qualified term into a goal.

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `true/0` | 7.8.1 | Succeeds. |
| `fail/0` | 7.8.2 | Fails. |
| `false/0` | 8.15.5 | Fails, as `fail` does. |
| `!/0` | 7.8.4 | The cut: removes the other choices of the clause and of the goals before it in the clause. |
| `','/2` | 7.8.5 | `(A, B)`: A, then B. |
| `;/2` | 7.8.6, 7.8.8 | `(A ; B)`: A, or else B. `(C -> T ; E)`: T if C succeeds, else E. |
| `->/2` | 7.8.7 | `(C -> T)`: T after the first answer of C; fails if C fails. |
| `*->/2` | — | `(C *-> T ; E)`: T after each answer of C, or E if C has none. |
| `if/3` | — | `if(C, T, E)`: as `(C *-> T ; E)`. |
| `\+/1` | 8.15.1 | `\+ G`: succeeds when G has no answer, and binds nothing. |
| `call/1` | 7.8.3 | `call(G)`: runs the goal G. A cut inside G stays inside. |
| `call/2` | 8.15.4 | `call(G, A)`: adds A as the last argument of G, then runs it. |
| `call/3` | 8.15.4 | The same with two more arguments. |
| `call/4` | 8.15.4 | The same with three more arguments. |
| `call/5` | 8.15.4 | The same with four more arguments. |
| `call/6` | 8.15.4 | The same with five more arguments. |
| `call/7` | 8.15.4 | The same with six more arguments. |
| `call/8` | 8.15.4 | The same with seven more arguments. |
| `once/1` | 8.15.2 | `once(G)`: the first answer of G only. |
| `repeat/0` | 8.15.3 | Succeeds again each time Prolog backtracks into it. |
| `forall/2` | — | `forall(C, A)`: succeeds when A succeeds for every answer of C. |
| `catch/3` | 7.8.9 | `catch(G, Catcher, Recovery)`: runs G, and runs Recovery if G raises an error that unifies with Catcher. |
| `throw/1` | 7.8.10 | Raises the term as an exception. |
| `setup_call_cleanup/3` | — | `setup_call_cleanup(S, G, C)`: runs S once, then G, then C once when G is done: after its last answer, a failure, an error or a cut. |
| `:/2` | 13211-2 6.5.2 | `M:G`: runs G in the module M (chapter 12). |

### Unification and comparison

These unify terms (chapter 5), compare them in the standard order of terms (variables first, then numbers, atoms
and compound terms) and sort them.

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `=/2` | 8.2.1 | Unifies two terms. |
| `\=/2` | 8.2.3 | Succeeds when the two terms do not unify. |
| `unify_with_occurs_check/2` | 8.2.2 | Unifies, but fails where `=` would make a term that holds itself, as in `X = f(X)`. |
| `==/2` | 8.4.1 | The two terms are the same, without binding anything. |
| `\==/2` | 8.4.1 | The two terms are not the same. |
| `@</2` | 8.4.1 | The first term comes before the second in the standard order. |
| `@=</2` | 8.4.1 | Before, or the same. |
| `@>/2` | 8.4.1 | After. |
| `@>=/2` | 8.4.1 | After, or the same. |
| `compare/3` | 8.4.2 | `compare(Order, A, B)`: Order is `<`, `=` or `>`, as A comes before B, is the same, or comes after. |
| `sort/2` | 8.4.3 | Sorts a list in the standard order and removes duplicates. |
| `sort/4` | — | `sort(Key, Order, List, Sorted)`: sorts on argument Key of each element (0 for the whole element), with Order one of `@<`, `@=<`, `@>`, `@>=`. `@<` and `@>` remove elements with equal keys. |
| `keysort/2` | 8.4.4 | Sorts a list of pairs `Key-Value` by key, and keeps pairs with equal keys in their order. |

### Types

These check what kind of term the argument is now. None of them binds anything.

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `var/1` | 8.3.1 | A variable without a value. |
| `nonvar/1` | 8.3.7 | Not a variable without a value. |
| `atom/1` | 8.3.2 | An atom. |
| `integer/1` | 8.3.3 | An integer. |
| `float/1` | 8.3.4 | A floating-point number. |
| `number/1` | 8.3.8 | An integer or a float. |
| `atomic/1` | 8.3.5 | An atom or a number. |
| `compound/1` | 8.3.6 | A compound term, such as `f(a)` or `[a]`. |
| `callable/1` | 8.3.9 | An atom or a compound term: something that can run as a goal. |
| `ground/1` | 8.3.10 | A term with no variable without a value inside. |
| `is_list/1` | — | A list that ends in `[]`. |

### Terms

These take terms apart and build them.

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `functor/3` | 8.5.1 | `functor(T, Name, Arity)`: the name and the number of arguments of T, or a new T with new variables as arguments. |
| `arg/3` | 8.5.2 | `arg(N, T, A)`: A is argument N of T, counting from 1. |
| `=../2` | 8.5.3 | `T =.. L`: L is the name of T followed by its arguments, as in `f(a, b) =.. [f, a, b]`. |
| `copy_term/2` | 8.5.4 | A copy of a term, with new variables. |
| `term_variables/2` | 8.5.5 | The list of the variables of a term, from left to right, each once. |
| `numbervars/3` | — | `numbervars(T, Start, End)`: binds the variables of T to `'$VAR'(Start)`, `'$VAR'(Start+1)` and so on, which `write/1` shows as `A`, `B`, …. |
| `length/2` | — | `length(L, N)`: N is the number of elements of L. With L unbound, makes a list of N new variables (chapter 7). |

### Arithmetic

`is/2` evaluates an arithmetic expression, and the comparisons evaluate both sides (chapter 6).

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `is/2` | 8.6.1 | `X is E`: evaluates E and unifies X with its value. |
| `=:=/2` | 8.7.1 | The two values are equal. |
| `=\=/2` | 8.7.1 | The two values are not equal. |
| `</2` | 8.7.1 | The first value is less than the second. |
| `=</2` | 8.7.1 | Less than or equal. |
| `>/2` | 8.7.1 | Greater than. |
| `>=/2` | 8.7.1 | Greater than or equal. |

### Atoms and text

These work on the characters of atoms and numbers. A character is one Unicode scalar value (chapter 11).

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `atom_length/2` | 8.16.1 | The number of characters of an atom. |
| `atom_concat/3` | 8.16.2 | `atom_concat(A, B, AB)`: AB is A followed by B. With AB alone given, gives each way to split it. |
| `sub_atom/5` | 8.16.3 | `sub_atom(Atom, Before, Length, After, Sub)`: Sub is the part of Atom of Length characters, with Before characters before it and After after it. |
| `atom_chars/2` | 8.16.4 | Between an atom and the list of its characters. |
| `atom_codes/2` | 8.16.5 | Between an atom and the list of its character codes. |
| `char_code/2` | 8.16.6 | Between a character and its code. |
| `number_chars/2` | 8.16.7 | Between a number and the list of the characters that write it. |
| `number_codes/2` | 8.16.8 | Between a number and the list of the character codes that write it. |
| `atom_number/2` | — | Between an atom such as `'42'` and the number that it writes. |
| `term_to_atom/2` | — | Between a term and an atom that holds the term as text. |

### Clauses and modules

These look at the clauses of predicates and change them, and look at modules.

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `asserta/1` | 8.9.1 | Adds a clause before the other clauses of its predicate. |
| `assertz/1` | 8.9.2 | Adds a clause after the other clauses of its predicate. |
| `retract/1` | 8.9.3 | Removes the first clause that unifies with the argument. |
| `retractall/1` | 8.9.5 | `retractall(Head)`: removes every clause whose head unifies with Head. |
| `abolish/1` | 8.9.4 | `abolish(Name/Arity)`: removes a dynamic predicate and all its clauses. |
| `clause/2` | 8.8.1 | `clause(Head, Body)`: a clause of a predicate of your program, whether it was loaded from a file or added with `assertz`. A built-in predicate raises a `permission_error`. |
| `current_predicate/1` | 8.8.2 | `current_predicate(Name/Arity)`: a predicate of your program that has clauses or is dynamic (declared with `dynamic`, or created by `assertz`/`asserta`). Built-in predicates are not listed. |
| `predicate_property/2` | 13211-2 7.2.2 | `predicate_property(Head, P)`: P is a property of the predicate, such as `built_in`, `dynamic` or `exported`. |
| `current_module/1` | 13211-2 7.2.1 | A module that exists. |
| `consult/1` | — | Loads a file (chapter 4). The name needs its `.pl`. |

### All solutions

These collect the answers of a goal in a list (chapter 8).

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `findall/3` | 8.10.1 | `findall(T, G, L)`: L is the list of T for every answer of G, or `[]`. |
| `findall/4` | — | `findall(T, G, L, Tail)`: as `findall/3`, with the list ending in Tail instead of `[]`. |
| `bagof/3` | 8.10.2 | As `findall/3`, but fails when G has no answer, and gives one list for each value of the other variables of G. |
| `setof/3` | 8.10.3 | As `bagof/3`, with each list sorted and without duplicates. |

### Streams

A stream is where text is read from or written to: a file, the terminal or memory (chapter 10). The predicates with
a stream argument take a stream, or an alias such as `user_input`, `user_output` or `user_error`.

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `open/3` | 8.11.5 | `open(File, Mode, S)`: opens a file to `read`, `write` or `append`, and gives the stream S. |
| `open/4` | 8.11.5 | As `open/3`, with a list of options such as `type(binary)` or `alias(Name)`. |
| `open_string/2` | — | `open_string(Text, S)`: a stream that reads from the atom or the list of codes Text. |
| `close/1` | 8.11.6 | Closes a stream. |
| `close/2` | 8.11.6 | As `close/1`, with a list of options: `force(true)` closes even after an error. |
| `flush_output/0` | 8.11.7 | Writes out what waits in the buffer of the current output. |
| `flush_output/1` | 8.11.7 | The same for a stream. |
| `current_input/1` | 8.11.1 | The current input stream. |
| `current_output/1` | 8.11.2 | The current output stream. |
| `set_input/1` | 8.11.3 | Makes a stream the current input. |
| `set_output/1` | 8.11.4 | Makes a stream the current output. |
| `stream_property/2` | 8.11.8 | `stream_property(S, P)`: P is a property of the open stream S, such as `file_name(F)` or `alias(A)`. |
| `at_end_of_stream/0` | 8.11.8 | The current input has nothing more to read. |
| `at_end_of_stream/1` | 8.11.8 | The same for a stream. |
| `set_stream_position/2` | 8.11.9 | Moves a stream to a position that `stream_property/2` gave as `position(P)`. |
| `with_output_to/2` | — | `with_output_to(atom(A), G)`: runs G once, and A is the atom of what G wrote. `codes(C)` gives a list of codes. |

### Characters and bytes

These read and write one character or one byte. Each predicate with one argument fewer uses the current input or
output.

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `get_char/1` | 8.12.1 | Reads the next character, or `end_of_file` at the end. |
| `get_char/2` | 8.12.1 | The same from a stream. |
| `get_code/1` | 8.12.1 | Reads the code of the next character, or -1 at the end. |
| `get_code/2` | 8.12.1 | The same from a stream. |
| `peek_char/1` | 8.12.2 | As `get_char/1`, but the character stays to be read. |
| `peek_char/2` | 8.12.2 | The same from a stream. |
| `peek_code/1` | 8.12.2 | As `get_code/1`, but the character stays to be read. |
| `peek_code/2` | 8.12.2 | The same from a stream. |
| `put_char/1` | 8.12.3 | Writes a character. |
| `put_char/2` | 8.12.3 | The same on a stream. |
| `put_code/1` | 8.12.3 | Writes the character of a code. |
| `put_code/2` | 8.12.3 | The same on a stream. |
| `nl/0` | 8.12.3 | Ends the line. |
| `nl/1` | 8.12.3 | The same on a stream. |
| `tab/1` | — | `tab(N)`: writes N spaces. |
| `tab/2` | — | The same on a stream. |
| `get_byte/1` | 8.13.1 | Reads the next byte (0 to 255) of a binary stream, or -1 at the end. |
| `get_byte/2` | 8.13.1 | The same from a stream. |
| `peek_byte/1` | 8.13.2 | As `get_byte/1`, but the byte stays to be read. |
| `peek_byte/2` | 8.13.2 | The same from a stream. |
| `put_byte/1` | 8.13.3 | Writes a byte. |
| `put_byte/2` | 8.13.3 | The same on a stream. |

### Reading and writing terms

These read and write whole terms, and define operators (chapter 10).

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `read_term/2` | 8.14.1 | `read_term(T, Options)`: reads a term that ends with a period, with options such as `variable_names(V)`. |
| `read_term/3` | 8.14.1 | The same from a stream. |
| `read/1` | 8.14.1 | Reads a term, or `end_of_file` at the end. |
| `read/2` | 8.14.1 | The same from a stream. |
| `write_term/2` | 8.14.2 | `write_term(T, Options)`: writes a term, with options such as `quoted(true)`. |
| `write_term/3` | 8.14.2 | The same on a stream. |
| `write/1` | 8.14.2 | Writes a term as text. |
| `write/2` | 8.14.2 | The same on a stream. |
| `writeq/1` | 8.14.2 | Writes a term with quotes where `read/1` needs them to read it back. |
| `writeq/2` | 8.14.2 | The same on a stream. |
| `write_canonical/1` | 8.14.2 | Writes a term with quotes and without operators. |
| `write_canonical/2` | 8.14.2 | The same on a stream. |
| `print/1` | — | Writes a term as `write/1` does, or with `portray/1` when your program defines it. |
| `print/2` | — | The same on a stream. |
| `op/3` | 8.14.3 | `op(Priority, Type, Name)`: defines an operator, such as `op(700, xfx, is_in)`. |
| `current_op/3` | 8.14.4 | `current_op(Priority, Type, Name)`: an operator that is defined. |
| `char_conversion/2` | 8.14.5 | `char_conversion(A, B)`: reads the character A as B, when the flag `char_conversion` is `on`. |
| `current_char_conversion/2` | 8.14.6 | A conversion that `char_conversion/2` set. |

### Flags and the system

These read and set flags, end Mosa, and look at memory and time.

<!-- predicates: system -->
| Predicate | ISO | What it does |
| --- | --- | --- |
| `current_prolog_flag/2` | 8.17.2 | `current_prolog_flag(Flag, Value)`: the value of a flag. |
| `set_prolog_flag/2` | 8.17.1 | Sets a flag that can change. |
| `halt/0` | 8.17.3 | Ends Mosa, with exit status 0. |
| `halt/1` | 8.17.4 | `halt(N)`: ends Mosa, with exit status N. |
| `nb_setval/2` | — | `nb_setval(Key, Value)`: keeps a copy of Value under the atom Key. Backtracking does not undo it. |
| `nb_getval/2` | — | `nb_getval(Key, Value)`: a copy of what `nb_setval/2` keeps under Key. |
| `garbage_collect/0` | — | Frees now the memory of terms that are no longer used. |
| `statistics/2` | — | `statistics(Key, Value)`: numbers on time (`runtime`, `walltime`, `cputime`), memory (`heap`) and its collection (`garbage_collection`). |

## Libraries

A library is loaded with `:- use_module(library(Name)).` in a file (chapter 12). None of these predicates is in ISO.

### `library(lists)`

<!-- predicates: lists -->
| Predicate | What it does |
| --- | --- |
| `append/3` | `append(A, B, AB)`: AB is the list A followed by B. With AB alone given, gives each way to split it. |
| `member/2` | `member(X, L)`: X is an element of L, one answer for each element. |
| `memberchk/2` | As `member/2`, with the first element that unifies only. |
| `length/2` | The built-in `length/2`, exported again. |
| `nth0/3` | `nth0(I, L, X)`: X is element I of L, counting from 0. |
| `nth1/3` | `nth1(I, L, X)`: the same, counting from 1. |
| `last/2` | `last(L, X)`: X is the last element of L. |
| `reverse/2` | The list in the reverse order. |
| `msort/2` | Sorts a list in the standard order, and keeps duplicates. |
| `sum_list/2` | The sum of a list of numbers. |
| `max_list/2` | The largest of a list of numbers. |
| `list_to_set/2` | The list without the elements that are the same (`==`) as an earlier one. |

### `library(apply)`

These call a goal G for elements of lists. G is a goal with some of its arguments left out, such as `atom_length`
in `maplist(atom_length, [ab, c], L)`, and each call adds the elements as its last arguments.

<!-- predicates: apply -->
| Predicate | What it does |
| --- | --- |
| `maplist/2` | `maplist(G, L)`: G succeeds for each element of L. |
| `maplist/3` | `maplist(G, L1, L2)`: G succeeds for each pair of elements of two lists of the same length. |
| `maplist/4` | The same with three lists. |
| `maplist/5` | The same with four lists. |
| `foldl/4` | `foldl(G, L, V0, V)`: calls G with each element, the value so far and the next value, from V0 to V. |
| `foldl/5` | The same with two lists. |
| `foldl/6` | The same with three lists. |
| `include/3` | `include(G, L, In)`: In is the list of the elements of L for which G succeeds. |
| `exclude/3` | `exclude(G, L, Out)`: Out is the list of the elements of L for which G fails. |

### `library(between)`

<!-- predicates: between -->
| Predicate | What it does |
| --- | --- |
| `between/3` | `between(Low, High, X)`: X is each integer from Low to High in turn. |
| `succ/2` | `succ(X, Y)`: Y is X + 1, with X 0 or more. |
| `plus/3` | `plus(X, Y, Z)`: Z is X + Y. Any one of the three may be unbound. |

### `library(format)`

The format text holds directives that start with `~`: `~w` writes an argument as `write/1`, `~q` as `writeq/1`, `~a`
an atom, `~d` an integer, `~f` and `~e` a float, `~s` a list of codes, `~c` a character code, `~n` a new line, `~t`
and `~|` set columns, and `~~` writes `~`.

<!-- predicates: format -->
| Predicate | What it does |
| --- | --- |
| `format/1` | `format(F)`: writes the format text F. |
| `format/2` | `format(F, Args)`: writes F, with the elements of the list Args in place of its directives. |
| `format/3` | `format(S, F, Args)`: the same on the stream S. |

### `library(strings)`

<!-- predicates: strings -->
| Predicate | What it does |
| --- | --- |
| `atomic_list_concat/2` | Joins the atoms and numbers of a list into one atom. |
| `atomic_list_concat/3` | `atomic_list_concat(L, Sep, A)`: joins them with Sep between them. With L unbound, splits A at each Sep. |
| `split_atom/4` | `split_atom(Text, Seps, Pad, Parts)`: splits Text at each character of Seps, and removes the characters of Pad around each part. |
| `upcase_atom/2` | The atom with its letters made capital. |

### `library(japanese)`

<!-- predicates: japanese -->
| Predicate | What it does |
| --- | --- |
| `ひらがな_カタカナ/2` | Hiragana to katakana, or back when only the second argument is given. |
| `全角_半角/2` | Full-width letters, digits and symbols to half-width, or back when only the second argument is given. |
| `読み幅/2` | The width of a text on screen: 2 for each wide character, such as a kanji, and 1 for the others. |

## Differences from ISO

Mosa follows ISO/IEC 13211-1 and its corrigenda. Where it does not, or where ISO leaves the choice to each system:

- Integers are 64-bit: there are no larger integers, and going past `max_integer` raises
  `evaluation_error(int_overflow)`. Floats are 64-bit, and Mosa writes the shortest text that reads back as the same
  float.
- `subsumes_term/2` (8.2.4) and `acyclic_term/1` (8.3.11) are not built in.
- `unknown` takes only `error` and `fail`, and the three `float_*` flags take only `error`.
- Modules (chapter 12) use the syntax of part 2 of the standard (ISO/IEC 13211-2), and a module holds predicates and
  operators. Atoms are the same in every module.
- The predicates marked with a dash above are additions to ISO. `format/2`, `between/3`, `member/2`, `append/3` and
  the other predicates of the libraries are not built in, and need `use_module`.
- Full-width punctuation is read as half-width only when the flag `japanese_punctuation` is `true` (chapter 11).

The reasons for these choices, clause by clause, are in the
[ISO interpretation notes](https://github.com/SilentMalachite/mosa-prolog/blob/main/docs/iso-notes.md) of the
source tree.

## Known limits

The [CHANGELOG](../../../CHANGELOG.md) lists the known limits of Mosa 0.1.1, and is updated when one is found or
removed. Among them:

- Grammar rules written with `-->` are not supported: a file that holds one does not load.
- Typing Japanese in the console window of Windows has not been verified.
- `mosac build --debug` links the runtime library built with optimization, and the libraries are not traced.
- A term nested very deeply in a file, such as `f(f(f(…)))`, can crash Mosa while it reads the file: from about 600
  levels on Windows, and from a few thousand on macOS and Linux.
- On Windows, Mosa does not start while Smart App Control is on (chapter 2).

---

[Index](README.md) · Previous: [13. Building executables with `mosac`](13-mosac.md)
