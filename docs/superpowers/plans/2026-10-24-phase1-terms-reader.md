# フェーズ1 項とリーダ・ライタ Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 実行エンジンを持たないまま、Cell 上の項を読み書きし、`mosatest --roundtrip tests/pl/syntax.pl` を終了コード 0 にする。

**Architecture:** 承認前は Unicode 分類、lexer、演算子表だけを足す。承認後は Cell をヒープに直接作り、Pratt リーダとライタがそれを往復する。`syntax.pl` はゴールではなく例文データである。別の構文木は作らない。

**Tech Stack:** C++20、CMake 3.25 以上、CTest、標準ライブラリのみ。浮動小数と整数のビット列は `std::bit_cast`。

**Spec:** `docs/superpowers/specs/2026-10-24-phase1-terms-reader-design.md`

各タスクは 1 コミットで終わり、そのコミットがセッションの区切りである。次のタスクは、前のタスクの確認コマンドが通ってから始める。実装中にこの計画と設計文書を書き換えない。ずれに気づいたら `docs/STATUS.md` の未解決に 1 行書いて止まり、利用者に聞く。

## Global Constraints

- 言語は C++20。`CMAKE_CXX_EXTENSIONS` は OFF。コンパイラ拡張、`__int128`、`#pragma once` 以外の独自プラグマは使わない
- 依存は C++ 標準ライブラリと CMake だけ。新しいライブラリは追加しない
- 警告は `-Wall -Wextra -Wpedantic -Werror`。MSVC は `/W4 /WX /utf-8 /permissive- /Zc:__cplusplus`
- 生の `new` と `delete`、`reinterpret_cast`、C 配列、マクロは使わない。`#pragma once` は使ってよい
- 1 関数は 60 行以内、1 ファイルは 800 行以内。超える前に、そのタスクの Files に書いた分割へ移す
- 識別子は英語。コメント、コミット、文書は日本語。コメントは理由だけを書く。文字コードは UTF-8、BOM なし
- Prolog の失敗と構文エラーは値で返す。C++ の例外は `mosa::ResourceLimit` だけが投げ、ヒープ上限の超過だけに使う
- `#ifdef` による OS 依存は書かない。`src/platform/` にソースを置かない
- `include/mosa/mosa.hpp` と `include/mosa/runtime.hpp` はフェーズ0のままである。署名を足さない
- 版文字列はタスク 8 の完了コミットまで `mosa 0.0.1 (phase 0)` のままである
- コミットメッセージは `[phase1] ` で始まる日本語 1 行とし、何を確認したかを含める
- 各タスクの終わりに `cmake --build build && ctest --test-dir build` を通し、`docs/STATUS.md` をそのタスクに書いた文へ置き換える。15 行を超えない
- テストを弱めない、飛ばさない、期待値を実装に合わせない
- Cell のコードはタスク 4 まで書かない。タスク 4 は ADR 0001 の状態行が未承認のままなら、コードを書かずに止まる
- `tests/cpp/harness.hpp` は変更しない

## Review Focus

設計の成功条件は正常な往復である。次の 5 つは、その往復だけを見ていると実装が通っても利用者の入力で壊れる。各行のテストは、書いたタスクの Step 1 に入っている。

- 全角の `（`（U+FF08）は括弧ではない。`（a）.` は `a` を包んだ項にならず、`UnexpectedToken` で失敗する
- `0X10` の接頭辞は 16 進ではない。トークンは整数 `0` と変数 `X10` の 2 つである
- 結合文字 U+0301 は先頭では `IllegalCharacter`、`e` の直後ではその名前の一部である
- 名前表に無い同一 REF が 2 回現れる項は `f(_G1, _G1)` と書く。1 回だけなら `_` である
- 不正な UTF-8 は例外にしない。`あ` の直後のバイト `0xFF` は `InvalidUtf8`、行 1、列 2 である

## ファイルの責任

| パス | 責任 | タスク |
| --- | --- | --- |
| `tools/gen_unicode_tables.py` | Unicode 18.0.0 から区間表を生成する。ビルドからは呼ばない | 1 |
| `src/unicode_tables.inc` | 生成物。一般カテゴリと、幅が W または F の区間 | 1 |
| `docs/unicode-version.md` | `18.0.0` | 1 |
| `include/mosa/char_class.hpp`、`src/read/char_class.cpp` | `classify` と `east_asian_wide`。lexer は幅を読まない | 1 |
| `include/mosa/syntax_error.hpp` | 失敗の理由、行、列、トークン | 2 |
| `include/mosa/lexer.hpp`、`src/read/lexer.cpp`、`src/read/lexer_literal.cpp` | トークン列。Cell は作らない | 2 |
| `include/mosa/operator_table.hpp`、`src/read/operator_table.cpp` | 既定演算子と `define`。キーは UTF-8 文字列 | 3 |
| `docs/adr/0002-cell-layout.md` | タグ番号と関数語のビット配置 | 4 |
| `include/mosa/cell.hpp` | タグ、`pack_functor` | 4 |
| `include/mosa/atom.hpp`、`src/core/atom.cpp` | アトム番号。0 は欠番 | 4 |
| `include/mosa/heap.hpp`、`src/core/heap.cpp` | 語の配列、`reserve` 一度、`ResourceLimit` | 4 |
| `include/mosa/unify.hpp`、`src/core/unify.cpp` | 出現検査なしの `unify` | 5 |
| `include/mosa/reader.hpp`、`src/read/reader.cpp`、`src/read/reader_bracket.cpp` | Pratt リーダ | 6 |
| `include/mosa/writer.hpp`、`src/write/writer.cpp`、`src/write/term_equal.cpp` | ライタと `same_term` | 7 |
| `include/mosa/roundtrip.hpp`、`src/read/roundtrip.cpp` | ケース分割と往復 | 8 |
| `src/main_mosatest.cpp`、`src/version.cpp` | `--roundtrip`。完了時だけ版を `phase 1` にする | 8 |
| `tests/pl/syntax.pl` | 例文データ | 8 |
| `CMakeLists.txt` | 各タスクが `target_sources` と CTest を足す | 1 以降 |

`src/read/lexer.cpp` は名前、変数、図形、ソロ、コメント、レイアウト、終端ピリオドを読む。`src/read/lexer_literal.cpp` は数、引用、エスケープ、`0'` を読む。`src/read/reader.cpp` は `Reader::next` と Pratt を持つ。`src/read/reader_bracket.cpp` は括弧、リスト、中括弧、引数リストを持つ。どちらも公開シンボルはヘッダにだけ置く。

## ビルドの足し方

`add_library(mosa_lib STATIC src/version.cpp)` は残す。各タスクは次の形をファイルの末尾側、`enable_testing()` より前に足す。テスト実行ファイルは `enable_testing()` より後に足す。

```cmake
target_sources(mosa_lib PRIVATE src/read/char_class.cpp)

add_executable(char_class_test tests/cpp/char_class_test.cpp)
target_include_directories(char_class_test PRIVATE tests/cpp)
target_link_libraries(char_class_test PRIVATE mosa_lib)
add_test(NAME char_class COMMAND $<TARGET_FILE:char_class_test>)
```

以降のタスクは、ソースパスとテスト名だけをこの形に置き換える。`mosa_lib` の `PUBLIC` include は既に `include` である。

## タスクをまたいで使う契約

失敗理由はタスク 2 で導入し、その後のタスクはこれをそのまま使う。

