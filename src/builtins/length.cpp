// length/2。ISO/IEC 13211-1 に条項は無い。system の組込にし、library(lists) が再公開する。
// 振る舞いは tests/conformance の commons length/2 に合わせる。
// length(?list, ?integer)
//   type_error(list), type_error(integer), domain_error(not_less_than_zero)
// テスト: tests/pl/length.pl

#include "../machine/internal.hpp"

namespace mosa {
namespace {

bool is_int(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

void fail(Impl& machine) { machine.failed = true; }

void type_at(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
}

// 長さが具体値なら、リストを見る前に整数であり、0 以上であることを確かめる。
bool count_instantiated(Impl& machine, const Goal& goal, Cell count) {
  if (machine.heap.unbound(count)) return true;
  if (!is_int(count)) {
    type_at(machine, goal, "integer", count);
    return false;
  }
  if (machine.heap.integer_value(count) >= 0) return true;
  raise_formal(machine, formal_domain(machine, "not_less_than_zero", count), goal.name, goal.arity);
  return false;
}

// 未束縛の要素を前に足す。要素はどれも新しい変数なので、順は答えに出ない。
Cell open_spine(Impl& machine, std::int64_t count) {
  Cell tail = machine.heap.atom(machine.atoms.intern("[]"));
  for (std::int64_t index = 0; index < count; ++index) {
    tail = machine.heap.list(machine.heap.fresh_ref(), tail);
  }
  return tail;
}

Cell open_tail(const Impl& machine, Cell list) {
  Cell cursor = machine.heap.deref(list);
  while (cursor.tag() == Tag::Lis) cursor = machine.heap.deref(machine.heap.list_tail(cursor));
  return cursor;
}

// 終端が変数のリストは、整数まで変数を足す。整数が接頭より短いときは失敗する。
bool fill_partial(Impl& machine, const Goal& goal, Cell list, Cell count) {
  if (!is_int(count)) return false;
  const WalkedList walked = walk_list(machine, list);
  if (walked.shape != ListShape::Partial) return false;
  const auto size = machine.heap.integer_value(count);
  const auto fixed = static_cast<std::int64_t>(walked.items.size());
  if (size < fixed) {
    fail(machine);
    return true;
  }
  if (!do_unify(machine, open_tail(machine, list), open_spine(machine, size - fixed))) fail(machine);
  else succeed(machine, goal);
  return true;
}

// リストが変数で長さが 0 以上の整数なら、その長さのリストを作る。
bool build_list(Impl& machine, const Goal& goal, Cell list, Cell count) {
  if (!machine.heap.unbound(list) || !is_int(count)) return false;
  const auto size = machine.heap.integer_value(count);
  if (!do_unify(machine, list, open_spine(machine, size))) fail(machine);
  else succeed(machine, goal);
  return true;
}

bool tie_open(Impl& machine, Cell list, Cell count, std::int64_t size, std::int64_t fixed) {
  const Cell tail = open_tail(machine, list);
  if (!do_unify(machine, tail, open_spine(machine, size - fixed))) return false;
  return do_unify(machine, count, machine.heap.integer(size));
}

// 次の長さは選択点に残す。束縛より先に枠を置くので、やり直しが今の束縛を戻せる。
void leave_longer(Impl& machine, const Goal& goal, std::size_t next) {
  Entry entry;
  entry.pick = Pick::LengthGen;
  entry.redo_goal = goal.at;
  entry.next = next;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(entry));
}

// 長さが変数の開いたリストは、今の長さから順に解を出す。
bool generate(Impl& machine, const Goal& goal, Cell list, Cell count) {
  if (!machine.heap.unbound(count)) return false;
  const WalkedList walked = walk_list(machine, list);
  if (walked.shape != ListShape::Variable && walked.shape != ListShape::Partial) return false;
  const auto fixed = static_cast<std::int64_t>(walked.items.size());
  leave_longer(machine, goal, static_cast<std::size_t>(fixed + 1));
  if (!tie_open(machine, list, count, fixed, fixed)) fail(machine);
  else succeed(machine, goal);
  return true;
}

bool length_redo(Impl& machine) {
  apply_catches(machine, machine.stack.back());
  const Goal goal = read_goal(machine, machine.stack.back().redo_goal);
  const auto size = static_cast<std::int64_t>(machine.stack.back().next);
  machine.stack.back().next += 1;
  const Cell list = machine.heap.deref(goal.arg(machine, 0));
  const Cell count = machine.heap.deref(goal.arg(machine, 1));
  const auto fixed = static_cast<std::int64_t>(walk_list(machine, list).items.size());
  if (!tie_open(machine, list, count, size, fixed)) fail(machine);
  else succeed(machine, goal);
  return true;
}

// 閉じたリストの長さを返すか、渡された整数と照合する。
void run_closed(Impl& machine, const Goal& goal) {
  const Cell list = machine.heap.deref(goal.arg(machine, 0));
  const Cell count = machine.heap.deref(goal.arg(machine, 1));
  const WalkedList walked = walk_list(machine, list);
  if (walked.shape == ListShape::Bad) {
    type_at(machine, goal, "list", list);
    return;
  }
  if (walked.shape != ListShape::Proper) {
    fail(machine);
    return;
  }
  const auto size = static_cast<std::int64_t>(walked.items.size());
  if (machine.heap.unbound(count)) {
    if (!do_unify(machine, count, machine.heap.integer(size))) fail(machine);
    else succeed(machine, goal);
    return;
  }
  if (!is_int(count) || machine.heap.integer_value(count) != size) {
    fail(machine);
    return;
  }
  succeed(machine, goal);
}

}  // namespace

bool run_length(Impl& machine, const Goal& goal) {
  if (machine.atoms.name(goal.name) != "length" || goal.arity != 2) return false;
  const Cell list = machine.heap.deref(goal.arg(machine, 0));
  const Cell count = machine.heap.deref(goal.arg(machine, 1));
  if (!count_instantiated(machine, goal, count)) return true;
  if (generate(machine, goal, list, count)) return true;
  if (!build_list(machine, goal, list, count) && !fill_partial(machine, goal, list, count)) {
    run_closed(machine, goal);
  }
  return true;
}

bool redo_length(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::LengthGen) return false;
  return length_redo(machine);
}

}  // namespace mosa
