# フェーズ2 実行の核 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 継続渡しの中間表現をトランポリンで実行し、`nrev` と `queens`、段階 A と算術の Examples を通す。

**Architecture:** 節は `IrTerm` にコンパイルし、ゴール枠をヒープに置いてループで呼び出す。選択点と `catch` は同じスタックに積む。トレイルは機械が持ち、ユニフィケーションの成功後も残す。`consult` とプロンプトは、この機械の `load_program` と `query` の上に載せる。

**Tech Stack:** C++20、CMake 3.25 以上、CTest、標準ライブラリのみ。

**Spec:** `docs/superpowers/specs/2026-10-24-phase2-engine-design.md`

各タスクは 1 コミットで終わり、そのコミットがセッションの区切りである。次のタスクは、前のタスクの確認コマンドが通ってから始める。実装中にこの計画と設計文書を書き換えない。ずれに気づいたら `docs/STATUS.md` の未解決に 1 行書いて止まり、利用者に聞く。

## Global Constraints

- 言語は C++20。`CMAKE_CXX_EXTENSIONS` は OFF。コンパイラ拡張、`__int128`、`#pragma once` 以外の独自プラグマは使わない
- 依存は C++ 標準ライブラリと CMake だけ。新しいライブラリは追加しない
- 警告は `-Wall -Wextra -Wpedantic -Werror`。MSVC は `/W4 /WX /utf-8 /permissive- /Zc:__cplusplus`
- 生の `new` と `delete`、`reinterpret_cast`、C 配列、マクロは使わない。`#pragma once` は使ってよい
- 1 関数は 60 行以内、1 ファイルは 800 行以内。超える前に、そのタスクの Files に書いた分割へ移す
- 識別子は英語。コメント、コミット、文書は日本語。コメントは理由だけを書く。文字コードは UTF-8、BOM なし
- Prolog の失敗と `throw/1` は機械の状態で表す。C++ の例外は `mosa::ResourceLimit` だけである
- `#ifdef` による OS 依存は `src/platform/` 以外に書かない。フェーズ2で置いてよいのは `src/platform/terminal.cpp` だけである
- Cell のタグとビット配置は ADR 0001 と ADR 0002 のままである
- 中間表現は ADR 0003 を書き直すまで変えない。タスク 1 より前に `src/machine/` の実装を置かない
- 版文字列はタスク 14 の完了コミットまで `mosa 0.0.1 (phase 1)` のままである
- コミットメッセージは `[phase2] ` で始まる日本語 1 行とし、何を確認したかを含める
- 各タスクの終わりに `cmake --build build && ctest --test-dir build` を通し、`docs/STATUS.md` をそのタスクに書いた文へ置き換える。15 行を超えない
- テストを弱めない、飛ばさない、期待値を実装に合わせない
- `tests/cpp/harness.hpp` は変更しない
- エラーの Formal はヘルパで作る。呼び出し側にアトム名の文字列を直書きしない。ヘルパの引数の `string_view` は許す
- 既定のユニフィケーションは出現検査をしない。`X = f(X)` は成功したままにする

## Review Focus

設計の成功条件は `nrev` と `queens` と Examples である。次の 5 つは、その成功だけを見ていると実装が通っても利用者の入力で壊れる。各行のテストは、書いたタスクの Step 1 に入っている。

- 標準入力が端末でないときの `mosa p.pl -g 'p(X).'` は、`-t halt` が無くても最初の解だけ出して終了コード 0 で終わる。`;` を待たない
- `X = [X]` の答えは `X = [X].` であり、書き出しは戻ってこないまま止まらない
- `(call(!), fail ; true)` は成功する。`call` の中のカットは、外側の `;` を捨てない
- 同じファイルを二度 `consult` しても、述語の解は倍にならない
- `X is -5 // 2` は `-2`、`X is div(-5, 2)` は `-3` である

## ファイルの責任

| パス | 責任 | タスク |
| --- | --- | --- |
| `docs/adr/0003-execution-ir.md` | 継続渡し、ゴール枠、選択点、トレイルの決定 | 1 |
| `include/mosa/ir.hpp` | `IrTerm` と `IrTag` | 2 |
| `include/mosa/machine.hpp`、`src/machine/engine.cpp` | ゴール枠、ループ、`query` と `next` | 2 |
| `src/machine/load.cpp` | ソース文字列から節を積む | 2 |
| `include/mosa/unify.hpp`、`src/core/unify.cpp` | 機械のトレイルを使う `unify` | 2 |
| `src/machine/choice.cpp` | 複数節、`;/2`、やり直し | 3 |
| `src/machine/cut.cpp` | `!/0` と `->/2` | 4 |
| `src/machine/catch.cpp` | `catch/3` と `throw/1` | 5 |
| `src/machine/index.cpp` | 第1引数の候補 | 6 |
| `include/mosa/errors.hpp`、`src/builtins/errors.cpp` | `error/2` のヘルパ | 7 |
| `src/builtins/types.cpp` | 型検査、`=/2`、出現検査、`is_list/1` | 7 |
| `src/builtins/order.cpp` | 標準順序と `compare/3` | 8 |
| `src/builtins/terms.cpp` | `functor/3`、`arg/3`、`=../2`、`copy_term/2`、`term_variables/2` | 9 |
| `src/builtins/control.cpp` | `call/1..8`、`\+/1`、`once/1`、`repeat/0`、`halt/0,1`、`*->/2` | 10 |
| `src/builtins/arith.cpp`、`src/builtins/arith_float.cpp` | `is/2`、比較、評価関数 | 11 |
| `src/machine/answer.cpp` | 答えの文字列。循環は名前で止める | 12 |
| `src/machine/consult.cpp` | ファイル読み、置き換え、`op`、離れた節 | 12 |
| `include/mosa/repl.hpp`、`src/machine/session.cpp` | 起動引数、プロンプト、終了コード | 13 |
| `include/mosa/terminal.hpp`、`src/platform/terminal.cpp` | 標準入力が端末か | 13 |
| `include/mosa/mosa.hpp` | `create`、`consult`、`query`、`next`、`halt` | 13 |
| `include/mosa/suite.hpp`、`src/machine/suite.cpp` | `test/3` の実行 | 13 |
| `src/main_mosa.cpp`、`src/main_mosatest.cpp` | 上記を呼ぶだけ | 13 |
| `tests/pl/*.pl` | Examples、`nrev`、`queens`。`syntax.pl` は往復のまま | 14 |
| `src/version.cpp` | タスク 14 の完了時だけ `phase 2` | 14 |