```cpp
namespace mosa {
enum class SyntaxReason {
  InvalidUtf8,
  IllegalCharacter,
  UnterminatedQuote,
  UnterminatedComment,
  BadEscape,
  IntegerOverflow,
  UnexpectedToken,
  MaxArity,
  IncompleteTerm,
};

struct SyntaxFailure {
  std::size_t line = 1;
  std::size_t column = 1;
  std::string token;
  SyntaxReason reason = SyntaxReason::IllegalCharacter;
};
}
```

`UnterminatedComment` は設計の最低限の一覧に足す。コメントの未終了を、引用やトークンの失敗と混ぜないためである。

文字分類はタスク 1 の宣言のままである。

```cpp
namespace mosa {
enum class CharClass {
  Layout, Small, Capital, Underline, Digit, Graphic, Solo, Mark, Quote, Illegal
};
[[nodiscard]] CharClass classify(char32_t code_point);
[[nodiscard]] bool east_asian_wide(char32_t code_point);
[[nodiscard]] std::string_view unicode_version();
}
```

ASCII は生成表を見ない。分類は次で固定する。

| コードポイント | 分類 |
| --- | --- |
| U+0009、U+000A、U+000B、U+000C、U+000D、U+0020 | Layout |
| `a-z` | Small |
| `A-Z` | Capital |
| U+005F `_` | Underline |
| `0-9` | Digit |
| `# $ & * + - . / : < = > ? @ ^ ~ \` | Graphic |
| `! ( ) , ; [ ] { \| } %` | Solo |
| `'` と `"` | Quote |
| 上記以外の ASCII、U+007F を含む | Illegal |

非 ASCII は生成表を見る。`Ll` `Lo` `Lm` `Nl` は Small。`Lu` `Lt` は Capital。`Mn` `Mc` は Mark。`Sm` `So` `Pd` `Po` は Graphic。`Zs` `Zl` `Zp` は Layout。U+0085 は Layout。U+FEFF は Illegal。それ以外は Illegal。全角数字 U+FF10 は Digit ではない。

`east_asian_wide` は東アジア幅が W または F のときだけ true である。`unicode_version()` は `18.0.0` を返す。

lexer はタスク 2 の宣言のままである。

```cpp
namespace mosa {
enum class TokenKind { End, Name, Variable, Integer, Float, String, Solo, EndFullStop };

struct Token {
  TokenKind kind = TokenKind::End;
  std::string text;
  std::int64_t integer = 0;
  double floating = 0;
  std::size_t line = 1;
  std::size_t column = 1;
  std::size_t byte = 0;
};

struct Scan {
  Token token;
  std::optional<SyntaxFailure> failure;
};

class Lexer {
 public:
  explicit Lexer(std::string_view source);
  [[nodiscard]] Scan next();
};
}
```

`failure` が値を持つとき、呼び出し側は `token` を見ない。引用アトムは `Name`、二重引用は `String` である。どちらもエスケープ展開後の UTF-8 を `text` に持つ。`Integer` の値は 0 以上で、`integer` に入る。`0'` も `Integer` である。

演算子はタスク 3 の宣言のままである。

```cpp
namespace mosa {
enum class Specifier { Xf, Yf, Xfx, Xfy, Yfx, Fx, Fy };

enum class OpResult {
  Ok, BadPriority, BadSpecifier, ModifyComma, CreateEmpty, InfixAndPostfix, CreateBar
};

struct OpDef {
  int priority = 0;
  Specifier specifier = Specifier::Xfx;
  std::string name;
};

class OperatorTable {
 public:
  [[nodiscard]] static OperatorTable iso_core();
  [[nodiscard]] OpResult define(int priority, Specifier specifier, std::string name);
  [[nodiscard]] std::vector<OpDef> lookup(std::string_view name) const;
};
}
```

`lookup` の順序は、優先度の昇順、同じ優先度では `Specifier` の宣言順である。`define(0, specifier, name)` はその組だけを消す。

Cell とヒープはタスク 4 の宣言のままである。後のタスクはビット演算を自分で書かず、この関数を呼ぶ。

```cpp
namespace mosa {
enum class Tag : std::uint64_t {
  Ref = 0, Atom = 1, Int = 2, Str = 3, Lis = 4, Float = 5, Big = 6, IntBox = 7
};

struct Cell {
  std::uint64_t raw = 0;
  [[nodiscard]] Tag tag() const;
};

[[nodiscard]] constexpr std::uint64_t pack_functor(std::uint32_t atom_id, std::uint16_t arity);
[[nodiscard]] constexpr std::uint32_t functor_atom_id(std::uint64_t word);
[[nodiscard]] constexpr std::uint16_t functor_arity(std::uint64_t word);

class ResourceLimit : public std::exception {
 public:
  [[nodiscard]] const char* what() const noexcept override;
};

class AtomTable {
 public:
  [[nodiscard]] std::uint32_t intern(std::string_view utf8);
  [[nodiscard]] std::uint32_t intern_char(char32_t code_point);
  [[nodiscard]] std::string_view name(std::uint32_t id) const;
  [[nodiscard]] bool ascii(std::uint32_t id) const;
};

class Heap {
 public:
  explicit Heap(AtomTable& atoms, std::size_t capacity = 1 << 20);
  [[nodiscard]] AtomTable& atoms();
  [[nodiscard]] Cell atom(std::uint32_t id);
  [[nodiscard]] Cell integer(std::int64_t value);
  [[nodiscard]] Cell floating(double value);
  [[nodiscard]] Cell fresh_ref();
  [[nodiscard]] Cell structure(std::uint32_t atom_id, std::span<const Cell> args);
  [[nodiscard]] Cell list(Cell head, Cell tail);
  [[nodiscard]] bool unbound(Cell cell) const;
  [[nodiscard]] Cell deref(Cell cell) const;
  [[nodiscard]] std::int64_t integer_value(Cell cell) const;
  [[nodiscard]] double float_value(Cell cell) const;
  [[nodiscard]] std::uint32_t functor_atom(Cell cell) const;
  [[nodiscard]] std::uint16_t functor_arity(Cell cell) const;
  [[nodiscard]] Cell arg(Cell cell, std::uint16_t index) const;
  [[nodiscard]] Cell head(Cell cell) const;
  [[nodiscard]] Cell tail(Cell cell) const;
  struct Mark { std::size_t heap_size = 0; std::size_t trail_size = 0; };
  [[nodiscard]] Mark mark() const;
  void rewind(Mark mark);
};
}
```

`pack_functor(atom, arity)` は `(atom << 14) | (arity << 3)` である。下位 3bit は 0 である。即値にできる整数は `-2^60` 以上かつ `2^60` 未満だけである。`ResourceLimit::what()` は `ヒープの上限を超えました` を返す。アトム番号は最初の登録が 1 である。

ユニフィケーションはタスク 5 の宣言のままである。

```cpp
namespace mosa {
[[nodiscard]] bool unify(Heap& heap, Cell left, Cell right);
}
```

リーダはタスク 6 の宣言のままである。

```cpp
namespace mosa {
enum class DoubleQuotes { Codes, Chars, Atom };

struct VarName {
  std::string name;
  Cell variable;
};

struct ReadTerm {
  Cell term;
  std::vector<VarName> names;
  std::vector<VarName> singletons;
};

struct ReadOutcome {
  bool eof = false;
  std::optional<ReadTerm> term;
  std::optional<SyntaxFailure> failure;
};

class Reader {
 public:
  Reader(Heap& heap, std::string_view source, OperatorTable& operators,
         DoubleQuotes quotes = DoubleQuotes::Codes);
  [[nodiscard]] ReadOutcome next();
};
}
```

