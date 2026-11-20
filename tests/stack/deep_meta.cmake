# レビュー 2026-11-20 の H-10。:- meta_predicate の頭は「,」でつないでも、リストに入れてもよい。その頭を集める
# collect_heads が「,」とリストの要素ごとに C++ で再帰し、30,000 段の左に入れ子の連言と、30,000 段に入れ子の
# リストで、1 MiB のスタックがあふれた（SIGSEGV）。同じ項を事実の引数に書けば読めるので、reader ではない。
# 大きな原文は tests に置かず、ここで WORK に作ってから small_stack.cmake に渡す。
# 使い方: cmake -DMOSA=<mosa> -DWORK=<作業ディレクトリ> -DSMALL_STACK=<small_stack.cmake> -P deep_meta.cmake

file(MAKE_DIRECTORY "${WORK}")
# ((((a(0), q(0)), q(0)), ...), z(0)) は 30,000 個の頭で、a が最初、z が最後。
string(REPEAT "(" 29999 opens)
string(REPEAT ",q(0))" 29998 middle)
set(conjunction "${opens}a(0)${middle},z(0))")
# [[[...[b(0)]...]]] は 30,000 段。
string(REPEAT "[" 30000 list_opens)
string(REPEAT "]" 30000 list_closes)
set(nested "${list_opens}b(0)${list_closes}")
file(WRITE "${WORK}/deep_meta.pl" ":- initialization(main, main).
:- meta_predicate ${conjunction}.
:- meta_predicate ${nested}.
a(G) :- call(G).
q(G) :- call(G).
z(G) :- call(G).
b(G) :- call(G).
show(P) :- ( predicate_property(P, meta_predicate(M)) -> write(M) ; write(none(P)) ), nl.
main :- show(a(_)), show(q(_)), show(z(_)), show(b(_)).
")
file(WRITE "${WORK}/deep_meta.expected" "a(0)\nq(0)\nz(0)\nb(0)\n")
set(COMMAND "${MOSA};${WORK}/deep_meta.pl")
set(EXPECTED "${WORK}/deep_meta.expected")
include("${SMALL_STACK}")
