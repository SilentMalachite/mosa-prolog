# フェーズ6 mosac Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `mosac` が Prolog のファイルから単独で動く実行ファイルを作り、第2層と第3層の全テストでインタプリタと出力が 1 バイトも違わず、翻訳した `nrev` がインタプリタの 3 倍以上速い。

**Architecture:** 実行ファイルは Prolog の原文を埋め込み、起動時に今の読み込み器でそのまま読む。指示の順、`op/3`、項の展開、モジュールの解決はインタプリタと同じ道を通る。`mosac` は翻訳のときに同じ読み込み器で静的述語の IR を作り、述語ごとに C++ 関数と IR の指紋を出す。起動時の読み込みで静的述語の IR の指紋が一致したら、その述語だけを生成関数に差し替える。一致しない述語、動的述語、`assert` された節はインタプリタで動く。生成関数はトランポリンの上で動き、本体はゴールごとに 1 状態の `switch` で再開する。

**Tech Stack:** C++20、CMake 3.25 以上（利用者の側では任意）、CTest、標準ライブラリのみ。

**Spec:** `docs/SPEC.md` 第6節（`mosac` の生成コード、ビルドと実行の流れ、ツールチェーン、エラーの扱い、実行ファイルの振る舞い）、第8節（第4層の差分テスト、ベンチマーク）、第9節（フェーズ6の行）。実行の形は ADR 0003、モジュールのゴール語は ADR 0006、節の置き場は ADR 0004 と 0008。

## 開始条件

- フェーズ5は完了している（2026-10-29、合格 2135/2171 = 98.3%、`japanese.pl` 全件、3 OS の CI 緑 run 40765329448）
- 2026-10-29 時点のインタプリタの `nrev` は、30 要素の `nrev` を失敗駆動で 20000 回まわして 3.15 秒（macOS、Release、約 3.1 MLIPS）。これは下見の値で、基準値は Task 6 で `bench/nrev.pl` から取り直す
- 1<<20 語のヒープでは、決定的な繰り返しはすぐ上限に届く（GC はフェーズ7）。ベンチマークの繰り返しは失敗駆動にする

## 持ち主の決定（2026-10-29）

1. 差分テストの単位は B: ディレクトリ単位。`tests/pl` と同梱スイートをそれぞれ 1 つの実行ファイルにまとめ、ファイルごとに新しい `Machine` を作ってそのファイルの単位を入れる
2. 共存の方式は A: 原文を埋め込み、IR の指紋が一致した静的述語だけを生成関数に差し替える。細部は Task 1 の ADR 0009 で決め、承認を受ける
3. 「きれいな環境」の確かめ方は A: Linux は素の `ubuntu:24.04` コンテナで実行する。macOS は `otool -L`、Windows は `dumpbin /dependents` で OS のライブラリ以外に依存しないことを確かめ、ビルドとは別のジョブで実行ファイルだけを落として走らせる

## Global Constraints

- 言語は C++20。依存は標準ライブラリと CMake だけ。`-Wall -Wextra -Wpedantic`（MSVC は `/W4`）を警告エラーで通す。生成コードにも同じ旗を当てる
- 1 関数 60 行以内、1 ファイル 800 行以内。`lgt_file.cpp`（730 行）、`lgt_exec.cpp`（708 行）、`lgt_judge.cpp`（691 行）、`lgt_io.cpp`（665 行）、`load.cpp`（585 行）に足すときは、先に分割する
- 識別子は英語。コメント、コミットメッセージは日本語。文書は英語が正で `*.ja.md` を同じコミットで直す。`docs/STATUS.md` は英語だけで 15 行以内
- `#ifdef _WIN32` などの OS 分岐は `src/platform/` だけに書く。vswhere と vcvarsall 相当の環境変数の設定もここに置く
- 生成コードが include するのは `mosa/runtime.hpp` だけ。マクロを使わない。`inline` 関数と `constexpr` で書く
- `include/mosa/mosa.hpp` の API を増やさない（フェーズ7まで）。生成コードが使う API は全部 `include/mosa/runtime.hpp` に置く
- 利用者向けの文面（`mosac` の診断、内部エラー、ツールチェーンの手順）は `lib/messages_ja.pl` と `lib/messages_en.pl` の鍵にする。C++ に文面を書かない（ADR 0007）
- Prolog の `throw/1` に C++ の例外を使わない。生成コードもトランポリンの上で動き、C++ の再帰を使わない
- Cell（ADR 0001、0002）と IR（ADR 0003）は変えない。ゴール枠の種類を足すことは ADR 0009 に書いて承認を受けてからにする
- `tests/conformance/logtalk/` は改変しない。期待値を実装に合わせて書き換えない
- `mosac dist`、GC、`garbage_collect/0`、`statistics/2`、任意精度整数はフェーズ7。この計画では作らない。クロスコンパイルは非目標
- `iso` 旗が真のときの振る舞いは常に ISO。翻訳しても変えない
- コミットは 1 論理単位ずつ、`[phase6] 内容（確認方法）` の日本語 1 行。本文と `Co-Authored-By` などの付記は付けない
- 各タスクの終わりに `cmake --build build && ctest --test-dir build` を通し、`docs/STATUS.md` を更新する
- 版は最後のタスクまで `mosa 0.0.1 (phase 5)` のまま

## Review Focus

