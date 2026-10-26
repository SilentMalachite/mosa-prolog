# フェーズ3 組込述語 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 仕様第9節のフェーズ3を、アトム処理から順に足し、INRIA Suite の合格率 90% 以上と `docs/conformance.md` の自動生成で終える。

**Architecture:** 組込述語は `src/builtins/` の章ごとのファイルに置き、`run_builtin` から関数を 1 つずつ呼ぶ。節の集まりとストリームの形は、それぞれのコードより前の ADR が決める。この計画は、その 2 つの形を書かない。フラグの値は機械の非公開の対応表が持ち、読み込みと入出力はそこを見る。

**Tech Stack:** C++20、CMake 3.25 以上、CTest、標準ライブラリのみ。

**Spec:** `docs/SPEC.md` 第3節、第4節、第7節、第8節、第9節。フェーズ2で済んだ範囲は `docs/superpowers/specs/2026-10-24-phase2-engine-design.md`。

各タスクは 1 コミットで終わり、そのコミットがセッションの区切りである。次のタスクは、前のタスクの確認コマンドが通ってから始める。実装中にこの計画と `docs/SPEC.md` とフェーズ0〜2の計画と設計を書き換えない。ずれに気づいたら `docs/STATUS.md` の未解決に 1 行書いて止まり、利用者に聞く。

段階 B の算術（`is/2` と比較 6 個と評価関数）はフェーズ2で実装済みである。この計画のタスクにしない。`src/builtins/arith.cpp` と `src/builtins/arith_float.cpp` の評価式は変えない。フラグを読む分岐だけ、タスク 2 で足してよい。

## Global Constraints

- 言語は C++20。`CMAKE_CXX_EXTENSIONS` は OFF。コンパイラ拡張、`__int128`、`#pragma once` 以外の独自プラグマは使わない
- 依存は C++ 標準ライブラリと CMake だけ。新しいライブラリは追加しない
- 警告は `-Wall -Wextra -Wpedantic -Werror`。MSVC は `/W4 /WX /utf-8 /permissive- /Zc:__cplusplus`
- 生の `new` と `delete`、`reinterpret_cast`、C 配列、マクロは使わない。`#pragma once` は使ってよい
- 1 関数は 60 行以内、1 ファイルは 800 行以内。超える前に、そのタスクの Files に書いた分割へ移す
- 識別子は英語。コメント、コミット、文書は日本語。コメントは理由だけを書く。文字コードは UTF-8、BOM なし。「文字」は Unicode スカラー値 1 個
- Prolog の失敗と `throw/1` は機械の状態で表す。C++ の例外は `mosa::ResourceLimit` だけである
- `#ifdef` による OS 依存は `src/platform/` 以外に書かない。フェーズ3では `src/platform/` にファイルを足さない
- Cell のタグとビット配置は ADR 0001 と ADR 0002 のままである。中間表現は ADR 0003 のままである。整数は 64bit のままである。多倍長と GC は書かない
- 版文字列はタスク 13 の完了コミットまで `mosa 0.0.1 (phase 2)` のままである。`src/version.cpp` と `CMakeLists.txt` の `version_pass_regex` はタスク 13 まで変えない
- コミットメッセージは `[phase3] ` で始まる日本語 1 行とし、何を確認したかを含める
- 各タスクの終わりに `cmake --build build && ctest --test-dir build` を通し、`docs/STATUS.md` をそのタスクに書いた文へ置き換える。15 行を超えない
- テストを弱めない、飛ばさない、期待値を実装に合わせない。既存の往復、`nrev`、`queens`、段階 A、算術を壊さない
- `tests/cpp/harness.hpp` と `include/mosa/mosa.hpp` は変更しない。`mosa.hpp` の公開は `create`、`consult`、`query`、`next`、`halt` のまま
- エラーの Formal はヘルパで作る。呼び出し側にアトム名の文字列を直書きしない。ヘルパの引数の `string_view` は許す
- 引数検査は、その述語の規格の Errors の列挙順に行う。検査が終わる前に束縛しない
- エラーのテストは Formal だけを比べ、Context は比べない
- 入出力のテストはメモリストリームで行う。一時ファイルは `tests/tmp/` だけに作る
- 組込述語のファイル先頭コメントには、規格の条番号、引数のモード、投げうるエラー、テストファイル名を書く。モードの字面は規格から写す
- 新しい組込述語は、規格の Examples（8.x.y.4）を `tests/pl/` に写してから実装する。Examples の各行が 1 つの `test/3` である。解が複数なら `solutions([...])`、失敗なら `fail`、エラーなら `error(Formal)` とする
- この計画に完了の印は付けない。ADR の状態が未承認のあいだ、その ADR の対象のコードは書かない

## Review Focus

Examples が通っても、次の入力で壊れる。各行のテストは、書いたタスクの Step 1 に入っている。

- `atom_length(X, foo)` は、第2引数の `type_error` より先に、第1引数を見て `instantiation_error` になる。タスク 1
- `atom_length('あ', 1)` と `atom_length('𠮷', 1)` はどちらも成功する。前者をバイト数で数えると 3、後者を UTF-16 で数えると 2 になる。`sub_atom('日本語処理系', 2, 2, 2, '語処')` は成功する。タスク 1
- 節がある静的述語への `assertz/1` は `permission_error(modify, static_procedure, PI)` で、節は増えない。二度 `consult` しても解は倍にならない。タスク 4
- テキストストリームの `get_byte/1` と、バイナリストリームの `get_char/1` は `permission_error` である。`peek_char/1` のあと、同じ文字を `get_char/1` が返す。タスク 8
- `set_prolog_flag(unknown, fail)` のあと、未定義述語は失敗する。`error` に戻すと `existence_error(procedure, PI)` に戻る。フラグの変更は、そのテストの中で戻す。タスク 2

## 決めたこと

仕様とフェーズ2設計が既に書いた値である。タスクで選び直さない。

