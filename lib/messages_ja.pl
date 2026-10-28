% 日本語の文面（ADR 0007）。message(Key, Format) の Format は format/2 の書式原子。
% 何も公開しない。C++ は messages_ja:message/2 と修飾して引くので、user の名前は増えない。
:- module(messages_ja, []).
:- use_module(library(format)).

message(goal_raised, '警告: ゴールは例外を送出しました: ~q').
message(heap_limit, '警告: ヒープの上限を超えました').
message(help_japanese_variables, '日本語の名前の変数は _太郎 のように下線で始める。下線の無い 太郎 は原子である。').

% 構文エラー（ADR 0007）。理由の鍵は syntax_error(Key) の Key と同じ。
message(syntax_error, '構文エラー: 行 ~d 列 ~d: ~w ~q').
message(syntax_error_at_end, '構文エラー: 行 ~d 列 ~d: 入力の終わりで~w').
message(invalid_utf8, '不正な UTF-8').
message(illegal_character, '使えない文字').
message(unterminated_quoted, '引用が閉じていない').
message(unterminated_comment, 'コメントが閉じていない').
message(bad_escape, '不正なエスケープ').
message(integer_overflow, '整数が範囲を超えている').
message(unexpected_token, '予期しないトークン').
message(max_arity, '引数が多すぎる').
message(incomplete_term, '項が閉じていない').
