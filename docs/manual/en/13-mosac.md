English | [日本語](../ja/13-mosac.md)

# 13. Building executables with `mosac`

In this chapter you install a C++ compiler, turn a Prolog program into an executable with `mosac`, and run it.

An executable is a file that your computer runs as a program, as it runs `mosa`. `mosac` translates your Prolog
program into C++, another programming language, and then calls a C++ compiler: a program that turns C++ into an
executable. The executable runs without `mosa` and without your `.pl` files.

## Steps

1. Install a C++ compiler. You do this only once. On macOS, install the Xcode Command Line Tools: type this line,
   click Install in the dialog that appears, and agree to the license. If the command says that the tools are
   already installed, you have nothing to do.

<!-- skip: compiler-macos -->
```console
$ xcode-select --install
```

On Linux (Ubuntu), install `g++`. `apt update` first brings the list of packages of Ubuntu up to date. `sudo` asks
for your password, and `apt install` asks you to confirm with `Y`:

<!-- skip: compiler-linux -->
```console
$ sudo apt update && sudo apt install g++
```

On Windows, download the
[installer of Build Tools for Visual Studio 2022](https://aka.ms/vs/17/release/vs_BuildTools.exe), and run it. In
the installer, choose the workload "Desktop development with C++", and click Install. You need no special window
afterwards: `mosac` finds the compiler from an ordinary PowerShell window. Close PowerShell and open a new one when
the installer is done. Mosa 0.1.0 is tested with Visual Studio 2022. If a newer Visual Studio is also installed,
`mosac` uses the newest one, which is not tested.

2. Check that `mosac` finds the compiler. `mosac toolchain` prints the compiler and its version, CMake (a build tool
   that `mosac` uses when it is installed, and does without when it is not), and the folder of the runtime library,
   the part of Mosa that every executable carries. These lines name folders on your computer, so they differ from
   machine to machine. If no compiler is found, the first line is `mosac: no C++20 compiler was found`, and the next
   line says what to install. On Windows, `mosac` also does without CMake when the path of the runtime library, or
   of the folder `.mosac-cache` that `mosac build` makes (step 4), has a character outside ASCII, such as a Japanese
   one: CMake's build for the compiler of Visual Studio can lose such characters, so `mosac` calls the compiler
   directly.

<!-- skip: toolchain -->
```console
$ mosac toolchain
```

3. Move into the folder `prolog` with the `cd` line of chapter 4. Create a file `hello.pl` in that folder with
   these lines. The first line is a directive (chapter 11). `initialization(main, main)` tells Mosa to run the goal
   `main` once the file is loaded, and to end the program when `main` is done. The rule says what `main` does: print
   `hello` and start a new line.

<!-- file: hello-file hello.pl -->
```prolog
:- initialization(main, main).

main :- write(hello), nl.
```

4. Build the executable, and run it. `mosac build hello.pl -o hello` prints nothing when it succeeds, and takes a
   second or two. `-o hello` names the executable: `hello`, or `hello.exe` on Windows. `./hello` runs the program
   `hello` in this folder. On a Linux with `clang++` and no `g++`, `mosac build` stops with `there is no runtime
   library for this compiler`; [What happened](#what-happened) says what to do.

<!-- check: build -->
```console
$ mosac build hello.pl -o hello
$ ./hello
hello
```

Type the `./`. Without it, your shell looks for `hello` only in the folders of your PATH (chapter 2), not in the
current folder, and does not find it: zsh, the default shell of macOS, prints `zsh: command not found: hello`, and
bash prints something like `bash: hello: command not found`. On Ubuntu, bash may also suggest installing a package
named `hello`, which you do not need. In PowerShell on Windows, type `.\hello` (`./hello` works there too). In the
Command Prompt (cmd), `hello` alone works.

`mosac build` also made a folder `.mosac-cache` in the folder `prolog`. It holds the C++ files and the other files
that `mosac` made on the way, so that building the same program again is fast. Its name starts with a period, so
`ls` and the Finder on macOS do not show it (`ls -a` does). You can delete it whenever you like: your executable
still runs, and the next `mosac build` makes the folder again, taking a second or two again.

5. Run the program without building it yourself. `mosa hello.pl` loads the file, and the directive runs `main` and
   ends Mosa, so you see no `?-` prompt. `mosac run hello.pl` builds the program in `.mosac-cache`, as
   `mosac build` does, runs it, and leaves no executable in the folder `prolog`.

<!-- check: run -->
```console
$ mosa hello.pl
hello
$ mosac run hello.pl
hello
```

6. Build a program from several files. Create the file `family_tree.pl` of chapter 12 in the folder `prolog`, if
   it is not there any more:

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

Then create a file `grandchildren.pl` in the folder `prolog` with these lines. Its `main` prints Tom's
grandchildren, as `show_grandchildren/1` of chapter 12 did.

<!-- file: grandchildren-file grandchildren.pl -->
```prolog
:- initialization(main, main).
:- use_module(family_tree).
:- use_module(library(format)).

main :- forall(grandparent(tom, Z), format('~w is a grandchild of tom~n', [Z])).
```

7. Build it and run it. `mosac` puts every file that the program loads with `use_module/1` into the executable, the
   libraries included.

<!-- check: build-modules -->
```console
$ mosac build grandchildren.pl -o grandchildren
$ ./grandchildren
ann is a grandchild of tom
pat is a grandchild of tom
```

8. See what a program does when `main` raises an error. Create a file `divide.pl` in the folder `prolog` with these
   lines, build it and run it. The program prints the same warning as the `?-` prompt (chapter 9), and ends.

<!-- file: divide-file divide.pl -->
```prolog
:- initialization(main, main).

main :- X is 10 / 0, write(X), nl.
```

<!-- check: build-error -->
```console
$ mosac build divide.pl -o divide
$ ./divide
Warning: goal raised exception: error(evaluation_error(zero_divisor),context((is)/2,_G1))
```

## What happened

`mosac build` read your program as `mosa` does, translated it into C++ in `.mosac-cache`, and called the compiler.
The compiler made the executable and joined it with the runtime library, which does for the executable what `mosa`
does for you at the prompt: it runs the goals, unifies, backtracks and prints. That is why the executable needs
neither `mosa` nor the `.pl` files. If your file has a mistake, `mosac build` names the line and the column as
`mosa` does (chapter 4), makes no executable, and you are back in your shell.

`initialization(main, main)` makes the file a program. When `main` succeeds, the program ends. When `main` fails,
the program ends without printing anything. When `main` raises an exception that nothing catches, the program prints
the warning and ends, as in step 8. Without the directive, nothing calls `main`: the executable starts and ends at
once, and prints nothing.

An executable picks the language of its messages as `mosa` does (chapter 11): from `LANG`, or, when `LANG` is not
set, from the Windows display language. So the warning of step 8 is in Japanese when `LANG` starts with `ja`.

`mosac` looks for a compiler in the usual places: `clang++` on macOS, `g++` and then `clang++` on Linux, and the
compiler of Visual Studio on Windows. After you change a file, run `mosac build` again, and it takes a second or two
again. For a program that has not changed, it reuses what is in `.mosac-cache` and is done almost at once.

The Linux package carries the runtime library for `g++` only. If your Linux has `clang++` and no `g++`, `mosac`
finds `clang++`, and `mosac build` stops with two lines like these, which name the folder where you unpacked Mosa:

<!-- skip: clang-linux -->
```console
$ mosac build hello.pl -o hello
mosac: there is no runtime library for this compiler: /home/<name>/mosa-0.1.0-linux-x86_64/lib/mosa/linux-x86_64-clang
Use the compiler mosac was built with, or run mosac build --runtime-source to build it from the bundled source (the result stays in .mosac-cache)
```

The simplest way out is to install `g++` as in step 1. Otherwise, add `--runtime-source` to each `mosac build` and
`mosac run`: `mosac` then builds the runtime library for `clang++` from the source that the package carries. This
needs CMake 3.25 or later (on Ubuntu, `sudo apt update && sudo apt install cmake`). The first such build takes a
minute or more and, like any build, prints nothing when it succeeds. It keeps the runtime library in `.mosac-cache`,
so the next builds in the same folder are fast again, until you delete `.mosac-cache`, after which the next build
takes a minute or more again; a build with `--clean` deletes it first, so that build itself takes that long:

<!-- skip: runtime-source -->
```console
$ mosac build hello.pl -o hello --runtime-source
$ ./hello
hello
```

`mosac dist`, which packs a program for other people to install, is not part of Mosa 0.1.0.

## Exercises

1. Write a program `squares.pl` in the folder `prolog` that prints the squares of 1, 2 and 3, one on each line. Use
   `forall/2`, `member/2` and `format/2` (chapter 12). Build it and run it.
2. Save `main :- write('Hello, world'), nl.` alone, without the directive, as `nomain.pl` in the folder `prolog`.
   Build it and run it. What does it print?

### Answers

1. `forall/2` runs the two goals in parentheses for each element: `is/2` computes the square, and `format/2` prints
   it:

<!-- file: squares-file squares.pl -->
```prolog
:- initialization(main, main).
:- use_module(library(lists)).
:- use_module(library(format)).

main :- forall(member(X, [1, 2, 3]), (Y is X * X, format('~w~n', [Y]))).
```

<!-- check: exercise-squares -->
```console
$ mosac build squares.pl -o squares
$ ./squares
1
4
9
```

2. Nothing. The build succeeds, but nothing calls `main`, so the program ends at once:

<!-- file: nodirective-file nomain.pl -->
```prolog
main :- write('Hello, world'), nl.
```

<!-- check: exercise-nodirective -->
```console
$ mosac build nomain.pl -o nomain
$ ./nomain
```

---

[Index](README.md) · Previous: [12. Modules and libraries](12-modules-libraries.md) · Next:
[14. Reference](14-reference.md)
