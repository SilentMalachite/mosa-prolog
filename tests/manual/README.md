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
table that differs from Mosa, a chapter directory that cannot be emptied). Exit status: 0 when nothing failed, 1 on any
failure, 2 on a usage error, also when `<dir>` lacks a readable `en/` or `ja/`, when there is no `check:` example at
all (nothing was checked, so nothing passed), when `<--mosa> --version` does not print the version line of the running
`mosatest`, and when the `lib/` beside `--mosa` cannot be read.

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

A line `<!-- predicates: system -->` or `<!-- predicates: LIB -->` opens a section that ends at the next ATX heading at
column 0 (a line starting with `#` to `######` followed by a space, a tab or the end of the line). LIB is the stem of a
`*.pl` file in the `lib/` beside `--mosa`, except the message catalogs `messages_*`, which have no list of predicates;
any other name is an error. A comment whose text starts with `predicate` in any case (`<!-- predicate: lists -->`,
`<!-- Predicates: lists -->`) and is not exactly `predicates: NAME` on one line is an error, so a misspelled marker
does not leave a table unchecked. Outside a section, markers inside code blocks and inside a multi-line comment do not
count; a comment ends on the line that has `-->`, so `<!-->` and `<!--->` end on their own line.

A section holds only tables. Following each Markdown shape that GitHub renders in its own way missed another shape
three times in a row, so the checker accepts only lines whose rendering is unambiguous. Each line of a section must be
one of these, and any other line is an error:

1. an empty line (a line of spaces is an error);
2. a header row: it starts with `|` at column 0, has no backtick and no `<`, and has as many cells as the delimiter row
   right under it;
3. a delimiter row (`| --- | --- |`, colons for alignment allowed) that starts with `|`, right under a header row;
4. a body row, under the delimiter row or another body row: it starts with `|` at column 0, its first cell is exactly
   one code span `` `name/arity` ``, and it has no `<` outside code spans.

Cells are counted as GitHub counts them: a `|` with a backslash right before it (`\|`) does not split cells, and a last
`|` followed only by spaces does not start another cell, so `| a \| b | c |` has two cells and needs `| --- | --- |`.
So prose, indented lines, list items, block quotes, comments other than the next marker, code blocks, thematic breaks,
setext underlines and raw HTML are all errors inside a section. Write sentences above the marker and end the section
with an ATX heading. No line of a section may hold a control character (a byte below 0x20 such as a tab, a form feed,
a vertical tab or NUL). A line ends at LF, at CRLF or, as in CommonMark, at a CR without LF; inside a section a line
that ends at a CR without LF is an error too. A table ends at an empty line; a line right under a body row that is not
another body row is an error, and so is a header row with no delimiter row of as many cells right under it. In a body
row, text after the code span (`` `nope/1` (deprecated) ``), a link, bold, two code spans and inline HTML (`<code>`,
`<!-- … -->`, any `<` outside a code span) are errors. The arity is a decimal number without a leading zero
(`between/03` is an error). Write the name as it is (`` `=../2` ``, `` `\+/1` ``, `` `\==/2` ``,
`` `ひらがな_カタカナ/2` ``) or in single quotes as in Prolog (`` `','/2` ``). Inside the quotes only `''` is decoded,
as one quote; a backslash stays as it is, so write names with a backslash (`\=`, `\==`, `\+`, `=\=`) without quotes.

```markdown
`between/3` enumerates integers. The sentence goes above the marker.

<!-- predicates: between -->
| Predicate | Meaning |
| --- | --- |
| `between/3` | `between(Low, High, X)`: X is an integer from Low to High. |
```

In a chapter with any `predicates:` marker, a raw HTML tag is an error on any line, also outside the sections. A tag
here is `<` followed by a letter, `/`, `?` or `!` (`<details>`, `</div>`, `<?php`, `<!DOCTYPE html>`, an autolink
`<https://…>`; write a URL without the angle brackets), except `<!--`. GitHub renders a tag as HTML, and a tag such as
`<details>` or `<script>` hides the tables after it. A tag is fine only inside a code span, a code block or a
`<!-- … -->` comment, and the checker reads these as GitHub does:

- A comment ends at the first `-->` or `--!>`, and `<!-->` and `<!--->` are empty comments. A line that starts at
  column 0 with `<!--` begins a comment block that runs to the first line with `-->`. The whole block is raw HTML, so
  text after the comment on such a line has no code spans and no backslash escapes (`` <!-- a --> `<b>` `` is an
  error). A comment inside a paragraph ends on its own line.
- Code spans pair as in CommonMark. Where the pairing can change, a tag inside a code span is an error too, from there
  to the next empty line: after a backtick run that does not close on its line, after a `<!--` that does not end on
  its line, and after a link destination, title or label (`[a](…)`, `[a][…]`) that holds a backtick or may go on to
  the next line, because a link takes its backticks. A line with a `|` is also read cell by cell, as GFM splits a
  table row into cells first.
- A `<!--` after indentation, a `>` or a list marker may start a comment block or may be plain text, so that line and
  the lines up to the next `-->` are read both ways, and a tag that either reading finds is an error.
- The lines of a code block are skipped, except a line indented less than the opening fence and every line after it
  in the block: GitHub ends a list item, and the fence in it, at such a line.

`\<` outside a comment block is a plain `<`, and `<` followed by a space (`X < Y`) is not a tag. In a body row of a
section, a link destination, title or label that holds a backtick also ends the code spans of its cell, so a `<` after
it is an error.

The entries of every section with the same name in one file are taken together, so a chapter can split a list under
several headings, and an entry listed twice for that name is an error. They are compared with Mosa in both directions:
an entry that Mosa lacks and a predicate that the tables lack both fail, and the failure names both lists, written on
the line of the first marker. `system` means the built-in predicates, those for which `predicate_property(H,
built_in)` holds (and `:/2`, which `predicate_property/2` reads as a module qualification), without the internal
helpers whose names start with `$`. The list is the one built into `mosatest`, which is why `--mosa` must print the
same version. `LIB` means the export list of `lib/LIB.pl` beside `--mosa` (beside the file it points to, when it is a
symbolic link, as `mosa` itself looks there), loaded by its absolute path in a fresh machine; `$MOSA_LIB` is ignored,
as it is for the `mosa` that runs the examples. A library that is present but cannot be loaded (a syntax error, no
`module/2`, a module with another name) fails with its name, its path and the load error. Directives such as
`use_module/1` and `dynamic/1` are not built-in predicates, so list them in a table without a marker. Both editions of
a chapter have the same sequence of `predicates:` markers.

With `--all-tables`, each edition must also have a section for `system` and for every library in that `lib/`; the
missing names fail as one error per edition. Use it for the full manual, whose reference chapter lists them all.
