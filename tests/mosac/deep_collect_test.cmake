# 入れ子の収集を mosac の実行ファイルでも 1 MiB のスタックで 100,000 段動かす（ADR 0013、レビュー H4）。
# 原文は tests/stack/deep_collect.pl。翻訳とコンパイルは既定のスタックで行い、実行ファイルだけを
# small_stack.cmake で動かす（コンパイラを 1 MiB で動かさないため）。
# 使い方: cmake -DMOSAC=<mosac> -DSRC=<tests> -DWORK=<一時ディレクトリ> -DCXX=<コンパイラ。空なら mosac の既定>
#   -DMOSA_LIB=<lib/> -DEXE_SUFFIX=<.exe など> -P deep_collect_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

file(COPY "${SRC}/stack/deep_collect.pl" DESTINATION "${WORK}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" build deep_collect.pl
  -o deep_collect_exe
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 1200)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "deep_collect: mosac build: code=[${code}] out=[${out}] err=[${err}]")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" "-DCOMMAND=${WORK}/deep_collect_exe${EXE_SUFFIX}"
  "-DEXPECTED=${SRC}/stack/deep_collect.expected" "-DWORK=${WORK}" -P "${SRC}/stack/small_stack.cmake"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 900)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "deep_collect: executable under 1 MiB: out=[${out}] err=[${err}]")
endif()
