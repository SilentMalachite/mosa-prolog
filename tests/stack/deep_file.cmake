# mosac build で作った実行ファイルが、データのファイルの 100,000 段の f(f(...)) を 1 MiB のスタックで read/1 で読む
# （ADR 0011 改訂 4）。翻訳器の emit_body と check は再帰のまま（改訂 2）なので、深い項はプログラムの原文に置かず、
# 実行時にデータのファイルから読ませる。翻訳とコンパイルは既定のスタックで行い、実行ファイルだけを small_stack.cmake
# で動かす（コンパイラを 1 MiB で動かさないため）。大きな原文は tests に置かず、ここで WORK に作る。
# 使い方: cmake -DMOSAC=<mosac> -DWORK=<一時ディレクトリ> -DCXX=<コンパイラ。空なら mosac の既定> -DMOSA_LIB=<lib/>
#   -DEXE_SUFFIX=<.exe など> -DSMALL_STACK=<small_stack.cmake> -P deep_file.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

function(fail what)
  message(FATAL_ERROR "deep_file: ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

string(REPEAT "f(" 100000 opens)
string(REPEAT ")" 100000 closes)
file(WRITE "${WORK}/deep_file.txt" "${opens}a${closes}.\n")
# 深さは最後の呼び出しの繰り返しで数える。数えるほうが再帰すると、読みでなく数えで溢れる。
file(WRITE "${WORK}/deep_file.pl" ":- initialization(main, main).
depth(f(X), N0, N) :- !, N1 is N0 + 1, depth(X, N1, N).
depth(_, N, N).
main :- open('deep_file.txt', read, S), read(S, T), close(S), depth(T, 0, N), write(depth:N), nl.
")
file(WRITE "${WORK}/deep_file.expected" "depth:100000\n")

# ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" build deep_file.pl
  -o deep_file_exe
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600 ENCODING UTF-8)
if(NOT code EQUAL 0)
  fail("mosac build")
endif()

set(COMMAND "${WORK}/deep_file_exe${EXE_SUFFIX}")
set(EXPECTED "${WORK}/deep_file.expected")
include("${SMALL_STACK}")
