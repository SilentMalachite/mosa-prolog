% 英語の文面（ADR 0007）。message(Key, Format) の Format は format/2 の書式原子。
% 何も公開しない。C++ は messages_en:message/2 と修飾して引くので、user の名前は増えない。
:- module(messages_en, []).
:- use_module(library(format)).

message(goal_raised, 'Warning: goal raised exception: ~q').
message(heap_limit, 'Warning: heap limit exceeded').
message(help_japanese_variables, 'Variables with Japanese names start with an underscore, as in _太郎. Without it, 太郎 is an atom.').
