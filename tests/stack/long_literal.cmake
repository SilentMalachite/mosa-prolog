# ソースに書いた 200,000 要素のリスト（事実の引数と本体の X = [...]）を、1 MiB のスタックで読んで動かす（ADR 0011）。
# 大きな原文は tests に置かず、ここで WORK に作ってから small_stack.cmake に渡す。
# 使い方: cmake -DMOSA=<mosa> -DWORK=<作業ディレクトリ> -DSMALL_STACK=<small_stack.cmake> -P long_literal.cmake

file(MAKE_DIRECTORY "${WORK}")
set(chunk "1")
foreach(index RANGE 2 1000)
  string(APPEND chunk ",${index}")
endforeach()
# 1000 要素を 200 回つなぐ。最後の要素は 1000 になる。
string(REPEAT "${chunk}," 199 items)
string(APPEND items "${chunk}")
file(WRITE "${WORK}/long_literal.pl" ":- initialization(main, main).
data([${items}]).
final([X], X) :- !.
final([_|T], X) :- final(T, X).
main :- data(L), length(L, N), final(L, E), write(N-E), nl,
    X = [${items}], length(X, M), final(X, F), write(M-F), nl.
")
file(WRITE "${WORK}/long_literal.expected" "200000-1000\n200000-1000\n")
set(COMMAND "${MOSA};${WORK}/long_literal.pl")
set(EXPECTED "${WORK}/long_literal.expected")
include("${SMALL_STACK}")
