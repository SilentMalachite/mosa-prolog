#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/unify.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

void check_atoms_vars_and_cycle() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  const auto a = heap.atom(atoms.intern("a"));
  const auto b = heap.atom(atoms.intern("b"));
  const auto x = heap.fresh_ref();
  const auto y = heap.fresh_ref();
  mosa_test::check(mosa::unify(heap, a, a), "atom");
  mosa_test::check(!mosa::unify(heap, a, b) && heap.unbound(x), "atom mismatch");
  mosa_test::check(mosa::unify(heap, x, y) && !heap.unbound(x), "var var");
  const auto f = atoms.intern("f");
  const auto cycle_var = heap.fresh_ref();
  const auto cycle = heap.structure(f, std::array<mosa::Cell, 1>{cycle_var});
  mosa_test::check(mosa::unify(heap, cycle_var, cycle), "no occurs check");
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
  mosa_test::check(!mosa::unify(heap, left, right) && heap.unbound(left_var) && heap.size() == mark,
                   "failure unbinds");
  const auto same_left = heap.structure(f, std::array<mosa::Cell, 2>{a, b});
  const auto same_right = heap.structure(f, std::array<mosa::Cell, 2>{a, b});
  mosa_test::check(mosa::unify(heap, same_left, same_right), "structure");
}

void check_numbers_and_lists() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  const auto zero = heap.floating(0.0);
  const auto neg_zero = heap.floating(-0.0);
  const auto nan = heap.floating(std::numeric_limits<double>::quiet_NaN());
  mosa_test::check(mosa::unify(heap, zero, neg_zero), "signed zero");
  mosa_test::check(!mosa::unify(heap, nan, nan), "nan");
  const auto big = heap.integer(std::int64_t{1} << 60);
  const auto big2 = heap.integer(std::int64_t{1} << 60);
  mosa_test::check(mosa::unify(heap, big, big2), "boxed int");
  const auto small = heap.integer(42);
  const auto small2 = heap.integer(42);
  const auto other = heap.integer(7);
  mosa_test::check(small.tag() == mosa::Tag::Int && mosa::unify(heap, small, small2), "immediate int");
  mosa_test::check(!mosa::unify(heap, small, other), "immediate mismatch");
  const auto nil = heap.atom(atoms.intern("[]"));
  const auto a = heap.atom(atoms.intern("a"));
  const auto b = heap.atom(atoms.intern("b"));
  mosa_test::check(mosa::unify(heap, heap.list(a, nil), heap.list(a, nil)), "list");
  const auto left_list = heap.list(a, nil);
  const auto right_list = heap.list(b, nil);
  const auto mark = heap.size();
  mosa_test::check(!mosa::unify(heap, left_list, right_list) && heap.size() == mark, "list mismatch");
  mosa_test::check(std::signbit(heap.float_value(neg_zero)), "neg zero kept");
}

int main() {
  check_atoms_vars_and_cycle();
  check_failure_restores();
  check_numbers_and_lists();
  return mosa_test::failures();
}
