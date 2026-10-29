[English](0009-native-code.md) | 日本語

状態: 承認済み（2026-10-29）

## 背景

持ち主は共存の方式 A を選んだ（2026-10-29）。実行ファイルは原文を埋め込んで今の読み込み器で読み、`mosac` が翻訳したときと IR が一致した静的述語だけが、トランポリンの上で動く C++ 関数に切り替わる（ADR 0003 の枠、ADR 0006 のゴール語、ADR 0008 の死んだ印はそのままである）。
捨てる下見（macOS、Release、1 プロセス、5 回の中央値）で、30 要素の `nrev` を失敗駆動で 20000 回まわした。インタプリタは 3352 ms、この ADR どおりに手で書いた `nrev/2` と `app/3` は、`regs` の単位の探索が 4 つの単位をたどる形で 150 ms で、22.3 倍速い（実行ファイル全体で番号を振った最初の下見は 3271 ms に対して 130 ms）。
同じ関数で、節の変数をすべて新しく作り、頭部の引数をすべて `rt::unify` に渡すと 1125 ms（3.0 倍）、`rt::unify` をただの `do_unify` にすると 2.1 倍（最初の下見、3 回の中央値）だった。残る費用は頭部の単一化（`do_unify` の `Attempt` が持つベクタ）なので、下の決定で頭部を特殊化する。

## 決定

埋め込んだ単位の原文の `load_source` が戻ったあと（プログラムの単位は `initialization(_, main)` のゴールの前、ライブラリの単位は `use_module/1` が読んだとき）、`(module, name, arity, fingerprint)` が静的述語と一致する `NativeEntry` を `Predicate::native` と、すべての単位の一致する `calls` の項目に付ける。付けたあとのその述語の節か `meta` への書き込み（定義し直す `consult/1`、multifile の追加、戻した読み込み）は `detach_native` を呼び、それらを一度に空にする。付け直すことはない。述語は IR の節を持つ普通の静的な `Predicate` のままなので、`assertz/1`、`retract/1`、`clause/2`、`abolish/1`、`predicate_property/2` は今のとおりに働き、`permission_error(modify, static_procedure, PI)` を投げる。`clause_fingerprint` は `meta_predicate/1` の引数を先に、次に節の IR を順に（タグ、スロット、アリティ、整数、浮動小数のビット、アトムは番号ではなく UTF-8 の名前とその長さで）数える 64 ビットの FNV-1a で、翻訳器と読み込み器は同じ関数を呼ぶ。
`NativeProc` は `void (*)(Machine::Impl& machine, std::uint64_t frame, std::uint32_t state)` である。`state` の 0 は呼び出しで、`select_clauses` と同じ規則で第1引数から節を選ぶ。`state_of(clause, point) = clause << 16 | point` で、節は 1 から数え、point 0 はやり直しでその節の頭部に入り、point n（n ≥ 1）はその節の n 番目の再開点である。再開点は翻訳器が作った順に番号を振る（呼び出しのあとの続きと、入れ子の制御構文の枝、then、else、done、回復、抜ける点。Task 7）。「本体の n 番目のゴールのあと」ではない。カットの境界は、状態 0 と節の入口の状態では `regs.cut`（実行時系が呼び出しとやり直しの前に必ず入れる `Impl::native_cut`）、再開の状態では `Ctrl::Native` の枠の語 2 である（回復の状態だけは入口で `choice_mark` を取る。7.8.9）。再開点は、`extras` を使わないヒープ上の `Ctrl::Native` の枠から続ける。語 0 = 0、語 1 = ビット 63 | 語 5 から先のセルの数 << 32 | 状態、語 2 = カットの境界、語 3 = 継続、語 4 = `NativeProc`、語 5 から先 = まずその状態の `skip` 個の制御語（選択点の高さ、ソフトカットの印、catch の位置を持つ即値整数のセルで、その状態の前置きが読む）、次に節の生きている変数のセルである。別の候補は `Pick::Native`（`redo_goal` = 枠、`candidates` = 残りの状態、`cut` = 境界）を積む。`push_alternative` はゴール枠を置いて `Entry::native` に関数を入れ、`push_branch` と `push_soft` は再開枠を置いて関数と状態をその語 4 と語 1 から読む。やり直しは関数を `candidates[next]` で呼ぶ。`push_catch` は `Pick::Catch` を `redo_goal` = 回復の再開枠で積み、`propagate_throw` は IR の回復ゴールを始める代わりに `ip` をその枠にする。
翻訳単位はそれぞれ `constinit rt::UnitTag` を 1 つ持つ。`regs(machine, tag)` は起動ごとに 1 回、印の番地からその Machine でのその単位の表を引き、呼び先とアトムの番号は単位の中だけで振る（`NativeUnit::calls` と `atoms`）。`call_goal(machine, regs, number, args, cont, cut)` は普通のゴール枠を書き、呼び先 `number` がまさにそのモジュールで付いていれば `Impl::native_next` を入れる。次の `step` はそれを `run_builtin` より先に動かし、`backtrack` と `propagate_throw` はそれを空にする。そうでなければ枠は今のまま `run_builtin` と `enter_user` へ行く。Task 6 と 7 までは、生成コードは自分では誤りを投げず、組込はすべて `call_goal` を通す。`runtime.hpp` は `Machine::Impl` を中身の見えない型として受け、`Cell::tag`、`Heap::data()` と、セルを読み項を書く（`put_*`）`rt::` の自由関数を inline にする。外の呼び出しは `install_unit`、`regs`、`alloc`、`bind`、`unify`、`call_goal`、`resume_frame`、`push_alternative`、`cut_to`、`proceed`、`fail`、`interpret_clause` だけで、正確な宣言は計画の Task 3 の **Interfaces** に置く（Task 7 は制御構文、catch、限定、算術の外の入口を `runtime.hpp` に足す）。頭部の単一化は引数ごとに出し（deref、タグの switch、照合か組み立て、`bind`）、`unify` を呼ぶのは繰り返す変数と、束縛されたセルに対する変数でない部分項だけにする。