1. 変数どうしの標準順序はヒープの番地で決まる。生成関数が節の変数をインタプリタの `enter_clause` と違う順で作ると、`sort/2`、`@</2`、`setof/3` の結果が変わる。Task 4 の `native_var_order` で固定する
2. 翻訳した静的述語を実行中に置き換えたり変えたりしたとき。別のファイルの `consult/1` が同じ述語を定義し直したら生成関数を外し、`assertz/1` は静的述語と同じ `permission_error(modify, static_procedure, PI)` を投げる。Task 3 の `native_reconsult_replaces` と `native_assert_static` で固定する
3. 起動時の読み込みで IR が翻訳時と変わったとき（指示のゴールが `op/3` を呼ぶ、旗 `double_quotes` を変える、`term_expansion/2` が実行時に定義される）。古い生成関数を使わずインタプリタで動くこと。Task 3 の `fingerprint_mismatch_falls_back` で固定する
4. 翻訳した述語の深い再帰が C++ のスタックを使わないこと。1<<20 語のヒープに収まる深さ（Task 7 で枠の語数から決め、目安は 30000 段）の末尾でない再帰が、インタプリタと同じ答えを返す。ヒープ上限の警告で終わる深さにはしない。Task 7 の `native_deep_recursion` で固定する
5. 単独の実行ファイルが外部のファイルを探さないこと。`MOSA_LIB` を外し、`lib/` の無いディレクトリで動かしても、捕捉されない例外の文面が `LANG` の言語で出て終了コード 2 になる。Task 10 の `standalone_uncaught_message` で固定する

---

## ファイルの責任

| パス | 責任 | タスク |
| --- | --- | --- |
| `docs/adr/0009-native-code.md`（新規、`.ja.md` も） | 生成関数の呼び出し規約、継続枠、選択点、指紋、差し替えの時点 | 1 |
| `src/machine/load_directive.cpp`、`src/builtins/flags.cpp`、`src/machine/session.cpp` | `initialization(main, main)`、旗 `argv`、終了コード | 2 |
| `include/mosa/runtime.hpp`、`src/machine/native.cpp`（新規）、`src/machine/native_redo.cpp`（新規）、`src/machine/internal.hpp`、`src/machine/choice.cpp`、`src/machine/engine.cpp` | 生成コードの API、差し替え、継続枠、やり直し | 3 |
| `src/codegen/emit.cpp`、`src/codegen/emit_head.cpp`、`src/codegen/emit_body.cpp`、`src/codegen/mangle.cpp`（すべて新規）、`src/codegen/codegen.hpp`（新規） | IR から C++ を出す | 4、6、7 |
| `src/main_mosac.cpp`、`src/codegen/project.cpp`（新規） | `mosac emit`、`main.cpp` と `CMakeLists.txt` の出力 | 5 |
| `bench/*.pl`、`src/machine/bench.cpp`（新規）、`docs/bench.md`（`.ja.md` も） | ベンチマークと計測 | 6、13 |
| `src/platform/toolchain.cpp`（新規）、`include/mosa/toolchain.hpp`（新規） | コンパイラと CMake の検出、MSVC の環境 | 8 |
| `src/codegen/build.cpp`（新規）、`src/codegen/cache.cpp`（新規） | `mosac build` と `run`、直接のコンパイル、`.mosac-cache/` | 9 |
| `CMakeLists.txt` の install、`src/module/library_path.cpp`、`src/machine/messages.cpp` | 同梱の実行時ライブラリ、翻訳済みの `lib/*.pl` | 10 |
| `src/machine/lgt_*.cpp` の分割、`src/machine/suite.cpp`、`src/main_mosatest.cpp`、`src/machine/diff.cpp`（新規） | `mosatest --diff` | 11 |
| `.github/workflows/ci.yml`、`README.md`、`README.ja.md`、`docs/agreement.json`（生成） | 3 OS の差分、一致率のバッジ、きれいな環境 | 12 |

---

## 前半: 規約と縦の一本

### Task 1: ADR 0009 を書き、手書きの nrev で 3 倍を見積もる

**Files:**
- Create: `docs/adr/0009-native-code.md`、`docs/adr/0009-native-code.ja.md`
- 使い捨て: 捨てるブランチ `phase6-spike` の上の機械の仮の変更と手書きの C++。測ったら捨て、数だけを ADR に入れる

**Interfaces:**
- Consumes: ADR 0003（ゴール枠、選択点と catch の同じスタック）、ADR 0006（ゴール語のモジュール）、ADR 0008（dead の印）
- Produces: Task 3 以降が従う規約。ADR は次の 7 点を「Context、Decision、Rejected alternatives、Consequences」各 3 行以内で決める
  1. 差し替えの時点と単位: 原文の読み込みで静的述語が定まった時点で、`(module, name, arity, fingerprint)` が一致すれば `Predicate::native` に関数を付ける。`consult` で定義し直されたら外す
  2. 指紋: 節の IR をアトムの番号ではなく名前で数える 64 ビットのハッシュ。翻訳時と起動時で同じ関数 `clause_fingerprint` を使う
  3. 生成関数の型: `void (*)(Machine::Impl&, std::uint64_t frame, std::uint32_t state)`。`state` は節の番号と本体のゴールの位置を詰めた値で、関数は `switch (state)` で再開する
  4. 継続枠: 本体の 2 つ目以降のゴールは、語 0 が 0 の制御枠の新しい種類 `Ctrl::Native`（関数、状態、節の変数のセル）で表す。ADR 0003 のゴール枠を拡げる文をここに書く
  5. 選択点: `Pick::Native` を足し、やり直しは同じ関数を次の節の状態で呼ぶ。候補の並びは `select_clauses` と同じ規則で作る
  6. 呼び出しの近道: 生成関数どうしの呼び出しは文字列を比べる `run_builtin` を通らず、述語の番号から関数を引く
  7. `runtime.hpp` の境界: 生成コードは `Machine::Impl` を中身の見えない型として受ける。`deref`、タグの判定、枠の引数の読み出しは `heap.hpp` と `cell.hpp` の inline の関数でその場に展開し、`libmosa` の外の呼び出しにするのは束縛（トレイル）、枠の確保、選択点だけにする。どれを inline にするかをここで決める
