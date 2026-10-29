% 英語の文面（ADR 0007）。message(Key, Format) の Format は format/2 の書式原子。
% 何も公開しない。C++ は messages_en:message/2 と修飾して引くので、user の名前は増えない。
:- module(messages_en, []).
:- use_module(library(format)).

message(goal_raised, 'Warning: goal raised exception: ~q').
message(heap_limit, 'Warning: heap limit exceeded').
message(help_japanese_variables, 'Variables with Japanese names start with an underscore, as in _太郎. Without it, 太郎 is an atom.').

% 構文エラー（ADR 0007）。理由の鍵は syntax_error(Key) の Key と同じ。
message(syntax_error, 'Syntax error: line ~d column ~d: ~w ~q').
message(syntax_error_at_end, 'Syntax error: line ~d column ~d: ~w at end of input').
message(invalid_utf8, 'invalid UTF-8').
message(illegal_character, 'illegal character').
message(unterminated_quoted, 'unterminated quoted').
message(unterminated_comment, 'unterminated comment').
message(bad_escape, 'bad escape sequence').
message(integer_overflow, 'integer out of range').
message(unexpected_token, 'unexpected token').
message(max_arity, 'too many arguments').
message(incomplete_term, 'incomplete term').

% 読み込みと使い方の診断（ADR 0007、Task 12b）。
message(usage, 'mosa: usage: mosa [file...] [-g goal] [-t halt] [-- argument...]').
message(mosatest_usage, 'mosatest: usage: mosatest --roundtrip <file>~nmosatest --suite <directory> [--report <file>]~nmosatest --bench <file> [--count <n>] [--work <directory>] [--cmake <path>]~n--report may be omitted only when tests/conformance is given from the repository root').
message(mosac_unavailable, 'mosac: this command is not available in phase 0').
message(mosac_usage, 'mosac: usage: mosac build <file> [-o <file>] [--debug] [--verbose] [--clean] [--cxx <compiler>] [--runtime-source]~nmosac run <file> [--debug] [--verbose] [--keep] [--clean] [--cxx <compiler>] [--runtime-source] [-- <argument>...]~nmosac emit <file> -o <directory>~nmosac emit --library <lib/name.pl> -o <directory>~nmosac toolchain [--cxx <compiler>]').
message(mosac_cannot_write, 'mosac: cannot write: ~w').
message(mosac_undefined_call, 'call to undefined procedure ~w').
message(mosac_export_undefined, 'exported procedure ~w is not defined').

% mosac toolchain（SPEC 第6節、計画 Task 8）。見つからないときの導入手順は OS ごとに 1 つ。
message(toolchain_compiler, 'compiler: ~w').
message(toolchain_version, 'version: ~w').
message(toolchain_cmake, 'CMake: ~w').
message(toolchain_cmake_missing, 'CMake: not found (mosac calls the compiler directly)').
message(toolchain_runtime, 'runtime: ~w').
message(toolchain_msvc_env, 'environment: ~w variables from vcvarsall.bat').
message(toolchain_not_found, 'mosac: no C++20 compiler was found').
message(toolchain_named_not_found, 'mosac: the compiler ~w given by ~w was not found or did not run').
message(toolchain_hint_macos, 'Install the Xcode Command Line Tools: xcode-select --install').
message(toolchain_hint_linux, 'Install a C++ compiler: sudo apt install g++').
message(toolchain_hint_windows, 'Install Visual Studio Build Tools with the "Desktop development with C++" workload: https://visualstudio.microsoft.com/downloads/').

% mosac build と run（SPEC 第6節、計画 Task 9）。環境の問題は直し方まで書き、生成コードの誤りは内部エラーとして出す。
message(mosac_internal_error, 'internal error: the generated code cannot be compiled. Please report it with ~w attached').
message(mosac_disk_full, 'mosac: not enough disk space to write ~w~nFree some space and try again. mosac build --clean removes old intermediate files in .mosac-cache').
message(mosac_no_permission, 'mosac: no permission to write ~w~nRun mosac in a directory you can write to, or give a writable place with -o').
message(mosac_runtime_missing, 'mosac: the runtime library was not found: ~w~nCheck its location with mosac toolchain').
message(mosac_cannot_start, 'mosac: cannot start ~w').
message(mosac_verbose_run, 'mosac: running: ~w').
message(mosac_cache_hit, 'mosac: using the cache: ~w').
message(mosac_kept, 'mosac: kept the temporary directory: ~w').
message(mosac_cmake_configure_failed, 'mosac: CMake could not configure the build (log: ~w)~nCheck CMake and the compiler, or set MOSA_NO_CMAKE=1 to let mosac call the compiler directly').
message(mosac_cache_symlink, 'mosac: ~w is a symbolic link, so mosac does not use it~nRemove it and run mosac again').

% 同梱の実行時ライブラリと翻訳済みの lib/*.pl（SPEC 第6節、計画 Task 10）。
message(mosac_library_not_module, 'mosac: ~w does not start with module/2, so it cannot become a library unit').
message(mosac_catalog_missing, 'mosac: the message catalogs messages_ja and messages_en cannot be read (~w)~nCheck the lib/ next to mosac or the directory in the MOSA_LIB environment variable').
message(mosac_runtime_abi_missing, 'mosac: there is no runtime library for this compiler: ~w~nUse the compiler mosac was built with, or run mosac build --runtime-source to build it from the bundled source (the result stays in .mosac-cache)').
message(mosac_unit_missing, 'mosac: the translated library ~w was not found~nCheck the runtime location with mosac toolchain').
message(mosac_runtime_source_missing, 'mosac: the bundled runtime source was not found: ~w').
message(mosac_runtime_source_needs_cmake, 'mosac: --runtime-source needs CMake 3.25 or later (check with mosac toolchain)').
message(mosac_runtime_building, 'mosac: building the runtime library from the bundled source: ~w').
message(mosac_runtime_build_failed, 'mosac: the runtime library could not be built from source (log: ~w)~nCheck the compiler and CMake').

message(cannot_open_file, '~w: cannot open file').
message(file_cycle, '~w: files load each other in a cycle').
message(clauses_apart, 'clauses of ~w are not together').
message(predicate_running, 'cannot replace a predicate that is running').
message(directive_unsupported, 'directive not supported: ~w').
message(directive_failed, 'directive goal failed: ~w').
message(not_predicate_indicator, 'not a predicate indicator').
message(static_to_dynamic, 'a static predicate cannot become dynamic').
message(op_bad_priority, 'operator priority out of range').
message(op_bad_specifier, 'invalid operator specifier').
message(op_modify_comma, 'the comma operator cannot be changed').
message(op_create_empty, '[] and {} cannot be operators').
message(op_infix_and_postfix, 'an operator cannot be both infix and postfix').
message(op_create_bar, 'this bar cannot be an operator').
message(op_name_not_atom, 'operator name is not an atom').
message(op_failed, 'cannot declare the operator').

% mosatest --bench（計画 Task 6）。時間はミリ秒、比はインタプリタの時間を翻訳の時間で割った値。
message(bench_header, '~w: bench(~w), median of ~w runs').
message(bench_interpreter, 'interpreter: ~w ms').
message(bench_translated, 'translated: ~w ms').
message(bench_ratio, 'ratio: ~w').
message(bench_cannot_start, 'mosatest: cannot start ~w').
message(bench_not_found, 'mosatest: ~w not found').
message(bench_run_failed, 'mosatest: ~w exited with code ~w').
message(bench_step_failed, 'mosatest: ~w failed with code ~w (output in ~w)').
message(bench_cannot_write, 'mosatest: cannot write ~w').
