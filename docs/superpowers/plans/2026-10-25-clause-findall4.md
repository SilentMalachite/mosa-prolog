# clause/2 と current_predicate/1 と findall/4 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 未決 3、5、6 を、利用者が節を調べて差分リストで解を足せる動作にし、`clause/2` と `current_predicate/1` と `findall/4` を登録する。

**Architecture:** 節は今の IR のまま返す。`Clause` にソース項は足さない。同じスロットの変数は一つの参照に戻し、匿名は毎回新しい参照にする。`findall/4` は `findall/3` の収集をそのまま使い、結果リストの末尾を第4引数にする。

**Tech Stack:** C++20、CMake 3.25 以上、CTest、標準ライブラリのみ。

**Spec:** `docs/SPEC.md` の段階 C（8.8 と 8.10）。節の並びは承認済みの `docs/adr/0004-clause-database.md`。この計画は `docs/superpowers/plans/2026-10-25-phase3-builtins.md` を書き換えない。未決 3、5、6 の実装禁止は、この計画が上書きする。

作業ツリーは `/Users/anchu/projects/GitHub/mosa-prolog-phase3` の `phase3-builtins` だけである。`main` の作業ツリーは変えない。実装者はこの計画のチェックを付けない。進捗は git と `docs/STATUS.md` に残す。

## 決めたこと

Prolog の調べ方に合わせた決定である。実装者は選び直さない。

- 未決 5 は A。利用者が定義した述語は公開である。`consult` の静的な節、`:- dynamic`、`assert` で作った述語、節を `retract` で空にした動的述語を含む。`clause/2` は定義順に節を返す。事実の本体はアトム `true` である。頭と本体で同じスロットの変数は、返した二つの項で同じ参照である。
- 組込と制御は非公開である。`is_builtin_procedure` が真なら `permission_error(access, private_procedure, PI)` であり、`modify` や `static_procedure` ではない。無い述語は失敗する。節の無い動的述語も失敗するが、非公開ではない。
- 静的な利用者述語は読める。読めないのは組込だけである。
- 未決 6 は B。`current_predicate/1` は利用者の述語だけを、定義された順に返す。組込は候補に入らない。`current_predicate(true/0)` は失敗であり、`permission_error` ではない。空の動的述語は成功する。`abolish` のあとは失敗する。
- 未決 3 は A。`findall(Template, Goal, List, Tail)` の `List` は、解のリストの末尾が `Tail` である差分リストである。解が無ければ `List` と `Tail` を単一化する。第4引数はリストかどうか検査しない。未束縛の末尾がこの述語の本体である。`findall/3` は末尾が `[]` の `findall/4` である。
- 本体の `false/0` は、コンパイルが `fail` に畳んでいるので、`clause/2` は `fail` を返す。`Clause` に字面を保存しない。ADR 0004 が `Clause` の形をフェーズ2のままにしている。
- `findall/4` は登録する。`clause/3` や `current_predicate/2` は登録しない。

## Global Constraints

フェーズ3計画の制約を、この2タスクにも適用する。

