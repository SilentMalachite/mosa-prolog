# フェーズ10 多倍長整数 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 整数を上限なし（旗 `bounded` が `false`）にする。int64 に収まる値は今の Int と IntBox のまま今の速さで動かし、収まらない値だけを予約タグ Big の箱に入れる。読み書き、算術、比較、単一化、標準順序、複製、GC、`mosac` の訳がすべて同じ規則で動く。

**Architecture:**
- ヒープの外の値の型 `BigInt`（`include/mosa/bigint.hpp`、`src/core/bigint*.cpp`）が、標準ライブラリだけで計算を受け持つ。
- ヒープの Big の箱は、頭の語（語数と符号）と、64 ビットの絶対値の語の並びでできている。正規形の規則は「int64 に収まる値は決して Big にしない」。
- Big を受ける側（GC、単一化、順序、writer、複製、IR）を先に直す。そのあとで作る側（字句解析、算術）を入れる。

**Tech Stack:** C++20 標準ライブラリ、CMake 3.25、`mosatest`（`tests/pl` の `test(Name, Goal, Expect)` 行。成功は出力なしで終了コード 0）、`tests/cpp/harness.hpp`（`mosa_test::check`）、Logtalk 準拠スイート。

**Spec:** `docs/superpowers/specs/2026-11-12-phase10-bignum-design.md`（承認済み、2026-11-12、所有者の決定 A）

## Global Constraints

- 依存は C++20 標準ライブラリと CMake 3.25 以上だけにする。GMP、`__int128`、コンパイラの組み込み（`_umul128`、`__builtin_*`）は使わない。64 ビットどうしの積は 32 ビットに割る。`<bit>` は使ってよい。
- `-Wall -Wextra -Wpedantic`（MSVC は `/W4`）の警告はエラーとして扱う。macOS、Linux（GCC と Clang）、Windows（MSVC）の 3 OS で通す。
- 生の `new`/`delete`、`reinterpret_cast`、C 配列、マクロ、コンパイラ拡張を使わない。関数は 60 行以内、ファイルは 800 行以内にする。`src/builtins/arith.cpp`（776 行）と `src/machine/compile.cpp`（792 行）は、書き足す前に分ける。
- エラーは `raise_formal`、`formal_*`、`raise_type` の族だけで投げる。アトム名を文字列リテラルで書かない（既存の型名の書き方に合わせる）。
- 組み込み述語の C++ は「引数の検査 → 本処理 → 束縛」の順に書く。ファイル頭のコメントには、規格の節番号、引数のモード、投げうるエラー、テストファイル名を書く。
- 規格の Examples と自前の行は、実装の前に `tests/pl/` に書く（赤にしてから緑にする）。エラーのテストは `Formal` だけを比べる。
- 期待値を変えてよいのは、設計の「仕様の変更で期待値が変わるテスト」の表にある行だけである。そのほかに変わる行を見つけたら、直さずに止めて、所有者に問う。
- 識別子は英語、コメントとコミットメッセージは日本語で書く。文書は英語が正で、`*.ja.md` を同じコミットで直す。
- コミットは論理単位ごとに 1 つ。メッセージは 1 行で、`[phase10] 内容（確認方法）` の形にする。
- 1 セッションでは 1 タスクだけを行う。始めに `docs/STATUS.md` の「In progress」にタスクを書く。終わりに `cmake --build build && ctest --test-dir build` を通し、完了したことと次にやることを書く（15 行以内）。
- スイートの結果を変えるコミットでは、`./build/mosatest --suite tests/conformance` と `ctest --test-dir build -L diff` を流す。`docs/conformance.md`、`docs/conformance.json`、`docs/agreement.json` を同じコミットに入れる（CI の「Check badges」がこの差分を見る）。
- 新しいタグを扱うとき、`default:` に頼らない。`-Wswitch` を効かせるため、`switch` に Big の場合を書く。
- レビュアーは opus を使う。タスク 3、4、11 のレビューは、`MOSA_GC_STRESS=1` の下で `ctest` が通ることを条件にする。
- 既定の扱い（iso-notes の問い 2 つ。タスク 1 で書く）:
  - `max_integer` と `min_integer` は、int64 の境界を返すまま残す。
  - double の範囲を越える整数と浮動小数点数の比較、`min`、`max` は、double に変えて比べる。変えられないときは `evaluation_error(float_overflow)` にする。

## Review Focus

1. **境界の値の正規形。** 2^60−1、2^60、2^63−1、2^63、−2^63、−2^63−1、2^64 が、どの道を通っても同じ表現になること。例: `X is 9223372036854775807 + 1 - 1, X == 9223372036854775807`。Big が int64 の値を持つと `==` と索引が誤る。タスク 6 の `norm_*` で確かめる。
2. **負の Big のビット演算とシフト。** `-(2^100) >> 1`、`\ (2^70)`、`-(2^70) /\ 255` などが、2 の補数で無限に符号を延ばした意味になること。タスク 7 の `bits_neg_*` で確かめる。
3. **Big を第 1 引数に持つ節の索引。** 最下位の語だけが違う 2 つの Big の頭を、取り違えないこと。インタプリタと `mosac` の両方で確かめる。タスク 4 の `index_big_*` と、タスク 10 の `tests/mosac/bigint.pl` で確かめる。
4. **演算子の中の負の Big の書き出しと読み戻し。** `writeq(- (-(2^70)))`、`writeq(1 - (-(2^70)))`、`writeq(-(2^70))` が読み戻せる字面になること。タスク 5 の `wq_neg_*` で確かめる。
5. **巨大な結果がすぐ止まること。** `X is 1 << (1 << 62)`、`X is 2^(2^62)`、`length(L, 2^70)` が、確保の前（または上限）で止まり、固まらないこと。`tests/cpp/bigint_limit_test.cpp` で確かめる。このテストは、タスク 6 で `state_limit_test.cpp` と同じ形で作る。16 MiB の上限のマシンで問い合わせを流し、`ResourceLimit` で止まることを確かめる。タスク 6、7、8 が、それぞれの行を足す。

---

### Task 1: ADR 0025 と iso-notes の問い、SPEC 第9節と第1節（文書だけ）

**Files:**
- Create: `docs/adr/0025-bignum-box.md`、`docs/adr/0025-bignum-box.ja.md`
- Modify: `docs/adr/0001-cell-representation.md` と `.ja.md`（「Consequences」に 1 行。BIG の使い方は ADR 0025 に従い、`max_integer` は int64 の境界）、`docs/adr/0002-cell-layout.md` と `.ja.md`（「Consequences」に 1 行。Big の箱は頭の語を持つ）、`docs/adr/README.md` と `.ja.md`（一覧）
- Modify: `docs/iso-notes.md` と `.ja.md`（新しい節 2 つ）
- Modify: `docs/SPEC.md` と `.ja.md`（第1節の非目標の 1 行、第9節の表の 10 行目）
- Modify: `README.md` と `.ja.md`（バッジを `phase-9_done` から `phase-10_in_progress` へ、フェーズの表に 10 行目）
- Modify: `docs/STATUS.md`（1 行目を「Phase 10 open (arbitrary-precision integers, opened 2026-11-12, owner decision A)」に）

