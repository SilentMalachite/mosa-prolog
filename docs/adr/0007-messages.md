English | [日本語](0007-messages.ja.md)

Status: Approved (2026-10-28)

## Context

SPEC Section 7 puts message text in `lib/messages_ja.pl` and `lib/messages_en.pl`, chosen by the flag `message_language` (default from `LANG`); today `session.cpp` and `main_mosa.cpp` hold the Japanese text.
`read_pred.cpp` puts a Japanese sentence with line, column and token into the Formal, so the Formal changes with the language and the place is written twice.
SPEC Section 4 wants the offending token in the syntax error message, Section 7 fixes Extra as `stream(S, Line, LinePos, CharNo)`, and 7.12.2 j makes the Formal `syntax_error(Atom)`, so the token has no slot yet.

## Decision

Modules `messages_ja` and `messages_en` each define `message(Key, Format)` (Format a `format/2` atom; keys `goal_raised`, `heap_limit`, `help_japanese_variables` now); `render_message` loads `library(messages_<lang>)` on first use, not imported into `user`, and runs `format/3` on the same machine inside a catch.
Flag `message_language` takes `ja` or `en`, other atoms give `domain_error(flag_value, message_language+V)`; the default is `ja` when `LANG` (read in `src/platform/`) starts with `ja`, else `en`. A missing catalog, missing key or failing render prints `Key: ` then each argument by `writeq`, separated by spaces.
`syntax_error(Key)` with one English atom per reason (`invalid_utf8`, `illegal_character`, `unterminated_quoted`, `unterminated_comment`, `bad_escape`, `integer_overflow`, `unexpected_token`, `max_arity`, `incomplete_term`); Extra is `syntax(stream(S, Line, LinePos, CharNo), Token)`, Token the offending token as an atom, `''` at end of input (owner chose A over `stream/5`).

## Rejected alternatives

A C++ table of strings per language. SPEC Section 7 forbids message text in C++, and users could not add a language without rebuilding.
One module `messages` with `message(Lang, Key, Format)`. Both languages would be loaded every time, and a third language would edit a shared file.
Keeping the sentence in the Formal. `catch/3` patterns would depend on the language and the column would be written twice.

## Consequences

`goal_raised` uses `~q`, so the shown exception is quoted as now (variables written by `numbervars` as `writeq/1` does). `main_mosa.cpp` and `warn_resource` in `session.cpp` have no usable heap when they catch `ResourceLimit`, so they create a fresh machine to render `heap_limit` and falls back as above if that fails.
CMake runs `session_test` with `LANG=ja_JP.UTF-8` so the existing Japanese checks do not depend on the machine, and once more with `MOSA_LIB` at `tests/tmp/messages_empty/`, whose `messages_en.pl` (written at configure time) has no keys; `MOSA_LIB` is searched before the exe `lib/`, so that run checks the fallback.
`read_syntax`, `read_incomplete` and `read_syntax_stream` in `tests/pl/read.pl` change to the Key and Extra above in the Task 12 commit. SPEC Section 7 names this Extra since approval. No code is written before approval.
