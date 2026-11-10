[English](0024-dcg-translation.md) | 日本語

状態: 承認済み（2026-11-10。持ち主が文法規則のためにフェーズ 8 を設け、この ADR を決定 A で承認した）。SPEC 第 3 節は文法規則を範囲に入れ、第 9 節はフェーズ 8 を定める。この ADR は `-->` を訳す場所と、訳した節の形を決める。

## 背景

`-->` は演算子（1200、xfx）だが、ローダは `H --> B` の項を断る（`src/machine/load.cpp` の `take_term`）ので、それを含むファイルは読み込めない。`term_expansion/2` は未実装なので、Prolog で書いた変換を呼ぶ所が無い。
`mosac` も同じ `take_term` でテキストを読み、IR はインタプリタと翻訳の共通の入力である（ADR 0003、0009）。
訳した節は `clause/2`、エラーの項、カットの位置を通してプログラムから見えるので、プログラムが頼り始めると形を変えにくい。

## 決定

ローダが C++（`src/machine/dcg.cpp`）で `H --> B` を `H' :- B'` に訳し、`compile_clause` に渡す。IR、Cell、`mosac` は変えない。`H'` は `H` に引数 `S0, S` を足したもの。頭部が `H, Pushback` なら本体の後ろに `S = Pushback ++ S1` を足す。`M:(H --> B)` は断ったままにする。`phrase/2,3` は `call/N` と同じく `system` の制御構造で、引数を検査し、同じコードで `G` を `L` から `R` までの本体として訳し（phrase/2 の `R` は `[]`）、call/1 と同じく動かす。中のカットは phrase に閉じる（改訂 1。初めの文は「`G` に `L` と `R` を足して呼ぶ」で、それでは `phrase([a], L)` が失敗する）。
本体: 非終端記号には `S0, S` を足す。リスト `[a,b]` はゴール `S0 = [a,b|S]`、`[]` は `S0 = S` とし、頭部には畳み込まない。`,`、`;`、`|`、`->` は構造を保つ。`M:B` は `M:B'`。`\+ A` は `\+ A', S0 = S`、`{G}` は `call(G), S0 = S`、`!` は `!, S0 = S`、`call(G, A…)` は `call(G, A…, S0, S)`、変数 `V` と、自分の中で再び出会う制御構造（phrase/2,3 に渡した巡回する本体）は `phrase(V, S0, S)`。call/1 が巡回するゴールを戻り先に着いたとき訳すのと同じである。本体は再帰でなく明示したスタックで歩く（改訂 2。Codex のレビューの P1。深い本体や巡回する本体で C++ のスタックが尽きた）。
読み込み時のエラーは `formal_*` の手助けで作る。頭部が変数なら `instantiation_error`、頭部や本体の部分が呼び出し可能でなければ `type_error(callable, Culprit)`、プッシュバックや終端記号のリストの末尾が変数なら `instantiation_error`、リストでなければ `type_error(list, Culprit)`。phrase/2,3 は、`G` が変数なら `instantiation_error`、`G` に呼び出し可能でない部分があれば `type_error(callable, G)`（call/1 と同じく `G` 全体）、`L` や `R` がリストでも部分リストでもなければ `type_error(list, Culprit)`。

## 却下した案

`term_expansion/2` の下で Prolog で訳す: その仕組みが無い。`lib/` のライブラリは `use_module` なしでは `user` から見えない（SPEC 第 3 節）が、`-->` の節は取り込みなしで読めなければならない。
終端記号を頭部で単一化する（`a([x|S], S)`）: 終端記号の前のカットや `{G}` が単一化の後に走ることになり、`a --> !, [x]` や `a --> {G}, [x]` の意味が変わる。
終端記号のための新しい IR 命令: フェーズ 2 の IR を変えるわりに、ここでは第 1 引数の索引に要らない利得しか無く、`mosac` にも同じ命令が要る。

## 結果

