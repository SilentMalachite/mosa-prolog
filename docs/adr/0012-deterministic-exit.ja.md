[English](0012-deterministic-exit.md) | 日本語

状態: 承認済み（2026-10-31。所有者がレビュー項目 H3 の計画を同じ依頼の中で実装するよう求め、後でこの状態を確認した）

## 背景

`catch/3` のゴールを抜けると `disarm_catch` は `armed` を下ろすだけで、`*->` の条件が成功すると SoftCommit は `cond_ok` を立てるだけである。そのため上に何も残っていなくても、どちらの `Entry` も選択点スタックに残る（2026-10-31 のレビュー H3）。
`lw(N) :- catch(true,_,true), N1 is N-1, lw(N1).` のような決定的なループは、1 段ごとに `Entry` を 1 つ残す。その `pattern`、`env`、`else_goal` は GC の根なので、`lw(300000)` は `--heap-max 8` で止まる。`catch/3` を除いた同じループは 1,000,000 段まわる。
`has_more` と入れ子の実行の補助はスタック全体をなめるので、残った Entry はあとの時間も食う。

## 決定

`catch/3` のゴールを抜けたとき（`Ctrl::CatchExit`、`rt::catch_exit`）、その `Catch` の Entry がスタックの一番上なら、`disarm_catch` は `trim_stack` で外す。そうでなければ今までどおり無効にし、`catch_log` に記録する。
`*->` の条件が成功したとき（`Ctrl::SoftCommit`、`rt::soft_commit`）、else の Entry が一番上なら外す。そうでなければ今までどおり `cond_ok` を立てる。`rt::soft_commit` は上から探し、最初に見つかったところで止める。
ゴールが作った選択点は、ちょうどその Entry より上の Entry である。だから上に何も無ければ、実行がゴールの中へ戻ることはなく、抜ける枠も二度と動かない。バックトラック、`has_more`、`propagate_throw` は、無効にした catch と印の付いた else をもともと飛ばすので、外しても安全である。

## 退けた案

上に死んだ Entry（無効にした catch、印の付いた else）しか無いときも外す案。抜けるたびに走査が要る。入れ子の決定的なコードでは内側の Entry が先に外れ、外側が一番上になるので、走査しなくても外れる。
死んだ Entry を GC で取り除く案。`cut`、`Extra::trim_to`、`catch_log`、生成コードが持つスタックの添字をすべて書き換える必要があり、抜けるときに外すのより得るものが無い。

## 結果

`catch/3` と `*->` を通る決定的なループは、インタプリタでも生成コードでも、選択点スタック、`catch_log`、ヒープが頭打ちになる。Entry の上に選択点が残るときは何も変わらない。そのようなループは `*->` を 2^31 回より多くまわせるようになるので、`soft_id` と `next_soft` を 64 ビットにする。
バックトラック、カット、例外の動きは前と同じ。プログラムから見える 3 点は、もともと Entry を残さない `once/1` と `->` と同じになる。指令の中でそのように抜けた後の op/3 は、死んだ Entry に記録されなくなり、外の問い合わせが戻っても残る（op/3 をそもそも戻すかはレビューの M4 が決める。2026-11-02 に A と決まり、戻さない。iso-notes 8.14.3）。決定的に抜けた `*->` の後の consult は、述語の定義し直しを `predicate_running` で拒まなくなる。`Machine::choice_depth()` が数える Entry が減る。
テストは `choice_exit`（2^16 語、それぞれ 1,000,000 段）、`mosac_heap_limit`（mosa と mosac の実行ファイルを 1 MiB で）、`control.pl`（`op_exit_same`、`soft_exit_consult`、やり直し・カット・throw の見張り）。
