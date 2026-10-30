#include "harness.hpp"
#include "mosa/reader.hpp"
#include "mosa/writer.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::string written(std::string_view source) {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  auto ops = mosa::OperatorTable::iso_core();
  mosa::Reader reader(source, heap, ops);
  const auto outcome = reader.next();
  if (!outcome.term) return "<read failed>";
  return mosa::write_term(heap, ops, outcome.term->term, {}, outcome.term->names);
}

struct Held {
  mosa::AtomTable atoms;
  mosa::Heap heap;
  mosa::OperatorTable ops;
  mosa::ReadTerm term{};
  bool ok = false;

  explicit Held(std::string_view source) : heap(atoms), ops(mosa::OperatorTable::iso_core()) {
    mosa::Reader reader(source, heap, ops);
    auto outcome = reader.next();
    if (!outcome.term) return;
    term = std::move(*outcome.term);
    ok = true;
  }
};

bool same(const Held& left, const Held& right) {
  return left.ok && right.ok &&
         mosa::same_term(left.heap, left.term.term, left.term.names, right.heap, right.term.term,
                         right.term.names);
}

void check_layout() {
  mosa_test::check(written("a+b*c.") == "a+b*c", "priority");
  mosa_test::check(written("(a+b)*c.") == "(a+b)*c", "parens");
  mosa_test::check(written("a-b-c.") == "a-b-c", "left assoc");
  mosa_test::check(written("a^b^c.") == "a^b^c", "right assoc");
  mosa_test::check(written("f(a, b).") == "f(a,b)", "tight comma");
  mosa_test::check(written("[a, b].") == "[a,b]", "list");
  mosa_test::check(written("[H|T].") == "[H|T]", "list bar");
  mosa_test::check(written("{a, b}.") == "{a,b}", "curly");
  mosa_test::check(written("東京.") == "東京", "unquoted japanese");
  mosa_test::check(written("'東 京'.") == "'東 京'", "quoted space");
  mosa_test::check(written("'\\n'.") == "'\\n'", "escape newline on output");
}

void check_unnamed_and_numbers() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  auto ops = mosa::OperatorTable::iso_core();
  const auto var = heap.fresh_ref();
  const auto shared = heap.structure(atoms.intern("f"), std::array<mosa::Cell, 2>{var, var});
  const auto once = heap.structure(atoms.intern("f"), std::array<mosa::Cell, 1>{heap.fresh_ref()});
  mosa_test::check(mosa::write_term(heap, ops, shared, {}, {}) == "f(_G1,_G1)", "shared unnamed");
  // iso-notes 7.10.5 a の決定 B。1 度だけ出る変数にも _ の後に名前を付ける。
  mosa_test::check(mosa::write_term(heap, ops, once, {}, {}) == "f(_G1)", "single unnamed");
  const auto mixed = heap.structure(atoms.intern("f"), std::array<mosa::Cell, 3>{var, heap.fresh_ref(), var});
  mosa_test::check(mosa::write_term(heap, ops, mixed, {}, {}) == "f(_G1,_G2,_G1)", "mixed unnamed");
  // スイートの実行器は貼り合わせのため、1 度だけ出る変数を _ のまま書く選択肢を使う。
  mosa::WriteOptions paste;
  paste.underscore_once = true;
  mosa_test::check(mosa::write_term(heap, ops, mixed, paste, {}) == "f(_G1,_,_G1)", "underscore once");
  const auto minus = atoms.intern("-");
  const auto inner = heap.structure(minus, std::array<mosa::Cell, 1>{heap.atom(atoms.intern("a"))});
  const auto outer = heap.structure(minus, std::array<mosa::Cell, 1>{inner});
  mosa_test::check(mosa::write_term(heap, ops, outer, {}, {}) == "- -a", "stacked minus");
  mosa_test::check(mosa::write_term(heap, ops, heap.atom(atoms.intern("Foo")), {}, {}) == "'Foo'",
                   "quote capital");
  mosa_test::check(mosa::write_term(heap, ops, heap.atom(atoms.intern("_")), {}, {}) == "'_'",
                   "quote underscore");
  mosa::WriteOptions bare;
  bare.ignore_ops = true;
  const auto list = heap.list(heap.atom(atoms.intern("a")), heap.atom(atoms.intern("[]")));
  mosa_test::check(mosa::write_term(heap, ops, list, bare, {}) == "'.'(a,[])", "ignore ops");
  const auto negative = mosa::write_term(heap, ops, heap.integer(-42), {}, {});
  mosa_test::check(negative == "-42", "negative integer");
  const auto neg_zero = mosa::write_term(heap, ops, heap.floating(-0.0), {}, {});
  mosa_test::check(neg_zero == "-0.0", "negative zero");
  const auto one = mosa::write_term(heap, ops, heap.floating(1.0), {}, {});
  mosa_test::check(one.find('.') != std::string::npos && one.find('e') == std::string::npos &&
                       one.find('E') == std::string::npos,
                   "float decimal");
  const auto big = mosa::write_term(heap, ops, heap.floating(1.5e20), {}, {});
  mosa_test::check(big.find('e') != std::string::npos && big.find('E') == std::string::npos &&
                       big.find('.') != std::string::npos,
                   "float exponent");
}

