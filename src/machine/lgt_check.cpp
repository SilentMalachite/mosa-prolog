// 表明の判定（assertion/1,2、variant/2、subsumes/2、==、=~=、比較）。ゴールとして動かさず、解の束縛で項を比べる。
// lgt_judge.cpp から分けた（730 行を超える前に、レビュー 2026-11-20 の M-51 の前）。

#include "lgt_suite.hpp"
#include "lgt_priv.hpp"

#include "mosa/bigint.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

bool atom_is(const Heap& heap, Cell cell, std::string_view name) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Atom && heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == name;
}

bool integer_cell(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return is_integer_tag(cell.tag());
}

bool on_stack(const std::vector<std::uint64_t>& stack, std::uint64_t raw) {
  for (const auto seen : stack) {
    if (seen == raw) return true;
  }
  return false;
}

Cell resolved_arg(Machine& machine, Cell cell, std::size_t index, const std::vector<VarName>& names,
                  const std::vector<Binding>& bindings) {
  return lgt_resolve_term(machine.heap(), machine.heap().arg(cell, index), names, bindings);
}

int stamp_id(std::vector<std::pair<std::uint64_t, int>>& map, std::uint64_t raw, int& next) {
  for (const auto& [id, number] : map) {
    if (id == raw) return number;
  }
  const int number = next;
  ++next;
  map.emplace_back(raw, number);
  return number;
}

Cell stamp_compound(Heap& heap, Cell cell, std::vector<std::pair<std::uint64_t, int>>& map, int& next,
                    std::vector<std::uint64_t>& stack);

