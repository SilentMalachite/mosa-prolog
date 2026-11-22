English | [日本語](../ja/15-debugger.md)

# 15. The debugger

In this chapter you watch Mosa run a goal one step at a time, see where it enters and leaves each predicate, and
jump straight to the predicate you care about in a larger search.

You can read this chapter at any time after chapter 8. It uses facts, rules, lists and recursion, and the
backtracking that you met with `;`.

## Steps

1. Move into the folder `prolog`, and start `mosa`. Create a file `len.pl` with a predicate `len/2` that gives the
   length of a list, written like `count/2` of chapter 7. Its first clause says the empty list has length 0. Its
   second clause counts the tail first, and then adds one.

<!-- file: len-file len.pl -->
```prolog
len([], 0).
len([_|T], N) :- len(T, M), N is M + 1.
```

2. Load it, and run `len([a], N)` after `trace`. `trace` turns the debugger on, and Mosa now stops each time it
   calls a goal or leaves one. Each stop is one line. At the end of the line, Mosa waits for you to press Enter.
   Press Enter on every line, and watch the goals go by. At the end, the answer comes as usual, and the prompt is
   `[trace] ?- ` while the debugger is on. Type `notrace.` and `nodebug.` to turn it off.

<!-- check: creep
consult('len.pl').
trace, len([a], N).






notrace.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([a], N).
 Call: (1) len([a],_G1) ? 
 Call: (2) len([],_G1) ? 
 Exit: (2) len([],0) ? 
 Call: (2) _G1 is 0+1 ? 
 Exit: (2) 1 is 0+1 ? 
 Exit: (1) len([a],1) ? 
N = 1.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

   In this transcript, where a line ends in `?`, you pressed Enter. The empty input does not show.

3. Read the lines from the top. ` Call: (1) len([a],_G1) ? ` says that Mosa is calling `len([a], N)`, and it is
   the first call that is open, the `(1)`. `_G1` is the unbound variable `N`. The next line, `(2) len([],_G1)`,
   is the call of the second clause's `len(T, M)`, one level deeper. Then ` Exit: (2) len([],0) ` shows that this
   call succeeded and that its variable now has the value 0. After the addition, the line ` Exit: (1) len([a],1) `
   shows the first call finishing with `N = 1`.

## What happened

Every goal that your program calls is a box. Mosa draws a line each time the run crosses one of the four sides of
the box, or leaves it through the bottom with an error. Each of these is called a port:

<!-- skip: ports-diagram -->
```text
            +-----------------------+
   Call --> |                       | --> Exit
            |         goal          |
   Fail <-- |                       | <-- Redo
            +-----------------------+
                       |
                       v
                   Exception
```

- **Call**: Mosa starts the goal.
- **Exit**: the goal succeeded. It may have left choices that Mosa has not tried yet.
- **Redo**: something after the goal failed, so Mosa comes back to the goal to look for another answer.
- **Fail**: the goal has no more answers.
- **Exception**: the goal raised an exception, and the exception passes through the box (the line also gives the
  term that was thrown).

The number in the parentheses is the depth: how many boxes are open now, this one included. A box shows the
goal as Mosa writes it with `writeq`, cut at depth 10 (`w` writes it whole, as you will see below). Variables are
named `_G1`, `_G2` and so on, in the order in which they appear in that line. The same variable can have a
different name in the next line, so do not compare names between lines.

