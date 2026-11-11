[English](iso-notes.md) | 日本語

# ISO 解釈メモ

この文書は [英語版](iso-notes.md) の日本語訳である。食い違うときは英語版に従う。

規格と仕様が食い違うときは、条番号を付けて「A か B か」の質問を 1 つ書く。確認前に実装しない。

## 仕様第4節 𠮷野家のコードポイント

決定（2026-10-25）: Unicode のコードポイント（スカラー値）で数える。`𠮷野家` は U+20BB7、U+91CE、U+5BB6 の 3 個である。`atom_length('𠮷野家', 3)` が正しい。UTF-16 の 4 符号単位にはしない。𠮷 1 文字は `atom_length('𠮷', 1)` であり、`char_code('𠮷', 134071)` である。

## 8.3.9 callable/1

決定（2026-10-25）: B。Corrigendum 2 の 8.3.9.3 のとおり、Errors は None で、`callable(3)` と `callable(X)` は失敗する。
`call/1` はこれまでどおり、変数なら `instantiation_error`、数なら `type_error(callable, 値)` を投げる。

## 8.16 atom_number/2（非 ISO）

決定（2026-10-25）: 未決 2 は A。`atom_number/2` を登録し、両方向に変換する。両方とも変数なら `instantiation_error`。アトムから数は `number_chars` と同じ構文解析で、合わない字面は `syntax_error(読み筋)`。数からアトムは引用符なしの `write_term` の文字列。両方とも具体値なら、一致すれば成功し、違えば失敗する。

決定（2026-10-29）: 持ち主は A を選んだ。`number_chars/2` と `number_codes/2` と共有する数の構文解析は、字面ではなく ADR 0007 の鍵で `syntax_error(Key)` を投げる。字句の誤りはその理由の鍵（`integer_overflow`、`bad_escape`、`unterminated_comment` など）、それ以外でちょうど 1 つの数でないものは `unexpected_token`。

決定（2026-10-29）: 持ち主は SWI の振る舞いではなく A を選んだ。具体の第一引数がアトムでなければ `type_error(atom, A)`、具体の第二引数が数でなければ `type_error(number, N)`。`number_chars/2` と同じく、どちらもアトムを読む前に調べる。

## 8.10 findall/4（非 ISO）

決定（2026-10-25）: `findall(Template, Goal, List, Tail)` は差分リストである。解が無ければ List と Tail を単一化し、Tail を `[]` には束縛しない。第4引数はリストとして検査しない。`findall/3` は末尾が `[]` の `findall/4` である。

決定（2026-10-27）: 上の「検査しない」は取り下げる。第4引数が部分リストでもリストでもなければ、収集の前に `type_error(list, Tail)` を投げる。変数の末尾は解の終わりに束縛する。空リストと真のリストも末尾のままである。検査が終わる前に末尾を束縛しない。

## 8.14 print/1 と print/2

決定（2026-10-25）: 未決 4 は B。`print/1` と `print/2` は `portray/1` を呼ばず、`write/1` と同じ文字列を出す。項の書き出しで登録した。

決定（2026-10-27）: 上の B は取り下げる。`portray/1` に節がありゴールが成功するとき、`print/1` と `print/2` はその書き出しを項の字面にする。節が無いときは `write/1` と同じ手続きである。節はあるが失敗するときは、その項は `write/1` と同じ字面で、複合項の引数だけ再び `print` にかける。`portray/1` は組込にしない。`write/1` は `portray` を見ない。失敗した `portray` の書き出しは残さない。

レビュー L14（2026-11-05。持ち主の決定ではない）: `portray` の中で halt したときは、項のそれまでの字面と `portray` が現在出力へ書いた分を、halt の前に流れへ書く。`write/1` の後の `halt` と同じである。今までどおり `print` は項を書き終えてから字面を一度に書くので、`portray` が名指しで別の流れへ書いた分の方が先に出る。例外のときは、`catch` の後で続きが書かれうるので、今までどおりその `print` の分は何も書かない。

## 8.14 書き出しオプション

決定（2026-10-25）: `write/1,2` と `print/1,2` は `numbervars(true)`。`writeq/1,2` は `quoted(true)` と `numbervars(true)`。`write_canonical/1,2` は `quoted(true)` と `ignore_ops(true)`。`write_term` の既定は三つとも偽。同じ論理オプションの値が食い違うときは `domain_error(write_option, 後の項)`。`variable_names` は一回で、名前が変数の綴りでなければ `domain_error(write_option, その対)`。束縛済みの Var は無視する。

`lgt_write_term_3_118` から `122` は、同じオプションを 2 度書いたとき後の方が勝つことを期待する。`[quoted(true), quoted(false)]` は `A` を書き、`[numbervars(true), numbervars(false)]` は `$VAR(0)` を書き、2 つ目の `variable_names/1` を使う。`lgt_open_4_41`（今は `os::` で止まっている）は `open/4` のオプション `eof_action/1` にも同じことを期待する。上の決定は `domain_error(write_option, LaterTerm)` である（`tests/pl/write.pl` の `opt_conflict`）。調べた範囲では、規格の文面（書き出しオプションの 7.10.4、ストリームのオプションの 7.10.2.11）は、オプションを繰り返したときのことを言っていない。

A か B か: A は `domain_error(write_option, LaterTerm)` のまま（今の決定。5 行は不合格のまま）。B は後のオプションが勝つ。スイートが `write_term/3` と `open/4` に仮定するとおりにし、`opt_conflict` を決定と同じコミットで直す。

決定（2026-10-28）: B。繰り返した書き出しオプションは後の方が勝つ。どの出現も検査はするので、`[quoted(true), quoted(foo)]` は `domain_error(write_option, quoted(foo))` になる。2 つ目の `variable_names/1` は 1 つ目を置き換える。2026-10-25 の決定のうち「`domain_error(write_option, LaterTerm)`」と「`variable_names` は一回」をこれで置き換える。8.14.2.3 が `domain_error(write_option, E)` にするのは要素が正しい書き出しオプションでないときだけで、繰り返した要素はどれも正しい。`open/4` はすでに後の `type/1`、`reposition/1`、`eof_action/1` を勝たせている。テストは `tests/pl/write.pl` の `opt_repeat_*`。

## 8.14.3 op/3

決定（2026-10-26）: ゴールは `OperatorTable::define` を呼ぶ。検査は具体化、型、ドメイン、許可の順で、名前のリストは全部を確かめてから表を置き換える。拒否のときは表を変えない。0 以外の宣言は、同じ類（前置、中置、後置）の古い指定子を置き換える。優先度 0 はその指定子だけを消す。`|` の優先度 0 は Cor.2 の例どおり削除として成功する。中置で 1 以上 1000 以下、および前置と後置は `permission_error(create, operator, '|')`。コンマは `permission_error(modify, operator, ',')`。`[]` と `{}` は `permission_error(create, operator, そのアトム)`。指示の `:- op` も同じ `define` を使う。

質問（2026-11-02、レビュー M4）: 45bbc64 以来、選択点の後ろや、守りが有効な catch/3 の中で呼んだ op/3 は、その点へバックトラックしたときや catch が throw を受けたときに戻っていた。そのため `findall(x, op(700, xfx, qq2), _)` の後に `qq2` が残らなかった。8.14.3 は表の変更をゴールの副作用とし、戻すことには触れていない。assertz/1、set_prolog_flag/2、char_conversion/2 は一度も戻していない。A か B か: A: op/3 はバックトラック、例外、findall/3、`\+` のどれでも戻さない。B: 今のまま戻し、意図的な逸脱として記す。

決定（2026-11-02）: A。op/3 が成功した時点から表の変更は残る。mosa でも mosac の実行ファイルでも同じで、`user` の表でもモジュール固有の層でも同じ。失敗した読み込みが、読み込みを始めたときの表へ戻すのは今のまま（これは読み込み自体の取り消しで、この規則とは別）。テストは `tests/pl/operators.pl` の `m4_*` と `op_backtrack_prefix`、`tests/pl/modules.pl` の `m4_local_backtrack`。

## 8.14.4 current_op/3

決定（2026-10-26）: 検査は引数の順で、最初に当てはまるものを投げる。優先度が変数でなく、0 以上 1200 以下の整数でもなければ `domain_error(operator_priority, Priority)`。アトムや浮動小数もこのドメインエラーである。指定子が変数でなく、`xf` `yf` `xfx` `xfy` `yfx` `fx` `fy` のどれでもなければ `domain_error(operator_specifier, Op_specifier)`。整数や複合項もこのドメインエラーである。演算子が変数でもアトムでもなければ `type_error(atom, Operator)`。優先度 0 は領域に入る。該当が無ければ失敗する。候補は呼び出しのときに写し、やり直しの途中で `op/3` が表を変えても写した候補は残る。名前がアトムのときの並びは `lookup` と同じで、優先度の昇順、同じ優先度では指定子の宣言順である。名前が変数のときは表の並びである。

## 8 with_output_to/2

決定（2026-10-25）: 最初の成功だけを取る。成功のとき `atom/1` と `codes/1` を束縛する。失敗と例外では束縛しない。現在出力は成功しても失敗しても例外でも戻す。ゴールは今の文脈で走る。作ったメモリの流れは終わったら閉じる（下の「8 with_output_to のメモリ」）。

## 8.8 clause/2

決定（2026-10-25）: 未決 5 は A。利用者が定義した述語は公開である。`consult` の静的な節、`:- dynamic`、`assert` で作った述語、`retract` で節を空にした動的述語を含む。`clause/2` は定義順に節を返す。事実の本体はアトム `true` である。同じスロットの変数は同じセルで、匿名変数は出現ごとに新しい参照である。本体の `false/0` はコンパイルが `fail` に畳むので、返る本体はアトム `fail` である。組込と制御は `permission_error(access, private_procedure, PI)` である。無い述語は失敗する。節の無い動的述語は失敗するが、非公開ではない。静的な利用者述語は読める。

## 8.9.1 本体が変数の asserta/1 と assertz/1

8.9.1.3 と 8.9.2.3 が `instantiation_error` とするのは頭部が変数のときだけで、7.6.2 は変数の本体 `X` を `call(X)` に変える。Mosa は `consult` と `mosac` から来た `p :- X` と、assert の `p(X) :- true, X` はそう格納していたが、`asserta((p :- X))` と `assertz((p(X) :- X))` は `instantiation_error` を出していた（レビュー H5）。

A か B か: A は 7.6.2 に従う。節は本体が `call(X)` であるかのように振る舞う。`clause(p, B)` は `B = call(_)` を返し、`retract((p :- B))` は `B` を `call(_)` と単一化する（したがって `retract(p)` と `retract((p :- true))` はこの節を消さない）。`X` が未束縛のまま呼ぶと `call/1` から `instantiation_error` になる。B は誤りを残し、意図した逸脱として記す。

決定（2026-11-01）: A（所有者が H5 の計画と実装を一度に依頼した）。retract/1 は格納した IR の変数そのものと単一化していたので、どの本体とも合っていた。H5 より前から `p(X) :- true, X` や consult した節でも起きていた。今は clause/2 と同じ形に本体を戻す（`retract((h :- halt))` も一致するようになり、本体の `false` は、このファイルの 8.8 clause/2 に記したとおり `fail` に見える）。`catch/3` の引数は 7.6.2 c のとおり書いたままにする。clause/2 は以前そこの変数を `call(X)` と見せていた。`p(X) :- X` に対する `retract((p(Y) :- Y))` は `Y` を `call(Y)` と単一化する。これは出現検査の対象外（7.3.3）なので、`Y` は循環項になる。テスト: `tests/pl/database.pl` の `assert_body_var_*`。

## 7.8.9 変数を含む catch/3 のゴール

7.8.9.1 は `catch/3` の Goal を `call/1` と同じように動かす。したがって中の変数は catch が動くときに置き換わり、その変数が `!` に束縛されていれば catch の Goal を切る。Mosa は節を作るときに Goal を訳し、その変数を切断を閉じる `call(X)` にする。`assertz((t(X) :- catch(((X, fail) ; true), _, true)))` の後、`t(!)` は成功する（mosa と mosac）が、`G = catch(((X, fail) ; true), _, true), X = !, call(G)` は失敗する。H5 以降 clause/2 は Goal を書いたまま見せる（7.6.2 c）ので、`clause(t(!), B), call(B)` は失敗し、`t(!)` は成功する。H5 の敵対的レビューで見つかった。訳し方は H5 より前からある。

A か B か: A は catch/3 の Goal と Recovery を、実行時に束縛された変数が切断に対して透過になるように訳す（IR の変更なので先に ADR）。B は訳した形を残し、ここに逸脱として記す。

決定（2026-11-01）: A。H6 の後に行う。ゴール位置に変数を持つ Goal か Recovery だけをデータのまま持ち、実行時に `call/1` で動かす。そういう変数の無い catch/3 は今のまま訳す。IR と mosac のコード生成が変わるので、先に ADR を書く。レビュー項目 M14。

完了（2026-11-01、0b333c3、ADR 0014）: そういう Goal か Recovery は `IrTag::Call` の節点になり、catch が動くときに `call/1` として動く。mosac は、ゴール位置に変数を含む `\+` と `M:G` のデータを先に訳さなくなった（以前は n(!) が mosa と違った）。テスト: `tests/pl/control.pl` の `m14_*`、`tests/mosac/undefined_catch.pl`、`tests/stack/deep_builtins.pl` の `call_catch_late_nest`。完了（2026-11-01、レビュー M15、ADR 0014 の改訂）: 変数が無くても、呼び出せない部分（`,` `;` `->` `*->` の下のゴール位置の数）のある Goal と Recovery は `Call` の節点になる。`catch((write(a), 1), E, true)` は `a` を書かずに `E` を `type_error(callable, (write(a), 1))` に束縛し、`call/1` と `assertz/1` は catch/3 の引数の中を見なくなった。テスト: `tests/pl/control.pl` の `m15_*`。

## 7.3.3 巡回するゴールの呼び出し