- 言語は C++20。コンパイラ拡張、`__int128`、`#pragma once` 以外の独自プラグマは使わない
- 依存は C++ 標準ライブラリと CMake だけ。新しいライブラリは追加しない
- 警告は `-Wall -Wextra -Wpedantic -Werror`。MSVC は `/W4 /WX /utf-8 /permissive- /Zc:__cplusplus`
- 生の `new` と `delete`、`reinterpret_cast`、C 配列、マクロは使わない。`#pragma once` は使ってよい
- 1 関数は 60 行以内、1 ファイルは 800 行以内。超える前に、そのタスクの Files に書いた分割へ移す
- 識別子は英語。コメント、コミット、文書は日本語。コメントは理由だけを書く
- Prolog の失敗と `throw/1` は機械の状態で表す。C++ の例外は `mosa::ResourceLimit` だけである
- Cell のタグ、IR（ADR 0003）、`Clause` の既存メンバは変えない。整数は 64bit のままである
- 版文字列は `mosa 0.0.1 (phase 2)` のままである
- コミットメッセージは `[phase3] ` で始まる日本語 1 行とし、何を確認したかを含める
- テストを弱めない、飛ばさない、期待値を実装に合わせない
- `tests/cpp/harness.hpp` と `include/mosa/mosa.hpp` は変更しない
- エラーの Formal はヘルパで作る。ヘルパの引数の `string_view` は許す。呼び出し側に項の形を直書きしない
- 引数検査は、その述語の Errors の列挙順に行う。検査が終わる前に束縛しない
- エラーのテストは Formal だけを比べ、Context は比べない
- 組込述語のファイル先頭コメントには、規格の条番号、引数のモード、投げうるエラー、テストファイル名を書く
- 各タスクの終わりに `cmake --build build && ctest --test-dir build` を通し、`docs/STATUS.md` をそのタスクの文へ置き換える。15 行を超えない

## Review Focus

テストが無いと、次の入力で壊れる。各行のテストは、そのタスクの Step 1 にある。

- `clause(legs(A, 6), B)` の `B` は `insect(A)` であり、`A` は頭と同じ参照である。別の変数に戻すと `==` が失敗する。タスク 1
- `clause(true, B)` は `permission_error(access, private_procedure, true/0)` である。`current_predicate(true/0)` は失敗である。無い述語への `clause/2` は失敗であり、`permission_error` ではない。タスク 1
- `:- dynamic(none/1)` のあと、`current_predicate(none/1)` は成功し、`clause(none(X), B)` は失敗する。タスク 1
- 本体が `false` の節は、`clause/2` が `fail` を返す。タスク 1
- `findall(X, fail, L, T)` は `L` と `T` を単一化し、`T` を `[]` にしない。`findall(X, (X=1; X=2), L, [])` は `L = [1, 2]` である。タスク 2
- `findall/4` のゴールが例外なら、`List` も `Tail` も束縛しない。タスク 2

## ファイルの責任

| パス | 責任 | タスク |
| --- | --- | --- |
| `src/builtins/clause_term.cpp` | IR を項に戻す。頭と本体は同じスロット表を使う | 1 |
| `src/builtins/database.cpp` | `clause/2` と `current_predicate/1` の検査、列挙、単一化 | 1 |
| `src/builtins/solutions.cpp` | `findall/4`。収集は `findall/3` と一つ | 2 |
| `tests/pl/clause.pl` | 8.8 の、この決定に対応する `test/3` | 1 |
| `tests/pl/solutions.pl` | `findall/4` の `test/3` を末尾に足す | 2 |
| `docs/iso-notes.md` | 未決 3、5、6 を決定の文に変える | 1 と 2 |

`run_database` が `clause/2` と `current_predicate/1` を扱う。`run_solutions` が `findall/4` を扱う。呼び順は変えない。

---

### Task 1: 節の参照と利用者述語の列挙

**Files:**
- Create: `src/builtins/clause_term.cpp`、`tests/pl/clause.pl`
- Modify: `src/builtins/database.cpp`、`src/machine/internal.hpp`、`CMakeLists.txt`、`docs/iso-notes.md`、`docs/STATUS.md`
- Test: `tests/pl/clause.pl`

**Interfaces:**
- Consumes: `Predicate::clauses`、`Clause` の `head` と `body`、`is_builtin_procedure`、`raise_formal`、`formal_instantiation`、`formal_type`、`formal_permission`、`cell_functor`。`Clause` のメンバは足さない
- Produces: `Cell ir_to_term(Impl& machine, const IrTerm& term, std::vector<Cell>& slots)`。`slots[n]` はスロット `n` の最初の出現で作り、以後は同じセルを返す。`IrTag::Anon` は `slots` を使わず、出現ごとに新しい参照を返す