- `current_prolog_flag/2` が返す値は、`integer_rounding_function=toward_zero`、`max_integer` は 2^63−1、`min_integer` は −2^63、`max_arity=1024`、`double_quotes` の既定は `codes`、`unknown` の既定は `error`、`bounded=true`、`encoding=utf8`、`japanese_punctuation=false`、`float_overflow` / `float_zero_div` / `float_undefined` の既定は `error`、である
- 整数は 64bit のままである。オーバーフローは `evaluation_error(int_overflow)`、ゼロ除算は `evaluation_error(zero_divisor)` である
- `double_quotes` の値は `codes`、`chars`、`atom` である。既定は `codes`。リーダの `DoubleQuotes` がこの 3 つである
- `bounded`、`max_integer`、`min_integer`、`integer_rounding_function`、`max_arity`、`encoding` は `set_prolog_flag/2` で変えない。拒否の Formal は規格 8.17 の、変更できない旗のエラーである
- アトム処理はコードポイント単位である。Unicode のスカラー値 1 個を 1 文字とし、UTF-16 の符号単位では数えない。`𠮷野家` は U+20BB7、U+91CE、U+5BB6 の 3 個なので、`atom_length('𠮷野家', 3)` である。𠮷 1 文字は長さ 1、文字コードは 134071 である。アトム表はバイト列で一意化し、正規化しない。`AtomTable::ascii` が真のアトムは、`sub_atom/5` の位置をバイト位置のまま使ってよい。偽のときはコードポイント列にほどく。1 文字のアトムは `intern_char` を使ってよい
- 文字コードは Unicode スカラー値である。0〜255 に切らない。サロゲートはコードポイントにしない
- `char_conversion/2` の写像の初期状態は恒等写像である。全角から半角への変換は入れない
- ストリームの `encoding` の既定は `utf8` である
- エラー項は `error(Formal, context(PI, Extra))` である。`syntax_error` の Extra は `stream(S, Line, LinePos, CharNo)` である。`位置(Line, Col)` は使わない
- 既定のユニフィケーションは出現検査をしない。`write_term` の循環は、フェーズ2と同じく名前か `_` で止まる
- `consult/1` はフェーズ2のまま残す。同じファイルを二度読んでも、静的述語の解は倍にならない。受け付けない指示の診断 `この指示はフェーズ2では使えない: ` は、その指示を実装するタスクが受け付けるようになるまで残す
- 離れた節の診断 `述語 名前/アリティ の節が離れている` は、`discontiguous/1` の宣言が無い述語では残す
- 起動時の標準ストリームの別名は、規格 8.11 の `user_input`、`user_output`、`user_error` である
- テキストのファイル終端はアトム `end_of_file`、バイナリのファイル終端は整数 `-1` である。条は 8.12 と 8.13
- `japanese_punctuation` の値のアトムは `false` と `true` である。既定は `false`。このフェーズでは、値が `true` でも字句解析は変えない
- ファイルパスは `std::filesystem::path` で開く。テストのパスは ASCII とする。日本語ファイル名の `consult/1` は、フェーズ3の完了条件にしない
- 仕様第6節は GC を「Phase 5 まで持たない」と書き、第9節と `AGENTS.md` は GC をフェーズ7とする。この計画はフェーズ7に従う。仕様の文は直さない
- 1 つの `tests/pl/*.pl` の中では、テストのあいだ節が残る。節を足すテストは述語名を分ける。ファイルごとに機械は新しい
- スイートは `test/3` のゴールと期待を `write_term` で書き戻してから実行する。二重引用符の字面は、そのとき残らない。ストリームへ入れるテキストは、単引用のアトムか、コードのリストで書く。往復の `roundtrip.cpp` は、`Reader` の既定の `DoubleQuotes::Codes` のままにする

## 未決

仕様もフェーズ2設計も一つに決めていない。各問いは「A か B か」である。実装者は、そのタスクの最初に、まだ書いていなければ `docs/iso-notes.md` へ条番号付きで問いを 1 つ写す。答えを推測で埋めない。回答まで、その問いが指す述語か旗の本体は書かない。隣の、決まっている述語は実装する。期待値を実装に合わせて書き換えない。

1. 決定済み（2026-10-25）。`𠮷野家` の長さは Unicode のコードポイントで 3 である。決めたことと `docs/iso-notes.md` を見る。実装者は問いをやり直さない。
2. `atom_number/2`（非 ISO）。仕様は名前だけで、モードが無い。A: 片側がアトム、もう片側が数で、両方向に変換する。B: モードが無いので述語は登録しない。
3. `findall/4`（非 ISO）。仕様は名前だけで、引数の意味が無い。A: 第4引数を結果リストの末尾にする。B: 意味が無いので述語は登録しない。
4. `print/1` と `print/2`。仕様は 8.14 の並びに置くが、`portray/1` には触れない。A: `portray/1` が定義されていればそれを使い、無ければ `write/1` と同じ文字列を出す。B: `portray/1` は見ず、`write/1` と同じ文字列を出す。
5. `clause/2` と利用者の述語。仕様に `public/1` が無い。A: 利用者の述語を公開とみなし、節を返す。B: 非公開とみなし、`permission_error(access, private_procedure, PI)` を投げる。
6. `current_predicate/1` の候補。A: 組込述語も含める。B: 利用者が定義した述語だけにする。
7. 入出力テスト用のメモリストリームの述語名。仕様第8節は「`open_string`、`with_output_to/2` 相当の system 述語」とだけ書く。A: 名前を `open_string/2` と `with_output_to/2` にする。B: 別の名前にする。名前は ADR 0005 の決定が書く。この計画はどちらとも書かない。
8. 旗 `char_conversion` の既定。写像の初期状態が恒等であることは決まっている。旗の既定は書いていない。A: `off`。B: `on`。
9. 旗 `debug`。既定も、`on` のときの実行も書いていない。A: 既定 `off`。値を変えてもフェーズ3ではトレースしない。B: 既定 `on`。
10. `unknown` の値 `warning`。`error` と `fail` はタスク 2 で実装する。`warning` の文面は、メッセージ目録がフェーズ5であることとぶつかる。A: 標準エラーに `警告: ` を出してゴールを失敗させる。目録ファイルは作らない。B: `warning` は受け付けない。
11. `float_overflow`、`float_zero_div`、`float_undefined` の、`error` 以外の値のアトム名。仕様は「`error` のとき投げ、それ以外は IEEE 754 の値を返す」とだけ書く。A: 実装者がアトム名を 1 つ決めて、そのとき IEEE の値を返す。B: `error` 以外への変更は、名前が決まるまで受け付けない。この計画はアトム名を決えないので、回答が A の具体的な名前を含むまで B の側で止める。
12. 旗 `invalid_utf8`。仕様第4節は、不正な UTF-8 に `representation_error(character)` を投げるか、旗が `replace` なら U+FFFD にする、と書く。既定が無い。A: 既定 `error`。B: 既定 `replace`。
13. `set_prolog_flag(japanese_punctuation, true)`。字句解析を変えることはフェーズ5までしない。A: 値のアトムだけ `true` にする。B: 変更を拒否し、値は `false` のままにする。
14. 旗 `version` の値の形。仕様は旗の名前だけを書く。A: アトムで、`version_line()` と同じ文字列。タスク 13 より前は `mosa 0.0.1 (phase 2)`。B: 複合項にする。形が回答に含まれるまで本体は書かない。
15. 旗 `iso` の既定。仕様第3節は、逸脱が `iso` 旗 true のとき無効だと書く。既定の true か false かは書いていない。A: 既定 `true`。B: 既定 `false`。フェーズ3はモジュールと全角記号を実装しないので、既定がどちらでもその 2 つの挙動は足さない。
16. テキストファイルの改行。A: どの OS でも U+000A 1 個を書く。B: C++ のテキストモードに任せ、処理系の変換を許す。メモリストリームは、どちらの回答でも U+000A 1 個とする。

## 非目標

- 段階 F のモジュール（`module/2`、`use_module/1,2`、`current_module/1`、`predicate_property/2`、`meta_predicate/1`、`export/1`）
- 段階 G の `library(*)`。`lib/lists.pl` ほか、`format/2`、`between/3`、`succ/2`、`msort/2`、`forall/2`、`nb_getval/2` を含む
- `mosac` の変換、差分テスト（`mosatest --diff`）、ランタイム同梱
- GC、`garbage_collect/0`、多倍長整数、`BIG` タグの生成
- 全角記号モード。`japanese_punctuation` を true にしても字句解析は変えない
- `lib/messages_ja.pl` と `lib/messages_en.pl`
- Windows コンソール。`src/platform/windows.cpp` は作らない
- 同梱スイートの合格率そのもの。ファイルは置いた。90% と `conformance.md` はランナーのあと
- 日本語ファイル名の `consult/1` を完了条件にすること
- 段階 B の算術の再実装
- Cell（ADR 0001 と ADR 0002）と IR（ADR 0003）の変更
- 節データベースとストリームの具体的なデータ構造を、この計画の中で凍結すること
- `docs/SPEC.md` の矛盾をこの計画の実装中に直すこと

