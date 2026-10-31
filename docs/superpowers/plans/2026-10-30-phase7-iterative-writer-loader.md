# フェーズ7 繰り返しで動く writer と loader 実装計画

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 項の深さや長さに比例する C++ の再帰を、書き出し（writer）・読み込み（loader）・その実行に要る道筋から無くす。1 MiB のスタック（Windows の既定）でも、深い項や長いリストで落ちないようにする。

**Architecture:** 再帰している関数を、`std::vector` に手で積む形に書き直す。手本は、フェーズ6で翻訳器に入れた形（`src/codegen/emit_head.cpp` の `place` / `PlaceFrame`、`src/codegen/emit_state.hpp` の `walk_term`）、`src/machine/answer_text.cpp` の `Step`、`src/builtins/types.cpp` の `ground_term` である。writer は文字列を返す再帰をやめ、出力へ順に書き足す。空白の判断には、最後に書いた文字と「次に書く文字の前で glue を判断する」印を使う。`IrTerm` は形を変えず、破棄と複製だけを繰り返しにする。portray の間の `GcInhibit` は、ADR 0010 に書いたとおり外す。writer の状態は `RootScope` で GC に知らせる。

**Tech Stack:** C++20 標準ライブラリ、CMake 3.25+。依存は足さない。

**Spec:** 設計書は無い（所有者が質問を省いた）。根拠は次の 4 つである。
- 所有者の決定 (2)（2026-10-30、writer の再帰をフェーズ7で繰り返しにする）
- `docs/STATUS.md` のフェーズ7の項目（loader はリストの要素ごとに再帰する）
- ADR 0010（writer を書き直したら portray の `GcInhibit` を外す）
- `docs/SPEC.md` 第6節（C++ の再帰を使わない、トランポリン）

決めたことは Task 1 の ADR 0011 に書き、所有者が承認する。

## Global Constraints

- AGENTS.md に従う。関数は 60 行以下、ファイルは 800 行以下にする。生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロは使わない。`#ifdef _WIN32` などは `src/platform/` だけに書く。警告はエラーとして扱う。新しい依存ライブラリは足さない。
- Task 1 の ADR 0011 を所有者が承認するまで、コードを書かない。
- Cell の表現（ADR 0001/0002）は変えない。IR の形（ADR 0003 の `IrTerm` のフィールド）も変えない。変えてよいのは、`IrTerm` の特殊メンバ関数（破棄と複製）だけである。
- 出力は、書き直す前と 1 バイトも変えない。Task 8 の (-)/1 の直しだけは例外とする。
- 目標の大きさ:
  - 実行時に作った項は、深さ 1,000,000 まで扱えること（`f(f(...))`、リスト、`a+b+...` の左入れ子、`(a,b,...)` の右入れ子）。
  - ソースに書いたリストは、200,000 要素まで扱えること。
  - どちらも 1 MiB のスタックで落ちないこと。
- 速度の基準: `mosatest --bench` で、インタプリタ・翻訳とも、この計画を始める前（Task 2 の直前のコミット）より 5% 以上遅くならないこと。
- 識別子は英語、コメントとコミットメッセージは日本語で書く。コミットは 1 行で、`[phase7] 内容（確かめ方）` の形にする。
- 文書は英語が正で、`*.ja.md` を同じコミットで直す。`docs/STATUS.md` は英語だけで、15 行以下に保つ。
- 各タスクの終わりで、`cmake --build build && ctest --test-dir build`（直列）が全部通ること。

## 範囲の外（ADR 0011 に書く）

- reader の入れ子の再帰（`f(f(...))`、`[[[...]]]`、`(...)` の入れ子、xfy の右の被演算子の連鎖）は残す。平らなリストと yfx の左の連鎖は、もう繰り返しで読める。ソースに 1 万段の入れ子を書くことはまれなので、限界として iso-notes に残す。
- `Build::goal` の本体の制御構造（`,` `;` `->` の連鎖）は残す。reader が先に限界に当たるので、ここだけ直しても効かない。

## Review Focus

