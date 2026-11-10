# フェーズ9 ISO の穴埋めと堅牢性 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `subsumes_term/2` と `acyclic_term/1` を `system` に入れ、reader を入れ子の深さで C++ のスタックを使わない形にし、文法規則の制限 3 つ（`Name//Arity`、本体の `*->`、`library(dcg)` の `expand_term/2`）を閉じて、0.1.2 の既知の制限を 4 つ減らす。

**Architecture:** 2 つの述語は既存の歩き手（`acyclic_terms`、`do_unify` + `restore_state`）の上の薄い組み込み。reader は `parse_expr` の相互再帰を `std::vector<Frame>` と 1 つのループに替え、既存の判断関数（`parse_name`、`find_follower`、`bad_token` …）はそのまま使う。文法規則は `src/machine/dcg.cpp` の訳を広げ、`expand_term/2` は `lib/dcg.pl` から隠し組み込み `'$dcg_translate'/2` で同じ訳を実行時に呼ぶ（`library(format)` と `'$format_float'/4` の形）。

**Tech Stack:** C++20 標準ライブラリ、CMake 3.25、`mosatest`（`tests/pl` の `test(Name, Goal, Expect)` 行）、`tests/stack/small_stack.cmake`（1 MiB のスタック）、Logtalk 準拠スイート。

**Spec:** `docs/superpowers/specs/2026-11-11-phase9-iso-completion-design.md`（所有者が A を選んで承認したあとに実行する）

## Global Constraints

- 依存は C++20 標準ライブラリと CMake 3.25 以上だけ。`-Wall -Wextra -Wpedantic`（MSVC は `/W4`）で警告はエラー。
- 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロ、コンパイラ拡張を使わない。関数は 60 行以内、ファイルは 800 行以内。
- エラーは `raise_formal` / `formal_*` / `raise_type` の族だけで投げる。アトム名を文字列リテラルで書かない（既存の `formal_type(machine, "callable", …)` のような型名は既存の書き方に合わせる）。
- 組み込み述語の C++ は「引数の検査 → 本処理 → 束縛」の順。途中で束縛してから失敗しない。
- 組み込み述語のファイル頭のコメントに「規格の節番号、引数のモード、投げうるエラー、テストファイル名」を書く。
- 規格の Examples は実装の前に `tests/pl/` へ写す。エラーのテストは `Formal` だけを比べる。
- 識別子は英語、コメントとコミットメッセージは日本語。文書は英語が正で、`*.ja.md` を同じコミットで直す。
- コミットは論理単位ごとに 1 つ。メッセージは 1 行で `[phase9] 内容（確認方法）`。
- 各セッションの始めに `docs/STATUS.md` の「In progress」にタスクを書き、終わりに完了と次とを書く（15 行以内）。
- `src/platform/` の外に `#ifdef` を書かない。Cell（ADR 0001/0002）と IR（ADR 0003/0009）は変えない。

## Review Focus

1. `subsumes_term(f(X), f(Y))` のあとで `X` と `Y` が別々の未束縛のままであること（束縛を残さない）: Task 2 の `subsumes_no_binding`。
2. 循環した項に `subsumes_term/2` を使っても止まること（`X = f(X), subsumes_term(Y, X)` 真、`subsumes_term(X, Y)` 偽）: Task 2 の `subsumes_cyclic_*`。
3. 共有された部分項を循環と間違えないこと（`X = f(Y, Y), acyclic_term(X)` 真）: Task 1 の `acyclic_dag`。
4. reader の書き換えで、深い入れ子の中の構文エラーの位置とトークンが変わらないこと: Task 4 の `read_error_deep_token`、および `tests/pl/read.pl` の既存の行を 1 行も変えずに通すこと。
5. `Name//Arity` の `Arity+2` が 1024 を超えるとき `representation_error(max_arity)` になり、`Name//(-1)` が `domain_error(not_less_than_zero, -1)` になること: Task 6 の `export_dcg_max_arity`、`export_dcg_negative`。

---

### Task 0: フェーズ9を開く（文書だけ）

所有者が設計を承認した（A を選んだ）あとに行う。

**Files:**
- Modify: `docs/SPEC.md`（第3節 69 行付近「Grammar rules (phase 8)」の段落、第9節の表 358 行の次）、`docs/SPEC.ja.md`（同じ箇所、355 行付近）
- Modify: `README.md`（8 行のバッジ `phase-8_done` → `phase-9_in_progress`、98 行からの表に 9 行目）、`README.ja.md`（8 行、110 行）
- Modify: `docs/STATUS.md`（1 行目を「Phase 9 open (ISO completion and robustness, opened 2026-MM-DD, owner decision A)」に）
- Modify: `docs/superpowers/specs/2026-11-11-phase9-iso-completion-design.md`（状態を「承認済み（日付）」に）

- [ ] **Step 1: SPEC 第9節の表に 9 行目を足す**（英・日）。内容: 「ISO の穴埋めと堅牢性: `subsumes_term/2`、`acyclic_term/1`、繰り返しで動く reader（ADR 0011 改訂 4）、`Name//Arity`、文法規則の本体の `*->`、`library(dcg)` の `expand_term/2`（ADR 0024 改訂 3）」。完了条件: 「8.2.4.4 と 8.3.11.4 の Examples が `tests/pl/unify.pl` と `types.pl` で通り、`mosatest --suite` が `predicates/subsumes_term_2` と `predicates/acyclic_term_1` をディレクトリとして範囲に入れ（STO の件は件ごとに別欄のまま）、`subsumes_term` の判定が組み込みを通り、合格率が 98.4% を下回らない。ctest `stack_deep_read` が 1 MiB の下で 3 OS で通る。`tests/pl/dcg.pl`、`lib_dcg.pl`、`modules.pl` が通り、`mosatest --diff` の差分が 0」。
- [ ] **Step 2: SPEC 第3節の「Grammar rules (phase 8)」の段落**の末尾の文「`term_expansion/2`, `goal_expansion/2` and `expand_term/2` are not part of it」を「`expand_term/2` is added in phase 9 in `library(dcg)` (it translates one grammar rule; `term_expansion/2` and `goal_expansion/2` stay out of scope)」に直す（日本語版も）。同じ段落に `Name//Arity` と `*->` がフェーズ9で入る旨を 1 文足す。第5節のライブラリ表に `library(dcg)` の行（`expand_term/2`）を足す。
- [ ] **Step 3: README のバッジと表、STATUS の 1 行目、設計文書の状態**を直す。
- [ ] **Step 4: 確認**: `grep -n 'phase-9\|フェーズ9\|phase 9' README.md README.ja.md docs/SPEC.md docs/SPEC.ja.md docs/STATUS.md` が 5 ファイルすべてに当たる。
- [ ] **Step 5: Commit**: `[phase9] フェーズ9を開く（SPEC 第3節と第9節、README の表とバッジ、STATUS。文書だけ）`

