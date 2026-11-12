// フェーズ10 のタスク 3（ADR 0025）。ヒープの Big の箱と、それを受ける側（単一化、変種の照合、==、標準順序、writer、
// 型の検査、functor/3、=../2、ExportedTerm の往復、評価で Big が値になること）。Big は bigint_support.hpp の
// query_with_bigs で作る。

#include "bigint_support.hpp"
#include "harness.hpp"

#include "mosa/bigint.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using mosa::BigInt;

constexpr std::int64_t k_max = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t k_min = std::numeric_limits<std::int64_t>::min();

BigInt power_of_two(std::uint64_t bits) { return mosa::shift_left(BigInt(1), bits); }

std::unique_ptr<mosa::Machine> make_machine() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(":- use_module(library(lists)).").ok, "load library(lists)");
  return machine;
}

mosa::StopKind run(mosa::Machine& machine, std::string_view goal, const std::vector<BigInt>& values) {
  return query_with_bigs(machine, goal, values).kind;
}

// 名前の変数の値。無ければ Cell{}（タグは Ref）を返すので、呼び手はタグを見てから値を読む。
mosa::Cell bound(mosa::Machine& machine, const mosa::Stop& stop, std::string_view name) {
  for (const mosa::Binding& binding : stop.bindings) {
    if (binding.name == name) return machine.heap().deref(binding.variable);
  }
  return mosa::Cell{};
}

bool holds_big(mosa::Machine& machine, mosa::Cell cell, const BigInt& value) {
  return cell.tag() == mosa::Tag::Big && machine.heap().big_value(cell) == value;
}

std::string atom_text(mosa::Machine& machine, mosa::Cell cell) {
  if (cell.tag() != mosa::Tag::Atom) return "<not an atom>";
  return std::string{machine.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3))};
}

// goal が書く字面。with_output_to/2 の atom(A) で受ける。
std::string written(mosa::Machine& machine, const std::string& goal, const std::vector<BigInt>& values) {
  const mosa::Stop stop = query_with_bigs(machine, "with_output_to(atom(A), " + goal + ")", values);
  if (stop.kind != mosa::StopKind::Success) return "<" + goal + " did not succeed>";
  return atom_text(machine, bound(machine, stop, "A"));
}

std::vector<mosa::Cell> list_items(const mosa::Heap& heap, mosa::Cell list) {
  std::vector<mosa::Cell> items;
  list = heap.deref(list);
  while (list.tag() == mosa::Tag::Lis) {
    items.push_back(heap.deref(heap.list_head(list)));
    list = heap.deref(heap.list_tail(list));
  }
  return items;
}

void check_box() {
  auto machine = mosa::Machine::create();
  mosa::Heap& heap = machine->heap();
  const BigInt big = power_of_two(100);
  const mosa::Cell cell = heap.big(big);
  mosa_test::check(cell.tag() == mosa::Tag::Big, "box: 2^100 is a Big");
  mosa_test::check(heap.big_value(cell) == big, "box: 2^100 reads back");
  const auto magnitude = heap.big_magnitude(cell);
  mosa_test::check(!heap.big_negative(cell) && magnitude.size() == 2 && magnitude[0] == 0 &&
                       magnitude[1] == (std::uint64_t{1} << 36),
                   "box: 2^100 sign and words");
  mosa_test::check(*heap.word(cell) == mosa::big_header(2, false), "box: 2^100 header word");
  const mosa::Cell negative = heap.big(-big);
  mosa_test::check(negative.tag() == mosa::Tag::Big && heap.big_negative(negative) && heap.big_value(negative) == -big,
                   "box: -2^100");
  const mosa::Cell edge = heap.big(BigInt(k_max) + BigInt(1));
  mosa_test::check(edge.tag() == mosa::Tag::Big && heap.big_magnitude(edge).size() == 1 &&
                       heap.big_magnitude(edge)[0] == (std::uint64_t{1} << 63),
                   "box: 2^63 is a one-word Big");
  const mosa::Cell low = heap.big(BigInt(k_min) - BigInt(1));
  mosa_test::check(low.tag() == mosa::Tag::Big && heap.big_negative(low) && heap.big_magnitude(low).size() == 1 &&
                       heap.big_magnitude(low)[0] == (std::uint64_t{1} << 63) + 1,
                   "box: -2^63-1 is a one-word negative Big");
  const mosa::Cell max = heap.big(BigInt(k_max));
  mosa_test::check(max.tag() == mosa::Tag::IntBox && heap.integer_value(max) == k_max, "box: INT64_MAX is an IntBox");
  const mosa::Cell min = heap.big(BigInt(k_min));
  mosa_test::check(min.tag() == mosa::Tag::IntBox && heap.integer_value(min) == k_min, "box: INT64_MIN is an IntBox");
  const mosa::Cell five = heap.big(BigInt(5));
  mosa_test::check(five.tag() == mosa::Tag::Int && heap.integer_value(five) == 5, "box: 5 is an Int");
  mosa_test::check(heap.big_value(five) == BigInt(5) && heap.big_value(max) == BigInt(k_max),
                   "box: big_value reads Int and IntBox");
}