7.3.3 は出現検査の対象にならない項（巡回する項）を定めていない。Mosa は `X = f(X), call(X)` と `X = call(X), call(X)` で落ち（SIGSEGV）、`X = (a, X), call(X)` から戻らなかった（レビュー M13）。レビュー M13（2026-11-01）から、実行時に組み立てたゴールは展開した形として動く。引数にゴール自身を含む述語の呼び出しは、その巡回する引数での普通の呼び出しになる。訳している途中の制御構造（`,` `;` `->` `*->` `catch/3`）に戻る所は、その位置で元の項を call/1 する。そのため `X = (a, X)` は `existence_error(procedure, a/0)` になり、`X = (Y = 1 ; X)` はやり直すたびに `Y = 1` を返す。カットはそれがある段の中に閉じる。`X = (((Y == x, !) ; (Y = x, X)) ; Y = extra), findall(Y, X, L)` は `[x, extra]` になり、同じゴールを手で 1 段展開して書くと `[x]` になる。展開しても止まらないゴールはここでも止まらない。`X = call(X)`、`X = user:X`、`X = (true, X)`、`X = (fail ; X)` は（`p :- (true, p)` と同じく）ヒープの上限の中で動き続け、`X = (X, true)`、`X = catch(X, _, true)`、`X = (X -> true ; true)`、`X = (\+ X)`、`X = (X ; true)` のような左再帰はヒープの上限で止まる。レビュー M17 からは、上限に選択点などヒープの外の状態も数える（ADR 0010 の改訂 3。`--heap-max 64` で RSS 95〜122 MB で止まる。前は 946 MB〜1.6 GB に達し、`X = (X ; true)` は止まらなかった）。`with_output_to/2` は今も C++ で再帰する（7.1）。テスト: `tests/pl/control.pl` の `m13_*`、`tests/cpp/machine_test.cpp` の `check_cyclic_goal_limit`、`tests/stack/deep_builtins.pl` の `call_cyclic_leaves`。

節には戻り先として指せる環境がないので、assertz/1 や asserta/1 に渡した巡回する本体は際限なく膨らんでいた（レビュー M7: `--heap-max 16` で `X = (a, X), assertz((p :- X))` が 11.6 GB）。巡回する頭や引数は、前から戻る所で匿名変数に切っていた（`X = f(X), assertz(p(X))` は `p(f(_))` を足す）。A か B か: A は、訳している途中の制御構造に戻る所を同じように新しい変数のゴールに切る。節は有限になり、そこまで来ると `instantiation_error` になる。B は、巡回する本体なら assertz/1 が誤りを投げる。

決定（2026-11-03）: A（レビュー M7）。`X = (a, X), assertz((p :- X))` は `p :- a, call(_)` を足し（clause/2 はその本体を返す）、`X = (Y = 1 ; X)` を本体にした `p(Y)` は `Y = 1` の後に `instantiation_error` になる。戻る所ごとに別の変数になり、制御構造を含むデータの引数はデータとして写してその中の巡回を切る（`X = (q(X), true)` は `q((q(_), true)), true` を足す）。テスト: `tests/pl/database.pl` の `m7_assert_*`。

## 7.3.3 循環する式の評価

評価器は、評価している途中の部分項を、ヒープの外にある自分のスタックに置く。そのため `X = X+1, Y is X` は止まらず、`--heap-max` を超えて育った（`--heap-max 64` で 10 秒で RSS 16 GB）。レビュー L3 の後は `format("~d", [X])` もここに着いた（レビュー L20）。7.3.3 は循環する項を未定義とし、7.9.1 は式を有限の木として評価する。A か B か: A は循環を見つけて `resource_error(cyclic_term)`（7.12.2 h）を投げ、`catch/3` で受けられる。B はそのスタックをヒープの上限に数え、「7.3.3 巡回するゴールの呼び出し」の循環するゴールと同じく上限で問い合わせを止め、`catch/3` では受けられない。

決定（2026-11-06）: A（レビュー L20）。評価の道の各段は下の段の引数なので、自分の道でもう一度出会う複合項は循環を意味し、それ以外にはない。評価は道の 1 段を見張り、入る複合項ごとにそれと比べる。道に当てた Brent の循環検出である（`src/builtins/arith.cpp` の `CycleWatch`。最初の 64 段の C++ の再帰 `eval_at` と、その先のスタック `eval_deep` で 1 つを共有する）。見張りは、入った数が 1、2、4…に届くたびにそのとき入った段へ移り、見張った段が脇道ごと評価を終えれば親へ移る。終わらない評価は同じ部分項を毎回同じに評価するので、ある深さから先、道に残る段は同じ周期で並ぶ。見張りはそのどれかに留まり、窓が 1 周期より長くなれば、その周期のうちに同じ項ともう一度出会う。だから誤りまでの手間は、循環しない部分と、脇道を含めた 1 周期の数倍で、有限の式は複合項ごとに比較 1 回を払う（時間の差は測れなかった。is/2 2 つと比較 1 つの 1,000,000 回は前も後も 0.97 秒、深さ 200,000 の 20 回の評価は前 0.29 秒、後 0.30 秒）。深さ 1,000,000 の脇道 `S` を持つ `X = S+X` は、`S` を 1 度評価した後に誤りになる（0.68 秒。うち `S` を組むのに 0.66 秒）。スタックは最初の繰り返しまでの道の 3 倍ほどと脇道 1 つまでで、これまでどおり `--heap-max` の外だが、式の大きさに比例する。周期 2^20 は RSS 240 MB、周期 2^20 + 1 は 365 MB で止まり、循環しない同じ式の評価は 155 MB である。引数はこれまでどおり左から評価し、複合項は入るときに調べるので、繰り返しより先に出会う誤りが先に出る。`X = foo+X` は `type_error(evaluable, foo/0)`、`X = _+X` は `instantiation_error` で、`X = X+foo` と `X = X+_` は `resource_error(cyclic_term)` になる。is/2、比較、tab/1,2、format/2 の数の引数（`~d`、`~e`、`~f`、`~c`、桁の `~*` など、`lib/format.pl` で is/2 を通るもの）、mosac の生成コードはどれもこの評価を通る。テスト: `tests/pl/arith.pl`、`tests/pl/lib_format.pl`、`tests/pl/utilities.pl` の `l20_*`（古いコードでは 23 件が 1 秒で止まらない）、`tests/pl/native.pl` の `native_l20_*`、`tests/stack/deep_builtins.pl` の `is_cyclic`、`is_cyclic_after_deep`、`is_cyclic_side`、`is_cyclic_side_prefix`。

## 8.8 current_predicate/1

決定（2026-10-25）: 未決 6 は B。`current_predicate/1` は利用者の述語だけを、定義された順に返す。`current_predicate(true/0)` は失敗であり、`permission_error` ではない。空の動的述語は成功する。`abolish` のあとは失敗する。

## 8 入出力テストのメモリストリーム

決定（2026-10-25）: 未決 7 は A。名前は `open_string/2` と `with_output_to/2`。置き場と改行は ADR 0005。

## 7.11 旗 char_conversion

決定（2026-10-25）: 未決 8 は A。旗 `char_conversion` の既定は `off`。`on` のとき読みは写像を使う。写像の初期状態は恒等のまま。

## 8.14.5 char_conversion/2

決定（2026-10-26）: 検査は、来が変数、来が 1 文字のアトムでない、先が変数、先が 1 文字のアトムでない、の順。型は `type_error(character, その引数)`。同じ文字を先にすると、その来の写像を消して恒等に戻す。別の文字へは、来ごとに一つだけ置き換える。`a` を `b` に、`b` を `c` にしても、`a` は `b` のままである。旗が `off` のあいだ、読みは写像を使わない。`on` のときは、引用の外と注釈の外の符号点だけを写す。引用と注釈の中は元の文字である。`0'` のうしろの文字は引用の外なので写す。指示 `:- char_conversion` も同じ検査で、読み込みが失敗したときは旗と写像を戻す。値は `on` と `off` である。

## 8.14.6 current_char_conversion/2

決定（2026-10-26）: 検査は来、先の順。変数は許す。1 文字のアトムでなければ `type_error(character, その引数)`。具体化エラーは投げない。恒等の組は解にしない。初期状態は解が無い。同じ変数を両引数にしても失敗する。計画にある「恒等のとき自分自身に結び付く」は採らない。旗の on と off は問い合わせに影響しない。旗が off でも登録した組は返り、読みは写さない。候補は呼び出しのときの写しで、登録順である。同じ来を置き換えると、その組は末尾に付く。やり直しの途中で写像を消しても、写した候補は残る。`a` を `b` に、`b` を `c` にした組は、そのまま二つの解である。

## 7.4.2 discontiguous/1

決定（2026-10-26）: 指示だけである。引数は述語指示子かそのリストで、`dynamic/1` と同じである。コンマで並べた列は指示子ではない。同じテキストで、隙間より前に宣言した述語だけ、節が離れてよい。宣言が無ければ診断は `述語 名前/アリティ の節が離れている` のまま。宣言は手続きを作らない。節が無ければ `current_predicate/1` は失敗し、呼び出しは `existence_error` である。宣言は次の読み込みへ残さない。次の `consult` はその述語を置き換える。別のアリティや別の述語の隙間は許さない。

`syntax/directives/discontiguous_1`（5 行）は、カンマで区切った並び `:- discontiguous((b/1, c/2)).` を書いており、上の決定がそれを拒むので（「述語指示子ではありません」）読み込みに失敗する。同じファイルの単独とリストの形は読め、`tests/pl/directives.pl` の `dc_arity2` から `dc_arity4` がその形を確かめる。私たちの読みでは、7.4.2 は `dynamic/1`、`multifile/1`、`discontiguous/1` の引数に述語指示子、述語指示子の並び、述語指示子のリストを認める。ただし条文そのものは確かめていない。`dynamic/1` と `multifile/1` も同じく並びを拒む。

A か B か: A は決定のまま（指示子かリストだけ。5 行は不合格のまま）。B は 3 つの指示すべてでカンマの並びも受け付け、各要素は同じ検査をする。確認前は A のまま。

決定（2026-10-28）: B。`dynamic/1`、`multifile/1`、`discontiguous/1` はカンマで区切った並び `(p/1, q/2)` も受け付ける。各要素は述語指示子でなければならず、同じ検査をする。並びの中のリストや誤った要素は、同じ診断で読み込みを失敗させる。2026-10-26 の `discontiguous/1` と `multifile/1` の決定のうち「コンマで並べた列は指示子ではない」をこれで置き換える。テストは `tests/pl/directives.pl` の `dc_seq_*`、`dyn_seq_*`、`mf_seq` と、`tests/cpp/machine_test.cpp` の "comma sequence"。

## 7.4.2 multifile/1

決定（2026-10-26）: 指示だけである。引数は述語指示子かそのリストで、`dynamic/1` と同じである。コンマで並べた列は指示子ではない。`モジュール:PI` はフェーズ4まで書かない。宣言は手続きを作らない。節が無ければ `current_predicate/1` は失敗し、呼び出しは `existence_error` である。宣言は次の読み込みへ残す。残った述語は、次の読み込みが節を足し、置き換えない。同じテキストを二度読むと節は倍になる。宣言が無い述語は置き換わり、解は倍にならない。この読み込みの最初の節より後の宣言は、その読み込みの置き換えを戻さない。その宣言は残り、その次の読み込みは足す。別のアリティや別の述語は足さない。失敗した読み込みは、足した節と宣言を戻す。動的のままの述語は、足しても動的のままである。隙間は許さず、診断は `述語 名前/アリティ の節が離れている` のまま。

決定（2026-10-28）: カンマの並びを受け付ける。7.4.2 discontiguous/1 を参照。

## 7.4.2 initialization/1

決定（2026-10-26）: A。節は例外のときも残す。指示だけである。ゴールはテキストを準備したあとに、ソースの順で一度実行する。引数が変数なら `instantiation_error`、callable でなければ `type_error(callable, Goal)`。この二つは読み込みを失敗させ、節を戻し、ゴールは実行しない。失敗はエラーにしない。読み込みは成功し、節は残り、後の初期化ゴールも実行する。失敗より前の副作用は残る。捕捉されない例外は、`error/2` ならその Formal、それ以外は投げた項を診断にして読み込みを失敗させる。後の初期化ゴールは実行しない。節と、例外より前の副作用は残す。

## 7.4.2 include/1

決定（2026-10-26）: 指示だけである。節は、その指示の位置で同じテキストに入る。離れた節の診断は `述語 名前/アリティ の節が離れている` のまま。入れたファイルの `op`、`discontiguous`、`initialization` も、その位置で同じテキストに効く。初期化ゴールは、テキスト全体を準備したあとに、ソースの順で実行する。相対パスは、その指示があるファイルのディレクトリから開く。文字だけの load は、渡されたパスのまま開く。同じファイルを二度 include すると節は倍になる。開き済みのファイルを再帰で開くと、診断は `パス: ファイルが循環しています` で、節は戻す。引数が変数なら `instantiation_error`、アトムでなければ `type_error(atom, File)`。この二つは読み込みを失敗させ、節を戻す。`[]` は空の指示ではなくファイル名である。開けないときは `パス: ファイルを開けません` で失敗し、節を戻す。規格の `existence_error(source_sink, File)` は使わない。入れ子の構文エラーは、そのファイルのパスと位置を診断に残す。拡張子の無い名前は、そのパスが普通のファイルでなければ、同じ名前に `.pl` を足して開く。名前のまま開けるときと、既に拡張子があるときは足さない。どちらも無ければ、診断のパスは渡された名前のままである。

## 7.4.2 ensure_loaded/1

決定（2026-10-26）: 指示だけである。引数と、開けないときの診断と、拡張子の扱いは include/1 と同じである。変数は `instantiation_error`、アトムでなければ `type_error(atom, File)`。`[]` はファイル名である。開けないときは `パス: ファイルを開けません` で失敗し、節を戻す。規格の `existence_error(source_sink, File)` は使わない。節は、その指示の位置で同じテキストに入る。同じ正規パスを二度 `ensure_loaded` しても、節も初期化も足さない。相対パスと絶対パスが同じファイルなら一度である。`include` したファイル、`consult` したファイル、今開いているファイルも準備済みである。そのあとの `ensure_loaded` は読み直さない。`include` は準備済みでも節を足す。`consult` は読み直して置き換える。自分自身や、互いに `ensure_loaded` するファイルは循環にしない。節を戻す失敗は、準備済みのパスも戻す。初期化の例外で失敗したときは節が残るので、パスも残し、次の `ensure_loaded` は例外を再実行しない。

## 7.4.2 [File]