`src/machine/engine.cpp` が 800 行を超えそうなら、そのタスクの中で `choice.cpp`、`cut.cpp`、`catch.cpp` へ先に分割する。公開する型は `machine.hpp` にだけ置く。

## ビルドの足し方

`add_library(mosa_lib STATIC ...)` は残す。各タスクは `enable_testing()` より前に `target_sources` を足し、より後にテスト実行ファイルを足す。既存のテストは消さない。

```cmake
target_sources(mosa_lib PRIVATE src/machine/engine.cpp src/machine/load.cpp)

add_executable(machine_test tests/cpp/machine_test.cpp)
target_include_directories(machine_test PRIVATE tests/cpp)
target_link_libraries(machine_test PRIVATE mosa_lib)
add_test(NAME machine COMMAND $<TARGET_FILE:machine_test>)
```

テストの `main` は、既存の C++ テストと同じく、各 `check_*` を呼んで `return mosa_test::failures();` とする。

## 共通の型

タスク 2 が作り、後のタスクは名前を変えない。

```cpp
enum class IrTag {
  Var, Anon, Atom, Int, Float, List, Fun,
  True, Fail, Cut, Call, And, Or, IfThen, SoftIf, Catch, Throw, Halt
};

struct IrTerm {
  IrTag tag = IrTag::True;
  std::uint16_t slot = 0;
  std::uint32_t atom = 0;
  std::uint16_t arity = 0;
  std::int64_t integer = 0;
  double floating = 0;
  std::vector<IrTerm> args;
};

enum class StopKind { Success, Failure, Exception, Halt };

struct Binding {
  std::string name;
  Cell variable;
};

struct Stop {
  StopKind kind = StopKind::Failure;
  int halt_code = 0;
  Cell exception{};
  std::vector<Binding> bindings;
  bool more = false;
};

struct LoadStatus {
  bool ok = false;
  std::string message;
};

struct Trail {
  std::vector<std::uint64_t*> slots;
};

bool unify(Heap& heap, Trail& trail, std::size_t trail_mark, std::size_t heap_mark, Cell left, Cell right);

class Machine {
 public:
  [[nodiscard]] static std::unique_ptr<Machine> create(std::size_t heap_limit = std::size_t{1} << 20);
  [[nodiscard]] AtomTable& atoms();
  [[nodiscard]] Heap& heap();
  [[nodiscard]] OperatorTable& operators();
  [[nodiscard]] LoadStatus load_program(std::string_view source);
  [[nodiscard]] Stop query(std::string_view goal_text);
  [[nodiscard]] Stop next();
  void halt(int code);
  [[nodiscard]] std::size_t choice_depth() const;
  [[nodiscard]] std::string_view diagnostic() const;
  [[nodiscard]] bool consult(std::string_view path);
};
```

`consult` と `diagnostic` の中身はタスク 12 まで、`consult` は `ok == false` と空の診断を返す。`choice_depth` はタスク 3 まで 0 である。`halt` はタスク 10 まで、呼ばれたら `StopKind::Halt` を次の `query` が返すだけで、プロセスは終わらせない。

ゴール枠はヒープの語で、項ではない。語 0 が述語のアトム番号、語 1 がアリティ、語 2 がカット境界、語 3 が継続枠の語番号で、継続が無いときは `UINT64_MAX`、語 4 から引数セルがアリティ個である。

選択点と `catch` は一つのスタックに積む。`choice_depth()` はそのスタックの要素数である。カットは、境界より上の要素を捨てる。

`unify` は、失敗したとき `trail.slots` を `trail_mark` まで戻してその分の束縛を外し、ヒープを `heap_mark` まで戻す。成功したときは両方を残す。

---

### Task 1: ADR 0003

**Files:**
- Create: `docs/adr/0003-execution-ir.md`
- Modify: `docs/STATUS.md`

**Interfaces:**
- Consumes: 設計文書の「アプローチ」と実装者への決定 3、4、5
- Produces: 承認済みの ADR。後のタスクはこの文面を実装する

- [ ] **Step 1: ADR が無いことを確認する**

Run: `test ! -f docs/adr/0003-execution-ir.md`
Expected: 終了コード 0

- [ ] **Step 2: `docs/adr/0003-execution-ir.md` を次の文面で作る**

```markdown
状態: 承認済み（2026-10-24）。承認はフェーズ2設計の承認である。

## 背景

インタプリタと mosac は同じ中間表現を実行する。
フェーズ6まで表現を変えないために、実行の前に固定する。
ゴール枠は項ではない。

## 決定

節は継続を最後に持つ中間表現にし、トランポリンで実行する。
ゴール枠はヒープ上の非項で、述語、アリティ、カット境界、継続、引数を持つ。
選択点と catch は一つのスタックに積み、トレイルは成功後も残す。

## 捨てた案

節の項を実行のたびに歩く形にはしない。
WAM の環境スタックと命令列にはしない。

## 影響

フェーズ7の GC はゴール枠を印の起点に含め、項としては走査しない。
この表現を変えるときは新しい ADR を書き、浩さんの承認を得る。
実装の詳細は docs/superpowers/specs/2026-10-24-phase2-engine-design.md に従う。
```

- [ ] **Step 3: 確認する**

Run: `test -f docs/adr/0003-execution-ir.md`
Expected: 終了コード 0。このタスクでは `ctest` の期待は変えない

- [ ] **Step 4: STATUS を次で置き換える**

```markdown
フェーズ: 2
進行中: なし
完了: ADR 0003 を承認済みで置いた
次: 事実 1 つの実行ループ
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 5: Commit**

```bash
git add docs/adr/0003-execution-ir.md docs/STATUS.md
git commit -m "[phase2] 実行の中間表現を ADR 0003 に固定する"
```

---

### Task 2: 事実 1 つとトレイル

**Files:**
- Create: `include/mosa/ir.hpp`、`include/mosa/machine.hpp`、`src/machine/engine.cpp`、`src/machine/load.cpp`、`tests/cpp/machine_test.cpp`
- Modify: `include/mosa/unify.hpp`、`src/core/unify.cpp`、`tests/cpp/unify_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: `Heap`、`AtomTable`、`OperatorTable`、`Reader`、ADR 0003
- Produces: 共通の型の節に書いた `Machine`、`unify`、`IrTerm`。`load_program` は事実だけを受け、規則と複数節は拒否する

- [ ] **Step 1: 失敗するテストを `tests/cpp/machine_test.cpp` に書く**

