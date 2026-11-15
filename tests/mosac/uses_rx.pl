% standalone の CASE reexport。rx_mid が再公開した lib_p/1 を、定義した rx_lib から呼ぶ。
:- use_module(rx_mid).
:- initialization(main, main).
main :- lib_p(X), write(X), nl.