決定（2026-10-26）: 指示だけである。本体がリストのとき、各アトムを `consult` と同じ手続きで左から読む。空リストは何もしない。要素の `[]` はファイル名である。部分リストと、要素の変数は `instantiation_error`。変数が一つでもあれば型より先である。要素がアトムでなければ `type_error(atom, その要素)`。閉じたリストでなければ `type_error(list, その項)`。`:- X.` はリストではない。相対パスは、その指示があるファイルのディレクトリから開く。文字だけの load は、渡されたパスのまま開く。開けないときは `パス: ファイルを開けません` で失敗し、節を戻す。節は親のテキストに継がない。別の述語を読んでも、親の節は離れない。`consult` は述語を置き換え、同じパスをもう一度読むと初期化ももう一度実行する。そのあとの `ensure_loaded` は読み直さない。親がすでに見た述語へは、そのあとの節を足す。親がまだ見ていない述語は、そのあとの節が置き換える。自分自身を開くと `ファイルが循環しています` で失敗し、節は戻す。初期化の例外は節を残し、後の節と初期化は読まない。リストの途中でそれ以外の失敗をしたときは、それより前に置き換えた節も戻す。

決定（2026-10-29）: ADR 0007。上の文面は `ja` のカタログのものである。開けないファイルは鍵 `cannot_open_file`、循環は `file_cycle` で、どちらもパスを引数に取る。英語の文面は `messages_en.pl` が持つ。それ以外の上の決定は変えない。

## 7.4.2 そのほかの指示（`:- G`）

7.4.2 は規格に合うテキストが使う指示を挙げ、SPEC 第3節の 8 も同じ組を挙げる。これまで、それ以外の `:- G` は `directive_unsupported` で読み込みを失敗させていた。フェーズ6の計画 Task 5 は、実行ファイルの起動時に `:- write(x).` が動くこと（テスト `mosac_emit_silent`）と、翻訳器がそのゴールを飛ばすことを求める。

ISO（5.5）は 7.4.2 のほかの指示を処理系固有の機能として扱う。厳密に規格に合う動作の切り替えを設けるなら、それは受けてはならない。その切り替えは作らない。

決定（2026-10-29、フェーズ6 Task 5、持ち主が 2026-10-29 に A と決定）: ゴールがアトムか複合項の指示は、読み込み器が受け持つ名前（`include`、`ensure_loaded`、`use_module`、`module`、`export`、`dynamic`、`discontiguous`、`multifile`、`meta_predicate`、`initialization`、`char_conversion`、`set_prolog_flag`、`op`、`consult`。形が合わないときは `directive_unsupported` のまま）を除き、テキストのその位置で、読み込み中のモジュールの文脈で、それまでに読んだ節のもとで 1 回呼ぶ。成功なら読み込みを続ける。失敗は鍵 `directive_failed` で、捕捉されない例外はその Formal で読み込みを失敗させ、どちらも受けない指示と同じく節と旗を戻す。変数と数は今までどおり `directive_unsupported`。テストは `tests/pl/directives.pl` の `general_directive_order` と `tests/cpp/machine_test.cpp` の `check_general_directive`。

指示（この指示か `initialization/1`）の中の `halt/0` と `halt/1` は読み込みをその場で止め、`mosa` も `mosac` の実行ファイルもその値で終わる。後の指示、`initialization(_, main)`、`-g` は動かない（`tests/cpp/session_test.cpp` の `check_directive_halt`、`mosac_emit_halt`）。mosatest が指示ごとに先に読む断片ではこれらのゴールを呼ばず、ファイルの最後の読み込みが 1 回だけ呼ぶ。

尋ねた問い: A（失敗と例外は読み込みを戻す。上のとおり）か B（SWI-Prolog と同じく警告を出して読み続ける）か。持ち主は 2026-10-29 に A を選んだ。

## 7.4 組込述語と制御構造の節、呼べない本体の節

レビュー M10。consult は、頭が組込述語か制御構造の節（`atom_length(_, 99).`、`length(_, 7).`、`call(_) :- write(mine).`、`catch(_, _, _).`、`(a, b) :- true.`、`! :- true.`）と、本体が呼べない節（`foo :- 1.`、`foo :- (true, 1).`、`p :- (q ; 2).`）を誤りにせず足す。mosa でも mosac build でも同じ（mosac build は同じ consult でプログラムを読む）。その節は動かない（`atom_length(abc, L)` は 3 のまま）が、`current_predicate(atom_length/2)` が真になり、`foo` は呼ばれたときに初めて文脈 `call/1` の `type_error(callable, 1)` を出す。assertz/1 は同じ節を `type_error(callable, Body)`、その次に `permission_error(modify, static_procedure, PI)` で拒む（8.9.1.3、この順）。頭が変数か数の節は、今も `directive_unsupported` で読み込みを失敗させる。`lib/` に Prolog で書いた述語（`append/3` など）は組込でなく、定義し直せるまま。`foo :- X.` と `foo :- call(1).` も正しい節のまま（7.6.2）。7.4 と 7.6 を読む限り、このような節は Prolog テキストの一部になれないが、手元の本文には処理系がそれをどう知らせるか、読み続けるかが書かれていない。条文そのものは確かめていない。Mosa のほかの読み込みの誤り（`:- dynamic(true/0).`、`discontiguous/1` の無い離れた節、失敗するか例外を出す指示、構文エラー）は、どれも読み込み全体を失敗させて戻す（上の 7.4.2 そのほかの指示、2026-10-29 の持ち主の決定 A）。

A か B か: A: このような節は、assertz/1 がその節に出すのと同じ Formal（本体が呼べなければ `type_error(callable, Body)`、そうでなく組込述語か制御構造なら `permission_error(modify, static_procedure, PI)`）で読み込みを失敗させ、ほかの読み込みの誤りと同じく、その読み込みの節と旗を戻す。B: SWI-Prolog と同じく、その誤りを警告として出し、その節を飛ばして読み続ける。確かめるまで実装しない。

決定（2026-11-03）: A（レビュー M10）。読み込み器は各節を訳した後、足す前に確かめる（`src/machine/load_clauses.cpp` の `refused_clause`、`take_term` から呼ぶ）。順は assertz/1 と同じで、ゴールの位置に数のある本体は `type_error(callable, Body)`、その次に、頭が組込述語、制御構造、`:-/1`、`:-/2` なら `permission_error(modify, static_procedure, Name/Arity)` になる。SPEC 第5節は組込述語と制御構造を `system` に置くので、どのモジュールでも、`user:` で修飾しても同じ。読み込みはその Formal を診断として失敗し、その読み込みの節と旗は戻る。実行中に呼んだ consult/1 は失敗し、`mosac emit` と `mosac build` は C++ を書かずに止まる。翻訳の読み込みは一般の指示を呼ばないので、その後ろ（その op/3 が後の項の読みを変えうる）で断った節は、構文エラーと同じく扱う。その節を外し、そのファイルを IR のまま動かし、実行ファイルの起動時の読み込みが mosa と同じ診断で判定する（Codex レビュー）。テストは `tests/pl/directives.pl` の `m10_*`、`tests/cpp/machine_test.cpp` の `check_refused_clauses`、ctest の `mosac_emit_refused` と `mosac_emit_refused_op`。

## 7.11 旗 debug

決定（2026-10-25）: 未決 9 は A。既定は `off`。`on` にしてもフェーズ3ではトレースしない。

## 7.11 旗 unknown の warning

決定（2026-10-25）: 未決 10 は B。`unknown` は `error` と `fail` だけを受け付ける。`warning` は受け付けない。

## 7.11 旗 float_overflow、float_zero_div、float_undefined

決定（2026-10-25）: 未決 11 は B。`float_overflow`、`float_zero_div`、`float_undefined` は `error` だけを受け付ける。別のアトム名は作らない。

## 第4節 旗 invalid_utf8

決定（2026-10-25）: 未決 12 は A。`invalid_utf8` の既定は `error`。`replace` のときは U+FFFD を入れる。

## 第4節と 8.12.1 壊れた UTF-8 の並びの後の位置

SPEC 第4節は、`invalid_utf8` が `error` のとき壊れた UTF-8 の並びで `representation_error(character)` を出すとするが、その後に流れがどこに立つかは書いていない。8.12.1 と 8.14.1 も書いていない。Mosa は位置を並びの前のままにしたので、誤りを捕えて読み続けるループは、get_char/1,2 でも read/1 でも同じ誤りを出し続けた（レビュー L6）。これとは別に、`replace` は先頭の 1 バイトだけを U+FFFD にしたので、`E3 81` のように途中で切れた並びが U+FFFD 2 つになった。

A か B か: A は並びを 1 文字として進めてから誤りを出す。位置は `replace` のときと同じになる。peek_char/1,2 は進めない。B は位置を並びの前のままにする。

決定（2026-11-05）: A。どちらの旗の値でも、並びはその maximal subpart とする（Unicode 15.0 の 3.9 節、表 3-7。正しい並びの頭として通る最長の部分で、1 バイト以上）。`E3 81` は 1 文字、`ED A0 80` は 3 文字になる。read/1 は、並びを U+FFFD に置き換えた `replace` と同じく項の終わりまで（構文の誤りなら、ほかの構文の誤りと同じく読み飛ばした後まで）読み、元のバイトに戻してから誤りを出す。その項は捨てる。`foo(<E3>).` の後に `bar.` があれば、`representation_error(character)` の次に `bar` を返す。注釈の中の並びは後ろの項の読みに属するので、その項も捨てる（`% <E9>` の後に `r.` と `ok.` があれば、誤りの次は `ok`）。並びの位置を求めるときは、字句解析と同じく先頭の BOM を飛ばす。テスト: `tests/pl/chars.pl` の `l6_*`、`tests/cpp/stream_test.cpp`（`check_invalid_utf8` の「utf flag advances」と「utf error advances」、`check_read_term` の「read utf advances」、`check_get_code` の「peek code stays on bad utf」。最後のものはファイルを壊れたバイト 2 つにした。前はどれも、流れが壊れたバイトの前に留まることを期待していた）。

## 第4節 旗 japanese_punctuation

決定（2026-10-25）: 未決 13 は A。`set_prolog_flag(japanese_punctuation, true)` は値として `true` を保存する。字句解析は変えない。

決定（2026-10-28）: フェーズ5で旗を字句解析に効かせる（SPEC 第4節の表）。引用、注釈、`0'c` の外では `（）［］，、：` を `()[],,:` として読む。`。` は後ろが配置文字か入力の終わりのときだけ終止符で、それ以外は記号文字のまま。だから `1。5` は小数にならない。`「` は引用原子を開き、`」` が閉じる。旗はファイルごと（SPEC 第5節）で、指令の次の節から効く。

決定（2026-10-31）: レビュー H1 の続きは A。`。` は後ろが `%` のときも `.` と同じく終止符にする（6.4.8、SPEC 第4節の表で `。` は `.`）。だから `w（x）。% c` は `w(x)` と読む。

解釈（2026-11-03、レビュー M8）: 「`writeq/1` は常に半角の形で書く」は、writer が自分で書く区切りの字の話である。原子 `'、'` は原子 `','` ではないので 7.10.5 h に従う。`quoted(true)` では、引用しないとその原子として読み戻せない原子を引用する。旗が真の間は、`（` `）` `，` `、` `：` を含むか `。` で終わる（後ろに配置文字が来ると節が終わる）記号文字の原子を引用する。`writeq(f('、'))` は `f('、')`、`writeq('：'(a))` は `'：'(a)` と書く。原子の途中の `。` の後ろは記号文字なので `。+` は引用しない。`［］「」` は前から引用している。旗が偽ならこれらは記号文字で、引用しない（`f(、)`）。引用する書き出し（writeq、write_canonical、`quoted(true)` の write_term、format の `~q`）、トップレベルの答え、メッセージの引数（字面にしてカタログの問い合わせで読み直す）、読んだゴールを書いて読み直す REPL は、書く時点の旗の値を使う。退けた案: 旗に関わらずこれらの字を引用し、どちらの旗でも読み戻せるようにする。7.10.5 h はいまの読み手に要るものだけを引用し、既定のモードの `f(、)` のような出力が変わるため。

## 第7節 旗 message_language

決定（2026-10-28）: ADR 0007（持ち主が承認）。実装定義の旗（7.11.3）で、`ja` か `en` を取り、変えられる。他の原子は `domain_error(flag_value, message_language+V)`。既定は `LANG` が `ja` で始まれば `ja`、それ以外は `en`。最上位の表示のカタログを選ぶだけで、誤りの項は変えない。

## 7.11 旗 version

決定（2026-10-25）: 未決 14 は A。`version` は読み取り専用のアトムで、文字列は `version_line()` と同じである。

## 第3節 旗 iso

決定（2026-10-25）: 未決 15 は A。`iso` の既定は `true` で、読み取り専用である。フェーズ3で新しい挙動は足さない。

## 8.12 テキストの改行

決定（2026-10-25）: 未決 16 は A。テキストファイルもメモリストリームも、どの OS でも改行は U+000A 1 個。C++ のテキストモードには任せない。

## 8.11 ストリーム項

決定（2026-10-25）: ストリーム項は常に `'$stream'(N)`。`N` はストリームのベクタの添字。別名は別の名前であり、項の形を変えない。

## 8.11 標準ストリーム

決定（2026-10-25）: 添字 0 が `user_input` で stdin、1 が `user_output` で stdout、2 が `user_error` で stderr。`reposition` は false。

## 8.11 終端の次の読み

決定（2026-10-25）: 既定の `eof_action` は `eof_code`。終端の次の読みは、テキストが `end_of_file`、バイナリが `-1`。`eof_action(reset)` は別途選べる。

2026-10-28 から `user_input` だけは例外で、`eof_action(reset)` を持つ（下の「8.11 標準ストリームの性質」）。

## 8.11 空の入力

決定（2026-10-25）: 位置が源の長さと等しいとき `end_of_stream` は `at`。空の入力を開いた直後は `at`。

## 8.11 位置の項

決定（2026-10-25）: `stream_position(CharNo, Line, LinePos)`。文字数は 0 から、行と行内位置は 1 から。

## 8.11.5.3 f ストリーム引数が具体値

決定（2026-10-25）: B。Cor.2 の 7.12.2 と 8.11.5.3 f のとおり `uninstantiation_error(Stream)`。`open_string/2` の `Stream` もモードが `-` なので同じ Formal。仕様第3節の一覧に `uninstantiation_error/1` を足した。

## 7.10.2.11 eof_action(reset)

決定（2026-10-26）: A。終端のあと `reset` で読むと、位置を先頭に戻して読み直す。テスト `reset rereads`。7.10.2.11 は、終端を越えた印を下ろして、もう一度読む、とだけ書く。ファイルは開いたときにバイト列へ写すので、印だけ下ろすと位置は末尾のままになり、次も終端で `eof_code` と同じになる。承認済みの計画は「ファイルをもう一度読む」なので、先頭へ戻す。これは 7.10.2.11 からの逸脱として残す。