- [ ] **Step 1: 失敗するテストを書く**

`tests/pl/clause.pl` に次を書く。

```prolog
:- dynamic(insect/1).
insect(ant).
insect(bee).

:- dynamic(none/1).

legs(A, 6) :- insect(A).

path(A, C) :- edge(A, B), edge(B, C).

frozen(1).

quiet :- false.

test(clause_facts, clause(insect(X), true), solutions([X = ant, X = bee])).
test(clause_share, (clause(legs(A, 6), B), B == insect(A)), true).
test(clause_and, (clause(path(A, C), B), B == (edge(A, D), edge(D, C))), true).
test(clause_static, clause(frozen(X), B), solutions([(X = 1, B = true)])).
test(clause_false, clause(quiet, B), solutions([B = fail])).
test(clause_var, clause(X, Y), error(instantiation_error)).
test(clause_number, clause(1, Y), error(type_error(callable, 1))).
test(clause_body, clause(insect(X), 1), error(type_error(callable, 1))).
test(clause_builtin, clause(true, B), error(permission_error(access, private_procedure, true/0))).
test(clause_builtin_body, clause(true, 1), error(type_error(callable, 1))).
test(clause_missing, clause(no_such_pred_phase3(X), B), fail).
test(clause_empty, clause(none(X), B), fail).
test(clause_asserta, (asserta(insect(gnat)), clause(insect(X), true)), solutions([X = gnat, X = ant, X = bee])).
test(current_user, current_predicate(insect/1), true).
test(current_empty, current_predicate(none/1), true).
test(current_builtin, current_predicate(true/0), fail).
test(current_missing, current_predicate(no_such_pred_phase3/1), fail).
test(current_order, (assertz(zz1(a)), assertz(zz2(b)), findall(P, current_predicate(P), L), zz_pair(L)), true).
test(current_gen, current_predicate(N/0), solutions([N = quiet])).

zz_pair([zz1/1, zz2/1|_]).
zz_pair([zz1/1, zz2/1]).
zz_pair([_|T]) :- zz_pair(T).
```

`zz_pair/1` は、生成順の列のどこかに `zz1/1` の直後へ `zz2/1` があることを見る。`append/3` は無い。`quiet/0` はこのファイルでアリティ 0 のただ一つの利用者述語である。アリティ 0 を足すときは、`current_gen` の解の列も定義順で足す。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R '^prolog_tests$'`

Expected: `prolog_tests` が `clause/2` の未定義で失敗する。

- [ ] **Step 3: IR から項に戻し、二つの述語を実装する**

`ir_to_term` は `IrTag` を一つ残さず項に戻す。`And` は `','/2`、`Or` は `';'/2`、`IfThen` は `'->'/2`、`SoftIf` は `'*->'/2`、`True` は `true`、`Fail` は `fail`、`Cut` は `!`、`Catch` は `catch/3`、`Throw` は `throw/1`、`Halt` は引数の数に合わせた `halt` である。`Call` は中のゴールに戻す。

`clause(+head, ?body)` の検査順は、頭が変数なら `instantiation_error`、頭が callable でなければ `type_error(callable, Head)`、本体が変数でも callable でもなければ `type_error(callable, Body)`、そのあと組込なら `permission_error(access, private_procedure, PI)` である。組込でない無い述語は失敗する。一致する節は定義順に、頭と本体を同じ `slots` で戻してから単一化する。検査が終わる前に単一化しない。

`current_predicate(?predicate_indicator)` は `machine.predicates` の順だけを候補にする。指示子の検査順は、変数なら生成、形が `Name/Arity` でなければ `type_error(predicate_indicator, PI)`、名前が変数でもアトムでもなければ `type_error(atom, Name)`、アリティが変数でも整数でもなければ `type_error(integer, Arity)`、負なら `domain_error(not_less_than_zero, Arity)` である。組込名は表に無いので失敗する。

`database.cpp` が 800 行を超えるなら、`clause/2` の単一化だけを `src/builtins/database_clause.cpp` へ移す。`ir_to_term` は `clause_term.cpp` に置く。

`docs/iso-notes.md` の未決 5 と未決 6 は、上の「決めたこと」を決定の文に変える。未決 3 はこのタスクで変えない。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: 全部成功。`./build/mosa --version` は `mosa 0.0.1 (phase 2)`。

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: clause/2 は利用者の節を返し、組込は access の permission_error。current_predicate/1 は利用者だけ
次: findall/4 を差分リストにする
未解決: 未決 3。ほかの未決と、ライセンスの無い準拠スイートは iso-notes と従前どおり
計画: docs/superpowers/plans/2026-10-25-clause-findall4.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/clause_term.cpp src/builtins/database.cpp src/machine/internal.hpp tests/pl/clause.pl CMakeLists.txt docs/iso-notes.md docs/STATUS.md
git commit -m "[phase3] clause/2 と current_predicate/1 を追加する（公開の利用者述語が通る）"
```

