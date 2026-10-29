# 2 万要素のリストの原文（最終レビューの直し 3）。'.'/2 の尾が 2 万段入れ子になる項を、翻訳器が C++ の呼び出しを
# 積まずにたどれること。mosa が読める原文は mosac も翻訳し、ビルドして、同じ答えを出すこと。事実の頭部と、本体の
# X = [...] の両方の形を見る。原文はここで作り、tests/mosac には 100 KB の原文を置かない。
# 使い方: cmake -DMOSA=<mosa> -DMOSAC=<mosac> -DWORK=<一時ディレクトリ> -DCXX=<コンパイラ。空なら mosac の既定>
#   -DMOSA_LIB=<lib/> -P long_list_test.cmake

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

function(fail what)
  message(FATAL_ERROR "long_list: ${what}: code=[${code}] err=[${err}]")
endfunction()

# 原文 long_list.pl を書く。要素は 0 から count - 1。
function(write_program count)
  set(items "0")
  math(EXPR last "${count} - 1")
  foreach(index RANGE 1 ${last})
    string(APPEND items ",${index}")
  endforeach()
  file(WRITE "${WORK}/long_list.pl" ":- initialization(main, main).\n"
    "main :- data(L), body_data(L), length(L, N), write(N), nl, writeq(L), nl.\n"
    "data([${items}]).\n"
    "body_data(X) :- X = [${items}].\n")
endfunction()

# 確かめの前提は「mosa が読める原文は mosac も翻訳できる」こと。macOS と Linux の主スレッドのスタック（8 MiB）では
# mosa が 2 万要素を読む。Windows の既定の 1 MiB ではインタプリタの読み込み自身が先に溢れる（CI run 40926240394。
# 読み込み器の再帰はフェーズ7の項目）ので、Windows だけは mosa が読める最大の長さまで下げ、その長さで比べる。
set(counts 20000)
if(CMAKE_HOST_WIN32)
  set(counts 20000 10000 5000 2000 1000)
endif()
set(chosen "")
foreach(count IN LISTS counts)
  write_program(${count})
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env "MOSA_LIB=${MOSA_LIB}" "${MOSA}" long_list.pl
    WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE expected ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
  if(code EQUAL 0 AND expected MATCHES "^${count}\r?\n\\[0,1,2,")
    set(chosen ${count})
    break()
  endif()
  message(STATUS "long_list: mosa cannot load ${count} elements here: code=[${code}]")
endforeach()
if(chosen STREQUAL "")
  fail("mosa must run the program")
endif()
message(STATUS "long_list: comparing mosa and mosac run with ${chosen} elements")

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" emit long_list.pl -o emitted
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120)
if(NOT code EQUAL 0)
  fail("mosac emit")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${compiler_env} "MOSA_LIB=${MOSA_LIB}" "${MOSAC}" run long_list.pl
  WORKING_DIRECTORY "${WORK}" OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 1200)
if(NOT code EQUAL 0)
  fail("mosac run")
endif()
if(NOT out STREQUAL expected)
  string(LENGTH "${out}" got)
  string(LENGTH "${expected}" want)
  fail("the translated program must print what mosa prints (${got} bytes, mosa ${want} bytes)")
endif()