// mosatest --roundtrip の変種の照合（same_term）は、別のマシンのヒープどうしで Big を語で比べる。
void check_variant() {
  auto left = mosa::Machine::create();
  auto right = mosa::Machine::create();
  const BigInt big = power_of_two(100);
  const mosa::Cell box = left->heap().big(big);
  const auto same = [&](const BigInt& value) {
    return mosa::same_term(left->heap(), box, {}, right->heap(), right->heap().big(value), {});
  };
  mosa_test::check(same(big), "variant: B and B in two heaps");
  mosa_test::check(!same(big + BigInt(1)), "variant: B and B+1");
  mosa_test::check(!same(-big), "variant: B and -B");
  mosa_test::check(!same(BigInt(k_max)), "variant: B and the IntBox INT64_MAX");
  const std::array<mosa::Cell, 1> left_args{box};
  const std::array<mosa::Cell, 1> right_args{right->heap().big(big)};
  const mosa::Cell left_term = left->heap().structure(left->atoms().intern("f"), left_args);
  const mosa::Cell right_term = right->heap().structure(right->atoms().intern("f"), right_args);
  mosa_test::check(mosa::same_term(left->heap(), left_term, {}, right->heap(), right_term, {}),
                   "variant: f(B) and f(B) in two heaps");
}

void check_unify() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  constexpr std::string_view goal = "X = '$big0', Y = '$big1', X = Y";
  const mosa::Stop same = query_with_bigs(*machine, goal, std::vector<BigInt>{big, big});
  mosa_test::check(same.kind == mosa::StopKind::Success, "unify: B = B");
  mosa_test::check(holds_big(*machine, bound(*machine, same, "X"), big), "unify: X is the Big B");
  mosa_test::check(run(*machine, goal, {big, big + BigInt(1)}) == mosa::StopKind::Failure, "unify: B = B+1 fails");
  mosa_test::check(run(*machine, goal, {big, BigInt(k_max)}) == mosa::StopKind::Failure, "unify: B = max fails");
  mosa_test::check(run(*machine, goal, {big, -big}) == mosa::StopKind::Failure, "unify: B = -B fails");
  mosa_test::check(run(*machine, "f('$big0', a) = f('$big1', a)", {-big, -big}) == mosa::StopKind::Success,
                   "unify: -B inside compounds");
}

void check_msort() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  const mosa::Stop stop = query_with_bigs(*machine, "msort(['$big0', '$big1', 0, '$big2', 1.0], L)",
                                          std::vector<BigInt>{-big, big, BigInt(k_min)});
  mosa_test::check(stop.kind == mosa::StopKind::Success, "msort: succeeds");
  const mosa::Heap& heap = machine->heap();
  const std::vector<mosa::Cell> items = list_items(heap, bound(*machine, stop, "L"));
  mosa_test::check(items.size() == 5, "msort: 5 items");
  if (items.size() != 5) return;
  mosa_test::check(items[0].tag() == mosa::Tag::Float && heap.float_value(items[0]) == 1.0, "msort: 1.0 first");
  mosa_test::check(holds_big(*machine, items[1], -big), "msort: -B second");
  mosa_test::check(items[2].tag() == mosa::Tag::IntBox && heap.integer_value(items[2]) == k_min, "msort: -2^63 third");
  mosa_test::check(items[3].tag() == mosa::Tag::Int && heap.integer_value(items[3]) == 0, "msort: 0 fourth");
  mosa_test::check(holds_big(*machine, items[4], big), "msort: B last");
}