- [ ] **Step 1: ADR 0025 を書く。** 形は「Context、Decision、Rejected alternatives、Consequences」で、それぞれ 3 行以内。状態は「Proposed」。Decision に書く内容:
  - 箱: タグ 6 の Cell は頭の語を指す。頭の語は `(n << 4) | (negative << 3)`（下位 3 ビットは 0）。その後に 64 ビットの絶対値の語が n 個（下位が先、最上位の語は 0 でない）続く。値は必ず int64 の外にある。GC は n+1 語を Raw として印を付け、移す。
  - IR: `IrTag::Big` を足す。`integer` は負なら 1、正なら 0。`args` は絶対値の語を、下位から順に `IrTag::Int` の子として持つ（`integer` に `std::bit_cast<std::int64_t>(word)`）。`ExportedTerm::Kind::Big` と `sol::CopyKind::Big` も同じ持ち方にする（子の節点を Int の種類にする）。
  - 索引: `KeyKind::Big` を足す。鍵は `big_key(negative, magnitude)`（FNV-1a）で、`Clause::key_int` の欄に入れる。
  - `mosac`: 定数は `static constexpr std::array<std::uint64_t, N>` と `rt::put_big` で出す。
  - Rejected alternatives: `IrTerm` に値の欄を足す（すべての節点が大きくなる）。節ごとの定数表（実行時に訳すゴールは節を持たない）。2 の補数で持つ（積と商が書きにくい）。
- [ ] **Step 2: iso-notes に 2 つの節を足す。** どちらも「A か B か」の形で、確認までの既定を書く。
  - 「7.11.1.2 and 7.11.1.3 `max_integer` and `min_integer` when `bounded` is false」。A: int64 の境界を返す（既定）。B: 旗を無くす。
  - 「8.7, 9.3.8 and 9.3.9 an integer beyond the double range against a float」。A: double に変え、範囲を越えたら `float_overflow`（既定）。B: 正確に比べる。
- [ ] **Step 3: SPEC を直す。**
  - 第1節: 非目標の多倍長整数の行を「GMP and other arbitrary-precision libraries (phase 10 implements arbitrary-precision integers with the standard library only)」に替える。
  - 第9節: 表に 10 行目を足す。内容は「Arbitrary-precision integers: the Big box (ADR 0025), unbounded arithmetic, reading and writing, `bounded` is `false`」。完了条件は設計の「完了条件」の 1〜3 と 5 を写す。
- [ ] **Step 4: README のバッジと表、STATUS の 1 行目を直す。**
- [ ] **Step 5: 確かめる。** `grep -n 'phase-10\|phase 10\|フェーズ10' README.md README.ja.md docs/SPEC.md docs/SPEC.ja.md docs/STATUS.md` が 5 ファイルすべてに当たり、`ctest --test-dir build -R manual` が通る（マニュアルの照合は文書を読む）。
- [ ] **Step 6: Commit.** メッセージは `[phase10] フェーズ10を開き ADR 0025（提案）と iso-notes の問い 2 つを書く（文書だけ）`。
- [ ] **Step 7: 止まる。** 所有者に「ADR 0025 を承認するか（A）、直すか（B）」を問う。承認を得たら ADR の状態を「Approved (日付)」にしてコミットし、タスク 2 に入る。

---

### Task 2: `BigInt`（ヒープの外の値の型）

**Files:**
- Create: `include/mosa/bigint.hpp`、`src/core/bigint.cpp`（加減乗、比較、シフト、ビット演算）、`src/core/bigint_div.cpp`（商、gcd、累乗）、`src/core/bigint_text.cpp`（字面と double）
- Modify: `CMakeLists.txt`（`mosa_lib` の源に 3 つ）、`tests/tests.cmake`（`bigint_test`）
- Test: `tests/cpp/bigint_test.cpp`

**Interfaces:**
- Produces（`include/mosa/bigint.hpp`、名前空間 `mosa`）:

```cpp
class BigInt {
 public:
  BigInt() = default;                                   // 0
  explicit BigInt(std::int64_t value);
  // 絶対値の 64 ビットの語（下位が先）と符号から作る。上位の 0 の語は落とし、0 の符号は正にする。
  [[nodiscard]] static BigInt from_words(std::span<const std::uint64_t> magnitude, bool negative);
  [[nodiscard]] bool negative() const;
  [[nodiscard]] bool is_zero() const;
  [[nodiscard]] std::optional<std::int64_t> to_int64() const;
  [[nodiscard]] std::vector<std::uint64_t> words() const;  // 絶対値。下位が先、上位の 0 なし、0 は空
  [[nodiscard]] std::uint64_t bit_length() const;          // 絶対値のビット数。0 は 0
  [[nodiscard]] BigInt operator-() const;
  friend bool operator==(const BigInt&, const BigInt&);
  friend std::strong_ordering operator<=>(const BigInt&, const BigInt&);
  friend BigInt operator+(const BigInt&, const BigInt&);
  friend BigInt operator-(const BigInt&, const BigInt&);
  friend BigInt operator*(const BigInt&, const BigInt&);
};
struct BigQuotient { BigInt quotient; BigInt remainder; };
[[nodiscard]] BigQuotient divide_truncate(const BigInt& a, const BigInt& b);  // 前提 b ≠ 0。余りは a の符号
[[nodiscard]] BigQuotient divide_floor(const BigInt& a, const BigInt& b);     // 前提 b ≠ 0。余りは b の符号
[[nodiscard]] BigInt shift_left(const BigInt& a, std::uint64_t bits);
[[nodiscard]] BigInt shift_right(const BigInt& a, std::uint64_t bits);        // 床に丸める（算術シフト）
[[nodiscard]] BigInt bit_and(const BigInt& a, const BigInt& b);               // 以下 4 つは 2 の補数の意味
[[nodiscard]] BigInt bit_or(const BigInt& a, const BigInt& b);
[[nodiscard]] BigInt bit_xor(const BigInt& a, const BigInt& b);
[[nodiscard]] BigInt bit_not(const BigInt& a);
[[nodiscard]] BigInt gcd(const BigInt& a, const BigInt& b);                   // 非負
[[nodiscard]] BigInt power(const BigInt& base, std::uint64_t exponent);
[[nodiscard]] std::string to_text(const BigInt& value, unsigned radix = 10);  // 負なら '-' を先に。小文字
// 数字だけ（符号、'_'、接頭辞なし）。radix は 2・8・10・16。数字でない字があれば nullopt。
[[nodiscard]] std::optional<BigInt> parse_magnitude(std::string_view digits, unsigned radix);
[[nodiscard]] std::optional<double> to_double(const BigInt& value);           // 最近接偶数。範囲外は nullopt
[[nodiscard]] BigInt from_integral_double(double value);                      // 前提: 有限で整数値
[[nodiscard]] std::uint64_t big_key(bool negative, std::span<const std::uint64_t> magnitude);  // src/core/fnv.hpp の式
```