## 捨てた案

`Ctrl::Native` をほかの制御枠と同じく `extras` に置く案。`step_control` が継続のたびに `Extra` をベクタごと写す。呼び先の枠そのものを制御枠にする案。誤りの文面とインタプリタへの戻りが読む名前とモジュールの語（ADR 0006）が無くなる。
SPEC 第6節の形 `bool pred_X_N(Machine& m, Cell* args, Cont k)` にする案。ADR 0003 のゴール枠がすでに引数と継続を持ち、再開には状態が要る（枠が `args` と `k` の代わりで、失敗は `machine.failed`）。呼び手から呼び先の C++ 関数を直接呼ぶ案。C++ のスタックが Prolog の深さだけ伸びる（Review Focus 4）。呼び先を `Impl::predicates` の位置で数える案。`abolish/1` がそのベクタから消すと位置がずれる。
呼び先とアトムの番号を実行ファイル全体で振る案。計画の Task 10 は、どのプログラムより先に作ったライブラリごとの単位を同梱する。頭部の単一化を引数ごとに `do_unify` に渡す案（下見で 2.1 倍）。`Heap::deref` ほかの `Heap` のメンバを `heap.hpp` で inline にする案。インタプリタも速くなり、比の分母が動く（計画の Task 6）。

## 影響

ADR 0003 に 3 つ目の枠の種類が加わる。語 1 のビット 63 が立った制御枠が `Ctrl::Native` で、ADR 0006 の「語 1 は extras の添字」はビット 63 が落ちているときだけ成り立つ。フェーズ7の GC はこの枠の語 5 から先をセルとして印を付け（制御語は即値整数のセルなので、この規則はそのまま成り立つ）、語 4 は項として読まない。
Task 3 は計画の Task 3 の **Interfaces** の宣言を書く。これはその計画の以前の並び（`fresh`、`make_*`、`bind_atom`、`bind_int`）を置き換える。足すのは `Predicate::native`、`Pick::Native`、`Ctrl::Native`、`Entry::native`、`Impl::native_units`、`native_next`、`native_cut` だけである。生成コードは、未束縛のまま残す節の変数をスロットの順に作り、`enter_clause` と同じ向き（節の側が `do_unify` の左）で束縛する。これは Task 4 の `native_var_order` で固定する。
承認したら、SPEC 第6節（英語と日本語）をこの関数の形とカットの境界に直す。埋め込んだ単位の `include/1`、`ensure_loaded/1`、`library(_)` をファイルを探さずに解くことは Task 5 と Task 10 に回す（Review Focus 5）。承認まで、この ADR にもとづくコードは書かない。 SPEC 第5節と第6節（英語と日本語）は、持ち主の決定により 2026-10-30 にこの ADR に合わせて直した。呼び出しはトランポリンを通って実行時に解決し、名前空間は `mosa_gen::m<マングルした名前>`、再開点は `goto` を使わず `state_of(clause, point)` で番号を振り、生成 C++ はいつも `.mosac-cache/<hash>/` に残る。
