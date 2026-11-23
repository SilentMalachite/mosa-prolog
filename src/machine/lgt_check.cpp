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

Cell resolved_arg(Machine& machine, Cell cell, std::size_t index, const std::vector<VarName>& names,
                  const std::vector<Binding>& bindings) {
  return lgt_resolve_term(machine.heap(), machine.heap().arg(cell, index), names, bindings);
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
  return lgt_variant(machine.heap(), left, right);
}

bool check_subsumes(Machine& machine, Cell cell, const std::vector<VarName>& names,
                    const std::vector<Binding>& bindings) {
  const Cell general = resolved_arg(machine, cell, 0, names, bindings);
  const Cell specific = resolved_arg(machine, cell, 1, names, bindings);
  return lgt_subsumes(machine.heap(), general, specific);
}

// ランナーが判定しないゴールは、解の束縛でスクラッチの機械に問う。前は偽にしたので、true((X == 1, atom(a),
// variant(X, 1))) のような表明が落ちた（レビュー M-51）。
bool plain_holds(Machine& machine, Cell cell, const std::vector<VarName>& names, const std::vector<Binding>& bindings) {
  const std::string text = write_term(machine.heap(), machine.operators(), cell, lgt_write_options(), names);
  return lgt_scratch_run(machine, text, bindings).kind == LgtScratchKind::Ok;
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
  // (C -> T ; E) は C の成否で T か E の一方だけを判定する。前は (C -> T) を選言の左として試し、T が偽なら E を
  // 判定した（7.8.8、レビュー R-H01）。
  if (name == ";" && arity == 2 && lgt_functor_is(heap, heap.arg(cell, 0), "->", 2)) {
    const Cell arrow = heap.deref(heap.arg(cell, 0));
    std::string ignored;
    if (check_goal(machine, heap.arg(arrow, 0), names, bindings, ignored)) {
      return check_goal(machine, heap.arg(arrow, 1), names, bindings, note);
    }
    return check_goal(machine, heap.arg(cell, 1), names, bindings, note);
  }
  // 選言の左右は同じ判定。左が失敗しても、その説明で右を不合格にしない。
  if (name == ";" && arity == 2) {
    std::string ignored;
    if (check_goal(machine, heap.arg(cell, 0), names, bindings, ignored)) return true;
    return check_goal(machine, heap.arg(cell, 1), names, bindings, note);
  }
  if (name == "->" && arity == 2) {
    std::string ignored;
    return check_goal(machine, heap.arg(cell, 0), names, bindings, ignored) &&
           check_goal(machine, heap.arg(cell, 1), names, bindings, note);
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
  return plain_holds(machine, cell, names, bindings);
}

bool check_goal(Machine& machine, Cell cell, const std::vector<VarName>& names, const std::vector<Binding>& bindings,
                std::string& note) {
  const Heap& heap = machine.heap();
  cell = heap.deref(cell);
  if (atom_is(heap, cell, "true")) return true;
  if (atom_is(heap, cell, "fail") || atom_is(heap, cell, "false")) return false;
  if (cell.tag() == Tag::Atom) return plain_holds(machine, cell, names, bindings);
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