1. **演算子の括弧と空白が前と同じこと。** 決まった種で作った項の集まりを、前の writer の出力で固定する（Task 2 の `writer_golden`）。全部一致することを期待する。
2. **循環項。** `X = f(X)` を書く、単一化する、copy する、比べる。前と同じ結果で、止まることを期待する（Task 2、6、7 の `*_cyclic`）。
3. **portray の中で GC が走ること。** 深い項を `print/1` し、portray がヒープを使う。GC が走っても、出力は GC が無いときと同じであることを期待する（Task 3 の `portray_gc`、`MOSA_GC_STRESS=1` でも）。
4. **ヒープの上限で、手で積む構築の途中で止まること。** 小さい `--heap-max` で、長いリストの節を呼ぶ。`heap_limit` で止まり、REPL の次の問い合わせが普通に動くことを期待する（Task 6 の `materialize_limit`）。
5. **単一化の速さ。** nrev などで、手で積む単一化が遅くならないこと。基準内であることを期待する（Task 6 の Step の bench）。

---

### Task 1: ADR 0011 を書き、所有者の承認を得る

**Files:**
- Create: `docs/adr/0011-bounded-native-stack.md`、`docs/adr/0011-bounded-native-stack.ja.md`
- Modify: `docs/adr/README.md`、`docs/adr/README.ja.md`（索引に 0011 を「承認待ち」で足す）、`docs/STATUS.md`

**Interfaces:**
- Produces: 承認された ADR 0011。以後のタスクはこの決定に従う。

- [ ] **Step 1: ADR 0011 を書く。** 節は既存の ADR と同じ 4 つ（Context / Decision / Rejected alternatives / Consequences）にし、各節は 3 行以下にする。
  - Context:
    - 30000 段の項を書くとスタックがあふれる。
    - 50000 要素（Windows では 20000）のリストを読むと落ちる。
    - 落ちる関数は次のとおり。
      - writer の `render` 一族
      - `Build::data`
      - `materialize`
      - `unify_cells`
      - `IrTerm` の暗黙の破棄と複製
  - Decision:
    - 上の関数と同じ形の歩き手（Task 7 の一覧）を、`std::vector` に手で積む形にする。
    - `IrTerm` には、繰り返しで動く破棄・複製・ムーブを持たせる。フィールドは変えない。
    - writer は出力へ順に書き足す。
    - portray の `GcInhibit` を外し、writer の状態を `RootScope` で GC に知らせる。
    - 目標の大きさは Global Constraints の値とする。
  - Rejected alternatives:
    - 大きなスタックのスレッドで動かす案。限界を動かすだけで、組み込みや mosac の実行ファイルでは呼ぶ側のスレッドを選べない。
    - 深さに上限を設けてエラーにする案。ISO の項に深さの上限は無い。
    - reader も直す案。範囲の外の理由のとおり。
  - Consequences:
    - `docs/iso-notes.md`（英日）の 566 行目の限界を、reader の入れ子だけに書き直す。
    - ADR 0010 の portray の例外の文を、承認のときに改める（英日）。
- [ ] **Step 2: 日本語訳と索引と STATUS を書く。** STATUS には、承認待ちであることと次のタスクを書く。
- [ ] **Step 3: コミットする。** `[phase7] ADR 0011 を書く（承認待ち、英日）`。この計画のファイルも同じコミットに入れる。
- [ ] **Step 4: 所有者に承認を求める。** 1 行の報告と、「承認しますか」の問いだけにする。承認されたら、ADR の状態を Accepted にし、ADR 0010 の文を改めてコミットする。

### Task 2: writer を出力へ書き足す形にする

**Files:**
- Modify: `src/write/writer.cpp`（`Printer::render`、`render_op`、`render_list`、`render_functor`、`count`、`glue`、`needs_paren` の使い方）
- Create: `tests/cpp/writer_golden_test.cpp`、`tests/cpp/data/writer_golden.txt`、`tests/stack/small_stack.cmake`、`tests/stack/deep_write.pl`
- Modify: `tests/cpp/writer_test.cpp`、`tests/tests.cmake`

**Interfaces:**
- Consumes: `write_term(...)`（src/write/writer.cpp:482）の公開の形。これは変えない。
- Produces:
  - `small_stack.cmake`。POSIX では `ulimit -s 1024` の下でコマンドを動かし、Windows ではそのまま動かす（既定が 1 MiB）。以後のタスクの深いテストはこれを使う。
  - writer の深さ 1,000,000 への耐性。

