% レビュー M-10。ゴールの指令で変えた double_quotes を、次の節から読み手が使う。
:- (true, set_prolog_flag(double_quotes, atom)).
m10_goal_quotes("abc").
:- set_prolog_flag(double_quotes, codes).
