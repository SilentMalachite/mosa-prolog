// 包摂は同じ変数セルを同じ項にする。lgt_file が 800 行を超える前に分けた。

#include "lgt_suite.hpp"

#include "mosa/bigint.hpp"
#include "mosa/unify.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

std::string_view atom_text(const Heap& heap, Cell cell) {
  return heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3));
}

bool integer_cell(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return is_integer_tag(cell.tag());
}

// integer_value は Big を読めない（ADR 0025）。正規形では int64 に収まる値は Big にならないので、Big と int64 の組は
// 値を作らずに偽とわかる。BigInt を作るのは Big どうしのときだけにする。
bool same_integer(const Heap& heap, Cell left, Cell right) {
  left = heap.deref(left);
  right = heap.deref(right);
  if (left.tag() == Tag::Big || right.tag() == Tag::Big) {
    return left.tag() == right.tag() && heap.big_value(left) == heap.big_value(right);
  }
  return heap.integer_value(left) == heap.integer_value(right);
}

// 一度合わせた項の再出現は、この形と変数セルで見る。名前の無い same_term は別の自由変数を同じにしてしまう。
bool same_shape(const Heap& heap, Cell left, Cell right) {
  left = heap.deref(left);
  right = heap.deref(right);
  if (heap.unbound(left) || heap.unbound(right)) {
    return heap.unbound(left) && heap.unbound(right) && left.raw() == right.raw();
  }
  if (integer_cell(heap, left) && integer_cell(heap, right)) return same_integer(heap, left, right);
  if (left.tag() != right.tag()) return false;
  if (left.tag() == Tag::Atom) return atom_text(heap, left) == atom_text(heap, right);
  // 単一化と同じく -0.0 と 0.0 を分ける（iso-notes 7.2.2 の決定 A、L2 の Codex レビュー）。
  if (left.tag() == Tag::Float) return same_float_term(heap.float_value(left), heap.float_value(right));
  if (left.tag() == Tag::Lis) {
    return same_shape(heap, heap.list_head(left), heap.list_head(right)) &&
           same_shape(heap, heap.list_tail(left), heap.list_tail(right));
  }
  if (left.tag() != Tag::Str || heap.functor_arity(left) != heap.functor_arity(right)) return false;
  if (heap.atoms().name(heap.functor_atom(left)) != heap.atoms().name(heap.functor_atom(right))) return false;
  const auto arity = heap.functor_arity(left);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (!same_shape(heap, heap.arg(left, index), heap.arg(right, index))) return false;
  }
  return true;
}

struct NumberValue {
  bool integer = false;
  BigInt as_int;
  double as_float = 0.0;
};

bool fill_number(const Heap& heap, Cell cell, bool negative, NumberValue& out) {
  cell = heap.deref(cell);
  if (integer_cell(heap, cell)) {
    // -(-2^63) と Big の符号の反転は int64 に収まらないことがある。BigInt で反転すれば、どの整数も正確に揃う。
    out.integer = true;
    out.as_int = negative ? -heap.big_value(cell) : heap.big_value(cell);
    return true;
  }
  if (cell.tag() != Tag::Float) return false;
  out.integer = false;
  out.as_float = negative ? -heap.float_value(cell) : heap.float_value(cell);
  return true;
}

// 数そのものか、その前の - だけ。式は評価しない。+(1) は 1 と別の項なので畳まない（レビュー M-51）。
std::optional<NumberValue> signed_number(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  NumberValue out;
  if (fill_number(heap, cell, false, out)) return out;
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return std::nullopt;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  if (name != "-") return std::nullopt;
  if (!fill_number(heap, heap.arg(cell, 0), name == "-", out)) return std::nullopt;
  return out;
}

bool sign_of_number(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  if (name != "-") return false;
  const Cell arg = heap.deref(heap.arg(cell, 0));
  return integer_cell(heap, arg) || arg.tag() == Tag::Float;
}

bool same_number(const NumberValue& left, const NumberValue& right) {
  if (left.integer != right.integer) return false;
  if (left.integer) return left.as_int == right.as_int;
  return same_float_term(left.as_float, right.as_float);
}

