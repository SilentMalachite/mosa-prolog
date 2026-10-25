#include "../machine/internal.hpp"

#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

int rank_of(const Impl& machine, Cell cell) {
  if (machine.heap.unbound(cell)) return 0;
  if (cell.tag() == Tag::Float) return 1;
  if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox) return 2;
  if (cell.tag() == Tag::Atom) return 3;
  return 4;
}

bool seen_pair(std::vector<std::pair<std::uint64_t, std::uint64_t>>& seen, std::uint64_t left,
               std::uint64_t right) {
  for (const auto& item : seen) {
    if (item.first == left && item.second == right) return true;
  }
  seen.emplace_back(left, right);
  return false;
}

int compare_cells(const Impl& machine, Cell left, Cell right,
                  std::vector<std::pair<std::uint64_t, std::uint64_t>>& seen) {
  left = machine.heap.deref(left);
  right = machine.heap.deref(right);
  if (left.raw() == right.raw() && left.tag() != Tag::Float) return 0;
  const int left_rank = rank_of(machine, left);
  const int right_rank = rank_of(machine, right);
  if (left_rank != right_rank) return left_rank < right_rank ? -1 : 1;
  if (left_rank == 0) return left.raw() < right.raw() ? -1 : (left.raw() > right.raw() ? 1 : 0);
  if (left_rank == 1) {
    const double a = machine.heap.float_value(left);
    const double b = machine.heap.float_value(right);
    const bool a_nan = std::isnan(a);
    const bool b_nan = std::isnan(b);
    if (a_nan || b_nan) return a_nan == b_nan ? 0 : (a_nan ? 1 : -1);
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
  }
  if (left_rank == 2) {
    const auto a = machine.heap.integer_value(left);
    const auto b = machine.heap.integer_value(right);
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
  }
  if (left_rank == 3) {
    const auto a = machine.atoms.name(static_cast<std::uint32_t>(left.raw() >> 3));
    const auto b = machine.atoms.name(static_cast<std::uint32_t>(right.raw() >> 3));
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
  }
  if (seen_pair(seen, left.raw(), right.raw())) return 0;
  const bool left_list = left.tag() == Tag::Lis;
  const bool right_list = right.tag() == Tag::Lis;
  const auto left_arity = left_list ? std::uint16_t{2} : machine.heap.functor_arity(left);
  const auto right_arity = right_list ? std::uint16_t{2} : machine.heap.functor_arity(right);
  if (left_arity != right_arity) return left_arity < right_arity ? -1 : 1;
  const std::string_view left_text =
      left_list ? std::string_view{"."} : machine.atoms.name(machine.heap.functor_atom(left));
  const std::string_view right_text =
      right_list ? std::string_view{"."} : machine.atoms.name(machine.heap.functor_atom(right));
  if (left_text < right_text) return -1;
  if (left_text > right_text) return 1;
  for (std::uint16_t index = 0; index < left_arity; ++index) {
    const Cell a = left_list ? (index == 0 ? machine.heap.list_head(left) : machine.heap.list_tail(left))
                             : machine.heap.arg(left, index);
    const Cell b = right_list ? (index == 0 ? machine.heap.list_head(right) : machine.heap.list_tail(right))
                              : machine.heap.arg(right, index);
    const int order = compare_cells(machine, a, b, seen);
    if (order != 0) return order;
  }
  return 0;
}

int compare_now(const Impl& machine, Cell left, Cell right) {
  std::vector<std::pair<std::uint64_t, std::uint64_t>> seen;
  return compare_cells(machine, left, right, seen);
}

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

bool ordered(Impl& machine, const Goal& goal, int mode) {
  const int order = compare_now(machine, goal.arg(machine, 0), goal.arg(machine, 1));
  const bool yes = (mode == -2 && order < 0) || (mode == -1 && order <= 0) || (mode == 0 && order == 0) ||
                   (mode == 1 && order >= 0) || (mode == 2 && order > 0);
  if (yes) succeed(machine, goal);
  else machine.failed = true;
  return true;
}

void run_compare(Impl& machine, const Goal& goal) {
  const Cell order_cell = machine.heap.deref(goal.arg(machine, 0));
  if (!machine.heap.unbound(order_cell) && order_cell.tag() != Tag::Atom) {
    raise_formal(machine, formal_type(machine, "atom", order_cell), goal.name, goal.arity);
    return;
  }
  if (order_cell.tag() == Tag::Atom) {
    const auto text = machine.atoms.name(static_cast<std::uint32_t>(order_cell.raw() >> 3));
    if (text != "<" && text != "=" && text != ">") {
      raise_formal(machine, formal_domain(machine, "order", order_cell), goal.name, goal.arity);
      return;
    }
  }
  const int order = compare_now(machine, goal.arg(machine, 1), goal.arg(machine, 2));
  const char* text = order < 0 ? "<" : (order > 0 ? ">" : "=");
  const Cell symbol = machine.heap.atom(machine.atoms.intern(text));
  if (!do_unify(machine, goal.arg(machine, 0), symbol)) machine.failed = true;
  else succeed(machine, goal);
}

}  // namespace

bool run_order(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "@<" && goal.arity == 2) return ordered(machine, goal, -2);
  if (name == "@=<" && goal.arity == 2) return ordered(machine, goal, -1);
  if (name == "==" && goal.arity == 2) return ordered(machine, goal, 0);
  if (name == "@>=" && goal.arity == 2) return ordered(machine, goal, 1);
  if (name == "@>" && goal.arity == 2) return ordered(machine, goal, 2);
  if (name == "\\==" && goal.arity == 2) {
    const int order = compare_now(machine, goal.arg(machine, 0), goal.arg(machine, 1));
    if (order != 0) succeed(machine, goal);
    else machine.failed = true;
    return true;
  }
  if (name == "compare" && goal.arity == 3) {
    run_compare(machine, goal);
    return true;
  }
  return false;
}

}
