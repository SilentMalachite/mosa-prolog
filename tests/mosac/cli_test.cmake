# mosac の入口の確かめ（最終レビューの直し 1 と 5）。どれもコンパイラを起こさない。
# - 引数無し、知らない副命令は使い方を標準エラーに出して 1、--help は標準出力に出して 0。dist はフェーズ7の文面で 1。
# - -o の無い build で出力が原文そのものになる（拡張子の無い原文 hello、Windows では hello.exe）なら、何も作らず
#   -o を促して 1 で止まり、原文はそのまま残る。
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

# 原文 hello（Windows では hello.exe）の語幹 hello に実行ファイルの接尾辞を付けると原文そのものになる。
set(source "hello${EXE_SUFFIX}")
file(COPY_FILE "${SRC}/hello.pl" "${WORK}/${source}")
file(SHA256 "${WORK}/${source}" before)
mosac(build "${source}")
if(NOT code EQUAL 1 OR NOT err MATCHES "^mosac: 出力 [^\n]*hello[^\n]* は原文のファイルそのもので[^\n]*\n-o で別の出力名")
  fail("build without -o must refuse to overwrite the source")
endif()
file(SHA256 "${WORK}/${source}" after)
if(NOT before STREQUAL after)
  fail("the source must stay unchanged")
endif()
if(IS_DIRECTORY "${WORK}/.mosac-cache")
  fail("the refusal must come before translating and compiling")
endif()
# -o で同じファイルを指しても同じく止まる（./ を付けた別の綴りでも）。
mosac(build "${source}" -o "./${source}")
if(NOT code EQUAL 1 OR NOT err MATCHES "^mosac: 出力 ")
  fail("-o naming the source must be refused too")
endif()
