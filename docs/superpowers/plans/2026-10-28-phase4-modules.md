# フェーズ4 モジュール Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** SPEC 第5節のモジュール系、`library(*)` の最初の 6 モジュール、`meta_predicate` を作り、`tests/pl/modules.pl` を通す。既存のテストは壊さない。

**Architecture:** 述語の鍵を `(module, name, arity)` にし、ゴールフレームの 1 語目（アリティの語）の上位ビットに文脈モジュールを入れる。フレームの語数と IR の形は変えない。本体のゴールの文脈は、その節が属する述語のモジュールである。`M:G` と `meta_predicate` の引数だけが文脈を切り替える。組み込み（`system`）は今どおり先に引き、`system` の名前の再定義は `permission_error` にするので、解決の順序（自モジュール → 既定の取り込み元 → `system`）と結果は同じになる。ライブラリは Prolog で書き、表を引く処理だけ C++ の `system` に置く。

**Tech Stack:** C++20、CMake 3.25 以上、CTest、標準ライブラリのみ。ライブラリは `lib/*.pl`。

**Spec:** `docs/SPEC.md` 第5節（モジュール系とライブラリ表）、第7節の段階 F と G、第9節のフェーズ4の行。フレームの形は `docs/adr/0003-execution-ir.md`、節の置き場は `docs/adr/0004-clause-database.md`。

## Global Constraints

- 言語は C++20。依存は標準ライブラリと CMake だけ。`-Wall -Wextra -Wpedantic`（MSVC は `/W4`）を警告エラーで通す
- 1 関数 60 行以内、1 ファイル 800 行以内。`src/machine/load.cpp`（793 行）は触る前に分ける
- 識別子は英語。コメント、コミットメッセージは日本語。文書は英語が正で `*.ja.md` を同じコミットで直す。`docs/STATUS.md` は英語だけで 15 行以内
- `#ifdef _WIN32` などの OS 分岐は `src/platform/` だけに書く
- エラーは `throw_error(m, Formal, PI)` と `make_*_error` 系だけで投げる。原子名を文字列リテラルで書かない
- 組み込み述語は「引数検査 → 本処理 → 束縛」の順。ファイル先頭の注釈に条項、引数のモード、投げうるエラー、テストファイル名を書く
- Cell（ADR 0001、0002）は変えない。ゴールフレームの意味の変更は Task 1 の ADR 0006 が承認されるまで書かない。IR（ADR 0003）の形は変えない
- `include/mosa/mosa.hpp` の API を増やさない（`Machine::create()`、`consult(path)`、`query(text, callback)`、`halt()` のまま）
- 段階 G のうち 6 モジュールの表に無いもの（`nb_setval/2`、`tab/1,2`、`term_to_atom/2`、`setup_call_cleanup/3`、`garbage_collect/0`、`statistics/2` など）は作らない。例外は Task 9 の `forall/2` だけ
- `tests/conformance/logtalk/` は改変しない。同梱スイートの合格数をフェーズ3の 2028 より下げない（下がったらそのタスクで直す）
- 期待値を実装に合わせて書き換えない
- 解釈が一つに決まらない点は推測で実装せず、`docs/iso-notes.md`（と `.ja.md`）に「A か B か」を 1 つ書いて、そのタスクを終える
- コミットは 1 論理単位ずつ、`[phase4] 内容（確認方法）` の日本語 1 行
- 各タスクの終わりに `cmake --build build && ctest --test-dir build` を通し、`docs/STATUS.md` を更新する
- 版は最後のタスクまで `mosa 0.0.1 (phase 3)` のまま

## Review Focus

1. モジュールの中の非公開述語と同名の述語が `user` にあっても、モジュールの中からはモジュールの方、`user` からは `user` の方が呼ばれること。Task 4 の `mod_shadow` で固定する
2. `user` から呼んだ公開述語が、自モジュールの非公開述語を閉包として `maplist/2` に渡しても動くこと（meta 引数がモジュール限定されること）。Task 9 の `meta_private_closure` で固定する
3. `M:G` の中で投げた例外を `user` の `catch/3` が捕まえた後、またはバックトラックで `M:G` に戻った後も、文脈が呼び手のモジュールに戻っていること。Task 4 の `mod_ctx_after_throw` と `mod_ctx_after_redo` で固定する
4. 同じライブラリを 2 つのモジュールから `use_module` しても、読み込みは 1 回で、取り込みの衝突にならないこと。モジュールのファイルを 2 回 `consult` しても節と公開が二重にならないこと。Task 5 の `use_twice` と `reconsult_module` で固定する
5. モジュールのファイルの読み込みが途中で失敗したとき、半分だけ登録されたモジュールや取り込みが残らないこと。Task 5 の `use_failed_rollback` で固定する