`unify_test.cpp` の呼び出しは、このタスクで次の形に変える。期待値は変えない。ローカルな `Trail trail` を作り、`unify(heap, trail, trail.slots.size(), heap.size(), 左, 右)` を呼ぶ。

```cpp
void check_one_fact() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(a).").ok, "load fact");
  auto yes = machine->query("p(a).");
  mosa_test::check(yes.kind == mosa::StopKind::Success && !yes.more, "fact hits");
  auto no = machine->query("p(b).");
  mosa_test::check(no.kind == mosa::StopKind::Failure, "fact misses");
  auto bound = machine->query("p(X).");
  mosa_test::check(bound.kind == mosa::StopKind::Success && bound.bindings.size() == 1, "fact binds");
  mosa_test::check(bound.bindings[0].name == "X", "name X");
  const auto text = mosa::write_term(machine->heap(), machine->operators(),
                                      machine->heap().deref(bound.bindings[0].variable),
                                      mosa::WriteOptions{}, {});
  mosa_test::check(text == "a", "value a");
  mosa_test::check(!machine->load_program("p(c).").ok, "second clause waits");
  mosa_test::check(!machine->load_program("q(X) :- q(X).").ok, "rule waits");
}
```

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build --target machine_test`
Expected: `Machine` が無く、コンパイルに失敗する

- [ ] **Step 3: 実装する**

`unify` の失敗はマークまで戻し、成功はトレイルを残す。既存の `unify_test` が通ることを、この意味の確認にする。

`load_program` はリーダで項を読む。事実だけを受ける。`:-` で始まる項、`-->` の項、同じ述語の 2 節目は `ok == false` にする。メッセージは空でよい。クエリは 1 項を読み、その述語のただ一つの節とヘッドを単一化し、成功なら継続が無いので `Success` を返す。変数の束縛は、リーダの名前表の順で `Binding` にする。`variable` は名前の付いた REF そのもので、表示するときは呼び出し側が `deref` する。節が 1 つなので `more` は false である。`consult` は `false` を返し、`diagnostic()` は空、`choice_depth()` は 0 とする。一致しなければ、そのクエリで積んだトレイルとヒープを戻して `Failure` を返す。

- [ ] **Step 4: テストが通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 既存のテストと `machine` が通る

- [ ] **Step 5: STATUS を次で置き換える**

```markdown
フェーズ: 2
進行中: なし
完了: 事実 1 つが実行できる。トレイルは成功後も残る
次: 規則と選択点
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add include/mosa/ir.hpp include/mosa/machine.hpp include/mosa/unify.hpp src/core/unify.cpp src/machine/engine.cpp src/machine/load.cpp tests/cpp/machine_test.cpp tests/cpp/unify_test.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] 事実 1 つをトランポリンで実行する（machine テストが通る）"
```

---

### Task 3: 規則、連言、選択点、nrev

**Files:**
- Create: `src/machine/choice.cpp`
- Modify: `src/machine/engine.cpp`、`src/machine/load.cpp`、`tests/cpp/machine_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: タスク 2 の `Machine`
- Produces: 複数節、`,/2`、`;/2`、`true/0`、`fail/0`。`next()` が次の解を返す

- [ ] **Step 1: 失敗するテストを足す**

```cpp
void check_choice_and_nrev() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(1). p(2).").ok, "two facts");
  auto first = machine->query("p(X).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more, "first choice");
  auto second = machine->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && !second.more, "second choice");
  mosa_test::check(machine->load_program(
      "nrev([], []). nrev([H|T], R) :- nrev(T, S), append(S, [H], R). "
      "append([], L, L). append([H|T], L, [H|R]) :- append(T, L, R).").ok,
                   "load nrev");
  auto rev = machine->query("nrev([1, 2, 3], R).");
  mosa_test::check(rev.kind == mosa::StopKind::Success && rev.bindings.size() == 1 &&
                       rev.bindings[0].name == "R",
                   "nrev binds");
  const auto text = mosa::write_term(machine->heap(), machine->operators(),
                                      machine->heap().deref(rev.bindings[0].variable),
                                      mosa::WriteOptions{}, {});
  mosa_test::check(text == "[3, 2, 1]", "nrev text");
}
```

`;/2` は `query("(true ; fail).")` が `Success` で `more == true`、`next()` が `Failure` であることも同じ関数で確認する。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ./build/machine_test`
Expected: `check_choice_and_nrev` が FAIL する。終了コードは 0 以外

- [ ] **Step 3: 実装する**

同じ述語の連続した節を蓄積する。タスク 2 の「2 節目を拒否」は外す。残りの節があるとき選択点を作る。節が 1 つなら作らない。やり直しはヒープとトレイルを選択点の水位まで戻し、次の節を試す。残りが無く、選択点も無ければ `Failure` を返す。

本体は `,/2` を継続に编译する。`true/0` は成功、`fail/0` と `false/0` は失敗とする。`;/2` は左右を選択点で分ける。ヘッドと本体の変数は、節に入るときにスロットへ新鮮な REF を割り当て、ヘッドの単一化で埋める。

`nrev` のリストはリーダが作る通常のリストである。実行が再帰しても C++ の呼び出しはトランポリンのループに留める。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: 規則と選択点が動き、nrev([1,2,3], R) が [3, 2, 1] になる
次: カットと ->/2
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add src/machine/choice.cpp src/machine/engine.cpp src/machine/load.cpp tests/cpp/machine_test.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] 規則と選択点を追加し nrev が解ける（machine テストが通る）"
```

---

### Task 4: カットと ->/2

**Files:**
- Create: `src/machine/cut.cpp`
- Modify: `src/machine/engine.cpp`、`src/machine/load.cpp`、`tests/cpp/machine_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: タスク 3 の選択点スタック
- Produces: `!/0` と `->/2`。`;` の左が `->` のときは else を条件の成否で選ぶ

- [ ] **Step 1: 失敗するテストを足す**

```cpp
void check_cut() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p :- !, fail. p.").ok, "load cut");
  mosa_test::check(machine->query("p.").kind == mosa::StopKind::Failure, "cut drops second clause");
  mosa_test::check(machine->load_program("q(X) :- (X = 1 ; X = 2), !.").ok, "load local cut");
  auto once = machine->query("q(X).");
  const auto once_text = mosa::write_term(machine->heap(), machine->operators(),
                                           machine->heap().deref(once.bindings[0].variable),
                                           mosa::WriteOptions{}, {});
  mosa_test::check(once.kind == mosa::StopKind::Success && !once.more && once_text == "1",
                   "cut drops sibling");
  mosa_test::check(machine->load_program("r(X) :- true -> X = 1 ; X = 2. s(X) :- fail -> X = 1 ; X = 2.").ok,
                   "load if then");
  auto then = machine->query("r(X).");
  const auto then_text = mosa::write_term(machine->heap(), machine->operators(),
                                           machine->heap().deref(then.bindings[0].variable),
                                           mosa::WriteOptions{}, {});
  mosa_test::check(then.kind == mosa::StopKind::Success && !then.more && then_text == "1", "then commits");
  auto otherwise = machine->query("s(X).");
  const auto else_text = mosa::write_term(machine->heap(), machine->operators(),
                                           machine->heap().deref(otherwise.bindings[0].variable),
                                           mosa::WriteOptions{}, {});
  mosa_test::check(otherwise.kind == mosa::StopKind::Success && !otherwise.more && else_text == "2",
                   "else runs");
}
```

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ./build/machine_test`
Expected: `check_cut` が FAIL する