void check_equality() {
  const Held swapped_left("f(A, B).");
  const Held swapped_right("f(B, A).");
  mosa_test::check(!same(swapped_left, swapped_right), "names differ");
  const Held shared("f(A, A).");
  mosa_test::check(same(shared, shared), "same shared name");
  const Held distinct("f(_, _).");
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  const auto var = heap.fresh_ref();
  const auto joined = heap.structure(atoms.intern("f"), std::array<mosa::Cell, 2>{var, var});
  mosa_test::check(distinct.ok && !mosa::same_term(distinct.heap, distinct.term.term, distinct.term.names,
                                                   heap, joined, {}),
                   "anon sharing differs");
  const Held named("f(A).");
  const Held anon("f(_).");
  mosa_test::check(!same(named, anon), "name is not underscore");
  const Held shared_x("f(X, X).");
  const Held shared_x_again("f(X, X).");
  mosa_test::check(same(shared_x, shared_x_again), "shared name across heaps");
  mosa::AtomTable left_atoms;
  mosa::Heap left_heap(left_atoms);
  const auto once = left_heap.fresh_ref();
  const auto left = left_heap.structure(left_atoms.intern("f"), std::array<mosa::Cell, 2>{once, once});
  const std::vector<mosa::VarName> left_names{{"X", once}};
  mosa::AtomTable right_atoms;
  mosa::Heap right_heap(right_atoms);
  const auto first = right_heap.fresh_ref();
  const auto second = right_heap.fresh_ref();
  const auto right =
      right_heap.structure(right_atoms.intern("f"), std::array<mosa::Cell, 2>{first, second});
  const std::vector<mosa::VarName> right_names{{"X", first}, {"X", second}};
  mosa_test::check(!mosa::same_term(left_heap, left, left_names, right_heap, right, right_names),
                   "shared name is one ref");
  mosa_test::check(!mosa::same_term(right_heap, right, right_names, left_heap, left, left_names),
                   "shared name is one ref reversed");
}

constexpr int kDeep = 1000000;

bool ends(std::string_view text, std::string_view head, std::string_view tail) {
  return text.starts_with(head) && text.ends_with(tail);
}

