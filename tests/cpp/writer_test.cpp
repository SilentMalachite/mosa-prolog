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
  mosa_test::check(written("f(a, b).") == "f(a, b)", "comma space");
  mosa_test::check(written("[a, b].") == "[a, b]", "list");
  mosa_test::check(written("[H|T].") == "[H|T]", "list bar");
  mosa_test::check(written("{a, b}.") == "{a, b}", "curly");
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
  mosa_test::check(mosa::write_term(heap, ops, shared, {}, {}) == "f(_G1, _G1)", "shared unnamed");
  mosa_test::check(mosa::write_term(heap, ops, once, {}, {}) == "f(_)", "single unnamed");
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
  mosa_test::check(mosa::write_term(heap, ops, list, bare, {}) == "'.'(a, [])", "ignore ops");
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

}  // namespace

int main() {
  check_layout();
  check_unnamed_and_numbers();
  check_equality();
  return mosa_test::failures();
}