---

## ファイルの責任

| パス | 責任 | タスク |
| --- | --- | --- |
| `docs/adr/0006-modules.md`（新規、`.ja.md` も） | 述語の鍵、フレームの文脈モジュール、取り込み表、演算子の範囲 | 1 |
| `src/module/module.hpp`、`src/module/module_table.cpp`（新規） | `Module`、モジュール表、解決 | 2〜5 |
| `src/machine/load.cpp` から分ける `src/machine/load_directive.cpp` | 指令の振り分け | 2 |
| `src/machine/internal.hpp`、`src/machine/engine.cpp`、`src/machine/catch.cpp`、`src/machine/cut.cpp`、`src/builtins/database.cpp` | 鍵とフレームの文脈 | 2 |
| `src/module/qualify.cpp`（新規） | `:/2`、`module/2`、`export/1` | 3、4 |
| `src/module/import.cpp`（新規） | `use_module/1,2`、取り込み、衝突 | 5 |
| `src/platform/exe_path.cpp`、`include/mosa/exe_path.hpp`（新規）、`src/module/library_path.cpp`（新規）、`CMakeLists.txt` | `library(...)` の探索と `lib/` の複製 | 6 |
| `src/builtins/database.cpp`、`src/builtins/clause_term.cpp` | `M:` 付きの節操作と宣言 | 7 |
| `src/builtins/module_prop.cpp`（新規） | `current_module/1`、`predicate_property/2` | 8 |
| `src/module/meta.cpp`（新規） | `meta_predicate/1`、引数の限定、`forall/2` | 9 |
| `include/mosa/operator_table.hpp`、`src/read/operator_table.cpp`、`src/builtins/operators.cpp` | モジュールごとの演算子 | 10 |
| `src/machine/load.cpp` の `SavedFlags` | ファイルごとの `double_quotes`、`japanese_punctuation` | 11 |
| `lib/lists.pl`、`src/builtins/lists.cpp`（新規、`length/2`、`keysort/2`） | `library(lists)` | 12 |
| `lib/apply.pl`、`lib/between.pl` | `library(apply)`、`library(between)` | 13、14 |
| `lib/format.pl`、`src/builtins/format_float.cpp`（新規） | `library(format)` | 15 |
| `lib/strings.pl`、`tools/gen_unicode_tables.py`、`src/unicode_tables.inc`、`src/builtins/case.cpp`（新規） | `library(strings)` | 16、17 |
| `lib/japanese.pl`、`src/builtins/char_width.cpp`（新規） | `library(japanese)` | 18 |
| `tests/pl/modules.pl`、`tests/pl/modules/*.pl`（読み込まれる側。`mosatest tests/pl` は下位ディレクトリを走査しない） | モジュールのテスト | 4〜11、19 |
| `tests/pl/lib_lists.pl`、`lib_apply.pl`、`lib_between.pl`、`lib_format.pl`、`lib_strings.pl` | ライブラリのテスト（`library(japanese)` は `tests/pl/japanese.pl`） | 12〜18 |

---

### Task 1: ADR 0006 と解釈の問い

**Files:**
- Create: `docs/adr/0006-modules.md`、`docs/adr/0006-modules.ja.md`
- Modify: `docs/iso-notes.md`、`docs/iso-notes.ja.md`、`docs/adr/README.md`、`docs/adr/README.ja.md`

- [ ] **Step 1:** ADR 0006 を「Context、Decision、Rejected alternatives、Consequences」を各 3 行以内で書く。Decision の案:
  - `Predicate` に `std::uint32_t module`（モジュール名の原子番号）を足し、鍵を `(module, name, arity)` にする。`system` は C++ の組み込みと制御構文、`user` は既定のモジュール
  - ゴールフレームの 1 語目は、下位 16 ビットがアリティ、ビット 16〜47 が文脈モジュールの原子番号。`kFrameHead` は 4 のまま。本体のゴールのフレームには、その節の述語の `module` を書く
  - 解決は「文脈モジュールの定義と取り込み → 既定の取り込み元（`user` の取り込み元は `system`）」。`system` は今どおり最初に引く。`system` の名前を定義すると `permission_error(modify, static_procedure, PI)`
  - 演算子は 3 層（モジュール固有 → `user` → ISO の核）。公開リストの `op(P, T, N)` は取り込み側の固有層に入る
  - 退けた案: フレームに 5 語目を足す（全フレームが 1 語太る）、読み込み時に述語の番号へ解決してフレームに入れる（後から定義した述語と `assert` が見えない）
  - Consequences: フェーズ6の `mosac` は同じ位置に文脈モジュールを書く。フェーズ7の GC はこの語を項として走査しない
