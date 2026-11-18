English | [日本語](0007-messages.ja.md)

Status: Approved (2026-10-28). Amendment 1 (2026-11-07; approved by the owner's choice on 2026-11-07): without `LANG`, Windows follows the display language. Amendment 2 (2026-11-13; records the phase 10 design approved by the owner's decision A on 2026-11-12): the key `integer_overflow` is removed. Amendment 3 (2026-11-19; the owner chose B, to fix it in phase 14, after the adversarial review of PR #4; the method is a default A in open-items): a render never loads anything on the user's machine

## Context

SPEC Section 7 puts message text in `lib/messages_ja.pl` and `lib/messages_en.pl`, chosen by the flag `message_language` (default from `LANG`); today `session.cpp` and `main_mosa.cpp` hold the Japanese text.
`read_pred.cpp` puts a Japanese sentence with line, column and token into the Formal, so the Formal changes with the language and the place is written twice.
SPEC Section 4 wants the offending token in the syntax error message, Section 7 fixes Extra as `stream(S, Line, LinePos, CharNo)`, and 7.12.2 j makes the Formal `syntax_error(Atom)`, so the token has no slot yet.

## Decision

Modules `messages_ja` and `messages_en` each define `message(Key, Format)` (Format a `format/2` atom; keys `goal_raised`, `heap_limit`, `help_japanese_variables` now); `render_message` loads `library(messages_<lang>)` on first use, not imported into `user`, and runs `format/3` on the same machine inside a catch.
Flag `message_language` takes `ja` or `en`, other atoms give `domain_error(flag_value, message_language+V)`; the default is `ja` when `LANG` (read in `src/platform/`) starts with `ja`, else `en`. A missing catalog, missing key or failing render prints `Key: ` then each argument by `writeq`, separated by spaces.
`syntax_error(Key)` with one English atom per reason (`invalid_utf8`, `illegal_character`, `unterminated_quoted`, `unterminated_comment`, `bad_escape`, `integer_overflow`, `float_overflow` (added 2026-11-02, review M2, iso-notes 6.4.5), `unexpected_token`, `max_arity`, `incomplete_term`); Extra is `syntax(stream(S, Line, LinePos, CharNo), Token)`, Token the offending token as an atom, `''` at end of input (owner chose A over `stream/5`).

## Rejected alternatives

A C++ table of strings per language. SPEC Section 7 forbids message text in C++, and users could not add a language without rebuilding.
One module `messages` with `message(Lang, Key, Format)`. Both languages would be loaded every time, and a third language would edit a shared file.
Keeping the sentence in the Formal. `catch/3` patterns would depend on the language and the column would be written twice.

## Consequences

`goal_raised` uses `~q`, so the shown exception is quoted as now (variables written by `numbervars` as `writeq/1` does). `main_mosa.cpp`, `warn_resource` in `session.cpp`, usage texts and load diagnostics (made while the machine is loading) have no machine that can run a query, so they render on a fresh machine marked `render_only`, whose own diagnostics use the fallback.
CMake runs `session_test` with `LANG=ja_JP.UTF-8` so the existing Japanese checks do not depend on the machine, and once more with `MOSA_LIB` at `tests/tmp/messages_empty/`, whose `messages_en.pl` (written at configure time) has no keys; `MOSA_LIB` is searched before the exe `lib/`, so that run checks the fallback.
`read_syntax`, `read_incomplete` and `read_syntax_stream` in `tests/pl/read.pl` change to the Key and Extra above in the Task 12 commit. SPEC Section 7 names this Extra since approval. No code is written before approval.

## Amendment 1 (2026-11-07; approved by the owner's choice on 2026-11-07)

Context: on Windows `LANG` is normally unset, so `message_language` was always `en` even on a Japanese Windows.
Decision: a non-empty `LANG` decides as before (`ja` prefix gives Japanese); when `LANG` is absent or empty, Windows uses the display language (`GetUserDefaultUILanguage`, primary language Japanese gives Japanese); other systems give English. The decision is `message_language_from(lang, ui_japanese)`, and the OS call lives in `src/platform/ui_language.cpp`.
Rejected: reading the console code page or the locale name (the user may change them for other reasons); keeping English until `LANG` is set (the default would not match the user's language).
Consequences: SPEC Section 7 says so; a set `LANG` still wins, so `LANG=en` gives English on a Japanese Windows; tests pin `LANG` so they do not depend on the machine.

## Amendment 2 (2026-11-13, phase 10; records the phase 10 design approved by the owner's decision A on 2026-11-12)

The phase 10 design (section 4) removed the key `integer_overflow`: an integer token of any length reads as a number, so no syntax error has that reason, and `messages_en.pl` and `messages_ja.pl` no longer hold it; the other keys and Extra are unchanged.

## Amendment 3 (2026-11-19; the owner chose B, to fix it in phase 14, after the adversarial review of PR #4; the method is a default A in open-items)

Context: loading the catalog on the user's machine also loads `library(format)` and, through it, `library(lists)`, so after the first warning `lists:append/3`, `spy(lists:foo/1)` and `current_module(lists)` act as if the program had loaded `lists`; before it they give `existence_error(module, lists)` or fail.
Decision: `render_message` on a machine that is not `render_only` copies the arguments (each on its own, atoms by name, with its shared and cyclic parts kept), the operator table and the `japanese_punctuation` flag to a fresh `render_only` machine with the same heap limit (so `--heap-max` bounds a render too) and renders there, so the text is the same as before and the user's machine loads nothing.
Rejected: loading the catalogs at start-up (every `Machine` would load four files, and `lists` would exist before the program loads it); marking the modules a render loads as hidden (every module lookup would need two meanings); a renderer kept in `Impl` (`internal.hpp` must be split first, and a fresh machine costs about 1 ms per message).
Consequences: the transcript `transcript_debug_spy_after_warning` checks that after a warning `lists`, `format` and both catalogs are still unknown, and the warnings it writes keep their text; `message_ops` checks that a render under `--heap-max 4` falls back to the key form as before; ctest took 115.3 s against 114.2 s before, and CI's GC-stress selection 1808 s against 1805 s.
