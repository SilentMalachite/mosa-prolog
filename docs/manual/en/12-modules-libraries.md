English | [日本語](../ja/12-modules-libraries.md)

# 12. Modules and libraries

In this chapter you load libraries that come with Mosa, use `append/3`, `member/2` and `format/2` from them, and
write a module of your own.

## Steps

1. Move into the folder `prolog` with the `cd` line of chapter 4, and start `mosa`. Join two lists with `append/3`,
   which works like your `app/3` of chapter 7. Mosa answers with the warning of chapter 3: there is no such predicate
   yet. `append/3` is in a library, a file of predicates written in Prolog that comes with Mosa. Its predicates are
   there only after the library is loaded with `use_module/1`, and `use_module/1` works only in a file: typed at the
   `?-` prompt, it gives the same kind of warning.

<!-- check: no-library -->
```text
?- append([a, b], [c, d], X).
Warning: goal raised exception: error(existence_error(procedure,append/3),context(append/3,_G1))
?- use_module(library(lists)).
Warning: goal raised exception: error(existence_error(procedure,use_module/1),context(use_module/1,_G1))
```

2. Create a file `libs.pl` in the folder `prolog` with this one line. It is a directive, as in chapter 11: while
   Mosa loads the file, it runs `use_module(library(lists))`, which loads the library named `lists`.
   `library(lists)` tells Mosa to look for it in the folder `lib` beside `mosa`, which is why chapter 2 asked you to
   keep the folder of Mosa together.

<!-- file: libs-file libs.pl -->
```prolog
:- use_module(library(lists)).
```

3. Load the file, and join the two lists again. Then split a list, as you did with `app/3`, pressing `;` until
   `false.`.

<!-- check: append
consult('libs.pl').
append([a, b], [c, d], X).
append(X, Y, [a, b]).
;
;
;
-->
```text
?- consult('libs.pl').
true.
?- append([a, b], [c, d], X).
X = [a,b,c,d].
?- append(X, Y, [a, b]).
X = [], Y = [a,b];

X = [a], Y = [b];

X = [a,b], Y = [];

false.
```

4. Use `member/2`, from the same library. `member(X, L)` is true when `X` is an element of the list `L`. With a
   variable for `X`, it gives each element in turn. With a value, it checks whether the value is in the list. After
   `X = c`, and after `true` in the second query, Mosa has not yet tried everything and waits without a period, as in
   chapter 4. Press `;`, and it finds no other answer.

<!-- check: member
consult('libs.pl').
member(X, [a, b, c]).
;
;
;
member(b, [a, b, c]).
;
member(d, [a, b, c]).
-->
```text
?- consult('libs.pl').
true.
?- member(X, [a, b, c]).
X = a;

X = b;

X = c;

false.
?- member(b, [a, b, c]).
true;

false.
?- member(d, [a, b, c]).
false.
```

5. Print text with `format/2`, from the library `format`. Create a file `prices.pl` in the folder `prolog` with
   these lines, the prices of chapter 6 and a rule that prints them. `format(Text, Args)` prints the atom `Text`,
   with each `~w` replaced by the next element of the list `Args`, written as `write/1` writes it, and each `~n`
   replaced by a new line. `forall(Condition, Action)`, which is built in, runs `Action` once for each answer of
   `Condition`, and succeeds when `Action` succeeded every time. So `show_prices` prints one line for each price.

<!-- file: prices-file prices.pl -->
```prolog
:- use_module(library(format)).

price(apple, 120).
price(bread, 250).
price(milk, 180).

show_prices :- forall(price(Item, Price), format('~w costs ~w yen~n', [Item, Price])).
```

6. Load the file and run `show_prices`.

<!-- check: format -->
```text
?- consult('prices.pl').
true.
?- show_prices.
apple costs 120 yen
bread costs 250 yen
milk costs 180 yen
true.
```

7. Write a module of your own. A module is a set of predicates, usually one file, that is kept apart from the
   predicates of other files. It lets other files see only the predicates that it exports, that is, the ones in its
   export list. The others stay inside. Create a file `family_tree.pl` in the folder `prolog` with these lines. The
   first line is the directive `module/2`. Its first argument names the module `family_tree`, as the file is named,
   and its second argument is the export list, written with the `name/arity` of each predicate. `parent/2` is not in
   the list, so it stays inside the module.

<!-- file: module-file family_tree.pl -->
```prolog
:- module(family_tree, [grandparent/2]).

parent(tom, bob).
parent(tom, liz).
parent(bob, ann).
parent(bob, pat).
parent(pat, jim).

grandparent(X, Z) :- parent(X, Y), parent(Y, Z).
```

8. Use the module from another file. Create a file `report.pl` in the folder `prolog` with these lines.
   `use_module(family_tree)`, without `library(…)`, loads your file `family_tree.pl` from the folder that holds
   `report.pl`. Here you can leave out the `.pl`. The directive imports `grandparent/2`: it makes that exported
   predicate usable in `report.pl`, so the rule `show_grandchildren/1` can call it.

