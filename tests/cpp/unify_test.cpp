#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/unify.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

bool unify_marked(mosa::Heap& heap, mosa::Cell left, mosa::Cell right) {
  mosa::Trail trail;
  return mosa::unify(heap, trail, trail.slots.size(), heap.size(), left, right);
}

}  // namespace

void check_atoms_vars_and_cycle() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  const auto a = heap.atom(atoms.intern("a"));
  const auto b = heap.atom(atoms.intern("b"));
  const auto x = heap.fresh_ref();
  const auto y = heap.fresh_ref();
  mosa_test::check(unify_marked(heap, a, a), "atom");
  mosa_test::check(!unify_marked(heap, a, b) && heap.unbound(x), "atom mismatch");
  mosa_test::check(unify_marked(heap, x, y) && !heap.unbound(x), "var var");
  const auto f = atoms.intern("f");
  const auto cycle_var = heap.fresh_ref();
  const auto cycle = heap.structure(f, std::array<mosa::Cell, 1>{cycle_var});
  mosa_test::check(unify_marked(heap, cycle_var, cycle), "no occurs check");
  mosa_test::check(heap.deref(cycle_var).tag() == mosa::Tag::Str, "x bound to f(x)");
}

void check_failure_restores() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  const auto a = heap.atom(atoms.intern("a"));
  const auto b = heap.atom(atoms.intern("b"));
  const auto f = atoms.intern("f");
  const auto left_var = heap.fresh_ref();
  const auto left = heap.structure(f, std::array<mosa::Cell, 2>{left_var, a});
  const auto right = heap.structure(f, std::array<mosa::Cell, 2>{b, b});
  const auto mark = heap.size();
  mosa_test::check(!unify_marked(heap, left, right) && heap.unbound(left_var) && heap.size() == mark,
                   "failure unbinds");
  const auto same_left = heap.structure(f, std::array<mosa::Cell, 2>{a, b});
  const auto same_right = heap.structure(f, std::array<mosa::Cell, 2>{a, b});
  mosa_test::check(unify_marked(heap, same_left, same_right), "structure");
}

void check_numbers_and_lists() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  const auto zero = heap.floating(0.0);
  const auto neg_zero = heap.floating(-0.0);
  const auto nan = heap.floating(std::numeric_limits<double>::quiet_NaN());
  mosa_test::check(unify_marked(heap, zero, neg_zero), "signed zero");
  mosa_test::check(!unify_marked(heap, nan, nan), "nan");
  const auto big = heap.integer(std::int64_t{1} << 60);
  const auto big2 = heap.integer(std::int64_t{1} << 60);
  mosa_test::check(unify_marked(heap, big, big2), "boxed int");
  const auto small = heap.integer(42);
  const auto small2 = heap.integer(42);
  const auto other = heap.integer(7);
  mosa_test::check(small.tag() == mosa::Tag::Int && unify_marked(heap, small, small2), "immediate int");
  mosa_test::check(!unify_marked(heap, small, other), "immediate mismatch");
  const auto nil = heap.atom(atoms.intern("[]"));
  const auto a = heap.atom(atoms.intern("a"));
  const auto b = heap.atom(atoms.intern("b"));
  mosa_test::check(unify_marked(heap, heap.list(a, nil), heap.list(a, nil)), "list");
  const auto left_list = heap.list(a, nil);
  const auto right_list = heap.list(b, nil);
  const auto mark = heap.size();
  mosa_test::check(!unify_marked(heap, left_list, right_list) && heap.size() == mark, "list mismatch");
  mosa_test::check(std::signbit(heap.float_value(neg_zero)), "neg zero kept");
}

constexpr int kDeep = 1000000;

// [first, 2, 3, ..., kDeep - 1, last]
mosa::Cell deep_list(mosa::Heap& heap, mosa::Cell first, mosa::Cell last) {
  mosa::Cell list = heap.list(last, heap.atom(heap.atoms().intern("[]")));
  for (int value = kDeep - 1; value >= 2; --value) list = heap.list(heap.integer(value), list);
  return heap.list(first, list);
}

mosa::Cell deep_f(mosa::Heap& heap, std::uint32_t f, mosa::Cell leaf) {
  for (int depth = 0; depth < kDeep; ++depth) leaf = heap.structure(f, std::array<mosa::Cell, 1>{leaf});
  return leaf;
}

// ADR 0011: 深さ 1,000,000 のリストと f(f(...)) を C++ の再帰なしで単一化する。失敗は一番深い所に置く。
void check_unify_deep() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  const auto last = heap.fresh_ref();
  const auto left = deep_list(heap, heap.integer(1), last);
  const auto right = deep_list(heap, heap.integer(1), heap.integer(kDeep));
  mosa_test::check(unify_marked(heap, left, right) && heap.deref(last).raw() == heap.integer(kDeep).raw(),
                   "deep list unifies");
  const auto first = heap.fresh_ref();
  const auto open = deep_list(heap, first, heap.integer(kDeep));
  const auto wrong = deep_list(heap, heap.integer(1), heap.integer(0));
  mosa_test::check(!unify_marked(heap, open, wrong) && heap.unbound(first), "deep list fails and unbinds");
  const auto f = atoms.intern("f");
  const auto a = heap.atom(atoms.intern("a"));
  const auto b = heap.atom(atoms.intern("b"));
  mosa_test::check(unify_marked(heap, deep_f(heap, f, a), deep_f(heap, f, a)), "deep f unifies");
  mosa_test::check(!unify_marked(heap, deep_f(heap, f, a), deep_f(heap, f, b)), "deep f fails");
}

// X = f(X), Y = f(Y), X = Y は止まって成功する。周期の違う巡回リストも同じ無限の項なら一致する。
void check_unify_cyclic() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  const auto f = atoms.intern("f");
  const auto a = heap.atom(atoms.intern("a"));
  const auto b = heap.atom(atoms.intern("b"));
  const auto loop = [&](mosa::Cell second) {
    const auto self = heap.fresh_ref();
    (void)unify_marked(heap, self, heap.structure(f, std::array<mosa::Cell, 2>{self, second}));
    return self;
  };
  const auto x = loop(a);
  const auto y = loop(a);
  const auto z = loop(b);
  mosa_test::check(unify_marked(heap, x, y), "cyclic f unifies");
  mosa_test::check(!unify_marked(heap, x, z), "cyclic f differs");
  const auto one = heap.integer(1);
  const auto two = heap.integer(2);
  const auto short_tail = heap.fresh_ref();
  (void)unify_marked(heap, short_tail, heap.list(one, heap.list(two, short_tail)));
  const auto long_tail = heap.fresh_ref();
  (void)unify_marked(heap, long_tail, heap.list(one, heap.list(two, heap.list(one, heap.list(two, long_tail)))));
  mosa_test::check(unify_marked(heap, short_tail, long_tail), "cyclic lists of different periods");
}

int main() {
  check_atoms_vars_and_cycle();
  check_failure_restores();
  check_numbers_and_lists();
  check_unify_deep();
  check_unify_cyclic();
  return mosa_test::failures();
}