A goal that your own predicates call is shown, and so is a built-in predicate such as `is/2` or `write/1`, but Mosa
does not go inside a built-in; only a goal that you give a built-in to run, as with `findall/3`, `forall/2` or `setup_call_cleanup/3`, is shown, one level deeper. A call that a library predicate makes inside itself is not shown either, so `append/3`, for example, is one box, and its inner recursion is hidden, also when you write it as `lists:append(X, Y, Z)`; a goal of yours that it calls, as `maplist/2` does, is shown. These stay hidden: the goals of a control construct after a library's name, as in `lists:(append(X, Y, Z), member(A, Z))`; a goal with a library's name that a library predicate calls for you, as in `maplist(lists:append([a]), Ls, Rs)`; a goal after two library names, as in `lists:lists:append(X, Y, Z)`; and a goal that you give a built-in written after a library's name, as in `lists:findall(X, member(X, L), Xs)` or `lists:once(G)`, since that goal then runs inside the library. The control constructs (`,`, `;`, `->`, `*->`, `!`, `catch/3`) and a few other goals
(`call/1` to `call/8`, `true`, `fail`, `false`, `throw/1`, `halt/0,1`, `M:Goal`, the debugger's own predicates, and every predicate whose name starts with `$`) have no ports of their own. `\+` has them, as a box around the goal that it negates.

The lines go to the error stream, so they are not mixed with what your program writes.

Here are the ports that the first run did not show. Write a file `ports.pl` with two facts, and a predicate `bad/0` that
raises an error with `throw/1`:

<!-- file: ports-file ports.pl -->
```prolog
c(1).
c(2).
bad :- throw(oops).
```

Ask for a fact that does not exist, and the goal fails right away:

<!-- check: fail-port
consult('ports.pl').
trace, c(3).


notrace.
nodebug.
-->
```text
?- consult('ports.pl').
true.
?- trace, c(3).
 Call: (1) c(3) ? 
 Fail: (1) c(3) ? 
false.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

When the goal after `c(X)` fails, Mosa backtracks into `c(X)`, and you see Redo and then the next answer:

<!-- check: redo-port
consult('ports.pl').
trace, (c(X), X > 1).








notrace.
nodebug.
-->
```text
?- consult('ports.pl').
true.
?- trace, (c(X), X > 1).
 Call: (1) c(_G1) ? 
 Exit: (1) c(1) ? 
 Call: (1) 1>1 ? 
 Fail: (1) 1>1 ? 
 Redo: (1) c(_G1) ? 
 Exit: (1) c(2) ? 
 Call: (1) 2>1 ? 
 Exit: (1) 2>1 ? 
X = 2.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

An error shows up as an Exception line, and then the error is passed on to the caller. Here `catch/3` catches it,
so the query goes on to the answer:

<!-- check: exception-port
consult('ports.pl').
trace, catch(bad, E, true).


notrace.
nodebug.
-->
```text
?- consult('ports.pl').
true.
?- trace, catch(bad, E, true).
 Call: (1) bad ? 
 Exception: (1) bad oops ? 
E = oops.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

## Going back for another answer

4. With `;` you ask Mosa for the next answer of a query. The debugger shows the same thing from the inside: the
   answer comes after an Exit, and `;` turns into a Redo.

<!-- check: retry-more
consult('ports.pl').
trace, c(X).


;


notrace.
nodebug.
-->
```text
?- consult('ports.pl').
true.
?- trace, c(X).
 Call: (1) c(_G1) ? 
 Exit: (1) c(1) ? 
X = 1;

 Redo: (1) c(_G1) ? 
 Exit: (1) c(2) ? 
X = 2.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

   The first answer `X = 1` comes after the Exit line. You typed `;`, so the next stop is the Redo line, and then
   the Exit with `c(2)`. The query had no more choices, so Mosa wrote the answer with a period.

## Jumping into a large search

`trace` shows every port and, by default, stops at each one, and that is too many once a program grows. A spy point asks Mosa to stop only at a
predicate you care about. `spy(Name/Arity)` puts a spy point on the predicate, and when it puts one, it also turns the debugger on.
Between spy points, Mosa runs without stopping, which Prolog people call leaping.

5. Write a file `route.pl`. `path/3` finds the way from one city to another along the `edge/2` roads:

<!-- file: route-file route.pl -->
```prolog
edge(a, b).
edge(a, c).
edge(b, d).
edge(c, d).
edge(d, e).

path(X, X, [X]).
path(X, Z, [X|P]) :- edge(X, Y), path(Y, Z, P).
```

6. Put a spy point on `edge/2`, and ask for the path from `a` to `e`. At each stop type `l`, the command that
   leaps to the next spy point. Mosa shows only the `edge/2` ports. The prompt is `[debug] ?- ` now, because
   the debugger is on but it is not stopping everywhere. Press Enter at the last answer to end the query, then
   remove the spy point and turn the debugger off.

<!-- check: leap
consult('route.pl').
spy(edge/2).
path(a, e, P).
l
l
l
l
l
l

nospy(edge/2).
nodebug.
-->
```text
?- consult('route.pl').
true.
?- spy(edge/2).
% Spy point on edge/2
true.
[debug] ?- path(a, e, P).
 Call: (1) edge(a,_G1) ? l
 Exit: (1) edge(a,b) ? l
 Call: (1) edge(b,_G1) ? l
 Exit: (1) edge(b,d) ? l
 Call: (1) edge(d,_G1) ? l
 Exit: (1) edge(d,e) ? l
P = [a,b,d,e]
.
[debug] ?- nospy(edge/2).
% Spy point removed from edge/2
true.
[debug] ?- nodebug.
true.
```

   Because Mosa opened no box for `path/3`, depth is always `(1)` here: while Mosa leaps, it opens a box only for a
   call of a predicate with a spy point, so the calls it makes during the leap are left out of the depth and out of
   the list that `g` writes. The boxes that were opened earlier, in creep, are still counted. If you press Enter
   instead of `l`, the debugger goes back to creep, which means that Mosa stops everywhere, and the prompt becomes
   `[trace]`.

## What you can type at a stop

At a port that stops, Mosa reads one line. The first character that is not a space says what to do. Here are the
commands:

| You type | What it does |
| --- | --- |
| Enter, `c` | creep: go to the next port, and stop there. |
| `s` | skip: at a Call or Redo port, run the goal without stopping until the next port of this same box. At the other ports it is the same as creep. |
| `l` | leap: run until a port of a predicate with a spy point. |
| `f` | fail: make this goal fail, and show its Fail port. At an exception port, the goal fails without a Fail port. |
| `r` | retry: go back to this goal's Call port, and undo the bindings. Side effects, such as output, are not undone. At the Call port itself, it is the same as creep. |
| `a` | abort: stop the query. The cleanups of `setup_call_cleanup/3` that it leaves run first, with no ports; as with an exception, each sees the bindings of its Setup but not those of its Goal. The debugger keeps its mode, so the prompt is `[trace] ?- `, `[debug] ?- ` or `?- ` as that mode says. |
| `n` | nodebug: turn the debugger off and go on. |
| `g` | write the open boxes, from the innermost one, with their depth, and ask again. |
| `w` | write the goal with no depth limit (at an Exception port, and the term thrown), and ask again. |
| `+`, `-` | put a spy point on this goal's predicate, or take it off. Ask again. |
| `h`, `?` | write this list, and ask again. |

Any other input is answered with "Unknown command" and a hint to type `h`. At the end of the input (Ctrl-D in a
terminal), Mosa does what `n` does.

7. Try `g` and `s`. `g` writes the boxes that are open, and `s` at a Call port runs the goal without showing what
   happens inside, so the next stop is its Exit:

<!-- check: cmd-goals
consult('len.pl').
trace, len([a], N).

g
s
l
notrace.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([a], N).
 Call: (1) len([a],_G1) ? 
 Call: (2) len([],_G1) ? g
 (2) len([],_G1)
 (1) len([a],_G1)
 Call: (2) len([],_G1) ? s
 Exit: (2) len([],0) ? l
N = 1.
[debug] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

8. `f` fails a goal on purpose, and `r` goes back to the Call port, so that you can watch a step again:

<!-- check: cmd-fail
consult('len.pl').
trace, len([], N).
f

notrace.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([], N).
 Call: (1) len([],_G1) ? f
 Fail: (1) len([],_G1) ? 
false.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

<!-- check: cmd-retry
consult('len.pl').
trace, len([], N).

r


notrace.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([], N).
 Call: (1) len([],_G1) ? 
 Exit: (1) len([],0) ? r
 Call: (1) len([],_G1) ? 
 Exit: (1) len([],0) ? 
N = 0.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

9. `w` writes the whole goal. `a` stops the whole query, and Mosa tells you, "Execution aborted":

<!-- check: cmd-write
consult('len.pl').
trace, len([a], N).

w
a
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([a], N).
 Call: (1) len([a],_G1) ? 
 Call: (2) len([],_G1) ? w
len([],_G1)
 Call: (2) len([],_G1) ? a
% Execution aborted
[trace] ?- nodebug.
true.
```

## Choosing the ports and looking at the state

`leash(Ports)` chooses where Mosa stops. `leash([none, fail])` stops only at the Fail ports, and `leash(all)` goes
back to the five ports. The other ports are still written, only without waiting for you:

<!-- check: leash
consult('ports.pl').
leash([none, fail]).
trace, (c(X), X > 1).

leash(all).
notrace.
nodebug.
-->
```text
?- consult('ports.pl').
true.
?- leash([none, fail]).
true.
?- trace, (c(X), X > 1).
 Call: (1) c(_G1)
 Exit: (1) c(1)
 Call: (1) 1>1
 Fail: (1) 1>1 ? 
 Redo: (1) c(_G1)
 Exit: (1) c(2)
 Call: (1) 2>1
 Exit: (1) 2>1
X = 2.
[trace] ?- leash(all).
true.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

`debugging/0` writes the state: whether the debugger is on, how it moves, the spy points and the leashed ports.
`nospy(Spec)` removes one spy point, and `nospyall/0` removes all of them:

<!-- check: debugging
consult('len.pl').
spy(len/2).
debugging.
nospy(len/2).
debugging.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- spy(len/2).
% Spy point on len/2
true.
[debug] ?- debugging.
% Debug mode: on (leap)
% Spy points: len/2
% Leashed ports: call exit redo fail exception
true.
[debug] ?- nospy(len/2).
% Spy point removed from len/2
true.
[debug] ?- debugging.
% Debug mode: on (leap)
% Spy points: none
% Leashed ports: call exit redo fail exception
true.
[debug] ?- nodebug.
true.
```

## Things to know

- A `mosa` whose input is not a terminal does not read commands. It writes the port lines and goes on, so you can
  keep a record of a run:
  `mosa len.pl -g 'trace, len([a], N)' -t halt < /dev/null 2> trace.log`. On Windows, run this line in Command Prompt (cmd.exe): `type nul | mosa len.pl -g "trace, len([a], N)" -t halt 2> trace.log`.
  If its input is a terminal, it waits at the first port instead.
- The debugger predicates `trace/0`, `notrace/0`, `debug/0`, `nodebug/0`, `spy/1`, `nospy/1`, `nospyall/0`,
  `leash/1` and `debugging/0` belong to Mosa, so a file of yours cannot define a predicate with one of those names
  and arities.
- A program that `mosac build` makes shows no ports. See chapter 13.
- The other limits are listed in chapter 14, under "Known limits".

## Exercises

1. Run `len([a, b], N)` after `trace`, and press Enter at each port. How many Call lines are there, and what is
   the greatest depth?
2. Put a spy point on `len/2`, run `len([a, b], N)`, and type `l` at each stop. Which ports do you see?

### Answers

1. `len([a, b], N)` makes three calls of `len/2` and two of `is/2`, so five Call lines. The greatest depth is 3:
   the third `len/2` is inside the second, and that is inside the first.

<!-- check: exercise-trace
consult('len.pl').
trace, len([a,b], N).










notrace.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- trace, len([a,b], N).
 Call: (1) len([a,b],_G1) ? 
 Call: (2) len([b],_G1) ? 
 Call: (3) len([],_G1) ? 
 Exit: (3) len([],0) ? 
 Call: (3) _G1 is 0+1 ? 
 Exit: (3) 1 is 0+1 ? 
 Exit: (2) len([b],1) ? 
 Call: (2) _G1 is 1+1 ? 
 Exit: (2) 2 is 1+1 ? 
 Exit: (1) len([a,b],2) ? 
N = 2.
[trace] ?- notrace.
true.
[debug] ?- nodebug.
true.
```

2. Only the ports of `len/2`: three Call lines and three Exit lines. `is/2` is not a spy point, so the
   leap passes it.

<!-- check: exercise-spy
consult('len.pl').
spy(len/2).
len([a,b], N).
l
l
l
l
l
l
nospyall.
nodebug.
-->
```text
?- consult('len.pl').
true.
?- spy(len/2).
% Spy point on len/2
true.
[debug] ?- len([a,b], N).
 Call: (1) len([a,b],_G1) ? l
 Call: (2) len([b],_G1) ? l
 Call: (3) len([],_G1) ? l
 Exit: (3) len([],0) ? l
 Exit: (2) len([b],1) ? l
 Exit: (1) len([a,b],2) ? l
N = 2.
[debug] ?- nospyall.
true.
[debug] ?- nodebug.
true.
```

---

[Index](README.md) · Previous: [14. Reference](14-reference.md)
