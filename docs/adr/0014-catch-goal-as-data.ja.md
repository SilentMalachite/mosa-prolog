[English](0014-catch-goal-as-data.md) | 日本語

状態: 承認済み（2026-11-01。レビュー項目 M14。所有者は iso-notes 7.8.9 で A を選び、mosac の `\+` と `M:G` を同じ項目で直すことにも A を選んだ）

## 背景

7.8.9.1 は `catch/3` の Goal と Recovery を、catch が動くときに `call/1` として動かす。そのときまでに `!` に束縛されたゴール位置の変数は、そのゴールに閉じたカットになる。`Build::goal`（`src/machine/compile.cpp`）は節や `call/1` のゴールを作るときに両方を訳し、変数は `call(X)` として動く `Var` の葉になってカットを通さない。`t(X) :- catch(((X, fail) ; true), _, true)` では `t(!)` が成功し、Recovery の形 `catch(throw(e), _, ((X, fail) ; true))` と `call((Y = !, catch(((Y, fail) ; true), _, true)))` も成功する。一方 `G = catch(((X, fail) ; true), _, true), X = !, call(G)` は失敗する（mosa も mosac も）。
mosac は `\+` と `M:G` のデータのゴールも同じように先に訳す（`src/codegen/emit_body.cpp` の `owned_goal`）。そのため `n(X) :- \+ ((X, fail) ; true)` の `n(!)` は mosac の実行ファイルでは失敗し、`\+/1` が動くときにゴールを訳す mosa では成功する。
ADR 0003 は IR を固定しており、この変更は今どこも作らない `IrTag::Call` に意味を与えるので、先に ADR を書く。

## 決定

`Build` が `catch/3` を訳すとき、Goal または Recovery が `,` `;` `->` `*->` のどれかで、その下のゴール位置に未束縛の変数があれば（7.6.2 と同じくこの 4 つだけを通して見る。入れ子の `catch/3` は自分で決める）、`IrTag::Call` の節点（アトム `call`、引数 1 個、部分項をデータとして持つ。実行時に組んだゴールでは写さずに元の項を指し、入れ子の catch が線形のままになる）にする。変数 1 つだけの場合と、そういう変数の無い構文は今のまま訳す。
この節点は `call/1` の葉として動く（`embody_leaf`。mosac では書かれた `call/1` と同じ `call_goal`）。そのためゴールは catch が動く時点の束縛で訳され、カットの境界は catch の内側に置かれて Goal（または Recovery）が作った選択点を切る。これは 7.8.9.1 と 7.8.3 のとおりである。clause/2 と retract は `Call` の節点を引数そのものとして見せる（`expose_start`）ので、Goal は書いたとおりに見える（7.6.2 c）。
mosac の `owned_goal` はゴール位置に変数を含むデータを断る。そのため変数を含む `\+` と `M:G` は mosa と同じく組み込みへ回る。翻訳器は項を、mosac は IR を、それぞれ手で積むスタックで 1 回なめて確かめる（ADR 0011）。mosac の未定義呼び出しの検査は `Call` の節点の中も今までどおり見る。

## 退けた案

`catch/3` の Goal と Recovery をすべてデータのまま持つ案。解析は要らないが、どの catch も呼ぶたびにゴールを訳し直し、mosac は `lib/lists.pl:63` のような catch のゴールのネイティブコードを失う。
新しいタグ、または `Catch` に「引数はデータ」という印を足す案。`open_catch`、`emit_catch`、`Catch` をなめる全ての処理が変わる。実行器、コード生成、`expose_start` はすでに `Call` を呼び出しの葉として動かし、見せている。B（違いを逸脱として記録する）は所有者が退けた。

## 結果

`t(!)`、Recovery の形、`call((Y = !, ...))` の形は失敗し、`n(!)` は成功する。mosa でも mosac の実行ファイルでも `call(G)` と同じになる。`tests/pl/control.pl` のテスト（どれも `mosac` でも通す）で、Goal、Recovery、`\+`、`M:G`、catch が始まった後に束縛される変数（`catch((X = !, ((X, fail) ; true)), _, true)` は今後も成功する。call/1 は `X` が未束縛のうちに変換するため）、そういう節への `clause/2` と `retract/1` を確かめる。
そういう catch は呼ぶたびにゴールを 1 回訳す。変数 `G` の `catch(G, ...)` はすでにそうしている。他の catch、`lib/`、ベンチのプログラムは変わらない。そういうゴールの中に呼び出せない部分があれば、`call/1` と同じく、どれかを動かす前に `type_error(callable, Goal)` になる（7.8.3.3）。
当初の対象外（レビュー M15。下の改訂で対応した）: 変数が無い場合、catch の引数の呼び出せない部分は、その前のゴールを動かしてしまう（`catch((write(a), 1), E, true)` は `a` を書く）。また `call(catch((true, 1), E, true))` は catch の外へ `type_error(callable, catch(...))` を投げる。`body_callable` が catch の引数の中まで見るためである。

## 改訂（2026-11-01、所有者の M15 の実装の依頼により 2026-11-01 承認）

背景: 変数が無い場合、呼び出せない部分のある catch/3 の Goal と Recovery は先に訳されていた。そのため `catch((write(a), 1), E, true)` は `type_error(callable, 1)` の前に `a` を書いた。また `body_callable` とデータベースの `goal_form` が `Catch` の中まで見るので、`call(catch((true, 1), E, true))` と `assertz((p :- catch(1, _, true)))` は catch の外へ `type_error(callable, catch(...))` を投げた（レビュー M15。7.8.9.1、7.8.3.3。7.6.2 は catch/3 の引数を変換しない）。
決定: Goal か Recovery が数のとき、または `,` `;` `->` `*->` の下のゴール位置に数があるとき（変数と同じ歩き方）も `IrTag::Call` の節点にする。`call/1` が何も動かす前に `type_error(callable, Goal)` を出す。`leaves_at` は `Catch` の中を見ない（catch/3 のゴールは呼び出せる）。mosac の未定義の呼び出しの検査は、ゴール位置に数のある `Call` の節点を飛ばす。どれも動かないからである。
退けた案: `open_catch` の中で実行時に Goal 全体を確かめる（catch を呼ぶたびにもう 1 回なめる）。結果: 数も変数も無い catch は変わらない。テストは `tests/pl/control.pl` の `m15_*`、`tests/cpp/machine_test.cpp`（数の前の halt/1）、`tests/mosac/halt_noncallable_*.pl` と `noncallable_undefined.pl`。
