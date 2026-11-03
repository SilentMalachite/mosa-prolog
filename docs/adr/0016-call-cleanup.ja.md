[English](0016-call-cleanup.md) | 日本語

状態: 承認済み（2026-11-03。所有者がレビュー M11 で A を選び、それが `setup_call_cleanup/3` の前にこの ADR を置いており、同じ依頼の中で M11 を実装するよう求めた）

## 背景

`setup_call_cleanup(S, G, C)`（SPEC 第7節の段階 G、レビュー M11、iso-notes の「第7節 段階 G の便利述語」）は、SWI-Prolog（`boot/init.pl`、`pl-wam.c`）と同じく、`G` が選択点を残さずに抜けたとき、失敗したとき、例外を投げたとき、選択点を切られたときに、`C` をちょうど 1 回動かす。
Mosa にはこのどこにも口が無い。カットは `trim_stack`（生成コードからも `rt::cut_to` で来る）、失敗は `backtrack`、例外は `propagate_throw` であり、終わった問い合わせと指示はスタックを見ずに捨てる。
`trim_stack` は歩みの途中で動き、呼び手がその後で `ip` を置くので、自分では Prolog のゴールを始められない。そこで入れ子の `run_loop` で `C` を動かすと、生成コードが C++ の局所に握るセルを GC が動かしてしまう。

## 決定

組込は `once(S)` を動かし、次に内部の `'$call_cleanup'(G, C)` を動かす。これは `C` を調べ（具体化と呼べるかの誤り。仕掛ける前）、`pattern` が `C` で `module` が文脈モジュールの `Pick::CallCleanup` の Entry を積み、後ろに `Ctrl::CleanupExit` の枠を付けて `G` を呼ぶ。`G` の誤りは仕掛けた後に出る。
Entry が一番上にあるときの `CleanupExit` と、その Entry を外す `trim_stack`（上から順）は、`C` を `Impl::pending_cleanups`（GC の根）に足す。`run_loop` は毎回の初めに、空でなければそれをゴール枠 `(call(C) -> true ; true)` にし、今の `ip` の前、機械が失敗していれば `fail` の前、例外が待っていれば `throw(Ball)` の前に置く。`propagate_throw` はこれが空でない間はスタックに触れないので、カットと同じ歩みの throw が `C` の下のヒープを切り詰めることは無い。1 回のカットが複数を外したときは、どれも `catch/3` の下で動かし、全部の後で最初の例外を投げ直す（SWI の `discardChoicesAfter` も全部動かす）。生成コードはカットのたび（`->` の確定の後も）に `rt::cleanup_pending` を見て、続きが同じ関数の中で動く（算術、構文、`\+`）なら次の歩みへ回すので、`C` はインタプリタと同じくカットの時点の束縛を見る。`backtrack` がこの Entry に着けば外して `C` を積み、その後で失敗する。`propagate_throw` が着けば Entry の水位まで戻して `(catch(call(C), _, true) -> true ; true), throw(Ball)` を動かす。したがって `C` は `S` の束縛を見るが `G` の束縛は見ず、ゴールの例外が勝つ。
選択点を残して止まった問い合わせ、指示、portray のゴールは、先にそれを切り、積んだ後始末を同じループで動かす。`-g` のゴール、トップレベルの答え、`initialization(G, main)` も答えの後で同じにする（SWI の `'$run_init_goal'` とトップレベルもそこで切る）。その例外は指示、portray、ゴールの例外になり、埋め込みの API で新しい問い合わせが古いものを閉じるときは無視する。`halt` とヒープの上限での停止は後始末を動かさず、次の問い合わせがそれを閉じるときも動かさない。

## 退けた案

`trim_stack`、`backtrack`、`propagate_throw` から入れ子の `run_loop` で `C` をすぐ動かす。生成コードは `rt::cut_to` をまたいでヒープのセルを C++ の局所に握り、入れ子のループの GC がそれを動かす。
Entry ごとの数を持って `trim_stack` の走査を飛ばす。Entry は `resize`、`pop_back`、`clear` とスタックの入れ替えで十あまりの場所から外れる。走査はどのみち壊す Entry の `pick` を読むだけである。
生成コードの関数をカットのたびに終える。`N > 0, !, N1 is N - 1` のどれもが、後始末を仕掛けていなくても枠 1 つと `run_loop` の 1 周を払う。

## 結果

カットは、インタプリタでも生成コードでも、その後の何よりも先に `C` を動かす。生成コードはカットごとにベクタを 1 回見て、同じ関数の中で動くゴールが続くカット（と `->` の then）ごとに再開の状態を 1 つ足す。同じゴールの並びの状態は 1 つにまとめるので、数はカットの数に比例する。
`run_loop` は 1 歩ごとにベクタを 1 つ多く読み、`trim_stack` は外す Entry ごとに `pick` を読む。`has_more` と consult の使用中の判定は `Pick::CallCleanup` を `Pick::Catch` と同じに扱うので、解には数えず、定義し直しも妨げない。
テスト: `tests/pl/utilities.pl` の `scc_*`（どの終わり方も、束縛、`G` と `C` の例外、`once/1`・`->`・`\+`・findall からのカット、節の本体でカットの直後の throw・評価の誤り・算術、1 回のカットで外れる複数の後始末）、`tests/pl/cleanup_dir.pl`、`machine_test`（次の問い合わせが閉じる、`halt` の後は閉じない）、`session_test`（`-g`、トップレベル、main）、`tests/mosac/cleanup_main.pl`。`diff_tests_pl` で mosa と mosac の実行ファイルの両方が動かし、`MOSA_GC_STRESS=1` でも動かす。