- [ ] **Step 2:** iso-notes に次の問いを 1 見出し 1 問で書く（推奨は A）
  - `Section 5 Autoload`: A: ライブラリの述語は `use_module` しないと見えない B: 起動時に 6 モジュールを `user` へ取り込む
  - `Section 5 existence_error PI`: A: 文脈が `user` なら `N/A`、それ以外は `M:N/A` B: 常に `N/A`
  - `Section 5 import conflict`: A: `permission_error(import_into, procedure, N/A)` B: `permission_error(import, procedure, M:N/A)`
  - `Section 5 Module:Goal type`: A: `M` が原子でなければ `type_error(atom, M)` B: `type_error(module, M)`
  - `Section 5 Operators`: A: `meta_predicate` を `op(1150, fx)`、`as` を `op(700, xfx)` として `system` の演算子表に足す B: 足さず、宣言は `meta_predicate(...)` の形だけ受ける
  - `8.4.3 sort/2`: SPEC 第7節の段階表に ISO の `sort/2` が無い。A: 段階 G の `keysort/2` と同じく Task 12 で `system` に作る B: フェーズ5へ回す
  - `Section 7 forall/2`: 段階 G にあるが第5節の 6 モジュールの表に無い。A: `system` に作る B: 作らない
- [ ] **Step 3:** 持ち主の承認と回答を待つ。ADR の Status 行が Approved になり、7 問の Decision が書かれるまで Task 2 以降のコードを書かない
- [ ] **Step 4:** コミット `[phase4] ADR 0006 モジュールの表現と解釈の問いを書く（承認済み）`

### Task 2: 述語の鍵とフレームに文脈モジュールを足す（振る舞いは変えない）

**Files:**
- Create: `src/machine/load_directive.cpp`（`load.cpp` から指令の処理を先に移す）、`src/module/module.hpp`、`src/module/module_table.cpp`
- Modify: `src/machine/internal.hpp`、`src/machine/engine.cpp`、`src/machine/catch.cpp`、`src/machine/cut.cpp`、`src/machine/load.cpp`、`src/builtins/database.cpp`、`CMakeLists.txt`
- Test: `tests/cpp/machine_test.cpp`

**Interfaces:**
- Produces:
  - `struct Module { std::uint32_t name; std::uint32_t default_import; std::vector<PredKey> exports; std::vector<Import> imports; std::string file; };`
  - `struct PredKey { std::uint32_t module; std::uint32_t name; std::uint16_t arity; };`
  - `struct Import { std::uint32_t name; std::uint16_t arity; std::uint32_t alias; std::uint32_t from; };`（`alias` は `as` の別名。無ければ `name` と同じ）
  - `Predicate* find_pred(Impl& machine, PredKey key)`（`load.cpp` の `find_pred` と `database.cpp` の `find_predicate` をこれ 1 つにする）
  - `Predicate* resolve_pred(Impl& machine, std::uint32_t context, std::uint32_t name, std::uint16_t arity)`（定義 → 取り込み → 既定の取り込み元の順）
  - `Goal` に `std::uint32_t module`。`read_goal` と `write_goal` が ADR 0006 のビット位置で詰める
  - `Impl` に `std::vector<Module> modules`、`std::uint32_t user_module`、`std::uint32_t system_module`

- [ ] **Step 1:** `load.cpp` から指令の処理を `load_directive.cpp` へ移すだけのコミットを作る（`ctest` 30 件が通る）
- [ ] **Step 2:** 失敗するテスト `frame_module_roundtrip`: 文脈 `user`、アリティ 3 のフレームを書いて読むと両方が戻る。`frame_arity_max`: アリティ 65535 と文脈の原子番号 `0xFFFFFFFF` が互いを壊さない
- [ ] **Step 3:** 鍵とフレームを実装する。今あるすべての述語は `user` に置く。トップレベル、`consult/1`、`mosatest` の文脈は `user`
- [ ] **Step 4:** `ctest` 全件と `mosatest --suite`（合格 2028 以上）を流す
- [ ] **Step 5:** コミット `[phase4] 述語の鍵とゴールフレームに文脈モジュールを足す（ADR 0006、machine_test と ctest 全件）`

### Task 3: `Module:Goal`

**Files:**
- Create: `src/module/qualify.cpp`
- Test: `tests/pl/modules.pl`（新規）