## 7.8.3 call/1 と節の無い multifile・discontiguous

Logtalk の `lgt_call_1_17` と `lgt_call_1_18` は、節の無い `fenix/1`（multifile）と `scattered/2`（discontiguous）の `call/1` を失敗と期待する。7.4.2 の決定と `tests/pl/directives.pl` は、宣言だけでは手続きを作らず、`unknown=error` では `existence_error(procedure, PI)` である。`dynamic/1` だけが空の手続きを作る、と 7.4.2 では読んだ。

A か B か: A は 7.4.2 のまま `existence_error`。B は指示だけで手続きがあり、節が無ければ `call/1` は失敗する。確認前に B は実装しない。

決定（2026-10-28）: B。`multifile/1` か `discontiguous/1` の指示だけで節の無い手続きになり、`unknown=error` でも呼べば失敗する。変わるのは呼び出しだけである。`current_predicate/1` は今もこの述語を挙げず、`assertz/1` は今も動的な述語として作るので、multifile のフックへ節を足せる。失敗した読み込みは、ほかと一緒にこの宣言も戻す（`src/machine/load.cpp` の `declared_only`、`tests/pl/directives.pl` の `*_only_call*` と `mf_hook_assert`、`tests/cpp/machine_test.cpp`）。

## 8.11.1.3 current_input のドメイン

決定（2026-10-26）: A。`current_input/1`、`current_output/1`、`stream_property/2` に流れでも別名でもない項を渡すと `domain_error(stream_or_alias, S)`。テスト `input_bad`、`output_bad`、`prop_bad_stream`。別名 `user_input` は `current_input(user_input)` で成功する。領域に別名が入るので、Formal は `stream` ではなく `stream_or_alias`。8.11.1.3 a と 8.11.8.3 a の `domain_error(stream, S)` からの逸脱として残す。`set_input/1` など、もとから `stream_or_alias` の述語は変えない。

## 8 with_output_to のメモリ

決定（2026-10-26）: A。`atom/1` と `codes/1` のために作ったメモリの流れは、成功でも失敗でも例外でも停止でも閉じる。成功のときはバイトを束縛してから閉じる。閉じたスロットは再利用しない。その後、`stream_property` は列挙で拾わない。その項を渡すと `existence_error(stream, S)`。呼んだ側が渡した流れは閉じない。ゴールの途中は開いたままなので、中の `current_output` と `stream_property` は見える。

## 準拠スイート

決定（2026-10-26）: 許可は待たない。`inriasuite.pl` は再配布の許諾が無いので同梱しない。フェーズ3の準拠スイートは Logtalk の `tests/prolog` を、コミット `4decb64b598feff542a926a085eef01ec51f5a72` のまま `tests/conformance/logtalk/prolog` に置く。ライセンスはリポジトリと同じ Apache-2.0 で、著作権表示は残す。ファイルは改変しない。仕様第9節の 90% はこのスイートで測る。合格率はこの決定では出さない。

決定（2026-10-28）: A。試験器が 6 ライブラリを取り込むようになったが、別欄のディレクトリ（`between_3`、`succ_2`、`plus_3`、`format_2`、`format_3`、`atomic_list_concat_2`、`atomic_list_concat_3`）はフェーズ5の間は実行しない。分母は 2171 のままにし、フェーズ5の出口は同じ基準値（2041）と比べる。これらの述語は `tests/pl/` のライブラリのテストが確かめる。

## ライセンス

決定（2026-10-26）: リポジトリは Apache-2.0 に揃える。本文は `LICENSE`。Mosa の著作権表示は `Copyright 2026 Mosa Prolog contributors` で、`NOTICE` に置く。Logtalk のテストも同じライセンスで、著作権は Paulo Moura の表示のままである。

## 9.3.1.3 d (**)/2 の 0 の負べき

`lgt_power_2_14` は `**(0, -1)` を `evaluation_error(undefined)` か `evaluation_error(zero_divisor)` と期待する。ECLiPSe の IsoErrata は、9.3.1.3.d の文面が undefined であり、無限大が定義されるので zero_divisor であるべきだと書く。Corrigendum 2 の 9.3.10.3 d は `(^)/2` の同じ形を undefined のまま残している。今は `std::pow(0, 負)` が無限大なので `evaluation_error(float_overflow)` である。

A か B か: A は 9.3.1.3.d の文面どおり `evaluation_error(undefined)`。B はエラータのとおり `evaluation_error(zero_divisor)`。確認前に `float_overflow` は変えない。

決定（2026-10-28）: A。0（整数、`0.0`、`-0.0`）の負の冪は、`(**)/2` でも浮動小数点数の `(^)/2` でも `evaluation_error(undefined)` にする。整数の `(^)/2` は以前からそうである。`float_undefined` 旗が `error` でないときは、ほかの undefined と同じく NaN を返す（`src/builtins/arith.cpp` の `eval_float_bin`、`tests/pl/arith.pl` の `pow_zero_*`）。

## 7.5.4 開いた走査と retract

`logical_update_semantics_retract` は、走査の途中で `retract` しても、呼んだときの節を最後まで試すことを期待する。`retractall` と `abolish` の同じ形も同じである。ADR 0004 は、retract が以後の候補からその節を外し、今の節の残りは残す、と決めている。`tests/pl/database.pl` の `retract_open_later` は `[X = 1, X = 2]` であり、消した `later(3)` は出てこない。

A か B か: A は ADR 0004 のまま、開いた走査から消した節を外す。B は 7.5.4 のとおり、その呼出しは最初の列を試す。確認前に A のままにする。

決定（2026-10-28）: B。設計は ADR 0008 に書き、ADR 0004 の retract と abolish の部分を置き換える。ADR 0008 は 2026-10-28 に承認し、実装した（死んだ節は `src/builtins/database.cpp`。`tests/pl/database.pl` の `lu_*` と `retract_open_later`、`tests/pl/solutions.pl` の `scan_retract` は、呼んだ時点の節をすべて期待する）。

## 8.11.8 reposition(false) の position

`sics_set_stream_position_2_06` は `reposition(false)` で開いた流れから `stream_property(Stream, position(Position))` を取り、`set_stream_position(Stream, Position)` が `permission_error(reposition, stream, Stream)` になることを期待する。ファイルを写したあとの実際は `fail` である。`stream_property/2` は戻せない流れに `position/1` を出さない（`tests/cpp/stream_test.cpp` の "no position"）。`set_stream_position/2` は、同じ流れに位置の項を渡すとすでに `permission_error(reposition, stream, Stream)` を投げる（同ファイルの "reposition formal"）。

A か B か: A は性質を出さない（今の実装。ゴールは失敗する）。B は `position/1` を出してから `permission_error` にする。確認前は A のまま。B は実装しない。

決定（2026-10-28）: A。位置を戻せないストリームは `position/1` を返さない。返しても使い道が無い。`sics_set_stream_position_2_06` は解釈の違いとして不合格のまま残し、`tests/cpp/stream_test.cpp` の「no position」も変えない。

## memberchk/2 と iso_stream_property_2_01

`memberchk/2` は `library(lists)` であり、フェーズ 3 の組込にはしない。`iso_stream_property_2_01` の表明は、テストファイルにある補助節を、その時点の利用者の節として実行する。行を `fail` のままにする、という読みは撤回する。

A か B か: A は組込の `memberchk/2` を足さない。B は組込として足す。確認前は A。B は実装しない。

閉じた（2026-10-28）: フェーズ 5 のタスク 2 からスイートの実行器が `library(lists)` をテスト用の機械へ取り込むので、`iso_stream_property_2_01` は組込の `memberchk/2` 無しで合格する。この問いで決まる行はもう無いので取り下げ、何も足さない。

## 8.11.1 閉じた流れの current_input と current_output

`sics_current_input_1_04` は、閉じた入力流れ `S` に `current_input(S)` を呼び、`domain_error(stream, S)` を期待する。実際は `existence_error(stream, '$stream'(3))`。`sics_current_output_1_04` は閉じた出力流れへの `current_output(S)` で、同じ食い違いである。8.11.1.3 と 8.11.2.3 が書く `domain_error(stream, Stream)` は、変数でも流れでもない項である。閉じた流れの項は流れの項のままなので、開いていない流れの `existence_error(stream, S)`（`set_input/1` の 8.11.3.3 と同じ Formal）にしている。`tests/pl/stream_prop.pl` の `set_closed` と `out_closed`、読み書きの閉じた流れも同じ Formal である。流れでない項の `stream_or_alias`（上の 8.11.1.3 の決定）は変えない。

A か B か: A は閉じた流れの `current_input/1` と `current_output/1` が `existence_error(stream, S)`（今の実装）。`sics_current_input_1_04` と `sics_current_output_1_04` は残る。B は `domain_error(stream, S)`。確認前は A。B は実装しない。

決定（2026-10-28）: B。`current_input/1` と `current_output/1` は、閉じた流れの項に `domain_error(stream, S)` を投げる。8.11.1.3 と 8.11.2.3 が挙げる誤りはこれだけである。`'$stream'(99)` のように一度も流れでなかった項は今も `existence_error(stream, S)`、変数でも流れでもない項は今も `domain_error(stream_or_alias, S)`（上の 8.11.1.3 の決定）。ほかの組込は、閉じた流れに今も `existence_error(stream, S)` を投げる（`src/builtins/stream_prop.cpp` の `match_current`、`tests/pl/stream_prop.pl` の `input_closed` と `output_closed`）。

## 第5節 自動取り込み

決定（2026-10-28）: A。`append/3`、`between/3`、`format/2` などのライブラリの述語は、`use_module` した後にだけ見える。起動時に `user` へ何も取り込まず、未定義の述語を呼んだときにライブラリを読むこともしない。SPEC 第5節の解決規則（自モジュール、既定の取り込み元、`system`）を保ち、利用者のプログラムが自分の `append/3` や `member/2` を取り込みの衝突なしに定義でき、フェーズ6の `mosac` が `use_module` の推移閉包だけをリンクする規則とも合う。宣言なしにこれらを呼ぶ準拠テストは、処理系を変えず、試験器がテストのモジュールへ 6 ライブラリを取り込んで扱う。

## 第5節 モジュールの中の手続きの existence_error

SPEC 第5節は、`user` 以外のモジュールで見つからない述語を `existence_error(procedure, PI)` の culprit でどう書くかを決めていない。ISO の例は `user` で動き、限定のない `Name/Arity` を期待する。

A か B か: A は文脈モジュールが `user` なら `Name/Arity`、それ以外なら `Module:Name/Arity`。B は常に `Name/Arity`。確認まで実装しない。

決定（2026-10-28）: A。`user` の中では ISO の例どおり culprit を `Name/Arity` のままにし、それ以外のモジュールでは `Module:Name/Arity` にして、どこで見つからなかったかを示す。

## 第5節 取り込みの衝突

SPEC 第5節は、自モジュールの述語と取り込んだ述語の衝突を読み込み時の `permission_error(import_into, ...)` とするが、残り 2 つの引数を決めていない。

A か B か: A は `permission_error(import_into, procedure, Name/Arity)`。B は `permission_error(import_into, procedure, Module:Name/Arity)`（`Module` は取り込み元）。確認まで実装しない。

決定（2026-10-28）: B。culprit は取り込み元のモジュールを付けた `Module:Name/Arity` とし、どの取り込みがぶつかったかを示す。上の `existence_error` で選んだモジュール付きの culprit とそろえる。

## 第5節 原子でないモジュールの Module:Goal

SPEC 第5節は未知のモジュールを `existence_error(module, M)` と決めるが、`M` が変数でも原子でもない場合（`1:true`）を決めていない。変数は `instantiation_error` である。

A か B か: A は `type_error(atom, M)`。B は `type_error(module, M)`。確認まで実装しない。

決定（2026-10-28）: A。`1:true` は `type_error(atom, 1)` とする。ISO 13211-1 に `module` という型は無いので、既存の型名の範囲に収める。

## 第5節 演算子 meta_predicate と as

SPEC 第5節の例は `:- meta_predicate 全員(1, ?).` と `[姓名分解/3 as 分解]` を書く。これを読むには `meta_predicate` と `as` が演算子でなければならないが、どちらも ISO の演算子表に無い。

A か B か: A は `op(1150, fx, meta_predicate)` と `op(700, xfx, as)` を `system` の演算子表に足し、すべてのモジュールで見えるようにする。B はどちらも足さず、`meta_predicate(...)` と `as(PI, Alias)` の標準形だけを受ける。確認まで実装しない。

決定（2026-10-28）: A。2 つの演算子を `system` の演算子表に足し、SPEC 第5節の例がどのモジュールでも書いたとおりに読めるようにする。これは ISO の演算子表（6.3.4.4）からの逸脱であり、`as` を演算子の位置で普通の原子として使うプログラムは読み方が変わる。

## 第5節 演算子 :

SPEC 第5節は `?- 家系:祖父(太郎, Z).` と書く。これを読むには `:` が中置の演算子でなければならない。`:` は ISO の演算子表（6.3.4.4）に無く、`src/read/operator_table.cpp` も意図して入れていない。

A か B か: A は `op(600, xfy, :)` を `system` の演算子表に足す。SWI-Prolog と同じで、Logtalk の判定器が一時的に足す演算子とも一致する。B は何も足さず、`:(M, G)` の標準形だけを受ける。

決定（2026-10-28）: A。`op(600, xfy, :)` を `system` の演算子表に置く。上の `meta_predicate` と `as` と同じく、ISO の演算子表（6.3.4.4）からの逸脱である。

## 第5節 Module:Goal の中のカット

SPEC 第5節は `Module:Goal` が `Goal` を `Module` の文脈で動かすと書くが、`Goal` の中のカットがどこまで届くかを決めていない。ISO 13211-1 の 7.8 では `call/1` はカットに不透明で、`,/2`、`;/2`、`->/2` は透明である。

A か B か: A は透明。`p :- member(X, L), user:!.` の `!` は `p` の節まで切る。SWI-Prolog が限定付きの本体ゴールをコンパイルするときと同じである。B は `call/1` と同じく不透明で、カットは `Module:Goal` の中だけに効く。

決定（2026-10-28）: A。`:/2` はカットに透明とする。`call/N` を通したゴールは、`call/N` 自身がカットの境界を置くので不透明のままである。

## 第5節 呼び出せないゴールの Module:Goal