`assertz((a --> b))` は今までどおり `-->/2` の節を足す。`term_expansion/2`、`goal_expansion/2`、`expand_term/2` は未実装のまま。文法規則を一覧すると訳した節が出る。
テスト: `tests/pl/dcg.pl`（本体の形ごとに 1 件、プッシュバック、カット、読み込み時のエラー、phrase/2,3 のエラーは `Formal` で比べる）と、`tests/pl/japanese.pl` の日本語の文法。どちらも `mosatest --diff` に掛ける。
「文法規則は使えない」という制限は、フェーズ 8 が終わるとき CHANGELOG の既知の制限、マニュアル 14 章、STATUS から外す。13211-3 の本文の出どころは iso-notes（「13211-3」）に書く。

## 改訂 3（2026-11-11、承認待ち）

背景: 非終端記号をエクスポートや宣言で `Name//Arity` と書けず（エクスポートの表では `type_error(predicate_indicator, greet//0)` になる）、制御構造 `*->` があるのに文法規則の本体に書けず、規則 1 つを実行時に訳す `expand_term/2` も無い。これらはリリース 0.1.2 の既知の制限だった。
決定: (1) エクスポートの表、インポートの表（`use_module/2`）、ディレクティブ `dynamic/1`、`discontiguous/1`、`multifile/1` の `Name//Arity` は `Name/(Arity+2)` の意味とする。エクスポートとインポートの表ではエラーは `/` に従い（`instantiation_error`、`type_error(atom, N)`、`type_error(integer, A)`、`domain_error(not_less_than_zero, A)`、`A+2 > 1024` のとき `representation_error(max_arity)`）、3 つのディレクティブは、今の `N/A` の誤りを断るのと同じ方法で `N//A` の誤りを断る。`abolish/1` と `current_predicate/1` は `/` だけを受ける（8.9.4.3）ので、`abolish(foo//1)` は `type_error(predicate_indicator, foo//1)` のまま。読む所は `src/module/qualify.cpp` の `read_indicator`（エクスポート、インポート）と `src/machine/load_directive.cpp` の `one_indicator`（3 つのディレクティブ）で、`abolish/1` のものである `src/builtins/database.cpp` の `parse_indicator` ではない。
(2) 文法規則の本体の `(C *-> T)` は `->` と同じく、`C` を `S0` から `S1`、`T` を `S1` から `S` として `(C' *-> T')` に訳す。`;` の中では `(C' *-> T' ; E')` になる。
(3) `expand_term/2` は `system` ではなく `library(dcg)`（`lib/dcg.pl`、`:- module(dcg, [expand_term/2]).`）に置く。SPEC 第 3 節は ISO でない述語を `library(*)` に置く。`expand_term/2` は隠しの組み込み `'$dcg_translate'/2`（`library(format)` と `'$format_float'/4` と同じ形）を呼び、それがローダの訳（`src/machine/dcg.cpp` の `dcg_rule`）を実行時に動かす。変数でなく `-->/2` でもない項はそのまま返り、第 1 引数が未束縛なら `instantiation_error` になる。
却下した案: `expand_term/2` を `system` に置く（SPEC 第 3 節に例外が増える）。`abolish/1` でも `//` を受ける（8.9.4.3 は `type_error(predicate_indicator)` と言う）。`term_expansion/2` の下で Prolog で訳す（元の決定と同じく、その仕組みが無い）。
結果: `term_expansion/2` と `goal_expansion/2` は範囲に入れず、`assertz/1` は今までどおり訳さない。コードが入ったとき、`Name//Arity` と `*->` の制限は CHANGELOG の既知の制限から外し、`assertz/1` の制限は `library(dcg)` の `expand_term/2` を指す書き方に直し、`term_expansion/2` と `goal_expansion/2` が無いことは載せたままにする。テストは `tests/pl/modules.pl`、`tests/pl/dcg.pl`、`tests/pl/lib_dcg.pl`、`tests/cpp/dcg_test.cpp`。