- [ ] **Step 3: 実装する**

述語に入ったときの `choice_depth()` をゴール枠のカット境界にする。`!/0` はその境界より新しいスタック要素を捨てる。`->/2` は、条件が成功したとき条件の中でできた選択点を、その条件に入った高さまで捨ててから then を実行する。`;/2` の左が `->/2` で、条件が失敗したときは else を実行する。単独の `->/2` は、条件が失敗したら失敗する。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: カットと ->/2 が選択点を捨てる
次: catch/throw
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add src/machine/cut.cpp src/machine/engine.cpp src/machine/load.cpp tests/cpp/machine_test.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] カットと ->/2 を追加する（machine テストが通る）"
```

---

### Task 5: catch/throw

**Files:**
- Create: `src/machine/catch.cpp`
- Modify: `src/machine/engine.cpp`、`src/machine/load.cpp`、`tests/cpp/machine_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: タスク 4 のスタック
- Produces: `catch/3` と `throw/1`。ボールは戻す前に複製する。キャッチが無ければ `StopKind::Exception`

- [ ] **Step 1: 失敗するテストを足す**

```cpp
void check_catch() {
  auto machine = mosa::Machine::create();
  auto caught = machine->query("catch(throw(a), a, true).");
  mosa_test::check(caught.kind == mosa::StopKind::Success, "catch same ball");
  auto missed = machine->query("catch(throw(b), a, true).");
  mosa_test::check(missed.kind == mosa::StopKind::Exception, "catch misses");
  auto tiny = mosa::Machine::create(8);
  auto boom = tiny->query("catch(p(a), _, true).");
  mosa_test::check(boom.kind != mosa::StopKind::Success, "resource is not a prolog ball");
}
```

`Machine::create(8)` が `ResourceLimit` を C++ 例外で出すなら、テストはそれを捕捉し、Prolog の `Exception` に変換されていないことを確認する。変換して `Exception` にすることは、このタスクではしない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ./build/machine_test`
Expected: `check_catch` が FAIL する

- [ ] **Step 3: 実装する**

`catch(Goal, Pattern, Recovery)` はスタックにキャッチ枠を積んでから Goal を、その時点の深さをカット境界にして実行する。`throw` はボールをヒープ上に複製してから、パターンと単一化できるキャッチまで戻す。複製なので、戻したあともボールの中身が残る。単一化できなければ、さらに外へ進む。キャッチが無ければ `Exception` で、`exception` は投げられた項である。`ResourceLimit` は捕捉して Prolog のボールにしない。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: catch/throw がボールを複製して戻す
次: 第1引数のインデクシング
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add src/machine/catch.cpp src/machine/engine.cpp src/machine/load.cpp tests/cpp/machine_test.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] catch/throw を追加する（machine テストが通る）"
```

---

### Task 6: 第1引数のインデクシング

**Files:**
- Create: `src/machine/index.cpp`
- Modify: `src/machine/engine.cpp`、`tests/cpp/machine_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: タスク 3 の節の列
- Produces: 第1引数が候補を 1 つに絞るとき `more == false`

- [ ] **Step 1: 失敗するテストを足す**

```cpp
void check_index() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(a). p(b).").ok, "index load");
  auto one = machine->query("p(a).");
  mosa_test::check(one.kind == mosa::StopKind::Success && !one.more, "single candidate");
  auto any = machine->query("p(X).");
  mosa_test::check(any.kind == mosa::StopKind::Success && any.more, "variable tries all");
  mosa_test::check(machine->load_program("q(a). q(X) :- X = c.").ok, "var head kept");
  auto q = machine->query("q(a).");
  mosa_test::check(q.kind == mosa::StopKind::Success && q.more, "variable head remains");
}
```

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ./build/machine_test`
Expected: `check_index` の `single candidate` が FAIL する。インデクシング前は 2 節あるので `more == true` になる

- [ ] **Step 3: 実装する**

第1引数が変数のときは全節を候補にする。定数、リスト、関手のときは、同じ第1引数を持つ節と、第1引数が変数の節だけを候補にする。候補が 1 つなら選択点を作らない。候補が 0 なら失敗する。迷う節は候補に残す。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功。タスク 3 の `p(1). p(2).` は第1引数が変数なので、いままで通り `more == true` である

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: 第1引数の候補が 1 つのとき選択点を作らない
次: 型検査と単一化の組込述語
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add src/machine/index.cpp src/machine/engine.cpp tests/cpp/machine_test.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] 第1引数のインデクシングを追加する（machine テストが通る）"
```

---

### Task 7: 型検査と単一化

**Files:**
- Create: `include/mosa/errors.hpp`、`src/builtins/errors.cpp`、`src/builtins/types.cpp`、`tests/pl/types.pl`、`tests/pl/unify.pl`
- Modify: `src/machine/engine.cpp`、`tests/cpp/machine_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: `Machine::query`
- Produces: `make_instantiation_error`、`make_type_error`、`make_domain_error`、`make_existence_error`、`make_evaluation_error`、`throw_error`。型検査の述語と `=/2`、`\=/2`、`unify_with_occurs_check/2`、`is_list/1`

ヘルパのシグネチャは次のとおりである。

```cpp
Cell make_instantiation_error(Machine& machine);
Cell make_type_error(Machine& machine, std::string_view type, Cell culprit);
Cell make_domain_error(Machine& machine, std::string_view domain, Cell culprit);
Cell make_existence_error(Machine& machine, std::string_view kind, Cell culprit);
Cell make_evaluation_error(Machine& machine, std::string_view which);
Cell throw_error(Machine& machine, Cell formal, std::uint32_t name, std::uint16_t arity);
```

