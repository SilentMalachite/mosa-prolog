:- use_module(library(format)).
% レビュー 2026-11-20 の H-24。print/1 と ~p は portray/1 を user から呼ぶので、掛け金は user から見える portray/1
% （user で定義したか、取り込んだもの）だけである。前はどのモジュールの portray/1 も見て、user に無い portray/1 を
% 呼び、existence_error(procedure, portray/1) になった。user に portray/1 の無いうちに走らせるため、別のファイルに置く。
test(h24_print_hidden, (use_module('tests/pl/fixtures/h24_hidden.pl'), h24_hello,
  with_output_to(atom(A), print(abc)), A == abc), true).
test(h24_format_hidden, (with_output_to(atom(A), format("~p", [f(abc)])), A == 'f(abc)'), true).
% 守り。取り込んだ portray/1 は user から見えるので、掛け金になる。合わない項はそのまま書く。
test(h24_print_imported, (use_module('tests/pl/fixtures/h24_shown.pl'),
  with_output_to(atom(A), print(g(h24_me, abc))), with_output_to(atom(B), format("~p", [h24_me])),
  A == 'g(seen,abc)', B == seen), true).
