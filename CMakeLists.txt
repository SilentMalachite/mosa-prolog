cmake_minimum_required(VERSION 3.25)

project(mosa VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mosa_warnings INTERFACE)
target_compile_options(mosa_warnings INTERFACE
  $<$<CXX_COMPILER_ID:MSVC>:/W4;/WX;/utf-8;/permissive-;/Zc:__cplusplus>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Werror>
)

add_library(mosa_lib STATIC src/version.cpp)
target_include_directories(mosa_lib PUBLIC include)
set_target_properties(mosa_lib PROPERTIES OUTPUT_NAME mosa)
target_link_libraries(mosa_lib PUBLIC mosa_warnings)
# CommandLineToArgvW（src/platform/command_line.cpp）。
if(WIN32)
  target_link_libraries(mosa_lib PRIVATE shell32)
endif()

add_executable(mosa src/main_mosa.cpp)
target_link_libraries(mosa PRIVATE mosa_lib)

add_executable(mosac src/main_mosac.cpp)
target_link_libraries(mosac PRIVATE mosa_lib)

add_executable(mosatest src/main_mosatest.cpp)
target_link_libraries(mosatest PRIVATE mosa_lib)

# library(...) は実行ファイルの隣の lib/ を探す（SPEC 第5節）。POST_BUILD は再リンクのときしか動かず、
# lib/*.pl だけを直したビルドで古い写しが残るので、ビルドのたびに写す目標にする。
# include/ も写す。mosac emit の CMakeLists.txt は mosac の隣で libmosa と include/ を引く（計画 Task 5）。
add_custom_target(mosa_lib_dir ALL
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lib $<TARGET_FILE_DIR:mosa>/lib
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/include $<TARGET_FILE_DIR:mosac>/include)
foreach(command mosa mosac mosatest)
  add_dependencies(${command} mosa_lib_dir)
endforeach()

target_sources(mosa_lib PRIVATE src/read/char_class.cpp src/read/fullwidth.cpp)
target_sources(mosa_lib PRIVATE src/read/lexer.cpp src/read/lexer_literal.cpp)
target_sources(mosa_lib PRIVATE src/read/operator_table.cpp)
target_sources(mosa_lib PRIVATE src/core/atom.cpp src/core/heap.cpp src/core/unify.cpp)
target_sources(mosa_lib PRIVATE src/read/reader.cpp src/read/reader_bracket.cpp)
target_sources(mosa_lib PRIVATE src/write/writer.cpp src/write/term_equal.cpp)
target_sources(mosa_lib PRIVATE src/read/roundtrip.cpp)
target_sources(mosa_lib PRIVATE
  src/machine/engine.cpp
  src/machine/load.cpp
  src/machine/load_clauses.cpp
  src/machine/native.cpp
  src/machine/native_redo.cpp
  src/machine/native_main.cpp
  src/machine/load_directive.cpp
  src/module/module_table.cpp
  src/module/qualify.cpp
  src/module/target.cpp
  src/module/meta.cpp
  src/module/module_ops.cpp
  src/module/import.cpp
  src/module/library_path.cpp
  src/machine/load_path.cpp
  src/machine/compile.cpp
  src/machine/choice.cpp
  src/machine/cut.cpp
  src/machine/catch.cpp
  src/machine/index.cpp
  src/machine/answer.cpp
  src/machine/consult.cpp
  src/machine/session.cpp
  src/machine/messages.cpp
  src/machine/suite.cpp
  src/machine/iso_suite.cpp
  src/machine/lgt_file.cpp
  src/machine/lgt_shape.cpp
  src/machine/lgt_judge.cpp
  src/machine/lgt_approx.cpp
  src/machine/lgt_copy.cpp
  src/machine/lgt_exec.cpp
  src/machine/lgt_cond.cpp
  src/machine/lgt_io.cpp
  src/machine/lgt_output.cpp
  src/machine/lgt_libs.cpp
  src/platform/terminal.cpp
  src/platform/exe_path.cpp
  src/platform/command_line.cpp
  src/platform/console.cpp
  src/builtins/errors.cpp
  src/builtins/types.cpp
  src/builtins/order.cpp
  src/builtins/lists.cpp
  src/builtins/format_float.cpp
  src/builtins/case.cpp
  src/builtins/char_width.cpp
  src/builtins/terms.cpp
  src/builtins/control.cpp
  src/builtins/arith.cpp
  src/builtins/arith_float.cpp
  src/builtins/atoms.cpp
  src/builtins/atoms_number.cpp
  src/builtins/flags.cpp
  src/builtins/database.cpp
  src/builtins/database_scan.cpp
  src/builtins/module_prop.cpp
  src/builtins/clause_term.cpp
  src/builtins/solutions.cpp
  src/builtins/solutions_answer.cpp
  src/builtins/streams.cpp
  src/builtins/stream_prop.cpp
  src/builtins/chars.cpp
  src/builtins/bytes.cpp
  src/builtins/read_pred.cpp
  src/builtins/write_pred.cpp
  src/builtins/operators.cpp
)
# mosac の翻訳器（計画 Task 4）。Machine の IR を読むので libmosa の中に置く。
target_sources(mosa_lib PRIVATE
  src/codegen/emit.cpp
  src/codegen/emit_head.cpp
  src/codegen/emit_body.cpp
  src/codegen/mangle.cpp
  src/codegen/project.cpp
  src/codegen/check.cpp
)