`throw_error` は `error(Formal, context(Name/Arity, _))` を作り、実行を `Exception` にする。未定義述語は `existence_error(procedure, 名前/アリティ)` をこのヘルパで投げる。

- [ ] **Step 1: 失敗するテストを足す**

```cpp
void check_types() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("var(X).").kind == mosa::StopKind::Success, "var");
  mosa_test::check(machine->query("var(a).").kind == mosa::StopKind::Failure, "not var");
  mosa_test::check(machine->query("atom([]).").kind == mosa::StopKind::Success, "nil atom");
  mosa_test::check(machine->query("integer(1.0).").kind == mosa::StopKind::Failure, "float is not integer");
  mosa_test::check(machine->query("compound([]).").kind == mosa::StopKind::Failure, "nil not compound");
  mosa_test::check(machine->query("callable(1).").kind == mosa::StopKind::Exception, "callable number");
  mosa_test::check(machine->query("is_list([a]).").kind == mosa::StopKind::Success, "proper list");
  mosa_test::check(machine->query("is_list([a|b]).").kind == mosa::StopKind::Failure, "partial list");
  mosa_test::check(machine->query("is_list(X).").kind == mosa::StopKind::Failure, "unbound list");
  auto cycle = machine->query("X = [X], is_list(X).");
  mosa_test::check(cycle.kind == mosa::StopKind::Failure, "cyclic list");
  mosa_test::check(machine->query("(X = f(X), unify_with_occurs_check(Y, f(Y))).").kind ==
                       mosa::StopKind::Failure,
                   "occurs check fails");
  mosa_test::check(machine->query("missing(1).").kind == mosa::StopKind::Exception, "unknown predicate");
}
```

`tests/pl/types.pl` と `tests/pl/unify.pl` には、13211-1 の 8.2 と 8.3 の Examples を `test(名前, ゴール, 期待).` で 1 例 1 事実ずつ書く。このタスクではまだ `mosatest` は実行しない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ./build/machine_test`
Expected: `check_types` が FAIL する

- [ ] **Step 3: 実装する**

`var/1`、`nonvar/1`、`atom/1`、`integer/1`、`float/1`、`atomic/1`、`compound/1`、`number/1`、`callable/1`、`is_list/1`、`=/2`、`\=/2`、`unify_with_occurs_check/2` をゴールとして実行する。`callable/1` は、変数なら `instantiation_error`、数なら `type_error(callable, 値)` を投げる。`is_list/1` は末尾が `[]` の有限リストだけを成功させ、未束縛と循環は失敗させる。出現検査付き単一化は、循環になる束縛を失敗させ、束縛を残さない。`=/2` は出現検査をしない。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: 型検査と単一化の組込述語がゴールとして動く
次: 標準順序
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add include/mosa/errors.hpp src/builtins/errors.cpp src/builtins/types.cpp src/machine/engine.cpp tests/cpp/machine_test.cpp tests/pl/types.pl tests/pl/unify.pl CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] 型検査と単一化の組込述語を追加する（machine テストが通る）"
```

---

### Task 8: 標準順序

**Files:**
- Create: `src/builtins/order.cpp`、`tests/pl/order.pl`
- Modify: `src/machine/engine.cpp`、`tests/cpp/machine_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: タスク 7 のエラーヘルパ
- Produces: `@</2`、`@=</2`、`@>/2`、`@>=/2`、`==/2`、`\==/2`、`compare/3`

- [ ] **Step 1: 失敗するテストを足す**

```cpp
void check_order() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("(1.0 @< 1).").kind == mosa::StopKind::Success, "float before integer");
  mosa_test::check(machine->query("(1 == 1.0).").kind == mosa::StopKind::Failure, "term equality is not numeric");
  mosa_test::check(machine->query("(1.0 =:= 1).").kind == mosa::StopKind::Failure, "arith compare waits");
  mosa_test::check(machine->query("compare(=, a, a).").kind == mosa::StopKind::Success, "compare same");
  mosa_test::check(machine->query("(a @< b).").kind == mosa::StopKind::Success, "atom order");
}
```

`=:=` はタスク 11 まで未定義で、`Exception` でも `Failure` でもこのタスクの失敗条件にはしない。上の 3 本目は書かない。代わりに `(0.0 @< -0.0)` が失敗すること、つまり両者が等しいことを確認する。`tests/pl/order.pl` には 8.4 の Examples を `test/3` で書く。変数どうしの大小は期待値にしない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ./build/machine_test`
Expected: `check_order` が FAIL する

- [ ] **Step 3: 実装する**

標準順序は、変数、浮動小数、整数、アトム、複合項の順である。変数どうしはヒープ番地の若いほうを先にする。浮動小数どうしと整数どうしは数値で比べ、`-0.0` と `0.0` は等しい。`NaN` はどの浮動小数より後ろである。アトムは UTF-8 バイト列の辞書順である。複合項はアリティ、関手名、引数の順で比べる。循環に再訪したら、その対の中ではそこで打ち切り、テストは止まることだけを別の 1 本で見る。`==/2` は単一化しない。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: 標準順序が動き、1.0 @< 1 が成功する
次: functor/arg/=../copy_term/term_variables
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/order.cpp src/machine/engine.cpp tests/cpp/machine_test.cpp tests/pl/order.pl CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] 標準順序を追加する（1.0 @< 1 が通る）"
```

---

### Task 9: 項の分解

**Files:**
- Create: `src/builtins/terms.cpp`、`tests/pl/terms.pl`
- Modify: `src/machine/engine.cpp`、`tests/cpp/machine_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: タスク 7 のヘルパとタスク 8 の順序
- Produces: `functor/3`、`arg/3`、`=../2`、`copy_term/2`、`term_variables/2`

- [ ] **Step 1: 失敗するテストを足す**

```cpp
void check_terms() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("functor(f(a, b), f, 2).").kind == mosa::StopKind::Success, "functor");
  mosa_test::check(machine->query("arg(1, f(a, b), a).").kind == mosa::StopKind::Success, "arg");
  mosa_test::check(machine->query("(f(a, b) =.. [f, a, b]).").kind == mosa::StopKind::Success, "univ");
  mosa_test::check(machine->query("(X = f(A), copy_term(X, Y), A = 1, var(B)).").kind ==
                       mosa::StopKind::Failure,
                   "copy shares no var");
  mosa_test::check(machine->query("term_variables(f(A, A, B), [A, B]).").kind == mosa::StopKind::Success,
                   "term variables");
}
```

