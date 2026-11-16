# 計画 Task 10。mosac build で作った実行ファイルが、lib/ も原文も無いディレクトリで、MOSA_LIB 無しに動くことを確かめる
# （SPEC 第6節「実行ファイルは起動時に外部ファイルを一切要求しない」、ADR 0009 Review Focus 5）。
# 原文は WORK/src に写してから build し、動かす前に消す。実行ファイルに翻訳した機械のパスが残らないことも見る。
# 使い方: cmake -DMOSAC=<mosac>
#   -DCASE=<uncaught_message|use_lists|use_dcg|include|plain_use|subdir|goal_use_body|goal_use_init|goal_use_late|
#           reexport|cond_late_lib|cond_build_mod|cond_skip_op|cond_export|
#           cond_goal_mod|cond_goal_lib>
#   -DSRC=<tests/mosac>
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
elseif(CASE STREQUAL "use_dcg")
  set(sources uses_dcg.pl)
  set(program uses_dcg)
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
elseif(CASE STREQUAL "goal_use_body")
  set(sources goal_uses_apply.pl)
  set(program goal_uses_apply)
elseif(CASE STREQUAL "goal_use_init")
  set(sources init_uses_apply.pl)
  set(program init_uses_apply)
elseif(CASE STREQUAL "goal_use_late")
  set(sources late_uses_apply.pl)
  set(program late_uses_apply)
elseif(CASE STREQUAL "reexport")
  set(sources uses_rx.pl rx_lib.pl rx_mid.pl)
  set(program uses_rx)
elseif(CASE STREQUAL "cond_late_lib")
  set(sources cond_late_lib.pl)
  set(program cond_late_lib)
elseif(CASE STREQUAL "cond_build_mod")
  set(sources cond_build_mod.pl cond_init_mod.pl)
  set(program cond_build_mod)
elseif(CASE STREQUAL "cond_skip_op")
  set(sources cond_skip_op.pl)
  set(program cond_skip_op)
elseif(CASE STREQUAL "cond_export")
  set(sources cond_export.pl cond_export_mod.pl)
  set(program cond_export)
elseif(CASE STREQUAL "cond_goal_mod")
  set(sources cond_goal_mod.pl cond_goal_m.pl)
  set(program cond_goal_mod)
elseif(CASE STREQUAL "cond_goal_lib")
  set(sources cond_goal_lib.pl)
  set(program cond_goal_lib)
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
# ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${library_env} ${compiler_env}
    "${MOSAC}" build --verbose "${program}.pl" -o "${WORK}/run/${program}"
  WORKING_DIRECTORY "${WORK}/src"
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 600 ENCODING UTF-8)
set(exe "${WORK}/run/${program}${EXE_SUFFIX}")
if(NOT code EQUAL 0 OR NOT EXISTS "${exe}")
  fail("build")
endif()
string(REPLACE "\r\n" "\n" err "${err}")
# 取り込みと include の述語は定義済みと見え、未定義の呼び出しの警告は出ない。警告は build を止めないので、
# 誤った警告はここで見る（L12 の敵対的レビュー A1）。頭は文面の言語で変わる。
# cond_export だけは見ない。answer/1 はビルド時に取らなかった分岐だけが定義するので、その呼び出しは警告になる
# （設計の既知の制限 3、計画の「設計との違い」8）。代わりに、公開の未定義が build を止めず警告になったことを見る。
if(NOT CASE STREQUAL "cond_export" AND err MATCHES "\\.pl:[0-9]+: (Warning|警告): ")
  fail("unexpected undefined-call warning")
endif()
if(CASE STREQUAL "cond_export" AND NOT err MATCHES "answer/1 を export したが定義が無い" AND
   NOT err MATCHES "exported procedure answer/1 is not defined")
  fail("an undefined export in a file with a branch not taken must be a warning")
endif()

# library(lists) は mosac の隣の lib/lists.pl と同じ原文なので、翻訳済みの単位 pl/ の書庫を結ぶ。
if(CASE STREQUAL "use_lists" AND NOT err MATCHES "mosac: 結ぶ翻訳済みライブラリ: [^\n]*mosa_pl_lists")
  fail("library(lists) must be linked as a translated archive")