`eof`、`term`、`failure` のうち値を持つのは常に 1 つである。`names` は最初に現れた順で、`_` は入らない。`singletons` は一度しか現れない名前だけである。

ライタはタスク 7 の宣言のままである。

```cpp
namespace mosa {
struct WriteOptions {
  bool quoted = true;
  bool ignore_ops = false;
};

[[nodiscard]] std::string write_term(const Heap& heap, const OperatorTable& operators, Cell term,
                                     const WriteOptions& options, const std::vector<VarName>& names);
[[nodiscard]] bool same_term(const Heap& left_heap, Cell left, const std::vector<VarName>& left_names,
                             const Heap& right_heap, Cell right, const std::vector<VarName>& right_names);
}
```

往復はタスク 8 の宣言のままである。

```cpp
namespace mosa {
[[nodiscard]] int run_roundtrip(std::string_view path, std::ostream& error);
}
```

成功は 0、失敗は 1 である。失敗行は `mosatest: <ケース識別子>: <理由>` の 1 行を `error` に書く。

---

### Task 1: 文字分類と Unicode 表

**Files:**
- Create: `include/mosa/char_class.hpp`
- Create: `src/read/char_class.cpp`
- Create: `tests/cpp/char_class_test.cpp`
- Modify: `tools/gen_unicode_tables.py`
- Create: `src/unicode_tables.inc`（生成してコミットする）
- Modify: `docs/unicode-version.md`
- Modify: `CMakeLists.txt`
- Modify: `docs/STATUS.md`

**Interfaces:**
- Consumes: なし
- Produces: 上の `classify`、`east_asian_wide`、`unicode_version`

- [ ] **Step 1: Write the failing test**

`tests/cpp/char_class_test.cpp` の `main` に、次を書く。

```cpp
mosa_test::check(mosa::unicode_version() == "18.0.0", "unicode 18.0.0");
mosa_test::check(mosa::classify(U'a') == mosa::CharClass::Small, "a");
mosa_test::check(mosa::classify(U'Z') == mosa::CharClass::Capital, "Z");
mosa_test::check(mosa::classify(U'_') == mosa::CharClass::Underline, "_");
mosa_test::check(mosa::classify(U'7') == mosa::CharClass::Digit, "7");
mosa_test::check(mosa::classify(U'+') == mosa::CharClass::Graphic, "+");
mosa_test::check(mosa::classify(U'!') == mosa::CharClass::Solo, "!");
mosa_test::check(mosa::classify(U';') == mosa::CharClass::Solo, ";");
mosa_test::check(mosa::classify(U'`') == mosa::CharClass::Illegal, "grave");
mosa_test::check(mosa::classify(U'\u3042') == mosa::CharClass::Small, "親の類 あ");
mosa_test::check(mosa::classify(U'\u89aa') == mosa::CharClass::Small, "親");
mosa_test::check(mosa::classify(U'\u00c4') == mosa::CharClass::Capital, "A-umlaut");
mosa_test::check(mosa::classify(U'\u20bb7') == mosa::CharClass::Small, "𠮷");
mosa_test::check(mosa::classify(U'\u3000') == mosa::CharClass::Layout, "ideographic space");
mosa_test::check(mosa::classify(U'\u0085') == mosa::CharClass::Layout, "next line");
mosa_test::check(mosa::classify(U'\ufeff') == mosa::CharClass::Illegal, "feff");
mosa_test::check(mosa::classify(U'\uff10') != mosa::CharClass::Digit, "fullwidth digit");
mosa_test::check(mosa::classify(U'\u0301') == mosa::CharClass::Mark, "combining acute");
mosa_test::check(mosa::classify(U'\uff08') == mosa::CharClass::Graphic, "fullwidth paren");
mosa_test::check(mosa::east_asian_wide(U'\u4e00'), "CJK wide");
mosa_test::check(!mosa::east_asian_wide(U'A'), "A narrow");
```

`U'\u89aa'` は `親`、`U'\u20bb7'` は `𠮷`、`U'\uff08'` は `（` である。

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target char_class_test`

Expected: ヘッダまたはシンボルが無く、リンクかコンパイルに失敗する

- [ ] **Step 3: 生成器と分類を実装する**

`tools/gen_unicode_tables.py` は標準ライブラリだけを使う。引数 `--unicode-dir DIR` があるときは `DIR/UnicodeData.txt` と `DIR/EastAsianWidth.txt` を読む。無いときは `https://www.unicode.org/Public/18.0.0/ucd/` の同名ファイルを取得する。失敗したら終了コード 1 で、`src/unicode_tables.inc` を書き換えない。成功したら一時ファイルへ書いてから置き換える。

`UnicodeData.txt` の `<..., First>` と `<..., Last>` は区間として読む。同じカテゴリの隣接は 1 区間にまとめる。`EastAsianWidth.txt` の `start..end` も区間として読み、W と F だけを残す。

`src/unicode_tables.inc` の先頭コメントに `Unicode 18.0.0` と書く。中身は `CategoryRange {first, last, category}` と `WideRange {first, last}` の配列である。全コードポイントの配列にはしない。`docs/unicode-version.md` の本文は `18.0.0` の 1 行にする。

`classify` は、ASCII では上の表を使い、非 ASCII では区間を二分探索する。生成器を一度実行し、生成物をコミット対象に含める。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: `char_class` を含む全テストが PASS。版テストは `phase 0` のままである

- [ ] **Step 5: Commit**

`docs/STATUS.md` を次で置き換える。

```text
フェーズ: 1
進行中: なし
完了: 文字分類と Unicode 18.0.0 の表
次: lexer
未解決: ADR 0001 は未承認。Cell は書かない
計画: docs/superpowers/plans/2026-10-24-phase1-terms-reader.md
```

```bash
git add tools/gen_unicode_tables.py src/unicode_tables.inc docs/unicode-version.md \
  include/mosa/char_class.hpp src/read/char_class.cpp tests/cpp/char_class_test.cpp \
  CMakeLists.txt docs/STATUS.md
git commit -m "[phase1] Unicode 18.0.0 の文字分類を追加（char_class テストが通る）"
```

### Task 2: lexer

**Files:**
- Create: `include/mosa/syntax_error.hpp`
- Create: `include/mosa/lexer.hpp`
- Create: `src/read/lexer.cpp`
- Create: `src/read/lexer_literal.cpp`
- Create: `tests/cpp/lexer_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/STATUS.md`

**Interfaces:**
- Consumes: `classify`
- Produces: `SyntaxFailure`、`Lexer`、`Scan`、`Token`

- [ ] **Step 1: Write the failing test**

ヘルパ `scan_all` は `failure` が空のトークンだけをベクタにする。失敗したらその場で `check` して空を返す。`main` の表明は次である。

