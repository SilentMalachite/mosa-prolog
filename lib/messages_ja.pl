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
message(incomplete_term, '項が閉じていません').

% 読み込みと使い方の診断（ADR 0007、Task 12b）。
message(usage, 'mosa: 使い方: mosa [ファイル...] [-g ゴール] [-t halt] [-- 引数...]').
message(mosatest_usage, 'mosatest: 使い方: mosatest --roundtrip <ファイル>~nmosatest --suite <ディレクトリ> [--report <ファイル>]~n--report を省けるのは、リポジトリ直下で tests/conformance を渡したときだけです').
message(mosac_unavailable, 'mosac: このコマンドはフェーズ0では使えません').
message(mosac_usage, 'mosac: 使い方: mosac emit <ファイル> -o <ディレクトリ>').
message(mosac_cannot_write, 'mosac: 書き出せません: ~w').
message(mosac_undefined_call, '~w を呼んでいるが、どこにも定義が無い').
message(mosac_export_undefined, '~w を export したが定義が無い').
message(cannot_open_file, '~w: ファイルを開けません').
message(file_cycle, '~w: ファイルが循環しています').
message(clauses_apart, '述語 ~w の節が離れている').
message(predicate_running, '実行中の述語は置き換えられない').
message(directive_unsupported, 'この指示はフェーズ2では使えない: ~w').
message(directive_failed, 'この指示のゴールは失敗した: ~w').
message(not_predicate_indicator, '述語指示子ではありません').
message(static_to_dynamic, '静的な述語は動的にできない').
message(op_bad_priority, '優先度が範囲外です').
message(op_bad_specifier, '演算子の指定子が不正です').
message(op_modify_comma, 'コンマの演算子は変更できません').
message(op_create_empty, '空リストや空の中括弧は演算子にできません').
message(op_infix_and_postfix, '中置と後置は同時に宣言できません').
message(op_create_bar, 'この縦棒は演算子にできません').
message(op_name_not_atom, '演算子名がアトムではありません').
message(op_failed, '演算子を宣言できません').