`copy_term` のテストは、複製の変数が元の変数と単一化されていないことを見る。ゴールは `(copy_term(f(A), f(B)), A = 1, var(B))` とし、成功することを確認する。`tests/pl/terms.pl` には 8.5 の Examples を書く。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ./build/machine_test`
Expected: `check_terms` が FAIL する

- [ ] **Step 3: 実装する**

引数検査が終わる前に束縛しない。検査の順は、各述語の規格の Errors の列挙順である。`copy_term/2` は循環を保って複製する。`term_variables/2` は左から見たかたちの変数を一度だけリストにし、循環は一周で止める。`functor(X, f, N)` が新しい項を作るとき、新しい変数は未束縛の REF である。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: functor/arg/=../copy_term/term_variables が動く
次: call と制御の残り
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/terms.cpp src/machine/engine.cpp tests/cpp/machine_test.cpp tests/pl/terms.pl CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] 項の分解を追加する（machine テストが通る）"
```

---

### Task 10: 制御の残り

**Files:**
- Create: `src/builtins/control.cpp`、`tests/pl/control.pl`
- Modify: `src/machine/engine.cpp`、`tests/cpp/machine_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: タスク 4 のカット境界とタスク 5 のキャッチ
- Produces: `call/1` から `call/8`、`\+/1`、`once/1`、`repeat/0`、`halt/0`、`halt/1`、`*->/2`

- [ ] **Step 1: 失敗するテストを足す**

```cpp
void check_control() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("(call(!), fail ; true).").kind == mosa::StopKind::Success,
                   "cut stays inside call");
  mosa_test::check(machine->query("(\\+ fail).").kind == mosa::StopKind::Success, "not fail");
  mosa_test::check(machine->query("(\\+ true).").kind == mosa::StopKind::Failure, "not true");
  auto halted = machine->query("halt(3).");
  mosa_test::check(halted.kind == mosa::StopKind::Halt && halted.halt_code == 3, "halt code");
  mosa_test::check(machine->query("call(1).").kind == mosa::StopKind::Exception, "call number");
  mosa_test::check(machine->load_program(":- op(600, xfy, *->). p(1). p(2).").ok, "soft cut data");
  auto soft = machine->query("(p(X) *-> true ; X = 9).");
  mosa_test::check(soft.kind == mosa::StopKind::Success && soft.more, "soft cut keeps solutions");
}
```

`*->` はフェーズ1の既定演算子に無い。このタスクの `load_program` は、名前がアトム 1 つの `:- op(優先度, 指定子, 名前).` を `OperatorTable::define` に渡す。リスト名・診断の文・節の置き換えはタスク 12 まで足さない。テストは `:- op(600, xfy, *->).` を同じソースの先頭に置く。

`tests/pl/control.pl` には 7.8 と 8.15 の Examples を書く。`*->` は自前で 4 本置く。条件が複数回成功する、条件が失敗して else が動く、else が無く条件が失敗すると失敗する、条件のカットは `*->` の外へ出ない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ./build/machine_test`
Expected: `check_control` が FAIL する

- [ ] **Step 3: 実装する**

`call/N` は、ゴールが変数なら `instantiation_error`、呼び出し可能でなければ `type_error(callable, 値)` を投げる。呼ぶ直前の `choice_depth()` を、その中のカット境界にする。`\+/1` はゴールが成功したらその選択点を捨てて失敗し、失敗したら成功する。`once/1` は 1 回成功させ、中の選択点を捨てる。`repeat/0` は成功し、やり直しでもまた成功する。`halt/0` はコード 0、`halt/1` は整数のコードで `StopKind::Halt` を返す。プロセスは終わらせない。`*->/2` は条件の解を捨てない。条件が一度も成功せず、`;/2` の右があるときだけ、その右を実行する。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: call のカットは外へ出ない。halt は終了コードを残す
次: 算術
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/control.cpp src/machine/engine.cpp src/machine/load.cpp tests/cpp/machine_test.cpp tests/pl/control.pl CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] call と制御の残りを追加する（カットが外へ出ない）"
```

---

### Task 11: 算術

**Files:**
- Create: `src/builtins/arith.cpp`、`src/builtins/arith_float.cpp`、`tests/pl/arith.pl`
- Modify: `src/machine/engine.cpp`、`tests/cpp/machine_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: タスク 7 の `make_evaluation_error` と `make_type_error`
- Produces: `is/2`、`=:=/2`、`=\=/2`、`</2`、`=</2`、`>/2`、`>=/2`、および設計が繰り上げた評価関数

評価関数は、`+` `-` `*` `/` `//` `mod` `rem` `div` `abs` `sign` `min` `max` `gcd` `**` `^` `sqrt` `sin` `cos` `tan` `asin` `acos` `atan` `atan2` `exp` `log` `log2` `float` `integer` `float_integer_part` `float_fractional_part` `truncate` `round` `ceiling` `floor` `>>` `<<` `/\\` `\\/` `xor` `\\` `pi` `e` `inf` `nan` `epsilon` `max_tagged_integer` `msb` `copysign` `nexttoward` `succ_or_zero` である。ISO にあるものの引数の個数と誤差は第9章に従う。ISO に無いものは次の意味に固定する。

- `pi/0`、`e/0`、`inf/0`、`nan/0`、`epsilon/0` は、それぞれ円周率、自然対数の底、正の無限大、quiet NaN、`std::numeric_limits<double>::epsilon()` である
- `max_tagged_integer/0` は `2^60 - 1` である
- `log2/1` は 2 を底とする対数である
- `msb/1` は正の整数の最上位ビットの位置で、`msb(1)` は 0、`msb(2)` は 1 である。0 と負は `evaluation_error(undefined)` である
- `succ_or_zero/1` は、0 以上の整数ならその数に 1 を足し、負の整数なら 0 を返す
- `copysign/2` と `nexttoward/2` は C++ の同名の関数と同じ値を返す

規格を読んでも期待値が一つに決まらない計算は、実装せず `docs/iso-notes.md` に質問を 1 つ書いて、このタスクをそこで止める。

- [ ] **Step 1: 失敗するテストを足す**

