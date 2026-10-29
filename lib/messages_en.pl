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

% 読み込みと使い方の診断（ADR 0007、Task 12b）。
message(usage, 'mosa: usage: mosa [file...] [-g goal] [-t halt] [-- argument...]').
message(mosatest_usage, 'mosatest: usage: mosatest --roundtrip <file>~nmosatest --suite <directory> [--report <file>]~n--report may be omitted only when tests/conformance is given from the repository root').
message(mosac_unavailable, 'mosac: this command is not available in phase 0').
message(cannot_open_file, '~w: cannot open file').
message(file_cycle, '~w: files load each other in a cycle').
message(clauses_apart, 'clauses of ~w are not together').
message(predicate_running, 'cannot replace a predicate that is running').
message(directive_unsupported, 'directive not supported: ~w').
message(directive_failed, 'directive goal failed: ~w').
message(not_predicate_indicator, 'not a predicate indicator').
message(static_to_dynamic, 'a static predicate cannot become dynamic').
message(op_bad_priority, 'operator priority out of range').
message(op_bad_specifier, 'invalid operator specifier').
message(op_modify_comma, 'the comma operator cannot be changed').
message(op_create_empty, '[] and {} cannot be operators').
message(op_infix_and_postfix, 'an operator cannot be both infix and postfix').
message(op_create_bar, 'this bar cannot be an operator').
message(op_name_not_atom, 'operator name is not an atom').
message(op_failed, 'cannot declare the operator').
