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
