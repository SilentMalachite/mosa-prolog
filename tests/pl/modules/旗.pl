% 旗はファイルごと。modules.pl が読む。double_quotes と japanese_punctuation は読み終えると戻り、debug は残る。
:- set_prolog_flag(double_quotes, atom).
:- set_prolog_flag(japanese_punctuation, true).
:- set_prolog_flag(debug, on).

旗の中の文字列("ab").
