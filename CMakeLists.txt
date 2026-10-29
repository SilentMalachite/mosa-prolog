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
# include/ も写す。mosac はビルド木の形では自分の隣で libmosa と include/ と pl/ を引く（計画 Task 5、Task 10）。
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
  src/machine/native_library.cpp
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
  src/codegen/project_main.cpp
  src/codegen/check.cpp
  src/codegen/build.cpp
  src/codegen/cache.cpp
  src/codegen/runtime_layout.cpp
)

# 実行時ライブラリの置き場所の鍵 <OS>-<arch>-<compiler>（SPEC 第6節、計画 Task 10）。綴りは mosac の runtime_abi
# （src/codegen/runtime_layout.cpp）と、mosac emit が出す CMakeLists.txt の既定と同じにする。構成（Debug/Release）は
# 鍵に入れない。インストールするのは Release だけで、mosac build --debug も Release の libmosa（MSVC は /MT）に結ぶ。
string(TOLOWER "${CMAKE_SYSTEM_NAME}" mosa_abi_os)
if(mosa_abi_os STREQUAL "darwin")
  set(mosa_abi_os macos)
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" mosa_abi_arch)
if(mosa_abi_arch MATCHES "^(amd64|x86_64|x64)$")
  set(mosa_abi_arch x86_64)
elseif(mosa_abi_arch MATCHES "^(arm64|aarch64)$")
  set(mosa_abi_arch arm64)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(mosa_abi_cxx clang)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(mosa_abi_cxx gcc)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(mosa_abi_cxx msvc)
else()
  string(TOLOWER "${CMAKE_CXX_COMPILER_ID}" mosa_abi_cxx)
endif()
set(mosa_abi "${mosa_abi_os}-${mosa_abi_arch}-${mosa_abi_cxx}")
set(mosa_runtime_dest "lib/mosa/${mosa_abi}")

# lib/*.pl の翻訳済みの単位（計画 Task 10）。作った mosac に emit --library を頼んで <語幹>.cpp を出し、ライブラリごとの
# 静的ライブラリ libmosa_pl_<語幹>.a（MSVC は mosa_pl_<語幹>.lib）にして mosac の隣の pl/ に置く。mosac build は
# use_module された物と文面の表だけを結ぶ。翻訳のとき library(_) は MOSA_LIB で渡したこの lib/ から読み、開発者の
# 環境の MOSA_LIB に左右されない。どのライブラリもほかのライブラリを use_module しうるので、依存は lib/*.pl 全部にする。
file(GLOB mosa_pl_sources CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/lib/*.pl)
set(mosa_pl_units_dir ${CMAKE_BINARY_DIR}/pl_units)
set(mosa_pl_targets)
foreach(pl_source ${mosa_pl_sources})
  get_filename_component(pl_stem ${pl_source} NAME_WE)
  add_custom_command(OUTPUT ${mosa_pl_units_dir}/${pl_stem}.cpp
    COMMAND ${CMAKE_COMMAND} -E env MOSA_LIB=${CMAKE_SOURCE_DIR}/lib
      $<TARGET_FILE:mosac> emit --library ${pl_source} -o ${mosa_pl_units_dir}
    DEPENDS mosac ${mosa_pl_sources}
    COMMENT "mosac emit --library ${pl_stem}.pl")
  add_library(mosa_pl_${pl_stem} STATIC ${mosa_pl_units_dir}/${pl_stem}.cpp)
  target_link_libraries(mosa_pl_${pl_stem} PRIVATE mosa_lib)
  set_target_properties(mosa_pl_${pl_stem} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:mosac>/pl)
  list(APPEND mosa_pl_targets mosa_pl_${pl_stem})
endforeach()
add_custom_target(mosa_pl_units ALL DEPENDS ${mosa_pl_targets})

# インストールの形（SPEC 第6節）。bin/ に 3 コマンドと lib/*.pl（library(_) は実行ファイルの隣の lib/ を探す。SPEC 第5節）、
# lib/mosa/<OS>-<arch>-<compiler>/ に libmosa と include/mosa/ と翻訳済みの単位 pl/、lib/mosa/source/ に
# mosac build --runtime-source が ABI の合わないコンパイラで作り直すための源（tests/ は要らない。MOSA_BUILD_TESTS=OFF）。
install(TARGETS mosa mosac mosatest RUNTIME DESTINATION bin COMPONENT tools)
install(DIRECTORY lib/ DESTINATION bin/lib COMPONENT tools FILES_MATCHING PATTERN "*.pl" PATTERN "README.md")
install(TARGETS mosa_lib ARCHIVE DESTINATION ${mosa_runtime_dest} COMPONENT runtime)
install(DIRECTORY include/mosa DESTINATION ${mosa_runtime_dest}/include COMPONENT runtime)
install(TARGETS ${mosa_pl_targets} ARCHIVE DESTINATION ${mosa_runtime_dest}/pl COMPONENT runtime)
install(FILES CMakeLists.txt DESTINATION lib/mosa/source COMPONENT source)
install(DIRECTORY src include lib DESTINATION lib/mosa/source COMPONENT source PATTERN ".DS_Store" EXCLUDE)

# テスト（tests/tests.cmake）。同梱の源から実行時ライブラリだけを作るとき（mosac build --runtime-source）は外す。
option(MOSA_BUILD_TESTS "Build the tests" ON)
if(MOSA_BUILD_TESTS)
  include(${CMAKE_SOURCE_DIR}/tests/tests.cmake)
endif()
