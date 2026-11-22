# レビュー H-31。64 KiB を超えるアトムの名前が C++ の 1 つの文字列の字面になり、MOSA_WERROR=1 の mosac build が
# -Woverlength-strings で落ちていた。kAtoms（データのアトム）、kCalls（翻訳しない動的述語の名前）、kEntries（モジュールの名前）
# のそれぞれに 70,000 バイトの名前を置き、mosa と mosac run の出力を比べる。日本語 2,000 字（6,000 バイト）のアトムは、
# 64 KiB より短いがエスケープで 24,000 文字の字面になっていた。手元の Clang は通すが、MSVC の字面の上限は CI でしか見られない。
# 長い述語の名前は C++ の識別子にもなり、MSVC は 2048 文字までしか区別しない（open-items）ので、ここでは使わない。
# 原文はここで作り、tests/mosac には 200 KB の原文を置かない。
# 使い方: cmake -DMOSA=<mosa> -DMOSAC=<mosac> -DWORK=<一時ディレクトリ> -DCXX=<コンパイラ。空なら mosac の既定>
#   -DMOSA_LIB=<lib/> -P long_atom_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

function(fail what)
  message(FATAL_ERROR "long_atom: ${what}: code=[${code}] err=[${err}]")
endfunction()

# データのアトムは引用符と \ と ? を含む（字面のエスケープと char の字面の両方を通す）。原文の '' は '、\\ は \ になる。
string(REPEAT "a" 69996 data)
string(APPEND data "''\\\\??")
string(REPEAT "c" 70000 callee)
string(REPEAT "m" 70000 module)
string(REPEAT "日" 2000 japanese)

file(WRITE "${WORK}/long_module.pl" ":- module('${module}', [q/1]).\nq(ok).\n")
file(WRITE "${WORK}/long_atom.pl" ":- initialization(main, main).\n"
  ":- use_module(long_module).\n"
  ":- dynamic('${callee}'/1).\n"
  "'${callee}'(yes).\n"
  "p('${data}').\n"
  "p('${japanese}').\n"
  "r('${data}', hit).\n"
  "main :- p(X), atom_length(X, N), sub_atom(X, _, 4, 0, S), writeq(N-S), nl, fail.\n"
  "main :- p(X), r(X, H), write(H), nl, fail.\n"
  "main :- q(Q), write(Q), nl, '${callee}'(Y), write(Y), nl, atom_length('${callee}', N), write(N), nl.\n")

# ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "${MOSA}" long_atom.pl
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE expected ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120 ENCODING UTF-8)
set(want "70000-'\\'\\\\??'\n2000-日日日日\nhit\nok\nyes\n70000\n")
string(REPLACE "\r\n" "\n" expected_lf "${expected}")
if(NOT code EQUAL 0 OR NOT expected_lf STREQUAL want)
  fail("mosa must run the program and print [${want}], printed [${expected}]")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" run long_atom.pl
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 1200 ENCODING UTF-8)
if(NOT code EQUAL 0)
  fail("mosac run")
endif()
if(NOT out STREQUAL expected)
  fail("the translated program must print what mosa prints: [${out}], mosa [${expected}]")
endif()