---

### Task 1: `acyclic_term/1`（8.3.11）

**Files:**
- Modify: `src/builtins/types.cpp`（頭のコメント、`acyclic_terms` のすぐ下、名前で振り分ける関数の 151 行と 198 行付近）
- Modify: `src/machine/engine.cpp`（組み込みの表 193〜207 行付近に `{"acyclic_term", 1}`）
- Modify: `src/machine/lgt_suite.hpp`（`kPredicateScope` の `"abolish_1"` の次に `"acyclic_term_1"`）
- Test: `tests/pl/types.pl`

**Interfaces:**
- Consumes: `bool acyclic_terms(const Impl& machine, std::span<const Cell> roots)`（`types.cpp` 41 行。明示したスタックで歩く）。
- Produces: `acyclic_term/1` が `system` にあり、`predicate_property(acyclic_term(_), built_in)` が真。

- [ ] **Step 1: 失敗するテストを書く**。`tests/pl/types.pl` の末尾に 8.3.11.4 の Examples を規格の行のまま 1 行 1 テストで写し、Review Focus 3 の行を足す:

```prolog
% 8.3.11.4 acyclic_term/1（Cor.2）
test(iso_acyclic_atom, acyclic_term(a), true).
test(iso_acyclic_var, acyclic_term(_), true).
test(iso_acyclic_compound, acyclic_term(f(X, Y, g(X))), true).
test(iso_acyclic_cyclic, (X = f(X), acyclic_term(X)), fail).
test(iso_acyclic_list_cyclic, (X = [a|X], acyclic_term(X)), fail).
test(acyclic_dag, (X = f(Y, Y), acyclic_term(X)), true).
test(acyclic_cycle_inside, (Y = g(Y), acyclic_term(f(a, Y))), fail).
```

  （実装者は規格 Cor.2 の 8.3.11.4 の行を手元の本文と突き合わせ、本文の行を優先する。本文に無い行は自前のテストとして残す。）

- [ ] **Step 2: 落ちることを確かめる**: `cmake --build build && build/mosatest tests/pl/types.pl` — `iso_acyclic_atom` などが `existence_error(procedure, acyclic_term/1)` で落ちる。
- [ ] **Step 3: 実装**。`types.cpp` の振り分けに `name == "acyclic_term" && goal.arity == 1` を足し、`acyclic_terms(machine, std::array<Cell, 1>{goal.arg(machine, 0)})` の結果で成功・失敗にする。`engine.cpp` の表に `{"acyclic_term", 1}`。頭のコメントに「8.3.11 acyclic_term(@term)。エラー無し。tests/pl/types.pl」を足す。
- [ ] **Step 4: 通ることを確かめる**: `build/mosatest tests/pl/types.pl` が全通過。`ctest --test-dir build -R 'prolog_tests|diff_tests_pl'` が通る。
- [ ] **Step 5: 準拠スイートに入れる**。`lgt_suite.hpp` の `kPredicateScope` に `"acyclic_term_1"`。`build/mosatest --suite tests/conformance/logtalk` を走らせ、`docs/conformance.md` の「別欄」からディレクトリの行 `predicates/acyclic_term_1 4` が消え、代わりに件ごとの行（`iso_acyclic_term_1_02`、`lgt_acyclic_term_1_03` の if 側と `- test` 側。旗 `coinduction` はランナーが常に偽にする）だけが残ること、分母が 2171 のまま（01 は `succeeds/1` で数えない）で合格数が 2136 のままであることを確かめる。
- [ ] **Step 6: Commit**（2 つに分ける）: `[phase9] acyclic_term/1 を実装（8.3.11.4 の Examples 7 行が types.pl で通る）` と `[phase9] 準拠スイートの範囲に acyclic_term_1 を入れる（分母 2171 のまま。STO の件は件ごとに別欄。conformance.md 再生成）`。

---

### Task 2: `subsumes_term/2`（8.2.4）

**Files:**
- Create: `src/builtins/subsumes.cpp`
- Modify: `src/machine/builtin_api.hpp`（`run_subsumes_term` の宣言、34 行の `run_phrase` の隣）
- Modify: `src/builtins/control.cpp`（258 行の `phrase` の隣で `name == "subsumes_term" && goal.arity == 2` を振り分け）
- Modify: `src/machine/engine.cpp`（表に `{"subsumes_term", 2}`）
- Modify: `CMakeLists.txt`（`mosa_lib` のソース一覧に `src/builtins/subsumes.cpp`）
- Modify: `src/machine/lgt_suite.hpp`（`"sub_atom_5"` の次に `"subsumes_term_2"`）
- Modify: `src/machine/lgt_exec.cpp`（214 行の `runner_functor` から `"subsumes_term"` を外す）、`src/machine/lgt_judge.cpp`（293 行の `check_subsumes` の `subsumes_term` の枝を外す。`subsumes` と `variant`、`=~=` は残す）
- Modify: `docs/iso-notes.md`、`docs/iso-notes.ja.md`（「## 8.2.2 unify_with_occurs_check/2 …」の次に「## 8.2.4 subsumes_term/2 and cyclic terms」）
- Test: `tests/pl/unify.pl`

