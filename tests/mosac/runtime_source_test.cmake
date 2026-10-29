# 計画 Task 10（直し 1）。mosac build --runtime-source を同じ場所で 3 回動かす。1 回目は同梱の源から実行時ライブラリを
# 作り、2 回目は作り直さずにキャッシュの物を使い、3 回目は --clean で消してから作り直す。鍵は源の 4 か所
# （CMakeLists.txt、src/、include/、lib/）だけから取るので、ビルド木の形（mosac の親がリポジトリの根）でも
# .mosac-cache 自身や build/ の変化で外れない。libmosa を 2 回作るので遅い。tests.cmake は MOSA_SLOW_TESTS=ON の
# ときだけ登録する。
# 使い方: cmake -DMOSAC=<mosac> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ> -DCXX=<コンパイラ。空なら mosac の既定>
#   -DEXE_SUFFIX=<.exe など> -P runtime_source_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
file(COPY "${SRC}/uses_lists.pl" DESTINATION "${WORK}")

set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

function(fail what)
  message(FATAL_ERROR "runtime_source ${round}: ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

set(building "mosac: 実行時ライブラリを同梱の源から作ります: ")
set(reusing "mosac: 源から作った実行時ライブラリを使い回します: ")
# third は --clean を添える。.mosac-cache を消してから実行時ライブラリを作り直し、それで結べること（最終レビューの直し 2）。
foreach(round first second third)
  set(clean)
  if(round STREQUAL "third")
    set(clean --clean)
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=MOSA_LIB ${compiler_env}
      "${MOSAC}" build --runtime-source --verbose ${clean} uses_lists.pl -o "${WORK}/uses_lists"
    WORKING_DIRECTORY "${WORK}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 1800)
  string(REPLACE "\r\n" "\n" err "${err}")
  if(NOT code EQUAL 0)
    fail("build --runtime-source")
  endif()
  if(NOT round STREQUAL "second" AND NOT err MATCHES "${building}")
    fail("the ${round} run must build the runtime from the bundled source")
  endif()
  if(round STREQUAL "second" AND (err MATCHES "${building}" OR NOT err MATCHES "${reusing}"))
    fail("the second run must reuse the cached runtime")
  endif()
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=MOSA_LIB "${WORK}/uses_lists${EXE_SUFFIX}"
  WORKING_DIRECTORY "${WORK}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 60)
string(REPLACE "\r\n" "\n" out "${out}")
if(NOT code EQUAL 0 OR NOT out STREQUAL "[a,b]\n")
  fail("the executable linked against the runtime built from source")
endif()
