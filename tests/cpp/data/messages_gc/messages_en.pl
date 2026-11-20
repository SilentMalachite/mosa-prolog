% ctest の message_ops_gc が MOSA_LIB に置くカタログ（L19 の Codex レビュー）。読み込みの指示が GC を起こす。
% 文面は lib/messages_en.pl と違えておく。同じだと、このカタログが読まれず代わりの表示に落ちても試験が通る（レビュー H-35）。
:- module(messages_en, []).
:- use_module(library(format)).
:- garbage_collect.
message(goal_raised, 'Warning (messages_gc catalog): goal raised exception: ~q').
