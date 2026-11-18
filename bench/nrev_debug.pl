% フェーズ14 Task 10。デバッグを入れて leap で走る nrev/queens。使われないスパイポイントを一つ置き、leap の各呼び出しの検査の費用を測る。
:- debug.
:- spy(bench_unused/0).
bench_unused.
:- include('nrev.pl').