```cpp
auto atoms = scan_all("foo.");
mosa_test::check(atoms.size() == 2 && atoms[0].kind == mosa::TokenKind::Name &&
                     atoms[0].text == "foo" && atoms[1].kind == mosa::TokenKind::EndFullStop,
                 "atom and full stop");
auto glued = scan_all("0X10.");
mosa_test::check(glued.size() == 3 && glued[0].kind == mosa::TokenKind::Integer &&
                     glued[0].integer == 0 && glued[1].kind == mosa::TokenKind::Variable &&
                     glued[1].text == "X10",
                 "0X10 is not hex");
auto cols = scan_all("あ.");
mosa_test::check(cols.size() == 2 && cols[1].kind == mosa::TokenKind::EndFullStop &&
                     cols[1].line == 1 && cols[1].column == 2,
                 "column counts あ as one");
mosa::Lexer crlf("a\r\nb");
auto first = crlf.next();
auto second = crlf.next();
mosa_test::check(second.token.line == 2 && second.token.column == 1, "crlf is one break");
auto mark = mosa::Lexer("\u0301");
auto bad_mark = mark.next();
mosa_test::check(bad_mark.failure &&
                     bad_mark.failure->reason == mosa::SyntaxReason::IllegalCharacter,
                 "combining mark cannot start");
auto composed = scan_all("e\u0301.");
mosa_test::check(composed.size() == 2 && composed[0].kind == mosa::TokenKind::Name &&
                     composed[0].text == "e\u0301",
                 "mark continues a name");
auto full = mosa::Lexer("（a）.");
auto open = full.next();
mosa_test::check(open.token.kind == mosa::TokenKind::Name && open.token.text == "（",
                 "fullwidth paren is a graphic name");
bool threw = false;
try {
  mosa::Lexer bad("あ\xff");
  auto failed = bad.next();
  (void)bad.next();
  mosa_test::check(failed.token.kind == mosa::TokenKind::Name, "hiragana before bad byte");
  auto utf = bad.next();
  mosa_test::check(utf.failure && utf.failure->reason == mosa::SyntaxReason::InvalidUtf8 &&
                       utf.failure->line == 1 && utf.failure->column == 2 &&
                       utf.failure->token.empty(),
                   "invalid utf-8 column");
} catch (...) {
  threw = true;
}
mosa_test::check(!threw, "invalid utf-8 does not throw");
auto hex = scan_all("0xFF.");
mosa_test::check(hex[0].kind == mosa::TokenKind::Integer && hex[0].integer == 255, "0xFF");
auto under = scan_all("1_000.");
mosa_test::check(under[0].integer == 1000, "underscore digits");
auto tail = mosa::Lexer("1_.");
auto tail_scan = tail.next();
mosa_test::check(tail_scan.failure &&
                     tail_scan.failure->reason == mosa::SyntaxReason::UnexpectedToken &&
                     tail_scan.failure->token == "1_",
                 "trailing underscore");
auto doubled_under = mosa::Lexer("1__0.");
auto doubled_scan = doubled_under.next();
mosa_test::check(doubled_scan.failure &&
                     doubled_scan.failure->reason == mosa::SyntaxReason::UnexpectedToken,
                 "doubled underscore");
auto var_under = scan_all("_1.");
mosa_test::check(var_under[0].kind == mosa::TokenKind::Variable && var_under[0].text == "_1",
                 "underscore variable");
```

続けて次を表明する。

```cpp
auto nl = scan_all("'\\n'.");
mosa_test::check(nl[0].text == "\n", "symbolic newline");
auto hexesc = scan_all("'\\x41\\'.");
mosa_test::check(hexesc[0].text == "A", "hex escape");
auto uesc = scan_all("'\\u3042'.");
mosa_test::check(uesc[0].text == "あ", "u escape");
auto cont = scan_all("'ab\\\nc'.");
mosa_test::check(cont[0].text == "abc", "line continuation");
auto oct = scan_all("'\\141\\'.");
mosa_test::check(oct[0].text == "a", "octal escape");
auto doubled = scan_all("'a''b'.");
mosa_test::check(doubled[0].text == "a'b", "doubled quote");
auto code = scan_all("0'あ.");
mosa_test::check(code[0].kind == mosa::TokenKind::Integer && code[0].integer == 0x3042, "0'");
auto code2 = scan_all("0'𠮷.");
mosa_test::check(code2[0].integer == 0x20BB7, "0' non-bmp");
auto stop = scan_all("1.");
mosa_test::check(stop.size() == 2 && stop[0].kind == mosa::TokenKind::Integer &&
                     stop[0].integer == 1 && stop[1].kind == mosa::TokenKind::EndFullStop,
                 "1. is integer and full stop");
auto real = scan_all("1.5e-2.");
mosa_test::check(real[0].kind == mosa::TokenKind::Float && real[0].floating == 1.5e-2, "float");
auto grave = mosa::Lexer("`");
auto grave_scan = grave.next();
mosa_test::check(grave_scan.failure &&
                     grave_scan.failure->reason == mosa::SyntaxReason::IllegalCharacter,
                 "grave");
auto big = mosa::Lexer("9223372036854775808.");
auto big_scan = big.next();
mosa_test::check(big_scan.failure &&
                     big_scan.failure->reason == mosa::SyntaxReason::IntegerOverflow,
                 "int64 overflow");
auto comment = scan_all("/* /* */ foo.");
mosa_test::check(comment[0].kind == mosa::TokenKind::Name && comment[0].text == "foo",
                 "block comment does not nest");
auto line = scan_all("% foo\nbar.");
mosa_test::check(line[0].text == "bar", "line comment");
auto bom = scan_all("\uFEFFfoo.");
mosa_test::check(bom[0].text == "foo", "leading BOM skipped");
```

`scan_all` の入力リテラルは、上のエスケープが lexer へのソースになるように書く。`'\\n'` という C++ 文字列は lexer から見て `'\n'` である。行継続の C++ 文字列は `'ab\\\nc'` であり、lexer のソースは引用符、`ab`、バックスラッシュ、改行、`c`、引用符である。

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target lexer_test`

Expected: `mosa/lexer.hpp` が無く、コンパイルに失敗する

- [ ] **Step 3: lexer を実装する**

ファイル先頭の U+FEFF は 1 回だけ捨てる。レイアウトとコメントはトークンにしない。ブロックコメントはネストしない。未終了なら `UnterminatedComment` で、`token` は `/*` である。

終端ピリオドは、`.` の直後がレイアウトか入力の終わりのときだけ `EndFullStop` である。浮動小数は、小数点の両側に桁があるときだけである。`0b` `0o` `0x` は小文字だけを接頭辞にする。16 進の桁だけ `A-F` を認める。`_` は同じ基数の桁と桁の間だけに置く。それ以外の `_` が数の途中に来たら `UnexpectedToken` とし、`token` にはそれまでに読んだ綴りを入れる。値は 0 以上である。`int64` の最大値を超えたら `IntegerOverflow` とし、浮動小数にはしない。

引用の中では、連続する同じ引用符は引用符 1 文字である。エスケープは、`\a \b \f \n \r \t \v`、`\\ \' \" \``、`\x` と 1 桁以上の 16 進と `\`、`\u` と 16 進 4 桁、`\U` と 16 進 8 桁、`\` と 8 進 1 から 3 桁と `\`、バックスラッシュと改行（`\n`、`\r\n`、`\r`）とその後のレイアウトの削除、バックスラッシュと図形文字またはソロ文字 1 つ、である。サロゲートと `U+10FFFF` を超える値は `BadEscape` である。引用が閉じないときは `UnterminatedQuote` で、`token` は開き引用符 1 文字である。

`0'` の直後は、エスケープ 1 つ、または改行でないコードポイント 1 つである。名前は Small で始まり、Small、Capital、Underline、Digit、Mark が続く。図形トークンは Graphic の連続である。ソロは 1 文字である。変数は Capital または Underline で始まり、名前と同じ継続を使う。入力の終わりは `TokenKind::End` で、`failure` は空である。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS。`char_class` も通ったままである