- [ ] **Step 1: 出力を固定する（GREEN のまま入れる）。**
  - `writer_golden_test` は、決まった種で項を作る。種類は演算子表の全演算子 × 深さ 3 まで、アトムの引用の要否、負の数、`'$VAR'`、リストと `{}` である。
  - 選択肢の組（`quoted`、`ignore_ops`、`numbervars`、`variable_names`）ごとに書き、`writer_golden.txt` と比べる。
  - `--write-golden` を付けて動かすと、ファイルを書き直す。
  - 前の writer で golden を作り、テストと一緒にコミットする。
  - 被演算子になる演算子アトム（(-)/1 の不具合の領域）は、Task 8 で直すので集まりから外す。外したことをテストのコメントに書く。
- [ ] **Step 2: 深い項のテストを書き、RED を確かめる。**
  - `writer_test` で、深さ 1,000,000 の次の項を書く。
    - `f(f(...))`
    - `-(-(...))`
    - `1+2+...` の左入れ子
    - `(a,b,...)` の右入れ子
    - `write_canonical` のリスト
    - `underscore_once` 付きのリスト
  - それぞれ先頭と末尾の数十文字を確かめる。
  - `deep_write.pl` も書く。中身は、`numlist` で作った深い項を `write/1`、`writeq/1`、`print/1` に渡すことである。これを `small_stack.cmake` の ctest（`stack_deep_write`）として登録する。
  - 今のコードで落ちること（RED）を記録する。
- [ ] **Step 3: writer を書き直す。**
  - 仕事の単位を `std::vector` に積む。単位は「項を描く（親の優先度、等しい優先度を許すか、hook を呼ぶか）」「文字列を書く」「`open_` から外す」である。
  - 括弧の要否（`needs_paren`、prefix の被演算子の括弧、`-(1)`）は、項を見た時点で決める。
  - 空白は `glue` を使って判断する。次の字句を書く直前に、最後に書いた文字とその字句の最初の文字を比べる。`call_form` の `(` の前の空白も、同じ印で扱う。
  - `count`（`underscore_once`）も手で積む。`open_` の線形の探索は、集合に置き換える。
- [ ] **Step 4: GREEN を確かめる。** 次がすべて通ること。
  - `writer_golden`、`writer_test`、`stack_deep_write`
  - `build/mosatest --diff tests/pl` が 0 件の違い
  - `diff_conformance`
  - ctest の全部
- [ ] **Step 5: コミットする。** Step 1 と Step 3 は別のコミットにする。

### Task 3: portray の間も GC を止めない

**Files:**
- Modify: `src/builtins/write_pred.cpp`（`apply_portray` の `GcInhibit` を外す。ここが最後の利用者なので、`GcInhibit` 自体を消せるなら消す）、`src/write/writer.cpp`（`Printer` を GC のルートにする）、`src/machine/gc.hpp`
- Modify: `docs/adr/0010-gc-heap-growth.md`、`.ja.md`（例外の文を削る。Task 1 で改めていれば不要）
- Test: `tests/cpp/gc_test.cpp`（`portray_gc`）

**Interfaces:**
- Consumes: `RootScope` / `GcVisitor`（ADR 0010、`src/machine/gc.hpp`）。
- Produces: GC が動かす項を書いても、名前と出力が保たれる writer。

- [ ] **Step 1: RED を書く。** `portray_gc` では、深さ 10,000 の項を `print/1` する。portray は、各節でリストを作って捨てる。`garbage_collect/0` も呼ぶ。
  - 今は `GcInhibit` で GC が走らない。テストは `gc_stats.count` が増えることを求めるので、RED になる。
  - 出力は、portray がヒープを使わない版と一致することを求める。
- [ ] **Step 2: `Printer` の状態をルートにする。**
  - 積んだ仕事の Cell、`open_`、`counts_`、`assigned_` の鍵を `RootScope` で知らせる。
  - 生の番地を鍵にしている表は、GC の後で鍵を直す。GC の回数を覚えておき、変わっていたら表を作り直すのでよい。
- [ ] **Step 3: GREEN を確かめる。** 次がすべて通ること。
  - `portray_gc`
  - `MOSA_GC_STRESS=1 build/mosatest --diff tests/pl` が 0 件の違い
  - `MOSA_GC_STRESS=1000 ctest -R "^(gc|heap_growth|mosac_heap_limit)$"`
  - ctest の全部
- [ ] **Step 4: コミットする。**

### Task 4: 書き出しの周りの歩き手

**Files:**
- Modify: `src/write/term_equal.cpp`（`match` / `match_compound`、roundtrip が使う）、`src/machine/answer.cpp`（`note_shown`。`seen` の線形の探索を集合にする）
- Test: `tests/cpp/writer_test.cpp`（`same_term_deep`）、`tests/cpp/answer_text_test.cpp` か session のテスト（REPL で深い答えを出す `answer_deep`）