**Interfaces:**
- Consumes: `do_unify(machine, a, b)`、`restore_state(machine, heap_mark, trail_mark, extras_mark)`（`src/builtins/terms.cpp` 67〜71 行の `functor/3` と同じ形）、`term_variables/2` の歩き手（`terms.cpp` 382 行から辿れる関数。`std::vector<Cell>` を返す形に切り出してよい）。`unify_with_occurs_check` は使わない（設計 2 節: 循環していない入力では真偽が同じで、循環した入力では iso-notes 8.2.2 の決定 B が SWI とスイートに反する）。
- Produces: `[[nodiscard]] bool run_subsumes_term(Impl& machine, const Goal& goal);`（`namespace mosa`）。成功しても束縛を残さない。

- [ ] **Step 1: 失敗するテストを書く**。`tests/pl/unify.pl` の末尾に 8.2.4.4 の Examples を規格の行のまま写し、Review Focus 1・2 の行を足す:

```prolog
% 8.2.4.4 subsumes_term/2（Cor.2）
test(iso_subsumes_atom, subsumes_term(a, a), true).
test(iso_subsumes_general, subsumes_term(f(X, Y), f(Z, Z)), true).
test(iso_subsumes_specific, subsumes_term(f(Z, Z), f(X, Y)), fail).
test(iso_subsumes_occurs, subsumes_term(g(X), g(f(X))), fail).
test(iso_subsumes_var_occurs, subsumes_term(X, f(X)), fail).
test(iso_subsumes_chain, (subsumes_term(X, Y), subsumes_term(Y, f(X))), true).
test(subsumes_no_binding, (subsumes_term(f(X), f(Y)), var(X), var(Y), X \== Y), true).
test(subsumes_no_binding_general, (subsumes_term(X, a), var(X)), true).
test(subsumes_cyclic_specific, (X = f(X), subsumes_term(_, X)), true).
test(subsumes_cyclic_in_arg, (X = f(X), subsumes_term(f(_), f(X))), true).
test(subsumes_cyclic_general, (X = f(X), subsumes_term(X, _)), fail).
test(subsumes_cyclic_both, (X = f(X), Y = f(Y), subsumes_term(X, Y)), true).
test(subsumes_alias_then_cycle, subsumes_term(f(V, V), f(W, g(W))), fail).
```

  （循環の 4 行は iso-notes の新しい項目の B の側。所有者が A を選んだら、期待値ではなく実装を `unify_with_occurs_check` に替え、これらの行の期待を A の値に書き直す。）

- [ ] **Step 2: 落ちることを確かめる**: `build/mosatest tests/pl/unify.pl` — `existence_error(procedure, subsumes_term/2)`。
- [ ] **Step 3: iso-notes に項目を書く**（英・日）。見出し「8.2.4 subsumes_term/2 and cyclic terms」。内容: Cor.2 の定義は `unify_with_occurs_check/2` を使う。mosa のそれは 8.2.2 の決定 B で、すでに循環した項への束縛も失敗する。そのため字面どおりなら `X = f(X), subsumes_term(f(_), f(X))` は偽、SWI とスイートの STO の行（`lgt_subsumes_term_2_15`、真を期待）は真。循環していない入力では、既定の単一化と変数の並びの検査で同じ真偽になる。「A か B か: A は 8.2.2 の決定 B を継いで偽にする。B は既定の単一化で真にする（SWI とスイートに同じ）。確認前は B」。
- [ ] **Step 4: 実装** `run_subsumes_term`。順: `Specific` の変数を `term_variables` の歩き手で `std::vector<Cell> before` に取る → 3 つの印を取る → `do_unify` で `General` と `Specific` を単一化（失敗なら `restore_state` して失敗）→ `Specific` の変数を取り直して `after` にし、`before.size() == after.size()` かつ各位置で deref したセルが同じ未束縛の変数（`heap.deref(before[i]) == heap.deref(after[i])` かつ unbound）であることを確かめる → 成否にかかわらず `restore_state` → 成功なら `succeed`。頭のコメント: 「8.2.4 subsumes_term(@term, @term)。エラー無し。循環した項は iso-notes 8.2.4。tests/pl/unify.pl」。`subsumes_cyclic_both` は `before` と `after` が共に空で真。`subsumes_alias_then_cycle` は `W` が `g(W)` に束縛されて `after` が空になり偽。
- [ ] **Step 5: 通ることを確かめる**: `build/mosatest tests/pl/unify.pl` 全通過。`ctest --test-dir build -R 'prolog_tests|diff_tests_pl'`。
- [ ] **Step 6: Commit**: `[phase9] subsumes_term/2 を実装（8.2.4.4 の Examples 6 行と循環・束縛の 7 行が unify.pl で通る。循環した項は iso-notes 8.2.4 の B）`。
- [ ] **Step 7: ランナーの横取りを外す**。`lgt_exec.cpp` の `runner_functor` と `lgt_judge.cpp` の `check_subsumes` から `subsumes_term` を外し（`subsumes`、`variant`、`=~=` はそのまま）、`build/mosatest --suite tests/conformance/logtalk` を走らせて合格数が 2136 のままであることを確かめる。減った件があれば、その件の `subsumes_term(...)` を `mosa` で手で評価して組み込みの誤りを直す（横取りに戻さない）。`docs/agreement.md` の「判定」の件数が減る（組み込みを通る件が差分テストに入る）ので `mosatest --diff tests/conformance` も走らせ、差分 0。
- [ ] **Step 8: Commit**: `[phase9] ランナーの subsumes_term の判定を組み込みに任せる（lgt_exec と lgt_judge から外す。スイート合格 2136 のまま、diff 差分 0、agreement.md 再生成）`。
- [ ] **Step 9: 準拠スイートに入れる**。`kPredicateScope` に `"subsumes_term_2"`。`build/mosatest --suite tests/conformance/logtalk` で別欄からディレクトリの行 `predicates/subsumes_term_2 20` が消え、件ごとの行（15〜17 の if 側と `- test` 側、計 6 行）だけが残り、分母 2185（14 件すべて通れば合格 2150）。落ちる行があれば直す（別欄に戻さない）。
- [ ] **Step 10: Commit**: `[phase9] 準拠スイートの範囲に subsumes_term_2 を入れる（分母 2185、合格 2150、STO の 6 行は件ごとに別欄。conformance.md 再生成）`。