// ADR 0011: writer は深さ 1,000,000 の項を C++ の再帰なしで書く。先頭と末尾だけを見る。
void check_deep_terms() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  auto ops = mosa::OperatorTable::iso_core();
  const auto f = atoms.intern("f");
  const auto minus = atoms.intern("-");
  mosa::Cell nested = heap.atom(atoms.intern("a"));
  mosa::Cell negated = nested;
  for (int depth = 0; depth < kDeep; ++depth) {
    nested = heap.structure(f, std::array<mosa::Cell, 1>{nested});
    negated = heap.structure(minus, std::array<mosa::Cell, 1>{negated});
  }
  const std::string fs = mosa::write_term(heap, ops, nested, {}, {});
  // 閉じ括弧は深さの数だけ続くので、a の位置で見る。
  mosa_test::check(fs.size() == 3U * kDeep + 1 && ends(fs, "f(f(f(", ")))") && fs.substr(2U * kDeep - 2, 4) == "f(a)",
                   "deep f");
  const std::string ms = mosa::write_term(heap, ops, negated, {}, {});
  mosa_test::check(ms.size() == 2U * kDeep && ends(ms, "- - - ", "- - -a"), "deep minus");
  mosa::Cell sum = heap.integer(1);
  mosa::Cell conj = heap.atom(atoms.intern("b"));
  for (int index = 2; index <= kDeep; ++index) {
    sum = heap.structure(atoms.intern("+"), std::array<mosa::Cell, 2>{sum, heap.integer(index)});
    conj = heap.structure(atoms.intern(","), std::array<mosa::Cell, 2>{heap.atom(atoms.intern("a")), conj});
  }
  const std::string ss = mosa::write_term(heap, ops, sum, {}, {});
  mosa_test::check(ends(ss, "1+2+3+4+", "+999999+1000000"), "deep left sum");
  const std::string cs = mosa::write_term(heap, ops, conj, {}, {});
  mosa_test::check(cs.size() == 2U * kDeep - 1 && ends(cs, "a,a,a,", "a,a,b"), "deep right conj");
}

void check_deep_lists() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  auto ops = mosa::OperatorTable::iso_core();
  mosa::Cell numbers = heap.atom(atoms.intern("[]"));
  mosa::Cell fresh = numbers;
  const auto shared = heap.fresh_ref();
  for (int index = kDeep; index >= 1; --index) {
    numbers = heap.list(heap.integer(index), numbers);
    fresh = heap.list(index == 1 ? shared : heap.fresh_ref(), fresh);
  }
  fresh = heap.list(shared, fresh);
  mosa::WriteOptions canonical;
  canonical.ignore_ops = true;
  const std::string ns = mosa::write_term(heap, ops, numbers, canonical, {});
  const std::string_view last = "'.'(1000000,[]";
  mosa_test::check(ends(ns, "'.'(1,'.'(2,'.'(3,", ")))") && ns.find(last) == ns.size() - kDeep - last.size(),
                   "deep canonical list");
  mosa::WriteOptions paste;
  paste.underscore_once = true;
  const std::string us = mosa::write_term(heap, ops, fresh, paste, {});
  mosa_test::check(us.size() == 2U * kDeep + 7 && ends(us, "[_G1,_G1,_,_,", ",_,_]"), "deep underscore once");
}

// 深い g(…, N) の連鎖と長いリスト。変数は 1 つを奇数段で共有するので、対応の表は 1 組で済む。
mosa::Cell deep_pair(mosa::AtomTable& atoms, mosa::Heap& heap, std::string_view leaf) {
  const auto var = heap.fresh_ref();
  mosa::Cell term = heap.atom(atoms.intern(leaf));
  mosa::Cell list = heap.atom(atoms.intern("[]"));
  for (int depth = 0; depth < kDeep; ++depth) {
    const mosa::Cell side = depth % 2 == 0 ? heap.integer(depth) : var;
    term = heap.structure(atoms.intern("g"), std::array<mosa::Cell, 2>{term, side});
    list = heap.list(heap.integer(depth), list);
  }
  return heap.structure(atoms.intern("h"), std::array<mosa::Cell, 2>{list, term});
}

// ADR 0011: same_term は深さ 1,000,000 の項を C++ の再帰なしで比べる。違いは一番深い葉だけに置く。
void check_same_term_deep() {
  mosa::AtomTable left_atoms;
  mosa::Heap left_heap(left_atoms);
  mosa::AtomTable right_atoms;
  mosa::Heap right_heap(right_atoms);
  const mosa::Cell left = deep_pair(left_atoms, left_heap, "a");
  const mosa::Cell right = deep_pair(right_atoms, right_heap, "a");
  const mosa::Cell other = deep_pair(right_atoms, right_heap, "b");
  mosa_test::check(mosa::same_term(left_heap, left, {}, right_heap, right, {}), "same_term deep equal");
  mosa_test::check(!mosa::same_term(left_heap, left, {}, right_heap, other, {}), "same_term deep leaf differs");
}

}  // namespace

int main() {
  check_layout();
  check_unnamed_and_numbers();
  check_equality();
  check_deep_terms();
  check_deep_lists();
  check_same_term_deep();
  return mosa_test::failures();
}
