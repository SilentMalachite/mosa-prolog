# 計画 Task 14。mosac build --interactive で作った実行ファイルが、mosa と同じ REPL で同じ答えの並びを出すことを確かめる。
# 基準は repl_ref（mosa と同じ run_session。端末でなくても REPL を動かす）に同じ原文と同じ入力を流した出力である。
# 入力は app/3 の全部の答え（; で次へ）と、動的な述語への assertz、翻訳済みの静的な app/3 への assertz（mosa と同じ
# permission_error になる）を含む。
# 使い方: cmake -DMOSAC=<mosac> -DREF=<repl_ref> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ>
#   -DCXX=<コンパイラ。空なら mosac の既定> -DEXE_SUFFIX=<.exe など> -P interactive_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${SRC}/interactive.pl" "${SRC}/interactive.in" "${SRC}/interactive_main.pl" "${SRC}/interactive_main.in"
  "${SRC}/interactive_redef.pl" "${SRC}/interactive_redef.in" DESTINATION "${WORK}")

set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

function(fail what)
  message(FATAL_ERROR "interactive: ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "${MOSAC}" build --interactive interactive.pl -o interactive
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
if(NOT code EQUAL 0 OR NOT EXISTS "${WORK}/interactive${EXE_SUFFIX}")
  fail("build --interactive")
endif()

# 基準と実行ファイルを同じ入力で動かす。改行は CRLF を LF にそろえて比べる。
macro(replay program)
  execute_process(COMMAND "${program}" ${ARGN}
    INPUT_FILE "${WORK}/interactive.in" WORKING_DIRECTORY "${WORK}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
  string(REPLACE "\r\n" "\n" out "${out}")
  string(REPLACE "\r\n" "\n" err "${err}")
endmacro()
replay("${REF}" interactive.pl)
set(want_out "${out}")
set(want_err "${err}")
if(NOT code EQUAL 0 OR NOT want_out MATCHES "X = \\[\\],[ \n]*Y = \\[1\\]" OR NOT want_out MATCHES "L = \\[1, ?2\\]")
  fail("reference run")
endif()
replay("${WORK}/interactive${EXE_SUFFIX}")
if(NOT code EQUAL 0 OR NOT out STREQUAL want_out OR NOT err STREQUAL want_err)
  message(FATAL_ERROR "interactive: output differs from mosa\n--- want ---\n${want_out}\n--- got ---\n${out}\n--- want err ---\n${want_err}\n--- got err ---\n${err}\ncode=[${code}]")
endif()

# 以下は実行ファイル側だけを見る（基準と一致するとは限らない）。
# 1. initialization(main, main) の main は動かさない（暫定。owner 未決。docs/STATUS.md）。argv は引数、halt(3) の値が終了コード。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "${MOSAC}" build --interactive interactive_main.pl -o imain
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
if(NOT code EQUAL 0)
  fail("build interactive_main")
endif()
execute_process(COMMAND "${WORK}/imain${EXE_SUFFIX}" arg1 INPUT_FILE "${WORK}/interactive_main.in" WORKING_DIRECTORY "${WORK}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
if(NOT code EQUAL 3 OR out MATCHES "MARKER-MAIN" OR NOT out MATCHES "Z = \\[a, ?b\\]" OR NOT out MATCHES "A = \\[arg1\\]")
  fail("main skipped, argv and halt(3)")
endif()

# 2. --debug の実行ファイルでは app/3 の節の追跡が標準エラーに出る。翻訳済みの節が実際に動いている印である。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "${MOSAC}" build --interactive --debug interactive_main.pl -o idebug
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
if(NOT code EQUAL 0)
  fail("build --debug")
endif()
execute_process(COMMAND "${WORK}/idebug${EXE_SUFFIX}" INPUT_FILE "${WORK}/interactive_main.in" WORKING_DIRECTORY "${WORK}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
if(NOT err MATCHES "app\\(")
  fail("translated app/3 must run natively (trace)")
endif()

# 3. consult/1 で app/3 を定義し直すと翻訳済みの単位が外れ（ADR 0009）、インタプリタの定義で答える。
execute_process(COMMAND "${WORK}/idebug${EXE_SUFFIX}" INPUT_FILE "${WORK}/interactive_redef.in" WORKING_DIRECTORY "${WORK}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
if(NOT out MATCHES "X = redefined")
  fail("consult must detach the translated app/3")
endif()
