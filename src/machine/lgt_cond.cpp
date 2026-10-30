// 条件の :: と旗は実行しない。lgt_exec が 800 行を超える前に分けた。

#include "lgt_priv.hpp"

#include "internal.hpp"

#include "mosa/writer.hpp"
#include "mosa/heap_option.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {
namespace {

std::string_view atom_text(const Heap& heap, Cell cell) {
  return heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3));
}

bool functor_is(const Heap& heap, Cell cell, std::string_view name, std::uint16_t arity) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Str && heap.functor_arity(cell) == arity &&
         heap.atoms().name(heap.functor_atom(cell)) == name;
}

bool atom_is(const Heap& heap, Cell cell, std::string_view name) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Atom && atom_text(heap, cell) == name;
}

enum class Verdict { Yes, No, Bad, Dead, Throw };

Verdict ask_text(Machine& machine, std::string_view goal) {
  Stop stop;
  try {
    stop = machine.query(goal);
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    return Verdict::Dead;
  }
  // 条件の halt を、次のテストのクエリに残さない。
  Impl& impl = engine_impl(machine);
  impl.halt_now = false;
  impl.halt_pending = false;
  if (stop.kind == StopKind::Success) return Verdict::Yes;
  if (stop.kind == StopKind::Exception) return Verdict::Throw;
  return Verdict::No;
}

// 呼び出しの位置だけ見る。普通の述語の引数はデータなので、旗があっても呼び出さない。
bool calls_special(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (name == "current_logtalk_flag" && arity == 2) return true;
  if (name == "::" || name == "^^") return true;
  if (name == "predicate_property" && arity == 2) return true;
  if ((name == "," || name == ";" || name == "->" || name == "*->") && arity == 2) {
    return calls_special(heap, heap.arg(cell, 0)) || calls_special(heap, heap.arg(cell, 1));
  }
  if ((name == "\\+" || name == "call" || name == "once" || name == "{}") && arity == 1) {
    return calls_special(heap, heap.arg(cell, 0));
  }
  if (name == "catch" && arity == 3) {
    return calls_special(heap, heap.arg(cell, 0)) || calls_special(heap, heap.arg(cell, 2));
  }
  return false;
}

Verdict ask_cell(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  return ask_text(machine, write_term(heap, ops, cell, lgt_write_options(), {}) + ".");
}

Verdict property_cond(Machine& machine, const Heap& heap, Cell cell) {
  const Cell pred = heap.deref(heap.arg(cell, 0));
  const Cell prop = heap.deref(heap.arg(cell, 1));
  if (!atom_is(heap, prop, "built_in")) return Verdict::No;
  std::string_view name;
  std::uint16_t arity = 0;
  if (pred.tag() == Tag::Atom) {
    name = atom_text(heap, pred);
  } else if (pred.tag() == Tag::Str) {
    name = heap.atoms().name(heap.functor_atom(pred));
    arity = heap.functor_arity(pred);
  } else {
    return Verdict::No;
  }
  Impl& impl = engine_impl(machine);
  return is_builtin_procedure(impl, impl.atoms.intern(name), arity) ? Verdict::Yes : Verdict::No;
}

Verdict message_cond(const Heap& heap, Cell cell) {
  if (!functor_is(heap, cell, "::", 2) || !atom_is(heap, heap.arg(cell, 0), "os")) return Verdict::Bad;
  const Cell msg = heap.deref(heap.arg(cell, 1));
  if (!functor_is(heap, msg, "operating_system_type", 1)) return Verdict::Bad;
  if (!atom_is(heap, heap.arg(msg, 0), "windows")) return Verdict::Bad;
  // windows は Prolog として実行しない。成功したことにしない。
  return Verdict::No;
}

Verdict eval_goal(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell);

Verdict eval_and(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  const Verdict left = eval_goal(machine, heap, ops, heap.arg(cell, 0));
  // 左が失敗したら右は実行しない。旗の右で存在エラーを起こさない。
  if (left != Verdict::Yes) return left;
  return eval_goal(machine, heap, ops, heap.arg(cell, 1));
}

