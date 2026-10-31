% control.pl の catch_parked_rearm が consult する。指令の中の catch は、選択点を残したまま抜ける。
:- catch((_ = a ; _ = b), _, true).
