% レビュー M-11。読み込みに失敗したファイルの旗は、trace_gc と message_language も含めて読む前へ戻る（ADR 0017）。
:- set_prolog_flag(trace_gc, true).
:- set_prolog_flag(message_language, en).
:- set_prolog_flag(unknown, fail).
:- fail.