- `include/mosa/cell.hpp` にも足す:
  - `constexpr std::uint64_t big_header(std::uint64_t words, bool negative)`
  - `constexpr std::uint64_t big_header_words(std::uint64_t header)`
  - `constexpr bool big_header_negative(std::uint64_t header)`
  - `constexpr bool is_integer_tag(Tag tag)`（Int、IntBox、Big）

- [ ] **Step 1: 失敗するテストを書く。** `tests/cpp/bigint_test.cpp` に、次の検査を `mosa_test::check` で書く（値は Python で確かめたもの）。
  - `round_trip`: `to_text(*parse_magnitude("1267650600228229401496703205376", 10)) == "1267650600228229401496703205376"`。16 進の `"ffffffffffffffffffff"` も往復する。
  - `int64_edges`: `BigInt(INT64_MIN).to_int64() == INT64_MIN`。`(BigInt(INT64_MAX) + BigInt(1)).to_int64()` は nullopt。`to_text(BigInt(INT64_MAX) + BigInt(1)) == "9223372036854775808"`。`to_text(BigInt(INT64_MIN) - BigInt(1)) == "-9223372036854775809"`。`to_text(BigInt(INT64_MAX) * BigInt(2)) == "18446744073709551614"`。
  - `div_signs`: P = 2^100 として、`divide_truncate(-P, 7)` が (−181092942889747057356671886482, −2) になる。`divide_floor(-P, 7)` が (−181092942889747057356671886483, 5) になる。
  - `shifts`: `shift_right(-P, 1) == -(2^99)`、`shift_right(P, 101) == 0`、`shift_right(-P, 101) == -1`、`shift_left(BigInt(1), 64) == 2^64`。
  - `bits`: Q = 2^70 として、`bit_not(Q) == -1180591620717411303425`、`bit_and(-Q, 255) == 0`、`bit_and(bit_not(Q), 255) == 255`、`bit_xor(Q, Q + 5) == 5`、`bit_or(-Q, 1) == -1180591620717411303423`。
  - `gcd_pow`: `gcd(P, power(6, 50)) == 1125899906842624`、`power(2, 100) == P`、`P * P == 1606938044258990275541962092341162602522202993782792835301376`。
  - `to_double`: 2^53+1 は `9007199254740992.0`、2^54+3 は `18014398509481988.0`（最近接偶数）、2^1024−2^970−1 は `1.7976931348623157e308`。2^1024−2^970 は nullopt（偶数への丸めで 2^1024 になる）。
  - `from_double`: `to_text(from_integral_double(1.0e32)) == "100000000000000005366162204393472"`、`from_integral_double(-1.0e20) == -100000000000000000000`。
  - `karatsuba`: 1,000 語と 1,500 語の乱数（決まった種の `std::mt19937_64`）の積が、試験の中に書いた 2 乗の掛け算（`schoolbook_multiply`。テストのファイルに置く）の積と一致する。
  - `factorial`: 1000! の 10 進の字面が 2568 桁で、`40238726007709377354` で始まり、`0000000000` で終わる。
  - `key`: `big_key` は等しい値で等しく、最下位の語だけが違う 2 つの値で違う。
- [ ] **Step 2: 走らせて失敗を確かめる。** `cmake --build build --target bigint_test` が、`bigint.hpp` が無いことで失敗する。
- [ ] **Step 3: `BigInt` を実装する。**
  - 語の幅は、内部で 32 ビットにしてもよい。`from_words` と `words` は 64 ビットの語で受け渡す。
  - 乗算は、閾値（測って決め、定数 `kKaratsubaWords` に書く）より上で Karatsuba にする。
  - 除算は Knuth の Algorithm D にする。
  - `to_text` は 10^19 ずつ割る 2 乗のアルゴリズムでよい。
  - `to_double` は上位 64 ビットと残りの有無（sticky）で最近接偶数に丸める。
- [ ] **Step 4: 走らせて通ることを確かめる。** `ctest --test-dir build -R '^bigint$' --output-on-failure` が PASS になる。
- [ ] **Step 5: Commit.** メッセージは `[phase10] BigInt を src/core に足す（bigint_test の 11 の検査が通る）`。

---

### Task 3: ヒープの Big の箱と、受ける側の 1 つ目

**Files:**
- Modify: `include/mosa/heap.hpp` と `src/core/heap.cpp`（箱）
- Modify: `src/machine/gc_mark.cpp:76-97`、`src/machine/gc_compact.cpp:72-78`
- Modify: `src/core/unify.cpp:108-121`、`src/machine/native.cpp:238`、`src/builtins/order.cpp:21-91`、`src/write/term_equal.cpp:80-94`
- Modify: `src/write/writer.cpp:400-406,416-428,472-481`（`'$VAR'(N)` の N が Big のときも含む）
- Modify: `src/machine/index.cpp:36-48,175-184`、`src/machine/clause_index.hpp:17`
- Modify: `src/builtins/types.cpp:20,24,182,190`（`integer/1`、`atomic/1`、`number/1`）、`src/builtins/arith.cpp:104-118`（`number_of`）
- Test: `tests/cpp/bigint_heap_test.cpp`（新規）、`tests/cpp/gc_test.cpp`（`check_big_relocation`）

**Interfaces:**
- Consumes: タスク 2 の `BigInt` と、`cell.hpp` の `big_header*` と `is_integer_tag`
- Produces（`Heap` のメンバ）:
  - `[[nodiscard]] Cell big(const BigInt& value)`: 正規形で作る。int64 なら `integer(int64)`、そうでなければ Big の箱。
  - `[[nodiscard]] BigInt big_value(Cell cell) const`: Int、IntBox、Big のどれでも受ける。
  - `[[nodiscard]] std::span<const std::uint64_t> big_magnitude(Cell cell) const`: Big だけを受ける。
  - `[[nodiscard]] bool big_negative(Cell cell) const`: Big だけを受ける。
  - `integer_value(Cell)` は Int と IntBox だけを受ける。前提を `assert` に書く。呼び手はすべて、先に Int か IntBox であることを確かめている（付録 B の 0）。

- [ ] **Step 1: 失敗するテストを書く。** `tests/cpp/bigint_heap_test.cpp` にマシンを作り、次の検査を書く。
  - `heap.big(2^100)` の Cell が `Tag::Big` で、`big_value` が 2^100 に戻る。`heap.big(BigInt(INT64_MAX))` は IntBox、`heap.big(BigInt(5))` は Int になる。
  - 別々に作った 2 つの 2^100 が `do_unify` で単一化でき、2^100 と 2^100+1 はできない。2^100 と IntBox の値も単一化できない。
  - `compare/3` の順が −2^100 < −2^63 < 0 < 2^63−1 < 2^100 で、浮動小数点数はすべての整数より前になる。`==` が値で比べる。
  - `write_canonical` が `-1267650600228229401496703205376` と書く。`writeq('$VAR'(Big))`（Big = 2^70）が `K45407370027592742439` と書く。
  - `integer/1`、`number/1`、`atomic/1` が真、`var/1`、`compound/1` が偽になる。
  - `X is Big + 0` が `evaluation_error(int_overflow)` になる（タスク 6 までの暫定）。
  - 索引の検査: 第 1 引数が 2^100 と 2^100+1 の節を 8 つ以上持つ述語で、`p(2^100 の Cell, X)` が 2^100 の節だけを選ぶ。

  `gc_test.cpp` には `check_big_relocation` を足す。Big を持つ項を作り、間にごみを置いて `garbage_collect/0` を 3 回呼び、値が変わらないことを確かめる（`check_boxed_relocation` と同じ形）。