## 実装する述語

ここがフェーズ3の範囲である。段階 B の算術は含めない。

- 8.16: `atom_length/2` `atom_concat/3` `sub_atom/5` `atom_chars/2` `atom_codes/2` `char_code/2` `number_chars/2` `number_codes/2` `atom_number/2`
- 8.8 と 8.9: `clause/2` `current_predicate/1` `asserta/1` `assertz/1` `retract/1` `abolish/1` `retractall/1`
- 8.10: `findall/3` `findall/4` `bagof/3` `setof/3`
- 8.11: `current_input/1` `current_output/1` `set_input/1` `set_output/1` `open/3` `open/4` `close/1` `close/2` `flush_output/0` `flush_output/1` `stream_property/2` `at_end_of_stream/0` `at_end_of_stream/1` `set_stream_position/2`
- 8.12 と 8.13: `get_char/1` `get_char/2` `get_code/1` `get_code/2` `peek_char/1` `peek_char/2` `peek_code/1` `peek_code/2` `put_char/1` `put_char/2` `put_code/1` `put_code/2` `nl/0` `nl/1` `get_byte/1` `get_byte/2` `peek_byte/1` `peek_byte/2` `put_byte/1` `put_byte/2`
- 8.14: `read_term/2` `read_term/3` `read/1` `read/2` `write_term/2` `write_term/3` `write/1` `write/2` `writeq/1` `writeq/2` `write_canonical/1` `write_canonical/2` `print/1` `print/2`、ゴールとしての `op/3`、`current_op/3` `char_conversion/2` `current_char_conversion/2`
- 8.17: `set_prolog_flag/2` `current_prolog_flag/2`。`halt/0` と `halt/1` はフェーズ2で実装済みなので、タスクを分けない
- 7.4.2: `dynamic/1` `discontiguous/1` `multifile/1` `initialization/1` `include/1` `ensure_loaded/1` と `[File]`。`consult/1` はフェーズ2のまま。指示としての `op/3` はフェーズ2で受け付け済み

## ファイルの責任

| パス | 責任 | タスク |
| --- | --- | --- |
| `src/builtins/atoms.cpp` | 8.16。800 行を超える前に `atoms_number.cpp` へ数の変換を移す | 1 |
| `src/builtins/flags.cpp` | 旗の対応表、`set_prolog_flag/2`、`current_prolog_flag/2` | 2 |
| `docs/adr/0004-clause-database.md` | 節の集まりの決定。この計画は中身を決めない | 3 |
| `src/builtins/database.cpp` | 8.8 と 8.9。`dynamic/1` の指示 | 4 |
| `src/builtins/solutions.cpp` | 8.10 | 5 |
| `docs/adr/0005-streams.md` | ストリームの決定と、未決 7 の述語名。この計画は中身を決めない | 6 |
| `src/builtins/streams.cpp` | 8.11 と、ADR が名付けたメモリストリーム | 7 |
| `src/builtins/chars.cpp`、`src/builtins/bytes.cpp` | 8.12 と 8.13 | 8 |
| `src/builtins/write_pred.cpp` | 項の書き出し。循環と引用は `src/write/` を呼ぶ | 9 |
| `src/builtins/read_pred.cpp` | 項の読み込み。リーダをストリームの上で呼ぶ | 10 |
| `src/builtins/operators.cpp` | ゴールとしての `op/3`、`current_op/3`、`char_conversion/2`、`current_char_conversion/2` | 11 |
| `src/machine/load.cpp` | 残りの指示、`[File]`、`include/1`、`ensure_loaded/1` | 12 |
| `src/machine/suite.cpp`、`src/main_mosatest.cpp` | `mosatest --suite` と `docs/conformance.md` | 13 |
| `src/version.cpp`、`CMakeLists.txt` の版の正規表現 | タスク 13 の完了時だけ `phase 3` | 13 |

`run_builtin` は、既存の `run_arith` のあとで、実装済みの `run_*` だけを呼ぶ。呼ぶ順は `run_atoms`、`run_flags`、`run_database`、`run_solutions`、`run_streams`、`run_char_io`、`run_byte_io`、`run_write`、`run_read`、`run_operators` である。どれも `bool run_*(Impl& machine, const Goal& goal)` で、扱ったゴールなら true を返す。

`tests/pl/` の新しいファイルは、既存の `prolog_tests` がディレクトリごと実行する。`syntax.pl` は往復のまま、`test/3` を足さない。CTest の登録を二重にしない。

## ビルドの足し方

`enable_testing()` より前に、そのタスクのソースだけを足す。

```cmake
target_sources(mosa_lib PRIVATE src/builtins/atoms.cpp)
```

公開ヘルパは `include/mosa/errors.hpp` に宣言を足す。実体は `src/builtins/errors.cpp`。内部宣言は `src/machine/internal.hpp`。

```cpp
[[nodiscard]] Cell make_permission_error(Machine& machine, std::string_view action,
                                         std::string_view type, Cell culprit);
[[nodiscard]] Cell make_representation_error(Machine& machine, std::string_view which);
[[nodiscard]] Cell make_syntax_error(Machine& machine, Cell detail);
[[nodiscard]] Cell make_resource_error(Machine& machine, std::string_view which);
[[nodiscard]] Cell make_system_error(Machine& machine);
```

`formal_representation` は既にある。公開の `make_representation_error` は、最初に投げるタスクが足す。使わないヘルパは足さない。`make_system_error` は、規格の Errors に `system_error` が出る述語を実装するタスクだけが足す。

---

### Task 1: アトム処理

**Files:**
- Create: `src/builtins/atoms.cpp`、`tests/pl/atoms.pl`、`tests/pl/japanese.pl`
- Modify: `src/machine/engine.cpp`、`src/machine/internal.hpp`、`CMakeLists.txt`、`docs/STATUS.md`、`docs/iso-notes.md`
- Test: `tests/pl/atoms.pl`、`tests/pl/japanese.pl`

**Interfaces:**
- Consumes: `AtomTable::intern`、`AtomTable::intern_char`、`AtomTable::ascii`、`AtomTable::name`、`make_instantiation_error`、`make_type_error`、`make_domain_error`、`throw_error`、既存のリーダ（数の字句）
- Produces: `bool run_atoms(Impl& machine, const Goal& goal)`。8.16 の、未決 2 を除く述語

最初の実装タスクはこれである。未決 2 の `atom_number/2` は、回答まで登録しない。`atom_number/2` という名前は、このタスクの対象一覧に残す。

- [ ] **Step 1: 失敗するテストを書く**

`tests/pl/atoms.pl` には、8.16.1 から 8.16.8 の Examples を 1 行 1 テストで写す。`tests/pl/japanese.pl` には次を書く。

```prolog
test(atom_length_hiragana, atom_length('あ', 1), true).
test(atom_length_scalar, atom_length('𠮷', 1), true).
test(atom_length_yoshinoya, atom_length('𠮷野家', 3), true).
test(char_code_scalar, char_code('𠮷', 134071), true).
test(sub_atom_go_sho, sub_atom('日本語処理系', 2, 2, 2, '語処'), true).
test(atom_length_arg_order, atom_length(X, foo), error(instantiation_error)).
```

