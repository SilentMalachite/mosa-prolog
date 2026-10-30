[English](0011-bounded-native-stack.md) | 日本語

状態: 承認済み（2026-10-30）

## 背景

約 30000 段に入れ子した項を書くと C++ のスタックがあふれ、50000 要素のリストの原文を読むと `mosa` が落ちる（既定のスタックが 1 MiB の Windows では 20000 要素）。
入れ子の段ごと、またはリストの要素ごとに再帰する関数は、writer の `render` 一族（`render`、`render_op`、`render_list`、`render_functor`、`count`）、`Build::data`、`materialize`、`unify_cells`、そして `IrTerm` の暗黙の破棄と複製である。
2026-10-30 の所有者の決定 (2) で、writer はフェーズ7で繰り返しにする。SPEC 第6節は実行に C++ の再帰を使わない（トランポリン）と定め、ADR 0010 は writer を書き直すまでの間だけ portray の周りに `GcInhibit` を置いている。

## 決定

上の関数と同じ形の歩き手（copy_term の `export_rec`、findall の `encode`/`copy_cell`、`compare_seen`、`collect_vars`、clause/2 の `remember_slot`/`expose`、`acyclic_term`、`term_equal` の `match`、答えの `note_shown`）は、仕事を C++ のスタックでなく `std::vector` に手で積む。`IrTerm` には繰り返しで動く破棄、複製のコンストラクタ、複製の代入を持たせる（ムーブは既定のまま）。フィールドは変えない（ADR 0003）。
writer は文字列を返さず、出力へ順に書き足す。空白（`glue`）は、最後に書いた文字と次の字句の最初の文字を比べて決める。portray の `GcInhibit` は外し、writer の積んだ仕事、`open_`、`counts_`、`assigned_` を `RootScope`（ADR 0010）で知らせる。番地を鍵にする表は GC の後で作り直す。
目標: 実行時に作った項は深さ 1,000,000 まで（`f(f(...))`、リスト、`a+b+...` の左入れ子、`(a,b,...)` の右入れ子）、ソースのリストは 200,000 要素まで、どちらも 1 MiB のスタックで扱う。出力は、被演算子の演算子アトムの 7.10.5 の直しを除き、今と 1 バイトも変えない。`mosatest --bench` はインタプリタも翻訳も 5% 以内にする。

## 捨てた案

大きなスタックのスレッドで動かす案。限界を動かすだけで、組み込む側のプログラムや `mosac` の実行ファイルは呼ぶ側のスレッドを選べない。
深さに上限を設けてエラーにする案。ISO の項に深さの上限は無い。
reader も繰り返しにする案。ソースで 1 万段の入れ子（`f(f(...))`、`[[...]]`、`(...)`、xfy の右の被演算子の連鎖）はまれで、平らなリストと yfx の左の連鎖はもう繰り返しで読める。`Build::goal` の制御構造も、reader が先に限界に当たるので再帰のまま残す。

## 影響

`docs/iso-notes.md`（英日）の「7.1 項の入れ子の深さ」を、reader の入れ子だけの限界に書き直す。`mosac_long_list` は全 OS で 20000 要素で動く。
承認のとき、ADR 0010 の portray の例外の文（英日）を、この ADR で例外を外したと改める。
この ADR が承認されるまで、これに基づくコードは書かない。