std::string compared(mosa::Machine& machine, const std::vector<BigInt>& values) {
  const mosa::Stop stop = query_with_bigs(machine, "compare(O, '$big0', '$big1')", values);
  if (stop.kind != mosa::StopKind::Success) return "<no answer>";
  return atom_text(machine, bound(machine, stop, "O"));
}

void check_order() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  mosa_test::check(run(*machine, "'$big0' == '$big1'", {big, big}) == mosa::StopKind::Success, "order: B == B");
  mosa_test::check(run(*machine, "'$big0' == '$big1'", {big, big + BigInt(1)}) == mosa::StopKind::Failure,
                   "order: B == B+1 fails");
  mosa_test::check(run(*machine, "'$big0' \\== '$big1'", {big, -big}) == mosa::StopKind::Success, "order: B \\== -B");
  mosa_test::check(compared(*machine, {big, big + BigInt(1)}) == "<", "order: B < B+1");
  mosa_test::check(compared(*machine, {big, BigInt(5)}) == ">", "order: B > 5");
  mosa_test::check(compared(*machine, {-big, BigInt(k_min)}) == "<", "order: -B < -2^63");
  mosa_test::check(compared(*machine, {BigInt(k_max) + BigInt(1), BigInt(k_max)}) == ">", "order: 2^63 > INT64_MAX");
  mosa_test::check(compared(*machine, {-big, big}) == "<", "order: -B < B");
  mosa_test::check(compared(*machine, {big, big}) == "=", "order: B = B");
  mosa_test::check(run(*machine, "1.0e40 @< '$big0', '$big0' @< a, '$big0' @< f(x)", {-big}) == mosa::StopKind::Success,
                   "order: float < Big < atom < compound");
}

// writeq の字面の形（空白、括弧）が int64 と同じになるか。@ に 7 か -7 を置いた字面を書き、7 を B の数字に替えれば、
// @ に Big を置いた字面と一致するはず（unsigned_number と leftmost_unsigned_number の判断）。
struct Shape {
  std::string_view text;
  bool negative;
};

constexpr std::array<Shape, 13> k_shapes{{{"-(@)", false},
                                          {"-(@)", true},
                                          {"1 - @", true},
                                          {"a- @", false},
                                          {"a- @", true},
                                          {"2 ** @", true},
                                          {"@ ^ 2", true},
                                          {"- (- (@))", false},
                                          {"f(@)", true},
                                          {"[@]", true},
                                          {"'$VAR'(@)", true},
                                          {"@", true},
                                          {"@ - 1", true}}};

std::string replaced(std::string_view text, std::string_view from, std::string_view to) {
  std::string out;
  for (std::size_t at = 0; at < text.size();) {
    if (text.substr(at).starts_with(from)) {
      out += to;
      at += from.size();
    } else {
      out += text[at++];
    }
  }
  return out;
}

void check_writer() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  mosa_test::check(written(*machine, "write_canonical('$big0')", {-big}) == "-1267650600228229401496703205376",
                   "writer: write_canonical(-B)");
  mosa_test::check(written(*machine, "writeq('$VAR'('$big0'))", {power_of_two(70)}) == "K45407370027592742439",
                   "writer: writeq('$VAR'(2^70))");
  // 2^70 mod 26 = 10。15 を足すと字母は最後の Z、16 を足すと A に戻って番号が 1 増える。
  mosa_test::check(written(*machine, "writeq('$VAR'('$big0'))", {power_of_two(70) + BigInt(15)}) ==
                       "Z45407370027592742439",
                   "writer: writeq('$VAR'(2^70+15))");
  mosa_test::check(written(*machine, "writeq('$VAR'('$big0'))", {power_of_two(70) + BigInt(16)}) ==
                       "A45407370027592742440",
                   "writer: writeq('$VAR'(2^70+16))");
  const std::string digits = mosa::to_text(big);
  for (const Shape& shape : k_shapes) {
    const std::string small = replaced(shape.text, "@", shape.negative ? "-7" : "7");
    const std::string boxed = replaced(shape.text, "@", "'$big0'");
    const std::string expected = replaced(written(*machine, "writeq(" + small + ")", {}), "7", digits);
    const std::string actual = written(*machine, "writeq(" + boxed + ")", {shape.negative ? -big : big});
    mosa_test::check(actual == expected, "writer: " + boxed + " wrote " + actual + ", int64 shape " + expected);
  }
}