---

### Task 3: ADR 0011 の改訂 4（reader の枠）

**Files:**
- Modify: `docs/adr/0011-bounded-native-stack.md`（末尾に `## Amendment 4 (2026-MM-DD, approval pending)`）、`docs/adr/0011-bounded-native-stack.ja.md`

**Interfaces:**
- Produces: Task 4・5 が従う枠の形。

- [ ] **Step 1: 改訂を書く**（Context・Decision・Rejected alternatives・Consequences を各 3 行以内）。Decision に次を書く: 「reader は `parse_expr` の相互再帰をやめ、`std::vector<Frame>` に積んだ枠（種類: `Prefix`、`Infix`、`Args`、`List`、`ListTail`、`Paren`、`Curly`）と 1 つのループで読む。各枠は『この枠の結果が満たす最大優先順位 `outer_max`』と『いま待つ被演算子の最大優先順位 `want`』を持つ。判断（`parse_name`、`find_prefix`、`find_follower`、`can_start`、`bad_token`、`max_arity`、`needs_minus`）と構文エラーの位置・トークンは変えない」。Rejected: 「再帰のまま深さを数えて `resource_error` にする（限界が OS ごとに違い、1,000,000 段の往復が崩れる）」「字句解析の段で括弧の深さだけ先に数える（演算子の入れ子に効かない）」。Consequences: 「ctest `stack_deep_read` が 5 つの形を 1,000,000 段で 1 MiB の下で通す。iso-notes 7.1 の reader の段落を閉じる」。
- [ ] **Step 2: 所有者に承認を問う**（「改訂 4 のとおり進めてよいか、A（承認）か B（枠の形を変える）か」）。承認が「計画して実装して」の依頼に含まれているときは、Status に「Approved (date; the owner asked for the plan to be implemented in the same request)」と書いてよい。
- [ ] **Step 3: Commit**: `[phase9] ADR 0011 改訂 4（reader を枠のスタックで動かす）を書く（文書だけ）`

---

### Task 4: reader の演算子の部分を枠で動かす

**Files:**
- Modify: `include/mosa/reader.hpp`（`Reader` の private に `Frame`、`FrameKind`、`frames_`、新しいメンバ関数）
- Modify: `src/read/reader.cpp`（192〜292 行の `apply_prefix`、`parse_rest`、`parse_expr`）
- Create: `src/read/reader_frames.cpp`（ループと還元。`reader.cpp` が 800 行を超えないよう最初から分ける）
- Modify: `CMakeLists.txt`（`src/read/reader_frames.cpp`）
- Test: `tests/pl/read.pl`（1 行足す）、`tests/stack/deep_read.pl`（Task 5 で登録。ここでは 2 つの形を書く）

**Interfaces:**
- Consumes: `Reader::Parsed`、`parse_prefix`、`parse_name`、`find_follower`、`compound`、`bad_token`（既存）。
- Produces（`include/mosa/reader.hpp`）:

```cpp
enum class FrameKind : std::uint8_t { Prefix, Infix, Args, List, ListTail, Paren, Curly };
struct Frame {
  FrameKind kind;
  int outer_max;            // この枠の結果が満たす最大優先順位
  int want;                 // いま待つ被演算子の最大優先順位
  OpDef op{};               // Prefix, Infix
  Cell left{};              // Infix
  std::string functor;      // Args
  std::vector<Cell> items;  // Args, List
};
[[nodiscard]] Parsed parse_expr(int max_priority);          // 入口。ループを回す
[[nodiscard]] Parsed parse_primary(int want);               // 1 つの基本項。枠を積んだときは pushed を返す
[[nodiscard]] Parsed follow(Parsed have, int current_max);  // 中置・後置。中置なら Infix を積んで pushed
[[nodiscard]] Parsed reduce(Frame frame, Parsed have);      // 枠を 1 つ降ろして項を作る
```

  `Parsed` に `bool pushed = false;` を足す（枠を積んで続きを待つ印）。`current_max` は `frames_.empty() ? top_max : frames_.back().want`。

- [ ] **Step 1: 失敗するテストを書く**。`tests/stack/deep_read.pl` を作り（`.expected` は 5 行 `nested:ok` `list:ok` `prefix:ok` `xfy:ok` `paren:ok`。Task 5 で登録）、ここでは `prefix` と `xfy` の 2 つを書く。項は `deep_builtins.pl` と同じく数え上げで作り、`tests/tmp/stack/deep_read.txt` に `writeq/1` で書いてから `read/1` で読み戻し、`==` で比べる:

```prolog
:- initialization(main, main).
depth(1000000).
nest_prefix(0, X, X) :- !.
nest_prefix(N, X, Y) :- M is N - 1, nest_prefix(M, -(X), Y).
nest_xfy(0, X, X) :- !.
nest_xfy(N, X, Y) :- M is N - 1, nest_xfy(M, N:X, Y).
roundtrip(Name, T) :-
    open('deep_read.txt', write, W), writeq(W, T), write(W, '.'), nl(W), close(W),
    open('deep_read.txt', read, R), read(R, T2), close(R),
    ( T == T2 -> write(Name), write(':ok') ; write(Name), write(':different') ), nl.
main :- depth(D), nest_prefix(D, a, P), roundtrip(prefix, P), nest_xfy(D, a, X), roundtrip(xfy, X).
```

  `tests/pl/read.pl` に Review Focus 4 の行を足す（既存の行は変えない）:

```prolog
test(read_error_deep_token, (open_string('f(g(h(i(j(,))))).', S), read(S, _)), error(syntax_error(_))).
test(read_error_deep_unclosed, (open_string('f(g(h(i(j(a))))).', S), read(S, _)), error(syntax_error(_))).
```

  （61 行の `read_bare_comma` と同じ形。深い入れ子の中でも同じ `Formal` で、読み残しを終止符まで捨てて次が読めること。）

- [ ] **Step 2: 落ちることを確かめる**: `cmake --build build && cmake "-DCOMMAND=$PWD/build/mosa;$PWD/tests/stack/deep_read.pl" -DEXPECTED=$PWD/tests/stack/deep_read.expected -DWORK=$PWD/tests/tmp/stack -P tests/stack/small_stack.cmake` — SIGSEGV（1 MiB で 850 段ほどで落ちる）。`read_error_deep_token` は通る（既存の動きの記録）。
- [ ] **Step 3: 実装**。`parse_expr` を「`frames_` の底の印を取り、`parse_primary` → `follow` → `reduce` を回し、`pushed` なら続け、枠が底まで戻れば返す」ループにする。`apply_prefix` は `Frame{Prefix, outer_max, arg_max, op}` を積んで `pushed`。`parse_rest` の中置の枝は `Frame{Infix, outer_max, right_max, op, left.term}` を積んで `pushed`、後置の枝は今のまま畳む。`reduce` の `Prefix`/`Infix` は今の `apply_prefix`/`parse_rest` が作っていた項を作り、優先順位を `op.priority` にして `follow` へ戻す。括弧（`parse_arguments`、`parse_list`、`parse_paren`、`parse_curly`）はこのタスクでは今のまま `parse_expr` を再帰で呼ぶ（入れ子のループが `frames_` の底の印から上だけを使うので正しい）。各関数 60 行以内。
- [ ] **Step 4: 通ることを確かめる**: Step 2 のコマンドで `prefix:ok` `xfy:ok`（`nested` 以降はまだ無い）。`ctest --test-dir build -R 'roundtrip|prolog_tests|diff_tests_pl|writer_golden|stack_'` 全通過。`build/mosatest --suite tests/conformance/logtalk` の合格数が Task 2 の値から変わらない。
- [ ] **Step 5: Commit**: `[phase9] reader の前置と中置の被演算子を枠のスタックで読む（ADR 0011 改訂 4。1 MiB で前置 1,000,000 段と xfy 1,000,000 段の往復、syntax.pl の往復と準拠率は不変）`

---

### Task 5: reader の括弧の部分を枠に載せ、深い読みを登録する

**Files:**
- Modify: `src/read/reader_bracket.cpp`（`parse_arguments`、`parse_list`、`take_bar_tail`、`parse_paren`、`parse_curly`）、`src/read/reader_frames.cpp`（`reduce` の `Args`/`List`/`ListTail`/`Paren`/`Curly`）
- Modify: `tests/stack/deep_read.pl`、`tests/stack/deep_read.expected`
- Create: `tests/stack/deep_file.cmake`（`long_literal.cmake` を写して、テストの手順が `f(f(...))` 100,000 段の項を 1 つ持つデータのファイルを書き、`mosac build` で作った小さなプログラムが、そのファイルを開いて `read/1` で読み、深さを確かめる。実行ファイルは 1 MiB のスタックで動かす。深い項はプログラムのソースに置かない）
- Modify: `tests/tests.cmake`（`stack_deep_read` を 56〜60 行の `stack_deep_write` と同じ形で、`mosac_deep_file` を 626 行の `mosac_long_list` と同じ形で登録。`TIMEOUT 900`）
- Modify: `docs/iso-notes.md` 632 行の節（reader の段落を「フェーズ9で reader も枠で読む」に）、`docs/iso-notes.ja.md` 634 行の節

**Interfaces:**
- Consumes: Task 4 の `Frame`、`parse_primary`、`follow`、`reduce`。
- Produces: `parse_primary` が `(`、`[`、`{`、関数表記の `(` で枠を積む。`reduce` の `Args` は項を `items` に足し、次が `,` なら `reject_empty_comma` と 1024・`needs_minus` の検査をしてから `want = 999` で待ち、`)` なら `compound(functor, items)`（優先順位 0）。`List` は `,` / `|` / `]` で分かれ、`|` は `ListTail`（`take_bar_tail` の検査をしてから `want = 999`）、`]` は `cons_all`。`Paren` は `)` を取って優先順位 0。`Curly` は `}` を取って `{}/1`。

- [ ] **Step 1: 失敗するテストを書く**。`deep_read.pl` に `nested`（`f(...)`）、`list`（`[[...]]`）、`paren`（`(((a)))` は字面を `put_char/2` で作る）を足す:

```prolog
nest_f(0, X, X) :- !.
nest_f(N, X, Y) :- M is N - 1, nest_f(M, f(X), Y).
nest_list(0, X, X) :- !.
nest_list(N, X, Y) :- M is N - 1, nest_list(M, [X], Y).
parens(N) :- open('deep_read.txt', write, W), put_n(N, W, '('), write(W, a), put_n(N, W, ')'), write(W, '.'), nl(W), close(W),
    open('deep_read.txt', read, R), read(R, T), close(R), ( T == a -> write('paren:ok') ; write('paren:different') ), nl.
```

  `main` は `nested`、`list`、`prefix`、`xfy`、`paren` の順（`.expected` と同じ順）。