SPEC 第5節は、`user:(fail, 1)` のように `Module:Goal` の `Goal` を呼び出せないときの `type_error(callable, ...)` の culprit を決めていない。ISO 13211-1 の 7.8.3.3 は `call/1` の引数を culprit とする。

A か B か: A は `Module:Goal` 全体。`call(user:(fail, 1))` は `call/1` に渡した項をそのまま示し、`call(user:1, a)` が `user:1` を示すのともそろう。B は `(fail, 1)` のように `Goal` だけ。

決定（2026-10-28）: A。culprit は `Module:Goal` 全体とする。

## 第5節 取り込み同士の衝突

SPEC 第5節は、局所の述語と取り込んだ述語が 1 つの名前空間を共有すると書き、局所の定義と取り込みがぶつかったときのエラーを決めている。1 つのモジュールが同じ `Name/Arity` を別々の 2 つのモジュールから取り込んだときは決めていない。

A か B か: A は、2 つ目の取り込みで読み込みを `permission_error(import_into, procedure, From:Name/Arity)` で失敗させる。`From` は 2 つ目の取り込み元である。同じ取り込み元からもう一度取り込むのは許す。B は、後の取り込みが前を黙って置き換える。

決定（2026-10-28）: A。1 つの名前に取り込み元が 2 つあるのは衝突であり、局所の定義と取り込みがぶつかったときと同じ Formal にする。

## 第5節 公開されていない述語の取り込み

SPEC 第5節は `use_module(File, Imports)` で取り込む述語を指定できるとするが、指定した述語が読み込んだモジュールの公開リストに無いときを決めていない。

A か B か: A は、読み込みを `existence_error(procedure, Module:Name/Arity)` で失敗させる。B は、SWI-Prolog が警告付きで行うように、それでも取り込む。どちらでも、`except([...])` の中の公開されていない項目は無視する。

決定（2026-10-28）: A。取り込めるのは公開された述語だけで、無いものは `existence_error(procedure, Module:Name/Arity)` で読み込みを失敗させる。

## 第5節 モジュールでないファイルの use_module

SPEC 第5節は 1 ファイルが 1 モジュールに対応すると書くが、最初の項が `module/2` でないファイルを `use_module/1,2` がどう扱うかを決めていない。

A か B か: A は、読み込みを `permission_error(load, source_sink, File)` で失敗させ、読んだ節も戻す。SWI-Prolog もモジュールでないファイルを拒む。B は、`consult/1` と同じく文脈モジュールに読み、取り込むものは無い。

決定（2026-10-28）: A。`use_module/1,2` はモジュールのファイルだけを受ける。

## 第5節 初期化の例外で途中までしか読まれていないファイル

入れ子の読み込みの `initialization/1` のゴールが例外を投げると、それまでに読んだ節は残り、外側のファイルはその指令で読むのをやめる。外側のファイルには読み込み済みの印が残るので、後の `use_module/1,2` や `ensure_loaded/1` は残りを読まず、作りかけのモジュールが見えていた。

A か B か: A は、最後まで読んでいないファイルの印を外し、次の `use_module/1,2` か `ensure_loaded/1` で読み直す。節とモジュールは今どおり残す。B は印を残す。

決定（2026-10-28）: A。

## 第5節 未知のモジュールへの節操作

SPEC 第5節は `assertz(家系:親(a, b))` を許すが、そのモジュールがまだ無いときを決めていない。

A か B か: A は `M:G` と同じく `existence_error(module, M)`。B は、SWI-Prolog と同じくモジュールを作って節を足す。

決定（2026-10-28）: A。モジュールを作るのは `module/2` だけである。未知の `M:` を付けた `asserta/1`、`assertz/1`、`retract/1`、`retractall/1`、`clause/2`、`abolish/1` は `existence_error(module, M)` にする。

## 第5節 取り込んだ名前への節操作

SPEC 第5節は `assertz/1` などで作った述語が、宣言または追加したモジュールに属すると書くが、文脈モジュールがその名前を取り込んでいるときを決めていない。

A か B か: A は、呼び出しと同じく取り込みをたどり、取り込み元のモジュールの定義を操作する。B は文脈モジュール自身の述語だけを見て、`assertz/1` は取り込みとぶつかる。

決定（2026-10-28）: A。データベースの組み込み述語は、モジュール自身の述語、次に明示した取り込み（`use_module/1,2` と、consult したモジュールのファイルの公開）を見る。既定の取り込み元はたどらないので、モジュールの中の `assertz(p(1))` が `user:p/1` に足されることはない。存在エラーと同じく、`permission_error(modify, static_procedure, PI)` も `user` の外の述語を `M:N/A` で示す。

## 8.8.2 モジュールの中の current_predicate/1

SPEC 第5節は、`current_predicate(N/A)` と `current_predicate(M:N/A)` が何を列挙するかを決めていない。

A か B か: A は、そのモジュール自身の述語だけ。`M` が変数なら全モジュールをたどり、`M` も束縛する。B は、自身の述語と取り込んだ述語。

決定（2026-10-28）: A。限定の無い `current_predicate/1` は文脈モジュール自身の述語を列挙するので、`user` での結果はモジュールを入れる前と同じである。

## 8.4.3 sort/2

`sort/2` は ISO の組み込み（8.4.3）だが、SPEC 第7節の段階表に無く、実装もされていない。同梱スイートの `sort_2` のテストは `docs/conformance.md` の別欄にある。`keysort/2`（8.4.4）は段階 G に system として載っている。

A か B か: A はフェーズ4の Task 12 で `keysort/2` と並べて `sort/2` を `system` に作り、SPEC 第7節の段階 G に足す。B はフェーズ5へ回す。確認まで実装しない。

決定（2026-10-28）: A。`sort/2` はフェーズ4の Task 12 で `keysort/2` と並べて `system` に作り、8.4.3.4 の Examples をテストにする。SPEC 第7節の段階 G に載せた。

## 第7節 forall/2

`forall/2` は SPEC 第7節の段階 G（「非 ISO の便利述語（`library(*)`、第5節の表）」）に載り、第5節もモジュールを跨いで動くべき述語に挙げるが、第5節の表の 6 モジュールのどれも公開していない。

A か B か: A はフェーズ4の Task 9 で `forall/2` を meta 述語として `system` に作る。B はフェーズ4では作らない。確認まで実装しない。

決定（2026-10-28）: A。`forall/2` はフェーズ4の Task 9 で meta 述語として `system` に作り、呼び手の文脈モジュールで `\+ (Cond, \+ Action)` を動かす。6 モジュールの外にある段階 G の述語でフェーズ4が足すのは、これだけである。

## 第7節 段階 G の便利述語

レビュー M11。SPEC 第7節の段階 G は `tab/1,2`、`term_to_atom/2`、`nb_setval/2`、`nb_getval/2`、`setup_call_cleanup/3`、`sort/4`、`numbervars/3`（そこでは `number_vars/3` と書く）を挙げるが、SPEC 第9節のどのフェーズもこれを名指さず、フェーズ4が足したのは第5節の表の 6 モジュールと `forall/2` だけで、どれも存在しないので呼ぶと `existence_error` になる。

A か B か: A はフェーズ7で、マニュアルとリリースパッケージの前に、`forall/2` と同じく `system` に作る。どれも ISO ではないので、引数の形と誤りは SWI-Prolog に合わせる。`setup_call_cleanup/3` は抜けたとき・失敗・カット・例外で後始末を動かす仕組みがエンジンに要るので、先に ADR 0016 を書く。SPEC の `number_vars/3` は `numbervars/3` に直す。B は今回のリリースに入れず（存在の誤りのまま）、段階 G から消し、マニュアルにそう書く。確認まで実装しない。

決定（2026-11-03）: A（レビュー M11）。7 つの述語はフェーズ7で `system` に作り、1 述語ずつコミットし、テストは `tests/pl/utilities.pl` に置く。SWI-Prolog の振る舞いがマニュアルとソースから読み取れないところは、述語のファイルの見出しのコメントに選んだ形を書く。SPEC 第7節は `numbervars/3` と書く。

2026-11-03 に実装した（`setup_call_cleanup/3` は ADR 0016）。SWI-Prolog と違うところ: `nb_getval/2` はヒープが後戻りで巻き戻るので、呼ぶたびに写しを返す。`sort/4` は整数の Key だけを受け、SWI の辞書の鍵と鍵の道は Mosa に辞書が無いので `type_error(sort_key, Key)` にする。`term_to_atom/2` は `[]` を原子 `'[]'` として読み、空の字面と閉じない字面は `syntax_error(incomplete_term)` にする（SWI 9.3.5 以降は `end_of_string`）。`numbervars/3` は 64 ビットの整数を超えると `representation_error(max_integer)`（SWI は `tagged_integer`）。`setup_call_cleanup/3` は `halt` とヒープの上限での停止では後始末を動かさず、ゴールが例外を投げたときは後始末の例外を捨て（SWI は急ぐ方を残す）、1 回のカットが外した後始末は全部動かしてから、その例外のうち最初のものを投げ直し、埋め込みの API で新しい問い合わせが古いものを閉じるときの後始末の例外は無視する（`-g` のゴール、トップレベルの答え、`initialization(G, main)` は SWI と同じく答えの後で切り、その後始末の例外はゴールの例外になる）。

## ISO/IEC 13211-2 7.2.2 predicate_property/2

SPEC 第5節は段階 F に `predicate_property/2` を挙げるが、頭部が変数のときと、無いモジュールを指したときを決めていない。

A か B か: A は、頭部が変数なら 13211-2 の DTR と同梱のスイート（`commons_predicate_property_2_16`）のとおり `instantiation_error`、無い `M:` なら `M:G` と同じ `existence_error(module, M)`。B は、SWI-Prolog のように見える述語を列挙し、無いモジュールでは失敗する。

決定（2026-10-28）: どちらも A。組み込み述語の性質は `defined`、`built_in`、`static` で、制御構文とゴールを受ける組み込み述語には `meta_predicate(Head)`（例: `findall(?, 0, -)`）も付く。利用者の述語は呼び出しと同じ順で引き、性質はこの順に `defined`、`dynamic` か `static`、`exported`、`imported_from(M)`（文脈モジュールの明示の取り込みのときだけ）、`multifile`、`meta_predicate(Head)`（宣言したとき）、`number_of_clauses(N)`。それ以外の性質の項は `domain_error(predicate_property, P)`。

## 第5節 モジュールの中の演算子

SPEC 第5節は、`op/3` を宣言したモジュールに閉じ、公開リストの `op(P, T, N)` を取り込み側でも効かせ、`user` の演算子をすべてのモジュールに見せると決める。モジュールの中の `op(0, T, N)` の働きと、取り込みリスト `[PI, ...]` が公開の演算子を持ち込むかは決めていない。

A か B か: A は、モジュールの中の `op(0, T, N)` がそのモジュールの中だけ `user` と ISO の核の同じ類の演算子を隠す（SWI-Prolog と同じ）。公開の演算子は `use_module/1`、`except(...)`、consult したモジュールのファイルで入り、取り込みリストでは入らない。B は、`op(0, T, N)` がそのモジュール自身の定義だけを消し、公開の演算子はどの取り込みでも入る。

決定（2026-10-28）: どちらも A。引く順はモジュールの固有層、次に `user` の表（ISO の核を含む）。`op/3`、`current_op/3`、`read_term/2,3` と `write` の仲間は呼び手の文脈モジュールを使う。モジュールのファイルの `:- op` 指令はそのモジュールへ入る。`user` へ取り込んだ演算子は `user` の表に入るので、すべてのモジュールから見える。

改訂（2026-11-06、レビュー L19 の A1 への持ち主の決定 A、ADR 0006 の改訂 1）: `library(...)` で見つけたファイルを読む間は、`user` の表の代わりに ISO の核を置く。そのファイルは自分の固有層と核で読まれる。ファイルの `user:op/3` は、核と退けた `user` の表の両方へその場で入り、どちらかが拒否すれば拒否される。

## 7.4.2.8 ファイルごとの旗と initialization/1

SPEC 第5節は、`double_quotes` と `japanese_punctuation` を読み込むファイルごとに戻す。7.4.2.8 は `initialization/1` のゴールを Prolog テキストの準備が終わった後に実行するが、そのファイルが設定した旗が見えるかは決めていない。

A か B か: A は、旗を戻す前にゴールを実行し、ファイルの旗のまま見せる。B は、先に旗を戻し、読み込み直後のクエリと同じ旗を見せる。

決定（2026-10-28）: B。ファイル自身の項はすでにその旗で読まれている。違いが出るのは、ゴールが実行中に読むものと `current_prolog_flag/2` だけである。ファイルの書式で読みたいゴールは、自分で旗を設定する。

## 第5節 負の整数を受けた succ/2

`succ/2` は規格に無い。フェーズ4の計画は SWI-Prolog と同じ `type_error(not_less_than_zero, -1)` を期待し、Logtalk のスイート（`lgt_succ_2_09`、`lgt_succ_2_10`）は `domain_error(not_less_than_zero, -1)` を期待する。

A か B か: A は `length/2` と同じ `domain_error(not_less_than_zero, N)`。B は `type_error(not_less_than_zero, N)`。

決定（2026-10-28）: A。`not_less_than_zero` は 7.12.2 c の領域であり、`length/2` もすでに領域の誤りとして投げている。

## 第5節 format/2 の誤り

`format/1,2,3` は規格に無い。Logtalk のスイート（`format_2`）は引数の誤りを ISO の形で決めている。`~c`、`~d`、`~e`、`~f`、`~n` と `~*` の回数や数の引数は `is/2` と同じく評価し、`~d` は整数の結果を要し（`type_error(integer, X)`）、`~a` は原子を要する（`type_error(atom, X)`）。引数の不足と余りには、どの `domain_error(_, _)` でも受ける。

A か B か: A は問題ごとの Formal。引数が尽きたら `domain_error(non_empty_list, [])`、残ったら `domain_error(empty_list, 残り)`、未知の指示の文字 `C` は `domain_error(format_directive, C)`（末尾の単独の `~` は `'~'`）。B は 3 つとも `domain_error(format_control, Format)` で、書式全体を罪人にする。

決定（2026-10-28）: A。`non_empty_list` は 7.12.2 c の領域であり、罪人からどこが悪いかが分かる。負の回数は `length/2` と同じく `domain_error(not_less_than_zero, N)`。

## 第4節 format/2 の埋め草の字を幅で数える

