% 日本語の文面（ADR 0007）。message(Key, Format) の Format は format/2 の書式原子。
% 何も公開しない。C++ は messages_ja:message/2 と修飾して引くので、user の名前は増えない。
:- module(messages_ja, []).
:- use_module(library(format)).

message(goal_raised, '警告: ゴールは例外を送出しました: ~q').
message(heap_limit, '警告: ヒープの上限を超えました').
message(help_japanese_variables, '日本語の名前の変数は _太郎 のように下線で始める。下線の無い 太郎 は原子である。').