- [ ] **Step 1:** 捨てるブランチ `phase6-spike` で、上の 1〜7 に必要な機械の仮の変更（`Predicate::native`、`Pick::Native`、`Ctrl::Native`）を入れ、`nrev/2` と `app/3` を規約どおり手で書いた C++ を、7 の境界だけを通して `libmosa` にリンクし、同じ失敗駆動の繰り返しを測る。インタプリタとの比を ADR の Context に書き、ブランチは捨てる
- [ ] **Step 2:** 比が 3 倍に届かなければ、どの費用が残っているか（頭部の単一化、枠の確保、引き当て）を 1 行で書き、規約を直してから測り直す
- [ ] **Step 3:** ADR の英語と日本語を同じコミットで置く。コミット `[phase6] ADR 0009 生成コードの共存と呼び出し規約を置く（手書き nrev で N 倍）`
- [ ] **Step 4:** 持ち主に承認を求めて、このタスクを終える。承認まで Task 3 以降のコードを書かない

### Task 2: 実行ファイルの入口をインタプリタに先に入れる

翻訳した実行ファイルと同じ振る舞いをインタプリタが持っていないと、差分が 0 にならない。

**Files:**
- Modify: `src/machine/load_directive.cpp`（`initialization/2`）、`src/builtins/flags.cpp`（旗 `argv`）、`src/machine/session.cpp`、`src/main_mosa.cpp`
- Test: `tests/pl/directives.pl`、`tests/cpp/session_test.cpp`

**Interfaces:**
- Produces: `:- initialization(G, main).` はファイルを読み終えたあと `G` を 1 回呼ぶ。成功で終了コード 0、失敗で 1、捕捉されない例外は文面を出して 2、`halt/1` の値はそのまま。旗 `argv` は読み取り専用で、プログラムに渡す引数のアトムのリスト（プログラム名は含めない）。`mosa` では `--` より後ろの引数
- [ ] **Step 1:** 失敗するテストを書く。`session_test` に `init_main_success`（0）、`init_main_fail`（1）、`init_main_throw`（2 と文面）、`init_main_halt3`（3）、`argv_after_dashes`（`mosa f.pl -- a 漢字` で `current_prolog_flag(argv, X)` が `[a, 漢字]`）。`directives.pl` に `argv_readonly`（`set_prolog_flag(argv, [])` が `permission_error(modify, flag, argv)`）
- [ ] **Step 2:** `ctest --test-dir build -R "session|prolog_tests"` で 6 件が失敗することを見る
- [ ] **Step 3:** 実装する。`-g` があるときの順は「ファイル、`initialization(_, main)`、`-g`」とし、`main` の終了で `-g` は動かさない
- [ ] **Step 4:** テストを流し、README の両言語の使い方に `--` を足す。コミット `[phase6] initialization(main, main) と旗 argv を入れる（session_test 5 件、directives.pl argv_readonly）`

### Task 3: 生成コードの API と差し替え

**Files:**
- Create: `src/machine/native.cpp`（登録、指紋、差し替え）、`src/machine/native_redo.cpp`（`Pick::Native` と `Ctrl::Native`）
- Modify: `include/mosa/runtime.hpp`、`src/machine/internal.hpp`、`src/machine/choice.cpp`、`src/machine/engine.cpp`、`src/machine/load.cpp`（先に 2 つに分ける）
- Test: `tests/cpp/native_test.cpp`（新規、手書きの生成関数を登録して使う）

**Interfaces:**
- Consumes: ADR 0009 の 7 点
- Produces: 次の宣言をこのとおりに書く（ADR 0009 が引く正の置き場）。これは以前のこの欄の関数の並び（`bind_atom`、`bind_int`、`fresh`、`make_atom`、`make_int`、`make_float`、`make_list`、`make_struct`）を置き換える。項は `alloc` で節ごとに 1 塊を取り、inline の `put_*` で書く。番号（呼び先とアトム）はすべて単位の中だけで振る

