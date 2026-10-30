# ヒープの上限（ADR 0010）。MOSA_HEAP_MAX=16 の mosac run は終了コード 2 で、標準エラーに「16 MiB」を出す。
# mosa の --heap-max 16 も同じ文面で止まり、REPL は止まったあとの次の問い合わせに答える。
# mosa の main は端末でない入力を読まないので、REPL は repl_ref（同じ run_session）で動かす。
# 使い方: cmake -DMOSA=<mosa> -DMOSAC=<mosac> -DREF=<repl_ref> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ>
#   -DCXX=<コンパイラ。空なら mosac の既定> -DMOSA_LIB=<lib/> -DEXE_SUFFIX=<.exe など> -P heap_limit_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

function(fail what)
  message(FATAL_ERROR "heap_limit: ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

# 入口の指示は、REPL 用には要らない（main. を打つ）ので、実行ファイル用の原文にだけ足す。
file(COPY "${SRC}/heap_grow.pl" DESTINATION "${WORK}")
file(READ "${SRC}/heap_grow.pl" body)
file(WRITE "${WORK}/heap_grow_main.pl" "${body}:- initialization(main, main).\n")
file(WRITE "${WORK}/repl.in" "main.\nX = 1.\n")

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "MOSA_HEAP_MAX=16"
  "${MOSAC}" run heap_grow_main.pl
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 1200)
if(NOT code EQUAL 2 OR NOT err MATCHES "16 MiB")
  fail("mosac run must stop with exit code 2 and say 16 MiB")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "${MOSA}" --heap-max 16 -g main heap_grow.pl
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
if(NOT code EQUAL 2 OR NOT err MATCHES "16 MiB")
  fail("mosa --heap-max 16 -g must stop with exit code 2 and say 16 MiB")
endif()

# 環境変数より --heap-max が先。環境は 16、オプションは 8。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "MOSA_HEAP_MAX=16" "${MOSA}" --heap-max 8 -g main heap_grow.pl
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
if(NOT code EQUAL 2 OR NOT err MATCHES "8 MiB")
  fail("--heap-max must beat MOSA_HEAP_MAX")
endif()

# 1 MiB の物差し（loop(1000000) と tak(18,12,6,A)）は、空きを生まない GC の規則（ADR 0010 の改訂 B）で止まらない。
file(COPY "${SRC}/heap_near_limit.pl" DESTINATION "${WORK}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "${MOSA}" --heap-max 1 -g yardsticks -t halt heap_near_limit.pl
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
if(NOT code EQUAL 0 OR NOT out MATCHES "tak\\(7\\)")
  fail("mosa --heap-max 1 must finish loop(1000000) and tak(18,12,6,A)")
endif()

# 上限の 96% を生かしたまま捨てる項を作り続けると、上限に当たる前に同じ文面と終了コード 2 で止まる。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "${MOSA}" --heap-max 1 -g thrash heap_near_limit.pl
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
if(NOT code EQUAL 2 OR NOT err MATCHES "1 MiB")
  fail("mosa --heap-max 1 -g thrash must stop with exit code 2 and say 1 MiB")
endif()

# 解釈できない値は使い方の誤り（終了コード 1）。
execute_process(COMMAND "${MOSA}" --heap-max 0 -g true
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 60)
if(NOT code EQUAL 1)
  fail("--heap-max 0 must be a usage error")
endif()

# 解釈できない MOSA_HEAP_MAX は既定の上限で動き、値と既定を名指す警告を 1 行だけ出す（黙って既定にしない）。
foreach(bad 512M 2GB)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "MOSA_HEAP_MAX=${bad}" "${MOSA}" -g halt
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 60)
  string(REGEX MATCHALL "MOSA_HEAP_MAX=${bad}" named "${err}")
  list(LENGTH named named_count)
  if(NOT code EQUAL 0 OR NOT named_count EQUAL 1 OR NOT err MATCHES "4096 MiB")
    fail("MOSA_HEAP_MAX=${bad} must run with the default and warn once with the value and 4096 MiB")
  endif()
endforeach()

# 予約を断られる上限は「上限に達した」ではなく、番地を予約できなかったと言い、--heap-max と MOSA_HEAP_MAX を名指す。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "${MOSA}" --heap-max 999999999 -g halt
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 60)
if(NOT code EQUAL 2 OR NOT err MATCHES "999999999 MiB" OR NOT err MATCHES "--heap-max" OR NOT err MATCHES "MOSA_HEAP_MAX")
  fail("mosa --heap-max 999999999 must exit 2 and say the address space could not be reserved")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "${REF}" --heap-max 16 heap_grow.pl
  INPUT_FILE "${WORK}/repl.in" WORKING_DIRECTORY "${WORK}"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
if(NOT err MATCHES "16 MiB" OR NOT out MATCHES "X = 1")
  fail("the REPL must report the limit and answer the next query")
endif()

# 上限を予約できない値でも、生成した実行ファイルは異常終了（134）にならず、文面を出して 2 で終わる。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" build heap_grow_main.pl -o heap_grow_exe
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 1200)
if(NOT code EQUAL 0)
  fail("mosac build")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E env "--unset=MOSA_LIB" "MOSA_HEAP_MAX=999999999" "${WORK}/heap_grow_exe${EXE_SUFFIX}"
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 60)
if(NOT code EQUAL 2 OR NOT err MATCHES "999999999 MiB" OR NOT err MATCHES "MOSA_HEAP_MAX")
  fail("an exe whose heap cannot be reserved must exit 2 and name MOSA_HEAP_MAX")
endif()