- [ ] **Step 2: 落ちることを確かめる**: Task 4 Step 2 のコマンドで `nested` の読みで SIGSEGV。
- [ ] **Step 3: 実装**。`parse_arguments`、`parse_list`、`parse_paren`、`parse_curly` を「開きを消費して枠を積み `pushed` を返す」だけにし、閉じと区切りの処理を `reduce` に移す。`take_bar_tail` の検査（`|` の後に項が無い、`|` が 2 つ）はそのまま `ListTail` を積む前に行う。空リスト `[]` と `{}` は今のまま枠を積まずに返す。`parse_expr` の再帰呼び出しが `src/read/` に 1 つも残らないこと（`grep -n 'parse_expr(' src/read/*.cpp` が入口の定義とループの中だけ）。
- [ ] **Step 4: 通ることを確かめる**: `cmake --build build && ctest --test-dir build -R 'stack_deep_read|mosac_deep_file|roundtrip|prolog_tests|diff_tests_pl|writer_golden|long_literal' --output-on-failure` 全通過。100,000 段の節を持つソースを `mosac build` が扱えるかは、手で 1 度確かめる。落ちたら iso-notes 7.1 に codegen の既知の限界として書く（ADR 0011 改訂 2）。フェーズ 9 は codegen を変えない。`stack_deep_read` の所要時間を記録する（5 つの形で合計が 60 秒を超えるなら枠の積み降ろしが O(1) でない。段数を下げず、原因を直す）。`build/mosatest --suite tests/conformance/logtalk` の合格数が不変。Windows と Linux は CI に任せる（push 後に 11 ジョブを確かめる）。
- [ ] **Step 5: iso-notes 7.1 を直す**（英・日）。「Only the reader still recurses …」の段落を「Phase 9 made the reader iterative too (ADR 0011 amendment 4): …」に替え、測った段数の文を消す。決定の行に「Phase 9: the reader (ctest `stack_deep_read`)」を足す。
- [ ] **Step 6: Commit**（2 つ）: `[phase9] reader の括弧を枠に載せ、5 つの形を 1,000,000 段で読む（1 MiB の stack_deep_read、mosac_deep_file 100,000 段、syntax.pl の往復と準拠率は不変）`、`[phase9] iso-notes 7.1 の reader の段落を閉じる（文書だけ）`。

---

### Task 6: ADR 0024 改訂 3 と `Name//Arity`

**Files:**
- Modify: `docs/adr/0024-dcg-translation.md`、`.ja.md`（末尾に `## Revision 3 (2026-MM-DD, approval pending)`: `Name//Arity` はエクスポート・インポートの表と `dynamic/1`・`discontiguous/1`・`multifile/1` で `Name/(Arity+2)` の意味、`*->` の訳、`expand_term/2` を `library(dcg)` に置き `system` の隠し組み込み `'$dcg_translate'/2` で訳す）
- Modify: `src/module/qualify.cpp`（`read_indicator` 118〜137 行）
- Modify: `src/builtins/database.cpp`（540〜575 行付近の `dynamic/1` などの PI の読み取り）
- Test: `tests/pl/modules.pl`、`tests/pl/modules/`（新しいモジュールファイル 1 つ）、`tests/pl/dcg.pl`

**Interfaces:**
- Consumes: `read_indicator(Impl&, Cell, PredKey&)`（`qualify.cpp`。呼び手は `qualify.cpp` 56 行と `import.cpp` 42・117 行）。
- Produces: `read_indicator` が `N//A` を `N/(A+2)` として受ける。`database.cpp` の宣言も同じ。`abolish/1`、`current_predicate/1`、`retract/1` は変えない。

- [ ] **Step 1: ADR 0024 改訂 3 を書き、所有者に承認を問う**（Task 3 Step 2 と同じ扱い）。Commit: `[phase9] ADR 0024 改訂 3（Name//Arity、本体の *->、expand_term/2）を書く（文書だけ）`。
- [ ] **Step 2: 失敗するテストを書く**。`tests/pl/modules/dcg_export.pl`:

```prolog
:- module(dcg_export, [greet//0, dcg_item//1]).
greet --> [hello], dcg_item(world).
dcg_item(X) --> [X].
```

  `tests/pl/modules.pl` に:

```prolog
:- use_module('modules/dcg_export.pl', [dcg_item//1]).
test(mod_export_dcg_call, phrase(dcg_export:greet, [hello, world]), true).
test(mod_import_dcg_list, phrase(dcg_item(world), [world]), true).
test(mod_export_dcg_property, predicate_property(dcg_export:greet(_, _), exported), true).
```

  （53 行の `use_module('modules/別源.pl', [...])` と同じ書き方。）エクスポートの表のエラーは読み込み時なので `tests/cpp/dcg_test.cpp` の `load_program` の検査（16〜17 行の形）で 3 つ: `":- module(m_neg, [x//(-1)])."` が `domain_error(not_less_than_zero,-1)`、`":- module(m_big, [x//1023])."` が `representation_error(max_arity)`、`":- module(m_atom, [1//0])."` が `type_error(atom,1)` で拒まれる。`tests/pl/dcg.pl` に:

```prolog
test(dcg_dynamic_slashslash, (dynamic(dyn_nt//1), assertz(dyn_nt(1, [x|S], S)), phrase(dyn_nt(1), [x])), true).
test(dcg_discontiguous_slashslash, discontiguous(disc_nt//0), true).
test(dcg_abolish_keeps_iso, abolish(foo//1), error(type_error(predicate_indicator, foo//1))).
```

- [ ] **Step 3: 落ちることを確かめる**: `build/mosatest tests/pl/modules.pl tests/pl/dcg.pl` — `type_error(predicate_indicator, greet//0)` などで落ちる。
- [ ] **Step 4: 実装**。`read_indicator`: 関数子が `//` で引数 2 のときも通し、`value + 2 > 1024` を `representation_error(max_arity)`、`key.arity = value + 2`。`database.cpp` の宣言の読み取りも同じ（`arity + 2`、エラーの順は同じ）。先に `grep -rn '//' tests/conformance/logtalk/prolog/predicates/dynamic_1 tests/conformance/logtalk/prolog/predicates/abolish_1` で `//` を期待する行が無いことを確かめる（Task 6 を書いた時点では無い）。
- [ ] **Step 5: 通ることを確かめる**: `build/mosatest tests/pl/modules.pl tests/pl/dcg.pl` 全通過。`ctest --test-dir build -R 'dcg_test|prolog_tests|diff_tests_pl'`。準拠率不変。
- [ ] **Step 6: Commit**: `[phase9] エクスポート・インポートの表と dynamic/discontiguous/multifile で Name//Arity を受ける（Name/(Arity+2) の意味。modules.pl 3 行、dcg_test.cpp のエラー 3 つ、dcg.pl 3 行、abolish/1 は ISO のまま）`

