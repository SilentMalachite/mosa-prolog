test(print_bare, (with_output_to(atom(A), print(foo)), with_output_to(atom(B), write(foo)), A == B), true).
test(print_portray_foo, (
  assertz((portray(A) :- atom(A), write(A), write(A))),
  assertz((portray(F) :- float(F), I is truncate(F), write(I))),
  with_output_to(atom(Atom), print(foo)),
  with_output_to(atom(Float), print(3.14)),
  with_output_to(atom(Fun), print(a(foo))),
  with_output_to(atom(Nest), print(a(foo, b(c(foo, 3.14))))),
  with_output_to(atom(Listed), print([foo, 3.14])),
  with_output_to(atom(Sum), print(foo + 3.14)),
  with_output_to(atom(Stream), (current_output(S), print(S, foo))),
  with_output_to(atom(Num), print(42)),
  with_output_to(atom(Plain), print(a(1, 2))),
  with_output_to(atom(PlainW), write(a(1, 2))),
  with_output_to(atom(Wrote), write(foo)),
  retractall(portray(_)),
  with_output_to(atom(After), print(foo)),
  Atom == foofoo,
  Float == '3',
  Fun == 'a(foofoo)',
  Nest == 'a(foofoo,b(c(foofoo,3)))',
  Listed == '[foofoo,3]',
  Sum == 'foofoo+3',
  Stream == foofoo,
  Num == '42',
  Plain == PlainW,
  Wrote == foo,
  After == foo
), true).
test(print_portray_alloc, (
  assertz(burst(0)),
  assertz((burst(N) :- N > 0, with_output_to(atom(_), true), N1 is N - 1, burst(N1))),
  assertz((portray(mark) :- burst(40), write(kept))),
  with_output_to(atom(Out), print(mark)),
  with_output_to(atom(W), write(foo)),
  retractall(portray(_)),
  Out == kept,
  W == foo
), true).
test(print_portray_gc, (
  assertz((portray(X) :- X == leaf, !, garbage_collect, write(leaf))),
  assertz((portray(_) :- length(_, 30), fail)),
  T = g(A, f(B, leaf), A, [B|C], h(C, leaf, [A|B])),
  with_output_to(atom(P), print(T)),
  with_output_to(atom(W), write(T)),
  retractall(portray(_)),
  P == W
), true).
% レビュー M7: portray が書く途中で束縛し、リストの残りを巡回させても止まる。H-25 からは portray の束縛を字面を
% 取った後に戻すので、リストの残りは束縛の無い T のままで、巡回はそもそも見えない（前は '[hh,g,a' で始まった）。
test(m7_print_portray_cycles, (
  assertz((portray(m7_h(T)) :- var(T), M = [a|M], T = M, write(hh))),
  L = [m7_h(T), g|T],
  with_output_to(atom(A), print(L)),
  retractall(portray(_)),
  A == '[hh,g|_G1]', var(T)), true).
% レビュー 2026-11-20 の H-25。print/1 は何も束縛しない（@term）。portray/1 は変数では呼ばず、字面を取った後に
% portray/1 の束縛を戻す。前は変数にも portray を呼んで束縛を残し、束縛済みの項の中の変数も束縛したままにした。
test(h25_var_rule, (assertz((portray(date(Y, M, D)) :- write(Y/M/D))),
  with_output_to(atom(A), print(f(V))), retractall(portray(_)), A == 'f(_G1)', var(V)), true).
test(h25_var_fact, (assertz(portray(date(2026, 1, 1))),
  with_output_to(atom(A), print(f(V))), retractall(portray(_)), A == 'f(_G1)', var(V)), true).
test(h25_bound_undone, (assertz((portray(f(1)) :- write(one))),
  with_output_to(atom(A), print(g(f(X), X))), retractall(portray(_)), A == 'g(one,_G1)', var(X)), true).
% 束縛の後に GC が走っても、戻す印は GC が直すので、同じく戻る。
test(h25_undone_after_gc, (assertz((portray(f(X)) :- X = 1, garbage_collect, write(one))),
  with_output_to(atom(A), print(g(f(Y), Y))), retractall(portray(_)), A == 'g(one,_G1)', var(Y)), true).