Verdict eval_or(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  const Verdict left = eval_goal(machine, heap, ops, heap.arg(cell, 0));
  if (left != Verdict::No) return left;
  return eval_goal(machine, heap, ops, heap.arg(cell, 1));
}

Verdict eval_not(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  const Verdict inner = eval_goal(machine, heap, ops, heap.arg(cell, 0));
  if (inner == Verdict::Yes) return Verdict::No;
  if (inner == Verdict::No) return Verdict::Yes;
  return inner;
}

Verdict eval_if(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  const Verdict cond = eval_goal(machine, heap, ops, heap.arg(cell, 0));
  if (cond != Verdict::Yes) return cond == Verdict::No ? Verdict::No : cond;
  return eval_goal(machine, heap, ops, heap.arg(cell, 1));
}

Verdict eval_catch(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  if (!calls_special(heap, heap.arg(cell, 0)) && !calls_special(heap, heap.arg(cell, 2))) {
    return ask_cell(machine, heap, ops, cell);
  }
  const Verdict ran = eval_goal(machine, heap, ops, heap.arg(cell, 0));
  if (ran != Verdict::Throw) return ran;
  if (!heap.unbound(heap.deref(heap.arg(cell, 1)))) return Verdict::Throw;
  return eval_goal(machine, heap, ops, heap.arg(cell, 2));
}

Verdict eval_goal(Machine& machine, const Heap& heap, const OperatorTable& ops, Cell cell) {
  cell = heap.deref(cell);
  if (atom_is(heap, cell, "true")) return Verdict::Yes;
  if (atom_is(heap, cell, "fail") || atom_is(heap, cell, "false")) return Verdict::No;
  // 旗も :: も無いゴールは一つの query。分けると束縛が消え、(-> ; ) の else が走る。
  if (!calls_special(heap, cell)) return ask_cell(machine, heap, ops, cell);
  if (cell.tag() != Tag::Str) return ask_cell(machine, heap, ops, cell);
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (name == "," && arity == 2) return eval_and(machine, heap, ops, cell);
  if (name == ";" && arity == 2) return eval_or(machine, heap, ops, cell);
  if ((name == "->" || name == "*->") && arity == 2) return eval_if(machine, heap, ops, cell);
  if (name == "\\+" && arity == 1) return eval_not(machine, heap, ops, cell);
  if ((name == "call" || name == "once" || name == "{}") && arity == 1) {
    return eval_goal(machine, heap, ops, heap.arg(cell, 0));
  }
  if (name == "catch" && arity == 3) return eval_catch(machine, heap, ops, cell);
  // 旗は実行しない。失敗であり、存在エラーではない。
  if (name == "current_logtalk_flag" && arity == 2) return Verdict::No;
  if (name == "::") return message_cond(heap, cell);
  if (name == "^^") return Verdict::Bad;
  if (name == "predicate_property" && arity == 2) return property_cond(machine, heap, cell);
  return ask_cell(machine, heap, ops, cell);
}

LgtCond publish_cond(Verdict verdict) {
  if (verdict == Verdict::Yes) return LgtCond::Yes;
  if (verdict == Verdict::Bad) return LgtCond::Bad;
  if (verdict == Verdict::Dead) return LgtCond::Dead;
  return LgtCond::No;
}

bool condition_ops(OperatorTable& ops) {
  if (ops.define(600, Specifier::Xfy, ":") != OpResult::Ok) return false;
  return ops.define(600, Specifier::Xfy, "::") == OpResult::Ok;
}

}  // namespace

LgtCond lgt_condition(Machine& machine, std::string_view goal) {
  if (goal.empty()) return LgtCond::No;
  try {
    AtomTable atoms;
    Heap heap(atoms);
    OperatorTable ops = OperatorTable::iso_core();
    if (!condition_ops(ops)) return LgtCond::Bad;
    // Reader は string_view しか持たない。一時文字列のまま渡すと、next の時点で消える。
    const std::string source = std::string{goal} + ".";
    Reader reader(source, heap, ops);
    const ReadOutcome read = reader.next();
    if (!read.term) return LgtCond::Bad;
    return publish_cond(eval_goal(machine, heap, ops, read.term->term));
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    return LgtCond::Dead;
  }
}

}  // namespace mosa