**Interfaces:**
- Consumes: `resolve_pred`、`Goal::module`
- Produces: 組み込み `:/2`。`M:G` は `G` のフレームの文脈を `M` にして積む。`M:(A, B)` などの制御構文は中の各ゴールに `M` が届く

- [ ] **Step 1:** 失敗するテスト。`mod_qual_user`: `user:true` が成功。`mod_qual_unknown`: `nomod:true` → `existence_error(module, nomod)`。`mod_qual_var`: `_:true` → `instantiation_error`。`mod_qual_type`: `1:true` → Task 1 の Decision の Formal。`mod_qual_nested`: `user:(user:true, true)` が成功
- [ ] **Step 2:** 失敗を見る。実装する。テストを流す
- [ ] **Step 3:** コミット `[phase4] Module:Goal を文脈モジュールの切り替えとして実装（modules.pl 5 件）`

### Task 4: `module/2`、`export/1`、モジュールのファイルの `consult`

**Files:**
- Modify: `src/module/qualify.cpp`、`src/machine/load_directive.cpp`
- Create: `tests/pl/modules/家系.pl`（SPEC 第5節の例から `use_module` と `meta_predicate` を除いたもの）、`tests/pl/modules/影.pl`
- Test: `tests/pl/modules.pl`

**Interfaces:**
- Produces: 指令 `module/2`（ファイルの最初の節でなければ `permission_error(create, module, M)`）、`export/1`。モジュールのファイルを読むと、公開述語は読み込んだ側の文脈モジュールへ取り込まれる

- [ ] **Step 1:** 失敗するテスト（`modules.pl` は `:- consult('modules/家系.pl')` のように自分のディレクトリからの相対で読む。相対の基準は読み込み中のファイルのディレクトリ）
  - `mod_call_export`: `祖父(X, Z)` の解が `家系.pl` の事実どおり
  - `mod_call_private`: 非公開の `家系:内部(X)` は成功し、限定なしの `内部(X)` は `existence_error`
  - `mod_shadow`: `影.pl` の中の非公開 `名前/1` と `modules.pl` の `名前/1` が別々に呼ばれる
  - `mod_ctx_after_throw`: `catch(家系:投げる, _, true), 名前(X)` が `user` の `名前/1` を呼ぶ
  - `mod_ctx_after_redo`: `(家系:親(_, _), fail ; 名前(X))` が `user` の `名前/1` を呼ぶ
  - `mod_export_late`: `:- export(新述語/1).` で足した述語が取り込まれる
  - `mod_not_first`: 2 節目の `module/2` → `permission_error(create, module, M)`（一時ファイルは `tests/tmp/`）
- [ ] **Step 2:** 失敗を見る。実装する。テストを流す
- [ ] **Step 3:** コミット `[phase4] module/2 と export/1 とモジュールのファイルの読み込みを実装（modules.pl 7 件）`

### Task 5: `use_module/1,2`

**Files:**
- Create: `src/module/import.cpp`、`tests/pl/modules/人名.pl`
- Test: `tests/pl/modules.pl`

**Interfaces:**
- Produces: `use_module(File)`、`use_module(File, Imports)`。`Imports` は `[PI, PI as Alias, ...]` か `except([PI, ...])`。読み込みは `ensure_loaded/1` と同じく 1 回だけ（`Impl::prepared` を使う）

- [ ] **Step 1:** 失敗するテスト
  - `use_all`: `use_module('modules/人名.pl')` 後に公開述語がすべて呼べる
  - `use_alias`: `use_module('modules/人名.pl', [姓名分解/3 as 分解])` 後に `分解/3` が呼べ、`姓名分解/3` は呼べない
  - `use_except`: `except([読み/2])` で `読み/2` だけ見えない
  - `use_conflict`: 同名の述語を定義したモジュールに取り込むと Task 1 の Decision の `permission_error`
  - `use_twice`: 2 つのモジュールから同じファイルを `use_module` しても読み込みは 1 回（ファイルの `initialization` で数える）
  - `reconsult_module`: 同じモジュールのファイルを 2 回 `consult` しても `findall` の解の数が変わらない
  - `use_failed_rollback`: 構文エラーを含むモジュールのファイルの読み込み後、`current_module/1` にそのモジュールが無く、取り込みも残らない（`current_module/1` は Task 8 なので、ここは C++ の `machine_test` で `Impl::modules` を見る）
- [ ] **Step 2:** 失敗を見る。実装する。読み込みの失敗では `predicates` と同じく `modules` と取り込みを戻す
- [ ] **Step 3:** テストを流す。コミット `[phase4] use_module/1,2 を実装（as と except、衝突、二重読み込み、modules.pl 7 件）`