// 具体の側の変数。一般の側の変数が両方に出るときは、自分自身にしか合わない（下の subsumes）。
void term_vars(const Heap& heap, Cell cell, std::vector<std::uint64_t>& vars, std::vector<std::uint64_t>& stack) {
  cell = heap.deref(cell);
  if (heap.unbound(cell)) {
    if (std::find(vars.begin(), vars.end(), cell.raw()) == vars.end()) vars.push_back(cell.raw());
    return;
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return;
  if (std::find(stack.begin(), stack.end(), cell.raw()) != stack.end()) return;
  stack.push_back(cell.raw());
  if (cell.tag() == Tag::Lis) {
    term_vars(heap, heap.list_head(cell), vars, stack);
    term_vars(heap, heap.list_tail(cell), vars, stack);
  } else {
    for (std::uint16_t index = 0; index < heap.functor_arity(cell); ++index) term_vars(heap, heap.arg(cell, index), vars, stack);
  }
  stack.pop_back();
}

struct Subsume {
  std::vector<std::pair<std::uint64_t, Cell>> bound;
  std::vector<std::uint64_t> specific_vars;
};

bool subsumes(const Heap& heap, Cell general, Cell specific, Subsume& state) {
  general = heap.deref(general);
  specific = heap.deref(specific);
  if (heap.unbound(general)) {
    for (const auto& [id, value] : state.bound) {
      if (id == general.raw()) return same_shape(heap, value, specific);
    }
    // 8.2.4（subsumes_term/2）は具体の側の変数を束縛しない。前は subsumes(X, f(X)) が通った（レビュー M-51）。
    const auto& vars = state.specific_vars;
    if (std::find(vars.begin(), vars.end(), general.raw()) != vars.end() && specific.raw() != general.raw()) return false;
    state.bound.emplace_back(general.raw(), specific);
    return true;
  }
  if (heap.unbound(specific)) return false;
  if (integer_cell(heap, general) && integer_cell(heap, specific)) return same_integer(heap, general, specific);
  // 期待の -1 は前置の複合項。投げた整数とタグが違うので、数の符号だけ揃える。
  if (sign_of_number(heap, general) || sign_of_number(heap, specific)) {
    const auto left = signed_number(heap, general);
    const auto right = signed_number(heap, specific);
    if (left && right) return same_number(*left, *right);
  }
  if (general.tag() != specific.tag()) return false;
  if (general.tag() == Tag::Atom) return atom_text(heap, general) == atom_text(heap, specific);
  if (general.tag() == Tag::Float) return same_float_term(heap.float_value(general), heap.float_value(specific));
  if (general.tag() == Tag::Lis) {
    return subsumes(heap, heap.list_head(general), heap.list_head(specific), state) &&
           subsumes(heap, heap.list_tail(general), heap.list_tail(specific), state);
  }
  if (general.tag() != Tag::Str || heap.functor_arity(general) != heap.functor_arity(specific)) return false;
  if (heap.atoms().name(heap.functor_atom(general)) != heap.atoms().name(heap.functor_atom(specific))) return false;
  const auto arity = heap.functor_arity(general);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (!subsumes(heap, heap.arg(general, index), heap.arg(specific, index), state)) return false;
  }
  return true;
}

// 変数の対応が 1 対 1 なら variant。前は変数を '$n'(K) に置き換えて比べたので、本物の '$n'(0) と区別できなかった
// （レビュー M-51）。数とアトムの比べ方は same_shape と同じ。循環する項は、同じ組に戻ったら成り立つとみる。
bool variant(const Heap& heap, Cell left, Cell right, std::vector<std::pair<std::uint64_t, std::uint64_t>>& pairs,
             std::vector<std::pair<std::uint64_t, std::uint64_t>>& stack) {
  left = heap.deref(left);
  right = heap.deref(right);
  if (heap.unbound(left) || heap.unbound(right)) {
    if (!heap.unbound(left) || !heap.unbound(right)) return false;
    for (const auto& [from, to] : pairs) {
      if (from == left.raw() || to == right.raw()) return from == left.raw() && to == right.raw();
    }
    pairs.emplace_back(left.raw(), right.raw());
    return true;
  }
  if (left.tag() != Tag::Lis && left.tag() != Tag::Str) return same_shape(heap, left, right);
  if (left.tag() != right.tag()) return false;
  const std::pair<std::uint64_t, std::uint64_t> here{left.raw(), right.raw()};
  if (std::find(stack.begin(), stack.end(), here) != stack.end()) return true;
  stack.push_back(here);
  bool same = true;
  if (left.tag() == Tag::Lis) {
    same = variant(heap, heap.list_head(left), heap.list_head(right), pairs, stack) &&
           variant(heap, heap.list_tail(left), heap.list_tail(right), pairs, stack);
  } else if (heap.functor_arity(left) != heap.functor_arity(right) ||
             heap.atoms().name(heap.functor_atom(left)) != heap.atoms().name(heap.functor_atom(right))) {
    same = false;
  } else {
    for (std::uint16_t index = 0; index < heap.functor_arity(left) && same; ++index) {
      same = variant(heap, heap.arg(left, index), heap.arg(right, index), pairs, stack);
    }
  }
  stack.pop_back();
  return same;
}

}  // namespace

bool lgt_same_shape(const Heap& heap, Cell left, Cell right) { return same_shape(heap, left, right); }

bool lgt_subsumes(const Heap& heap, Cell general, Cell specific) {
  Subsume state;
  std::vector<std::uint64_t> stack;
  term_vars(heap, specific, state.specific_vars, stack);
  return subsumes(heap, general, specific, state);
}

bool lgt_variant(const Heap& heap, Cell left, Cell right) {
  std::vector<std::pair<std::uint64_t, std::uint64_t>> pairs;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> stack;
  return variant(heap, left, right, pairs, stack);
}

}  // namespace mosa