void check_types() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  for (const BigInt& value : {big, -big}) {
    for (const std::string_view goal : {"integer('$big0')", "number('$big0')", "atomic('$big0')"}) {
      mosa_test::check(run(*machine, goal, {value}) == mosa::StopKind::Success, "types: " + std::string{goal});
    }
    for (const std::string_view goal :
         {"var('$big0')", "compound('$big0')", "atom('$big0')", "float('$big0')", "callable('$big0')"}) {
      mosa_test::check(run(*machine, goal, {value}) == mosa::StopKind::Failure, "types: not " + std::string{goal});
    }
  }
}

void check_terms() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  const mosa::Stop stop =
      query_with_bigs(*machine, "functor('$big0', N, A), N == '$big1', A == 0", std::vector<BigInt>{big, big});
  mosa_test::check(stop.kind == mosa::StopKind::Success, "terms: functor(B, N, A)");
  mosa_test::check(holds_big(*machine, bound(*machine, stop, "N"), big), "terms: functor name is B");
  mosa_test::check(run(*machine, "functor(X, '$big0', 0), X == '$big1'", {-big, -big}) == mosa::StopKind::Success,
                   "terms: functor(X, -B, 0)");
  mosa_test::check(run(*machine, "functor('$big0', _, '$big1')", {big, big}) == mosa::StopKind::Failure,
                   "terms: functor(B, _, B) fails");
  mosa_test::check(run(*machine, "'$big0' =.. L, L == ['$big1']", {big, big}) == mosa::StopKind::Success,
                   "terms: B =.. [B]");
  mosa_test::check(run(*machine, "X =.. ['$big0'], X == '$big1'", {-big, -big}) == mosa::StopKind::Success,
                   "terms: X =.. [-B]");
}

void check_exported() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  const mosa::Stop stop =
      query_with_bigs(*machine, "catch(throw(e('$big0')), e(Z), true)", std::vector<BigInt>{big});
  mosa_test::check(stop.kind == mosa::StopKind::Success, "exported: catch succeeds");
  mosa_test::check(holds_big(*machine, bound(*machine, stop, "Z"), big), "exported: Z == B");
  mosa_test::check(run(*machine, "catch(throw(e('$big0')), e(Z), true), Z == '$big1'", {-big, -big}) ==
                       mosa::StopKind::Success,
                   "exported: -B round trip");
  // 取り込みは正規形に直す。int64 に収まる値を Big の節点で渡しても、Int か IntBox になる。
  const std::array<std::pair<BigInt, mosa::Tag>, 3> canonical{
      {{BigInt(k_max), mosa::Tag::IntBox}, {BigInt(k_min), mosa::Tag::IntBox}, {BigInt(5), mosa::Tag::Int}}};
  for (const auto& [value, tag] : canonical) {
    const mosa::Stop got = query_with_bigs(*machine, "X = '$big0'", std::vector<BigInt>{value});
    const mosa::Cell cell = bound(*machine, got, "X");
    mosa_test::check(cell.tag() == tag && BigInt(machine->heap().integer_value(cell)) == value,
                     "exported: " + mosa::to_text(value) + " imports in canonical form");
  }
}

// タスク 6 から Big は評価できる（タスク 3 の暫定の int_overflow はなくなった）。is の値は B そのもので、
// それを == で確かめる。比較も Big のまま成り立つ。
void check_eval() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  for (const std::string_view goal : {"X is '$big0' + 0", "X is '$big0'"}) {
    mosa_test::check(run(*machine, std::string{goal} + ", X == '$big1'", {big, big}) == mosa::StopKind::Success,
                     "eval: " + std::string{goal} + " gives X == B");
  }
  mosa_test::check(run(*machine, "'$big0' > 0", {big}) == mosa::StopKind::Success, "eval: '$big0' > 0 holds");
}

}  // namespace

int main() {
  check_box();
  check_variant();
  check_unify();
  check_msort();
  check_order();
  check_writer();
  check_types();
  check_terms();
  check_exported();
  check_eval();
  return mosa_test::failures();
}
