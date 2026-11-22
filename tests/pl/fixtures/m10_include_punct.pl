% レビュー M-10。include したテキストで変えた japanese_punctuation を、include の後の節から読み手が使う。
:- include(m10_flag_punct).
m10_include_punct（a，b）。
:- set_prolog_flag(japanese_punctuation, false).