```cpp
void check_arith() {
  auto quot_machine = mosa::Machine::create();
  auto quot = quot_machine->query("X is -5 // 2.");
  const auto q = mosa::write_term(quot_machine->heap(), quot_machine->operators(),
                                   quot_machine->heap().deref(quot.bindings[0].variable),
                                   mosa::WriteOptions{}, {});
  mosa_test::check(quot.kind == mosa::StopKind::Success && q == "-2", "toward zero is -2");
  auto floor_machine = mosa::Machine::create();
  auto floor = floor_machine->query("X is div(-5, 2).");
  const auto d = mosa::write_term(floor_machine->heap(), floor_machine->operators(),
                                   floor_machine->heap().deref(floor.bindings[0].variable),
                                   mosa::WriteOptions{}, {});
  mosa_test::check(floor.kind == mosa::StopKind::Success && d == "-3", "floor div is -3");
  auto errors = mosa::Machine::create();
  mosa_test::check(errors->query("X is 1 / 0.").kind == mosa::StopKind::Exception, "zero divisor");
  mosa_test::check(errors->query("X is 9223372036854775807 + 1.").kind == mosa::StopKind::Exception,
                   "int overflow");
  mosa_test::check(errors->query("(1.0 =:= 1).").kind == mosa::StopKind::Success, "numeric equal");
}
```

`tests/pl/arith.pl` には 8.6、8.7、第9章の Examples を `test/3` で書く。オーバーフローの例は、結果を浮動小数にしない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ./build/machine_test`
Expected: `check_arith` が FAIL する

- [ ] **Step 3: 実装する**

`//` と `rem` は 0 へ向かって丸める。`div` と `mod` は負の無限大へ向かって丸める。結果が `int64` に収まらないときは `evaluation_error(int_overflow)` とし、浮動小数にはしない。ゼロ除算は `evaluation_error(zero_divisor)` である。`sqrt(-1)` のような未定義は `evaluation_error(undefined)` である。浮動小数のオーバーフローは `evaluation_error(float_overflow)` である。片方が浮動小数なら、整数を浮動小数へ変換してから計算する。`/` の結果は浮動小数である。`is/2` は右を評価してから左と単一化する。比較は両辺を評価してから数値として比べる。

ファイルが 800 行を超える前に `arith_float.cpp` へ三角関数と指数対数を移す。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: 算術が動き、-5 // 2 は -2、div(-5, 2) は -3
次: 答えの表示と consult
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/arith.cpp src/builtins/arith_float.cpp src/machine/engine.cpp tests/cpp/machine_test.cpp tests/pl/arith.pl CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] 算術を追加する（-5 // 2 と div が通る）"
```

---

### Task 12: 答えの表示と consult

**Files:**
- Create: `src/machine/answer.cpp`、`src/machine/consult.cpp`
- Modify: `src/write/writer.cpp`、`src/machine/load.cpp`、`include/mosa/machine.hpp`、`tests/cpp/machine_test.cpp`、`tests/cpp/writer_test.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: タスク 11 までの `load_program` とライタ
- Produces: `consult(path)`、`diagnostic()`、`format_answer`。`format_answer` のシグネチャは `std::string format_answer(const Machine& machine, const Stop& stop)` とする

- [ ] **Step 1: 失敗するテストを足す**

```cpp
void check_answer_and_consult() {
  auto machine = mosa::Machine::create();
  auto cycle = machine->query("X = [X].");
  mosa_test::check(mosa::format_answer(*machine, cycle) == "X = [X]", "cyclic answer");
  auto alias = machine->query("X = Y.");
  mosa_test::check(mosa::format_answer(*machine, alias) == "X = Y", "alias once");
  auto both = machine->query("(X = Y, Y = 1).");
  mosa_test::check(mosa::format_answer(*machine, both) == "X = 1, Y = 1", "both ground");
  mosa_test::check(machine->load_program("p(1).").ok, "first p");
  mosa_test::check(machine->load_program("p(2).").ok, "replace p");
  auto replaced = machine->query("p(X).");
  const auto replaced_text = mosa::write_term(machine->heap(), machine->operators(),
                                               machine->heap().deref(replaced.bindings[0].variable),
                                               mosa::WriteOptions{}, {});
  mosa_test::check(replaced.kind == mosa::StopKind::Success && !replaced.more && replaced_text == "2",
                   "not doubled");
  auto broken = mosa::Machine::create();
  mosa_test::check(!broken->load_program("q(1). r(1). q(2).").ok, "discontiguous q");
}
```

往復の `writer_test` は期待値を変えない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ./build/machine_test`
Expected: `check_answer_and_consult` が FAIL する

- [ ] **Step 3: 実装する**

ライタは、名前表にある REF に束縛を追うより先に出会ったら、その名前を書く。名前の無い循環は、複合項の番地を覚え、再訪を `_` と書く。`format_answer` は、成功で名前が無いとき `true`、失敗のとき `false`、名前があるときは設計の決定 20 の規則で書く。末尾のピリオドは付けない。

`load_program` は、ファイルが定義する述語を、そのロードの最初の節で置き換える。選択点スタックが空でないときに既存の述語を置き換えようとしたら失敗し、診断は `実行中の述語は置き換えられない` とする。同じロードの中で節が離れたら失敗し、診断は `述語 名前/アリティ の節が離れている` とする。`:- op(優先度, 指定子, 名前).` は名前がアトムでもアトムのリストでも受け、`OperatorTable::define` に渡す。それ以外の `:-/1` と `?-/1` と `-->/2` は失敗し、診断は `この指示はフェーズ2では使えない: ` に項を続けたものとする。`consult` はパスをバイト列のまま開き、中身を `load_program` に渡す。開けないときは診断 `ファイルを開けません` で `false` を返す。診断は `diagnostic()` でも読める。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功。往復も含む

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: 答えの表示と consult の置き換えが動く
次: プロンプトと mosatest
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add src/machine/answer.cpp src/machine/consult.cpp src/write/writer.cpp src/machine/load.cpp include/mosa/machine.hpp tests/cpp/machine_test.cpp tests/cpp/writer_test.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] 答えの表示と consult を追加する（往復が通る）"
```

---

### Task 13: プロンプトと test/3

**Files:**
- Create: `include/mosa/repl.hpp`、`src/machine/session.cpp`、`include/mosa/terminal.hpp`、`src/platform/terminal.cpp`、`include/mosa/suite.hpp`、`src/machine/suite.cpp`、`tests/cpp/session_test.cpp`、`tests/pl/runner.pl`
- Modify: `include/mosa/mosa.hpp`、`src/main_mosa.cpp`、`src/main_mosatest.cpp`、`CMakeLists.txt`、`docs/STATUS.md`

**Interfaces:**
- Consumes: タスク 12 の `consult`、`query`、`format_answer`
- Produces: `SessionResult run_session(std::span<const std::string_view> args, std::string_view input, bool tty)` と `int run_suite(std::string_view path)`。`mosa.hpp` は `create`、`consult`、`query`、`next`、`halt` を公開する

