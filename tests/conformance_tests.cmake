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

# tests/tmp/suite は tests.lgt を流す全てが共有する（lgt_io_context のスクラッチ）。空かどうかを見る
# suite_path_test と conformance_io_test が、ほかの実行中スクラッチで ctest -j のとき落ちていたので、
# 流す側すべてに同じロックを付ける（diff_conformance は下で別に付く。全体レビュー推奨5）。
set_tests_properties(assert_shared suite_assert_test suite_if_test suite_imports_test suite_approx_test
  suite_path_test suite_vars_test suite_encoding_test conformance_report_test conformance_io_test
  conformance_output_test conformance_badge_test conformance_cwd_test
  PROPERTIES RESOURCE_LOCK suite_scratch)

add_executable(conformance_scope_test tests/cpp/conformance_scope_test.cpp)
target_include_directories(conformance_scope_test PRIVATE tests/cpp src/machine)
target_link_libraries(conformance_scope_test PRIVATE mosa_lib)
add_test(NAME conformance_scope_test COMMAND $<TARGET_FILE:conformance_scope_test>)