// 番号を整数のまま置くと、項の中の整数と区別できない。コピーなので解の変数は残る。
Cell stamp_term(Heap& heap, Cell cell, std::vector<std::pair<std::uint64_t, int>>& map, int& next,
                std::vector<std::uint64_t>& stack) {
  cell = heap.deref(cell);
  if (heap.unbound(cell)) {
    const std::array<Cell, 1> args{heap.integer(stamp_id(map, cell.raw(), next))};
    return heap.structure(heap.atoms().intern("$n"), args);
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return cell;
  if (on_stack(stack, cell.raw())) return cell;
  return stamp_compound(heap, cell, map, next, stack);
}

Cell stamp_compound(Heap& heap, Cell cell, std::vector<std::pair<std::uint64_t, int>>& map, int& next,
                    std::vector<std::uint64_t>& stack) {
  stack.push_back(cell.raw());
  Cell built;
  if (cell.tag() == Tag::Lis) {
    const Cell head = stamp_term(heap, heap.list_head(cell), map, next, stack);
    const Cell tail = stamp_term(heap, heap.list_tail(cell), map, next, stack);
    built = heap.list(head, tail);
  } else {
    const auto arity = heap.functor_arity(cell);
    std::vector<Cell> args;
    args.reserve(arity);
    for (std::uint16_t index = 0; index < arity; ++index) {
      args.push_back(stamp_term(heap, heap.arg(cell, index), map, next, stack));
    }
    built = heap.structure(heap.functor_atom(cell), args);
  }
  stack.pop_back();
  return built;
}

bool terms_variant(Heap& heap, Cell left, Cell right) {
  std::vector<std::pair<std::uint64_t, int>> left_map;
  std::vector<std::pair<std::uint64_t, int>> right_map;
  std::vector<std::uint64_t> stack;
  int left_next = 0;
  int right_next = 0;
  const Cell stamped_left = stamp_term(heap, left, left_map, left_next, stack);
  stack.clear();
  const Cell stamped_right = stamp_term(heap, right, right_map, right_next, stack);
  return lgt_same_shape(heap, stamped_left, stamped_right);
}

// NaN との比較は unordered になり、どの比較も偽になる。double の < などと同じ結果である。
bool ordered(std::string_view name, std::partial_ordering order) {
  if (name == "<") return order < 0;
  if (name == "=<") return order <= 0;
  if (name == ">") return order > 0;
  if (name == ">=") return order >= 0;
  return false;
}

// 整数どうしは lgtunit の算術の比較と同じく正確に比べる。double を通すと 2^53 を越える整数が丸まり、2^100 >= 2^100+1 が
// 真になる（偽の合格）。int64 どうしは確保なしで比べ、Big を含むときだけ BigInt にする（integer_value は Big を読めない）。
std::optional<std::strong_ordering> integer_order(const Heap& heap, Cell left, Cell right) {
  left = heap.deref(left);
  right = heap.deref(right);
  if (!integer_cell(heap, left) || !integer_cell(heap, right)) return std::nullopt;
  if (left.tag() != Tag::Big && right.tag() != Tag::Big) return heap.integer_value(left) <=> heap.integer_value(right);
  return heap.big_value(left) <=> heap.big_value(right);
}

bool compare_args(Machine& machine, std::string_view name, Cell cell, const std::vector<VarName>& names,
                  const std::vector<Binding>& bindings) {
  const Cell left = resolved_arg(machine, cell, 0, names, bindings);
  const Cell right = resolved_arg(machine, cell, 1, names, bindings);
  if (name == "==") return lgt_same_shape(machine.heap(), left, right);
  if (name == "=~=") return approx(machine.heap(), left, right);
  if (const auto order = integer_order(machine.heap(), left, right)) return ordered(name, *order);
  const auto first = lgt_as_number(machine.heap(), left);
  const auto second = lgt_as_number(machine.heap(), right);
  if (!first || !second) return false;
  return ordered(name, *first <=> *second);
}

bool check_goal(Machine& machine, Cell cell, const std::vector<VarName>& names, const std::vector<Binding>& bindings,
                std::string& note);

bool check_variant(Machine& machine, Cell cell, const std::vector<VarName>& names,
                   const std::vector<Binding>& bindings) {
  const Cell left = resolved_arg(machine, cell, 0, names, bindings);
  const Cell right = resolved_arg(machine, cell, 1, names, bindings);
  return terms_variant(machine.heap(), left, right);
}

bool check_subsumes(Machine& machine, Cell cell, const std::vector<VarName>& names,
                    const std::vector<Binding>& bindings) {
  const Cell general = resolved_arg(machine, cell, 0, names, bindings);
  const Cell specific = resolved_arg(machine, cell, 1, names, bindings);
  return lgt_subsumes(machine.heap(), general, specific);
}

bool check_compound(Machine& machine, Cell cell, const std::vector<VarName>& names,
                    const std::vector<Binding>& bindings, std::string& note) {
  const Heap& heap = machine.heap();
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (name == "," && arity == 2) {
    return check_goal(machine, heap.arg(cell, 0), names, bindings, note) &&
           check_goal(machine, heap.arg(cell, 1), names, bindings, note);
  }
  // 選言の左右は同じ判定。左が失敗しても、その説明で右を不合格にしない。
  if (name == ";" && arity == 2) {
    std::string ignored;
    if (check_goal(machine, heap.arg(cell, 0), names, bindings, ignored)) return true;
    return check_goal(machine, heap.arg(cell, 1), names, bindings, note);
  }
  if (name == "assertion" && arity == 1) return check_goal(machine, heap.arg(cell, 0), names, bindings, note);
  if (name == "assertion" && arity == 2) {
    if (check_goal(machine, heap.arg(cell, 1), names, bindings, note)) return true;
    note = write_term(heap, machine.operators(), heap.arg(cell, 0), lgt_write_options(), names);
    return false;
  }
  if (name == "variant" && arity == 2) return check_variant(machine, cell, names, bindings);
  if (name == "subsumes" && arity == 2) {
    return check_subsumes(machine, cell, names, bindings);
  }
  if (arity == 2 && (name == "==" || name == "=~=" || name == "<" || name == "=<" || name == ">" || name == ">=")) {
    return compare_args(machine, name, cell, names, bindings);
  }
  return false;
}

bool check_goal(Machine& machine, Cell cell, const std::vector<VarName>& names, const std::vector<Binding>& bindings,
                std::string& note) {
  const Heap& heap = machine.heap();
  cell = heap.deref(cell);
  if (atom_is(heap, cell, "true")) return true;
  if (atom_is(heap, cell, "fail") || atom_is(heap, cell, "false")) return false;
  if (cell.tag() != Tag::Str) return false;
  return check_compound(machine, cell, names, bindings, note);
}

}  // namespace

bool lgt_check_text(Machine& machine, std::string_view text, const std::vector<Binding>& bindings, std::string& note) {
  const LgtParsed parsed = lgt_parse_term(machine, text);
  if (!parsed.ok) return false;
  return check_goal(machine, parsed.term, parsed.names, bindings, note);
}

}  // namespace mosa
