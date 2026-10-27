English | [日本語](0005-streams.ja.md)

Status: Approved (2026-10-25)

## Context

I/O predicates advance the same cursor for files, standard I/O and memory.
If the location is not decided first, open, get and write will each have their own shape.
SPEC section 6 says abstract class. The public header mosa.hpp is not extended until phase 7.

## Decision

Stream is a vector of values in Impl. It is not an abstract class. The kinds are File, Standard, Memory. The data is Text or Binary. The modes are Read, Write, Append.
It holds the bytes, the cursor, a closed flag, reposition and aliases. The default encoding is utf8. The default type is text. It is placed right after operators.
The predicate names are open_string/2 and with_output_to/2. A text newline is a single U+000A for both files and memory, on every OS. The aliases at startup are user_input, user_output, user_error.

## Rejected alternatives

The alternative name open_memory/2. It would not match the name in SPEC section 8.
Leaving newlines to C++ text mode. On Windows they become CR LF, and the test bytes would differ by OS.
Adding stream operations to mosa.hpp. The public API stays create, consult, query, next, halt.

## Consequences

Stream code may be written once this ADR is approved.
Files are opened in binary mode. consult already uses ios::binary. nl writes the single byte 0x0A.
The goal of with_output_to/2 runs in the current context. The module wrapping is not added until phase 4.