- [ ] **Step 2: 走らせて失敗を確かめる。** `heap.big` が無いことでビルドが失敗する。
- [ ] **Step 3: 実装する。**
  - `Heap::big` は `allocate(n + 1)` に頭の語と語の並びを書き、`tagged(pointer, Tag::Big)` を返す。
  - GC: `mark_value` の早期の return から Big を外し、頭の語を読んで n+1 語を Raw として印を付ける。`relocate` も同じく Big を指す先として移す。
  - 単一化と `==`: タグが両方 Big なら、頭の語と全語を比べる。片方だけ Big なら偽にする（正規形の規則による）。
  - 順序: `rank_of` で Big を整数の順位 2 にする。`compare_scalar` は、どちらかが Big なら `big_value` どうしを比べる。
  - writer: `to_text(big_value(c))` を書く。`unsigned_number` の判断は int64 と同じにする。
  - 索引: `cell_key` と `clause_key` は、Big に `KeyKind::Big` と `big_key(...)` を返す。
  - `number_of`: Big なら `int_overflow` にする（暫定）。
- [ ] **Step 4: 走らせて通ることを確かめる。** `ctest --test-dir build -R 'bigint_heap|^gc$' --output-on-failure` が通り、`MOSA_GC_STRESS=1 ctest --test-dir build -R 'bigint_heap|^gc$'` も通る。そのあと ctest 全体が通る。
- [ ] **Step 5: Commit.** メッセージは `[phase10] ヒープの Big の箱と GC・単一化・順序・writer・索引・型の検査（bigint_heap_test と gc_test、MOSA_GC_STRESS=1 でも通る）`。

---

### Task 4: 複製と IR（受ける側の 2 つ目）

**Files:**
- Modify: `include/mosa/ir.hpp:10-29`（`IrTag::Big` を最後に足す）、`src/core/ir.cpp`（写しと移動で値を落とさない。子の持ち方なので変更は要らないはず。確かめる）
- Modify: `src/machine/compile.cpp:198-225`（`data_leaf`）、`:640-649`（`set_key`）、`:746-747`（頭部の検査）。書き足す前に分ける（800 行）。
- Modify: `src/machine/materialize.cpp:66-87`、`src/builtins/clause_term.cpp:116-117`、`src/machine/native.cpp:40-53`（`hash_term`）と `:173-182`（`native_eligible` は Big の定数を持つ節を外す）
- Modify: `src/machine/internal.hpp:252-265`、`src/machine/catch.cpp:39-48,136-138`
- Modify: `src/builtins/solutions_internal.hpp:14-23`、`src/builtins/solutions.cpp:384-397`、`src/builtins/solutions_answer.cpp:123-124`
- Modify: 「呼び出せる項」の判定: `src/machine/engine.cpp:380`、`src/builtins/database.cpp:197`、`src/codegen/check.cpp:49`、`src/machine/load_clauses.cpp:60`
- Modify: 勘定: `src/builtins/solutions.cpp:33,494`（`footprint`）、`src/machine/state_count.cpp:32-45`、`src/machine/compile.cpp:677-681`
- Test: `tests/cpp/bigint_copy_test.cpp`（新規）

**Interfaces:**
- Consumes: タスク 3 の `Heap::big`、`big_value`、`big_magnitude`、`big_negative`
- Produces:
  - `IrTag::Big`: `integer` は負なら 1。`args` は `IrTag::Int` の子で、絶対値の語を下位から持つ。
  - `ExportedTerm::Kind::Big` と `sol::CopyKind::Big`: 同じ持ち方。
  - `native_eligible` が偽を返す条件に「節の IR に `IrTag::Big` がある」を足す（タスク 10 で外す）。

- [ ] **Step 1: 失敗するテストを書く。** `tests/cpp/bigint_copy_test.cpp` で、Big = 2^100 と −2^100 の Cell を C++ から作り、`p(X)` の X に束縛して、次の道を通す。どの道でも `big_value` が元の値に戻ることを確かめる。
  - `copy_term/2`
  - `findall/3` と `bagof/3`（鍵に Big を持つ `bagof(X, member(K-X, L), Xs)`）
  - `assertz(q(Big))` のあとの `q(Y)`
  - `clause(q(Y), true)`
  - `retract(q(Big))`
  - `nb_setval(k, f(Big))` のあとの `nb_getval(k, V)`
  - `catch(throw(e(Big)), e(Z), true)`
  - `assertz` した `q(Big)` に `call(Big)` を入れた節が `type_error(callable, _)` になること（「呼び出せる項」の判定）
  - `index_big_first`: 第 1 引数が 2^100 と 2^100+1 の節を交互に 10 個 `assertz` し、`r(2^100 の Cell, N)` がちょうど 5 解を返す。
  - 節の指紋: 2^100 を持つ節と 2^100+1 を持つ節の指紋が違う。
- [ ] **Step 2: 走らせて失敗を確かめる。** `ctest --test-dir build -R bigint_copy --output-on-failure` が失敗する（`data_leaf` の既定の枝が `true` を返すので、`q(Y)` が `q(true)` になる）。
- [ ] **Step 3: 実装する。** 各場所で、Big の場合を `switch` か `if` の明示した枝に足す。`footprint` などの勘定は、子の節点を含めて数える（子の持ち方なので、今の節点の勘定がそのまま効く）。
- [ ] **Step 4: 走らせて通ることを確かめる。** `bigint_copy` が通り、`MOSA_GC_STRESS=1` でも通る。ctest 全体も通る。
- [ ] **Step 5: Commit.** メッセージは `[phase10] IR・複製・指紋・勘定を Big に広げ、Big の定数を持つ節を native_eligible から外す（bigint_copy_test、MOSA_GC_STRESS=1 でも通る）`。

---

### Task 5: 字句解析と reader、`number_codes/2` の族（作る側の 1 つ目）

**Files:**
- Modify: `include/mosa/lexer.hpp:19,26-28,79-85`、`src/read/lexer_literal.cpp:28-34,132-255`
- Modify: `src/read/reader.cpp:105,198-205,228-237`、`src/read/reader_bracket.cpp:93`、`src/builtins/atoms_number.cpp:102-112,173-195`
- Modify: `lib/messages_en.pl:22`、`lib/messages_ja.pl:22`（`integer_overflow` の鍵が使われなくなったら消す）
- Modify（期待値の変更。設計の表）: `tests/pl/read.pl:135-143,159,160,162,166,167,173,174`、`tests/pl/atoms.pl:87`、`tests/cpp/lexer_test.cpp:128-145`
- Create: `tests/pl/bigint.pl`

