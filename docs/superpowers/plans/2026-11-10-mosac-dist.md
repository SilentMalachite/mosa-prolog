# mosac dist Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `mosac dist app.pl [-o DIR]` builds the program and writes `DIR/<stem>-<os>-<arch>.tar.gz` (macOS, Linux) or `.zip` (Windows) holding the executable, Mosa's `LICENSE` and `NOTICE`, and a `README.txt`.

**Architecture:** `src/codegen/archive.cpp` writes ustar-in-gzip (stored deflate blocks) and stored zip from a list of entries, in standard C++ only. `src/codegen/dist.cpp` puts the entries together (the license texts come from a header CMake generates from `LICENSE` and `NOTICE`) and writes the archive; `src/main_mosac.cpp` gets `run_dist`.

**Tech Stack:** C++20, CMake 3.25, the message catalogs `lib/messages_{en,ja}.pl`, CTest scripts in `tests/mosac/`.

**Spec:** `docs/adr/0023-mosac-dist.md` (and SPEC section 6, the row `mosac dist`).

## Global Constraints

- Only the C++20 standard library; no new dependency (AGENTS.md). No `#ifdef` outside `src/platform/`; pick zip or tar.gz with `mosa::host_os()`.
- No raw `new`/`delete`, `reinterpret_cast`, C arrays or macros; functions ≤ 60 lines, files ≤ 800 lines; warnings are errors (`-Wall -Wextra -Wpedantic`, `/W4`).
- Identifiers in English, comments in Japanese explaining why; commit messages one line in Japanese, `[phase7] …（how it was verified）`.
- User-facing text only through catalog keys, the same keys in `messages_en.pl` and `messages_ja.pl`.
- Docs: change the English file and its `.ja.md` in the same commit.

## Review Focus

1. A Japanese program name (`こんにちは.pl`) must give `こんにちは-<os>-<arch>/こんにちは` after unpacking, on all 3 OSes: covered by `dist_japanese` (Task 2).
2. `-o dist/` with a trailing slash, as SPEC writes it, must work: `dist_hello` uses `-o out/` (Task 2).
3. Packing twice must replace the archive and leave no temporary file: `dist_replace` (Task 2).
4. A file larger than 65,535 bytes must split into several stored deflate blocks: `tar_gz_splits_blocks` (Task 1).
5. A time before 1980 (zip cannot hold it) must not wrap: `zip_dos_time_floor` (Task 1).

---

### Task 1: Archive writer

**Files:**
- Create: `src/codegen/archive.hpp`, `src/codegen/archive.cpp`
- Modify: `CMakeLists.txt` (add `src/codegen/archive.cpp` to the `target_sources(mosa_lib …)` codegen list)
- Test: `tests/cpp/archive_test.cpp`, registered in `tests/tests.cmake` as `archive` (links `mosa_lib`, include `tests/cpp` and `src`)

**Interfaces:**
- Produces (namespace `mosa::codegen`):
  - `struct ArchiveEntry { std::string name; std::string data; bool executable = false; };` — `name` is UTF-8, `/`-separated, relative.
  - `[[nodiscard]] std::uint32_t crc32(std::string_view bytes);`
  - `[[nodiscard]] std::string tar_gz_archive(std::span<const ArchiveEntry> entries, std::int64_t mtime);`
  - `[[nodiscard]] std::string zip_archive(std::span<const ArchiveEntry> entries, std::int64_t mtime);`
  - `mtime` is seconds since 1970-01-01 UTC. No directory entries are written (extractors make the parents).

- [ ] **Step 1: Write the failing tests** in `archive_test.cpp`, each a function called from `main`. The test decodes what it checks by hand (read the gzip stored blocks back into the tar, read little-endian fields of the zip):
  - `crc32_known`: `crc32("123456789") == 0xCBF43926`, `crc32("") == 0`.
  - `tar_gz_frame`: one entry `a/hello` ("hi\n", executable): bytes 0–2 are `1f 8b 08`; the last 8 bytes are the CRC-32 and the length of the tar (little-endian); the tar length is a multiple of 10240; header: name `a/hello`, mode `0000755\0`, size `00000000003\0`, magic `ustar\0` and version `00`, the checksum field equals the sum of the 512 header bytes with that field taken as 8 spaces; data `hi\n` padded to 512; then at least two zero blocks. A second non-executable entry has mode `0000644\0`.
  - `tar_gz_splits_blocks`: a 200,000-byte entry: every stored block has `LEN ≤ 65535` and `NLEN == ~LEN`, only the last block has BFINAL set, and the joined payload is the tar.
  - `tar_pax_name`: entry `こんにちは-x/こんにちは` and an ASCII name of 120 bytes: each is preceded by a header with typeflag `x` whose data is `"<n> path=<name>\n"`, `<n>` being the length of that whole record in decimal; the following header has typeflag `0`.
  - `zip_entries`: two entries: local header `50 4b 03 04`, version needed 20, flag bit 11 set, method 0, CRC and both sizes equal to the data; central header `50 4b 01 02` with version made by `0x031E` and external attributes `0100755 << 16` (executable) or `0100644 << 16`; end record `50 4b 05 06` with 2 entries and the right directory offset and size.
  - `zip_dos_time`: mtime 1,794,314,096 (2026-11-10 12:34:56 UTC) gives DOS date `(46 << 9) | (11 << 5) | 10` and time `(12 << 11) | (34 << 5) | 28`.
  - `zip_dos_time_floor`: mtime 0 gives date `(0 << 9) | (1 << 5) | 1` and time 0.

