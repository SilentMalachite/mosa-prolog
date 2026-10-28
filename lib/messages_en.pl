% 英語の文面（ADR 0007）。message(Key, Format) の Format は format/2 の書式原子。
% 何も公開しない。C++ は messages_en:message/2 と修飾して引くので、user の名前は増えない。
:- module(messages_en, []).
:- use_module(library(format)).

message(goal_raised, 'Warning: goal raised exception: ~q').
message(heap_limit, 'Warning: heap limit exceeded').
message(help_japanese_variables, 'Variables with Japanese names start with an underscore, as in _太郎. Without it, 太郎 is an atom.').

% 構文エラー（ADR 0007）。理由の鍵は syntax_error(Key) の Key と同じ。
message(syntax_error, 'Syntax error: line ~d column ~d: ~w ~q').
message(syntax_error_at_end, 'Syntax error: line ~d column ~d: ~w at end of input').
message(invalid_utf8, 'invalid UTF-8').
message(illegal_character, 'illegal character').
message(unterminated_quoted, 'unterminated quoted').
message(unterminated_comment, 'unterminated comment').
message(bad_escape, 'bad escape sequence').
message(integer_overflow, 'integer out of range').
message(unexpected_token, 'unexpected token').
message(max_arity, 'too many arguments').
message(incomplete_term, 'incomplete term').