- [ ] **Step 1: RED を書く。**
  - `same_term_deep`: 深さ 1,000,000 の 2 つの項を比べる。
  - `answer_deep`: REPL で `numlist` の深い項を答えとして出す。
- [ ] **Step 2: 手で積む形にする。**
- [ ] **Step 3: GREEN を確かめる。** ctest の全部が通ること。
- [ ] **Step 4: コミットする。**

### Task 5: `IrTerm` の破棄・複製と節の組み立て（loader）

**Files:**
- Modify: `include/mosa/ir.hpp`（`IrTerm` に、繰り返しで動く破棄、複製のコンストラクタと代入、ムーブを足す。フィールドは変えない）
- Modify: `src/machine/compile.cpp`（`Build::data` / `compound_data` を後順の組み立てにする。`building()` の線形の探索を集合にする）
- Create: `tests/stack/long_literal.cmake`（原文はここで作る。tests には大きな原文を置かない）
- Test: `tests/cpp/machine_test.cpp`（`ir_deep_copy_destroy`、`compile_long_list`）

**Interfaces:**
- Produces:
  - 深さ 200,000 以上の `IrTerm` を、複製しても破棄しても落ちない。
  - `compile_clause` は 200,000 要素のリストの節を組み立てる。

- [ ] **Step 1: RED を書く。**
  - `ir_deep_copy_destroy`: 長さ 1,000,000 のリストの `IrTerm` を作り、複製して比べ、破棄する。
  - `compile_long_list`: 200,000 要素のリストの事実を `Machine::consult` する。
  - `long_literal.cmake`: `small_stack.cmake` の下で、200,000 要素の `data([...])` と `X = [...]` の原文を mosa で読んで動かし、`length` と最後の要素を確かめる。
  - 今は落ちること（RED）を記録する。`long_literal` は、Task 6 が済むまで RED のままでよい。
- [ ] **Step 2: `IrTerm` の特殊メンバ関数を書く。**
  - 破棄: 子の `args` を外に出し、`std::vector<IrTerm>` に積んでから壊す。
  - 複製: 手で積む前順の複製にする。`emit_state.hpp` の `walk_term` の形を使う。
  - ムーブは既定でよい。
- [ ] **Step 3: `Build::data` を後順の組み立てにする。** `emit_head.cpp` の `PlaceFrame` の形を使う（子の結果を親の部品に積み、親が揃ったら閉じる）。
- [ ] **Step 4: GREEN を確かめる。** 次がすべて通ること。
  - `ir_deep_copy_destroy`、`compile_long_list`
  - mosac の ctest（`mosac_*`。翻訳器も `IrTerm` を使う）
  - ctest の全部（`long_literal` を除く）
- [ ] **Step 5: コミットする。**

### Task 6: 節の実体化と単一化（実行）

**Files:**
- Modify: `src/machine/engine.cpp`（`materialize`）、`src/core/unify.cpp`（`unify_cells` / `unify_compound` / `unify_args`）
- Test: `tests/cpp/unify_test.cpp`（`unify_deep`、`unify_cyclic`）、`tests/cpp/gc_test.cpp` か machine のテスト（`materialize_limit`）、`long_literal`（Task 5）

- [ ] **Step 1: RED を書く。**
  - `unify_deep`: 深さ 1,000,000 のリスト同士、`f(f(...))` 同士を単一化する。成功と失敗の両方を確かめる。
  - `unify_cyclic`: `X = f(X), Y = f(Y), X = Y` が止まって成功すること。
  - `materialize_limit`: `--heap-max 1` で 200,000 要素の節を呼ぶ。`heap_limit` で止まり、次の問い合わせが動くこと。
- [ ] **Step 2: `materialize` を書き直す。** 手で積む後順にする。リストの尾は繰り返しで伸ばしてよい。
- [ ] **Step 3: 単一化を書き直す。**
  - 対の `std::vector` を積む。
  - アトムと整数どうしなど、片方が複合項でない対は、積まずにその場で決める（速さのため）。
  - `seen` は「祖先だけ」から「この単一化で見た対すべて」に変える。これでも余帰納的に正しく、止まる。表は集合にする。
