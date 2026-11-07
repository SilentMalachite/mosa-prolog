English | [日本語](README.ja.md)

# Manual examples and `mosatest --manual`

This directory holds the fixtures of `mosatest --manual` (`good`, `bad-output`, `bad-ids`, `unmarked`, `empty`,
`bad-preds`, `bad-lib`, registered in `tests/tests.cmake`). The rules below are for whoever writes a chapter of
`docs/manual/`.

```text
mosatest --manual <dir> [--mosa <path>] [--work <dir>] [--all-tables]
```

`<dir>` holds `en/` and `ja/`. `--mosa` defaults to the `mosa` beside the running `mosatest`; `mosac` and `mosatest`
in examples are the ones beside that `mosa`. `--work` defaults to `tests/tmp/manual`. The checker prints one block per
failure (`file:line id`, the expected text, the actual text), then `manual: N checked, M skipped, F failed`. That last
line is the same in both message languages, so scripts can match it. N and M count examples. F counts failed examples
and every structural error (a parse error, a missing edition of a file, different ids or predicate tables, a predicate
table that differs from Mosa, a chapter directory that cannot be emptied). Exit
status: 0 when nothing failed, 1 on any failure, 2 on a usage error, also when `<dir>` lacks a readable `en/` or `ja/`,
when there is no `check:` example at all (nothing was checked, so nothing passed), when `<--mosa> --version` does not
print the version line of the running `mosatest`, and when the `lib/` beside `--mosa` cannot be read.

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

## Predicate tables

A line `<!-- predicates: system -->` or `<!-- predicates: LIB -->` opens a section that ends at the next ATX heading
(a line starting with `#` to `######`, indented by at most three spaces). A setext heading (a line underlined with
`===` or `---`) does not end a section, so follow a section with an ATX heading. LIB is the stem of a `*.pl` file in
the `lib/` beside `--mosa`, except the message catalogs `messages_*`, which have no list of predicates; any other name
is an error. A comment whose text starts with `predicate` in any case (`<!-- predicate: lists -->`,
`<!-- Predicates: lists -->`) and is not exactly `predicates: NAME` on one line is an error, so a misspelled marker
does not leave a table unchecked.

A table in a section is a header row, a `| --- |` row and body rows. Every line after the `| --- |` row is a body row
until a blank line, a heading, a comment or a code block, as GitHub renders a line right after a table as one more row.
Every body row starts with `|`, and its first cell is exactly one code span `` `name/arity` ``. Anything else is an
error, not a row to skip: text after the code span (`` `nope/1` (deprecated) ``), a link, bold, two code spans, a row
without the leading `|`, or a line of prose right under the table. A line with a `|` that is not followed by a
`| --- |` row is an error when it starts with `|`, and so is a line that starts with a code span and has a `|`. The
header row is not compared, so a header whose first cell starts with a code span is an error, with or without the
leading `|`. Inside a section, a line that starts with `>` and has a `|` (a table in a block quote) is an error too. The
arity is a decimal number without a leading zero (`between/03` is an error). Write the name as it is (`` `=../2` ``,
`` `\+/1` ``, `` `\==/2` ``, `` `ひらがな_カタカナ/2` ``) or in single quotes as in Prolog (`` `','/2` ``). Inside the
quotes only `''` is decoded, as one quote; a backslash stays as it is, so write names with a backslash (`\=`, `\==`,
`\+`, `=\=`) without quotes.

Lines that a reader does not see as a table are not read: lines inside code blocks, lines inside a multi-line
`<!-- … -->` comment, and lines indented by four or more spaces outside a list item (an indented code block). As in
GitHub, a comment ends on the line that has `-->`, even with text after it, and the lines after that are read again.
A table inside a list item may be indented. A thematic break (`* * *`, `- - -`, `___`) is not a list item, so lines
indented under it are an indented code block.

```markdown
<!-- predicates: between -->
| Predicate | Meaning |
| --- | --- |
| `between/3` | `between(Low, High, X)`: X is an integer from Low to High. |
```

The entries of every section with the same name in one file are taken together, so a chapter can split a list under
several headings, and an entry listed twice for that name is an error. They are compared with Mosa in both directions:
an entry that Mosa lacks and a predicate that the tables lack both fail, and the failure names both lists, written on
the line of the first marker. `system` means the built-in predicates, those for which `predicate_property(H,
built_in)` holds (and `:/2`, which `predicate_property/2` reads as a module qualification), without the internal
helpers whose names start with `$`. The list is the one built into `mosatest`, which is why `--mosa` must print the
same version. `LIB` means the export list of `lib/LIB.pl` beside `--mosa` (beside the file it points to, when it is a
symbolic link, as `mosa` itself looks there), loaded by its absolute path in a fresh
machine; `$MOSA_LIB` is ignored, as it is for the `mosa` that runs the examples. A library that is present but cannot
be loaded (a syntax error, no `module/2`, a module with another name) fails with its name, its path and the load error.
Directives such as `use_module/1` and `dynamic/1` are not built-in predicates, so list them in a table without a
marker. Both editions of a chapter have the same sequence of `predicates:` markers.

With `--all-tables`, each edition must also have a section for `system` and for every library in that `lib/`; the
missing names fail as one error per edition. Use it for the full manual, whose reference chapter lists them all.