- [ ] **Step 5: Commit**

STATUS の完了を `lexer`、次を `演算子表` にする。未解決の行はタスク 1 のまま残す。

```bash
git add include/mosa/syntax_error.hpp include/mosa/lexer.hpp src/read/lexer.cpp \
  src/read/lexer_literal.cpp tests/cpp/lexer_test.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase1] lexer を追加（トークンと列番号のテストが通る）"
```

### Task 3: 演算子表

**Files:**
- Create: `include/mosa/operator_table.hpp`
- Create: `src/read/operator_table.cpp`
- Create: `tests/cpp/operator_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/STATUS.md`
- Modify: `docs/iso-notes.md`（Cor.2 を見て `div` を外すときだけ）

**Interfaces:**
- Consumes: なし（Cell もアトム表も使わない）
- Produces: `OperatorTable`、`Specifier`、`OpResult`、`OpDef`

- [ ] **Step 1: Write the failing test**

`iso_core().lookup(name)` が、設計の表の各組を含むことを、名前ごとに表明する。最低限、次の組がちょうどその指定子と優先度で存在する。

```cpp
auto ops = mosa::OperatorTable::iso_core();
auto has = [&](int priority, mosa::Specifier spec, std::string_view name) {
  for (const auto& op : ops.lookup(name)) {
    if (op.priority == priority && op.specifier == spec) return true;
  }
  return false;
};
mosa_test::check(has(1200, mosa::Specifier::Xfx, ":-"), ":- xfx");
mosa_test::check(has(1200, mosa::Specifier::Fx, ":-"), ":- fx");
mosa_test::check(has(1200, mosa::Specifier::Xfx, "-->"), "-->");
mosa_test::check(has(1200, mosa::Specifier::Fx, "?-"), "?-");
mosa_test::check(has(1100, mosa::Specifier::Xfy, ";"), ";");
mosa_test::check(has(1050, mosa::Specifier::Xfy, "->"), "->");
mosa_test::check(has(1000, mosa::Specifier::Xfy, ","), "comma");
mosa_test::check(has(900, mosa::Specifier::Fy, "\\+"), "\\+");
mosa_test::check(has(700, mosa::Specifier::Xfx, "=.."), "=..");
mosa_test::check(has(700, mosa::Specifier::Xfx, "is"), "is");
mosa_test::check(has(500, mosa::Specifier::Yfx, "+"), "+ infix");
mosa_test::check(has(200, mosa::Specifier::Fy, "+"), "+ prefix");
mosa_test::check(has(400, mosa::Specifier::Yfx, "div"), "div");
mosa_test::check(has(200, mosa::Specifier::Xfx, "**"), "**");
mosa_test::check(has(200, mosa::Specifier::Xfy, "^"), "^");
mosa_test::check(ops.lookup(":").empty(), "no module colon");
mosa_test::check(ops.lookup("|").empty(), "bar is not predefined");
mosa_test::check(ops.lookup("*->").empty(), "no *->");
mosa_test::check(ops.lookup("xor").empty(), "xor is not an operator");
mosa_test::check(ops.define(0, mosa::Specifier::Fy, "+") == mosa::OpResult::Ok, "remove prefix +");
mosa_test::check(!has(200, mosa::Specifier::Fy, "+") && has(500, mosa::Specifier::Yfx, "+"),
                 "infix + remains");
```

`define` の拒否は、別の `OperatorTable::iso_core()` で次を表明する。優先度 `-1` と `1201` は `BadPriority`。`,` への `define(1000, Xfy, ",")` と `define(0, Xfy, ",")` は `ModifyComma`。`[]` と `{}` は `CreateEmpty`。`|` の `Fx` と、`Xfx` の優先度 `1000` は `CreateBar`。`|` の `Xfx` で優先度 `1001` は `Ok`。既に中置がある名前へ後置を足すと `InfixAndPostfix`。`+` の中置を消しても前置が残ることは上で見ている。

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target operator_test`

Expected: ヘッダが無く、コンパイルに失敗する

- [ ] **Step 3: 演算子表を実装する**

`iso_core` は設計の表を入れる。`:`、`|`、`*->`、`xor` は入れない。手元に Cor.2 があるときだけ表7を見る。`div` が表に無ければ `iso_core` から外し、テストの `has(..., "div")` も外し、`docs/iso-notes.md` に条番号と「表に無いので入れなかった」と書く。他の追加演算子が表にあれば、表とテストに足す。手元に Cor.2 が無ければ、設計の表のまま進める。

優先度 0 はその名前と指定子の組だけを消す。`|` の条項が「1000 より大きい」でなければ、コードを確定せず `docs/iso-notes.md` に「A: 1000 より大きい、B: 条項の値」と書き、STATUS の未解決に同じ 1 行を書いて止まる。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS

- [ ] **Step 5: Commit**

STATUS の完了を `演算子表`、次を `ADR 0001 の承認後に Cell` にする。

```bash
git add include/mosa/operator_table.hpp src/read/operator_table.cpp \
  tests/cpp/operator_test.cpp CMakeLists.txt docs/STATUS.md docs/iso-notes.md
git commit -m "[phase1] ISO コアの演算子表を追加（既定と define の拒否が通る）"
```

`docs/iso-notes.md` を変更していないときは、そのパスを `git add` から外す。

### Task 4: ADR 0002、Cell、ヒープ、アトム

**Files:**
- Create: `docs/adr/0002-cell-layout.md`
- Create: `include/mosa/cell.hpp`
- Create: `include/mosa/atom.hpp`
- Create: `src/core/atom.cpp`
- Create: `include/mosa/heap.hpp`
- Create: `src/core/heap.cpp`
- Create: `tests/cpp/cell_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/STATUS.md`

**Interfaces:**
- Consumes: なし
- Produces: 上の `Cell`、`AtomTable`、`Heap`、`ResourceLimit`、`pack_functor`

- [ ] **Step 1: ADR 0001 を確認する**

`docs/adr/0001-cell-representation.md` の状態行を読む。`承認待ち` のままなら、コードも ADR 0002 も書かず、STATUS の未解決に `ADR 0001 が未承認のため Cell を書いていない` と残してこのタスクを終える。コミットはしない。利用者が状態行を承認済みに変えてから Step 2 へ進む。

- [ ] **Step 2: Write the failing test**

```cpp
mosa_test::check(mosa::pack_functor(1, 2) == ((std::uint64_t{1} << 14) | (std::uint64_t{2} << 3)),
                 "functor word");
