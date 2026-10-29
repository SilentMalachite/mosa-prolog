# 計画 Task 10。mosac build で作った実行ファイルが、lib/ も原文も無いディレクトリで、MOSA_LIB 無しに動くことを確かめる
# （SPEC 第6節「実行ファイルは起動時に外部ファイルを一切要求しない」、ADR 0009 Review Focus 5）。
# 原文は WORK/src に写してから build し、動かす前に消す。実行ファイルに翻訳した機械のパスが残らないことも見る。
# 使い方: cmake -DMOSAC=<mosac> -DCASE=<uncaught_message|use_lists|include|plain_use|subdir> -DSRC=<tests/mosac>
#   -DWORK=<一時ディレクトリ> -DCXX=<コンパイラ。空なら mosac の既定> -DEXE_SUFFIX=<.exe など> -P standalone_test.cmake
# CTest は MOSA_LIB を外し（ENVIRONMENT_MODIFICATION）、LANG=ja_JP.UTF-8 を与える。

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/src" "${WORK}/run")

# 生成コードは警告なしで通ること（mosac build は MOSA_WERROR=1 のときだけ警告を誤りにする）。
set(ENV{MOSA_WERROR} 1)
set(compiler_env)
if(CXX)
  set(compiler_env "MOSA_CXX=${CXX}")
endif()

# 翻訳のとき library(_) は mosac の隣の lib/ から読む（MOSA_LIB は外してある）。plain_use だけは利用者のライブラリの
# ディレクトリを MOSA_LIB で渡し、その中の foo.pl が素のパスで util.pl を use_module する。
set(library_env --unset=MOSA_LIB)
if(CASE STREQUAL "uncaught_message")
  set(sources raises.pl)
  set(program raises)
elseif(CASE STREQUAL "use_lists")
  set(sources uses_lists.pl)
  set(program uses_lists)
elseif(CASE STREQUAL "include")
  set(sources inc.pl a.pl b.pl)
  set(program inc)
elseif(CASE STREQUAL "plain_use")
  set(sources uses_foo.pl)
  set(program uses_foo)
  file(COPY "${SRC}/mylib" DESTINATION "${WORK}/src")
  set(library_env "MOSA_LIB=${WORK}/src/mylib")
elseif(CASE STREQUAL "subdir")
  set(sources uses_bar.pl)
  set(program uses_bar)
  file(COPY "${SRC}/mylib" DESTINATION "${WORK}/src")
  set(library_env "MOSA_LIB=${WORK}/src/mylib")
else()
  message(FATAL_ERROR "unknown case ${CASE}")
endif()
foreach(source ${sources})
  file(COPY "${SRC}/${source}" DESTINATION "${WORK}/src")
endforeach()

function(fail what)
  message(FATAL_ERROR "${CASE}: ${what}: code=[${code}] out=[${out}] err=[${err}]")
endfunction()

# mosac は WORK/src で動かす。--verbose は結んだ翻訳済みのライブラリの行を出す。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${library_env} ${compiler_env}
    "${MOSAC}" build --verbose "${program}.pl" -o "${WORK}/run/${program}"
  WORKING_DIRECTORY "${WORK}/src"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600)
set(exe "${WORK}/run/${program}${EXE_SUFFIX}")
if(NOT code EQUAL 0 OR NOT EXISTS "${exe}")
  fail("build")
endif()
string(REPLACE "\r\n" "\n" err "${err}")

# library(lists) は mosac の隣の lib/lists.pl と同じ原文なので、翻訳済みの単位 pl/ の書庫を結ぶ。
if(CASE STREQUAL "use_lists" AND NOT err MATCHES "mosac: 結ぶ翻訳済みライブラリ: [^\n]*mosa_pl_lists")
  fail("library(lists) must be linked as a translated archive")
endif()
# 利用者のライブラリは同梱の lib/ に無いので、原文だけを同梱する。その旨を --verbose で出す。
if(CASE STREQUAL "plain_use" AND NOT err MATCHES "mosac: 原文だけを同梱するライブラリ[^\n]*: [^\n]*foo[^\n]*util")
  fail("foo and util must be reported as bundled source only")
endif()
# 下のディレクトリのライブラリの名前は根からの相対（sub/deep）。library('sub/deep') もこの名前で引く。
if(CASE STREQUAL "subdir" AND NOT err MATCHES "mosac: 原文だけを同梱するライブラリ[^\n]*: [^\n]*bar[^\n]*sub/deep")
  fail("bar and sub/deep must be reported as bundled source only")
endif()

# 原文を消し、実行ファイルだけのディレクトリで動かす。隣に lib/ も .pl も library/ も無い。
file(REMOVE_RECURSE "${WORK}/src")
file(GLOB leftovers "${WORK}/run/*")
list(REMOVE_ITEM leftovers "${exe}")
if(leftovers OR IS_DIRECTORY "${WORK}/run/lib")
  set(code "-")
  set(out "${leftovers}")
  set(err "")
  fail("the run directory must hold only the executable")
endif()

# 翻訳した機械のパス（原文を置いた WORK とその下）が実行ファイルに残っていてはならない（Task 5 の持ち越し）。
# 正規表現にせず文字列で探す。パスに . や + が入っても誤って通らない。Windows は \ の綴りも見る。
file(STRINGS "${exe}" embedded_strings LENGTH_MINIMUM 8)
string(REPLACE "/" "\\" work_backslash "${WORK}")
string(FIND "${embedded_strings}" "${WORK}" leaked_at)
string(FIND "${embedded_strings}" "${work_backslash}" leaked_backslash_at)
if(NOT leaked_at EQUAL -1 OR NOT leaked_backslash_at EQUAL -1)
  set(code "-")
  set(out "${WORK}")
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
elseif(CASE STREQUAL "include")
  if(NOT code EQUAL 0 OR NOT out STREQUAL "1-2\n")
    fail("include/1 and ensure_loaded/1 from the embedded units")
  endif()
elseif(CASE STREQUAL "plain_use")
  # foo の中の素のパスの use_module(util) も、ディスクに library/ が無いまま埋め込みから引く。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "42\n")
    fail("a plain-path use_module inside a bundled library")
  endif()
else()
  # bar の中の use_module('sub/deep') は library/sub/deep.pl を引き、プログラムの library('sub/deep') も同じ物を指す。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "7-7\n")
    fail("a plain-path use_module into a subdirectory of a bundled library")
  endif()
endif()