- [ ] **Step 4: GREEN を確かめる。** 次がすべて通ること。
  - `unify_*`、`materialize_limit`、`long_literal`
  - `mosatest --diff tests/pl` と `diff_conformance`
  - ctest の全部
- [ ] **Step 5: 速さを測る。** `mosatest --bench` を Task 2 の直前のコミットと今で 5 回ずつ測り、中央値で比べる。インタプリタも翻訳も 5% 以内であること。超えたら、単一化の速い道を見直してから測り直す。
- [ ] **Step 6: コミットする。** `mosac_long_list` の Windows の段階的な引き下げ（`tests/mosac/long_list_test.cmake` の 20000〜1000）を外し、全 OS で 20000 にする。この変更は同じタスクの別のコミットにする。

### Task 7: 同じ形の残りの歩き手

**Files:**
- Modify:
  - `src/machine/catch.cpp`（`export_rec`、copy_term）
  - `src/builtins/solutions.cpp`（`encode`、`copy_cell`、findall/bagof/setof）
  - `src/builtins/order.cpp`（`compare_seen`、compare/3 と `==`）
  - `src/builtins/terms.cpp`（`collect_vars`、term_variables/2）
  - `src/builtins/clause_term.cpp`（`remember_slot` / `expose`、clause/2）
  - `src/builtins/types.cpp`（`acyclic_term`）
- Create: `tests/stack/deep_builtins.pl`（`small_stack.cmake` の ctest `stack_deep_builtins`）

- [ ] **Step 1: RED を書く。** 深さ 1,000,000 の項に対して、次を 1 つずつ確かめる。循環項の場合も、前と同じ結果であることを確かめる。
  - `copy_term/2`
  - `findall/3`
  - `bagof/3`
  - `compare/3`
  - `==`
  - `term_variables/2`
  - `assertz/1` と `clause/2`
  - `acyclic_term/1`
- [ ] **Step 2: 1 関数ずつ手で積む形にする。** 関数ごとに GREEN を確かめてコミットする。
- [ ] **Step 3: GREEN を確かめる。** 次がすべて通ること。
  - ctest の全部
  - `MOSA_GC_STRESS=1 build/mosatest --diff tests/pl` が 0 件の違い

### Task 8: 被演算子の演算子アトムに括弧を付ける（7.10.5）

**Files:**
- Modify: `src/write/writer.cpp`（アトムの被演算子と引数に、演算子アトムかどうかの検査を足す）、`src/codegen/check.cpp:55-58`（迂回をやめる）
- Test: `tests/pl/write.pl`（`writeq((-)/1)` が `(-)/1`、`writeq(f(:-))`、`writeq(- - a)` など、7.10.5 の round trip）、`tests/cpp/writer_golden_test.cpp`（Task 2 で外した演算子アトムを集まりに戻す）

- [ ] **Step 1: 7.10.5 の round trip のテストを書き、RED を確かめる。** 同じ問い合わせを SWI-Prolog（ISO モード）でも確かめ、iso フラグの下で ISO と同じ出力にする。
- [ ] **Step 2: 直す。**
- [ ] **Step 3: golden を作り直す。** 変わってよいのは、演算子アトムを含む行だけである。変わった行の数をコミットメッセージに書く。
- [ ] **Step 4: GREEN を確かめる。** ctest の全部が通ること。STATUS と `docs/iso-notes.md`（英日）の、この不具合の行を消す。
- [ ] **Step 5: コミットする。**

### Task 9: 1 MiB の下での確かめと文書を閉じる

**Files:**
- Modify:
  - `.github/workflows/ci.yml`（`stack_*` の ctest が 3 OS で動くこと。Windows は既定の 1 MiB で動く）
  - `docs/iso-notes.md`、`.ja.md`（限界を reader の入れ子だけにする）
  - `docs/bench.md`、`docs/bench.json`
  - `docs/STATUS.md`

- [ ] **Step 1: CI を確かめる。** ブランチを push し、8 ジョブが通ることを確かめる。特に Windows の `stack_deep_write`、`stack_deep_builtins`、`long_literal` と、20000 要素の `mosac_long_list` が通ること。
- [ ] **Step 2: 文書を直す。**
  - bench の数を記録する。
  - iso-notes の限界の段落を改める（英日）。
  - STATUS の Phase 7 の項目から、writer と loader を消す。次は、手引き書と配布物である。
- [ ] **Step 3: コミットして、最後の全体のレビューに回す。** レビューアは Opus にする。