---

### Task 7: 文法規則の本体の `*->`

**Files:**
- Modify: `src/machine/dcg.cpp`（24 行の `enum class Step` に `SoftIf`、146〜150 行の `,`/`->` の枝、180 行の名前の選択）
- Test: `tests/pl/dcg.pl`

**Interfaces:**
- Consumes: `Work{Step, body, s0, s, …}` の積み方（146〜157 行）。
- Produces: `(C *-> T)` → `(C' *-> T')`（`C: S0→S1`、`T: S1→S`）。`(C *-> T ; E)` はすでに `;` の枝が `*->` を左に持つので、左の `*->` が `SoftIf` で訳されれば `(C' *-> T' ; E')` になる。

- [ ] **Step 1: 失敗するテストを書く**。`tests/pl/dcg.pl` に:

```prolog
soft_nt --> ([x] *-> [y] ; [z]).
soft_multi --> (one_of *-> [y]).
one_of --> [a].
one_of --> [b].
test(dcg_soft_then, phrase(soft_nt, [x, y]), true).
test(dcg_soft_else, phrase(soft_nt, [z]), true).
test(dcg_soft_no_else_for_then, phrase(soft_nt, [x, z]), fail).
test(dcg_soft_multi, findall(L, (member(L, [[a, y], [b, y]]), phrase(soft_multi, L)), Ls), solutions([Ls = [[a, y], [b, y]]])).
test(dcg_soft_bare_fail, phrase(soft_multi, [c, y]), fail).
test(dcg_phrase_soft_body, phrase(([x] *-> [y] ; [z]), [x, y]), true).
```

  （`findall` の行は `lib_lists` の `member/2` が無ければ `phrase(soft_multi, [a, y]), phrase(soft_multi, [b, y])` の 2 行に分ける。）

- [ ] **Step 2: 落ちることを確かめる**: `build/mosatest tests/pl/dcg.pl` — `existence_error(procedure, (*->)/4)`。
- [ ] **Step 3: 実装**。`Step::SoftIf` を足し、146 行の条件を `name == "," || name == "->" || name == "*->"` に、`Work` の `step` を `name == "*->" ? Step::SoftIf : …` に、180 行の名前の選択に `Step::SoftIf ? "*->"` を足す。
- [ ] **Step 4: 通ることを確かめる**: `build/mosatest tests/pl/dcg.pl` 全通過。`ctest --test-dir build -R 'dcg_test|prolog_tests|diff_tests_pl'`（`diff_tests_pl` の REQUIRE に `dcg.pl` が入っているので差分 0 を確かめる）。
- [ ] **Step 5: Commit**: `[phase9] 文法規則の本体の *-> をソフトカットとして訳す（ADR 0024 改訂 3。dcg.pl 6 行、差分テスト 0）`

---

### Task 8: `library(dcg)` の `expand_term/2`

**Files:**
- Create: `lib/dcg.pl`、`src/builtins/dcg_translate.cpp`、`tests/pl/lib_dcg.pl`
- Modify: `src/machine/builtin_api.hpp`（`run_dcg_translate`）、`src/builtins/control.cpp`（258 行の隣で `name == "$dcg_translate" && goal.arity == 2`）、`src/machine/engine.cpp`（319 行付近の `{"$format_float", 4}` の隣に `{"$dcg_translate", 2}`）、`CMakeLists.txt`
- Modify: `tests/pl/modules.pl`（`strings_needs_use` の隣に `dcg_needs_use`）
- Test: `tests/pl/lib_dcg.pl`

**Interfaces:**
- Consumes: `DcgOut dcg_rule(Impl& machine, Cell rule)`（`src/machine/dcg.hpp`。ローダーの `load.cpp` 426 行と同じ呼び方）、`do_unify`、`restore_state`。`lib/format.pl` 215 行の `'$format_float'/4` の呼び方。
- Produces: `[[nodiscard]] bool run_dcg_translate(Impl& machine, const Goal& goal);`（`'$dcg_translate'(+Rule, ?Clause)`。第 1 引数が `-->/2`（deref 後。`M:(H --> B)` も `dcg_rule` が受ける形ならそのまま渡す）なら訳した節を、そうでなければ第 1 引数そのものを第 2 引数と単一化する）。`lib/dcg.pl`: `:- module(dcg, [expand_term/2]).` と `expand_term(Term, Expanded) :- '$dcg_translate'(Term, Expanded).`（頭のコメントは `lib/strings.pl` と同じ形: SPEC 第5節の表、テストファイル名、ADR 0024 改訂 3）。

- [ ] **Step 1: 失敗するテストを書く**。`tests/pl/lib_dcg.pl`（`lib_strings.pl` の頭と同じ形で `:- use_module(library(dcg)).`）:

