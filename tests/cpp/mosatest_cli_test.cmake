# レビュー L-42。mosatest の入口の確かめ。どれも試験を流さない。
# - --version は先頭の位置だけで見る（mosac と同じ）。副命令の後ろの --version は副命令の引数で、使い方の誤りになる。
# - --help は使い方を標準出力に出して 0（mosac と同じ）。一つきりの --suite、--roundtrip、知らない - の旗は使い方の誤りで 1。
#   前はどれも試験のファイルの名前と取り、「開けません」で 1 になった。
# - --roundtrip のディレクトリと無いファイルは「開けません」で 1。前のディレクトリは空の原文として黙って 0 だった。
# 使い方: cmake -DMOSATEST=<mosatest> -DWORK=<一時ディレクトリ> -P mosatest_cli_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
# 文面の言語は LANG で決まる（manual_fixture_test.cmake と同じ）。
set(ENV{LANG} "en_US.UTF-8")

macro(mosatest)
  execute_process(COMMAND "${MOSATEST}" ${ARGN} WORKING_DIRECTORY "${WORK}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120 ENCODING UTF-8)
  string(REPLACE "\r\n" "\n" out "${out}")
  string(REPLACE "\r\n" "\n" err "${err}")
endmacro()

function(fail what)
  message(FATAL_ERROR "mosatest_cli: ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

set(usage "^mosatest: usage: mosatest --roundtrip <file>\n")

mosatest(--version)
if(NOT code EQUAL 0 OR NOT out MATCHES "^mosa [0-9]")
  fail("--version must print the version and exit 0")
endif()
mosatest(--suite --version)
if(NOT code EQUAL 1 OR NOT err MATCHES "${usage}" OR NOT out STREQUAL "")
  fail("--version after a subcommand must be a usage error")
endif()
mosatest(--help)
if(NOT code EQUAL 0 OR NOT out MATCHES "${usage}" OR NOT err STREQUAL "")
  fail("--help must print the usage to stdout and exit 0")
endif()
foreach(lone --suite --roundtrip --bogus)
  mosatest(${lone})
  if(NOT code EQUAL 1 OR NOT err MATCHES "${usage}")
    fail("${lone} alone must print the usage and exit 1")
  endif()
endforeach()
file(MAKE_DIRECTORY "${WORK}/dir")
mosatest(--roundtrip dir)
if(NOT code EQUAL 1 OR NOT err MATCHES "dir: cannot open file")
  fail("--roundtrip on a directory must say it cannot open it and exit 1")
endif()
mosatest(--roundtrip missing.pl)
if(NOT code EQUAL 1 OR NOT err MATCHES "missing.pl: cannot open file")
  fail("--roundtrip on a missing file must say it cannot open it and exit 1")
endif()
# レビュー N-08。--roundtrip の op/3 の優先度は op/3 と同じく 0〜1200 に限る。前は int に切り詰め、4294967496 を
# 200 と取って 0 で終えた。
file(WRITE "${WORK}/n08.pl" "% case: big_priority\n:- op(4294967496, xfx, foo).\na foo b.\n")
mosatest(--roundtrip n08.pl)
if(code EQUAL 0)
  fail("--roundtrip must reject an op/3 priority above 1200")
endif()
# レビュー R-N08。--suite のディレクトリも - で始まれば旗の書き損じとして使い方の誤りにする（--roundtrip のファイルと
# 同じ）。前は -x をディレクトリと取り、何も言わずに 1 で終えた。
mosatest(--suite -x --report r.md)
if(NOT code EQUAL 1 OR NOT err MATCHES "${usage}" OR EXISTS "${WORK}/r.md")
  fail("--suite with a directory that starts with - must be a usage error")
endif()
file(REMOVE_RECURSE "${WORK}")