<!-- file: report-file report.pl -->
```prolog
:- use_module(family_tree).
:- use_module(library(format)).

show_grandchildren(X) :- forall(grandparent(X, Z), format('~w is a grandchild of ~w~n', [Z, X])).
```

9. Load `report.pl`, and print Tom's grandchildren. `grandparent/2` works at the prompt too. `parent/2` does not:
   it is not exported, so outside `family_tree` it does not exist. To call it inside the module anyway, write the
   name of the module and a colon before the goal.

<!-- check: module
consult('report.pl').
show_grandchildren(tom).
grandparent(X, jim).
;
parent(tom, X).
family_tree:parent(tom, X).
;
-->
```text
?- consult('report.pl').
true.
?- show_grandchildren(tom).
ann is a grandchild of tom
pat is a grandchild of tom
true.
?- grandparent(X, jim).
X = bob;

false.
?- parent(tom, X).
Warning: goal raised exception: error(existence_error(procedure,parent/2),context(parent/2,_G1))
?- family_tree:parent(tom, X).
X = bob;

X = liz.
```

10. Type `halt.` to quit.

## What happened

Every predicate belongs to a module. The `?-` prompt, and every file without `module/2` such as `libs.pl` and
`report.pl`, use the module `user`. A library is a module too: `library(lists)` is the module `lists`, which exports
`append/3`, `member/2` and others. `use_module/1` imports the exported predicates into the module of the file that
holds the directive. In `report.pl` that is `user`, so `grandparent/2` was usable at the prompt as well.

`parent/2` stayed inside `family_tree`, so another module can have a `parent/2` of its own, and the two do not mix.
`Module:Goal` runs `Goal` inside the module `Module`.

`use_module/1` takes two kinds of names:

- `library(Name)` names a library. Mosa looks for the file `Name.pl` in the folder `lib` beside `mosa`.
- A plain name, such as `family_tree`, names a file of your own. Mosa looks for it in the folder of the file that
  holds the directive, and adds `.pl` when the name has none, unlike `consult/1`, which needs the whole name
  (chapter 4). `use_module('family_tree.pl')` works too. When no file has that name, loading stops, and Mosa names
  the file being loaded and the name it could not find: with `use_module(family_tre)`, `consult('report.pl')` prints
  `report.pl: family_tre: cannot open file` and `false.`.

`use_module/1` loads a file only once. If you change `family_tree.pl` and load `report.pl` again, Mosa does not read
`family_tree.pl` again, so the change does not show. Quit Mosa and start it again, or load the module file itself
with `consult('family_tree.pl')`. `use_module/1` cannot be typed at the prompt, so to use a library there, put the
directive in a file, as `libs.pl` does, and load that file.

These libraries come with Mosa. Each is loaded with `use_module(library(Name))`:

| Library | Some of its predicates |
| --- | --- |
| `lists` | `append/3`, `member/2`, `reverse/2`, `last/2`, `nth1/3`, `sum_list/2`, `msort/2` |
| `apply` | `maplist/2` to `maplist/5`, `foldl/4` to `foldl/6`, `include/3`, `exclude/3` |
| `between` | `between/3`, `succ/2`, `plus/3` |
| `format` | `format/1`, `format/2`, `format/3` |
| `strings` | `atomic_list_concat/2`, `atomic_list_concat/3`, `split_atom/4`, `upcase_atom/2` |
| `dcg` | `expand_term/2` |
| `japanese` | `ひらがな_カタカナ/2`, `全角_半角/2`, `読み幅/2` |

`length/2` is in `lists` too, but it is also built in, so it works without the library, as in chapter 7. The built-in
predicates, such as `atom_length/2` and `forall/2`, are in the module `system`, and every module can use them.

Export and import lists, and the directives `dynamic`, `discontiguous` and `multifile`, can name a non-terminal of a grammar rule as `Name//Arity`: `greet//0` stands for `greet/2`.

## Exercises

1. Load `libs.pl`, and reverse the list `[1, 2, 3]` with `reverse/2` from `library(lists)`.
2. Export `parent/2` from `family_tree` too, and ask at the prompt who Tom's children are.

### Answers

1. `reverse(L, R)` is true when `R` holds the elements of `L` in the opposite order:

<!-- check: exercise-reverse -->
```text
?- consult('libs.pl').
true.
?- reverse([1, 2, 3], L).
L = [3,2,1].
```

2. Add `parent/2` to the export list of `family_tree.pl`:

<!-- file: exercise-module-file family_tree.pl -->
```prolog
:- module(family_tree, [parent/2, grandparent/2]).

parent(tom, bob).
parent(tom, liz).
parent(bob, ann).
parent(bob, pat).
parent(pat, jim).

grandparent(X, Z) :- parent(X, Y), parent(Y, Z).
```

`use_module/1` does not read the changed file again in the same session, so quit Mosa, start it again, and load
`report.pl`:

<!-- check: exercise-parent
consult('report.pl').
parent(tom, X).
;
-->
```text
?- consult('report.pl').
true.
?- parent(tom, X).
X = bob;

X = liz.
```

---

[Index](README.md) · Previous: [11. Writing in Japanese](11-japanese.md) · Next:
[13. Building executables with `mosac`](13-mosac.md)