`database_clause.cpp` を作ったときは、同じコミットに含める。

---

### Task 2: findall/4 の差分リスト

**Files:**
- Modify: `src/builtins/solutions.cpp`、`tests/pl/solutions.pl`、`docs/iso-notes.md`、`docs/STATUS.md`
- Test: `tests/pl/solutions.pl`

**Interfaces:**
- Consumes: タスク 1 のあとにある `run_solutions` と、今の `findall/3` の収集。収集を二つに分けない
- Produces: `findall/4`。`findall/3` は末尾 `[]` として同じ手続きを呼ぶ

`src/builtins/solutions.cpp` は 712 行ある。このタスクで 800 行を超えるなら、収集を `src/builtins/solutions_find.cpp` へ移してから `findall/4` を足す。

- [ ] **Step 1: 失敗するテストを書く**

`tests/pl/solutions.pl` の末尾に足す。既存の期待値は変えない。

```prolog
test(findall4_closed, findall(X, (X = 1; X = 2), L, []), solutions([L = [1, 2]])).
test(findall4_none, (findall(X, fail, L, T), L == T, var(T)), true).
test(findall4_open, (findall(X, (X = 1; X = 2), L, T), L == [1, 2|T], var(T)), true).
test(findall4_throw, findall(X, throw(error(instantiation_error, _)), L, T), error(instantiation_error)).
test(findall4_var_goal, findall(X, Y, L, T), error(instantiation_error)).
test(findall3_still, findall(X, (X = 1; X = 2), S), solutions([S = [1, 2]])).
```

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure -R '^prolog_tests$'`

Expected: `prolog_tests` が `findall/4` の未定義で失敗する。

- [ ] **Step 3: 末尾を受け取って同じ収集を使う**

第1引数から第3引数の検査は `findall/3` と同じ順である。第4引数は検査しない。解が無ければ第3引数と第4引数を単一化する。解があるときは、末尾が第4引数であるリストを第3引数と単一化する。ゴールが例外を投げたら、どちらも束縛しない。`findall/3` は第4引数を `[]` としてこの手続きを呼ぶ。

`docs/iso-notes.md` の未決 3 を、差分リストであるという決定の文に変える。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: 全部成功。版は `mosa 0.0.1 (phase 2)`。

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: findall/4 は差分リスト。解が無ければ List と Tail が同じ
次: ストリームの ADR
未解決: 未決 2、4、7–16。準拠スイートはライセンスが無くタスク 13 に進まない
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/solutions.cpp tests/pl/solutions.pl docs/iso-notes.md docs/STATUS.md
git commit -m "[phase3] findall/4 を差分リストにする（末尾が通る）"
```

`solutions_find.cpp` を作ったときは、`CMakeLists.txt` と一緒に同じコミットへ入れる。
