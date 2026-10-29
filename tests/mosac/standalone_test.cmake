# 計画 Task 10。mosac build で作った実行ファイルが、lib/ も原文も無いディレクトリで、MOSA_LIB 無しに動くことを確かめる
# （SPEC 第6節「実行ファイルは起動時に外部ファイルを一切要求しない」、ADR 0009 Review Focus 5）。
# 原文は WORK/src に写してから build し、動かす前に消す。実行ファイルに翻訳した機械のパスが残らないことも見る。
# 使い方: cmake -DMOSAC=<mosac> -DCASE=<uncaught_message|use_lists|include> -DSRC=<tests/mosac> -DWORK=<一時ディレクトリ>
#   -DCXX=<コンパイラ。空なら mosac の既定> -DEXE_SUFFIX=<.exe など> -P standalone_test.cmake
# CTest は MOSA_LIB を外し（ENVIRONMENT_MODIFICATION）、LANG=ja_JP.UTF-8 を与える。

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/src" "${WORK}/run")

# 生成コードは警告なしで通ること（mosac build は MOSA_WERROR=1 のときだけ警告を誤りにする）。
set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

if(CASE STREQUAL "uncaught_message")
  set(sources raises.pl)
  set(program raises)
elseif(CASE STREQUAL "use_lists")
  set(sources uses_lists.pl)
  set(program uses_lists)
elseif(CASE STREQUAL "include")
  set(sources inc.pl a.pl b.pl)
  set(program inc)
else()
  message(FATAL_ERROR "unknown case ${CASE}")
endif()
foreach(source ${sources})
  file(COPY "${SRC}/${source}" DESTINATION "${WORK}/src")
endforeach()

function(fail what)
  message(FATAL_ERROR "${CASE}: ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

# mosac は WORK/src で動かす。翻訳のとき library(_) は mosac の隣の lib/ から読む（MOSA_LIB は外してある）。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=MOSA_LIB ${compiler_env}
    "${MOSAC}" build "${program}.pl" -o "${WORK}/run/${program}"
  WORKING_DIRECTORY "${WORK}/src"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
set(exe "${WORK}/run/${program}${EXE_SUFFIX}")
if(NOT code EQUAL 0 OR NOT EXISTS "${exe}")
  fail("build")
endif()

# 原文を消し、実行ファイルだけのディレクトリで動かす。隣に lib/ も .pl も無い。
file(REMOVE_RECURSE "${WORK}/src")
file(GLOB leftovers "${WORK}/run/*")
list(REMOVE_ITEM leftovers "${exe}")
if(leftovers OR IS_DIRECTORY "${WORK}/run/lib")
  set(code "-")
  set(out "${leftovers}")
  set(err "")
  fail("the run directory must hold only the executable")
endif()

# 翻訳した機械のパス（原文を置いた WORK/src）が実行ファイルに残っていてはならない（Task 5 の持ち越し）。
file(STRINGS "${exe}" leaked REGEX "src/${program}\\.pl")
if(leaked)
  set(code "-")
  set(out "${leaked}")
  set(err "")
  fail("the executable must not embed the translating machine's paths")
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E env --unset=MOSA_LIB "LANG=ja_JP.UTF-8" "${exe}"
  WORKING_DIRECTORY "${WORK}/run"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 60)
string(REPLACE "\r\n" "\n" out "${out}")
string(REPLACE "\r\n" "\n" err "${err}")

if(CASE STREQUAL "uncaught_message")
  # 捕まえなかった例外は文面を出して 2（SPEC 第6節）。文面は同梱した messages_ja から来る。
  if(NOT code EQUAL 2 OR NOT err MATCHES "^警告: ゴールは例外を送出しました: " OR NOT out STREQUAL "")
    fail("uncaught exception message")
  endif()
elseif(CASE STREQUAL "use_lists")
  if(NOT code EQUAL 0 OR NOT out STREQUAL "[a,b]\n")
    fail("library(lists) in a standalone executable")
  endif()
else()
  if(NOT code EQUAL 0 OR NOT out STREQUAL "1-2\n")
    fail("include/1 and ensure_loaded/1 from the embedded units")
  endif()
endif()
