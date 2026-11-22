# レビュー M-16（7.6.2、7.8.3.3、iso-notes 7.4.2 の initialization/1）。呼べない部分を含む問い合わせ、指令、初期化の
# ゴールは、どの部分も動かす前に、ゴール全体の type_error(callable, Goal) で止まる。前は前半を動かしてから葉で止まり、
# initialization/1 では節と assertz/1 の結果も残った。
# 使い方: cmake -DMOSA=<mosa> -DWORK=<一時ディレクトリ> -P callable_goal_test.cmake

file(MAKE_DIRECTORY "${WORK}")
set(directive "${WORK}/callable_goal_directive.pl")
set(init "${WORK}/callable_goal_init.pl")
file(WRITE "${directive}" ":- (write(dir_ran), nl, 1).\n")
file(WRITE "${init}" ":- initialization((assertz(xx), write(init_ran), nl, 1)).\np(1).\n")

function(run_mosa label)
  # ENCODING UTF-8: 子は UTF-8 で書く。Windows の既定は CMP0176（3.31）が NEW でなければ AUTO で、コンソールのコードページで読み、932 などでは化ける。
  execute_process(COMMAND "${MOSA}" ${ARGN} WORKING_DIRECTORY "${WORK}"
    OUTPUT_VARIABLE out ERROR_VARIABLE err RESULT_VARIABLE code TIMEOUT 120 ENCODING UTF-8)
  set(out "${out}" PARENT_SCOPE)
  set(err "${err}" PARENT_SCOPE)
  set(code "${code}" PARENT_SCOPE)
endfunction()

function(fail label)
  message(FATAL_ERROR "callable_goal: ${label}: code=${code} out=[${out}] err=[${err}]")
endfunction()

# -g の問い合わせ。write(ran) を動かさず、ゴール全体を名指す。
run_mosa("query" -g "(write(ran), nl, 1)" -t halt)
if(out MATCHES "ran" OR NOT err MATCHES "callable,\\(write\\(ran\\),nl,1\\)")
  fail("a non-callable query")
endif()

# 指令。前半の write を動かさず、読み込みを失敗させる。
# -g の中に ; を書かない。CMake の引数の ; は並びの区切りになる。読み込みの成否によらず続けるため forall/2 で包む。
run_mosa("directive" -g "forall(catch(consult('callable_goal_directive.pl'), _, true), true)" -t halt)
if(out MATCHES "dir_ran" OR NOT err MATCHES "callable,\\(write\\(dir_ran\\),nl,1\\)")
  fail("a non-callable directive")
endif()

# 初期化。ゴールを動かさず、読み込みを戻す（p/1 も xx も残らない）。
run_mosa("initialization" -g "forall(catch(consult('callable_goal_init.pl'), _, true), true), forall(catch(p(1), _, fail), write(kept)), forall(catch(xx, _, fail), write(asserted)), write(done), nl" -t halt)
if(out MATCHES "init_ran|kept|asserted" OR NOT out MATCHES "done" OR NOT err MATCHES "callable,\\(assertz\\(xx\\),")
  fail("a non-callable initialization goal")
endif()
file(REMOVE "${directive}" "${init}")
