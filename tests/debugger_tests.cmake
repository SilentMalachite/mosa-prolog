# tests/tests.cmake から include する。フェーズ 14 のデバッガ（ADR 0028）のテスト。tests.cmake が 800 行に近づいたので
# 分けた（PR #4 の敵対的レビューの Low の直しの前）。名前は ci.yml の正規表現が見るので変えない。

# フェーズ 14 のデバッガのポートの行（ADR 0028）。mosa に dbg.pl（creep_ja と spy_ja は ja.pl、redo_nested は nested.pl、
# spy_after_warning と spy_after_warning_ja は plain.pl）を読ませる。read_line は read/1 が返した改行をポートの
# コマンドに読まないことを見る（敵対的レビュー、2026-11-19）。skip_rollback は skip_rollback.pl を consult し、取り消した
# 読み込みが外の skip の箱を戻すことを見る（同じレビュー）。
# redo_r_catch と redo_f_catch は再試行のポートの r と f が箱の中の後始末を動かしても、外の catch/3 がその例外を捕らえる
# ことを見る（レビュー 2026-11-20 の H-07）。期待値は同じ入力の r と f を creep にした出力で、そこだけ違う。
# redo_inner_catch は、再試行のポートが箱の中で抜けた catch/3 を戻さず、やり直しの後の例外を外の catch/3 が受けることを見る。
# spy_alias は alias.pl を読み、as の別名で取り込んだ述語のスパイポイントが定義の述語に付くことを見る（レビュー H-14）。
# 根で動かすのは、.in の中の相対パスをリポジトリの根から開くため。
set(debug_cases creep creep_ja more fail library reload redo_nested builtin_inner exception skip leap retry
    force_fail nodebug leash leap_library leap_late retry_catch exception_retry exception_fail fail_retry
    abort abort_load abort_catch abort_nested_list abort_nested_goal goals write spy_toggle spy_forms spy_after_warning
    spy_after_warning_ja directive_depth spy_ja help eof read_line skip_rollback redo_r_catch redo_f_catch
    redo_inner_catch spy_alias)
foreach(debug_case ${debug_cases})
  set(debug_lang en_US.UTF-8)
  set(debug_program dbg.pl)
  if(debug_case STREQUAL "creep_ja" OR debug_case STREQUAL "spy_ja")
    set(debug_lang ja_JP.UTF-8)
    set(debug_program ja.pl)
  elseif(debug_case STREQUAL "redo_nested")
    set(debug_program nested.pl)
  elseif(debug_case STREQUAL "spy_alias")
    set(debug_program alias.pl)
  elseif(debug_case STREQUAL "spy_after_warning" OR debug_case STREQUAL "spy_after_warning_ja")
    # 警告の文面を作っても library(lists) が読まれないことを見るので、ライブラリを読まない plain.pl にする（ADR 0007 の改訂 3）。
    # _ja は日本語の表で同じことを見る（Muse のレビュー）。
    set(debug_program plain.pl)
    if(debug_case STREQUAL "spy_after_warning_ja")
      set(debug_lang ja_JP.UTF-8)
    endif()
  endif()
  add_test(NAME transcript_debug_${debug_case} COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
    -DDATA=${CMAKE_SOURCE_DIR}/tests/cpp/data/debugger -DCASE=debug_${debug_case}
    -DPROGRAM=${CMAKE_SOURCE_DIR}/tests/cpp/data/debugger/${debug_program} -DLANG_VALUE=${debug_lang}
    -P ${CMAKE_SOURCE_DIR}/tests/cpp/transcript_test.cmake WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()
# 端末でない mosa はコマンドを読まず、どのポートも止まらない行として書く（MOSA_TRANSCRIPT なし、設計の「コマンド」）。
# ポートの行は stderr、答えは stdout に出るので、分けて比べる（SPLIT=1）。
add_test(NAME transcript_debug_batch COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DDATA=${CMAKE_SOURCE_DIR}/tests/cpp/data/debugger -DCASE=debug_batch -DTRANSCRIPT=0 -DSPLIT=1 -DLANG_VALUE=en_US.UTF-8
  "-DARGS=tests/cpp/data/debugger/dbg.pl -g \"trace, len([a], N)\" -t halt"
  -P ${CMAKE_SOURCE_DIR}/tests/cpp/transcript_test.cmake WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# a が動かす後始末がヒープの上限で止まっても、後の指令の失敗を abort と書かない（PR #4 の 2 度目の敵対的レビュー）。
# 指令は問い合わせと違い drop_run を通らないので、印が残ると間の指令を越えて届いた。
add_test(NAME transcript_debug_abort_heap_limit COMMAND ${CMAKE_COMMAND} -DMOSA=$<TARGET_FILE:mosa>
  -DDATA=${CMAKE_SOURCE_DIR}/tests/cpp/data/debugger -DCASE=debug_abort_heap_limit -DLANG_VALUE=en_US.UTF-8
  "-DARGS=--heap-max 16"
  -P ${CMAKE_SOURCE_DIR}/tests/cpp/transcript_test.cmake WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
# data/debugger の debug_*.in はどれも上で登録し、.out を持つ。一覧に足し忘れた組は黙って走らないので、構成で止める
# （Muse のレビュー）。
set(debug_data ${CMAKE_SOURCE_DIR}/tests/cpp/data/debugger)
file(GLOB debug_inputs CONFIGURE_DEPENDS RELATIVE ${debug_data} ${debug_data}/debug_*.in)
foreach(debug_input ${debug_inputs})
  string(REGEX REPLACE "^debug_(.*)\\.in$" "\\1" debug_name "${debug_input}")
  if(NOT debug_name IN_LIST debug_cases AND NOT debug_name STREQUAL "batch" AND NOT debug_name STREQUAL "abort_heap_limit")
    message(FATAL_ERROR "tests/cpp/data/debugger/${debug_input} is not registered in tests/debugger_tests.cmake")
  endif()
  if(NOT EXISTS ${debug_data}/debug_${debug_name}.out)
    message(FATAL_ERROR "tests/cpp/data/debugger/${debug_input} has no debug_${debug_name}.out")
  endif()
endforeach()

# 追跡しても答えと選択点の深さが変わらず、DebugConsole の無い機械は箱を作らない（ADR 0028）。
add_executable(debugger_test tests/cpp/debugger_test.cpp)
target_include_directories(debugger_test PRIVATE tests/cpp src)
target_link_libraries(debugger_test PRIVATE mosa_lib)
add_test(NAME debugger COMMAND $<TARGET_FILE:debugger_test>)
# デバッガの a が問い合わせを止め、後始末を動かす（ADR 0028）。debugger_test から分けた。
add_executable(debugger_abort_test tests/cpp/debugger_abort_test.cpp)
target_include_directories(debugger_abort_test PRIVATE tests/cpp src)
target_link_libraries(debugger_abort_test PRIVATE mosa_lib)
add_test(NAME debugger_abort COMMAND $<TARGET_FILE:debugger_abort_test>)
# PR #4 の敵対的レビュー。トレースの時間が回数に比例する範囲に収まる。ほかの scale と同じくひとりで走らせる。
add_executable(debugger_scale_test tests/cpp/debugger_scale_test.cpp)
target_include_directories(debugger_scale_test PRIVATE tests/cpp src)
target_link_libraries(debugger_scale_test PRIVATE mosa_lib)
add_test(NAME debugger_scale COMMAND $<TARGET_FILE:debugger_scale_test>)
set_tests_properties(debugger_scale PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(debugger debugger_abort PROPERTIES ENVIRONMENT "MOSA_LIB=${CMAKE_SOURCE_DIR}/lib")
