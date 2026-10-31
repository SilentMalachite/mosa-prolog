#include "../machine/internal.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <unordered_set>
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

struct PairHash {
  std::size_t operator()(const std::pair<std::uint64_t, std::uint64_t>& pair) const {
    return std::hash<std::uint64_t>{}(pair.first ^ (pair.second * 0x9E3779B97F4A7C15ULL));
  }
};

// 比べ始めた複合項の組を全部覚える。前と同じく、覚えた組に再び来たら等しいとみなす。小さい比較は線形の
// 表で済ませ、この数を超えたら探すのが定数時間の表へ移す。深い項でも比較が 2 乗の時間にならない。
constexpr std::size_t kFewPairs = 256;
// この深さまでは C++ の再帰で子を比べる。より深い部分木は、その場で手で積んで比べ終えてから戻る（ADR 0011）。
constexpr int kShallow = 64;

class PairSeen {
 public:
  // 前に比べ始めた組なら true を返す。新しい組は覚える。
  bool check(std::uint64_t left, std::uint64_t right) {
    if (!many_) {
      for (const auto& item : few_) {
        if (item.first == left && item.second == right) return true;
      }
      if (few_.size() < kFewPairs) {
        few_.emplace_back(left, right);
        return false;
      }
      many_.emplace(few_.begin(), few_.end());
    }
    return !many_->emplace(left, right).second;
  }

 private:
  std::vector<std::pair<std::uint64_t, std::uint64_t>> few_;
  // 処理系によっては空の表でも確保するので、比べるたびに作らないよう要る時まで持たない。
  std::optional<std::unordered_set<std::pair<std::uint64_t, std::uint64_t>, PairHash>> many_;
};

int compare_scalar(const Impl& machine, Cell left, Cell right, int rank) {
  if (rank == 0) return left.raw() < right.raw() ? -1 : (left.raw() > right.raw() ? 1 : 0);
  if (rank == 1) {
    const double a = machine.heap.float_value(left);
    const double b = machine.heap.float_value(right);
    const bool a_nan = std::isnan(a);
    const bool b_nan = std::isnan(b);
    if (a_nan || b_nan) return a_nan == b_nan ? 0 : (a_nan ? 1 : -1);
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
  }
  if (rank == 2) {
    const auto a = machine.heap.integer_value(left);
    const auto b = machine.heap.integer_value(right);
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
  }
  const auto a = machine.atoms.name(static_cast<std::uint32_t>(left.raw() >> 3));
  const auto b = machine.atoms.name(static_cast<std::uint32_t>(right.raw() >> 3));
  if (a < b) return -1;
  if (a > b) return 1;
  return 0;
}

// 組の頭だけを比べる。子を比べる必要があるときだけその数を返し、それ以外は order に答えを置いて 0 を返す。
std::uint16_t compare_head(const Impl& machine, Cell& left, Cell& right, PairSeen& seen, int& order) {
  order = 0;
  left = machine.heap.deref(left);
  right = machine.heap.deref(right);
  if (left.raw() == right.raw() && left.tag() != Tag::Float) return 0;
  const int left_rank = rank_of(machine, left);
  const int right_rank = rank_of(machine, right);
  if (left_rank != right_rank) {
    order = left_rank < right_rank ? -1 : 1;
    return 0;
  }
  if (left_rank < 4) {
    order = compare_scalar(machine, left, right, left_rank);
    return 0;
  }
  if (seen.check(left.raw(), right.raw())) return 0;
  const bool left_list = left.tag() == Tag::Lis;
  const bool right_list = right.tag() == Tag::Lis;
  const auto left_arity = left_list ? std::uint16_t{2} : machine.heap.functor_arity(left);
  const auto right_arity = right_list ? std::uint16_t{2} : machine.heap.functor_arity(right);
  if (left_arity != right_arity) {
    order = left_arity < right_arity ? -1 : 1;
    return 0;
  }
  const std::string_view left_text =
      left_list ? std::string_view{"."} : machine.atoms.name(machine.heap.functor_atom(left));
  const std::string_view right_text =
      right_list ? std::string_view{"."} : machine.atoms.name(machine.heap.functor_atom(right));
  if (left_text != right_text) {
    order = left_text < right_text ? -1 : 1;
    return 0;
  }
  return left_arity;
}

Cell child_of(const Impl& machine, Cell cell, std::uint16_t index) {
  if (cell.tag() == Tag::Lis) return index == 0 ? machine.heap.list_head(cell) : machine.heap.list_tail(cell);
  return machine.heap.arg(cell, index);
}

// 部分木を手で積んで比べる。子は左から比べるよう逆順に積むので、比べる順は前の再帰と同じになる。
int compare_deep(const Impl& machine, Cell left, Cell right, PairSeen& seen) {
  std::vector<std::pair<Cell, Cell>> todo{{left, right}};
  while (!todo.empty()) {
    auto [a, b] = todo.back();
    todo.pop_back();
    int order = 0;
    const std::uint16_t arity = compare_head(machine, a, b, seen, order);
    if (order != 0) return order;
    for (std::uint16_t index = arity; index > 0; --index) {
      const auto at = static_cast<std::uint16_t>(index - 1);
      todo.emplace_back(child_of(machine, a, at), child_of(machine, b, at));
    }
  }
  return 0;
}

int compare_seen(const Impl& machine, Cell left, Cell right, PairSeen& seen, int depth) {
  if (depth >= kShallow) return compare_deep(machine, left, right, seen);
  int order = 0;
  const std::uint16_t arity = compare_head(machine, left, right, seen, order);
  for (std::uint16_t index = 0; index < arity && order == 0; ++index) {
    order = compare_seen(machine, child_of(machine, left, index), child_of(machine, right, index), seen, depth + 1);
  }
  return order;
}

int compare_now(const Impl& machine, Cell left, Cell right) {
  PairSeen seen;
  return compare_seen(machine, left, right, seen, 0);
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

int compare_cells(const Impl& machine, Cell left, Cell right) {
  PairSeen seen;
  return compare_seen(machine, left, right, seen, 0);
}

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
