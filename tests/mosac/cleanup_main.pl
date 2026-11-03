% 敵対的レビュー A3（M11、ADR 0016）。選択点を残して成功した main も、SWI-Prolog の '$run_init_goal' と同じく
% 答えの後で切り、setup_call_cleanup/3 の後始末を動かす。mosac の実行ファイルも run_main_goal で終わる。
:- initialization(main, main).
main :- setup_call_cleanup(true, (X = 1 ; X = 2), (write(cleanup(X)), nl)), write(got(X)), nl.
