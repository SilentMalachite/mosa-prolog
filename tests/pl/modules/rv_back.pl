:- module(rv_back, [rv_pick/1]).
% 本体の ; は、成功の戻りで calling が user になったあとにやり直される。
rv_pick(X) :- (X = 1 ; X = 2), rv_keep(X).
rv_keep(_).