**Interfaces:**
- Consumes: タスク 2 の `parse_magnitude`、タスク 3 の `Heap::big`
- Produces: `Token` が int64 の外の値を持てる（持ち方は実装者に任せる）。`needs_minus` は無くなる。

- [ ] **Step 1: 期待値を直し、新しい行を書く。**
  - `read.pl` の `syntax_error(integer_overflow)` の行は、読めた値を確かめる行にする。例: `test(read_2_63, (term_to_atom(X, '9223372036854775808')), solutions([X = 9223372036854775808]))`。
  - `read.pl:173-174`（広い読み）は、規則のとおり `syntax_error(max_arity)` にする。
  - `read.pl:147,163-165`（「あふれのあとに読み直せる」）は、Big を読み直す行に置き替える。
  - `lexer_test.cpp` の `IntegerOverflow` と `needs_minus` の検査は、Big のトークンの検査にする。

  `tests/pl/bigint.pl` には次の行を書く。
  - `lit_big`: `X = 123456789012345678901234567890, Y = 123456789012345678901234567890, X == Y`
  - `lit_hex`: `X = 0xffffffffffffffffffff, X =:= 1208925819614629174706175`
  - `lit_sep`: `X = 1_000_000_000_000_000_000_000, X == 1000000000000000000000000`
  - `neg_min`: `X = -9223372036854775808, integer(X)`（IntBox の −2^63）
  - `nc_big`: `number_codes(X, "-1267650600228229401496703205376"), number_codes(X, C), atom_codes(A, C), A == '-1267650600228229401496703205376'`
  - `wq_neg_1`: `X = -(-1267650600228229401496703205376), term_to_atom(X, A), term_to_atom(Y, A), Y == X`
  - `wq_neg_2`: `X = 1 - (-1267650600228229401496703205376)` で、`wq_neg_1` と同じ往復
  - `wq_neg_3`: `X = -(1267650600228229401496703205376)`（複合項）と `Y = -1267650600228229401496703205376`（数）の `term_to_atom/2` の字面が違い、それぞれ読み戻すと元の項に `==` になる
  - `atom_number_big`: `atom_number('18446744073709551616', N), N == 18446744073709551616`
- [ ] **Step 2: 走らせて失敗を確かめる。** `./build/mosatest tests/pl/read.pl tests/pl/bigint.pl` が、直した行と新しい行の名前を出して失敗する。
- [ ] **Step 3: 実装する。**
  - `read_digits` は、int64 に収まらなくなった時点で数字の並びを `parse_magnitude` に渡す。
  - `- N` の負の定数は、`BigInt` の符号を反転し、`heap.big` で正規化する。2^63 は IntBox の −2^63 になる。
  - `strip_sign` の 2^63 の特例を消す。
- [ ] **Step 4: 走らせて通ることを確かめる。** `./build/mosatest tests/pl` が終了コード 0 で終わる。`ctest --test-dir build` 全体と `ctest --test-dir build -L diff` が通る（Big を持つ述語は `native_eligible` から外れているので、差分は 0 のまま）。
- [ ] **Step 5: Commit.** メッセージは `[phase10] 字句解析と reader と number_codes の族が int64 を越える字面を Big として読む（read.pl・atoms.pl・lexer_test の integer_overflow の行は設計の表どおりに直す。bigint.pl の読みの行が通る）`。

---

### Task 6: 算術 1 と旗 `bounded`

**Files:**
- Modify: `src/builtins/arith.cpp`（先に `arith_int.cpp` などへ分ける）、`src/builtins/arith_float.cpp:34-71,138-149`
- Modify: `src/builtins/flags.cpp:88`（`bounded` を `false` に）
- Create: `tests/cpp/bigint_limit_test.cpp`
- Modify（期待値の変更）: `tests/pl/arith.pl:13,74,75,77,78,79,128,129,168`、`tests/pl/lib_between.pl:29,43`、`tests/cpp/machine_test.cpp:446`、`tests/pl/flags.pl:1,3,4`、マニュアル `docs/manual/en/06-arithmetic.md:178-185` と `ja/06-arithmetic.md:153-158`（`check: limits`）
- Modify: `docs/SPEC.md` と `.ja.md`（第3節の 5 と 7、処理系定義の表）、`docs/manual/en/14-reference.md:135-137` と `ja/14-reference.md:123-125`（旗の行）
- Test: `tests/pl/bigint.pl`

**Interfaces:**
- Consumes: タスク 2 の演算、タスク 3 の `Heap::big` と `big_value`
- Produces: `Num` が Big を持てる（形は実装者に任せる）。int64 の道は確保をしない。

- [ ] **Step 1: 失敗するテストを書く。** `bigint.pl` に次の行を足す。値はすべて Python で確かめたもの。
  - `norm_add`: `X is 9223372036854775807 + 1 - 1, X == 9223372036854775807`
  - `norm_mul`: `X is 9223372036854775807 * 2, X == 18446744073709551614`
  - `norm_min`: `X is -9223372036854775808 - 1, X == -9223372036854775809`
  - `norm_neg`: `X is -(-9223372036854775808), X == 9223372036854775808`
  - `norm_abs`: `X is abs(-9223372036854775808), X == 9223372036854775808`
  - `norm_quot`: `X is -9223372036854775808 // -1, X == 9223372036854775808`
  - `big_div`: P = 1267650600228229401496703205376 として、`X is -P // 7` が −181092942889747057356671886482、`X is -P rem 7` が −2、`X is div(-P, 7)` が −181092942889747057356671886483、`X is -P mod 7` が 5
  - `big_cmp`: `P > 9223372036854775807`、`-P < -9223372036854775808`、`P =:= P`、`P =\= P + 1`
  - `big_minmax`: `X is max(P, 24), X == P`、`X is min(-P, 0), X == -P`
  - `big_sign`: `X is sign(-P), X == -1`
  - `big_float`: `X is float(P), X == 1.2676506002282294e30`。`X is float(36893488147419107328), X == 3.6893488147419103e19` は、2^65 + 2^12 がちょうど中間なので、偶数の 2^65 に丸める例である。`X is float(36893488147419107329), X == 3.689348814741911e19` は、中間を 1 越えるので上に丸める例である。
  - `big_float_div`: `pow10(400, A), pow10(399, B), X is A / B, X == 10.0`。`pow10/2` は `*` だけを使う補助述語で、`bigint.pl` の頭に置く。`/` は整数どうしの商を正しく丸める。被演算子それぞれを double に変えることはしない。
  - `big_float_over`: `pow10(400, A), X is float(A)` が `error(evaluation_error(float_overflow))` になる
  - `big_round`: `X is round(1.0e32), X == 100000000000000005366162204393472`、`X is truncate(-1.0e20), X == -100000000000000000000`
  - `big_msb`: `X is msb(P), X == 100`
  - `bounded_flag`: `current_prolog_flag(bounded, false)`、`current_prolog_flag(max_integer, 9223372036854775807)`

  `tests/cpp/bigint_limit_test.cpp`（新規。`tests/tests.cmake` に `state_limit` と同じく登録し、`TIMEOUT 300`）に `huge_mul` を書く。上限 16 MiB のマシンで、`sq(X) :- Y is X * X, sq(Y).` を `pow10(400, A), sq(A)` から流す。ヒープの上限（`ResourceLimit`）で止まり、300 秒の中で終わる。

  次の期待値を直す（設計の表）。
  - `arith.pl` の 13・74・75・128・129 行は、`X == 9223372036854775808` を確かめる行にする。
  - `arith.pl` の 77〜79 行（`integer`、`floor`、`truncate` の `2.0 ** 63`）も、`X == 9223372036854775808` を確かめる行にする。
  - `arith.pl` の 168 行（`integer(1.0e300)`）は、値が `1000000000000000052504760255204420248704468581108159154915854115511802457988908195786371375080447864043704443832883878176942523235360430575644792184786706982848387200926575803737830233794788090059368953234970799945081119038967640880074652742780142494579258788820056842838115669472196386865459400540160` であることを確かめる行にする。
  - `lib_between.pl` の 29 行（`succ`）と 43 行（`plus`）は、`N =:= M + 1` を確かめる行にする。
  - `machine_test.cpp:446` は `StopKind::Success` を確かめる検査にする。
  - `flags.pl` の 1・3・4 行は、`bounded` が `false` を確かめる行にする。