enable_testing()

add_executable(smoke_test tests/cpp/smoke_test.cpp)
target_include_directories(smoke_test PRIVATE tests/cpp)
target_link_libraries(smoke_test PRIVATE mosa_warnings)

add_test(NAME smoke COMMAND $<TARGET_FILE:smoke_test>)

add_executable(char_class_test tests/cpp/char_class_test.cpp)
target_include_directories(char_class_test PRIVATE tests/cpp)
target_link_libraries(char_class_test PRIVATE mosa_lib)
add_test(NAME char_class COMMAND $<TARGET_FILE:char_class_test>)

add_executable(lexer_test tests/cpp/lexer_test.cpp)
target_include_directories(lexer_test PRIVATE tests/cpp)
target_link_libraries(lexer_test PRIVATE mosa_lib)
add_test(NAME lexer COMMAND $<TARGET_FILE:lexer_test>)

add_executable(operator_test tests/cpp/operator_test.cpp)
target_include_directories(operator_test PRIVATE tests/cpp)
target_link_libraries(operator_test PRIVATE mosa_lib)
add_test(NAME operator_test COMMAND $<TARGET_FILE:operator_test>)

add_executable(cell_test tests/cpp/cell_test.cpp)
target_include_directories(cell_test PRIVATE tests/cpp)
target_link_libraries(cell_test PRIVATE mosa_lib)
add_test(NAME cell COMMAND $<TARGET_FILE:cell_test>)

add_executable(unify_test tests/cpp/unify_test.cpp)
target_include_directories(unify_test PRIVATE tests/cpp)
target_link_libraries(unify_test PRIVATE mosa_lib)
add_test(NAME unify COMMAND $<TARGET_FILE:unify_test>)

add_executable(reader_test tests/cpp/reader_test.cpp)
target_include_directories(reader_test PRIVATE tests/cpp)
target_link_libraries(reader_test PRIVATE mosa_lib)
add_test(NAME reader COMMAND $<TARGET_FILE:reader_test>)

add_executable(writer_test tests/cpp/writer_test.cpp)
target_include_directories(writer_test PRIVATE tests/cpp)
target_link_libraries(writer_test PRIVATE mosa_lib)
add_test(NAME writer COMMAND $<TARGET_FILE:writer_test>)