SPEC 第4節は `~t~|` の桁を東アジア幅で数える。幅 2 の埋め草の字（``~`漢t``）では、奇数の桁をちょうど埋められないことがある。``~`漢t~5|`` は `漢` を 2 つ置くと 1 桁余る。

A か B か: A: 入るだけ埋め草の字を置き、余った桁は空白で埋めて桁位置を守る。B: 埋め草の字をもう 1 つ置き、桁位置を 1 つはみ出す。

決定（2026-10-29）: A。`~|` が約束するのは桁位置で、SPEC 第4節はそれを幅で測る。幅 0 の埋め草の字は幅 1 として数え、埋め草が必ず尽きるようにする。

## 7.9.1 関数子と引数のどちらの誤りを先に出すか

`X is foo(bar)` では `foo/1` も `bar/0` も評価できない。7.12.1 は 2 つの誤りの条件のどちらを出すかを処理系に任せる。Mosa は引数を先に評価して `bar/0` を出していた。Logtalk のスイート（`format_2` の `foo(bar)` を渡す `~d` と `~f`）は `foo/1` を期待する。

A か B か: A は今のまま引数を先に評価する（`type_error(evaluable, bar/0)`）。B は引数を評価する前に主関数子を確かめる（`type_error(evaluable, foo/1)`）。

決定（2026-10-28）: B。7.9.1 は主関数子が評価できるかで分かれてから引数を評価する。外側の関数子が悪ければ、引数に手を付ける前に止まる。

## 第5節 結果が数の atomic_list_concat/2,3

`atomic_list_concat/2,3` は規格に無い。Logtalk のスイート（`atomic_list_concat_2`、`atomic_list_concat_3`）は結果の引数が複合項なら `type_error(atom, a(1))` を期待するが、`atomic_list_concat([1, 2], 12)` の `12` のような数の扱いは決めていない。

A か B か: A は `type_error(atom, 12)`。結果の引数は変数か原子に限る。B は文字として比べ、成功させる。

決定（2026-10-28）: A。8.16.2.3 は `atom_concat/3` の第3引数に同じ決まりを置いており、Logtalk の複合項の場合とも揃う。

## 第5節 空の区切りで分ける atomic_list_concat/3

`atomic_list_concat/3` が分けるとき（リストが決まっておらず原子が決まっているとき）、区切りが空の `''` では分け方が 1 つに決まらない。

A か B か: A は `domain_error(non_empty_atom, '')`。B は失敗させるか、1 文字ずつに分ける。

決定（2026-10-28）: A。黙って失敗するより誤りの方が何が悪いかを伝え、1 文字ずつ分けるのは `atom_chars/2` の仕事である。

## 第5節 as の別名を通した節操作

`use_module(別源, [静/1 as 静か])` の後の `assertz(静か(2))` は、別名 `静か/1` を通して静的な手続き `別源:静/1` を変えようとする。8.9.1.3 は `permission_error(modify, static_procedure, PI)` で手続きを示すが、呼び手が別名でたどり着いたときにどちらの名前を使うかは決めていない。

A か B か: A は取り込み元の手続き `別源:静/1`。別名でない取り込み（`家系:親/2`）と同じ形。B は呼び手が書いた別名 `静か/1`。

決定（2026-10-28）: A。誤りは変えられない手続きについてのもので、別名はその手続きの別の名前にすぎない。別名でない取り込みはすでに取り込み元を出しており、罪人のモジュールから、どの宣言を dynamic にすればよいかが分かる。

## 第3節 if/3

`if/3` は規格にも SPEC 第3節にも無い。`control/if_3` の 12 件はこれを呼び、`existence_error(procedure, if/3)` になる。

A か B か: A は SPEC 外なので実装せず、12 行は不合格のまま残す。B は SPEC 第3節に `if/3` を足して実装する。確認前は A のまま。

決定（2026-10-28）: B。`if/3` は `system` の組込とし、SPEC 第7節の段階 A で `*->/2` の隣に挙げる。`if(C, T, E)` は呼び手の文脈モジュールで `(C *-> T ; E)` を走らせる。`call/1` と同じく、どの引数の切断も内に閉じ、呼べない引数があれば何も走らせる前に `type_error(callable, if(C, T, E))` を投げる（`src/builtins/control.cpp` の `run_if`、`tests/pl/control.pl` の `if_*`）。

## 第3節 旗 dialect と version_data

旗 `dialect` と `version_data` は 7.11 にも SPEC 第3節にも無い。`commons_current_prolog_flag_2_16` から `18` は `domain_error(prolog_flag, dialect)` と `domain_error(prolog_flag, version_data)` になる。

A か B か: A は SPEC 外なので実装せず、3 行は不合格のまま残す。B は SPEC 第3節に `dialect`（`mosa`）と `version_data`（`mosa(Major, Minor, Patch, [])`）を足して実装する。確認前は A のまま。

決定（2026-10-28）: B。どちらも変更できない旗とし、SPEC 第3節の 7 に挙げる。`version_data` の番号は `include/mosa/version.hpp` から取り、`mosa --version` も同じ番号を使う。どちらかを設定すると、値の形が合えば（原子、または引数 4 個の複合項）`permission_error(modify, flag, F)`、合わなければ `domain_error(flag_value, F+V)` になる（`src/builtins/flags.cpp`、`tests/pl/flags.pl` の `flag_dialect*` と `flag_version_data*`）。

## 第4節 ストリームの符号化と BOM

`unicode/builtins` の不合格 21 行は、IANA の名前 `'UTF-8'`、BOM、バイナリの流れに符号化が無いことを期待する。SPEC 第4節と第3節の 7 はすでに旗 `encoding` を `utf8` に固定し、すべての流れに既定が `utf8` の `encoding` 選択肢を与えており、Mosa はそれに従う。`current_prolog_flag(encoding, utf8)` であり、`open/4` は `encoding(utf8)` を受け付け、バイナリの流れも `encoding(utf8)` を出す。そのため `open/4` に `encoding('UTF-8')` を渡すと `domain_error(stream_option, encoding('UTF-8'))` になり、`lgt_unicode_current_prolog_flag_2_02` は名前で失敗し、`lgt_unicode_stream_property_2_07` と `09` はバイナリの流れが `encoding/1` を出すので失敗する。`open/4` の選択肢 `bom/1`、流れの性質 `bom/1`、テキストの流れを読むときの BOM の読み飛ばしは、規格（7.10.2）にも SPEC にも無い。

A か B か: A は SPEC のまま（名前は `utf8` だけ、すべての流れが `encoding/1` を出す、BOM は扱わない）で、21 行は不合格のまま残す。B は SPEC 第4節を改め、`'UTF-8'` を別名として受け付け、バイナリの流れから `encoding/1` を外し、選択肢と性質の `bom/1` を足し、テキストの入力で BOM を読み飛ばしてから実装する。確認前は A のまま。

決定（2026-10-28）: A。これらの選択肢、性質、名前は規格の外であり、SPEC はすでにすべての流れの符号化を `utf8` に固定している。21 行は解釈の違いとして不合格のまま残す。テキストの入力での BOM の読み飛ばしは、タスク 15 で別に問うことがある。

## 9.3.10.3 (^)/2 の負の整数の指数

`iso_integer_power_2_06` は `2^(-1)` が `type_error(float, 2)` になることを期待する。`src/builtins/arith.cpp` は Corrigendum 2 の 9.3.10.3 e を、指数が -1 より小さいときだけ float の型エラーと読み、`2^(-1)` は `evaluation_error(undefined)` にしている。

A か B か: A は今の読みのまま（`evaluation_error(undefined)`）。B は 1 と -1 以外の整数の底に負の整数の指数なら `type_error(float, Base)`。確認前は A のまま。

決定（2026-10-28）: B。0、1、-1 以外の整数の底に負の整数の指数なら `type_error(float, 底)` を投げ、`2^(-1)` も同じにする。`1^N` は 1、`(-1)^N` は `N` の偶奇で 1 か -1、`0^N` は今のまま `evaluation_error(undefined)`（9.3.10.3 d。9.3.1.3 d の決定と同じ）。`tests/pl/arith.pl` の `hat_neg` は、同じコミットで `undefined` から型の誤りに変える（`src/builtins/arith.cpp` の `negative_hat`）。

## 8.11 標準ストリームの性質

`sics_stream_property_2_05c` と `lgt_stream_property_2_08b` は、`user_input` が `eof_action(reset)` を出すことを期待する。`sics_stream_property_2_06d`、`lgt_stream_property_2_09c`、`lgt_stream_property_2_10c` は、`user_output` と `user_error` が `mode(append)` を出すことを期待する。Mosa は `eof_action(eof_code)`（「8.11 終端の次の読み」の既定）と `mode(write)` を出す。

A か B か: A は `eof_code` と `write` のまま。B は `user_input` を `eof_action(reset)`、`user_output` と `user_error` を `mode(append)` にする。確認前は A のまま。

決定（2026-10-28）: B。`user_input` は `eof_action(reset)` を返し、そのとおりに動く。終わりの後に読むと、終わりの印を下ろして OS にもう一度尋ねる。読んだ分は繰り返さないので、端末は Ctrl-D の後も読み続けられ、パイプは `end_of_file` を返し続ける。`user_output` と `user_error` は `mode(append)` を返す。`tests/pl/stream_prop.pl` の `all_std` と `tests/pl/write.pl` の `capture_leaves_output` は同じコミットで変える（`src/builtins/streams.cpp` の `standard_stream`、`src/builtins/chars.cpp` の `reset_at_end`、`tests/cpp/stream_test.cpp` の「standard reset」）。

## 仕様第4節 So の文字は記号文字

`lgt_unicode_escape_sequence_full_05` は `writeq('🀙🀚🀛🀜🀝🀞🀟🀠')` が原子を引用することを期待する。SPEC 第4節は `So` の非 ASCII 文字を記号文字にするので、この 8 文字は記号文字の原子（6.4.2）になり、`writeq`（7.10.5）は引用せずに書く。この行は解釈の違いとして不合格のまま残す。承認済みの SPEC に従うので、未決の問いは無い。

## 7.10.5 a 1 度だけ出る変数の書き方

`lgt_write_term_3_043`、`044`、`065`、`066` は、書いた変数が `_` の後に 1 文字以上続くことを期待する。Mosa は項の中に 1 度だけ出る変数を `_` だけで書き、2 度以上出る変数を `_G` と番号で書く（`f(_G1,_G1,_)`）。7.10.5 a は、変数の字面は処理系依存で `_` で始まるとだけ言う。

A か B か: A は 1 度だけ出る変数を `_` のままにする（今のライタ）。B はすべての変数に `_G1` のように `_` の後 1 文字以上の名前を付ける。確認前は A のまま。

決定（2026-10-28）: B。書き手は名前の無い変数すべてに、出てきた順で `_G` と番号を付ける。番号は書き出しごとに 1 から振る（`f(_G1,_G2,_G1)`）。トップレベルは 1 つの答えの束縛を 1 回の書き出しとして書く（`write_terms`、レビュー M22）。スイートの実行器は項を字面に書き、貼り合わせて読み直す。1 度だけ出る変数は読むたびに新しい変数になることに頼っているので、新しい `WriteOptions::underscore_once` を真にして、その変数を `_` のまま書く。`src/read/roundtrip.cpp` の読み戻しの検査は、書き手が付けた名前を無名として比べる。`tests/pl/write.pl` の `write_fresh` と `tests/cpp/writer_test.cpp` の「single unnamed」は同じコミットで変える（`src/write/writer.cpp` の `var_text`、`src/machine/lgt_suite.hpp` の `lgt_write_options`、`tests/cpp/suite_vars_test.cpp` の `fresh_*`）。

## 9.1.6 丸め関数に整数を渡したとき

`lgt_ceiling_1_05`、`lgt_floor_1_06`、`lgt_round_1_13`、`lgt_truncate_1_06`、`lgt_float_integer_part_1_04`、`lgt_float_fractional_part_1_04` は、`X is floor(9)` などが `type_error(float, 9)` になることを期待する。9.1.6 は `floor`、`ceiling`、`round`、`truncate` の型を F → I、`float_integer_part` と `float_fractional_part` の型を F → F と定めるので、整数の引数は型の誤りになる。Mosa は前の 4 つでは整数をそのまま返し（`floor(9) = 9`）、後の 2 つでは浮動小数点数を返す（`float_integer_part(9) = 9.0`）。SPEC はこれらの評価子を挙げるが、引数の型には触れていない。

A か B か: A は 9.1.6 に従い、6 つすべてで整数の引数に `type_error(float, N)` を投げる。B は今のまま整数を受け付ける。確認前は B のまま。

決定（2026-10-28）: A。この 6 つに整数を渡すと `type_error(float, N)` を投げる。`N` は評価した後の値である（`floor(4 + 5)` は `type_error(float, 9)`）。`integer/1` はこの 6 つに含まれず、今も整数を受け付ける（`src/builtins/arith.cpp` の `float_only`、`tests/pl/arith.pl`）。Mosa にはまだ `iso` 旗が無いので、どのモードでもこの動きになる。

## 9.1.6 round/1 の負の半分

`eclipse_round_1_07` と `08` は、`round(-3.5)` が `-3`、`round(-4.5)` が `-4` になることを期待する。9.1.6 は `round(x)` を `floor(x + 1/2)` と定めるので、ちょうど半分は正の無限大の側へ丸まる。Mosa は `std::round` を使っており、半分を 0 から遠い側へ丸めるので `-4` と `-5` を返す。正の半分はどちらでも同じ結果になる（`round(2.5) = 3`）。

A か B か: A は今のまま半分を 0 から遠い側へ丸める（`round(-2.5) = -3`）。B は 9.1.6 に従い `floor(x + 1/2)` にする（`round(-2.5) = -2`）。確認前は A のまま。

決定（2026-10-28）: B。`round/1` は 9.1.6 に従い、ちょうど半分を正の無限大の側へ丸める。端数は `floor(x)` を引いて取り出す。この引き算は誤差を生まないので、`0.49999999999999994` や 2^52 を超える奇数で丸めが二重に起きない（`src/builtins/arith_float.cpp`、`tests/pl/arith.pl` の `round_*`）。

## 第4節 「…」の引用原子の中

SPEC 第4節は `「` `」` を引用原子の始めと終わりに写し、入れ子にできないとだけ書く。中のどの文字が特別かは決めていない。

A か B か: A: `'…'`（6.4.2）と同じ規則。`\` のエスケープが効き、`」」` は `」` 1 字、生の改行は誤り、`'` は普通の文字。B: エスケープなし。最初の `」` までの文字を書いたとおりに取り、中に `」` は書けない。