`134071` は U+20BB7 である。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が、未定義の `atom_length/2` で失敗する

- [ ] **Step 3: `run_atoms` を実装する**

コードポイント単位で数える。`atom_length(X, foo)` は第1引数が変数なので `instantiation_error` であり、第2引数の `foo` を先に見ない。検査が終わる前に束縛しない。数の変換は、フェーズ1の数の字句を再利用する。新しい数の構文は足さない。ファイルが 800 行を超える前に `atoms_number.cpp` へ `number_chars/2` と `number_codes/2` を移す。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。往復、`nrev`、`queens`、段階 A、算術を含む。`./build/mosa --version` は `mosa 0.0.1 (phase 2)`

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: アトム処理がコードポイント単位で動く。𠮷野家の長さは 3。atom_length(X, foo) は instantiation_error
次: フラグ（set_prolog_flag/2 と current_prolog_flag/2）
未解決: 未決 2（atom_number/2）
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

未決 2 が回答済みなら、未解決は `なし` とする。

- [ ] **Step 6: Commit**

```bash
git add src/builtins/atoms.cpp src/machine/engine.cpp src/machine/internal.hpp tests/pl/atoms.pl tests/pl/japanese.pl CMakeLists.txt docs/STATUS.md docs/iso-notes.md
git commit -m "[phase3] アトム処理を追加する（引数順とコードポイントが通る）"
```

`atoms_number.cpp` を作ったときは、同じコミットに含める。

---

### Task 2: フラグ

**Files:**
- Create: `src/builtins/flags.cpp`、`tests/pl/flags.pl`
- Modify: `src/machine/engine.cpp`、`src/machine/load.cpp`、`src/machine/session.cpp`、`src/machine/internal.hpp`、`src/builtins/arith.cpp`、`src/builtins/arith_float.cpp`、`CMakeLists.txt`、`docs/STATUS.md`、`docs/iso-notes.md`
- Test: `tests/pl/flags.pl`

**Interfaces:**
- Consumes: タスク 1 の `run_builtin` の呼び順、既存の `existence_error(procedure, PI)`
- Produces: `bool run_flags(Impl& machine, const Goal& goal)` と `DoubleQuotes current_double_quotes(const Impl& machine)`。後者は `src/machine/internal.hpp` に宣言する

旗の対応表は `Impl` の非公開メンバに置く。公開の型は足さない。評価関数の式は変えない。`float_overflow` が `error` のときは、今と同じエラーを投げる。

- [ ] **Step 1: 失敗するテストを書く**

`tests/pl/flags.pl` に、決まっている旗の値を書く。

```prolog
test(flag_bounded, current_prolog_flag(bounded, true), true).
test(flag_rounding, current_prolog_flag(integer_rounding_function, toward_zero), true).
test(flag_max_integer, current_prolog_flag(max_integer, 9223372036854775807), true).
test(flag_min_integer, (current_prolog_flag(min_integer, X), Y is -9223372036854775807 - 1, X =:= Y), true).
test(flag_max_arity, current_prolog_flag(max_arity, 1024), true).
test(flag_double_quotes, current_prolog_flag(double_quotes, codes), true).
test(flag_unknown, current_prolog_flag(unknown, error), true).
test(flag_encoding, current_prolog_flag(encoding, utf8), true).
test(flag_ja_punct, current_prolog_flag(japanese_punctuation, false), true).
test(flag_float_overflow, current_prolog_flag(float_overflow, error), true).
test(flag_float_zero_div, current_prolog_flag(float_zero_div, error), true).
test(flag_float_undefined, current_prolog_flag(float_undefined, error), true).
test(unknown_fail, (set_prolog_flag(unknown, fail), no_such_pred_phase3), fail).
test(unknown_restored, (set_prolog_flag(unknown, error), no_such_pred_phase3), error(existence_error(procedure, no_such_pred_phase3/0))).
```

`9223372036854775807` は 2^63−1 である。−2^63 は、正のリテラル `9223372036854775808` が int64 に入らないので、`−9223372036854775807 − 1` で作る。この 2 つの `unknown` のテストはファイルの末尾で、この順にする。`fail` の期待は、例外と区別できる。`catch` で例外を失敗に変えない。未決 8、9、10、11、12、13、14、15 の旗は、回答まで期待値を書かない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が `current_prolog_flag/2` の未定義で失敗する

- [ ] **Step 3: `run_flags` を実装する**

「決めたこと」の値を返す。`current_double_quotes` は、旗 `double_quotes` の現在値を返す。既定は `DoubleQuotes::Codes`。`Reader` を作る `load.cpp`、`engine.cpp` のクエリ、`session.cpp` のプロンプトは、この関数の値を渡す。`roundtrip.cpp` と C++ のリーダテストは、引数を足さず既定のままにする。`unknown` が `error` のときはフェーズ2と同じ `existence_error`、`fail` のときはゴールを失敗させる。`warning` は未決 10 の回答まで登録しない。変更できない旗への `set_prolog_flag/2` は、規格 8.17 の Formal を投げる。指示 `:- set_prolog_flag(旗, 値).` も同じ手続きで受ける。未決の旗の名前は、回答まで候補に入れない。

