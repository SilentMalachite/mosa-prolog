English | [日本語](0023-mosac-dist.ja.md)

Status: Approved (2026-11-10; the owner asked for `mosac dist` to be built without further questions, which counts as approval). SPEC section 6 gives `mosac dist app.pl -o dist/` as "build + distribution package (executable, LICENSE, README)" in zip / tar.gz; this ADR fixes what goes in and how the archive is written.

## Context

The user's machine needs only a C++ compiler (SPEC section 6; CMake is optional), so mosac cannot pack with `cmake -E tar` as Mosa's own release does (`cmake/package.cmake`), and the system `tar` differs by OS (GNU tar writes no zip; Windows has `tar.exe` only from 10 1803).
The executable links libmosa and the translated library units statically, and they are Apache-2.0: section 4 of the license requires a copy of the license and the NOTICE to go with a redistributed binary.
The executable of `hello.pl` is 1.3 MB on macOS, and a program's name can be Japanese (`こんにちは.pl`).

## Decision

`mosac dist app.pl [-o DIR]` takes the options of `mosac build`, builds the same way, and writes `DIR/<stem>-<os>-<arch>.tar.gz` on macOS and Linux and `DIR/<stem>-<os>-<arch>.zip` on Windows (`DIR` defaults to `dist`, `<os>-<arch>` as in the runtime key). The archive holds one folder `<stem>-<os>-<arch>/` with the executable, `LICENSE` and `NOTICE` of Mosa (their text is put into mosac when mosac is built) and `README.txt` (the catalog key `mosac_dist_readme` in English, then in Japanese). mosac prints the archive's path on standard output.
mosac writes both formats itself in standard C++ (`src/codegen/archive.cpp`), with no compression: ustar entries, with a PAX `path` record before a name that is not ASCII or does not fit the 100-byte field, in a gzip stream of stored deflate blocks; zip entries stored with the UTF-8 name flag and Unix modes. The executable is mode 0755 and the other files 0644; the time of every entry is the packing time.
An archive of the same name is replaced by a new file written beside it and then renamed, so a failed write leaves the old one. A `DIR` that is an existing file stops with the cannot-write message.

## Rejected alternatives

Run `tar`, `cmake -E tar` or PowerShell `Compress-Archive`: CMake is optional, the tools differ by OS and version, and passing non-ASCII paths to child processes on Windows has needed special routes before (7779233).
Deflate compression: it adds code that the tests have to prove for a 1.3 MB file; a later version can add it inside the same formats. Reading `LICENSE` and `NOTICE` next to mosac at run time: the build tree and the installed tree keep them in different places, and a missing file would stop `dist`.
Put the program's own `README` and `LICENSE` in the archive: their names collide with the runtime's files, and mosac cannot tell which files belong to a program; the user can add files to the archive afterwards.

## Consequences

An archive is about as large as its files. The same program packed twice gives archives that differ only in their times.
Tests: `archive_test` checks the bytes of both formats (CRC-32, tar checksums, PAX records, the gzip trailer, zip directories), and `mosac_dist` unpacks the archive with `cmake -E tar`, runs the executable and compares the three text files, also for a Japanese file name, on the 3 OSes of CI.
The message `mosac_dist_unavailable` goes away; SPEC section 6, manual chapter 13, the usage text, the README status and the CHANGELOG of 0.1.1 describe `dist`.