決定（2026-10-28）: A。`「…」` は `'…'` のエスケープを使い、引用符の役は `」` が持つ。だから `」」` は `」` 1 字で、`'` は普通の文字。

## 7.7.7 と SPEC 第5節: `mosac emit` での静的に未定義の呼び出し

ISO に翻訳の段は無い。どこにも定義の無い手続きの呼び出しは、ゴールが動くときの `existence_error(procedure, PI)` であり（7.7.7、旗 `unknown` 7.11.2.4、7.12.2 c）、SPEC 第5節は無いモジュール `M` への `M:G` を実行時の `existence_error(module, M)` とする。`mosac emit`（フェーズ6 Task 5 と 7）はそれと違い、節の本体がどこにも定義の無い述語を字面で呼んでいれば、読み込まれないモジュールへの `nomod:foo(X)` も含めて、翻訳のときにプログラムを拒み（`mosac_undefined_call`、src/codegen/check.cpp）、何も書かなかった。下の 2026-11-05 の決定で、これは警告になった。

決定（2026-10-29）: これは翻訳器だけの意図した逸脱である。インタプリタと生成した実行ファイルは実行時に ISO の振る舞いを保ち、ゴールに届いたときに誤りを投げる（`tests/pl/modules.pl` の `mod_qual_unknown`、`tests/pl/native.pl` の `native_qualified_missing`。後者は翻訳が通るように、無いモジュールを変数 `M` で渡す）。`mosatest --diff`（`--units`、Task 11）はこの検査を飛ばし、存在エラーを確かめるテストも翻訳できるようにする。

決定（2026-10-30）: A。静的に未定義の呼び出しの診断は、手続きを実行時に `assertz/1` で作るプログラムに `:- dynamic(名前/アリティ).` を案内する。案内は前置形の `dynamic 名前/アリティ` ではなく `dynamic(名前/アリティ)` と書く。表 7（6.3.4.4）に `dynamic` の演算子は無く、7.4.2.1 の `dynamic/1` は述語標識を普通の引数として取るからである。名前は `writeq` の書き方で書き、演算子や記号文字のアトムなら括弧に入れる（ctest `mosac_emit_undefined_quoted`）。

A か B か（レビュー L12）: `unknown` を `fail` にするか `existence_error` を捕まえて、こうした呼び出しを意図して扱うプログラムを、mosa では動くのに mosac は拒んだ。案内した `:- dynamic` は catch に見えるものを変える（呼び出しは例外を投げずに失敗する）。A は、同じ診断を警告として出し、そのまま作る。B は止め続ける。

決定（2026-11-05）: A。`mosac emit`、`build`、`run` は診断（ファイルと行、`:- dynamic` の案内）の頭に「警告:」を付けて出し、先へ進む。実行ファイルは mosa と同じく、ゴールが動くときに誤りを投げるか、旗 `unknown` に従う。export の不整合は今までどおり翻訳を止める。テスト: ctest の `mosac_emit_undefined`、`mosac_emit_undefined_catch`、`mosac_emit_undefined_quoted`（今は build して実行する）と `mosac_emit_undefined_handled`。

## 7.1 項の入れ子の深さ（実装の上限）

writer、loader（節の組み立て、`IrTerm` の複製と破棄、実体化）、単一化と、項を辿る組込（`copy_term/2`、`findall/3`、`bagof/3`、`setof/3`、`compare/3`、`==`、`term_variables/2`、`clause/2`、`unify_with_occurs_check/2`、`is_list/1`）は、深い部分項を手で積むスタックに置く（ADR 0011）。1 MiB のスタックの下で、深さ 1,000,000 の項と 200,000 要素のリストのリテラルを扱える（ctest `stack_deep_write`、`stack_deep_builtins`、`long_literal`）。実行時に組み立てたゴールと算術式（`call/1`、`assertz/1`、`is/2` と比較）にも深さの上限はない（ADR 0011 の改訂 2 と 3）。式と、左にも右にも入れ子の `,` `;` `->` `*->` `catch/3` を深さ 1,000,000 で 1 MiB のスタックで確かめ、長さに比例する時間で動く。

入れ子の `findall/3`、`findall/4`、`bagof/3`、`setof/3` は呼び手のループの中で動き、ネイティブのスタックを使わない（ADR 0013、レビュー H4）。1 MiB のスタックで 100,000 段を、mosa と mosac の実行ファイルの両方で確かめる（ctest `stack_deep_collect`、`mosac_deep_collect`）。portray のフック、`with_output_to/2`、指令は今も 1 段ごとに入れ子の `run_loop` を始めるので、それらどうしの入れ子や、それらと収集を交互にした入れ子はネイティブのスタックで抑えられる。`w(N) :- M is N-1, findall(x, with_output_to(atom(_), w(M)), [x]).` は 1 MiB の下で 500 段は通り、1,000 段で SIGSEGV になる。

reader だけが、ソースの字面の入れ子の 1 段ごとにまだ再帰する。macOS で `ulimit -s 1024`（Windows の既定も 1 MiB）の下で測ると、`f(f(...))` や `[[...]]` は約 600 段、前置演算子の入れ子は約 850 段、括弧の入れ子は約 1,000 段、`xfy` の右の被演算子の連鎖は約 1,600 段まで読める。macOS の既定の 8 MiB のスタックでは、`f(f(...))` は約 4,800 段である。平らなリストと `yfx` の左の連鎖は繰り返しで読むので、この上限は無い。これは要件からの逸脱ではなく、処理系の上限（ISO 7.1、処理系定義）として扱う。

決定（2026-10-30）: A。writer、reader、loader を、フェーズ7かそれ以降に反復にする。フェーズ7で writer と loader を反復にした（ADR 0011）。reader は再帰のまま残し、`mosac_long_list` は全 OS で 20000 要素で動く。

## 7.1 アトム表の GC は無い（実装の上限）

ADR 0010 の GC はヒープ、`extras`、trail を回収するが、アトム表は回収しない。このため、新しいアトムを作り続けるループ（たとえば `atom_concat/3` や `atom_codes/2` を使うもの）では、プロセスが終わるまで表が伸び続ける。これは処理系の既知の上限であり、標準からの逸脱ではない。

決定（2026-10-30）: A。フェーズ7でもこの上限を残す。ADR 0010 はアトム表の GC を非目標としている。

## 8.11.6.1 標準ストリームの close/1,2

下の決定までは、Mosa は `user_input`、`user_output`、`user_error` を本当に閉じていた（`src/builtins/streams.cpp` の `run_close`、`mark_closed`）。`close(user_output)` の後、現在出力は添字 1 へ戻るが、その流れは閉じているので、以降の `write/1` はセッションの終わりまで `existence_error(stream, user_output)` になる（`mosa -g "close(user_output), write(hello)"`）。`with_output_to/2` と `query()` も同じ閉じた流れに着地する（レビュー項目 L13。H6 のレビューで見つかった）。この記録を書いた時点で 8.11.6.1 の本文は手元に無かった。Logtalk のスイートは `close(user_input)`、`close(user_output)`、`close(user_error)` と、その `force(true)` 付きが成功することだけを見ており（`lgt_close_1_07`〜`09`、`lgt_close_2_16`〜`18`）、その後の出力は見ていない。Prolog-cafe は標準ストリームの close/2 では閉じずに戻る。

A か B か: A は標準ストリームの close/1,2 で吐き出しだけして成功し、閉じない。現在の流れのまま書き込める。B は今のまま閉じる。確認前は B のまま。

決定（2026-11-01）: A。close/1,2 は引数と選択肢を今までどおり検査し、`user_output` と `user_error` を吐き出し、三つの標準ストリームを開いたまま残す（`src/builtins/streams.cpp` の `run_close`、`tests/pl/close.pl` の `close_std_*`、`tests/cpp/output_stop_test.cpp` の `check_session_close_user_output`）。`tests/cpp/stream_test.cpp` は同じコミットで変える。「close user_output」は流れが開いたまま現在出力であることを期待し、「closed current input」（今は「closed input」）は `user_input` ではなくファイルの流れを閉じる。

## 6.4.5 範囲を超えた浮動小数点数リテラル

下の決定までは、値が IEEE 754 の double に入らない浮動小数点数のトークン（`1.0e400`、`1.8e308`）は黙って `inf.0` になっていた（`src/read/lexer_literal.cpp` の `make_float`）。read/1、consult、number_codes/2、number_chars/2、atom_number/2 のどれでも同じである。そのため `float_overflow` が `error`（7.11）でも `X is 1.0e400 * 1` は誤りにならず、`inf.0` は読み戻せないので 8.16.8 の往復も成り立たなかった（レビュー項目 M2）。範囲を超えた浮動小数点数のトークンをどう扱うかは規格に書かれていない。

A か B か: A は整数の `syntax_error(integer_overflow)` と同じく `syntax_error(float_overflow)` にする（read/1 は終止符まで読み飛ばす）。B は `evaluation_error(float_overflow)` にする。

決定（2026-11-02）: A。最大の double より大きい浮動小数点数には、字句が ADR 0007 の新しい鍵 `float_overflow` を返す。最大の double に丸まる綴りは今までどおり数である。範囲外の値の向きは指数の符号ではなく値の桁（最初の 0 でない数字の位置と指数の和）で決めるので、`1<0 を 400 個>.0e-10` は上位に、`0.<0 を 400 個>1` は下位に溢れる。下位の溢れは変えない。`1.0e-400` は `0.0` と読み、`1.0e-310` のような非正規化数はそのまま残る（`tests/pl/read.pl` と `tests/pl/atoms.pl` の `m2_*`、`tests/cpp/lexer_test.cpp` の `test_float_range`）。

## 6.4.5 NaN と無限大（ISO にない）

下の決定の前は、writeq/1 などの書き手は NaN と無限大を `std::to_chars` の字面に `.0` を足して書いた（`src/write/spelling.cpp` の `format_float`）。`nan.0`、`inf.0`、`-inf.0` となり、符号の立った NaN（`X is -(nan)`、L2 からは `sign(-(nan))` も）は標準ライブラリ独自の字面で、macOS では `-nan(ind).0` だった。どれも読み戻せず、`term_to_atom(T, '-nan(ind).0')` も `term_to_atom(T, 'nan.0')` も `syntax_error(unexpected_token)` になる（レビュー L22）。ISO の浮動小数点数に NaN と無限大は無いので、規格はその字面を決めていない。SWI-Prolog と ECLiPSe は Joachim Schimpf の浮動小数点の core update の提案に沿って `1.0Inf`、`-1.0Inf`、`1.5NaN` と書き、読み戻す。

A か B か: A: NaN は符号の立ち方や標準ライブラリによらずすべて `nan.0` と書き、読み手は変えない（NaN と無限大は読み戻せないまま）。B: SWI-Prolog と同じく NaN を `1.5NaN`、無限大を `1.0Inf` と `-1.0Inf` と書き、字句解析がこれを浮動小数点数として読む。

決定（2026-11-06）: B。NaN はどれも `1.5NaN` と書き、符号と中身は書かない。`==` と compare/3 はもとからどの NaN も同じ項に見る。字句は `<桁>.<桁>Inf` を正の無限大（SWI と同じく桁は何でもよい）、`<桁>.<桁>NaN` を、桁の値が 1.0 以上 2.0 未満（SWI が書く `1.xxx`）のとき NaN と読む。範囲は丸めた double でなく小数点の前の桁（前の 0 を除いて `1`）で決めるので、`1.99999999999999999NaN` は NaN で、`0.99999999999999999NaN` は NaN でない。前の `-` は、ほかの浮動小数点数と同じく負の数にする。`Inf` や `NaN` の直後に名前の字が続くもの（`1.0Info`、`1.0Inf_`）、指数の付くもの（`1.0e5Inf`）、小文字（`1.0inf`）、ほかの NaN の仮数（`2.0NaN`）は、今までどおり浮動小数点数と変数に分かれて構文の誤りになる。ISO のテキストでは数の直後に変数は来ないので、規格に合うテキストの意味は変わらない。これらは溢れではないので、`float_overflow` が `error` でも読める。`1.0e400` は M2 のとおり `syntax_error(float_overflow)` のまま。read/1、consult、number_codes/2、number_chars/2、atom_number/2 はどれもこの字句解析を使う。符号の立った NaN の `-(X)` を writeq すると `- (1.5NaN)` と書き、複合項として読み戻す。`mosatest --roundtrip` は NaN 同士を同じ項に見る（`src/write/term_equal.cpp`）。テスト: `tests/pl/write.pl`、`tests/pl/read.pl`、`tests/pl/atoms.pl` の `l22_*` と、`tests/pl/syntax.pl` の `float_inf`、`float_nan`、`float_special_args` などの場合。

レビュー M29 から、format/2 の `~e` と `~f`（`'$format_float'/4`）も、無限大と NaN を桁の数と標準ライブラリによらずこの同じ字面で書き、0 で埋めない（`tests/pl/lib_format.pl` の `m29_*`）。

## 6.4.2 終止符の前の記号文字の字句

下の決定までは、字句解析器は、後ろに配置文字か `%` が来る `.` で記号文字の字句を切っていた。直前も `.` のときだけは切らない（`src/read/lexer.cpp` の `Lexer::ends_clause`）。だから `X = +.` の後に改行が来ると `X = +` と読み、`writeq(t('+.' - b))` が書く `t(+. -b)` は読み戻せなかった（レビュー L17）。6.4.2 が図形の字句として禁じるのは、配置文字が後ろに来る単独の `.` だけなので、そこでの `+.` は 1 つの字句である。

A か B か: A は字句解析器が `+.` を 1 つの字句に読む（6.4.2）。`X = +.` は終わりを待つ。B は字句解析器をそのままにし、writeq が `.` で終わる記号文字の原子を引用する。

決定（2026-11-03）: A（レビュー L17）。2026-10-31 の決定で `。` は `.` と読むので、全角の `。` も同じ規則に従う。`。` で終わる原子を引用すること（レビュー M8）はそのままで正しい。`lib/`、`tests/pl`、`tests/conformance` に、記号文字のすぐ後の `.` で節を終えるファイルは無い。