```cpp
// include/mosa/runtime.hpp（生成コードが include する唯一のヘッダ）
namespace mosa::rt {
using NativeProc = void (*)(Machine::Impl& machine, std::uint64_t frame, std::uint32_t state);
struct UnitTag { char unused = 0; };  // 翻訳単位ごとに constinit で 1 つ置く。番地だけを使う
struct UnitState;                     // Machine ごとの単位の表。internal.hpp で定義する
struct NativeEntry { std::string_view module; std::string_view name; std::uint16_t arity;
                     std::uint64_t fingerprint; NativeProc proc; };
struct NativeCallee { std::string_view module; std::string_view name; std::uint16_t arity; };
struct NativeUnit { std::string_view name; std::string_view source; const UnitTag* tag;
                    std::span<const NativeEntry> entries;      // 添字 = interpret_clause の entry
                    std::span<const std::string_view> atoms;    // 添字 = regs.atoms の添字
                    std::span<const NativeCallee> calls; };     // 添字 = call_goal の number
struct Regs { const std::uint64_t* words; const std::uint32_t* atoms; const UnitState* unit; std::size_t cut; };

inline constexpr std::size_t kFrameHead = 4;    // ゴール枠の頭。引数は frame + 4 + i
inline constexpr std::size_t kNativeVars = 5;   // Ctrl::Native の変数は frame + 5 + i
inline constexpr std::uint64_t kNativeBit = std::uint64_t{1} << 63;
inline constexpr std::uint32_t kCall = 0;
[[nodiscard]] constexpr std::uint32_t state_of(std::uint32_t clause, std::uint32_t goal);  // clause << 16 | goal。節は 1 から

// inline（libmosa を呼ばない）
[[nodiscard]] inline const std::uint64_t* cell_words(Cell cell);
[[nodiscard]] inline Cell deref(Cell cell);
[[nodiscard]] inline bool unbound(Cell cell);
[[nodiscard]] inline Cell head(Cell list);
[[nodiscard]] inline Cell tail(Cell list);
[[nodiscard]] inline std::uint64_t functor_word(Cell structure);       // pack_functor と比べる
[[nodiscard]] inline Cell struct_arg(Cell structure, std::size_t index);
[[nodiscard]] inline Cell atom_cell(std::uint32_t atom);
[[nodiscard]] inline Cell small_int(std::int64_t value);               // 即値の範囲だけ。外は put_int_box
[[nodiscard]] inline Cell arg(const Regs& regs, std::uint64_t frame, std::size_t index);
[[nodiscard]] inline Cell var(const Regs& regs, std::uint64_t frame, std::size_t index);
[[nodiscard]] inline std::uint64_t frame_cut(const Regs& regs, std::uint64_t frame);   // 語 2
[[nodiscard]] inline std::uint64_t frame_cont(const Regs& regs, std::uint64_t frame);  // 語 3
[[nodiscard]] inline Cell put_ref(std::uint64_t* slot);                                  // 1 語
[[nodiscard]] inline Cell put_list(std::uint64_t* slot, Cell first, Cell rest);          // 2 語
[[nodiscard]] inline Cell put_struct(std::uint64_t* slot, std::uint32_t atom, std::span<const Cell> args);  // 1 + n 語
[[nodiscard]] inline Cell put_int_box(std::uint64_t* slot, std::int64_t value);          // 1 語
[[nodiscard]] inline Cell put_float(std::uint64_t* slot, double value);                  // 1 語

// libmosa の外の呼び出しはこれだけ
[[nodiscard]] bool install_unit(Machine& machine, const NativeUnit& unit);  // 表を作ってから source を読み、読み終えたら付ける
[[nodiscard]] Regs regs(Machine::Impl& machine, const UnitTag& unit);       // 1 回の起動に 1 回。tag の番地で単位を引く
[[nodiscard]] std::uint64_t* alloc(Machine::Impl& machine, std::size_t count);  // 節ごとに 1 塊
void bind(Machine::Impl& machine, Cell variable, Cell value);                 // 最新の選択点より古ければトレイル
[[nodiscard]] bool unify(Machine::Impl& machine, Cell left, Cell right);     // 変数と同じセルの近道のあと do_unify
void call_goal(Machine::Impl& machine, const Regs& regs, std::uint32_t number, std::span<const Cell> args,
               std::uint64_t cont, std::size_t cut);
[[nodiscard]] std::uint64_t resume_frame(Machine::Impl& machine, NativeProc proc, std::uint32_t state, std::size_t cut,
                                         std::uint64_t cont, std::span<const Cell> vars);
void push_alternative(Machine::Impl& machine, const Regs& regs, std::uint64_t frame, NativeProc proc,
                      std::span<const std::uint32_t> states);  // cut = regs.cut
void cut_to(Machine::Impl& machine, std::size_t barrier);
void proceed(Machine::Impl& machine, std::uint64_t cont);
void fail(Machine::Impl& machine);
void interpret_clause(Machine::Impl& machine, const Regs& regs, std::uint32_t entry, std::uint64_t frame,
                      std::uint32_t clause);
}
```

  - `interpret_clause` は節全体（頭部と本体）を IR のまま動かす。`entry` はその単位の `entries` の添字（今動いている述語）、`clause` は `state_of` と同じく 1 から数える節の番号、`frame` はゴール枠で、カットの境界は `regs.cut`。状態 0 か節の入口の状態から、その節の生成した頭部と本体の代わりに呼び、中で `enter_clause(machine, predicate, clause - 1, frame, regs.cut)` を呼ぶ
  - 再開の状態（goal が 1 以上）では、カットの境界は `Ctrl::Native` の枠の語 2（`frame_cut`）で、`regs.cut` は状態 0 と節の入口の状態でだけ正しい
  - 内部（`internal.hpp`）: `namespace mosa::rt { struct UnitState { const UnitTag* tag; std::vector<std::uint32_t> atoms; std::vector<NativeCall> calls; std::vector<PredKey> entries; }; }`、`struct NativeCall { std::uint32_t module; std::uint32_t name; std::uint16_t arity; rt::NativeProc proc; };`。足すメンバは `Predicate::native`、`Pick::Native`、`Ctrl::Native`、`Entry::native`、`Impl::native_units`、`Impl::native_next`、`Impl::native_cut` だけ
  - 内部: `std::uint64_t clause_fingerprint(const AtomTable& atoms, const Predicate& predicate);`（`src/machine/native.cpp`、Task 4 の翻訳器も同じ関数を呼ぶ）。`meta` の引数を先に、次に節の IR を数える。`void detach_native(Machine::Impl& machine, Predicate& predicate);` は付けたあとの節か `meta` への書き込みのたびに呼び、`Predicate::native` と全単位の同じ鍵の `calls` を一度に空にする