算術は、`float_overflow`、`float_zero_div`、`float_undefined` が `error` のときだけ、今の `evaluation_error` を投げる。それ以外の値は未決 11 の回答まで受け付けない。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: 決まっている旗が読める。unknown=fail は失敗、error に戻すと existence_error
次: 節データベースの ADR
未解決: 未決 8 から 15 のうち、まだ回答が無いもの
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/flags.cpp src/builtins/arith.cpp src/builtins/arith_float.cpp src/machine/engine.cpp src/machine/load.cpp src/machine/session.cpp src/machine/internal.hpp tests/pl/flags.pl CMakeLists.txt docs/STATUS.md docs/iso-notes.md
git commit -m "[phase3] 旗を追加する（toward_zero と unknown=fail が通る）"
```

---

### Task 3: 節データベースの ADR

**Files:**
- Create: `docs/adr/0004-clause-database.md`
- Modify: `docs/STATUS.md`
- Test: なし。`src/` は変更しない

**Interfaces:**
- Consumes: フェーズ2の `Predicate` と `Clause`（`src/machine/internal.hpp`）。形は変えない
- Produces: 未承認の ADR。タスク 4 は、状態が未承認のあいだコードを書かない

このタスクは表現を決めない。決定の本文は実装者が ADR に書く。この計画へは書かない。

- [ ] **Step 1: ファイルが無いことを確認する**

Run: `test ! -f docs/adr/0004-clause-database.md`
Expected: 終了コード 0

- [ ] **Step 2: ADR を書く**

状態の行は「未承認」とする。背景、決定、捨てた案、影響を、各 3 行以内で書く。決定は次の問いへの答えである。答えはこの計画に書かない。

- 動的な節を、既存の `Predicate::clauses` のどこに持つか
- `asserta/1` と `assertz/1` が、実行中の選択点の候補添字をどうずらすか
- `retract/1` が、今実行している節の残りのゴールを消すか残すか
- 追加した節を、いつ IR にするか
- `abolish/1` が述語を消したあと、同じ名前を静的な定義が再び使えるか
- 未決 5 と未決 6 を、ADR の中でどちらかに決めない。それらは `docs/iso-notes.md` の問いのまま残す

`src/` と `tests/` は変更しない。

- [ ] **Step 3: ビルドが壊れていないことを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 4: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: 節データベースの ADR を未承認で置いた
次: ADR の承認を待つ。承認まで節の改変コードは書かない
未解決: ADR 0004 の承認待ち
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 5: Commit**

```bash
git add docs/adr/0004-clause-database.md docs/STATUS.md
git commit -m "[phase3] 節データベースの ADR を未承認で置く"
```

承認が付くまでタスク 4 に進まない。承認は、利用者が ADR の状態の行を書き換えたことで足りる。この計画の文面は変えない。

---

### Task 4: 節の検索と改変

**Files:**
- Create: `src/builtins/database.cpp`、`tests/pl/database.pl`
- Modify: `src/machine/engine.cpp`、`src/machine/internal.hpp`、`src/machine/load.cpp`、`include/mosa/errors.hpp`、`src/builtins/errors.cpp`、`CMakeLists.txt`、`docs/STATUS.md`、`docs/iso-notes.md`
- Test: `tests/pl/database.pl`

**Interfaces:**
- Consumes: 承認された ADR 0004 の型。未決 5 と未決 6 が回答済みであること
- Produces: `bool run_database(Impl& machine, const Goal& goal)`。`make_permission_error(Machine&, string_view action, string_view type, Cell culprit)`

ADR の状態が未承認のあいだは、Step 1 も書かずに止まる。未決 5 が未回答なら `clause/2` の本体を書かない。未決 6 が未回答なら `current_predicate/1` の本体を書かない。`asserta/1`、`assertz/1`、`retract/1`、`abolish/1`、`retractall/1` は、規格 8.9 の Errors が静的述語への `permission_error(modify, static_procedure, PI)` を決めているので実装する。

`dynamic/1` は、8.9 の Examples が指示として使う。このタスクで `:- dynamic` を読む。`discontiguous/1`、`multifile/1`、`initialization/1`、`include/1`、`ensure_loaded/1`、`[File]` はタスク 12 まで受け付けない。

- [ ] **Step 1: 失敗するテストを書く**

8.8 と 8.9 の Examples を `tests/pl/database.pl` に写す。加えて次を書く。

```prolog
frozen(1).
test(assertz_static, assertz(frozen(2)), error(permission_error(modify, static_procedure, frozen/1))).
test(assertz_static_keeps, frozen(X), solutions([X = 1])).
```

二度 `consult` しても解が倍にならない既存テストは、期待値を変えない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が `assertz/1` の未定義で失敗する

- [ ] **Step 3: `run_database` を実装する**

ADR 0004 の型だけを使う。この計画に書いた別の形へ置き換えない。検査が終わる前に節を足さない。静的述語への追加と削除は `permission_error(modify, static_procedure, PI)` で、節の本数は変えない。`:- dynamic` が付いた述語だけを追加と削除の対象にする。選択点が残っている述語の置き換えは、フェーズ2と同じく失敗させる。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: 動的な節の追加と削除が動く。静的述語への assertz は permission_error
次: すべての解（findall/3、bagof/3、setof/3）
未解決: なし
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

未決 5 か 6 が残っていれば、未解決にその番号を書く。

- [ ] **Step 6: Commit**

```bash
git add src/builtins/database.cpp src/builtins/errors.cpp include/mosa/errors.hpp src/machine/engine.cpp src/machine/internal.hpp src/machine/load.cpp tests/pl/database.pl CMakeLists.txt docs/STATUS.md docs/iso-notes.md
git commit -m "[phase3] 節の追加と削除を追加する（静的述語の permission_error が通る）"
```

---

### Task 5: すべての解

**Files:**
- Create: `src/builtins/solutions.cpp`、`tests/pl/solutions.pl`
- Modify: `src/machine/engine.cpp`、`src/machine/internal.hpp`、`CMakeLists.txt`、`docs/STATUS.md`、`docs/iso-notes.md`
- Test: `tests/pl/solutions.pl`

**Interfaces:**
- Consumes: 既存の `copy_term/2` の複製、フェーズ2の標準順序、タスク 4 の動的述語
- Produces: `bool run_solutions(Impl& machine, const Goal& goal)`。`findall/3`、`bagof/3`、`setof/3`。`findall/4` は未決 3 の回答まで登録しない

- [ ] **Step 1: 失敗するテストを書く**

8.10 の Examples を `tests/pl/solutions.pl` に写す。`bagof/3` と `setof/3` の `^/2` の例も写す。`setof/3` の解の順は、フェーズ2の標準順序である。`findall/4` のテストは、未決 3 の回答まで置かない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が `findall/3` の未定義で失敗する

- [ ] **Step 3: `run_solutions` を実装する**

解のテンプレートは、各解の時点で複製する。ゴールが例外を投げたら、リストを作らずその例外を届ける。`setof/3` は標準順序で並べ、同じ項は 1 つにする。`bagof/3` は並べ替えない。`^/2` で隠した変数は、解を分ける鍵にしない。検査が終わる前に、呼び出し側の変数を束縛しない。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: findall/3 と bagof/3 と setof/3 が動く
次: ストリームの ADR
未解決: 未決 3 が未回答ならその番号
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/solutions.cpp src/machine/engine.cpp src/machine/internal.hpp tests/pl/solutions.pl CMakeLists.txt docs/STATUS.md docs/iso-notes.md
git commit -m "[phase3] findall と bagof と setof を追加する（Examples が通る）"
```

---

### Task 6: ストリームの ADR

**Files:**
- Create: `docs/adr/0005-streams.md`
- Modify: `docs/STATUS.md`
- Test: なし。`src/` は変更しない

**Interfaces:**
- Consumes: 仕様第6節の「ファイル、標準入出力、メモリ」という区分だけ。クラスの形は仕様が決めていない
- Produces: 未承認の ADR。未決 7 の述語名は、この ADR の決定が書く

- [ ] **Step 1: ファイルが無いことを確認する**

Run: `test ! -f docs/adr/0005-streams.md`
Expected: 終了コード 0

- [ ] **Step 2: ADR を書く**

状態の行は「未承認」とする。背景、決定、捨てた案、影響を、各 3 行以内で書く。決定は次の問いへの答えである。答えはこの計画に書かない。

- ファイル、標準入出力、メモリの 3 つを、どの型で持つか
- 現在の入力と出力を、機械のどこに持つか
- 未決 7 のメモリストリーム述語の名前と引数
- `set_stream_position/2` の位置の項の形
- `peek_char/1` が読んだ文字を、どこに 1 文字分残すか
- 未決 16 の、ファイルの改行。メモリストリームは U+000A 1 個、と決定に書く

`src/` と `tests/` は変更しない。

- [ ] **Step 3: ビルドが壊れていないことを確認する**

Run: `cmake --build build && ctest --test-dir build`
Expected: 全部成功。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 4: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: ストリームの ADR を未承認で置いた
次: ADR の承認を待つ。承認まで open/3 は書かない
未解決: ADR 0005 の承認待ち
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 5: Commit**

```bash
git add docs/adr/0005-streams.md docs/STATUS.md
git commit -m "[phase3] ストリームの ADR を未承認で置く"
```

