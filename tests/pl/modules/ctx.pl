% modules.pl の sm_* と cm_*。節の中の strip_module/3 と context_module/1 は、節のモジュールを文脈にする。
:- module(ctx, [ctx_call/1, ctx_here/1, ctx_strip/2]).
:- meta_predicate(ctx_call(0)).

% 引数は呼び手の文脈モジュールで M: に包まれる。cm_meta は user の文脈を返す
ctx_call(G) :- call(G).

ctx_here(M) :- context_module(M).

ctx_strip(T, M-P) :- strip_module(T, M, P).
