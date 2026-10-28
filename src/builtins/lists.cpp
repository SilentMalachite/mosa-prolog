// ISO/IEC 13211-1 Cor.2 8.4.3 sort/2 (@list, -list)
//   instantiation_error, type_error(list, List), type_error(list, Sorted)
// ISO/IEC 13211-1 Cor.2 8.4.4 keysort/2 (@list, -list)
//   instantiation_error, type_error(list, Pairs), type_error(pair, E), type_error(list, Sorted)
// length/2 (?list, ?integer)。13211-1 に条項は無く、SPEC 第5節の表で system に置き library(lists) が公開し直す。
//   type_error(integer, N), type_error(list, List), domain_error(not_less_than_zero, N)
// テスト: tests/pl/lib_lists.pl
#include "../machine/internal.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace mosa {
namespace {

constexpr std::uint8_t kLengthGen = 6;

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

bool is_int(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

bool is_pair(const Impl& machine, Cell cell) {
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == "-";
}

// 8.4.4.3 の順。Pairs の形、その要素、Sorted の形、その要素。sort/2 は要素を見ない。
std::optional<Cell> sort_problem(Impl& machine, Cell list, Cell sorted, bool keyed, WalkedList& input) {
  input = walk_list(machine, list);
  if (input.shape == ListShape::Variable || input.shape == ListShape::Partial) return formal_instantiation(machine);
  if (input.shape == ListShape::Bad) return formal_type(machine, "list", machine.heap.deref(list));
  if (keyed) {
    for (const Cell item : input.items) {
      if (machine.heap.unbound(item)) return formal_instantiation(machine);
      if (!is_pair(machine, item)) return formal_type(machine, "pair", item);
    }
  }
  const WalkedList output = walk_list(machine, sorted);
  if (output.shape == ListShape::Bad) return formal_type(machine, "list", machine.heap.deref(sorted));
  if (keyed) {
    for (const Cell item : output.items) {
      if (!machine.heap.unbound(item) && !is_pair(machine, item)) return formal_type(machine, "pair", item);
    }
  }
  return std::nullopt;
}

void run_sort(Impl& machine, const Goal& goal, bool keyed) {
  WalkedList input;
  if (const auto problem = sort_problem(machine, goal.arg(machine, 0), goal.arg(machine, 1), keyed, input)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return;
  }
  std::vector<Cell> items = std::move(input.items);
  // keysort/2 は同じキーの順を保つ（8.4.4.1）。sort/2 は重複を落とすので安定でなくてよいが、同じ手で済ませる。
  std::stable_sort(items.begin(), items.end(), [&machine, keyed](Cell left, Cell right) {
    if (keyed) return compare_cells(machine, machine.heap.arg(left, 0), machine.heap.arg(right, 0)) < 0;
    return compare_cells(machine, left, right) < 0;
  });
  if (!keyed) {
    const auto same = [&machine](Cell left, Cell right) { return compare_cells(machine, left, right) == 0; };
    items.erase(std::unique(items.begin(), items.end(), same), items.end());
  }
  if (do_unify(machine, goal.arg(machine, 1), list_from_cells(machine, items))) succeed(machine, goal);
  else machine.failed = true;
}

struct LengthShape {
  std::size_t prefix = 0;
  // 未束縛なら、リストの末尾の変数（リスト全体が変数ならそれ自身）。
  std::optional<Cell> tail;
};

std::optional<Cell> length_problem(Impl& machine, const Goal& goal, LengthShape& shape) {
  const Cell list = machine.heap.deref(goal.arg(machine, 0));
  const Cell count = machine.heap.deref(goal.arg(machine, 1));
  if (!machine.heap.unbound(count) && !is_int(count)) return formal_type(machine, "integer", count);
  const WalkedList walked = walk_list(machine, list);
  if (walked.shape == ListShape::Bad) return formal_type(machine, "list", list);
  if (is_int(count) && machine.heap.integer_value(count) < 0) {
    return formal_domain(machine, "not_less_than_zero", count);
  }
  shape.prefix = walked.items.size();
  if (walked.shape == ListShape::Variable) shape.tail = list;
  if (walked.shape == ListShape::Partial) shape.tail = walked.tail;
  return std::nullopt;
}

// 末尾を size - prefix 個の新しい変数で閉じ、個数を size にする。size は利用者の整数なので先に確保しない。
// ヒープの上限で fresh_ref が ResourceLimit を投げる方が、vector の length_error より先に来る。
bool bind_length(Impl& machine, const Goal& goal, const LengthShape& shape, std::size_t size) {
  std::vector<Cell> fresh;
  for (std::size_t index = shape.prefix; index < size; ++index) fresh.push_back(machine.heap.fresh_ref());
  return do_unify(machine, *shape.tail, list_from_cells(machine, fresh)) &&
         do_unify(machine, goal.arg(machine, 1), machine.heap.integer(static_cast<std::int64_t>(size)));
}

// 個数が未束縛のときは size から一つずつ長くする。選択点は束縛の前に積み、やり直しで束縛を戻す。
void prove_length_from(Impl& machine, const Goal& goal, const LengthShape& shape, std::size_t size) {
  Entry entry;
  entry.pick = Pick::AtomGen;
  entry.gen_kind = kLengthGen;
  entry.next = size + 1;
  entry.redo_goal = goal.at;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(entry));
  if (bind_length(machine, goal, shape, size)) succeed(machine, goal);
  else machine.failed = true;
}

void run_length(Impl& machine, const Goal& goal) {
  LengthShape shape;
  if (const auto problem = length_problem(machine, goal, shape)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return;
  }
  const Cell count = machine.heap.deref(goal.arg(machine, 1));
  if (!shape.tail) {
    const Cell size = machine.heap.integer(static_cast<std::int64_t>(shape.prefix));
    if (do_unify(machine, count, size)) succeed(machine, goal);
    else machine.failed = true;
    return;
  }
  if (is_int(count)) {
    const auto wanted = static_cast<std::size_t>(machine.heap.integer_value(count));
    if (wanted >= shape.prefix && bind_length(machine, goal, shape, wanted)) succeed(machine, goal);
    else machine.failed = true;
    return;
  }
  // length(L, L) の L はリストにも整数にもなれない。生成すると止まらない。
  if (count.raw() == shape.tail->raw()) {
    machine.failed = true;
    return;
  }
  prove_length_from(machine, goal, shape, shape.prefix);
}

}  // namespace

bool redo_length_gen(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::AtomGen) return false;
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  const Goal goal = read_goal(machine, top.redo_goal);
  machine.stack.pop_back();
  // 束縛は戻っている。初めの呼び出しと同じ形を辿り直す。
  LengthShape shape;
  if (length_problem(machine, goal, shape) || !shape.tail) return false;
  // 束縛に失敗しても次の長さの選択点は積んである。failed を立てて run_loop に戻す。
  prove_length_from(machine, goal, shape, top.next);
  return true;
}

bool run_lists(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "length" && goal.arity == 2) {
    run_length(machine, goal);
    return true;
  }
  if ((name == "sort" || name == "keysort") && goal.arity == 2) {
    run_sort(machine, goal, name == "keysort");
    return true;
  }
  return false;
}

}  // namespace mosa