add_test(NAME roundtrip COMMAND $<TARGET_FILE:mosatest> --roundtrip ${CMAKE_SOURCE_DIR}/tests/pl/syntax.pl)
# テストのパスはリポジトリの根から書く。build の場所に左右されない（計画 Task 15）。
add_test(NAME prolog_tests COMMAND $<TARGET_FILE:mosatest> ${CMAKE_SOURCE_DIR}/tests/pl
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(session_test tests/cpp/session_test.cpp)
target_include_directories(session_test PRIVATE tests/cpp)
target_compile_definitions(session_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(session_test PRIVATE mosa_lib)
add_test(NAME session COMMAND $<TARGET_FILE:session_test>)
# 今ある日本語の文面の確認を、流す機械の LANG に左右させない（ADR 0007）。
set_tests_properties(session PROPERTIES ENVIRONMENT "LANG=ja_JP.UTF-8")
# MOSA_LIB は実行ファイルの lib/ より先に探す。鍵の無い messages_en.pl で、代わりの表示を確かめる。
file(WRITE ${CMAKE_SOURCE_DIR}/tests/tmp/messages_empty/messages_en.pl ":- module(messages_en, []).\n")
add_test(NAME session_messages_missing COMMAND $<TARGET_FILE:session_test> missing)
set_tests_properties(session_messages_missing PROPERTIES
  ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/tests/tmp/messages_empty")
add_test(NAME session_english COMMAND $<TARGET_FILE:session_test> english)
set_tests_properties(session_english PROPERTIES ENVIRONMENT "LANG=C")

add_executable(stream_test tests/cpp/stream_test.cpp)
target_include_directories(stream_test PRIVATE tests/cpp)
target_compile_definitions(stream_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(stream_test PRIVATE mosa_lib)
add_test(NAME stream_test COMMAND $<TARGET_FILE:stream_test>)

add_executable(machine_test tests/cpp/machine_test.cpp)
target_include_directories(machine_test PRIVATE tests/cpp)
target_link_libraries(machine_test PRIVATE mosa_lib)
add_test(NAME machine COMMAND $<TARGET_FILE:machine_test>)
add_executable(evaluable_test tests/cpp/evaluable_test.cpp)
target_include_directories(evaluable_test PRIVATE tests/cpp src)
target_link_libraries(evaluable_test PRIVATE mosa_lib)
add_test(NAME evaluable COMMAND $<TARGET_FILE:evaluable_test>)
add_executable(module_load_test tests/cpp/module_load_test.cpp)
target_include_directories(module_load_test PRIVATE tests/cpp)
target_link_libraries(module_load_test PRIVATE mosa_lib)
target_compile_definitions(module_load_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME module_load COMMAND $<TARGET_FILE:module_load_test>)
add_executable(library_path_test tests/cpp/library_path_test.cpp)
target_include_directories(library_path_test PRIVATE tests/cpp)
target_link_libraries(library_path_test PRIVATE mosa_lib)
target_compile_definitions(library_path_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# 同じテストを MOSA_LIB の無い環境と有る環境で流す。テスト側に環境変数を書き換える OS 分岐を置かない。
add_test(NAME library_path COMMAND $<TARGET_FILE:library_path_test>)
set_tests_properties(library_path PROPERTIES ENVIRONMENT_MODIFICATION "MOSA_LIB=unset:")
add_test(NAME library_path_env COMMAND $<TARGET_FILE:library_path_test> env)
set_tests_properties(library_path_env PROPERTIES
  ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/tests/tmp/library_path/lib_env")

add_executable(include_test tests/cpp/include_test.cpp)
target_include_directories(include_test PRIVATE tests/cpp)
target_compile_definitions(include_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(include_test PRIVATE mosa_lib)
add_test(NAME include_test COMMAND $<TARGET_FILE:include_test>)

add_executable(ensure_test tests/cpp/ensure_test.cpp)
target_include_directories(ensure_test PRIVATE tests/cpp)
target_compile_definitions(ensure_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(ensure_test PRIVATE mosa_lib)
add_test(NAME ensure_test COMMAND $<TARGET_FILE:ensure_test>)

add_executable(suite_assert_test tests/cpp/suite_assert_test.cpp)
target_include_directories(suite_assert_test PRIVATE tests/cpp)
target_compile_definitions(suite_assert_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_assert_test PRIVATE mosa_lib)
add_test(NAME suite_assert_test COMMAND $<TARGET_FILE:suite_assert_test>)

add_executable(suite_if_test tests/cpp/suite_if_test.cpp)
target_include_directories(suite_if_test PRIVATE tests/cpp)
target_compile_definitions(suite_if_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_if_test PRIVATE mosa_lib)
add_test(NAME suite_if_test COMMAND $<TARGET_FILE:suite_if_test>)

add_executable(suite_imports_test tests/cpp/suite_imports_test.cpp)
target_include_directories(suite_imports_test PRIVATE tests/cpp)
target_compile_definitions(suite_imports_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_imports_test PRIVATE mosa_lib)
add_test(NAME suite_imports_test COMMAND $<TARGET_FILE:suite_imports_test>)

add_executable(suite_approx_test tests/cpp/suite_approx_test.cpp)
target_include_directories(suite_approx_test PRIVATE tests/cpp)
target_compile_definitions(suite_approx_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_approx_test PRIVATE mosa_lib)
add_test(NAME suite_approx_test COMMAND $<TARGET_FILE:suite_approx_test>)

add_executable(suite_path_test tests/cpp/suite_path_test.cpp)
target_include_directories(suite_path_test PRIVATE tests/cpp)
target_compile_definitions(suite_path_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_path_test PRIVATE mosa_lib)
add_test(NAME suite_path_test COMMAND $<TARGET_FILE:suite_path_test>)

add_executable(suite_vars_test tests/cpp/suite_vars_test.cpp)
target_include_directories(suite_vars_test PRIVATE tests/cpp)
target_compile_definitions(suite_vars_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_vars_test PRIVATE mosa_lib)
add_test(NAME suite_vars_test COMMAND $<TARGET_FILE:suite_vars_test>)

add_executable(suite_encoding_test tests/cpp/suite_encoding_test.cpp)
target_include_directories(suite_encoding_test PRIVATE tests/cpp)
target_compile_definitions(suite_encoding_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_encoding_test PRIVATE mosa_lib)
add_test(NAME suite_encoding_test COMMAND $<TARGET_FILE:suite_encoding_test>)

add_executable(files_test tests/cpp/files_test.cpp)
target_include_directories(files_test PRIVATE tests/cpp)
target_compile_definitions(files_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(files_test PRIVATE mosa_lib)
add_test(NAME files_test COMMAND $<TARGET_FILE:files_test>)

add_executable(conformance_report_test tests/cpp/conformance_report_test.cpp)
target_include_directories(conformance_report_test PRIVATE tests/cpp src/machine)
target_compile_definitions(conformance_report_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(conformance_report_test PRIVATE mosa_lib)
add_test(NAME conformance_report_test COMMAND $<TARGET_FILE:conformance_report_test>)

add_executable(conformance_io_test tests/cpp/conformance_io_test.cpp)
target_include_directories(conformance_io_test PRIVATE tests/cpp)
target_compile_definitions(conformance_io_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(conformance_io_test PRIVATE mosa_lib)
add_test(NAME conformance_io_test COMMAND $<TARGET_FILE:conformance_io_test>)

add_executable(conformance_output_test tests/cpp/conformance_output_test.cpp)
target_include_directories(conformance_output_test PRIVATE tests/cpp)
target_compile_definitions(conformance_output_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(conformance_output_test PRIVATE mosa_lib)
add_test(NAME conformance_output_test COMMAND $<TARGET_FILE:conformance_output_test>)

add_executable(conformance_badge_test tests/cpp/conformance_badge_test.cpp)
target_include_directories(conformance_badge_test PRIVATE tests/cpp)
target_compile_definitions(conformance_badge_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(conformance_badge_test PRIVATE mosa_lib)
add_test(NAME conformance_badge_test COMMAND $<TARGET_FILE:conformance_badge_test>)

add_executable(conformance_cwd_test tests/cpp/conformance_cwd_test.cpp)
target_include_directories(conformance_cwd_test PRIVATE tests/cpp)
target_compile_definitions(conformance_cwd_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(conformance_cwd_test PRIVATE mosa_lib)
add_test(NAME conformance_cwd_test COMMAND $<TARGET_FILE:conformance_cwd_test>)

add_executable(suite_args_test tests/cpp/suite_args_test.cpp)
target_include_directories(suite_args_test PRIVATE tests/cpp)
target_compile_definitions(suite_args_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_args_test PRIVATE mosa_lib)
add_test(NAME suite_args_test COMMAND $<TARGET_FILE:suite_args_test>)

# \r と \n は CMake のエスケープで CR と LF にする。\\r のままだと文字 r になり、1 行の版表示に一致しない。
set(version_pass_regex "^mosa 0\\.0\\.1 \\(phase 5\\)\r?\n?$")
add_test(NAME mosa_version COMMAND $<TARGET_FILE:mosa> --version)
add_test(NAME mosac_version COMMAND $<TARGET_FILE:mosac> --version)
add_test(NAME mosatest_version COMMAND $<TARGET_FILE:mosatest> --version)
set_tests_properties(mosa_version mosac_version mosatest_version PROPERTIES
  PASS_REGULAR_EXPRESSION "${version_pass_regex}")

add_executable(console_test tests/cpp/console_test.cpp)
target_include_directories(console_test PRIVATE tests/cpp)
target_link_libraries(console_test PRIVATE mosa_lib)
add_test(NAME console COMMAND $<TARGET_FILE:console_test> plain --flag 42)
add_test(NAME mosa_utf8_pipe COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp -P ${CMAKE_SOURCE_DIR}/tests/cpp/mosa_utf8_pipe_test.cmake)

add_test(NAME mosa_japanese_arg COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp -P ${CMAKE_SOURCE_DIR}/tests/cpp/mosa_japanese_arg_test.cmake)

add_test(NAME mosa_stdin COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp -P ${CMAKE_SOURCE_DIR}/tests/cpp/mosa_stdin_test.cmake)

# 手書きの生成関数は runtime.hpp だけを見る。internal.hpp を見るのは指紋を出す側の native_test.cpp だけ。
add_executable(native_test tests/cpp/native_test.cpp tests/cpp/native_units.cpp)
target_include_directories(native_test PRIVATE tests/cpp src)
target_compile_definitions(native_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(native_test PRIVATE mosa_lib)
add_test(NAME native COMMAND $<TARGET_FILE:native_test>)

add_executable(codegen_test tests/cpp/codegen_test.cpp)
target_include_directories(codegen_test PRIVATE tests/cpp src)
target_compile_definitions(codegen_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(codegen_test PRIVATE mosa_lib)
add_test(NAME codegen COMMAND $<TARGET_FILE:codegen_test>)

# mosac emit の目録を本体と同じ生成器、コンパイラ、構成で作る。libmosa は作り直さず、このビルド木のものを使う。
foreach(emit_case hello broken undefined silent newop halt)
  add_test(NAME mosac_emit_${emit_case} COMMAND ${CMAKE_COMMAND}
    -DMOSAC=$<TARGET_FILE:mosac> -DCASE=${emit_case} -DSRC=${CMAKE_SOURCE_DIR}/tests/mosac
    -DWORK=${CMAKE_SOURCE_DIR}/tests/tmp/mosac_${emit_case} "-DGENERATOR=${CMAKE_GENERATOR}"
    "-DPLATFORM=${CMAKE_GENERATOR_PLATFORM}" "-DCXX=${CMAKE_CXX_COMPILER}" "-DBUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCONFIG=$<CONFIG>" "-DCXX_FLAGS=${CMAKE_CXX_FLAGS}" "-DLINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
    "-DEXE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}" -DMOSA_LIB=${CMAKE_SOURCE_DIR}/lib
    -P ${CMAKE_SOURCE_DIR}/tests/mosac/emit_test.cmake)
endforeach()

# 日本語の文面を確かめるテストを、流す機械の LANG に左右させない（ADR 0007）。英語の既定は session_english が見る。
get_property(mosa_all_tests DIRECTORY PROPERTY TESTS)
list(REMOVE_ITEM mosa_all_tests session_english)
set_property(TEST ${mosa_all_tests} APPEND PROPERTY ENVIRONMENT_MODIFICATION "LANG=set:ja_JP.UTF-8")