- [ ] **Step 2: Run** `cmake --build build && ctest --test-dir build -R '^archive$' --output-on-failure` — fails to build (no `archive.hpp`).

- [ ] **Step 3: Implement** `archive.cpp`: CRC-32 with the reflected polynomial `0xEDB88320` and a table made in a `constexpr` function returning `std::array<std::uint32_t, 256>`; tar headers as `std::string` of 512 bytes, numbers in octal; the gzip header `1f 8b 08 00` + mtime (LE, 4 bytes) + `00 ff`; zip date from `std::chrono::year_month_day` of the UTC day, clamped to 1980-01-01 00:00:00.

- [ ] **Step 4: Run** the same command — `archive` passes; then the whole `ctest --test-dir build`.

- [ ] **Step 5: Commit** `[phase7] mosac dist の書庫を書く archive.cpp（ustar を無圧縮の gzip に、無圧縮の zip）を足す（ADR 0023、archive_test …）`.

### Task 2: `mosac dist`

**Files:**
- Create: `src/codegen/dist.hpp`, `src/codegen/dist.cpp`, `src/codegen/dist_texts.inc.in`, `tests/mosac/dist_test.cmake`
- Modify: `CMakeLists.txt` (generate `dist_texts.inc`, add `dist.cpp`, add `LICENSE NOTICE` to the `source` component), `src/codegen/runtime_layout.cpp` + `build.hpp` (`host_os_arch`), `src/main_mosac.cpp` (`run_dist`), `lib/messages_en.pl`, `lib/messages_ja.pl`, `tests/mosac/cli_test.cmake:2,37-39`, `tests/tests.cmake`

**Interfaces:**
- Consumes: Task 1's `ArchiveEntry`, `tar_gz_archive`, `zip_archive`; `build_cached`, `with_exe_suffix`, `exe_suffix`, `report_write_error`, `mosac_message`, `read_text` (`build.hpp`); `mosa::host_os`, `mosa::host_arch`; `render_message_fresh` (`mosa/messages.hpp`).
- Produces:
  - `build.hpp`: `[[nodiscard]] std::string host_os_arch();` — `macos-arm64`, `linux-x86_64`, `windows-x86_64`; `runtime_abi` uses it.
  - `dist.hpp`: `[[nodiscard]] std::string dist_readme(std::string_view stem);` and `[[nodiscard]] int write_dist(const std::filesystem::path& exe, std::string_view stem, const std::filesystem::path& out_dir);` — returns the exit code, prints the archive path (UTF-8) and a newline on standard output on success.
  - `dist_texts.inc.in`: `inline constexpr std::string_view k_mosa_license = R"mosa_text(@MOSA_LICENSE_TEXT@)mosa_text";` and `k_mosa_notice` the same way, in `namespace mosa::codegen`. CMake: `file(READ LICENSE MOSA_LICENSE_TEXT)`, `file(READ NOTICE MOSA_NOTICE_TEXT)`, `configure_file(… ${CMAKE_BINARY_DIR}/generated/dist/dist_texts.inc @ONLY)`, `target_include_directories(mosa_lib PRIVATE ${CMAKE_BINARY_DIR}/generated/dist)`, `set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS LICENSE NOTICE)`. The LICENSE text is 11,358 bytes, below MSVC's limit of 16,380 bytes for one literal.
  - Catalog key `mosac_dist_readme` with arguments program name, Mosa version (`mosa::kVersion…` as in `--version`), start command (`./hello`, or `hello.exe` on Windows):
    - en: `'~w~n~nThis program was built with Mosa Prolog ~w. To start it, open a terminal in this folder and type ~w~n~nIt contains the runtime of Mosa Prolog, licensed under the Apache License, Version 2.0 (see LICENSE and NOTICE).'`
    - ja: `'~w~n~nこのプログラムは Mosa Prolog ~w で作りました。動かすには、このフォルダーで端末を開いて ~w と打ちます。~n~nMosa Prolog の実行時ライブラリを含みます。そのライセンスは Apache License, Version 2.0 です（LICENSE と NOTICE を見てください）。'`
    - `README.txt` is the en text, an empty line, the ja text, and a final newline.
  - Remove the key `mosac_dist_unavailable`; add to `mosac_usage` (both languages, after the run line) `mosac dist <file> [-o <directory>] [--interactive] [--debug] [--verbose] [--clean] [--cxx <compiler>] [--runtime-source]` (ja: `<ファイル>`, `<ディレクトリ>` as the ja usage already spells them).