mosa_test::check(mosa::functor_arity(mosa::pack_functor(3, 1024)) == 1024, "arity 1024");
mosa::AtomTable atoms;
mosa_test::check(atoms.intern("foo") == 1 && atoms.intern("foo") == 1, "intern stable");
mosa_test::check(atoms.intern("") == 2 && atoms.intern("") != 0, "empty atom is not zero");
mosa_test::check(atoms.ascii(atoms.intern("abc")), "ascii flag");
auto kanji = atoms.intern("親");
mosa_test::check(!atoms.ascii(kanji), "non ascii");
mosa_test::check(atoms.intern_char(U'\u6771') == atoms.intern("東"), "char atom");
auto composed = atoms.intern("\u304c");
auto decomposed = atoms.intern("\u304b\u3099");
mosa_test::check(composed != decomposed, "no normalization");
mosa::Heap heap(atoms, 2);
auto first = heap.fresh_ref();
mosa_test::check(heap.unbound(first), "fresh ref");
bool threw = false;
try {
  (void)heap.fresh_ref();
} catch (const mosa::ResourceLimit& error) {
  threw = std::string_view{error.what()} == "ヒープの上限を超えました";
}
mosa_test::check(threw && heap.unbound(first), "cap throws and keeps the first ref");
mosa::Heap wide(atoms);
auto small = wide.integer((std::int64_t{1} << 60) - 1);
auto edge = wide.integer(std::int64_t{1} << 60);
auto neg = wide.integer(-(std::int64_t{1} << 60));
auto boxed_neg = wide.integer(-(std::int64_t{1} << 60) - 1);
mosa_test::check(small.tag() == mosa::Tag::Int && edge.tag() == mosa::Tag::IntBox, "positive edge");
mosa_test::check(neg.tag() == mosa::Tag::Int && boxed_neg.tag() == mosa::Tag::IntBox, "negative edge");
mosa_test::check(wide.integer_value(edge) == (std::int64_t{1} << 60), "box value");
auto term = wide.structure(atoms.intern("f"), std::array<mosa::Cell, 2>{wide.atom(atoms.intern("a")), small});
mosa_test::check(term.tag() == mosa::Tag::Str && wide.functor_arity(term) == 2 &&
                     wide.functor_atom(term) == atoms.intern("f"),
                 "structure");
```

`std::array` と `std::string_view` のインクルードをテストに書く。`structure` の引数は `std::span<const Cell>` なので、配列から渡せる。

- [ ] **Step 3: Run test to verify it fails**

Run: `cmake --build build --target cell_test`

Expected: ヘッダが無く、コンパイルに失敗する

- [ ] **Step 4: ADR 0002 と Cell を実装する**

`docs/adr/0002-cell-layout.md` の状態は `この設計で承認済み` とする。背景は、61bit に収まらない整数のタグ番号と、ペイロードが項ではないことを実装前に固定する、という 3 行にする。決定は、タグ 0 から 7 が Ref、Atom、Int、Str、Lis、Float、Big、IntBox であること、即値は `-2^60` 以上 `2^60` 未満であること、関数語は下位 3bit が 0 で続く 11bit がアリティ、続く 32bit がアトム番号であること、の 3 行にする。捨てた案は、Big を int64 の箱に使わないこと、全語を正当な Cell にしないこと、の 2 行にする。影響は、フェーズ7の GC が線形走査で語を項とみなさないこと、を含めて 3 行以内にする。

ヒープは `std::vector<std::uint64_t>` に一度だけ `reserve` する。再配置しない。未束縛の REF は、その語の番地を値にする。タグは 0 なので番地をそのまま書く。Float のペイロードは `std::bit_cast<std::uint64_t>(double)` の 1 語、IntBox のペイロードは `std::bit_cast<std::uint64_t>(std::int64_t)` の 1 語である。リストは先頭と末尾の 2 語を置き、`Lis` タグを足したポインタを返す。空リスト用の特別なタグは作らない。`Big` を返す関数は作らない。

- [ ] **Step 5: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS。版は `phase 0` のままである

- [ ] **Step 6: Commit**

STATUS の完了を `Cell とヒープとアトム`、次を `ユニフィケーション` にする。未解決が ADR 0001 だけなら、その行は消す。

```bash
git add docs/adr/0002-cell-layout.md include/mosa/cell.hpp include/mosa/atom.hpp \
  src/core/atom.cpp include/mosa/heap.hpp src/core/heap.cpp tests/cpp/cell_test.cpp \
  CMakeLists.txt docs/STATUS.md
git commit -m "[phase1] Cell とヒープとアトムを追加（タグ境界と ResourceLimit が通る）"
```

### Task 5: ユニフィケーション

**Files:**
- Create: `include/mosa/unify.hpp`
- Create: `src/core/unify.cpp`
- Create: `tests/cpp/unify_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/STATUS.md`

**Interfaces:**
- Consumes: `Heap`、`AtomTable`、`Cell`
- Produces: `unify`

- [ ] **Step 1: Write the failing test**

```cpp
mosa::AtomTable atoms;
mosa::Heap heap(atoms);
auto a = heap.atom(atoms.intern("a"));
auto b = heap.atom(atoms.intern("b"));
auto x = heap.fresh_ref();
auto y = heap.fresh_ref();
mosa_test::check(mosa::unify(heap, a, a), "atom");
mosa_test::check(!mosa::unify(heap, a, b) && heap.unbound(x), "atom mismatch");
mosa_test::check(mosa::unify(heap, x, y) && !heap.unbound(x), "var var");
auto f = atoms.intern("f");
auto cycle_var = heap.fresh_ref();
auto cycle = heap.structure(f, std::array<mosa::Cell, 1>{cycle_var});
mosa_test::check(mosa::unify(heap, cycle_var, cycle), "no occurs check");
mosa_test::check(heap.deref(cycle_var).tag() == mosa::Tag::Str, "x bound to f(x)");
auto left_var = heap.fresh_ref();
auto left = heap.structure(f, std::array<mosa::Cell, 2>{left_var, a});
auto right = heap.structure(f, std::array<mosa::Cell, 2>{b, b});
mosa_test::check(!mosa::unify(heap, left, right) && heap.unbound(left_var), "failure unbinds");
auto zero = heap.floating(0.0);
auto neg_zero = heap.floating(-0.0);
auto nan = heap.floating(std::numeric_limits<double>::quiet_NaN());
mosa_test::check(mosa::unify(heap, zero, neg_zero), "signed zero");
mosa_test::check(!mosa::unify(heap, nan, nan), "nan");
auto big = heap.integer(std::int64_t{1} << 60);
auto big2 = heap.integer(std::int64_t{1} << 60);
mosa_test::check(mosa::unify(heap, big, big2), "boxed int");
```

選択点スタック型をテストから参照しない。`unify.cpp` にもその型を書かない。

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target unify_test`

Expected: `unify` が無く、リンクに失敗する

- [ ] **Step 3: `unify` を実装する**

開始時のヒープ長を境界にする。その境界より前の REF だけをトレイルする。失敗したらトレイルを戻し、ヒープ長も開始時に戻す。成功したら束縛を残す。再帰で引数を揃えるとき、境界は最初の呼び出しのままにする。訪問集合は持たない。浮動小数は `==` で比べる。整数は `integer_value` で比べる。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS

- [ ] **Step 5: Commit**

STATUS の完了を `ユニフィケーション`、次を `リーダ` にする。

```bash
git add include/mosa/unify.hpp src/core/unify.cpp tests/cpp/unify_test.cpp \
  CMakeLists.txt docs/STATUS.md
git commit -m "[phase1] 出現検査なしのユニフィケーションを追加（失敗時に束縛が戻る）"
```

### Task 6: リーダ

**Files:**
- Create: `include/mosa/reader.hpp`
- Create: `src/read/reader.cpp`
- Create: `src/read/reader_bracket.cpp`
- Create: `tests/cpp/reader_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/STATUS.md`

**Interfaces:**
- Consumes: `Lexer`、`OperatorTable`、`Heap`、`AtomTable`、`SyntaxFailure`
- Produces: `Reader`、`ReadOutcome`、`ReadTerm`、`VarName`、`DoubleQuotes`

- [ ] **Step 1: Write the failing test**

ヘルパは `AtomTable` と `Heap` と `OperatorTable::iso_core()` を作り、`Reader` の最初の `next()` を返す。表明は次である。

