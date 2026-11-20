// 条件の :: と旗は実行しない。lgt_exec が 800 行を超える前に分けた。

#include "lgt_priv.hpp"

#include "internal.hpp"

#include "mosa/writer.hpp"
#include "mosa/heap_option.hpp"

#include <array>
#include <cstdint>
#include <span>
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

enum class Verdict { Yes, No, Bad, Dead };

// 実行しない Logtalk のゴールの印。そこを通ったときだけ未対応にする。通らない枝の :: は数えない。
constexpr std::string_view unsupported_ball = "$mosa_condition_unsupported";

Verdict ask_text(Machine& machine, std::string_view goal) {
  Stop stop;
  try {
    stop = query_written(machine, goal);
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    return Verdict::Dead;
  }
  // 条件の halt を、次のテストのクエリに残さない。
  Impl& impl = engine_impl(machine);
  impl.halt_now = false;
  impl.halt_pending = false;
  if (stop.kind == StopKind::Success) return Verdict::Yes;
  if (stop.kind == StopKind::Exception && atom_is(machine.heap(), stop.exception, unsupported_ball)) {
    return Verdict::Bad;
  }
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

Cell make(Heap& heap, std::string_view name, std::span<const Cell> args) {
  return heap.structure(heap.atoms().intern(name), args);
}

Cell unsupported(Heap& heap) {
  return make(heap, "throw", std::array<Cell, 1>{heap.atom(heap.atoms().intern(unsupported_ball))});
}

Cell rewrite(Machine& machine, Heap& heap, Cell cell);

// 印は変数の捕まえ手にも捕まえさせない。ほかの球は ISO の catch/3 のとおり捕まえ手と単一化し、合わなければ投げ直す。
// 回復は call/1 で包み、元の catch/3 と同じく切りを外へ出さない。
Cell guarded_catch(Machine& machine, Heap& heap, Cell cell) {
  const Cell goal = rewrite(machine, heap, heap.arg(cell, 0));
  const Cell recovery = rewrite(machine, heap, heap.arg(cell, 2));
  const Cell ball = heap.fresh_ref();
  const Cell marker = heap.atom(heap.atoms().intern(unsupported_ball));
  const Cell other = make(heap, "\\==", std::array<Cell, 2>{ball, marker});
  const Cell caught = make(heap, "=", std::array<Cell, 2>{ball, heap.arg(cell, 1)});
  const Cell test = make(heap, ",", std::array<Cell, 2>{other, caught});
  const Cell then = make(heap, "->", std::array<Cell, 2>{test, make(heap, "call", std::array<Cell, 1>{recovery})});
  const Cell choose = make(heap, ";", std::array<Cell, 2>{then, make(heap, "throw", std::array<Cell, 1>{ball})});
  return make(heap, "catch", std::array<Cell, 3>{goal, ball, choose});
}

Cell rewrite_special(Machine& machine, Heap& heap, Cell cell, std::string_view name, std::uint16_t arity) {
  const Cell yes = heap.atom(heap.atoms().intern("true"));
  const Cell no = heap.atom(heap.atoms().intern("fail"));
  // 旗は実行しない。失敗であり、存在エラーではない。
  if (name == "current_logtalk_flag" && arity == 2) return no;
  if (name == "::") return message_cond(heap, cell) == Verdict::No ? no : unsupported(heap);
  if (name == "predicate_property" && arity == 2) return property_cond(machine, heap, cell) == Verdict::Yes ? yes : no;
  return unsupported(heap);
}

// 旗、:: と predicate_property/2 は実行せずに真偽を決め、true か fail か印を投げるゴールに置き換える。残りは
// 一つの問いにする。部分ごとに問うと左の束縛が右に届かず、(C -> T ; E) は T の失敗で E へ進んだ（レビュー M-52）。
Cell rewrite(Machine& machine, Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (!calls_special(heap, cell)) return cell;
  const std::uint32_t functor = heap.functor_atom(cell);
  const std::string name{heap.atoms().name(functor)};
  const auto arity = heap.functor_arity(cell);
  if ((name == "," || name == ";" || name == "->" || name == "*->") && arity == 2) {
    const Cell left = rewrite(machine, heap, heap.arg(cell, 0));
    const Cell right = rewrite(machine, heap, heap.arg(cell, 1));
    return heap.structure(functor, std::array<Cell, 2>{left, right});
  }
  if ((name == "\\+" || name == "call" || name == "once") && arity == 1) {
    return heap.structure(functor, std::array<Cell, 1>{rewrite(machine, heap, heap.arg(cell, 0))});
  }
  // {Goal} は中を Prolog のゴールとして呼ぶ。ここでは call/1 に置き換えて切りを外へ出さない。前の評価も中を別の問い
  // にしていたので、切りは外へ届かなかった。
  if (name == "{}" && arity == 1) {
    return make(heap, "call", std::array<Cell, 1>{rewrite(machine, heap, heap.arg(cell, 0))});
  }
  if (name == "catch" && arity == 3) return guarded_catch(machine, heap, cell);
  return rewrite_special(machine, heap, cell, name, arity);
}

Verdict eval_goal(Machine& machine, Heap& heap, const OperatorTable& ops, Cell cell) {
  const Cell goal = rewrite(machine, heap, cell);
  const std::string text = write_term(heap, ops, goal, lgt_write_options(), {});
  return ask_text(machine, text + full_stop_after(text));
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
    const std::string source = std::string{goal} + full_stop_after(goal);
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