- `run_dist` in `main_mosac.cpp`: `parse_build(args, false)`, `-o` is the directory (default `dist`), `build_cached`, then `write_dist(built.exe, stem, out)`.
- `write_dist`: folder `<stem>-<host_os_arch()>`; entries `<folder>/<stem><exe_suffix()>` (executable), `<folder>/LICENSE`, `<folder>/NOTICE`, `<folder>/README.txt`; zip when `host_os() == HostOs::windows`, else tar.gz; mtime from `std::chrono::system_clock`. An `out_dir` that exists and is not a directory → `report_write_error(out_dir, std::make_error_code(std::errc::not_a_directory))`, code 1; `create_directories` failure → `report_write_error`; write `<archive>.tmp` in binary, then `rename` over the archive, removing the temporary file on failure.

- [ ] **Step 1: Write the failing tests.** `tests/mosac/dist_test.cmake`, in the style of `build_test.cmake` (`mosac` macro, `fail`), cases chosen by `-DCASE`, each in a fresh `WORK`; `tests.cmake` registers `mosac_dist_<case>` with `-DMOSAC -DCASE -DSRC=tests/mosac -DWORK=tests/tmp/mosac_dist_<case> -DCXX=${mosac_build_cxx} -DEXE_SUFFIX -DMOSA_LIB -DROOT=${CMAKE_SOURCE_DIR} -DOS_ARCH=<first two parts of MOSA_ABI> -DARCHIVE_EXT=<.zip on WIN32, else .tar.gz>`:
  - `hello`: `mosac dist hello.pl -o out/` → code 0; the last line of stdout ends with `out/hello-${OS_ARCH}${ARCHIVE_EXT}`; `cmake -E tar xf` it into `WORK/x`; `x/hello-${OS_ARCH}/hello${EXE_SUFFIX}` runs with code 0 and the output of `hello.pl`; `LICENSE` and `NOTICE` equal `${ROOT}/LICENSE` and `${ROOT}/NOTICE` byte for byte; `README.txt` contains `Mosa Prolog 0.1.0`, `Apache License, Version 2.0` and `このプログラムは Mosa Prolog 0.1.0 で作りました`.
  - `default_dir`: no `-o` → `dist/hello-${OS_ARCH}${ARCHIVE_EXT}` exists.
  - `japanese`: copy `hello.pl` to `こんにちは.pl`, dist it, unpack, run `こんにちは-${OS_ARCH}/こんにちは${EXE_SUFFIX}`.
  - `replace`: dist twice; both code 0; the archive unpacks; no file ending in `.tmp` under `out/`.
  - `out_is_file`: `out` is a file → code 1, stderr has `mosac: 書けません: ` or the English `mosac: cannot write: ` text (the message the catalog gives for `mosac_cannot_write`), and `out` is still the same file.
  - `build_fails`: `mosac dist broken.pl -o out` → the same nonzero code as `mosac build broken.pl`, and `out` does not exist.
  - `cli_test.cmake`: `mosac dist` with no file → code 1 and the usage text, replacing the "not part of Mosa 0.1.0" check (update its header line 2).
- [ ] **Step 2: Run** `ctest --test-dir build -R 'mosac_dist|mosac_cli' --output-on-failure` — fail (dist prints the old message).
- [ ] **Step 3: Implement** as in Interfaces.
- [ ] **Step 4: Run** the same, then the whole `ctest --test-dir build` and `./build/mosatest --diff tests/pl` as the CI does.
- [ ] **Step 5: Commit** `[phase7] mosac dist を作る（ADR 0023、mosac_dist の 6 件 …）`.

### Task 3: Documents

**Files:** `docs/SPEC.md` + `docs/SPEC.ja.md` (section 6 row of `mosac dist`: "build + archive `<stem>-<os>-<arch>.tar.gz` (Windows `.zip`) with the executable, Mosa's LICENSE and NOTICE, and README.txt; see ADR 0023"), `docs/manual/{en,ja}/13-mosac.md` (replace the "not part of Mosa 0.1.0" sentence with a short section: the command, what the archive holds, that the receiver unpacks it and runs the executable without Mosa; the command output names the machine, so use a `skip:` block), `CHANGELOG.md` + `CHANGELOG.ja.md` (`### Added` under 0.1.1, before `### Fixed`), `README.md` + `README.ja.md` (status: `mosac dist` done; phase 7 row "In progress (exit check left)"), `src/main_mosac.cpp:1` and `src/codegen/README.md` if it lists files.

- [ ] **Step 1:** Edit the files; run `./build/mosatest --manual docs/manual --all-tables --work tests/tmp/manual_check` → `0 failed`.
- [ ] **Step 2: Commit** `[phase7] mosac dist を SPEC、マニュアル第 13 章、CHANGELOG、README に書く（日英、mosatest --manual …）`.

### Task 4: Review and STATUS

- [ ] **Step 1:** One fresh reviewer on opus over the whole branch; fix what it finds, with a test first for each bug.
- [ ] **Step 2:** Update `docs/STATUS.md` (≤ 15 lines) and commit.