```prolog
:- use_module(library(dcg)).
:- dynamic(exp_a//0).
:- dynamic(exp_b//0).
test(expand_term_rule, (expand_term((a --> [x], b), C), C = (H :- _), H = a(_, _)), true).
test(expand_term_assert, (expand_term((exp_a --> [x], exp_b), C), assertz(C), assertz((exp_b([y|S], S))), phrase(exp_a, [x, y])), true).
test(expand_term_other, expand_term(foo(1), X), solutions([X = foo(1)])).
test(expand_term_var, expand_term(_, _), error(instantiation_error)).
test(expand_term_var_head, expand_term((_ --> [x]), _), error(instantiation_error)).
test(expand_term_bad_head, expand_term((1 --> [x]), _), error(type_error(callable, 1))).
test(expand_term_bad_list, expand_term((a --> [x|_]), _), error(instantiation_error)).
test(expand_term_pushback, (expand_term((a, [p] --> [x]), C), C = (a(S0, S) :- _), var(S0), var(S)), true).
```

  `tests/pl/modules.pl` に `test(dcg_needs_use, expand_term(foo, _), error(existence_error(procedure, expand_term/2))).`（`strings_needs_use` と同じ形。`modules.pl` は `library(dcg)` を読まない）。

- [ ] **Step 2: 落ちることを確かめる**: `build/mosatest tests/pl/lib_dcg.pl` — `library(dcg)` が見つからず読み込みが落ちる。`dcg_needs_use` は通る。
- [ ] **Step 3: 実装** `run_dcg_translate` と `lib/dcg.pl`。順: 第 1 引数を deref し、未束縛なら `instantiation_error` → `-->/2` なら `dcg_rule` を呼び、`formal` があれば `raise_formal`（`phrase.cpp` の `raised` と同じ）、無ければ `translated.term` を結果に → それ以外は第 1 引数を結果に → 第 2 引数と `do_unify`、失敗なら `restore_state` して失敗。頭のコメント: 「'$dcg_translate'(+term, ?term)。library(dcg) の expand_term/2 の実体。文法規則を 1 つ訳す（ADR 0024 改訂 3）。エラー: instantiation_error、type_error(callable, Culprit)、type_error(list, Culprit)。tests/pl/lib_dcg.pl」。
- [ ] **Step 4: 通ることを確かめる**: `build/mosatest tests/pl/lib_dcg.pl tests/pl/modules.pl`、`cmake -S . -B build`（`CMakeLists.txt` 225 行の `file(GLOB … lib/*.pl)` に `dcg.pl` を拾わせる）のあと `ctest --test-dir build -R 'prolog_tests|diff_tests_pl|mosac_'`。`tests/mosac/standalone_test.cmake` の `library(strings)` の確かめと同じ形で、`use_module(library(dcg))` と `expand_term/2` を使うプログラムが `mosac build` で動く確かめを 1 つ足す。準拠率不変。
- [ ] **Step 5: Commit**: `[phase9] library(dcg) の expand_term/2 を実装（'$dcg_translate'/2 で文法規則 1 つを訳す。lib_dcg.pl 8 行、modules.pl の dcg_needs_use、mosac の束ね、差分テスト 0）`

---

### Task 9: 文書を閉じ、CI を確かめ、フェーズを閉じるかを問う

**Files:**
- Modify: `CHANGELOG.md`、`CHANGELOG.ja.md`（`## 0.1.2` の上に `## Unreleased`: Added に 2 つの述語、`Name//Arity`、`*->`、`library(dcg)` の `expand_term/2`、深い入れ子の読み; Known limits から「深く入れ子になった項で落ちる」「`subsumes_term/2` と `acyclic_term/1` が無い」「`Name//Arity` のエクスポート」「`*->`」を消し、「`assertz/1` の文法規則」を「`library(dcg)` の `expand_term/2` を挟む。`term_expansion/2` と `goal_expansion/2` は無い」に書き替える; 「ISO との違い」の `subsumes_term/2` の行を消す）
- Modify: `docs/manual/en/14-reference.md`（537 行の制限を消し、組み込みの表に `subsumes_term/2` と `acyclic_term/1` を足す）、`docs/manual/ja/14-reference.md`（508 行）。`mosatest --manual docs/manual --all-tables` が通る形の例を 1 つずつ添える
- Modify: `docs/manual/en/12-modules-libraries.md`、`ja`（ライブラリの表に `library(dcg)` の行、エクスポートの表に `Name//Arity` が書ける旨 1 文）
- Modify: `README.md`、`README.ja.md`（96 行付近の進捗の文に「Phase 9 … after the check of its exit criteria」。バッジは所有者が閉じると決めてから `phase-9_done`）
- Modify: `docs/STATUS.md`

- [ ] **Step 1: CHANGELOG とマニュアルを直す**（英・日を同じコミットで）。
- [ ] **Step 2: 確認**: `build/mosatest --manual docs/manual --all-tables` が 0 failed。`grep -n 'subsumes_term\|acyclic_term' CHANGELOG.md docs/manual/en/14-reference.md` が「無い」と書く行に当たらない。
- [ ] **Step 3: 全体の確認**: `cmake --build build && ctest --test-dir build --output-on-failure`（直列）全通過。`build/mosatest --suite tests/conformance/logtalk` で分母 2185・合格率 ≥ 98.4%、`docs/conformance.md` と `conformance.json` を再生成してコミット。`build/mosatest --diff tests/pl` と `tests/conformance` の差分 0（`docs/agreement.md` 再生成）。
- [ ] **Step 4: Commit**: `[phase9] CHANGELOG の未リリースとマニュアル第12章・第14章にフェーズ9の追加を書き、既知の制限 4 つを消す（mosatest --manual --all-tables 通過。文書だけ）`。push して CI の 11 ジョブを確かめ、run の番号を STATUS に書く。
- [ ] **Step 5: 所有者に問う**: 「完了条件 1〜4 を満たした。フェーズ9を閉じる（A）か、何か足す（B）か」。A なら README のバッジを `phase-9_done` にし、表の 9 行目を Done にして `[phase9] フェーズ9を終える（持ち主の決定 A。…）` でコミットする。

---

## 実行後に残すもの

- 次のフェーズの候補 B（多倍長整数）は設計文書の「候補と推奨」に書いてある。フェーズ9を閉じたあと、所有者が B を選べば別の設計を書く。
- 公開（0.1.3 か 0.2.0 か）は所有者の依頼で別に行う（`docs/superpowers/plans/2026-11-07-release-0.1.0.md` の手順）。
