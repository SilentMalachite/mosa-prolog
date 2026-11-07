English | [日本語](README.ja.md)

# Manual examples and `mosatest --manual`

This directory holds the fixtures of `mosatest --manual` (`good`, `bad-output`, `bad-ids`, `unmarked`, `empty`,
registered in `tests/tests.cmake`). The rules below are for whoever writes a chapter of `docs/manual/`.

```text
mosatest --manual <dir> [--mosa <path>] [--work <dir>]
```

`<dir>` holds `en/` and `ja/`. `--mosa` defaults to the `mosa` beside the running `mosatest`; `mosac` and `mosatest`
in examples are the ones beside that `mosa`. `--work` defaults to `tests/tmp/manual`. The checker prints one block per
failure (`file:line id`, the expected text, the actual text), then `manual: N checked, M skipped, F failed`. That last
line is the same in both message languages, so scripts can match it. F counts failed examples and every structural
error (a parse error, a missing edition of a file, different ids, a chapter directory that cannot be emptied). Exit
status: 0 when nothing failed, 1 on any failure, 2 on a usage error, also when `<dir>` lacks a readable `en/` or `ja/`
and when there is no `check:` example at all (nothing was checked, so nothing passed).

## Markers

Every fenced code block in `en/*.md` and `ja/*.md`, except in `README.md`, has one of these on the line right before
its opening fence. A block without one is an error, and so is a marker that is not right before a block.

| Marker | Meaning |
| --- | --- |
| `<!-- check: ID -->` | Run the block and compare its output. |
| `<!-- skip: ID -->` | Do not run it (installation, OS-specific steps, anything the checker cannot run). |
| `<!-- file: ID NAME -->` | A `prolog` block written to the chapter directory as `NAME` (a plain file name). |

`ID` has no spaces and is used once per file. Both editions of a chapter have the same file name and the same
sequence of marker kinds and ids (all three kinds counted), so `en` and `ja` show the same examples in the same order
and one edition cannot `skip:` what the other `check:`s.

A fence starts its line, indented with spaces only. A fence inside a block quote (`>`), on the same line as a list
marker (`- `, `1. `) or after a tab is an error, because the checker would not read it. An indented code block without
a fence (four spaces) is not read at all and needs no marker, so do not use one for an example.

## What `check:` runs

- A `text` block whose first line starts with `?- ` runs `mosa` with no arguments, as a reader at a terminal would.
- A `console` block is a list of commands. Each line starting with `$ ` is a command; its expected output is the lines
  up to the next `$ ` line (blank lines included). A command starts with `mosa`, `mosac`, `mosatest` or `./NAME` (a
  program in the chapter directory; `.exe` is added on Windows). Arguments split on spaces, and `'…'` and `"…"` group
  words, as in a shell. A character that a POSIX shell would treat specially is an error, so the command runs with the
  arguments a shell would give it: outside quotes ``\ $ ` | & ; < > ( ) * ? [ # ~``, and inside `"…"` ``\ $ ` ``.
  Inside `'…'` everything is literal. The commands run in order and the block stops at the first difference.
- Any other `check:` block is an error.

## Input

By default the input is the text after `?- ` of each expected line that starts with `?- `, each followed by a newline.
For anything else (a `;` for more answers, input read by `read/1`, a multi-line query), write the input in the marker:

```text
<!-- check: ID
(X = 1 ; X = 2).
;
-->
```

The lines between `<!-- check: ID` and `-->` are the input, verbatim, empty lines included. In a `console` block every
command gets this input.

## How output is compared

Every run gets `LANG=en_US.UTF-8` (`en`) or `LANG=ja_JP.UTF-8` (`ja`) and `MOSA_TRANSCRIPT=1`; `MOSA_LIB`,
`MOSA_HEAP_MAX` and `MOSA_GC_STRESS` are removed. With `MOSA_TRANSCRIPT=1`, `mosa` echoes each input line, so its
output is what a terminal shows. Standard output and standard error are merged. Before the byte-for-byte comparison,
`\r` is removed from both sides. When the run has input and its last line is not `halt.`, the final `?- ` that `mosa`
leaves waiting at the end of that input is removed too. Any other `?- ` at the end stays, because the reader sees
that prompt waiting too (for example `mosa -g G` without `-t halt` and with no input).

The exit status is not compared, because an example that shows an error may end with a non-zero status. A run that
crashes still fails, even when its expected output is empty: a signal on POSIX (an exit status above 128, as a shell
shows it), an `NTSTATUS` error on Windows (0xC0000000 or above), or no output file. After a `;` reply `mosa` prints an
empty line, which the expected text shows too. A run cannot time out, so an example that loops stops the whole check.

## Files

`file:` blocks are written to `<work>/<edition>/<chapter-stem>/NAME` when the checker reaches them, and every example
of the chapter runs with that directory as its working directory. The directory is emptied at the start of each
chapter, so a chapter cannot use files from another chapter or from an earlier run. When it cannot be emptied, the
chapter is not run and counts as a failure.