### Task 6: `library(...)` の探索

**Files:**
- Create: `src/platform/exe_path.cpp`、`include/mosa/exe_path.hpp`、`src/module/library_path.cpp`、`lib/README.md` を更新
- Modify: `CMakeLists.txt`（`mosa`、`mosatest`、`mosac` の POST_BUILD で `lib/` を `$<TARGET_FILE_DIR:...>/lib` へ複製）
- Test: `tests/cpp/library_path_test.cpp`（新規）、`tests/pl/modules.pl`

**Interfaces:**
- Produces: `std::optional<std::filesystem::path> executable_path()`（macOS は `_NSGetExecutablePath`、Linux は `/proc/self/exe`、Windows は `GetModuleFileNameW`）。`std::optional<std::filesystem::path> find_library(std::string_view name)`（`$MOSA_LIB` → 実行ファイルのディレクトリの `lib/`、拡張子 `.pl` を補う）

- [ ] **Step 1:** 失敗するテスト。`library_env`: `MOSA_LIB` を `tests/tmp/lib_a` にすると、そこの `x.pl` が `library(x)` で見つかる。`library_exe`: 環境変数が無いとき、実行ファイルの隣の `lib/` から見つかる。`library_missing`: `use_module(library(nosuch))` → `existence_error(source_sink, library(nosuch))`
- [ ] **Step 2:** 実装する。環境変数の読み取りも `src/platform/` に置く
- [ ] **Step 3:** 3 OS の CI で流す。コミット `[phase4] library(...) を MOSA_LIB と実行ファイルの lib/ から探す（library_path_test、3 OS）`

### Task 7: `M:` 付きの節操作と宣言

**Files:**
- Modify: `src/builtins/database.cpp`、`src/builtins/clause_term.cpp`、`src/machine/load_directive.cpp`
- Test: `tests/pl/modules.pl`

- [ ] **Step 1:** 失敗するテスト。`mod_assert_qual`: `assertz(家系:親(a, b)), 家系:親(a, b)` が成功し、`user` の `親/2` は増えない。`mod_assert_ctx`: `家系:assertz(足した(1))` が `家系` に入る。`mod_retract_qual`、`mod_clause_qual`、`mod_abolish_qual` を同じ形で。`mod_dynamic_qual`: `:- dynamic(家系:動的/1).`。`mod_multifile`: `:- multifile(家系:複/1).` を書いた別のファイルの節が `家系:複/1` に足される。`mod_current_predicate`: `current_predicate(家系:祖父/2)` が成功
- [ ] **Step 2:** 失敗を見る。実装する。節のない述語の扱いは iso-notes 7.8.3 の決定に従う
- [ ] **Step 3:** テストを流す。コミット `[phase4] 節操作と宣言を M: 付きで受ける（modules.pl 8 件）`

### Task 8: `current_module/1`、`predicate_property/2`

**Files:**
- Create: `src/builtins/module_prop.cpp`
- Test: `tests/pl/modules.pl`

**Interfaces:**
- Produces: `current_module(?atom)`、`predicate_property(?callable, ?property)`。性質は `built_in`、`dynamic`、`static`、`defined`、`exported`、`imported_from(M)`、`multifile`、`meta_predicate(Head)`、`number_of_clauses(N)` だけ

- [ ] **Step 1:** 失敗するテスト。`cm_enum`: `findall(M, current_module(M), L)` が `user`、`system`、`家系` を含む。`cm_type`: `current_module(1)` → `type_error(atom, 1)`。`pp_builtin`: `predicate_property(atom_length(_, _), built_in)`。`pp_imported`: `predicate_property(祖父(_, _), imported_from(家系))`。`pp_dynamic`、`pp_clauses`: `number_of_clauses(2)`
- [ ] **Step 2:** 実装する。テストを流す。`mosatest --suite` の `predicate_property_2` が下がっていないことを見る
- [ ] **Step 3:** コミット `[phase4] current_module/1 と predicate_property/2 を実装（modules.pl 6 件）`

### Task 9: `meta_predicate/1` と高階の組み込み

**Files:**
- Create: `src/module/meta.cpp`
- Modify: `src/machine/engine.cpp`（`enter_user` で引数を限定）、`call/N`、`findall`、`bagof`、`setof`、`catch`、`\+`、`once` が積むフレームの文脈
- Test: `tests/pl/modules.pl`

**Interfaces:**
- Produces: `Predicate` に `std::vector<MetaArg> meta`（`enum class MetaArg { None, Goal, Colon, Caret, Dcg }` と閉包の追加引数の数 0〜9）。呼び出し時に、限定の無い meta 引数を `文脈:引数` にする。`forall/2`（Task 1 で A のとき）は `system` に置く