承認が付くまでタスク 7 に進まない。

---

### Task 7: ストリームの選択と制御

**Files:**
- Create: `src/builtins/streams.cpp`、`tests/pl/streams.pl`
- Modify: `src/machine/engine.cpp`、`src/machine/internal.hpp`、`include/mosa/errors.hpp`、`src/builtins/errors.cpp`、`CMakeLists.txt`、`docs/STATUS.md`
- Test: `tests/pl/streams.pl`

**Interfaces:**
- Consumes: 承認された ADR 0005。未決 7 の名前は ADR の決定のものを使う
- Produces: `bool run_streams(Impl& machine, const Goal& goal)`。8.11 の述語と、ADR が名付けたメモリストリームの system 述語

ADR が未承認のあいだはコードを書かない。

- [ ] **Step 1: 失敗するテストを書く**

8.11 の Examples を `tests/pl/streams.pl` に写す。ファイルを作る例は、ADR のメモリストリーム述語へ置き換えてよい。置き換えない例だけ `tests/tmp/` にファイルを作る。`tests/tmp/` 以外には作らない。期待値の Formal は Examples のままにする。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が `open/3` または `current_input/1` の未定義で失敗する

- [ ] **Step 3: `run_streams` を実装する**

ADR 0005 の型だけを使う。起動時の別名は `user_input`、`user_output`、`user_error` である。`encoding` の既定は `utf8` である。メモリストリームの既定の type は text である。これは `open/3` の既定と同じであり、バッファの形は ADR が決める。閉じたストリームへの操作は、規格 8.11 の Errors の順で失敗させる。`open/3` と `open/4` の選択肢は、規格 8.11 が列挙するものと、仕様が足す `encoding` だけである。仕様に無い選択肢を成功させない。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。版は `mosa 0.0.1 (phase 2)`。`tests/tmp/` 以外に新しいファイルが残っていない

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: open と現在ストリームが動く。テストはメモリストリーム
次: 文字とバイトの入出力
未解決: なし
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/streams.cpp src/builtins/errors.cpp include/mosa/errors.hpp src/machine/engine.cpp src/machine/internal.hpp tests/pl/streams.pl CMakeLists.txt docs/STATUS.md
git commit -m "[phase3] ストリームの選択を追加する（メモリストリームの Examples が通る）"
```

---

### Task 8: 文字とバイトの入出力

**Files:**
- Create: `src/builtins/chars.cpp`、`src/builtins/bytes.cpp`、`tests/pl/chars.pl`、`tests/cpp/stream_test.cpp`
- Modify: `src/machine/engine.cpp`、`src/machine/internal.hpp`、`CMakeLists.txt`、`docs/STATUS.md`、`docs/iso-notes.md`
- Test: `tests/pl/chars.pl`、`tests/cpp/stream_test.cpp`

**Interfaces:**
- Consumes: タスク 7 のストリーム型とメモリストリーム述語、`current_double_quotes` はまだ使わない
- Produces: `bool run_char_io(Impl& machine, const Goal& goal)` と `bool run_byte_io(Impl& machine, const Goal& goal)`

- [ ] **Step 1: 失敗するテストを書く**

8.12 と 8.13 の Examples を `tests/pl/chars.pl` に写す。ストリームは ADR のメモリストリーム述語で作る。加えて次を書く。述語名は ADR の決定に合わせ、引数の並びだけ変えない。下の `open_string` は、ADR が別の名前ならその名前に置き換える。

```prolog
test(peek_does_not_consume,
     (open_string('あ', S), peek_char(C1, S), get_char(C2, S), C1 == C2, C1 == あ),
     true).
test(get_byte_on_text, (open_string('a', S), get_byte(_, S)), error(permission_error(input, text_stream, _))).
```

`open_string` は、ADR が決めたメモリストリーム述語の名前に置き換える。引数がアトムでなければ、同じ文字のコードリストにする。二重引用は使わない。Formal が規格 8.12 と 8.13 の Errors と違えば、規格の Formal に合わせ、第3引数がストリームなら変数のままにする。未決 12 が未回答なら、不正な UTF-8 のテストは置かない。

バイナリストリームの `get_char` は `tests/cpp/stream_test.cpp` で確認する。空のファイルを `tests/tmp/phase3-binary.bin` に作り、`open/4` の絶対パスで開く。絶対パスは `MOSA_SOURCE_DIR` から作る。定義は `session_test` と同じにする。作業ディレクトリには依存しない。`get_char` の結果が例外であり、Formal が規格 8.12 のバイナリストリームの `permission_error` であることを、`write_term` した Formal の文字列で確認する。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が `get_char/1` の未定義で失敗する

- [ ] **Step 3: `run_char_io` と `run_byte_io` を実装する**

文字はコードポイント単位である。`peek_char/1` と `peek_char/2` と `peek_code/1` と `peek_code/2` と `peek_byte/1` と `peek_byte/2` は、ストリーム位置を進めない。テキストへのバイト操作と、バイナリへの文字操作は、規格 8.12 と 8.13 の `permission_error` である。ファイル終端は、テキストが `end_of_file`、バイナリが `-1` である。`nl/0` と `nl/1` は、メモリストリームへ U+000A 1 個を書く。`stream_test` は `session_test` と同じ `MOSA_SOURCE_DIR` を付け、`add_test` で登録する。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: 文字とバイトの入出力が動く。peek は位置を進めない
次: 項の書き出し
未解決: なし
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/chars.cpp src/builtins/bytes.cpp src/machine/engine.cpp src/machine/internal.hpp tests/pl/chars.pl tests/cpp/stream_test.cpp CMakeLists.txt docs/STATUS.md docs/iso-notes.md
git commit -m "[phase3] 文字とバイトの入出力を追加する（peek が通る）"
```

---

### Task 9: 項の書き出し

**Files:**
- Create: `src/builtins/write_pred.cpp`、`tests/pl/write.pl`
- Modify: `src/machine/engine.cpp`、`src/machine/internal.hpp`、`include/mosa/writer.hpp`、`src/write/writer.cpp`、`CMakeLists.txt`、`docs/STATUS.md`、`docs/iso-notes.md`
- Test: `tests/pl/write.pl`

**Interfaces:**
- Consumes: 既存の `write_term` と `WriteOptions`、タスク 7 の出力ストリーム
- Produces: `bool run_write(Impl& machine, const Goal& goal)`。`write_term/2`、`write_term/3`、`write/1`、`write/2`、`writeq/1`、`writeq/2`、`write_canonical/1`、`write_canonical/2`。`print/1` と `print/2` は未決 4 の回答まで登録しない

`WriteOptions` に足してよい欄は、規格 8.14.3 と Corrigenda が列挙する書き出しオプションだけである。仕様が名指しする `quoted` は、既存の `WriteOptions::quoted` を使う。

- [ ] **Step 1: 失敗するテストを書く**

8.14 の書き出しの Examples を `tests/pl/write.pl` に写す。出力の比較は、ADR のメモリストリームを読み戻して行う。加えて次を書く。

```prolog
test(write_kanji, (with_output_to(atom(A), write(東)), A == 東), true).
test(writeq_space, (with_output_to(atom(A), writeq('東 京')), atom_chars(A, ['\'', 東, ' ', 京, '\''])), true).
test(writeq_bare, (with_output_to(atom(A), writeq(東京)), A == 東京), true).
```