- [ ] **Step 1:** `native_test` を書く。手書きの `app/3` を登録した機械と、登録しない機械で、`app(X, Y, [1,2])` の全解、`app([1], [2], L)`、カットの境界（`once(app(X, _, [1,2]))`）、本体の例外が外の `catch/3` に届くことが同じになる。加えて Review Focus 2 と 3 の `native_reconsult_replaces`、`native_assert_static`、`fingerprint_mismatch_falls_back`（原文に `:- op(700, xfx, ===>).` を指示のゴールの中で呼び、IR が変わる形）
- [ ] **Step 2:** `ctest --test-dir build -R native` が失敗することを見る
- [ ] **Step 3:** `load.cpp` を分けるコミットを先に作り、次に実装する。`predicate_property/2` は翻訳した述語にも `static` を返し、`number_of_clauses` は残した IR の節の数を返す
- [ ] **Step 4:** テストと `mosatest --suite tests/conformance` を流し、合格 2135 が変わらないことを見る。コミット `[phase6] 生成コードの API と静的述語の差し替えを入れる（native_test N 件、スイート合格 2135 で変わらず）`

### Task 4: IR から C++ を出す（頭部と連言）

**Files:**
- Create: `src/codegen/codegen.hpp`、`src/codegen/emit.cpp`、`src/codegen/emit_head.cpp`、`src/codegen/emit_body.cpp`、`src/codegen/mangle.cpp`
- Test: `tests/cpp/codegen_test.cpp`（新規）、`tests/pl/native.pl`（新規、差分の素材。Task 11 から `--diff` でも流す）

**Interfaces:**
- Consumes: Task 3 の `rt::` と `clause_fingerprint`
- Produces:
  - `struct EmitResult { std::string source; std::string header; std::size_t translated = 0; std::size_t interpreted = 0; };`
  - `[[nodiscard]] EmitResult emit_module(const Machine& machine, std::uint32_t module, std::string_view unit_name);`
  - `[[nodiscard]] std::string mangle(std::string_view name);` 英数字はそのまま、`_` は `__`、ほかの UTF-8 のバイトは `_` と 2 桁の 16 進。関数名は `p_` + `mangle(name)` + `_` + アリティ、モジュールは C++ の名前空間 `mosa_gen::m_` + `mangle(module)`
  - 各述語の前に元の節を `writeq` の形でコメントに入れる（日本語のままでよい）
- 頭部は `Var`、`Anon`、`Atom`、`Int`、`Float`、`List`、`Fun` を全部その場の C++ で単一化する。本体は `Atom`、`Fun`、`Call` の連言を Task 3 の継続枠で動かす。それ以外の構文（`Or`、`IfThen`、`SoftIf`、`Catch`、`Throw`、`Halt`、`Var`、`List`）を含む節は、その節だけ `interpret_clause` に渡す（Task 7 で減らす）
- 節の変数のセルは、`enter_clause` と同じくスロットの順にすべて節に入った時点で作る（Review Focus 1）
- [ ] **Step 1:** `codegen_test` を書く。`mangle("親")` が `_e8_a6_aa`、`mangle("a_b")` が `a__b`。`app/3` の出力にコメント `% app([], L, L).` と関数 `p_app_3` があり、`translated == 1`。`;` を含む節は `interpreted` に数えられる
- [ ] **Step 2:** `tests/pl/native.pl` に `native_var_order`（`mk(L) :- L = [B, A], g(A), g(B).` と `g(_).` のもとで `test(native_var_order, (mk(L), msort(L, S), S == L), true)`。スロットの順に作れば `B` が古い）と、事実、規則、深さのある構造、浮動小数、負の整数、日本語の原子の頭部を 1 件ずつ書く。この時点ではインタプリタで通るだけでよい
- [ ] **Step 3:** 実装し、`codegen_test` を通す。コミット `[phase6] IR から頭部と連言の C++ を出す（codegen_test N 件）`

### Task 5: `mosac emit` で hello を動かす

**Files:**
- Modify: `src/main_mosac.cpp`
- Create: `src/codegen/project.cpp`、`tests/mosac/hello.pl`（新規）
- Test: CTest `mosac_emit_hello`、`mosac_emit_broken`、`mosac_emit_undefined`、`mosac_emit_silent`（CMake のスクリプトで emit、configure、build、実行を順に行う）

**Interfaces:**
- Consumes: Task 4 の `emit_module`、Task 3 の `install_unit`
- Produces: `mosac emit app.pl -o out/` が `out/` に、モジュールごとの `.cpp` と `.hpp`、`main.cpp`、`CMakeLists.txt` を書く。`main.cpp` は `Machine::create(std::size_t{1} << 20)`（`mosa` と同じ既定値）、旗 `argv` を入れ、`install_unit` を呼び、Task 2 の終了コードの規則で返す。`CMakeLists.txt` は `MOSA_RUNTIME_DIR`（既定は emit した `mosac` のビルド木）から `libmosa` と `include/` を見つける。読み込みの段階の誤り（構文エラー、`export` したのに定義の無い述語、どこにも定義の無い述語を呼ぶ静的な警告）は Prolog の行番号で出してコンパイラを起動しない。差分用の `--units`（Task 11）はこの止め方をしない。存在エラーを確かめるテストがあるため
- 翻訳のときの読み込みは宣言だけを実行する: `op/3`、`module/2`、`use_module/1,2`、`dynamic/1`、`discontiguous/1`、`multifile/1`、`set_prolog_flag/2`、`include/1`、`ensure_loaded/1`。`initialization/1,2` は記録だけして呼ばない。ほかの指示のゴールは飛ばす（そのゴールが `op/3` を呼ぶ形なら、起動時の指紋が合わずインタプリタで動くだけで、誤りにはならない）
- 翻訳のときの読み込みが開いたファイルは、正規化したパスを鍵にした単位になる。起動時の読み込みは `include/1`、`ensure_loaded/1`、`consult/1`、`use_module(File)` をファイルシステムより先に埋め込みの単位から引く
- [ ] **Step 1:** 翻訳のときの読み込みの失敗するテスト `mosac_emit_silent` を書く。`:- write(x).` を持つファイルは `emit` のとき何も書かず、できた実行ファイルが起動すると `x` を書く
- [ ] **Step 1b:** `tests/mosac/hello.pl`（`:- initialization(main, main). main :- write('こんにちは'), nl.`）、構文エラーの `tests/mosac/broken.pl`、未定義の述語を呼ぶ `tests/mosac/undefined.pl` を置く。テスト `mosac_emit_hello` は実行結果が `こんにちは\n`、終了コード 0。テスト `mosac_emit_broken` は `broken.pl:行:列:` で始まる診断と終了コード 1、`out/` に `.cpp` が無い。テスト `mosac_emit_undefined` は呼んだ行の番号と述語の指示子を出して終了コード 1
- [ ] **Step 2:** 失敗を見て実装する。文面の鍵は `messages_*.pl` に足す
- [ ] **Step 3:** コミット `[phase6] mosac emit で hello を出し CMake で動かす（mosac_emit_hello、mosac_emit_broken、mosac_emit_undefined、mosac_emit_silent）`

