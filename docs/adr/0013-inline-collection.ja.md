[English](0013-inline-collection.md) | 日本語

状態: 承認済み（2026-10-31。所有者がレビュー項目 H4 の計画を同じ依頼の中で実装するよう求めた）

## 背景

`findall/3`、`findall/4`、`bagof/3`、`setof/3` は、ゴールを C++ からもう一度 `run_loop` を呼んで動かす（`src/builtins/solutions.cpp` の `collect`）。そのため入れ子 1 段ごとにネイティブの枠を 1 つ使う。`d(N,s(L)) :- N1 is N-1, findall(X, d(N1,X), [L]).` は 8 MiB のスタックで約 6,000 段、1 MiB では 1,000 段未満で SIGSEGV になり、bagof は約 10,000 段で落ちる（2026-10-31 のレビュー H4）。
各段は選択点スタック全体を写し（`save_exec`）、下の catch を無効にして有効に戻すために 2 回なめるので、n 段の入れ子は O(n²) かかる。
ADR 0011 は ISO 7.1 が深さを定めないことから深さの上限を退けた。Prolog の 1 段ごとに再帰する構造で残っているのは収集だけである。

## 決定

収集は `\+` と同じく呼び手の `run_loop` の中で動かす。引数を調べた後、`Pick::Collect` の `Entry` を積む。その `collect` 欄（共有の `Collector`。テンプレート、証人、`^` の変数、変数の番号付け、それまでの解、呼び出しの枠）は `GcVisitor::collect` を通る GC の根で、共有の env と同じく 1 回の訪問で 1 度だけ辿る。ゴールは、その Entry の上をカットの境界とし、`Ctrl::Collect` の枠を継続として始める。
`Ctrl::Collect` に着くと解を記録し（`remember`）、失敗する。`Collect` の Entry まで戻ると状態を戻し、Entry を外し、`bind_found` か `bind_answers` で答えを束ねる。どちらも今と同じく呼び出しの継続で始める。例外は他の選択点と同じく Entry を素通りするので、外側の catch は有効なままで、無効にして戻すことはしない。`halt/0` はその場で機械を止める。
生成コードは今までどおり `call_goal` と `run_builtin` を通って収集に届くので、変えない。ほかの入れ子の `run_loop`（portray のフック、指令、`with_output_to/2`、Logtalk の写しの確認）はそのままにする。

## 退けた案

深さの上限と `resource_error`。ISO 7.1 は深さを定めず（ADR 0011）、ある深さで残るネイティブのスタックは OS ごとに違う（Windows は 1 MiB）ので、上限は低すぎるか安全でないかのどちらかになる。
組込の枠に入り直して終える案（枠で収集を見分ける）。ゴールを読み直し、枠の同一性に頼る。`Pick` を分ければ、終わりの処理は `backtrack` の中で `redo_not` の隣に置ける。

## 結果

入れ子の収集はネイティブのスタックを使わず、深さに比例する時間で動く（1 段あたり約 1.1 KB。`Entry` と `Collector` はヒープの外にあるので、ほかの選択点と同じく `--heap-max` には数えない）。`tests/stack/deep_collect.pl` は findall/3、findall/4、bagof/3、setof/3 と一番下からの throw を、1 MiB のスタックで 100,000 段、mosa と mosac の実行ファイルの両方で動かす。
答え、誤りの項、副作用の順序、収集の終わりで op/3 が戻ること（レビュー M4）は変わらない。`Machine::choice_depth()` は、動いている収集 1 つにつき `Barrier` の代わりに `Collect` の Entry を 1 つ数える。
portray のフック、`with_output_to/2`、指令と交互に入れ子にした収集は、今も組ごとに 1 回再帰する（1 MiB では約 1,000 組で SIGSEGV になる）。これは iso-notes 7.1 に記す。`Saved`、`park_callers`、`disarm_below`、`rearm_below`、`unwind_throw` は `solutions.cpp` から無くなる。`Pick::Barrier` はほかの入れ子の実行のために残す。