- [ ] **Step 2: 走らせて失敗を確かめる。** `./build/mosatest tests/pl/bigint.pl tests/pl/arith.pl tests/pl/flags.pl` が、新しい行で失敗する。
- [ ] **Step 3: 実装する。**
  - `add_i`、`sub_i`、`mul_i` が失敗したら、`BigInt` で計算し直す。
  - 混合の演算は、Big を `to_double` で変える。`nullopt` なら `float_overflow` にする。
  - 整数どうしの `/` は、どちらかが Big のとき、商を 54 ビット以上に桁を揃えてから丸める。
  - 浮動小数点数から整数へは `from_integral_double` を使う（無限大と NaN は今のエラーのまま）。
  - 結果の大きさの見積もりは、`*` が `bit_length` の和、`+` が大きい方 +1 になる。ヒープの残りを越えたら、計算も確保もせずに `ResourceLimit("heap_limit")` を投げる。
  - 旗 `bounded` を `false` にする。
  - SPEC 第3節と第14章の旗の行（日英）を書き替える。
- [ ] **Step 4: 走らせて通ることを確かめる。** `./build/mosatest tests/pl` が終了コード 0 で終わり、ctest 全体と `-L diff` が通る。
  - `./build/mosatest --suite tests/conformance` を流す。範囲内で `bounded` が `true` を条件にした 15 件が別欄に移り、`lgt_msb_1_07`・`08` が通る。合格率が 98.4% を下回らないことを確かめる。
  - 実際の数を STATUS に書く。
  - `bench` を走らせる（タスク 12 と同じ測り方で、`nrev` と `queens` を 1 回ずつ）。0.1.4 の 1.05 倍を越えていたら、レビューの前に直す。
- [ ] **Step 5: Commit.** メッセージは `[phase10] 加減乗除・比較・浮動小数点数との変換・msb を Big に広げ、旗 bounded を false にする（bigint.pl・arith.pl・flags.pl の表どおりの行、スイート 実数/実数）`。`docs/conformance.*`、`docs/agreement.json`、SPEC、マニュアルを同じコミットに入れる。STATUS には、タスク 7 までの暫定（`2^100` はまだあふれる）を書く。

---

### Task 7: 算術 2（`gcd`、`^`、シフト、ビット演算）

**Files:**
- Modify: `src/builtins/arith*.cpp`（`gcd_of`、`int_hat`、`pow_int`、`negative_hat`、`shift_bits`、ビット演算、`succ_or_zero`）
- Modify（期待値の変更）: `tests/pl/arith.pl:87,134,135`（`1 << 63` と `gcd` の最小の整数）
- Modify: `tests/cpp/bigint_limit_test.cpp`（`huge_shift`）
- Test: `tests/pl/bigint.pl`

**Interfaces:**
- Consumes: タスク 2 の `gcd`、`power`、`shift_left`、`shift_right`、`bit_*`。タスク 6 の `Num` と、大きさの見積もり。

- [ ] **Step 1: 失敗するテストを書く。** `bigint.pl` に次の行を足す。
  - `big_pow`: `X is 2^100, X == 1267650600228229401496703205376`、`X is (-2)^63, X == -9223372036854775808`、`X is 2^63, X == 9223372036854775808`
  - `big_gcd`: `X is gcd(2^100, 6^50), X == 1125899906842624`、`X is gcd(-9223372036854775808, 0), X == 9223372036854775808`
  - `big_shift`: `X is 1 << 64, X == 18446744073709551616`、`X is -(2^100) >> 1, X == -633825300114114700748351602688`、`X is 2^100 >> 101, X == 0`、`X is -(2^100) >> 101, X == -1`
  - `bits_neg_1`: `X is \ (2^70), X == -1180591620717411303425`
  - `bits_neg_2`: `X is -(2^70) /\ 255, X == 0`、`X is (\ (2^70)) /\ 255, X == 255`
  - `bits_neg_3`: `X is (2^70) xor (2^70 + 5), X == 5`、`X is -(2^70) \/ 1, X == -1180591620717411303423`
  - `pow_neg_exp`: `X is 2^(-1)` が `type_error(float, 2)` のまま（iso-notes 9.3.10.3 の決定 B）
  - `succ_big`: `use_module(library(between)), succ(123456789012345678901234567890, N), N == 123456789012345678901234567891`

  `bigint_limit_test.cpp` に `huge_shift` を足す。`X is 1 << (1 << 62)` と `X is 2^(2^62)` が、それぞれ 1 秒の中で `ResourceLimit` で止まる（確保の前の見積もりで止まるので、ヒープは伸びない）。

  `arith.pl` の 87 行（`1 << 63`）と 134・135 行（`gcd` の最小の整数）は、`X == 9223372036854775808` を確かめる行に直す。
- [ ] **Step 2: 走らせて失敗を確かめる。** `./build/mosatest tests/pl/bigint.pl tests/pl/arith.pl tests/pl/lib_between.pl` が失敗する。
- [ ] **Step 3: 実装する。**
  - `^` の結果の大きさは「指数 × 底の `bit_length`」で見積もる。底が 0、1、−1 のときは見積もらない。
  - `<<` の大きさは「シフトの数 + `bit_length`」で見積もる。
  - シフトの数が Big なら、`>>` は 0 か −1 になる。`<<` はヒープの上限で止まる。