- [ ] **Step 1:** 失敗するテスト。`meta_wrap`: `:- meta_predicate 全員(1, ?).` の述語が受けた引数が `user:G` になっている。`meta_private_closure`: 公開述語が自モジュールの非公開述語を `maplist/2` へ渡して成功する（`maplist/2` は Task 13 までは `tests/pl/modules/高階.pl` に書いた仮の meta 述語で代える）。`meta_findall_ctx`: `家系:findall(X, 内部(X), L)` が非公開述語の解を集める。`meta_catch_ctx`、`meta_call_n_ctx` を同じ形で。`meta_already`: すでに `m:G` なら二重に包まない。`pp_meta`: `predicate_property(家系:全員(_, _), meta_predicate(全員(1, ?)))`。`forall_basic`: `forall(member(X, [1, 2]), integer(X))`
- [ ] **Step 2:** 失敗を見る。実装する。テストを流す
- [ ] **Step 3:** コミット `[phase4] meta_predicate/1 と高階の組み込みの文脈を実装（modules.pl 7 件）`

### Task 10: モジュールごとの演算子

**Files:**
- Modify: `include/mosa/operator_table.hpp`、`src/read/operator_table.cpp`、`src/builtins/operators.cpp`、読み込みと書き出しが表を受け取る所
- Test: `tests/pl/modules.pl`、`tests/cpp/operator_test.cpp`

**Interfaces:**
- Produces: `class OperatorView`（固有層、`user` 層、ISO の核を順に引く。`lookup` と `entries` は `OperatorTable` と同じ型を返す）。`Reader` と `Writer` は `const OperatorView&` を受ける

- [ ] **Step 1:** 失敗するテスト。`op_local`: モジュールの中の `:- op(700, xfx, は).` が `user` で効かない。`op_exported`: 公開リストの `op(700, xfx, は)` が取り込み側で効く（`太郎 は 学生` が読める）。`op_user_global`: `user` の `op/3` がモジュールの中でも効く。`op_writeq_view`: `writeq` が文脈モジュールの演算子で書く。`current_op` が文脈の見え方を返す
- [ ] **Step 2:** 失敗を見る。実装する。`tests/pl/operators.pl` と `syntax.pl` の往復が変わらないことを見る
- [ ] **Step 3:** コミット `[phase4] 演算子をモジュールごとにし、公開と user を見せる（modules.pl 5 件、operator_test）`

### Task 11: ファイルごとの旗

**Files:**
- Modify: `src/machine/load.cpp`（`SavedFlags` を成功時にも戻す）
- Test: `tests/pl/modules.pl`

- [ ] **Step 1:** 失敗するテスト。`flag_dq_per_file`: `:- set_prolog_flag(double_quotes, atom).` を書いたファイルを読んだ後、読み手の `double_quotes` は `codes` のまま。`flag_jp_per_file`: `japanese_punctuation` も同じ。`flag_other_kept`: `unknown` などそれ以外の旗は今どおり残る
- [ ] **Step 2:** 既存の `tests/pl/flags.pl` と `directives.pl` に、ファイルを跨いで `double_quotes` が残ることを前提にしたテストがあれば、iso-notes に書いて止める（期待値を書き換えない）
- [ ] **Step 3:** 実装する。テストを流す。コミット `[phase4] double_quotes と japanese_punctuation をファイルごとに戻す（SPEC 第5節、modules.pl 3 件）`

### Task 12: `library(lists)`

**Files:**
- Create: `lib/lists.pl`、`src/builtins/lists.cpp`（`length/2`、`keysort/2`、Task 1 で A なら `sort/2`）
- Test: `tests/pl/lib_lists.pl`

**Interfaces:**
- Produces: モジュール `lists` が `append/3`、`member/2`、`memberchk/2`、`length/2`、`nth0/3`、`nth1/3`、`last/2`、`reverse/2`、`msort/2`、`sum_list/2`、`max_list/2`、`list_to_set/2` を公開する。`length/2` は `system` にあり、`lists` の公開リストに `system` の述語を書いても衝突にしない

