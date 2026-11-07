English | [日本語](../ja/README.md)

# Mosa Prolog manual

This manual is for Mosa Prolog 0.1.0. It is written for people who have never used Prolog. It starts with
installing Mosa and goes on, one small step at a time, to writing programs and building executables.

## How to read this manual

- Read the chapters in order. Each chapter uses only what the chapters before it have shown.
- Each chapter starts with one sentence that says what you will do. Then come numbered steps that you type,
  a "What happened" section that explains them, and one or two exercises with their answers.
- A line that starts with `?- ` is a query: you type the text after `?- `, and the lines under it are what Mosa
  prints. A line that starts with `$ ` is a command for your shell (Terminal on macOS and Linux, PowerShell on
  Windows): you type the text after `$ `. Lines only for PowerShell are shown without a prompt: type each of them
  whole. A `$` at the start of such a line, as in `$bin = …`, is part of the command, so type it too.
- Every example is run by `mosatest --manual` against Mosa 0.1.0, and the check fails if the output differs by a
  single character. So what you see here is what Mosa prints. Only installation steps and other things that depend
  on your machine are not run.

## Supported platforms

- macOS 14 or later on Apple Silicon.
- Linux on x86_64 with the glibc of Ubuntu 24.04 or later.
- Windows 10 22H2 or later and Windows 11, on x64, with Smart App Control off.

The packages are not signed. [Chapter 2](02-install.md) shows how to start Mosa anyway on macOS and Windows.

## Chapters

Chapters 1 to 8 are written. The others are listed without a link until they are written.

1. [Introduction](01-introduction.md)
2. [Getting and starting Mosa](02-install.md)
3. [Your first query](03-first-query.md)
4. [Facts and rules](04-facts-rules.md)
5. [Terms and unification](05-terms-unification.md)
6. [Arithmetic](06-arithmetic.md)
7. [Lists and recursion](07-lists-recursion.md)
8. [Control](08-control.md)
9. Errors and exceptions
10. Input and output
11. Writing in Japanese
12. Modules and libraries
13. Building executables with `mosac`
14. Reference
