# mosac の入口の確かめ（最終レビューの直し 5）。どれもコンパイラを起こさない。
# - 引数無し、知らない副命令は使い方を標準エラーに出して 1、--help は標準出力に出して 0。dist はフェーズ7の文面で 1。
# 使い方: cmake -DMOSAC=<mosac> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ> -DEXE_SUFFIX=<.exe など>
#   -DMOSA_LIB=<lib/> -P cli_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

macro(mosac)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" ${ARGN}
    WORKING_DIRECTORY "${WORK}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
  string(REPLACE "\r\n" "\n" out "${out}")
  string(REPLACE "\r\n" "\n" err "${err}")
endmacro()

function(fail what)
  message(FATAL_ERROR "mosac_cli: ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

set(usage "^mosac: 使い方: mosac build ")

mosac()
if(NOT code EQUAL 1 OR NOT err MATCHES "${usage}" OR NOT out STREQUAL "")
  fail("no arguments must print the usage to stderr and exit 1")
endif()
mosac(--help)
if(NOT code EQUAL 0 OR NOT out MATCHES "${usage}" OR NOT err STREQUAL "")
  fail("--help must print the usage to stdout and exit 0")
endif()
mosac(frobnicate hello.pl)
if(NOT code EQUAL 1 OR NOT err MATCHES "${usage}")
  fail("an unknown subcommand must print the usage and exit 1")
endif()
mosac(dist hello.pl -o dist)
if(NOT code EQUAL 1 OR NOT err MATCHES "^mosac: mosac dist はフェーズ7まで使えません\n$")
  fail("dist must say it arrives in phase 7")
endif()