- [ ] **Step 1:** 8.4.4.4（`keysort/2`）と 8.4.3.4（`sort/2`）の Examples を写し、ライブラリの各述語に 1 つ以上のテストを書く。`length_neg`: `length(L, -1)` → `domain_error(not_less_than_zero, -1)`。`length_type`: `length(a, N)` → `type_error(list, a)`。`length_gen`: `length(L, 2)` が `L = [_, _]`。`lists_needs_use`: Task 1 の Autoload が A なら、`use_module` 前の `append/3` は `existence_error`
- [ ] **Step 2:** 失敗を見る。`length/2` と `keysort/2` を C++ で、残りを Prolog で書く。`msort/2` は `keysort/2` に頼らずに書く（安定でなくてよいが重複を残す）
- [ ] **Step 3:** テストを流す。コミット `[phase4] library(lists) と length/2、keysort/2 を実装（8.4 の Examples と lib_lists.pl）`

### Task 13: `library(apply)`

**Files:**
- Create: `lib/apply.pl`
- Modify: Task 9 の仮の `tests/pl/modules/高階.pl` をやめ、`meta_private_closure` を `maplist/2` に戻す
- Test: `tests/pl/lib_apply.pl`

- [ ] **Step 1:** 失敗するテスト。`maplist/2..5`、`foldl/4..6`、`include/3`、`exclude/3` に各 1 つ以上。`maplist_len_diff`: `maplist(=, [a], [a, b])` が失敗。`foldl_sum`: `foldl(plus3, [1, 2, 3], 0, S)` が `S == 6`（`plus3/3` はテストファイルで定義）
- [ ] **Step 2:** すべて `meta_predicate` を宣言して Prolog で書く。テストを流す
- [ ] **Step 3:** コミット `[phase4] library(apply) を実装（lib_apply.pl、modules.pl の meta_private_closure）`

### Task 14: `library(between)`

**Files:**
- Create: `lib/between.pl`
- Test: `tests/pl/lib_between.pl`

- [ ] **Step 1:** 失敗するテスト。`between_enum`: `findall(X, between(1, 3, X), L)` が `[1, 2, 3]`。`between_inf`: `between(1, inf, 5)` が成功。`between_type`: `between(a, 3, _)` → `type_error(integer, a)`。`succ_zero`: `succ(X, 0)` が失敗。`succ_neg`: `succ(X, -1)` → `type_error(not_less_than_zero, -1)`。`succ_inst`: `succ(_, _)` → `instantiation_error`。`plus_modes`: `plus(1, X, 3)` が `X == 2`
- [ ] **Step 2:** Prolog で書く。テストを流す。`mosatest --suite` の `between_3`、`succ_2`、`plus_3` の件数を見る
- [ ] **Step 3:** コミット `[phase4] library(between) を実装（lib_between.pl）`

### Task 15: `library(format)`

**Files:**
- Create: `lib/format.pl`、`src/builtins/format_float.cpp`
- Test: `tests/pl/lib_format.pl`

**Interfaces:**
- Produces: `format/1,2,3`。指示は `~w`、`~q`、`~a`、`~d`、`~f`、`~e`、`~s`、`~c`、`~n`、`~t`、`~|`（と `~~`）だけ。`'$format_float'(+Kind, +Digits, +Float, -Atom)`（`Kind` は `f` か `e`、`std::to_chars` で書く）を `system` に置く。`~t~|` の桁は文字数で数える（東アジア幅はフェーズ5の Task 13）

- [ ] **Step 1:** 失敗するテスト。各指示に 1 つ以上。`fmt_f2`: `format("~2f", [3.14159])` → `3.14`。`fmt_e`: `format("~e", [1.0])` → `1.000000e+00`。`fmt_d_type`: `format("~d", [a])` → `format` の Formal（SWI に寄せず、iso-notes に `format/2 errors` を 1 問書いて決める）。`fmt_column`: `format("~w~t~10|~w", [ab, x])` → `ab` と空白 8 と `x`。`fmt_args_short`: 引数が足りないと `format` の Formal。`fmt_stream`: `format(user_error, "x", [])` が成功
- [ ] **Step 2:** Prolog で書く。出力は `with_output_to/2` で 1 行ずつ組み、桁を数える
- [ ] **Step 3:** テストを流す。コミット `[phase4] library(format) を実装（lib_format.pl）`

### Task 16: `library(strings)` の `atomic_list_concat/2,3` と `split_atom/4`

**Files:**
- Create: `lib/strings.pl`
- Test: `tests/pl/lib_strings.pl`

- [ ] **Step 1:** 失敗するテスト。`alc_join`: `atomic_list_concat([a, 1, 'あ'], X)` が `X == 'a1あ'`。`alc_sep`: `atomic_list_concat([a, b], '、', X)` が `X == 'a、b'`。`alc_split`: `atomic_list_concat(L, ',', 'a,b')` が `L == [a, b]`。`alc_inst`: `atomic_list_concat([a, _], X)` → `instantiation_error`。`split_atom_basic`: `split_atom('a b  c', ' ', '', L)` が `L == [a, b, '', c]`。`split_atom_pad`: `split_atom(' a ', '', ' ', L)` が `L == [a]`
- [ ] **Step 2:** Prolog で書く。テストを流す
- [ ] **Step 3:** コミット `[phase4] atomic_list_concat/2,3 と split_atom/4 を実装（lib_strings.pl）`