- [ ] **Step 1: 失敗するテストを `tests/cpp/session_test.cpp` に書く**

```cpp
void check_batch_without_tty() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/session_p.pl";
  std::ofstream file(path, std::ios::binary);
  file << "p(1).\np(2).\n";
  file.close();
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "p(X).";
  std::vector<std::string_view> args{arg0, path, arg_g, goal};
  auto result = mosa::run_session(args, "", false);
  mosa_test::check(result.code == 0, "batch exit");
  mosa_test::check(result.out.find("X = 1.") != std::string::npos, "first solution");
  mosa_test::check(result.out.find("X = 2.") == std::string::npos, "does not print the rest");
}
```

`session_test` には `target_compile_definitions(session_test PRIVATE MOSA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")` を付ける。`tty == true` で同じ引数のときは、入力 `";\n"` を与えると `X = 2.` が出て、そのあと `?- ` に戻ることを別の `check` で確認する。`-t halt` を足したときはプロンプトへ戻らない。壊れたファイルは終了コード 1 で、標準エラーに `項が閉じていません` が出て、標準出力に `?- ` が無い。開けないパスは `ファイルを開けません` で終了コード 1 である。

`tests/pl/runner.pl` は次のとおりである。

```prolog
p(1).
p(2).
test(two, p(X), solutions([X = 1, X = 2])).
test(no, p(3), fail).
test(yes, true, true).
test(boom, throw(error(instantiation_error, _)), error(instantiation_error)).
```

`run_suite` がこのファイルで 0 を返すことを `session_test` で確認する。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build --target session_test`
Expected: コンパイルに失敗する

- [ ] **Step 3: 実装する**

`run_session` は設計の画面と、決定 21、22、23、24 に従う。端末でないときの `-g` は、`-t halt` の有無にかかわらず最初の解だけ出して終了する。`;` は読まない。成功は 0、失敗は 1、捕捉されない例外は 2 である。対話中の例外は `警告: ゴールは例外を送出しました: ` に項を続け、プロンプトへ戻る。構文エラーの理由は設計の決定 23 の日本語にする。使い方は `mosa: 使い方: mosa [ファイル...] [-g ゴール] [-t halt]` とする。`--version` は版だけを出して 0 で終わる。

`standard_input_is_terminal` は `src/platform/terminal.cpp` に置く。`main` は実入力とこの関数を `run_session` に渡す。

`run_suite` は項をリーダで分け、`test/3` をテストとして取り、ほかの節は機械へ入れる。各テストの前にヒープとトレイルと選択点を、読み終わった直後へ戻す。期待は `true`、`fail`、`error(Formal)`、`solutions([...])` である。Formal だけを単一化し、Context は見ない。ディレクトリを受けたら、直下の `*.pl` を名前順に実行し、`syntax.pl` は除く。`mosatest` の `--roundtrip` と `--version` は残す。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功。`roundtrip` も成功する

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2
進行中: なし
完了: プロンプトと test/3 が動く。端末でない -g は最初の解で終わる
次: Examples と queens
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add include/mosa/repl.hpp include/mosa/terminal.hpp include/mosa/suite.hpp include/mosa/mosa.hpp src/machine/session.cpp src/machine/suite.cpp src/platform/terminal.cpp src/main_mosa.cpp src/main_mosatest.cpp tests/cpp/session_test.cpp tests/pl/runner.pl CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] プロンプトと test/3 を追加する（端末でない -g が通る）"
```

---

### Task 14: Examples、nrev、queens、版

**Files:**
- Modify: `tests/pl/types.pl`、`tests/pl/unify.pl`、`tests/pl/order.pl`、`tests/pl/terms.pl`、`tests/pl/control.pl`、`tests/pl/arith.pl`、`CMakeLists.txt`、`src/version.cpp`、版を期待しているテスト、`docs/STATUS.md`
- Create: `tests/pl/nrev.pl`、`tests/pl/queens.pl`

**Interfaces:**
- Consumes: タスク 13 の `run_suite` と `mosatest`
- Produces: フェーズ2の完了条件。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 1: プログラムと実行を足す**

`tests/pl/nrev.pl` と `tests/pl/queens.pl` のプログラムは、設計の決定 30 と 31 と一字一句同じにする。それぞれに `test/3` を足す。

- `nrev([1, 2, 3], R)` の期待は `solutions([R = [3, 2, 1]])` である
- `queens(4, Qs)` の期待は `solutions([Qs = [2, 4, 1, 3]])` である。続きの解は期待に書かない
- `queens(8, Qs)` の最初の解が `Qs = [1, 5, 8, 6, 3, 7, 2, 4]` であることも、同じ `solutions` の先頭で確認する。ランナーが全解を求める実装なら、このテストは最初の解だけを比べる形に `run_suite` を変えない。`queens(8)` は全解を列挙すると遅いので、`test` の期待は最初の 1 解で成功とし、続きは比べない。ランナーは `solutions` のリストの長さだけ解を求め、その後の解があっても失敗にしない

タスク 7 から 11 で書いた Examples に抜けがある述語は、ここで規格の例を足す。期待値は規格のままにする。

`CMakeLists.txt` に次を足す。

```cmake
add_test(NAME prolog_tests COMMAND $<TARGET_FILE:mosatest> ${CMAKE_SOURCE_DIR}/tests/pl)
```

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が、未記入の Examples か版の期待値で失敗する。版を変える前に、Examples の失敗を 0 にする

- [ ] **Step 3: 版を変える**

Examples と `nrev` と `queens` と往復が通ってから、`src/version.cpp` の版を `mosa 0.0.1 (phase 2)` にし、版を期待している既存テストの文字列も同じにする。それより前のコミットでは版テストを壊さない。このタスクのコミットは、通ったあとの 1 回だけである。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。`mosa --version` は `mosa 0.0.1 (phase 2)` を出す

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 2 完了
進行中: なし
完了: nrev と queens と段階 A と算術の Examples が通る。版は mosa 0.0.1 (phase 2)
次: フェーズ3の組込述語（アトム処理）
未解決: なし
計画: docs/superpowers/plans/2026-10-24-phase2-engine.md
```

- [ ] **Step 6: Commit**

```bash
git add tests/pl src/version.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase2] Examples と nrev と queens が通りフェーズ2を完了する（ctest）"
```

版テストのファイルも、変えていれば同じコミットに含める。
