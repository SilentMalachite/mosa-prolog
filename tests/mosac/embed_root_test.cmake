# レビュー M-48。実行ファイルは埋め込んだ原文の鍵を今の作業場所から絶対にする。主のファイルのディレクトリからの相対の
# 鍵 '../x.pl' と '../../x.pl' は、/ から動かすと同じパスになり、片方の原文を 2 度読んだ。今は鍵をプログラムのファイルの
# 共通の祖先から作るので、どの作業場所から動かしても mosa と同じ答えを出す。
# 使い方: cmake -DMOSA=<mosa> -DMOSAC=<mosac> -DWORK=<一時ディレクトリ> -DCXX=<コンパイラ。空なら mosac の既定>
#   -DMOSA_LIB=<lib/> -DEXE_SUFFIX=<.exe など> -P embed_root_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/a/b/c")
set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()
set(main_dir "${WORK}/a/b/c")
file(WRITE "${WORK}/a/b/x.pl" "fact(from_b).\n")
file(WRITE "${WORK}/a/x.pl" "fact(from_a).\n")
file(WRITE "${main_dir}/main.pl" ":- initialization(main, main).\n:- include('../x.pl').\n:- include('../../x.pl').\n"
  "main :- forall(fact(X), (write(X), nl)).\n")

function(fail what)
  message(FATAL_ERROR "embed_root: ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

# ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "${MOSA}" main.pl
  WORKING_DIRECTORY "${main_dir}" OUTPUT_VARIABLE expected ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120 ENCODING UTF-8)
string(REPLACE "\r\n" "\n" expected "${expected}")
if(NOT code EQUAL 0 OR NOT expected STREQUAL "from_b\nfrom_a\n")
  set(out "${expected}")
  fail("mosa must print from_b and from_a")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" build main.pl -o app
  WORKING_DIRECTORY "${main_dir}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 1200 ENCODING UTF-8)
if(NOT code EQUAL 0)
  fail("mosac build")
endif()

# 主のディレクトリと、ファイルシステムの根（/、Windows ならその駆動器の根）から動かす。
foreach(cwd "${main_dir}" "/")
  execute_process(COMMAND "${main_dir}/app${EXE_SUFFIX}" WORKING_DIRECTORY "${cwd}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120 ENCODING UTF-8)
  string(REPLACE "\r\n" "\n" out "${out}")
  if(NOT code EQUAL 0 OR NOT out STREQUAL expected)
    fail("the executable run from ${cwd} must print what mosa prints")
  endif()
endforeach()
