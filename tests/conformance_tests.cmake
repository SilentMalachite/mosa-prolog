# tests/tests.cmake から include する。assert_shared のテストに属性を付けるので、その定義より後で読む。
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

# レビュー 2026-11-20 の W1。落ちるべき試験を判定器が不合格にし、プロセスごと落ちないこと（C-04 から）。
add_executable(suite_judge_test tests/cpp/suite_judge_test.cpp)
target_include_directories(suite_judge_test PRIVATE tests/cpp)
target_compile_definitions(suite_judge_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_judge_test PRIVATE mosa_lib)
add_test(NAME suite_judge_test COMMAND $<TARGET_FILE:suite_judge_test>)

# レビュー 2026-11-20 の H-40。スイートの分母は、test/1 も含めてファイルに書かれた試験をすべて数える。
add_executable(suite_count_test tests/cpp/suite_count_test.cpp)
target_include_directories(suite_count_test PRIVATE tests/cpp)
target_compile_definitions(suite_count_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_count_test PRIVATE mosa_lib)
add_test(NAME suite_count_test COMMAND $<TARGET_FILE:suite_count_test>)

# レビュー 2026-11-20 の M-50。test/3 の setup(Goal) と cleanup(Goal) は試験ごとに、本体と変数を共有して動く。
add_executable(suite_option_test tests/cpp/suite_option_test.cpp)
target_include_directories(suite_option_test PRIVATE tests/cpp)
target_compile_definitions(suite_option_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_option_test PRIVATE mosa_lib)
add_test(NAME suite_option_test COMMAND $<TARGET_FILE:suite_option_test>)

# レビュー 2026-11-20 の M-52。:- if と condition/1 の条件は一つのゴールとして動き、(C -> T ; E) と束縛を守る。
add_executable(suite_cond_test tests/cpp/suite_cond_test.cpp)
target_include_directories(suite_cond_test PRIVATE tests/cpp)
target_compile_definitions(suite_cond_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_cond_test PRIVATE mosa_lib)
add_test(NAME suite_cond_test COMMAND $<TARGET_FILE:suite_cond_test>)
add_executable(suite_super_test tests/cpp/suite_super_test.cpp)
target_include_directories(suite_super_test PRIVATE tests/cpp)
target_compile_definitions(suite_super_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_super_test PRIVATE mosa_lib)
add_test(NAME suite_super_test COMMAND $<TARGET_FILE:suite_super_test>)

add_executable(suite_encoding_test tests/cpp/suite_encoding_test.cpp)
target_include_directories(suite_encoding_test PRIVATE tests/cpp)
target_compile_definitions(suite_encoding_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_encoding_test PRIVATE mosa_lib)
add_test(NAME suite_encoding_test COMMAND $<TARGET_FILE:suite_encoding_test>)

add_executable(suite_tester_test tests/cpp/suite_tester_test.cpp)
target_include_directories(suite_tester_test PRIVATE tests/cpp)
target_compile_definitions(suite_tester_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_tester_test PRIVATE mosa_lib)
add_test(NAME suite_tester_test COMMAND $<TARGET_FILE:suite_tester_test>)

add_executable(files_test tests/cpp/files_test.cpp)
target_include_directories(files_test PRIVATE tests/cpp)
target_compile_definitions(files_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(files_test PRIVATE mosa_lib)
add_test(NAME files_test COMMAND $<TARGET_FILE:files_test>)
add_executable(consult_cycle_test tests/cpp/consult_cycle_test.cpp)
target_include_directories(consult_cycle_test PRIVATE tests/cpp)
target_compile_definitions(consult_cycle_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(consult_cycle_test PRIVATE mosa_lib)
add_test(NAME consult_cycle_test COMMAND $<TARGET_FILE:consult_cycle_test>)

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

add_executable(suite_halt_test tests/cpp/suite_halt_test.cpp)
target_include_directories(suite_halt_test PRIVATE tests/cpp src/machine)
target_compile_definitions(suite_halt_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_halt_test PRIVATE mosa_lib)
add_test(NAME suite_halt_test COMMAND $<TARGET_FILE:suite_halt_test>)

# レビュー 2026-11-20 の H-41。tests/pl の error(Formal) は、投げた error(F, _) の F が Formal の具体化かで見る。
add_executable(suite_exam_test tests/cpp/suite_exam_test.cpp)
target_include_directories(suite_exam_test PRIVATE tests/cpp)
target_compile_definitions(suite_exam_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(suite_exam_test PRIVATE mosa_lib)
add_test(NAME suite_exam_test COMMAND $<TARGET_FILE:suite_exam_test>)

# レビュー 2026-11-20 の H-42。mosatest --diff は、足した節が user に入らないモジュールのファイルを組み立てない。
add_executable(diff_module_test tests/cpp/diff_module_test.cpp)
target_include_directories(diff_module_test PRIVATE tests/cpp src/machine)
target_compile_definitions(diff_module_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(diff_module_test PRIVATE mosa_lib)
add_test(NAME diff_module_test COMMAND $<TARGET_FILE:diff_module_test>)

# tests/tmp/suite は tests.lgt を流す全てが共有する（lgt_io_context のスクラッチ）。空かどうかを見る
# suite_path_test と conformance_io_test が、ほかの実行中スクラッチで ctest -j のとき落ちていたので、
# 流す側すべてに同じロックを付ける（diff_conformance のロックは tests/mosac_tests.cmake で付く。全体レビュー推奨5）。
set_tests_properties(assert_shared suite_assert_test suite_if_test suite_imports_test suite_approx_test
  suite_path_test suite_vars_test suite_encoding_test suite_tester_test suite_judge_test suite_count_test suite_option_test suite_cond_test suite_super_test conformance_report_test
  conformance_io_test
  conformance_output_test conformance_badge_test conformance_cwd_test
  PROPERTIES RESOURCE_LOCK suite_scratch)

add_executable(conformance_scope_test tests/cpp/conformance_scope_test.cpp)
target_include_directories(conformance_scope_test PRIVATE tests/cpp src/machine)
target_link_libraries(conformance_scope_test PRIVATE mosa_lib)
add_test(NAME conformance_scope_test COMMAND $<TARGET_FILE:conformance_scope_test>)