- [ ] **Step 4: 走らせて通ることを確かめる。** `./build/mosatest tests/pl` が通り、ctest 全体と `-L diff` が通る。スイートを流して、合格数が変わったら `docs/conformance.*` を作り直す。
- [ ] **Step 5: Commit.** メッセージは `[phase10] gcd・^・シフト・ビット演算を Big に広げる（bigint.pl の bits_neg_*、bigint_limit_test の huge_shift、arith.pl の表どおりの 3 行）`。

---

### Task 8: 整数の引数を取る組み込み述語

**Files:**
- Modify: 付録 B の 5 の場所。主なもの:
  - `src/builtins/terms.cpp:123-165,331-358`（`functor/3`、`arg/3`、`numbervars/3`）
  - `src/builtins/atoms_sub.cpp:44-88`、`src/builtins/atoms.cpp:205-226,297-312`
  - `src/builtins/lists.cpp:86-93,173-228`
  - `src/builtins/chars.cpp:176-245,400-413`、`src/builtins/bytes.cpp:78-128`
  - `src/builtins/operators.cpp:84-86,143-154,349`、`src/builtins/stream_prop.cpp:73-79,310-330`
  - `src/builtins/control.cpp:201-206`、`src/machine/catch.cpp:280-284`
  - `src/builtins/database.cpp:561-572`、`src/module/qualify.cpp:140-143`、`src/builtins/database_scan.cpp:179-190`
  - `src/builtins/read_pred.cpp:120-122`、`src/builtins/write_pred.cpp:129-131`、`src/builtins/streams.cpp:347-352,412-414`
  - `lib/format.pl:106-111,159-165`
- Modify: `docs/iso-notes.md` と `.ja.md`（460 行の `numbervars/3`、新しい節「8.17.4 halt/1」）
- Modify: `tests/cpp/bigint_limit_test.cpp`（`length_big`）
- Test: `tests/pl/bigint.pl`

**Interfaces:**
- Consumes: タスク 3 の `is_integer_tag` と `big_negative`

- [ ] **Step 1: 失敗するテストを書く。** B = 1844674407370909797907654848955145546336677616（スイートの `big/1` と同じ値）として、`bigint.pl` に次の行を足す。
  - `arg_big`: `arg(B, t(1,2,3), _)` は失敗し、`arg(-B, t(1,2,3), _)` は `domain_error(not_less_than_zero, -B)` になる
  - `functor_big`: `functor(_, t, B)` が `representation_error(max_arity)` になる
  - `char_code_big`: `char_code(C, B)` が `representation_error(character_code)` になる
  - `atom_length_big`: `atom_length(abc, B)` は失敗し、`atom_length(abc, -B)` は `domain_error(not_less_than_zero, -B)` になる
  - `sub_atom_big`: `sub_atom(abc, B, _, _, _)` が失敗する
  - `length_big`: `length(L, -B)` が `domain_error(not_less_than_zero, -B)` になる。`length(L, B)` は `bigint_limit_test.cpp` に足し、リストを作り始める前に `ResourceLimit` で止まることを確かめる。
  - `numbervars_big`: `numbervars(f(X, Y), B, E)` のあと `E =:= B + 2`、`writeq(X)` が `'$VAR'(B)` の字母と番号で書く
  - `format_big`: `use_module(library(format))`、`format(atom(A), '~d', [B])`、`A == '1844674407370909797907654848955145546336677616'`
  - `op_big`: `op(B, xfx, foo)` が `domain_error(operator_priority, B)` になる
  - `halt_big`: `halt(B)` が `representation_error(max_integer)` に、`halt(-B)` が `representation_error(min_integer)` になる。範囲は C++ の `int` で、7.12.2 g の Flag を使う（私の判断）。規格は終了状態の範囲を決めていないので、iso-notes に「8.17.4 halt/1 の範囲の外の整数」として A（この表現のエラー）か B（下位のビットを OS に渡す）かを書く。既定は A で、`int` の外の int64 にも同じ規則を当てる（今の黙った切り詰めを無くす）。
- [ ] **Step 2: 走らせて失敗を確かめる。** `./build/mosatest tests/pl/bigint.pl` が新しい行で失敗する。
- [ ] **Step 3: 実装する。** 設計の「8. 整数の引数」の規則で直す。各場所で、`static_cast` の前に範囲を検査する。
- [ ] **Step 4: 走らせて通ることを確かめる。** `./build/mosatest tests/pl` が通り、ctest 全体と `-L diff` が通る。
- [ ] **Step 5: Commit.** メッセージは `[phase10] 整数の引数を取る組み込みが Big を受け、黙った切り詰めを無くす（bigint.pl の arg_big ほか 10 行）`。

---

### Task 9: 準拠スイート（`unbounded/` を範囲に入れる）

**Files:**
- Modify: `src/machine/lgt_shape.cpp:23,33-39,54-70,118-126`、`src/machine/lgt_copy.cpp:30`、`src/machine/lgt_approx.cpp:14,22`、`src/machine/suite.cpp:149-157`、`src/machine/lgt_judge.cpp:45,222-232`、`src/machine/lgt_io.cpp:155`
- Modify: `src/machine/lgt_suite.hpp:155-160`（`excluded_tree` から `unbounded` を外す）
- Modify（期待値の変更）: `tests/cpp/conformance_report_test.cpp:164-165,351,374`
- Regenerate: `docs/conformance.md`、`docs/conformance.json`

- [ ] **Step 1: 期待値を直す。** `conformance_report_test.cpp` が `unbounded` を範囲の外とする検査を、範囲の中とする検査にする。
- [ ] **Step 2: 走らせて失敗を確かめる。** `ctest --test-dir build -R conformance_report` が失敗する。
- [ ] **Step 3: ランナーの数の扱いを Big に広げる。**
  - `negate_number` と `fill_number` は `BigInt` で符号を反転する。
  - `=~=` の比べ方（`lgt_approx`）は、Big を `to_double` で変える。
  - `copy_number` は `heap.big` で写す。
  - そのあとで `excluded_tree` から `unbounded` を外す。
- [ ] **Step 4: 走らせて確かめる。** `./build/mosatest --suite tests/conformance` を流す。
  - `unbounded/` の動く件（見積もりでは約 102 件）がすべて通る。通らない件は iso-notes に件の名前と期待と結果を書き、所有者に問う。
  - 合格率は 98.4% 以上である。
  - ctest 全体と `-L diff` が通る。
- [ ] **Step 5: Commit.** メッセージは `[phase10] 準拠スイートの unbounded を範囲に入れ、ランナーの数の扱いを Big に広げる（スイート 実数/実数）`。`docs/conformance.*` と `docs/agreement.json` を同じコミットに入れる。

---

### Task 10: `mosac`

**Files:**
- Modify: `include/mosa/runtime.hpp:73-116`（`rt::put_big`、`rt::big_key_of`）
- Modify: `src/codegen/emit_head.cpp:41-53,61-65,88-97,213-227`、`src/codegen/emit.cpp:158-182,425-472`
- Modify: `src/machine/native.cpp:173-182`（タスク 4 の除外を外す）
- Create: `tests/mosac/bigint.pl` と、`tests/mosac/` の既存の形に合わせた期待の出力
- Modify: `tests/tests.cmake`（`tests/mosac/` の既存の `foreach` に `bigint` を足す）