- `foo.` はアトム `foo`。`failure` は空、`eof` は false
- `-42.` のタグは `Str`、関手は `-`、アリティは 1、引数の `integer_value` は 42。タグは `Int` ではない値として、項全体が `Int` ではない
- `a+b*c.` は `+` の引数が `a` と `*` である。`(a+b)*c.` は `*` の引数が `+` と `c` である
- `a-b-c.` は左の引数が `-` である。`a^b^c.` は右の引数が `^` である
- `f(a, b).` のアリティは 2。`f((a, b)).` のアリティは 1 で、その引数の関手は `,`
- `[].` はアトム `[]` であり `Lis` ではない。`[a, b].` は `Lis`。`[H|T].` は `Lis` で、末尾は変数 `T`。`[a|b|c].` は `UnexpectedToken`
- `{a}.` の関手は `{}` でアリティ 1。`{}.` はアトム `{}`。`{a, b}.` の引数の関手は `,`
- `f(X, X).` の 2 引数は同じ `raw`。`names` は 1 つで name は `X`。`f(_, _).` の 2 引数の `raw` は異なり、`names` は空
- `f(A, A, B).` の `singletons` は name `B` だけである
- `"ab".` は既定で `Lis` の整数 97 と 98。`DoubleQuotes::Chars` では 1 文字アトム `a` と `b`。`DoubleQuotes::Atom` ではアトム `ab`。`"".` は `Codes` でアトム `[]`、`Atom` でアトム `''`
- 空の入力の `next()` は `eof` だけが true。`end_of_file.` はアトムであり `eof` は false
- `（a）.` は `UnexpectedToken`。例外は出ない
- 引数 1024 個の `f(a,a,...).` は成功する。1025 個は `MaxArity` で、`line` と `column` は 1 以上
- `Reader` に渡す表へ `define(700, Xfx, "は")` したあと、`太郎 は 学生.` の関手は `は` でアリティは 2
- コメントを挟んだ `/* c */ foo.` はアトム `foo`

ソース文字列は終端ピリオドを含める。

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target reader_test`

Expected: `Reader` が無く、コンパイルに失敗する

- [ ] **Step 3: Pratt リーダを実装する**

`read(最大優先度)` は、その優先度以下の項を読む。演算子の引数の優先度は ISO 6.3.4 に合わせる。`xfx` は両側が演算子より小さい。`xfy` は左が小さく右は以下。`yfx` は左が以下で右は小さい。`fx` と `xf` は引数が小さい。`fy` と `yf` は引数が以下である。関数の引数とリスト要素は最大優先度 999 で読む。括弧の中は 1200 で読む。

`[` の直後が `]` ならアトム `[]` である。`|` はリストの区切りで、1 つだけ置ける。空の `{}` はアトム `{}` である。空でない `{...}` は関手 `{}` のアリティ 1 で、中を優先度 1200 で読む。

文字列は `DoubleQuotes` に従ってリストかアトムを作る。コードポイントは `integer` で、1 文字アトムは `intern_char` で作る。変数名は解読後の `text` を使う。`_` ちょうど 1 文字の変数は、出現ごとに新しい REF とし、`names` に入れない。

項を 1 つ読んだあと、終端ピリオドが無ければ `IncompleteTerm` である。次の非レイアウトが入力の終わりなら `eof` だけを返す。ヒープを増やす途中で `ResourceLimit` が出たら、それは構文の `failure` に変えず、そのまま投げる。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS

- [ ] **Step 5: Commit**

STATUS の完了を `リーダ`、次を `ライタ` にする。

```bash
git add include/mosa/reader.hpp src/read/reader.cpp src/read/reader_bracket.cpp \
  tests/cpp/reader_test.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase1] Pratt リーダを追加（優先度とリストと変数共有が通る）"
```

### Task 7: ライタ

**Files:**
- Create: `include/mosa/writer.hpp`
- Create: `src/write/writer.cpp`
- Create: `src/write/term_equal.cpp`
- Create: `tests/cpp/writer_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `docs/STATUS.md`

**Interfaces:**
- Consumes: `Heap`、`AtomTable`、`OperatorTable`、`VarName`、`Reader`
- Produces: 契約節の `write_term` と `same_term`

- [ ] **Step 1: Write the failing test**

`OperatorTable::iso_core()` を `write_term` に渡す。`WriteOptions` の既定は `quoted == true`、`ignore_ops == false` である。

```cpp
mosa_test::check(written("a+b*c.") == "a+b*c", "priority");
mosa_test::check(written("(a+b)*c.") == "(a+b)*c", "parens");
mosa_test::check(written("a-b-c.") == "a-b-c", "left assoc");
mosa_test::check(written("a^b^c.") == "a^b^c", "right assoc");
mosa_test::check(written("f(a, b).") == "f(a, b)", "comma space");
mosa_test::check(written("[a, b].") == "[a, b]", "list");
mosa_test::check(written("[H|T].") == "[H|T]", "list bar");
mosa_test::check(written("{a, b}.") == "{a, b}", "curly");
mosa_test::check(written("東京.") == "東京", "unquoted japanese");
mosa_test::check(written("'東 京'.") == "'東 京'", "quoted space");
mosa_test::check(written("'\\n'.") == "'\\n'", "escape newline on output");
```

`written` はリーダで 1 項読み、`write_term` にその `names` を渡した結果である。入力のピリオドは出力に含めない。

名前の無い共有は、リーダを通さずヒープで作る。

```cpp
auto var = heap.fresh_ref();
auto shared = heap.structure(atoms.intern("f"), std::array<mosa::Cell, 2>{var, var});
auto once = heap.structure(atoms.intern("f"), std::array<mosa::Cell, 1>{heap.fresh_ref()});
mosa_test::check(mosa::write_term(heap, ops, shared, mosa::WriteOptions{}, {}) == "f(_G1, _G1)",
                 "shared unnamed");
mosa_test::check(mosa::write_term(heap, ops, once, mosa::WriteOptions{}, {}) == "f(_)",
                 "single unnamed");
```

続けて、前置マイナスを重ねた項 `-( -(atom a) )` の出力が `- -a` であること。アトム `Foo` の出力が `'Foo'`、アトム `_` の出力が `'_'` であること。`ignore_ops` が true のとき `[a]` の出力が `'.'(a, [])` であること。整数セル `-42` の出力が `-42` であること。`floating(-0.0)` の出力が `-0.0` であること。`floating(1.0)` の出力が `.` を含み `e` も `E` も含まないこと。`floating(1.5e20)` の出力が `e` を含み `E` を含まないこと。

`same_term` は、`f(A, B)` と `f(B, A)` をそれぞれの名前表で比べて false、同じ項の `f(A, A)` を同じ名前表で比べて true、共有しない無名 2 つと共有する無名 1 つを比べて false、名前のある変数と `_` を比べて false、である。

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target writer_test`

Expected: `write_term` が無く、リンクに失敗する

- [ ] **Step 3: ライタと `same_term` を実装する**

空白は、隣り合うトークンが 1 つの図形トークンや名前に融合するときだけ置き、加えてコンマの直後に空白を 1 つ置く。中置 `+` の前後には置かない。引用は、空アトム、変数として読める綴り、図形トークンでも小文字の名前でもソロ 1 文字でも `[]` でも `{}` でもない綴り、に付ける。引用の中の改行、タブ、`\`、`'` は記号エスケープで書く。非 ASCII は `\u` にせず UTF-8 のまま書く。