### Task 17: `upcase_atom/2` と大文字の表

**Files:**
- Modify: `tools/gen_unicode_tables.py`（`UnicodeData.txt` の 12 列目、単純大文字化を出す）、`src/unicode_tables.inc`（生成して commit）、`lib/strings.pl`
- Create: `src/builtins/case.cpp`（`'$upcase_atom'/2` を `system` に置き、`strings` が `upcase_atom/2` として公開する）
- Test: `tests/pl/lib_strings.pl`、`tests/cpp/char_class_test.cpp`

- [ ] **Step 1:** 失敗するテスト。`upcase_ascii`: `upcase_atom(abc, 'ABC')`。`upcase_greek`: `upcase_atom('σ', 'Σ')`。`upcase_kana_kept`: `upcase_atom('あa', 'あA')`。`upcase_sharp_s`: `upcase_atom('ß', 'ß')`（単純写像なので 1 文字のまま）。`char_class_test` に `simple_upper(U'a') == U'A'`
- [ ] **Step 2:** 表を生成し直し、`docs/unicode-version.md` の版と同じ `UnicodeData.txt` を使ったことを見る。実装する。テストを流す
- [ ] **Step 3:** コミット `[phase4] upcase_atom/2 を単純大文字化の表で実装（lib_strings.pl、char_class_test）`

### Task 18: `library(japanese)`

**Files:**
- Create: `lib/japanese.pl`、`src/builtins/char_width.cpp`（`'$char_width'(+Code, -Width)` を `k_wide_ranges` で引く）
- Test: `tests/pl/japanese.pl`

**Interfaces:**
- Produces: `ひらがな_カタカナ/2`（U+3041〜U+3096 と U+30A1〜U+30F6 を 0x60 ずらす。どちら向きにも使える）、`全角_半角/2`（U+FF01〜U+FF5E と U+0021〜U+007E、U+3000 と U+0020。半角カタカナは扱わない）、`読み幅/2`（`East_Asian_Width` が W と F なら 2、それ以外 1 の和）。フェーズ5の Task 13 は `読み幅/2` を使う

- [ ] **Step 1:** 失敗するテスト。`hk_basic`: `ひらがな_カタカナ(さくら, X)` が `X == サクラ`。`hk_reverse`: `ひらがな_カタカナ(X, サクラ)` が `X == さくら`。`hk_mixed`: `ひらがな_カタカナ('さくらA', X)` が `X == 'サクラA'`。`zh_basic`: `全角_半角('ＡＢ１', X)` が `X == 'AB1'`。`zh_space`: 全角空白が半角空白になる。`width_mixed`: `読み幅('日本a', 5)`。`width_scalar`: `読み幅('𠮷', 2)`
- [ ] **Step 2:** 実装する。テストを流す
- [ ] **Step 3:** コミット `[phase4] library(japanese) を実装（japanese.pl の hk、zh、width 7 件）`

### Task 19: 出口

**Files:**
- Modify: `tests/pl/modules.pl`、`tests/pl/modules/家系.pl`、`src/version.cpp`、`CMakeLists.txt` の `version_pass_regex`、`docs/conformance.md`、`docs/conformance.json`、`docs/STATUS.md`

- [ ] **Step 1:** SPEC 第5節の宣言の例を `use_module(library(lists))`、`use_module(人名, [姓名分解/3 as 分解])`、`except`、`meta_predicate`、`export`、`家系:祖父(太郎, Z)` まで含めて `家系.pl` に戻し、`modules.pl` の `spec_example` で `祖父(太郎, Z)` の解を確かめる
- [ ] **Step 2:** `library(lists)` を使う既存テスト（`lib_lists.pl`、`lib_apply.pl`、`modules.pl`）を含めて `ctest` 全件を 3 OS の CI で通す
- [ ] **Step 3:** `mosatest --suite` を流し、合格数が 2028 以上であることを見て、報告書と JSON を commit する
- [ ] **Step 4:** 版を `mosa 0.0.1 (phase 4)` にし、`docs/STATUS.md` を `Phase: 4 done`、`Next: phase 5` にする。コミット `[phase4] フェーズ4を終える（modules.pl 全件、合格 N 件、3 OS の CI）`