**Interfaces:**
- Consumes: タスク 4 の `IrTag::Big` の持ち方、タスク 2 の `big_key`
- Produces:
  - `[[nodiscard]] inline Cell rt::put_big(std::uint64_t* slot, bool negative, std::span<const std::uint64_t> magnitude)`: `slot` から n+1 語に頭の語と語を書き、Big の Cell を返す。
  - `[[nodiscard]] std::uint64_t rt::big_key_of(Cell cell)`: 生成コードの `key_test` が使う。

- [ ] **Step 1: 失敗するテストを書く。** `tests/mosac/bigint.pl` に、次のものを入れたプログラムを書く。`main` で結果を書き出し、期待の出力と比べる。
  - 頭部に Big を持つ節が 8 つ以上ある述語（`index_big_*`。2^100 と 2^100+1 を交互にする）
  - 本体で Big を作る節（`X = 123456789012345678901234567890, Y is X * X`）
  - Big を返す事実
- [ ] **Step 2: 走らせて失敗を確かめる。** `native_eligible` の除外を外した状態で、`ctest --test-dir build -R bigint --output-on-failure` が失敗する。`build_words` の `default:` が 0 語と数えるためである。
- [ ] **Step 3: 実装する。** `build_words` は Big を n+1 語と数える。`place_leaf` は `static constexpr std::array<std::uint64_t, N>` と `rt::put_big` を出す。`match` は `match_built` に送る。`key_test` は `rt::big_key_of(first) == 0x...` を出す。
- [ ] **Step 4: 走らせて通ることを確かめる。** `tests/mosac` の新しい試験が通り、`ctest --test-dir build -L diff` の差分が 0、ctest 全体が通る。
- [ ] **Step 5: Commit.** メッセージは `[phase10] mosac が Big の定数と索引の鍵を出す（tests/mosac/bigint.pl、mosatest --diff 差分 0）`。

---

### Task 11: GC と資源

**Files:**
- Modify: `tests/pl/gc.pl`（`big_plateau`）、`tests/cpp/gc_test.cpp`（`check_big_loop`）
- Modify: 落ちが見つかった場所

- [ ] **Step 1: テストを書く。**
  - `gc.pl` に `big_plateau` を足す。`gc_plateau`（7 行）と同じ形で、ループの中で毎回 `X is 2^200 + N` を作って捨てる。`statistics(heap, …)` の容量が、1,000,000 回の前と後で同じであることを確かめる。
  - `gc_test.cpp` に `check_big_loop` を足す。`check_loop` と同じ形で、Big を作るプログラムを流す。
- [ ] **Step 2: 走らせる。** `ctest --test-dir build -R '^gc$|prolog_tests'` と、`MOSA_GC_STRESS=1` の同じ試験を流す。`MOSA_GC_STRESS=1 ./build/mosatest tests/pl/bigint.pl` も流す。落ちがあれば、その根を直す（落ちのない場合は Step 3 を飛ばす）。
- [ ] **Step 3: 落ちを直す。** 落ちを再現するテストを先に足す。
- [ ] **Step 4: 走らせて通ることを確かめる。** 上のすべてが通り、ctest 全体が通る。
- [ ] **Step 5: Commit.** メッセージは `[phase10] Big を作り続けるループで GC が頭打ちになることを確かめる（gc.pl の big_plateau、gc_test の check_big_loop、MOSA_GC_STRESS=1 で bigint.pl が通る）`。

---

### Task 12: 速さ

**Files:**
- Modify: `docs/bench.md` と `.ja.md`、`docs/bench.json`（`--report` で作る）
- Create: `bench/bigint.pl`（`bench(N)` で `7^7^7` の `msb` と、1000! の `number_codes` を N 回）

- [ ] **Step 1: 0.1.4 と今のものを作る。** `git worktree add ../mosa-0.1.4 v0.1.4` で別の作業木を作り、両方を Release で作る。
- [ ] **Step 2: 測る。**
  - 両方の `mosatest --bench bench/nrev.pl` と `bench/queens.pl` を、`bench.json` と同じ N で 3 回ずつ走らせ、3 回の中央値を取る。
  - 今のものでは `mosatest --bench bench/bigint.pl --report docs/bench.json` も走らせる。
- [ ] **Step 3: 確かめる。**
  - インタプリタの時間が、0.1.4 の 1.05 倍以下である。
  - `nrev` の比（インタプリタ / 翻訳）が 3 以上である。
  - 越えていたら直す。原因は `Num` の写し、`number_of` の分岐、索引の鍵の分岐の順に疑う。
- [ ] **Step 4: `bench.md` に表の行と測った日を書く**（日英）。作業木は `git worktree remove ../mosa-0.1.4` で消す。
- [ ] **Step 5: Commit.** メッセージは `[phase10] bench に bigint.pl を足し、int64 の道が 0.1.4 の 1.05 倍以内であることを測る（nrev と queens の 3 回の中央値）`。

---

### Task 13: 文書を閉じる

**Files:**
- Modify: `CHANGELOG.md` と `.ja.md`（未リリースの節）
- Modify: マニュアルの `docs/manual/en/06-arithmetic.md`、`09-errors.md:149`、`14-reference.md:157,543-544` と、`ja` の同じ場所（`ja` は `09-errors.md:121`、`14-reference.md:145,516`）
- Modify: `README.md` と `.ja.md`（準拠率 96・198 行、バッジ）
- Modify: `docs/iso-notes.md` と `.ja.md`（`:20,606,660,684-694` の整数の上限に触れる文）、`docs/STATUS.md`
- Modify: 元の指示書の 62 行は過去の記録なので直さない。STATUS に、直さない理由を 1 行で書く。

- [ ] **Step 1: 書く。**
  - CHANGELOG に、多倍長整数、`bounded` が `false`、`syntax_error(integer_overflow)` と `int_overflow` が無くなったこと（大きさの上限はヒープの上限）、所要時間の目安（`bench.md`）を書く。0.1.0 の既知の制限の節は過去の記録なので直さない。
- [ ] **Step 2: 確かめる。** `ctest --test-dir build -R manual` と `./build/mosatest --manual docs/manual --all-tables` が通る。`grep -rn 'int_overflow\|integer_overflow' docs/manual README.md README.ja.md` が、意図した記述（`max_tagged_integer` など）のほかに当たらない。
- [ ] **Step 3: CI を確かめる。** 所有者の依頼で push したあと、CI の全ジョブ（今は 10）が通ることを確かめる。
- [ ] **Step 4: Commit.** メッセージは `[phase10] CHANGELOG・マニュアル・README・iso-notes を多倍長整数に合わせて閉じる（manual と mosatest --manual --all-tables が通る）`。
- [ ] **Step 5: 止まる。** 完了条件 1〜5 の確かめた結果を STATUS に書き、所有者に「フェーズ10を閉じるか（A）、残すか（B）」を問う。