### Task 6: nrev を 3 倍にする

**Files:**
- Create: `bench/nrev.pl`、`src/machine/bench.cpp`、`docs/bench.md` と `.ja.md` の表
- Modify: `src/codegen/emit_head.cpp`、`src/codegen/emit_body.cpp`、`src/machine/native.cpp`、`src/main_mosatest.cpp`（`--bench`）

**Interfaces:**
- Produces: `mosatest --bench bench/nrev.pl` は、インタプリタと `mosac emit` で作った実行ファイルで `bench(N)` を 5 回ずつ流し、壁時計の中央値と比を出す。`bench/nrev.pl` は SPEC の `nrev` に 30 要素のリストと失敗駆動の繰り返し `bench(N)` を足したもの。`statistics/2` は使わない（フェーズ7）
- 速さは 3 点だけで取る: 第1引数の型で節を選ぶ `switch`、生成関数どうしの直接の呼び出し（ADR 0009 の 6）、頭部の単一化で枠の引数を直接読むこと。インタプリタの道は速くしない（比の分母を動かさない）
- [ ] **Step 1:** `--bench` を先に作り、インタプリタの基準値を `docs/bench.md` に書く
- [ ] **Step 2:** 翻訳の比を測る。3 倍に届かなければ、この計画を進めず、残る費用を STATUS に書いて ADR 0009 の改訂を持ち主に諮る
- [ ] **Step 3:** 3 倍を超えたら表を両言語で書く。コミット `[phase6] 翻訳した nrev がインタプリタの N 倍（mosatest --bench、5 回の中央値）`

---

## 中盤: 翻訳の幅とビルドの流れ

### Task 7: 制御構文を生成コードで動かす

**Files:**
- Modify: `src/codegen/emit_body.cpp`（800 行を超えるなら `emit_control.cpp` に分ける）
- Test: `tests/pl/native.pl`、`tests/cpp/codegen_test.cpp`

**Interfaces:**
- 生成コードで動かすもの: `!`（節のカット境界）、`(A -> B ; C)`、`(A ; B)`、`(A *-> B ; C)`、`\+ A`、`call/N`、`catch/3`、`throw/1`、モジュール限定 `M:G`（ADR 0006 のゴール語）、`meta_predicate` の引数。`is/2` と比較の 6 述語は組込の C++ を直接呼ぶ。ほかの組込は `call_goal` で枠を作る
- [ ] **Step 1:** `tests/pl/native.pl` に構文ごとに 1 件と、Review Focus 4 の `native_deep_recursion`（`len(L, N)` の末尾でない再帰。深さは 1<<20 語に収まるよう枠の語数から決め、答えの `N` を確かめる）を書く。`codegen_test` で各構文が `translated` に数えられることを確かめる失敗するテストを書く
- [ ] **Step 2:** 構文を 1 つずつ実装し、そのたびに `tests/pl/native.pl` をインタプリタと `mosac emit` の実行ファイルの両方で流して同じ出力であることを見る（Task 11 の前は手で比べる）
- [ ] **Step 3:** 構文ごとにコミット `[phase6] 生成コードで <構文> を動かす（native.pl <名前>、codegen_test）`

### Task 8: ツールチェーンの検出と `mosac toolchain`

**Files:**
- Create: `src/platform/toolchain.cpp`、`include/mosa/toolchain.hpp`
- Test: `tests/cpp/toolchain_test.cpp`

**Interfaces:**
- Produces: `struct Toolchain { std::filesystem::path cxx; std::string cxx_version; std::optional<std::filesystem::path> cmake; std::filesystem::path runtime_dir; std::vector<std::pair<std::string, std::string>> env; };`、`[[nodiscard]] std::optional<Toolchain> find_toolchain(const ToolchainQuery& query);`。順は `--cxx` → `MOSA_CXX` → `CXX` → OS の既定（macOS は `clang++`、Linux は `g++` → `clang++`、Windows は `vswhere` で MSVC を見つけ、vcvarsall 相当の環境変数を `env` に入れる）。探索は環境とパスを引数で受ける版を分け、テストは偽の PATH で引く（`find_library_in` と同じ形）
- `mosac toolchain` はコンパイラ、版、CMake、同梱の実行時ライブラリの場所を出す。見つからないときは OS ごとの導入手順（Xcode Command Line Tools、`apt install g++`、Visual Studio Build Tools）を `messages_*.pl` の鍵から出して終了コード 1
- [ ] **Step 1:** `toolchain_test` に `cxx_flag_wins`、`mosa_cxx_before_cxx`、`default_order_linux`、`nothing_found` を書いて失敗を見る
- [ ] **Step 2:** 実装し、`mosac toolchain` を 3 OS の CI で 1 回走らせる。コミット `[phase6] ツールチェーンを検出し mosac toolchain で出す（toolchain_test 4 件、3 OS の CI）`

