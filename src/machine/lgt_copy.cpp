// 表明は別機械で実行する。query は選択点も節も捨てるので使わない。
// -2^63 を字面に戻すと正のトークンが溢れて変数になる。ゴールと同じセル写しで節を載せる。

#include "lgt_priv.hpp"
#include "mosa/heap_option.hpp"

#include "internal.hpp"

#include <array>
#include <utility>
#include <vector>

namespace mosa {
namespace {

const Cell* copied_seen(const std::vector<std::pair<std::uint64_t, Cell>>& seen, std::uint64_t raw) {
  for (const auto& item : seen) {
    if (item.first == raw) return &item.second;
  }
  return nullptr;
}

Cell copy_atom(Impl& dst, const Impl& src, Cell cell) {
  const auto name = src.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3));
  return dst.heap.atom(dst.atoms.intern(name));
}

Cell copy_number(Impl& dst, const Impl& src, Cell cell) {
  if (cell.tag() == Tag::Float) return dst.heap.floating(src.heap.float_value(cell));
  return dst.heap.integer(src.heap.integer_value(cell));
}

Cell copy_goal(Impl& src, Impl& dst, Cell cell, std::vector<std::pair<std::uint64_t, Cell>>& seen);

Cell copy_compound(Impl& src, Impl& dst, Cell cell, std::vector<std::pair<std::uint64_t, Cell>>& seen) {
  const Cell hole = dst.heap.fresh_ref();
  seen.emplace_back(cell.raw(), hole);
  Cell real;
  if (cell.tag() == Tag::Lis) {
    const Cell head = copy_goal(src, dst, src.heap.list_head(cell), seen);
    const Cell tail = copy_goal(src, dst, src.heap.list_tail(cell), seen);
    real = dst.heap.list(head, tail);
  } else {
    const auto arity = src.heap.functor_arity(cell);
    std::vector<Cell> args;
    args.reserve(arity);
    for (std::uint16_t index = 0; index < arity; ++index) {
      args.push_back(copy_goal(src, dst, src.heap.arg(cell, index), seen));
    }
    const auto name = src.atoms.name(src.heap.functor_atom(cell));
    real = dst.heap.structure(dst.atoms.intern(name), args);
  }
  dst.heap.bind(hole, real);
  return hole;
}

Cell copy_goal(Impl& src, Impl& dst, Cell cell, std::vector<std::pair<std::uint64_t, Cell>>& seen) {
  cell = src.heap.deref(cell);
  const bool share = src.heap.unbound(cell) || cell.tag() == Tag::Lis || cell.tag() == Tag::Str;
  if (share) {
    if (const Cell* found = copied_seen(seen, cell.raw())) return *found;
  }
  if (src.heap.unbound(cell)) {
    const Cell fresh = dst.heap.fresh_ref();
    seen.emplace_back(cell.raw(), fresh);
    return fresh;
  }
  if (cell.tag() == Tag::Atom) return copy_atom(dst, src, cell);
  if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox || cell.tag() == Tag::Float) {
    return copy_number(dst, src, cell);
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return {};
  return copy_compound(src, dst, cell, seen);
}

bool copied_succeeds(Machine& scratch, Cell goal) {
  Impl& machine = engine_impl(scratch);
  // 計画 Task 5 で RootScope にするか決める仮の措置。それまで写した先の機械では GC しない。
  const GcInhibit inhibit(machine);
  Entry cleanup;
  cleanup.pick = Pick::Cleanup;
  cleanup.heap_mark = machine.heap.size();
  cleanup.trail_mark = machine.trail.slots.size();
  cleanup.extras_mark = machine.extras.size();
  machine.stack.push_back(std::move(cleanup));
  const Compiled compiled = compile_goal(machine.heap, goal);
  launch(machine, compiled.term, kNoFrame, 0, compiled.env, machine.user_module);
  return run_loop(machine).kind == StopKind::Success;
}

bool keep_predicate(const Impl& src, const Predicate& predicate) {
  // 組込は Machine::create の分岐のまま。述語表へ重ねると二重になる。
  if (is_builtin_procedure(src, predicate.name, predicate.arity) || predicate.abolished) return false;
  return predicate.dynamic || live_count(predicate) > 0;
}

bool compile_copied(Impl& src, Impl& dst, const Clause& clause, Clause& out) {
  std::vector<Cell> slots;
  const Cell head = ir_to_term(src, clause.head, slots, false);
  const Cell body = ir_to_term(src, clause.body, slots, true);
  std::vector<std::pair<std::uint64_t, Cell>> seen;
  const Cell copied_head = copy_goal(src, dst, head, seen);
  const Cell copied_body = copy_goal(src, dst, body, seen);
  const std::array<Cell, 2> args{copied_head, copied_body};
  const Cell term = dst.heap.structure(dst.atoms.intern(":-"), args);
  return compile_clause(dst.heap, term, out);
}

void plant_predicate(Impl& src, Impl& dst, const Predicate& predicate) {
  Predicate made;
  made.module = dst.atoms.intern(src.atoms.name(predicate.module));
  made.name = dst.atoms.intern(src.atoms.name(predicate.name));
  made.arity = predicate.arity;
  made.dynamic = predicate.dynamic;
  made.dynamic_declared = predicate.dynamic_declared;
  made.meta_declared = predicate.meta_declared;
  for (MetaArg arg : predicate.meta) {
    // None の mode はアトムの番号。複製先の表で引き直す。
    if (arg.kind == MetaArg::Kind::None) arg.mode = dst.atoms.intern(src.atoms.name(arg.mode));
    made.meta.push_back(arg);
  }
  made.seen = predicate.seen;
  for (const Clause& clause : predicate.clauses) {
    // 死んだ節は元の機械の開いた走査のためだけにある。写した先には走査が無い。
    if (clause.dead) continue;
    Clause copied;
    if (!compile_copied(src, dst, clause, copied)) continue;
    made.clauses.push_back(std::move(copied));
  }
  if (made.clauses.empty() && !made.dynamic) return;
  dst.predicates.push_back(std::move(made));
}

void plant_user_clauses(Impl& src, Impl& dst) {
  const auto mark = src.heap.size();
  try {
    for (const Predicate& predicate : src.predicates) {
      if (!keep_predicate(src, predicate)) continue;
      plant_predicate(src, dst, predicate);
    }
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    // 表明の一時項だけ戻す。開いている query の水位より後ろを残さない。
    src.heap.truncate(mark);
    throw;
  }
  src.heap.truncate(mark);
}

}  // namespace

bool lgt_run_copied_goal(Machine& main, Machine& scratch, Cell goal) {
  Impl& src = engine_impl(main);
  Impl& dst = engine_impl(scratch);
  plant_user_clauses(src, dst);
  std::vector<std::pair<std::uint64_t, Cell>> seen;
  const Cell copied = copy_goal(src, dst, goal, seen);
  return copied_succeeds(scratch, copied);
}

}  // namespace mosa