`with_output_to` の名前と、出力をアトムとして受け取る引数は、ADR 0005 に合わせる。`write(東)` の出力アトムは `東` である。`writeq(東京)` の出力アトムは `東京` である。`writeq('東 京')` の出力アトムは、引用符、`東`、空白、`京`、引用符の 5 文字である。二重引用は使わない。`WriteOptions` の既定値は変えない。`write/1` は呼び出しが `quoted` を偽にして渡す。往復の `writer_test` と `syntax.pl` の期待値は変えない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が `write/1` の未定義で失敗する。`write/1` がフェーズ2で未定義であることは、ゴールとして呼んで失敗することで確認する

- [ ] **Step 3: `run_write` を実装する**

`writeq/1` と `writeq/2` は `quoted` を真にする。`write_canonical/1` と `write_canonical/2` は `ignore_ops` を真にし、`quoted` を真にする。`write/1` と `write/2` は演算子を使い、引用しない。循環項はフェーズ2のライタのまま止まり、戻ってこないままにしない。漢字 1 文字は引用しない。空白を含むアトムは `writeq` で引用する。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。往復を含む。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: write と writeq と write_canonical が動く。writeq(東京) は引用しない
次: 項の読み込み
未解決: 未決 4 が未回答ならその番号
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/write_pred.cpp src/write/writer.cpp include/mosa/writer.hpp src/machine/engine.cpp src/machine/internal.hpp tests/pl/write.pl CMakeLists.txt docs/STATUS.md docs/iso-notes.md
git commit -m "[phase3] 項の書き出しを追加する（writeq の引用が通る）"
```

---

### Task 10: 項の読み込み

**Files:**
- Create: `src/builtins/read_pred.cpp`、`tests/pl/read.pl`
- Modify: `src/machine/engine.cpp`、`src/machine/internal.hpp`、`CMakeLists.txt`、`docs/STATUS.md`、`docs/iso-notes.md`
- Test: `tests/pl/read.pl`

**Interfaces:**
- Consumes: 既存の `Reader`、タスク 2 の `current_double_quotes`、タスク 7 の入力ストリーム
- Produces: `bool run_read(Impl& machine, const Goal& goal)`。`read_term/2`、`read_term/3`、`read/1`、`read/2`

- [ ] **Step 1: 失敗するテストを書く**

8.14.1 と 8.14.2 の Examples を `tests/pl/read.pl` に写す。入力はメモリストリームである。ストリームへ書く文字は単引用のアトムかコードのリストにし、テストファイルの二重引用にしない。仕様が名指しする `variable_names`、`singletons`、`quoted` を、Examples に無い場合でも 1 テストずつ足す。`double_quotes` が既定の `codes` のとき、ストリーム上の 2 文字の二重引用（コード 34、97、98、34）を `read_term` が `[97,98]` として読むことを 1 テストにする。`chars` に変えたテストは、同じテストの中で `codes` に戻す。

受け付けるオプションは、規格 8.14.1 と Corrigenda が列挙するものと、仕様が名指しする `quoted` だけである。それ以外は、規格の read_option のドメインエラーにする。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が `read/1` の未定義で失敗する

- [ ] **Step 3: `run_read` を実装する**

ストリームから、項が閉じるまでコードポイント単位で読む。`Reader` には `current_double_quotes` の値を渡す。構文エラーの Formal は `syntax_error/1`、Context の Extra は `stream(S, Line, LinePos, CharNo)` である。テストは Formal だけを比べる。列はコードポイント単位である。`make_syntax_error` が無ければ、このタスクで足す。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。往復を含む。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: read_term がメモリストリームから読める。double_quotes の既定は codes
次: ゴールとしての op/3 と char_conversion/2
未解決: なし
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/read_pred.cpp src/builtins/errors.cpp include/mosa/errors.hpp src/machine/engine.cpp src/machine/internal.hpp tests/pl/read.pl CMakeLists.txt docs/STATUS.md docs/iso-notes.md
git commit -m "[phase3] 項の読み込みを追加する（double_quotes=codes が通る）"
```

---

### Task 11: 演算子と文字写像

**Files:**
- Create: `src/builtins/operators.cpp`、`tests/pl/operators.pl`
- Modify: `src/machine/engine.cpp`、`src/machine/internal.hpp`、`src/machine/load.cpp`、`CMakeLists.txt`、`docs/STATUS.md`、`docs/iso-notes.md`
- Test: `tests/pl/operators.pl`

**Interfaces:**
- Consumes: 既存の `OperatorTable::define` と `OperatorTable::lookup`、タスク 2 の旗 `char_conversion`（未決 8 が回答済みであること）
- Produces: `bool run_operators(Impl& machine, const Goal& goal)`。ゴールとしての `op/3`、`current_op/3`、`char_conversion/2`、`current_char_conversion/2`

指示としての `:- op(優先度, 指定子, 名前).` はフェーズ2のまま残す。優先度 0 が組を消すことも残す。このタスクは、同じ `define` をゴールから呼ぶ。

- [ ] **Step 1: 失敗するテストを書く**

8.14 の `op/3`、`current_op/3`、`char_conversion/2`、`current_char_conversion/2` の Examples を `tests/pl/operators.pl` に写す。ゴールとしての `op/3` が、その後の `read_term` で効くことを 1 テストにする。未決 8 が未回答なら、旗が写像を無効にするテストは置かない。写像が恒等のとき、変換を登録する前の `current_char_conversion/2` が文字を自分自身にだけ結び付けることを 1 テストにする。全角から半角への初期変換は書かない。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が、ゴールとしての `op/3` の未定義で失敗する

- [ ] **Step 3: `run_operators` を実装する**

`op/3` は `OperatorTable::define` の拒否を、フェーズ2の読み込みと同じ理由で失敗させる。表は拒否のとき変えない。`char_conversion/2` は 1 文字のアトムだけを受ける。文字の同一性はコードポイントで見る。初期写像は恒等である。指示 `:- char_conversion(来, 先).` も同じ手続きで受ける。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。往復を含む。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: ゴールとしての op/3 と char_conversion/2 が動く
次: 残りのディレクティブ
未解決: なし
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 6: Commit**

```bash
git add src/builtins/operators.cpp src/machine/engine.cpp src/machine/internal.hpp src/machine/load.cpp tests/pl/operators.pl CMakeLists.txt docs/STATUS.md docs/iso-notes.md
git commit -m "[phase3] ゴールとしての op と char_conversion を追加する（Examples が通る）"
```

---

### Task 12: 残りのディレクティブ

**Files:**
- Create: `tests/pl/directives.pl`
- Modify: `src/machine/load.cpp`、`src/machine/consult.cpp`、`CMakeLists.txt`、`docs/STATUS.md`、`docs/iso-notes.md`
- 800 行を超えたときだけ Create: `src/machine/directive.cpp`
- Test: `tests/pl/directives.pl`

**Interfaces:**
- Consumes: タスク 4 の `dynamic/1`、タスク 11 の `op/3` と `char_conversion/2`、タスク 2 の `set_prolog_flag/2`、フェーズ2の `consult/1`
- Produces: 読み込みが `discontiguous/1`、`multifile/1`、`initialization/1`、`include/1`、`ensure_loaded/1`、`[File]` を受ける