endif()
# 節の本体と initialization/1 のゴールにだけ書いた library(apply) も同梱の閉包に入り、翻訳済みの単位を結ぶ（問い 2 の A）。
if((CASE STREQUAL "goal_use_body" OR CASE STREQUAL "goal_use_init" OR CASE STREQUAL "goal_use_late") AND
   NOT err MATCHES "mosac: 結ぶ翻訳済みライブラリ: [^\n]*mosa_pl_apply")
  fail("library(apply) named in a goal must be linked as a translated archive")
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
  OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 60 ENCODING UTF-8)
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
elseif(CASE STREQUAL "use_dcg")
  # '$dcg_translate'/2 は埋め込みの機械が持つ。library(dcg) の expand_term/2 が単独の実行ファイルで訳せる。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "a/2\n")
    fail("library(dcg) in a standalone executable")
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
elseif(CASE STREQUAL "goal_use_body")
  # 隣に lib/ が無いので、同梱していなければ use_module のゴールが診断を書いて失敗し、main も失敗する。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "[2,1]\n" OR NOT err STREQUAL "")
    fail("library(apply) loaded from a clause body")
  endif()
elseif(CASE STREQUAL "goal_use_init")
  if(NOT code EQUAL 0 OR NOT out STREQUAL "[3]\n" OR NOT err STREQUAL "")
    fail("library(apply) loaded from an initialization/1 goal")
  endif()
elseif(CASE STREQUAL "goal_use_late")
  # apply（ゴールでだけ読む）も lists（文面の表が読む）も、実行ファイルは起動時に読まない。M:G は呼び出しのときに
  # :/2 が確かめ、mosa と同じ existence_error(module, M) になる。生成コードがその場で結ぶと procedure の誤りになる。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "existence_error(module,apply)\nexistence_error(module,lists)\n"
     OR NOT err STREQUAL "")
    fail("a qualified call into a module loaded after the program's own load")
  endif()
elseif(CASE STREQUAL "reexport")
  # rx_mid が reexport で開いた rx_lib.pl も同梱の閉包に入る（SPEC 第5節）。公開の項目は定義した rx_lib を指すので、
  # export_warnings は build を止めず、lib_p/1 の呼び出しも未定義の警告にならない（ADR 0026）。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "42\n" OR NOT err STREQUAL "")
    fail("a predicate re-exported through rx_mid")
  endif()
elseif(CASE STREQUAL "cond_late_lib")
  # ビルド時は :- assertz(cond_flag) を呼ばないので条件が例外になり、分岐を取らない。読み飛ばした項の library(strings) も
  # 同梱の閉包に入り、起動時に取った分岐の use_module が埋め込みから引く（ADR 0027）。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "ABC\n" OR NOT err STREQUAL "")
    fail("a library named only in a branch taken at start-up")
  endif()
elseif(CASE STREQUAL "cond_build_mod")
  # ビルド時にだけ取る分岐が読んだモジュールのファイルは、起動時には読まない。初期化のゴール（init_ran）は動かず、
  # cond_init_mod:hello は :/2 が確かめ、取り込みも無い。どちらも mosa と同じ誤りになる。
  if(NOT code EQUAL 0 OR
     NOT out STREQUAL "existence_error(module,cond_init_mod)\nexistence_error(procedure,hello/0)\n" OR
     NOT err STREQUAL "")
    fail("a module file read only in a branch taken at build time")
  endif()
elseif(CASE STREQUAL "cond_skip_op")
  # ビルド時に取らなかった分岐の op/3 を後ろの項が使う。build は止まらず、ファイルは IR のまま動く。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "a===>b\n" OR NOT err STREQUAL "")
    fail("an op/3 in a branch not taken at build time")
  endif()
elseif(CASE STREQUAL "cond_export")
  if(NOT code EQUAL 0 OR NOT out STREQUAL "42\n" OR NOT err STREQUAL "")
    fail("an exported predicate defined only in a branch taken at start-up")
  endif()
elseif(CASE STREQUAL "cond_goal_mod")
  # 条件のゴールそのものが、ビルド時にだけモジュールのファイルを読む。解いている間の if も閉じていない if に数えるので、
  # cond_goal_m:hello はその場で結ばれず、mosa と同じ module の誤りになる（procedure の誤りにならない）。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "existence_error(module,cond_goal_m)\n" OR NOT err STREQUAL "")
    fail("a module file read only by a condition goal at build time")
  endif()
elseif(CASE STREQUAL "cond_goal_lib")
  # 条件のゴールに字面で書いた library(strings) は、ビルド時の評価がそこへ届かなくても同梱する（フェーズ12 の問い 2 の A）。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "ABC\n" OR NOT err STREQUAL "")
    fail("a library named only in a condition goal")
  endif()
else()
  # bar の中の use_module('sub/deep') は library/sub/deep.pl を引き、プログラムの library('sub/deep') も同じ物を指す。
  if(NOT code EQUAL 0 OR NOT out STREQUAL "7-7\n")
    fail("a plain-path use_module into a subdirectory of a bundled library")
  endif()
endif()
