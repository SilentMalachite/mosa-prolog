cmake_minimum_required(VERSION 3.25)

project(mosa VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# 既定の静的リンク（SPEC 第6節）。MSVC の C++ 実行時ライブラリも静的（/MT）にし、mosac build の実行ファイルが
# DLL を要らないようにする。libmosa と生成コードの /MT と /MD がずれると LNK2038 でリンクできない。
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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
  src/machine/native_control.cpp
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
  src/platform/process.cpp
  src/platform/toolchain.cpp
  src/machine/bench.cpp
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
  src/codegen/emit_control.cpp
  src/codegen/mangle.cpp
  src/codegen/project.cpp
  src/codegen/check.cpp
  src/codegen/build.cpp
  src/codegen/cache.cpp
)

# テスト（tests/tests.cmake）。同梱の源から実行時ライブラリだけを作るとき（mosac build --runtime-source）は外す。
option(MOSA_BUILD_TESTS "Build the tests" ON)
if(MOSA_BUILD_TESTS)
  include(${CMAKE_SOURCE_DIR}/tests/tests.cmake)
endif()