`dynamic/1` はタスク 4 で受け付け済みである。このタスクで二重に定義しない。`consult/1` の「二度読むと置き換わる」は、`multifile/1` の宣言が無い述語では残す。`load.cpp` が 800 行を超える前に、指示の処理を `src/machine/directive.cpp` へ移す。

- [ ] **Step 1: 失敗するテストを書く**

`tests/pl/directives.pl` に、7.4.2 の振る舞いを 1 指示 1 テストで書く。ファイルが要るテストは `tests/tmp/` にだけ作る。

- `discontiguous/1` が無い述語の離れた節は、診断 `述語 名前/アリティ の節が離れている` のまま失敗する
- `discontiguous/1` がある述語は、間に別の述語があっても読み込める
- `multifile/1` の述語は、次の `consult/1` が節を足し、フェーズ2の置き換えをしない
- `multifile/1` が無い述語は、次の `consult/1` で置き換わり、解は倍にならない
- `initialization/1` のゴールは、ファイルを読み終わったあと、ソースの順に実行する
- `include/1` の節は、読み込み中のファイルの節として連続する
- `ensure_loaded/1` は、同じパスを二度処理しても節を倍にしない
- `[File]` は、リストの各アトムを `consult/1` と同じ手続きで読む

モジュール限定の `モジュール:PI` は書かない。フェーズ4までである。`initialization/1` の失敗時の Formal が規格で一つに決まらなければ、その 1 指示だけ `docs/iso-notes.md` に「A か B か」を書いて止め、他の指示は実装する。

- [ ] **Step 2: 失敗を確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: `prolog_tests` が、`discontiguous/1` をまだ拒む診断で失敗する

- [ ] **Step 3: `load.cpp` を拡張する**

受け付ける指示を増やす。フェーズ2が受け付けていた `:- op` は残す。宣言の無い離れた節の診断文は変えない。`include/1` と `ensure_loaded/1` と `[File]` のパスは `std::filesystem::path` で開く。開けないときは、フェーズ2の `consult/1` と同じく診断 `ファイルを開けません` で失敗する。

- [ ] **Step 4: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。二度 consult しても倍にならない既存テストを含む。版は `mosa 0.0.1 (phase 2)`

- [ ] **Step 5: STATUS**

```markdown
フェーズ: 3
進行中: なし
完了: discontiguous と multifile と include と ensure_loaded と [File] が読める
次: INRIA Suite の 90% と conformance.md
未解決: なし
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

- [ ] **Step 6: Commit**

```bash
git add src/machine/load.cpp src/machine/consult.cpp tests/pl/directives.pl CMakeLists.txt docs/STATUS.md docs/iso-notes.md
git commit -m "[phase3] 残りのディレクティブを追加する（離れた節の診断が残る）"
```

---

### Task 13: INRIA Suite、conformance.md、版

**Files:**
- Create: `tests/conformance/` のライセンスファイルとスイート、`tests/cpp/conformance_report_test.cpp`
- Modify: `include/mosa/suite.hpp`、`src/machine/suite.cpp`、`src/main_mosatest.cpp`、`src/version.cpp`、`CMakeLists.txt`、`.github/workflows/ci.yml`、`docs/STATUS.md`、`docs/iso-notes.md`、生成される `docs/conformance.md`
- Test: `mosatest --suite` と、レポートを書く C++ テスト

**Interfaces:**
- Consumes: タスク 12 までの述語と、既存の `run_suite`
- Produces: `int run_iso_suite(std::string_view suite_dir, std::string_view report_path)`。完了条件は仕様第9節のフェーズ3そのものである

このタスクより前に版を変えない。合格率は手で `docs/conformance.md` に書かない。不合格を、テストの改変や期待値の書き換えで消さない。

- [ ] **Step 1: ライセンスを記録してからスイートを置く**

決定済み（2026-10-26、`docs/iso-notes.md`）。`inriasuite.pl` は置かない。スイートは `tests/conformance/logtalk/` に Apache-2.0 のまま置いた。合格率はランナーができてから `mosatest --suite` が書く。推測の数値は書かない。

- [ ] **Step 2: 失敗するテストを書く**

`conformance_report_test` は、公開の `run_iso_suite` を呼ぶ。引数は `tests/conformance` ではない、`test/3` が 1 件だけあるディレクトリとする。生成されたファイルを読み、合格数と総数がその実行の結果と一致することを確認する。合格率の数値を、このテストの期待値に埋め込まない。INRIA の形式がライセンスと一緒の説明に書いてあるまで、説明に無い形式を成功扱いにしない。

`mosatest --suite <ディレクトリ>` は `run_iso_suite` を呼び、`docs/conformance.md` を上書きする。`--roundtrip` と、ディレクトリを `test/3` として実行する既存の入口は残す。使い方の文は、既存の文に `--suite <ディレクトリ>` を足した 1 行にする。

生成する `docs/conformance.md` の先頭は「このファイルは自動生成である。手で編集しない。」とする。本文に、合格数、総数、合格率、不合格の一覧を書く。

- [ ] **Step 3: スイートを実行し、90% 未満なら直す**

Run: `cmake --build build && ./build/mosatest --suite tests/conformance`
Expected: 終了コード 0 は、合格率が 90% 以上のときだけとする。90% 未満なら終了コード 1 とし、不合格を `docs/conformance.md` に残す

不合格の原因が規格の解釈で一つに決まらないときは、テストを弱めずに `docs/iso-notes.md` へ条番号と「A か B か」を 1 つ書く。決まる不合格は、実装の側を直す。スイートの期待値は変えない。

- [ ] **Step 4: 版を変える**

合格率 90% 以上、往復、`nrev`、`queens`、段階 A、算術、フェーズ3の `tests/pl/` が通ってから、`src/version.cpp` を `mosa 0.0.1 (phase 3)` にし、`CMakeLists.txt` の `version_pass_regex` も同じ文字列にする。それより前のコミットでは版テストを壊さない。

CI の Job Summary は、`conformance: n/a (phase 0)` をやめ、`mosatest --suite` が書いた合格率の 1 行にする。スイートが同梱されていないジョブでは、合格率を作らず、そのジョブを失敗させる。

- [ ] **Step 5: 全体が通ることを確認する**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部成功。`./build/mosa --version` は `mosa 0.0.1 (phase 3)`。`docs/conformance.md` の合格率は 90% 以上で、ファイルはコマンドの出力である

- [ ] **Step 6: STATUS**

```markdown
フェーズ: 3 完了
進行中: なし
完了: INRIA Suite の合格率が 90% 以上。docs/conformance.md は mosatest --suite が生成する。版は mosa 0.0.1 (phase 3)
次: フェーズ4のモジュール
未解決: iso-notes に残した解釈
計画: docs/superpowers/plans/2026-10-25-phase3-builtins.md
```

15 行を超えない。未解決が無ければ `なし` とする。

- [ ] **Step 7: Commit**

```bash
git add tests/conformance include/mosa/suite.hpp src/machine/suite.cpp src/main_mosatest.cpp tests/cpp/conformance_report_test.cpp src/version.cpp CMakeLists.txt .github/workflows/ci.yml docs/conformance.md docs/iso-notes.md docs/STATUS.md
git commit -m "[phase3] INRIA Suite が 90% を超えフェーズ3を完了する（conformance.md）"
```

ライセンスファイルは、このコミットに含める。スイートより前のコミットにソースだけを置かない。
