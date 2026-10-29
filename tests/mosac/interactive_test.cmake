# 計画 Task 14。mosac build --interactive で作った実行ファイルが、mosa と同じ REPL で同じ答えの並びを出すことを確かめる。
# 基準は repl_ref（mosa と同じ run_session。端末でなくても REPL を動かす）に同じ原文と同じ入力を流した出力である。
# 入力は app/3 の全部の答え（; で次へ）と、動的な述語への assertz、翻訳済みの静的な app/3 への assertz（mosa と同じ
# permission_error になる）を含む。
# 使い方: cmake -DMOSAC=<mosac> -DREF=<repl_ref> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ>
#   -DCXX=<コンパイラ。空なら mosac の既定> -DEXE_SUFFIX=<.exe など> -P interactive_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${SRC}/interactive.pl" "${SRC}/interactive.in" DESTINATION "${WORK}")

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
