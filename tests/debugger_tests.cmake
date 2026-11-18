# tests/tests.cmake から include する。フェーズ 14 のデバッガ（ADR 0028）のテスト。tests.cmake が 800 行に近づいたので
# 分けた（PR #4 の敵対的レビューの Low の直しの前）。名前は ci.yml の正規表現が見るので変えない。

# フェーズ 14 のデバッガのポートの行（ADR 0028）。mosa に dbg.pl（creep_ja は ja.pl、redo_nested は nested.pl）を読ませる。
# 根で動かすのは、.in の中の相対パスをリポジトリの根から開くため。
foreach(debug_case creep creep_ja more fail library reload redo_nested builtin_inner exception skip leap retry
    force_fail nodebug leash leap_library leap_late retry_catch exception_retry exception_fail fail_retry
    abort abort_load abort_catch abort_nested_list abort_nested_goal goals write spy_toggle help eof)
  set(debug_lang en_US.UTF-8)
  set(debug_program dbg.pl)
  if(debug_case STREQUAL "creep_ja")
    set(debug_lang ja_JP.UTF-8)
    set(debug_program ja.pl)
  elseif(debug_case STREQUAL "redo_nested")
    set(debug_program nested.pl)
  endif()
  add_test(NAME transcript_debug_${debug_case} COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
    -DDATA=${CMAKE_SOURCE_DIR}/tests/cpp/data/debugger -DCASE=debug_${debug_case}
    -DPROGRAM=${CMAKE_SOURCE_DIR}/tests/cpp/data/debugger/${debug_program} -DLANG_VALUE=${debug_lang}
    -P ${CMAKE_SOURCE_DIR}/tests/cpp/transcript_test.cmake WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()
# 端末でない mosa はコマンドを読まず、どのポートも止まらない行として書く（MOSA_TRANSCRIPT なし、設計の「コマンド」）。
add_test(NAME transcript_debug_batch COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DDATA=${CMAKE_SOURCE_DIR}/tests/cpp/data/debugger -DCASE=debug_batch -DTRANSCRIPT=0 -DLANG_VALUE=en_US.UTF-8
  "-DARGS=tests/cpp/data/debugger/dbg.pl -g \"trace, len([a], N)\" -t halt"
  -P ${CMAKE_SOURCE_DIR}/tests/cpp/transcript_test.cmake WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# 追跡しても答えと選択点の深さが変わらず、DebugConsole の無い機械は箱を作らない（ADR 0028）。
add_executable(debugger_test tests/cpp/debugger_test.cpp)
target_include_directories(debugger_test PRIVATE tests/cpp src)
target_link_libraries(debugger_test PRIVATE mosa_lib)
add_test(NAME debugger COMMAND $<TARGET_FILE:debugger_test>)
# PR #4 の敵対的レビュー。トレースの時間が回数に比例する範囲に収まる。ほかの scale と同じくひとりで走らせる。
add_executable(debugger_scale_test tests/cpp/debugger_scale_test.cpp)
target_include_directories(debugger_scale_test PRIVATE tests/cpp src)
target_link_libraries(debugger_scale_test PRIVATE mosa_lib)
add_test(NAME debugger_scale COMMAND $<TARGET_FILE:debugger_scale_test>)
set_tests_properties(debugger_scale PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(debugger PROPERTIES ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")