### Task 9: `mosac build` と `mosac run`

**Files:**
- Create: `src/codegen/build.cpp`、`src/codegen/cache.cpp`
- Modify: `src/main_mosac.cpp`
- Test: CTest `mosac_build_hello`、`mosac_run_exit_code`、`mosac_run_args`、`mosac_cache_reuse`、`mosac_internal_error`、`mosac_debug_trace`

**Interfaces:**
- Produces: `mosac build app.pl -o app`（Windows は `app.exe`）、`mosac run app.pl -- 引数...`（一時ディレクトリで作り、終了コードをそのまま返す）。CMake が無くても、生成コードを 1 ファイルずつコンパイルし、`mosac` がリンクの命令を組む。既定は `-O2` と静的リンク、Windows は `/MT`。`--debug` は `-O0 -g` に加え、生成関数が節に入るたびに元の節のコメントと同じ文を標準エラーに書く（実行時の節の追跡。生成コードの `constexpr bool` で切り替え、マクロは使わない）。中間物は `.mosac-cache/<hash>/` に置き、ハッシュは原文と実行時ライブラリから取る。`--keep`、`--clean`、`--verbose`、`--cxx`
- 生成コードのコンパイルが失敗したら「内部エラー: 生成コードがコンパイルできません。`.mosac-cache/<hash>/` を添えて報告してください」の鍵だけを出し、コンパイラの生の誤りは `--verbose` のときだけ出す。コンパイラが無い、ディスクが足りない、権限が無いは環境の問題として手順を出す
- [ ] **Step 1:** 5 つのテストと `mosac_debug_trace`（`--debug` の hello が標準エラーに `main :- ` で始まる行を書く）を書く。`mosac_run_exit_code` は `halt(3)` で 3、`mosac_run_args` は `-- a 漢字` が旗 `argv` に `[a, 漢字]`、`mosac_cache_reuse` は 2 回目の `build` がコンパイラを呼ばない（`--verbose` の出力で見る）、`mosac_internal_error` は `MOSA_CXX` に必ず失敗するコンパイラの代わり（テスト用の小さな実行ファイル）を指して内部エラーの文面と終了コード 1
- [ ] **Step 2:** 失敗を見て実装する。CMake を使う道と直接の道の両方を、`MOSA_NO_CMAKE=1` で直接の道に固定したテストでも流す
- [ ] **Step 3:** コミット `[phase6] mosac build と run を入れる（mosac_* 6 件、CMake 無しの道も）`

### Task 10: 同梱の実行時ライブラリと翻訳済みの `lib/*.pl`

**Files:**
- Modify: `CMakeLists.txt`（install の規則）、`src/module/library_path.cpp`、`src/machine/messages.cpp`、`src/codegen/build.cpp`
- Test: CTest `mosac_installed_layout`、`standalone_uncaught_message`、`standalone_use_lists`、`standalone_include`

**Interfaces:**
- Produces: `cmake --install` が `<prefix>/bin/` に 3 コマンド、`<prefix>/lib/mosa/<OS>-<arch>-<compiler>/` に `libmosa` の静的ライブラリと `include/mosa/`、翻訳済みの `lib/*.pl` の単位（ライブラリごとのオブジェクト）を置く。`mosac` は自分の実行ファイルの場所から、インストールの形とビルド木の形の両方で実行時ライブラリを見つける。ABI が合わないときの `--runtime-source` は同梱の源から作り、キャッシュする
- `use_module(library(X))` は、実行ファイルの中の単位の一覧を SPEC 第5節の探索より先に引く。`mosac` は推移的に `use_module` されたライブラリと、文面のための `messages_ja`、`messages_en` だけをリンクする。SPEC 第5節の探索順（`$MOSA_LIB`、実行ファイルの横の `lib/`）を変える必要が出たら、変えずに持ち主に「A か B か」を 1 つ聞いて止まる
- [ ] **Step 1:** 4 つのテストを書く。`standalone_include` は `:- include('a.pl').` と `:- ensure_loaded(b).` を持つプログラムを build し、`a.pl` と `b.pl` の無いディレクトリで動かして両方の述語が呼べる（Task 5 の埋め込みの単位）。`mosac_installed_layout` は一時の prefix にインストールした `mosac` で hello を build できる。`standalone_uncaught_message`（Review Focus 5）は `MOSA_LIB` を外し、`lib/` の無い一時ディレクトリで `LANG=ja_JP.UTF-8` のとき `警告: ゴールは例外を送出しました: ` で始まり終了コード 2。`standalone_use_lists` は `library(lists)` の `append/3` を使う実行ファイルが同じ条件で動く
- [ ] **Step 2:** 失敗を見て実装する。コミット `[phase6] 実行時ライブラリと翻訳済みライブラリを同梱し、単独の実行ファイルが lib/ を探さない（3 件）`

---

## 後半: 差分、CI、残りのベンチマーク、出口

### Task 11: `mosatest --diff`

**Files:**
- 先に分割: `src/machine/lgt_file.cpp`、`src/machine/lgt_exec.cpp`（読み込みの関数を `lgt_load.cpp` へ）
- Create: `src/machine/diff.cpp`
- Modify: `src/machine/suite.cpp`、`src/main_mosatest.cpp`、`src/main_mosac.cpp`（内部用の `mosac emit --units`）
- Test: CTest `diff_tests_pl`、`diff_conformance`

