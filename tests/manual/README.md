English | [日本語](README.ja.md)

# Manual examples and `mosatest --manual`

This directory holds the fixtures of `mosatest --manual` (`good`, `bad-output`, `bad-ids`, `unmarked`, registered in
`tests/tests.cmake`). The rules below are for whoever writes a chapter of `docs/manual/`.

```text
mosatest --manual <dir> [--mosa <path>] [--work <dir>]
```

`<dir>` holds `en/` and `ja/`. `--mosa` defaults to the `mosa` beside the running `mosatest`; `mosac` and `mosatest`
in examples are the ones beside that `mosa`. `--work` defaults to `tests/tmp/manual`. The checker prints one block per
failure (`file:line id`, the expected text, the actual text), then `manual: N checked, M skipped, F failed`. Exit
status: 0 when nothing failed, 1 on any failure, 2 on a usage error (also when `<dir>` lacks `en/` or `ja/`).

## Markers

Every fenced code block in `en/*.md` and `ja/*.md`, except in `README.md`, has one of these on the line right before
its opening fence. A block without one is an error, and so is a marker that is not right before a block.

| Marker | Meaning |
| --- | --- |
| `<!-- check: ID -->` | Run the block and compare its output. |
| `<!-- skip: ID -->` | Do not run it (installation, OS-specific steps, anything the checker cannot run). |
| `<!-- file: ID NAME -->` | A `prolog` block written to the chapter directory as `NAME` (a plain file name). |

`ID` has no spaces and is used once per file. Both editions of a chapter have the same file name and the same
sequence of ids, all three kinds counted, so `en` and `ja` show the same examples in the same order.

## What `check:` runs

- A `text` block whose first line starts with `?- ` runs `mosa` with no arguments, as a reader at a terminal would.
- A `console` block is a list of commands. Each line starting with `$ ` is a command; its expected output is the lines
  up to the next `$ ` line (blank lines included). A command starts with `mosa`, `mosac`, `mosatest` or `./NAME` (a
  program in the chapter directory; `.exe` is added on Windows). Arguments split on spaces, and `'…'` and `"…"` group
  words, as in a shell; there is nothing else of a shell (no pipes, redirection, variables or `cd`). The commands run
  in order and the block stops at the first difference.
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
`\r` is removed from both sides, and the final `?- ` that `mosa` leaves waiting at the end of the input is removed
(it is absent when the input ends with `halt.`). The exit status is not compared. After a `;` reply `mosa` prints an
empty line, which the expected text shows too. A run cannot time out, so an example that loops stops the whole check.

## Files

`file:` blocks are written to `<work>/<edition>/<chapter-stem>/NAME` when the checker reaches them, and every example
of the chapter runs with that directory as its working directory. The directory is emptied at the start of each
chapter, so a chapter cannot use files from another chapter or from an earlier run.