`ignore_ops` が false のとき、`'.'/2` はリスト、`'{}'/1` は中括弧、演算子は演算子表記である。true のとき、それらは関数表記で、`.` は `'.'` と引用する。被演算子の括弧は ISO 6.3.4 の優先度で省く。

名前表にある REF はその綴りで書く。無い REF は、その書き出しの中で 2 回目以降が必要なら `_G` と出現順の十進番号、1 回なら `_` である。負の整数セルは十進の符号付きで書く。浮動小数は `std::to_chars` の `chars_format::general` を使い、小数点も `e` も無いときは `.0` を足す。負のゼロは `-0.0` と書く。

`same_term` はユニフィケーションを呼ばない。左右のヒープは別でよい。アトムは番号ではなく `name` のバイト列で比べる。名前のある変数は名前が同じときだけ対応する。名前の無い変数は一対一で対応し、同じ REF の再出現は同じ相手でなければならない。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS

- [ ] **Step 5: Commit**

STATUS の完了を `ライタ`、次を `syntax.pl の往復` にする。

```bash
git add include/mosa/writer.hpp src/write/writer.cpp src/write/term_equal.cpp \
  tests/cpp/writer_test.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase1] ライタを追加（引用と括弧と -0.0 が通る）"
```

### Task 8: 往復とフェーズ1完了

**Files:**
- Create: `include/mosa/roundtrip.hpp`
- Create: `src/read/roundtrip.cpp`
- Create: `tests/pl/syntax.pl`
- Modify: `src/main_mosatest.cpp`
- Modify: `src/version.cpp`
- Modify: `CMakeLists.txt`（版の正規表現と `roundtrip` テスト）
- Modify: `tests/pl/README.md`
- Modify: `docs/STATUS.md`

**Interfaces:**
- Consumes: `Reader`、`write_term`、`same_term`、`OperatorTable::iso_core`、`define`
- Produces: `run_roundtrip`。版文字列 `mosa 0.0.1 (phase 1)`

- [ ] **Step 1: Write the failing test**

`tests/pl/syntax.pl` に、次のケースをこの順で書く。`% case: ` は行頭から始める。`% expect: ` の右側は、その行の改行を除いて完全一致である。

```prolog
% case: atom
foo.

% case: quoted_space
% expect: 'a b'
'a b'.

% case: empty_atom
''.

% case: japanese_atom
% expect: 親(太郎, 一郎)
親(太郎, 一郎).

% case: non_bmp_atom
% expect: 𠮷
𠮷.

% case: quoted_japanese
% expect: '東 京'
'東 京'.

% case: shared_var
f(X, X).

% case: distinct_anon
f(_, _).

% case: integer
42.

% case: unary_minus
% expect: -42
-42.

% case: radix_bin
0b1010.

% case: radix_oct
0o17.

% case: radix_hex
0xFF.

% case: radix_underscore
1_000.

% case: boxed_int
9223372036854775807.

% case: char_a
0'a.

% case: char_hiragana
0'あ.

% case: char_non_bmp
0'𠮷.

% case: float_simple
1.5.

% case: float_one
1.0.

% case: float_exponent
1.5e-2.

% case: list_empty
[].

% case: list_pair
[a, b].

% case: list_bar
[H|T].

% case: curly_one
{a}.

% case: curly_comma
{a, b}.

% case: compound
f(a, b).

% case: priority_times
a+b*c.

% case: priority_plus
(a+b)*c.

% case: assoc_left
a-b-c.

% case: assoc_right
a^b^c.

% case: comma_two
f(a, b).

% case: comma_one
f((a, b)).

% case: escape_n
'\n'.

% case: escape_x
'\x41\'.

% case: escape_u
'\u3042'.

% case: escape_cont
'ab\
c'.

% case: double_quotes
"ab".

% case: comment
/* skip */ foo.

% case: japanese_op
:- op(700, xfx, は).
太郎 は 学生.
```

`escape_cont` の引用は、`ab`、バックスラッシュ、改行、`c` の順である。1 ケースの項は 1 つだけである。`japanese_op` だけが、その前に `op/3` の指示を持つ。

CTest は次を足す。

```cmake
add_test(NAME roundtrip COMMAND $<TARGET_FILE:mosatest> --roundtrip ${CMAKE_SOURCE_DIR}/tests/pl/syntax.pl)
```

版の `PASS_REGULAR_EXPRESSION` は、このタスクで `phase 0` を `phase 1` に変える。タスク 8 の実装が終わるまで、この期待値の変更と `version.cpp` の変更は同じコミットに置く。Step 2 の時点では版テストを変えない。往復テストだけが失敗する。

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --test-dir build -R roundtrip --output-on-failure`

Expected: `mosatest` が `--roundtrip` を知らず、終了コード 1 で FAIL

- [ ] **Step 3: ハーネスと版を実装する**

行頭の `% case: ` で分割する。識別子はその後ろを、行末の改行を除いてそのまま使う。`% expect: ` で始まる行はリーダへ渡さず、接頭辞の後ろを期待文字列にする。ケースごとに新しい `AtomTable`、容量既定の `Heap`、`OperatorTable::iso_core()` を作る。前のケースの `define` は見えない。

本文を `Reader` で読む。項が `:-` のアリティ 1 で、その引数が `op` のアリティ 3 なら、第 1 引数の整数、第 2 引数のアトムを `Specifier` に写したもの、第 3 引数のアトム名で `define` する。`define` が `Ok` でなければ、そのケースは失敗である。それ以外の `:-` も失敗である。指示のあと項が 1 つあり、その後は `eof` だけが許される。

`write_term` は `quoted true`、`ignore_ops false`、読んだ `names` で呼ぶ。期待文字列があれば完全一致を見る。出力に `.` を足し、新しい `AtomTable` と `Heap` と、ケース終了時の演算子表のコピーでもう一度読む。`same_term` は左右で別のヒープを受け取り、アトムは番号ではなく名前で比べる。true ならそのケースは成功である。

`main` は、引数のどれかが `--version` なら版だけを出して 0 を返す。`argv[1]` が `--roundtrip` で `argv[2]` がパス、引数がそれだけのとき `run_roundtrip` の戻り値を返す。それ以外は標準エラーに `mosatest: 使い方: mosatest --roundtrip <ファイル>` と出して 1 を返す。ファイルはバイナリで読む。パスの文字コードは変換しない。

`version_line()` の戻り値を `mosa 0.0.1 (phase 1)` にする。`tests/pl/README.md` は、`syntax.pl` が往復の例でありゴールとしては実行しない、という 1 行にする。

規格第6章の例がこのファイルより細かく手元にあるときは、例を同じ形式で足す。既存の期待文字列は変えない。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: `roundtrip` と 3 つの版テストを含む全テストが PASS。版の出力は `mosa 0.0.1 (phase 1)`

- [ ] **Step 5: Commit**

STATUS を次で置き換える。

```text
フェーズ: 1 完了
進行中: なし
完了: syntax.pl の往復と日本語アトムの往復。版は mosa 0.0.1 (phase 1)
次: フェーズ2の実行の核（IR とトランポリン）
未解決: なし
```

```bash
git add include/mosa/roundtrip.hpp src/read/roundtrip.cpp tests/pl/syntax.pl \
  tests/pl/README.md src/main_mosatest.cpp src/version.cpp CMakeLists.txt docs/STATUS.md
git commit -m "[phase1] syntax.pl の往復が通りフェーズ1を完了（mosatest --roundtrip が 0）"
```
