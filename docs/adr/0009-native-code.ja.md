[English](0009-native-code.md) | 日本語

状態: 提案（2026-10-29）

## 背景

持ち主は共存の方式 A を選んだ（2026-10-29）。実行ファイルは原文を埋め込んで今の読み込み器で読み、`mosac` が翻訳したときと IR が一致した静的述語だけが、トランポリンの上で動く C++ 関数に切り替わる（ADR 0003 の枠、ADR 0006 のゴール語、ADR 0008 の死んだ印はそのままである）。
捨てる下見（macOS、Release、1 プロセス、5 回の中央値）で、30 要素の `nrev` を失敗駆動で 20000 回まわした。インタプリタは 3271 ms、この ADR どおりに手で書いた `nrev/2` と `app/3` は 130 ms で、25.2 倍速い（`build/` のインタプリタをコマンドラインで動かすと 3.19 秒）。
同じ関数で、節の変数をすべて新しく作り、頭部の引数をすべて `rt::unify` に渡すと 1069 ms（3.1 倍）、`rt::unify` をただの `do_unify` にすると 1571 ms（2.1 倍、3 回の中央値）だった。残る費用は頭部の単一化（`do_unify` の `Attempt` が持つベクタ）なので、下の決定で頭部を特殊化する。

## 決定

埋め込んだ原文の読み込みが静的述語を定義し終えたとき、`(module, name, arity, fingerprint)` がすべて一致する `NativeEntry` を `Predicate::native` と、同じ鍵を持つ `Impl::native_calls` のすべての項目に付ける。後の `consult/1` がその述語を定義し直したら両方を外す。指紋は `clause_fingerprint` で、節の IR を順に（タグ、スロット、アリティ、整数、浮動小数のビット、アトムは番号ではなく UTF-8 の名前とその長さで）数える 64 ビットの FNV-1a である。翻訳器と読み込み器は同じ関数を呼ぶ。
`using NativeProc = void (*)(Machine::Impl& machine, std::uint64_t frame, std::uint32_t state);` とする。`state` の 0 は呼び出しで、`select_clauses` と同じ規則で第1引数から節を選ぶ。`state_of(clause, goal) = clause << 16 | goal` で、節は 1 から数え、goal 0 はやり直しでその節の頭部に入り、goal g は本体の g 番目のゴールのあとから再開する。カットの境界は `Impl::native_cut`（`regs.cut` で読む）で、実行時系が呼び出しとやり直しの前に必ず入れる。再開の状態では代わりに `Ctrl::Native` の枠の語 2 から読む。本体の 2 つ目以降のゴールは、`extras` を使わないヒープ上の `Ctrl::Native` の枠から再開する。語 0 = 0、語 1 = ビット 63 | 変数の数 << 32 | 状態、語 2 = カットの境界、語 3 = 継続、語 4 = `NativeProc`、語 5 から先 = 節の変数のセルである。別の候補は `Pick::Native`（`Entry::native` = 関数、`redo_goal` = ゴール枠、`candidates` = 残りの状態、`cut` = 境界）を積み、やり直しは同じ関数を `candidates[next]` で呼ぶ。
生成コードは `rt::call_goal(machine, number, args, cont, cut)` で呼ぶ。`number` は `mosac` が実行ファイル全体で番号を振る呼び先の表 `Impl::native_calls` の添字である。普通のゴール枠を書き、その呼び先 (module, name, arity) がまさにそのモジュールの付いた述語なら `Impl::native_next` を入れ、`step` はそれを `run_builtin` より先に動かす。そうでなければ枠は今のまま `run_builtin` と `enter_user` へ行く。`runtime.hpp` は `Machine::Impl` を中身の見えない型として受け、`Cell::tag`（cell.hpp）、`Heap::data()`（heap.hpp）と、deref、未束縛の判定、タグの判定、リストと構造の読み出し、アトムと小さい整数のセル、枠の引数と変数の読み出し、確保した語への項の書き込みをする `rt::` の自由関数を inline にする。外の呼び出しは次のものだけにする。`regs`（ヒープの語、アトムの表、カットの境界。1 回の起動に 1 回）、`alloc`（節ごとに 1 塊）、`bind`（条件付きのトレイル）、`unify`（変数と同じセルの近道のあと `do_unify`）、`call_goal`、`resume_frame`、`push_alternative`、`cut_to`、`proceed`、`fail`、`interpret_clause`。頭部の単一化は引数ごとに出し（deref、タグの switch、照合か組み立て、`bind`）、`unify` を呼ぶのは繰り返す変数と、束縛されたセルに対する変数でない部分項だけにする。

## 捨てた案

`Ctrl::Native` をほかの制御枠と同じく `extras` に置く案。`step_control` が継続のたびに `Extra` をベクタごと写す。呼び先の枠そのものを制御枠にする案。誤りの文面とインタプリタへの戻りが読む名前とモジュールの語（ADR 0006）が無くなる。
SPEC 第6節の形 `bool pred_X_N(Machine& m, Cell* args, Cont k)` にする案。ADR 0003 のゴール枠がすでに引数と継続を持ち、再開には状態が要る（枠が `args` と `k` の代わりで、失敗は `machine.failed`）。呼び手から呼び先の C++ 関数を直接呼ぶ案。C++ のスタックが Prolog の深さだけ伸びる（Review Focus 4）。呼び先を `Impl::predicates` の位置で数える案。`abolish/1` がそのベクタから消すと位置がずれる。
頭部の単一化を引数ごとに `do_unify` に渡す案（下見で 2.1 倍）。`Heap::deref` ほかの `Heap` のメンバを `heap.hpp` で inline にする案。インタプリタも速くなり、比の分母が動く（計画の Task 6）。

## 影響

ADR 0003 に 3 つ目の枠の種類が加わる。語 1 のビット 63 が立った制御枠が `Ctrl::Native` で、ADR 0006 の「語 1 は extras の添字」はビット 63 が落ちているときだけ成り立つ。フェーズ7の GC はこの枠の語 5 から先をセルとして印を付け、語 4 は項として読まない。
`NativeUnit` に、実行ファイル全体の呼び先の鍵 `calls`（添字 = `number`）を `atoms` の隣に足す。`atoms` も実行ファイル全体の表で、生成コードはアトム i を `regs.atoms[i]` で読む。`interpret_clause(machine, number, frame, clause)` はその節を IR のまま動かす。Task 3 は上の型を書き、機械に足すのは `Predicate::native`、`Pick::Native`、`Ctrl::Native`、`Entry::native` と `Impl::native_calls`、`native_atoms`、`native_next`、`native_cut` だけである。
生成コードは、未束縛のまま残す節の変数をスロットの順に作り、`enter_clause` と同じ向き（節の側が `do_unify` の左）で束縛する。これは Task 4 の `native_var_order` で固定する。承認まで、この ADR にもとづくコードは書かない。