実装（レビュー L17）: 終止符になれるのは図形の字句の最初の字だけである。配置文字の前の `+.`、`#.`、（全角記号モードの）`+。` はどれも 1 つの名前で、`X = +.` の後に改行が来ても終わりを待つ。書いた項の後ろに終止符を置く Mosa 自身の書き手（REPL のゴールと答え、テストの実行器、mosac の節のコメント）は、字面が図形の字で終わるなら前に空白を置く。だから `X = '#'.` の答えは `X = # .` になる。Mosa がそのゴールを読み直すところ（REPL、テストの実行器、文面を作る目録のゴールとカタログ）では、char_conversion なしで読む。旗（7.11）が写すのは読んでいる Prolog テキストで、利用者の字面は読んだときに写してあり、書き手は項をそのまま書く。もう一度写すとゴールが変わる（`a` を `b`、`b` を `c` に写すと、`X = a.` の答えが `X = c` になっていた）。`japanese_punctuation` が偽なら `、` と `。` も図形の字なので、`X = 、.` は未完である。`.` のすぐ前の UTF-8 でないバイトは、`.` を取り込む U+FFFD（第4節で図形の字）になるので、構文エラーは次の終止符まで読み飛ばす（`tests/pl/chars.pl` の `l17_bad_byte_takes_dot`）。これで第4節の前の 2 つの文の範囲が狭まる。日付の付いた文面は変えない。2026-10-28 の決定（「`。` は後ろに配置文字か入力の終わりが来るときだけ節の終わり」）は字句の先頭の `。` に当てはまり、M8 の解釈が `。` で終わるとして引用する原子のうち、引用が要るのは原子 `。` そのものだけになる。ほかも引用したままにし、それでも読み戻せる。レビュー L23 からは、`initialization(G, main)` はゴールの項をヒープの外に写して（`export_term`）持ち、書きも読みもしないので、後の op/3、旗、char_conversion で変わらない。

## 9.3.8 と 9.3.9 整数と浮動小数点数の max/2 と min/2

Cor.2 は、引数が整数と浮動小数点数のとき、値の型を処理系依存とする（例: `max(2.0, 3)` は `3` か `3.0`、`max(0, 0.0)` は `0` か `0.0`、`max(2, 3.0)` は `3.0`）。Mosa は整数を浮動小数点数に直し、`<`/2 と同じく比べて浮動小数点数を返す。そのため `max(9007199254740993, 9007199254740992.0)` は整数の引数より小さい `9.007199254740992e+15` になる（レビュー L1）。選んだ引数を型のまま返せば値は正確になるが、型が値によって変わる。9.1.6（決定 A）で `floor` などの丸めの関数は整数を受け付けないので、`floor(max(0.0, 3))` が `type_error(float, 3)` になる（L1 の敵対的レビュー A1）。

A か B か: A は丸めずに比べ、選んだ引数を型のまま返す（`max(2.0, 3) = 3`）。B はこれまでどおり浮動小数点数に直して比べ、浮動小数点数を返す（`max(2.0, 3) = 3.0`）。

決定（2026-11-05）: B。動作は変えない。NaN との比べは大小どちらも偽なので、`max` は左、`min` は右の引数を返す（`max(1, nan) = 1.0`、`max(nan, 1) = nan`）。等しい値も同じ（`max(0, 0.0) = 0.0`）。テスト: `tests/pl/arith.pl` の `l1_max_*` と `l1_min_*`。

`-0.0` と `0.0` が別の項になったので（下の 7.2.2）、等しい値の決まりにより `max(-0.0, 0.0)` は `-0.0`、`max(0.0, -0.0)` は `0.0` になった（L2 の敵対的レビュー A3）。A か B か: A は `-0.0` を `0.0` と `0` より小さいとみなし、引数の順によらず `max` は `0.0`、`min` は `-0.0` を返す。標準順序と IEEE 754-2019 の maximum と minimum と同じ。B は等しい値の決まりのままにする。

決定（2026-11-05）: A。NaN とほかの等しい値は決定 B のまま。テスト: `tests/pl/arith.pl` の `l2_max_zero_*` と `l2_min_zero_*`。

## 6.3.3 複合項の名前としての `[]` と `{}`

6.3.3 は関数表記の複合項を、名前の字句と続く `(` で書く。`[]` と `{}` は名前の字句ではない（6.4.2。6.3.1.3 で作る原子である）ので、`'[]'(x)` と `'{}'(a)` には ISO の構文で引用しない形が無い。Mosa は `writeq('[]'(x))` を `[](x)`、`write_canonical({a})` を `{}(a)` と書き、読み手はどちらも読み戻す（レビュー L7）。適合スイートの Logtalk の移植性テストも同じことを期待する。`write_canonical({a})` は `{}(a)` を書き（`lgt_curly_bracketed_term_07`、`08`）、`[](2,3)` と `{}(2,3)` は複合項に読む（`lgt_compound_01`、`02`）。

A か B か: A は 6.3.3 に従い、quoted が真なら名前を引用する（`'[]'(x)`、`'{}'(a)`）。Logtalk のテスト 2 件が落ちる。B は `[](x)` と `{}(a)` のまま。

決定（2026-11-05）: B。動作は変えない。`tests/pl/write.pl` の `canonical_brace` が `{}(a)` を留める。

## 8.2.2 前から循環している項の unify_with_occurs_check/2

7.3.2 の出現検査付きの単一化は、変数をそれ自身を含む項に束縛するとき失敗する。8.2.2 は、引数が前から循環しているときを定めていない（7.3.3 はそのような項を未定義とする）。Mosa は単一化してから、結果が循環していれば失敗していた。そのため、何も束縛しない `X = f(X), Y = f(Y), unify_with_occurs_check(X, Y)` まで失敗した（レビュー L3）。

A か B か: A は 7.3.2 の検査と同じく、束縛が新しい循環を作るときだけ失敗する（`X = f(X), unify_with_occurs_check(A, X)` は、A が X の中に出ないので成功する）。B は、変数を循環する項に束縛するときも失敗する。

決定（2026-11-05）: B。初めは A を選んだが、L3 の敵対的レビューで、A では答えが束縛の順で変わると分かった。`X = f(X)` の後で、`unify_with_occurs_check(g(A, A), g(X, f(A)))` は成功し、`unify_with_occurs_check(g(A, A), g(f(A), X))` は失敗した。2 つは同じ方程式を解いている。A が無限の項を表すかは、順によらない。Mosa は `=` と同じく単一化してから、この呼び出しで束縛した変数の値を、辿り終えた部分を共有しながら辿る。どれかが循環していれば、束縛を戻して失敗する（`src/builtins/types.cpp` の `occurs_unify`）。失敗する単一化は `=` と同じ費用で済み、何も束縛しない呼び出しは何も辿らずに成功する。テスト: `tests/pl/unify.pl` の `l3_occurs_*`（`l3_occurs_cyclic_equal`、`l3_occurs_cyclic_unfolded`、`l3_occurs_bind_beside_old`、`l3_occurs_bind_inside_old` は 5dd9ed9 で失敗する。`l3_occurs_order_left` は A のとき成功していた）、`tests/stack/deep_builtins.pl` の `occurs_check_cyclic`（変えていない）。

## 8.2.4 subsumes_term/2 と循環した項

Cor.2 は `subsumes_term/2` を `unify_with_occurs_check/2` で定める。Mosa のそれは 8.2.2 の決定 B に従い、変数をすでに循環した項に束縛するときも失敗する。字面どおりなら `X = f(X), subsumes_term(f(_), f(X))` は偽になるが、SWI-Prolog と同梱のスイート（真を期待する `lgt_subsumes_term_2_15`）は真とする。循環していない入力では、既定の単一化と Specific の変数の並びの比較で、同じ真偽になる。

A か B か: A は 8.2.2 の決定 B を継いで、そのような呼び出しを偽にする。B は既定の単一化で真にする（SWI-Prolog とスイートに同じ）。

確認前は B（`src/builtins/subsumes.cpp` の `run_subsumes_term`。テストは `tests/pl/unify.pl` の `subsumes_cyclic_*`）。所有者が A を選んだら、単一化を `unify_with_occurs_check` に替え、これらの行の期待値を A の値に書き直す。

## 8 要素が循環するリストの is_list/1（ISO の外）

`is_list/1` は項全体が循環していないかを調べていた。そのため `X = [X], is_list(X)` と `X = f(X), is_list([X])` は失敗し、これでリストの引数を調べる `format/2` と `msort/2` は、そのようなリストを誤りにした（レビュー L3）。

A か B か: A は背骨だけを見る。背骨が `[]` で終われば、要素によらずリストである。B は項のどこかが循環していれば失敗し続ける。

決定（2026-11-05）: A。背骨が循環するリスト（`L = [a|L]`）はこれまでどおり失敗する。テスト: `tests/pl/types.pl` の `list_cycle` と `l3_list_*`、`tests/cpp/machine_test.cpp` の `cyclic list`。

## 7.2.2 項としての負のゼロ

7.2.2 は 2 つの浮動小数を値で順序付けるが、`-0.0` と `0.0` が同じ項かどうかは言わない。Mosa は両者を単一化し、同一とみなしていたので、`-0.0 == 0.0` が成り立ち、`sort([0.0, -0.0], L)` は `[0.0]` になった。writeq と copysign は両者を区別する（レビュー L2）。

A か B か: A は別の項にする。`-0.0 \== 0.0` で、単一化せず、`-0.0 @< 0.0` で、`-0.0 =:= 0.0` は真のまま。B は同じ項のままにする。

決定（2026-11-05）: A。SWI-Prolog と同じ。`include/mosa/unify.hpp` の `same_float_term` が、単一化と `mosatest --roundtrip` と Logtalk のスイートの照合の規則である。compare/3 と `==` は `-0.0` を先にする。第1引数の索引と mosac の生成コードは 2 つを別の鍵にする。NaN はこれまでどおり何とも、自分とも単一化せず、`==` と compare/3 は 2 つの NaN を等しいとみなす（変えていない）。テスト: `tests/pl/order.pl` の `signed_zero_eq` と `l2_zero_*`、`tests/pl/database.pl` の `m3_keys_zero` と `m3_keys_zero_negative`、`tests/cpp/unify_test.cpp` の「signed zero」。この 4 つの期待は決定によって変わった。

## 9.1.4 NaN の sign/1 と integer/1（ISO にない）

ISO の浮動小数に NaN は無いので、9.1.4 は `sign(nan)` について何も言わない。Mosa は `1.0` を返していた。`integer/1` は SWI-Prolog の拡張で ISO にない。Mosa は切り捨てていた（`integer(2.5)` が `2`）が、SWI は近い整数へ丸める（レビュー L2）。

A か B か: sign/1 は、A は NaN を返し、B は `1.0` のまま。integer/1 は（先に切り捨てを残す案も示したが選ばれなかった）、A は SWI と同じく近い整数へ丸め、ちょうど半分は 0 から遠い方へ（`integer(-2.5) = -3`）。B は Mosa の round/1 と同じく 9.1.6.1 の floor(x + 1/2) で丸める（`integer(-2.5) = -2`）。integer/1 の最初の問いは A を round/1 と同じと誤って書いていた。直してから持ち主がもう一度答えた。

決定（2026-11-05）: どちらも A。テスト: `tests/pl/arith.pl` の `l2_sign_*` と `l2_integer_*`。`l2_round_neg_half` は round/1 が変わらないことを見る。

## 7.4.2 同じパスをもう一度 consult する

ISO は Prolog のテキストを 1 度準備するもので（7.4）、読み直しを定めない。SPEC も定めない。同じパスをもう一度 consult すると、新しい中身が定義する述語だけを置き換えていた。そのため、前の中身が定義し、新しい中身にはもう無い述語が残った。そこで `:- dynamic` と宣言した述語も残った（レビュー L11）。

A か B か: A は、読み直しのとき、前にそのパスを読んで定義し、新しい中身に無い述語を、SWI-Prolog と同じく消す。失敗した読み込みは残す。B は残す。

決定（2026-11-05）: A。ADR 0020 による。ゴールが作った述語（assertz）と multifile の述語は残り、後で別のファイルが定義した述語も残る。新しい中身が dynamic と宣言しただけの述語と、読み直しのゴールが節を足すか消した述語は、読み直しの間に足した節だけを持って残る。入れ子の consult の古い述語は、一番外の読み込みが成功したときに消える。

## 7.10.2 と 8.12 Windows の標準の流れの行末

「8.12 テキストの改行」の決定は、テキストのファイルとメモリの流れの改行を、どの OS でも U+000A の 1 字とし、C++ のテキストモードに任せない。ファイルは binary で開くが、stdin、stdout、stderr は Windows では C 実行時ライブラリのテキストモードのままである（レビュー L8）。そのため、パイプの 0x1A のバイトで入力が終わり、パイプからの CRLF は LF として届き、stdout の `nl` は CRLF で書かれる。Windows のコンソールは `ReadConsoleW` で読み、Enter が出す CRLF の CR を落とす（src/platform/console.cpp の `console_line_text`）。

A か B か（レビュー L8）: Windows でも stdin、stdout、stderr を binary にして、0x1A をただのバイトとし、改行を macOS と Linux と同じく LF だけで書くとき、A は、パイプの CRLF をファイルからと同じく、また macOS と Linux と同じくそのまま読み、get_char/1 には CR が見える（read/1 はそれを空白とみなす）。B は、Windows ではパイプの CRLF の CR を、コンソールと同じく、また今のテキストモードと同じく落とす。

決定（2026-11-05）: A。Windows では、mosa、mosac、mosatest（`mosatest --diff` も）、mosac が作る実行ファイルのどれも、何かを読み書きする前に `prepare_console`（src/platform/console.cpp）が stdin、stdout、stderr を binary にする。実行ファイルはヒープを予約する前に呼ぶ。libmosa を組み込むプログラムは自分のモードのままである。0x1A のバイトはほかのバイトと同じく読む。パイプや stdin に渡したファイルからの CRLF は、macOS や Linux と同じく CR と LF として get_char/1 に届き、`nl` は LF だけを書く。コンソールは今までどおり、Enter が出す CRLF の CR を落とす。テスト: ctest の `mosa_std_binary`。

## 13211-3 文法規則: 本文の出どころ

13211-1 に文法規則は無い。SPEC 第 3 節はフェーズ 8 から ISO/IEC 13211-3 を拠り所にする。リポジトリに 13211-3 の本文は無く、ADR 0024 は本文なしで、Prolog の処理系に共通する動作から書いた。そのため 13211-3 の条番号はまだ引いていない。本文が手に入るまで `tests/pl/dcg.pl` のテストは自前のもので、手に入ったら Examples を写す。ADR 0024 と本文が食い違う点は、条番号を付けてここに 1 項目ずつ書く。