**Interfaces:**
- Consumes: 持ち主の決定 1（B）
- Produces: 試験の実行関数は「ファイルをどう読むか」を引数 `std::function<bool(Machine&, const std::filesystem::path&)>` で受ける。インタプリタは今の `load_program`、翻訳はその単位の `install_unit`。`mosac emit --units DIR -o out/` はディレクトリの各ファイルを 1 単位にし、試験のゴールを `'$mosa_diff_goal'(K, 名前のある変数...) :- ゴール.` の節として単位に足して翻訳する。名前のある変数を引数に並べるので、答えの名前と順はインタプリタと同じ。この形にできないゴールはそのまま問い合わせで動かし、翻訳しなかった数として数える。`.lgt` は Prolog として読めないので、単位の `source` は試験器が `.lgt` の物から組み立てる原文（今 `lgt_file.cpp` の `load_ok` に渡しているもの）を同じ `libmosa` の関数で作ったものにする。両方の側が同じバイト列を読む
- `mosatest --diff DIR` は両方の出力（解、エラー項、標準出力）を 1 バイトずつ比べ、違った試験の名前と両方の出力を出す。一致率と、翻訳した述語の割合を `docs/agreement.json` と `docs/agreement.md` に書く
- [ ] **Step 1:** 分割だけのコミットを先に作る（スイート合格 2135 で変わらず）
- [ ] **Step 2:** `diff_tests_pl` を足して失敗を見る。`japanese.pl` と `native.pl` が必ず含まれることもこのテストで確かめる
- [ ] **Step 3:** 実装し、違いが出たら 1 件ずつ生成コードを直す。直せない形は翻訳せずインタプリタに回すが、その節の形と件数を `docs/agreement.md` に残す。期待値は書き換えない
- [ ] **Step 4:** `diff_conformance` を足し、同じ手順で違いを 0 にする
- [ ] **Step 5:** コミットは段階ごとに `[phase6] mosatest --diff で tests/pl の違い 0（一致 N/N、翻訳 M%）`、`[phase6] mosatest --diff でスイートの違い 0（一致 N/N、翻訳 M%）`

### Task 12: CI

**Files:**
- Modify: `.github/workflows/ci.yml`、`README.md`、`README.ja.md`

**Interfaces:**
- 3 OS のジョブに `mosatest --diff tests/pl` と `mosatest --diff tests/conformance` を足し、違いが 1 件でもあれば赤にする。生成コードは警告エラーの旗でコンパイルする（「生成した C++ は常にコンパイルできる」の不変条件）。Windows は `chcp 65001` で標準出力を UTF-8 で取る
- 一致率を Job Summary に出し、README の 2 つ目のバッジは `docs/agreement.json` を読む（`conformance.json` と同じく macOS だけで照合する）
- 持ち主の決定 3: `mosac build tests/mosac/hello.pl -o hello` の成果物を各 OS で上げ、別のジョブで落として走らせる。Linux はそのジョブの中で素の `ubuntu:24.04` コンテナで動かす。macOS は `otool -L` が `/usr/lib/` と `/System/` だけ、Windows は `dumpbin /dependents` が OS の DLL だけであることを確かめる
- [ ] **Step 1:** ワークフローを直して push し、3 OS で緑を見る。run の番号を STATUS に書く
- [ ] **Step 2:** コミット `[phase6] CI で 3 OS の差分と単独の hello を確かめる（run N）`

### Task 13: 残りのベンチマーク

**Files:**
- Create: `bench/queens.pl`、`bench/zebra.pl`、`bench/tak.pl`、`bench/deriv.pl`、`bench/crypt.pl`
- Modify: `docs/bench.md`、`docs/bench.ja.md`、`.github/workflows/ci.yml`

**Interfaces:**
- 各ファイルは `bench(N)` を失敗駆動の繰り返しで定義し、正しさの確かめ（最初の解）を 1 つ持つ。表はインタプリタと翻訳の時間と比。CI は macOS で `--bench` を流し、コミット済みの `docs/bench.json` より 20% 以上遅ければ警告にする（失敗にはしない）
- [ ] **Step 1:** 5 本を置き、両方で同じ最初の解になることを `--diff` の素材としても流す
- [ ] **Step 2:** コミット `[phase6] ベンチマーク 5 本と表、CI の 20% 警告（mosatest --bench）`

### Task 14: `mosac build --interactive`

**Files:**
- Modify: `src/codegen/project.cpp`、`src/main_mosac.cpp`
- Test: CTest `mosac_interactive`

**Interfaces:**
- 翻訳した単位を入れたあと `mosa` と同じ REPL（`run_session`）を起動する。翻訳した述語とインタプリタの述語が同じ `Machine` に並ぶ
- [ ] **Step 1:** `mosac_interactive` は、標準入力に `app(X, Y, [1]).` と `;` を流して `mosa` と同じ答えの並びが出る。失敗を見て実装する
- [ ] **Step 2:** コミット `[phase6] mosac build --interactive で翻訳済みの REPL を起動する（mosac_interactive）`

### Task 15: 出口

- [ ] **Step 1:** 完了条件を 1 つずつ機械で確かめる。(a) 3 OS で `--diff` の違い 0（CI の run 番号）、(b) 3 OS できれいな環境の hello が動く（同じ run）、(c) `docs/bench.md` の `nrev` の比が 3 倍以上
- [ ] **Step 2:** 版を `mosa 0.0.1 (phase 6)` にし、README の両言語に `mosac` の使い方とバッジ、`docs/STATUS.md` にフェーズ6の完了と次（フェーズ7）を書く
- [ ] **Step 3:** コミット `[phase6] フェーズ6を終える（差分 0、きれいな環境の hello、nrev N 倍、3 OS の CI 緑 run N）`
