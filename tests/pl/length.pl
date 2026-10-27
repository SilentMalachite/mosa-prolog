% length/2 は system の組込。library(lists) を取り込まなくても、閉じたリストの長さが合う。
test(length_closed, length([a, b], 2), true).
% 長さが変数なら、要素の数を束縛する。
test(length_count, length([a, b, c], N), solutions([N = 3])).
% リストが変数で長さが 0 なら、空リストになる。
test(length_empty, length(List, 0), solutions([List = []])).
% リストが変数なら、その長さの未束縛要素を作る。
test(length_build, length(List, 2), solutions([List = [_, _]])).
% 長さが整数でないときは、リストより先に type_error(integer)。
test(length_bad_int, length(_, a), error(type_error(integer, a))).
test(length_bad_int_list, length([], a), error(type_error(integer, a))).
% 負の長さは、リストの形より先に domain_error。
test(length_neg, length(_, -1), error(domain_error(not_less_than_zero, -1))).
test(length_neg_list, length([], -1), error(domain_error(not_less_than_zero, -1))).
% リストでない項と、終端が変数でない不完全なリストは type_error(list)。
test(length_bad_list, length(a, _), error(type_error(list, a))).
test(length_bad_list_n, length(a, 1), error(type_error(list, a))).
test(length_bad_list_int, length(1, 1), error(type_error(list, 1))).
% 終端が変数のときは、足りなければ変数を足し、ちょうどなら [] で閉じる。短すぎる整数は失敗する。
test(length_close, (length([a, b|Tail], 2), Tail = []), true).
test(length_extend, (length([a|Tail], 3), Tail = [b, c]), true).
test(length_short, length([a, b, c|_], 2), fail).
% 両方変数なら、空リストから順に長さを増やす。
test(length_gen, length(List, N), solutions([
  (List = [], N = 0),
  (List = [_], N = 1),
  (List = [_, _], N = 2)
])).
% 終端が変数で長さも変数なら、今の長さで閉じてから、一つずつ延ばす。
test(length_open_tail, length([a, b|Tail], N), solutions([
  (Tail = [], N = 2),
  (Tail = [_], N = 3),
  (Tail = [_, _], N = 4)
])).
test(length_improper, length([_, _|a], _), error(type_error(list, [_, _|a]))).
test(length_improper_zero, length([_, _|a], 0), error(type_error(list, [_, _|a]))).
test(length_improper_n, length([_, _|a], 3), error(type_error(list, [_, _|a]))).
