% レビュー M-10。include したテキストで変えた double_quotes を、include の後の節から読み手が使う。
:- include(m10_flag_quotes).
m10_include_quotes("abc").
:- set_prolog_flag(double_quotes, codes).
