#include "internal.hpp"

#include <bit>
#include <cstdint>

namespace mosa {
namespace {

const std::uint64_t* watermark(const Impl& machine) {
  if (machine.stack.empty()) return machine.heap.data();
  return machine.heap.data() + machine.stack.back().heap_mark;
}

// まだ書いた通りの組だけを戻す。あとから define が優先度を替えていたら、その指示を残す。
void undo_touch(OperatorTable& table, const OpTouch& touch) {
  if (touch.priority == 0) {
    if (!touch.had || table.stored(touch.name, touch.specifier)) return;
    table.put_stored(touch.old_priority, touch.old_specifier, touch.name);
    return;
  }
  const auto current = table.stored(touch.name, touch.specifier);
  if (!current || current->priority != touch.priority) return;
  table.erase_stored(touch.name, touch.specifier);
  if (touch.had) table.put_stored(touch.old_priority, touch.old_specifier, touch.name);
}

}  // namespace

void restore_state(Impl& machine, std::size_t heap_mark, std::size_t trail_mark, std::size_t extras_mark) {
  while (!machine.meta_undos.empty() && machine.meta_undos.back().born >= heap_mark) {
    const MetaUndo undo = machine.meta_undos.back();
    machine.meta_undos.pop_back();
    // 枠ごと捨てる選択点では、消える語へ書き戻さない。
    if (undo.slot < heap_mark) machine.heap.set_word(undo.slot, undo.old);
  }
  while (machine.trail.slots.size() > trail_mark) {
    auto* slot = machine.trail.slots.back();
    machine.trail.slots.pop_back();
    *slot = std::bit_cast<std::uint64_t>(slot);
  }
  machine.heap.truncate(heap_mark);
  if (machine.extras.size() > extras_mark) machine.extras.resize(extras_mark);
}

void note_catches(const Impl& machine, Entry& entry) {
  entry.live_catches.clear();
  for (std::size_t index = 0; index < machine.stack.size(); ++index) {
    const Entry& frame = machine.stack[index];
    if (frame.pick == Pick::Catch && frame.armed) {
      entry.live_catches.push_back(static_cast<std::uint32_t>(index));
    }
  }
}

void apply_catches(Impl& machine, const Entry& entry) {
  std::vector<char> on(machine.stack.size(), 0);
  for (const std::uint32_t index : entry.live_catches) {
    if (index < on.size()) on[index] = 1;
  }
  for (std::size_t index = 0; index < machine.stack.size(); ++index) {
    if (machine.stack[index].pick == Pick::Catch) machine.stack[index].armed = on[index] != 0;
  }
}

void push_frame(Impl& machine, Entry entry) {
  entry.extras_mark = machine.extras.size();
  // query の Cleanup はここを通さないので印は 0。通った枠より後ろの op/3 だけを戻す。
  entry.effect_id = ++machine.effect_clock;
  entry.resume_mod = machine.calling;
  note_catches(machine, entry);
  machine.stack.push_back(std::move(entry));
}

void trail_operators(Impl& machine, int priority, Specifier specifier, const std::vector<std::string>& names) {
  if (machine.stack.empty()) return;
  const auto effect_id = machine.stack.back().effect_id;
  if (effect_id == 0) return;
  OpUndo undo;
  undo.effect_id = effect_id;
  for (const std::string& name : names) {
    OpTouch touch;
    touch.name = name;
    touch.priority = priority;
    touch.specifier = specifier;
    const auto old = priority == 0 ? machine.operators.stored(name, specifier)
                                   : machine.operators.stored_class(name, specifier);
    if (old) {
      touch.had = true;
      touch.old_priority = old->priority;
      touch.old_specifier = old->specifier;
    }
    undo.touches.push_back(std::move(touch));
  }
  machine.op_undos.push_back(std::move(undo));
}

void restore_operators(Impl& machine, std::uint64_t effect_id) {
  if (effect_id == 0) return;
  while (!machine.op_undos.empty() && machine.op_undos.back().effect_id >= effect_id) {
    const OpUndo undo = std::move(machine.op_undos.back());
    machine.op_undos.pop_back();
    for (auto it = undo.touches.rbegin(); it != undo.touches.rend(); ++it) undo_touch(machine.operators, *it);
  }
}

bool do_unify(Impl& machine, Cell left, Cell right) {
  machine.trail.mark = watermark(machine);
  return unify(machine.heap, machine.trail, machine.trail.slots.size(), machine.heap.size(), left, right);
}

bool unify_visible(Impl& machine, std::size_t floor, Cell left, Cell right) {
  const auto* saved = machine.trail.mark;
  // 節の中の匿名は Cleanup より新しい。選択点の水位だと、最初の候補の束縛が残る。
  machine.trail.mark = machine.heap.data() + floor;
  const bool ok = unify(machine.heap, machine.trail, machine.trail.slots.size(), machine.heap.size(), left, right);
  machine.trail.mark = saved;
  return ok;
}

// 定義モジュールと呼び出し元が違うときだけ枠を置く。同じなら今まで通り選択点を増やさない。
void guard_return(Impl& machine, std::uint32_t home, std::uint32_t caller) {
  if (home == caller) return;
  Entry guard;
  guard.pick = Pick::CallMod;
  guard.return_mod = caller;
  guard.heap_mark = machine.heap.size();
  guard.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(guard));
}

void enter_clause(Impl& machine, const Clause& clause, std::uint64_t goal, std::size_t cut,
                  std::uint32_t home, std::uint32_t caller) {
  // 本体の述語は定義したモジュールで探す。user の節は呼び出し元が同じなので何もしない。
  if (machine.calling != home) machine.calling = home;
  std::vector<Cell> env;
  env.reserve(clause.vars);
  for (std::uint16_t index = 0; index < clause.vars; ++index) env.push_back(machine.heap.fresh_ref());
  if (clause.head.tag == IrTag::Fun) {
    for (std::size_t index = 0; index < clause.head.args.size(); ++index) {
      const Cell expected = materialize(machine, clause.head.args[index], env);
      const Cell actual = Cell{machine.heap.word_at(goal + kFrameHead + index)};
      if (!do_unify(machine, expected, actual)) {
        machine.failed = true;
        return;
      }
    }
  }
  auto cont = machine.heap.word_at(goal + 3);
  if (caller != home) cont = restore_calling(machine, caller, cont);
  launch(machine, clause.body, cont, cut, env);
}

void miss_user(Impl& machine, const Goal& goal) {
  // 7.7.7 b。解決順に無い手続きは、他モジュールにあっても呼び出し側には無い。
  // unknown=fail のときだけ失敗にし、error では存在エラーにする。
  if (machine.unknown_fail) {
    machine.failed = true;
    return;
  }
  const Cell pi = cell_functor(machine, goal.name, goal.arity);
  raise_formal(machine, formal_existence(machine, "procedure", pi), goal.name, goal.arity);
}

void enter_user(Impl& machine, const Goal& goal) {
  Predicate* slot = predicate_in(machine, machine.calling, goal.name, goal.arity);
  // 第5節の解決順の 2。user 以外は既定の取り込み元 user を見る。user の取り込みもここに含む。
  if (slot == nullptr && machine.calling != machine.user_mod) {
    slot = predicate_in(machine, machine.user_mod, goal.name, goal.arity);
  }
  if (slot == nullptr) {
    miss_user(machine, goal);
    return;
  }
  // 取り込みの節は定義したモジュールにある。席だけを見ると呼び出しが失敗する。
  // 別名の席は呼び出し側の名前なので、節は公開したときの名前で探す。
  Predicate* predicate = slot;
  if (slot->imported) {
    const auto source_name = slot->origin_name != 0 ? slot->origin_name : goal.name;
    predicate = predicate_in(machine, slot->origin, source_name, goal.arity);
    if (predicate == nullptr) {
      machine.failed = true;
      return;
    }
  }
  if (predicate->clauses.empty()) {
    // 節を消し切っても動的述語は残る。呼び出しは失敗であり、存在エラーではない。
    machine.failed = true;
    return;
  }
  // 包みは選択点より前。やり直しがヒープを戻しても、モジュール付きの引数が残る。
  wrap_meta(machine, goal, *predicate);
  const bool has_arg = goal.arity >= 1;
  const Cell first = has_arg ? machine.heap.deref(goal.arg(machine, 0)) : Cell{};
  const auto candidates = select_clauses(machine, *predicate, has_arg, first);
  if (candidates.empty()) {
    machine.failed = true;
    return;
  }
  const auto caller = machine.calling;
  guard_return(machine, predicate->home, caller);
  const auto boundary = machine.stack.size();
  if (candidates.size() > 1) {
    Entry entry;
    entry.pick = Pick::Clauses;
    entry.candidates = candidates;
    entry.next = 1;
    entry.redo_goal = goal.at;
    // やり直しは節のある述語を探す。別名だと席の名前と違う。
    entry.pred_name = predicate->name;
    entry.pred_arity = goal.arity;
    entry.pred_home = predicate->home;
    entry.return_mod = caller;
    entry.cut = boundary;
    entry.heap_mark = machine.heap.size();
    entry.trail_mark = machine.trail.slots.size();
    push_frame(machine, std::move(entry));
  }
  enter_clause(machine, predicate->clauses[candidates[0]], goal.at, boundary, predicate->home, caller);
}

void arm_caller(Impl& machine, std::uint32_t caller) {
  Entry guard;
  guard.pick = Pick::CallMod;
  guard.return_mod = caller;
  guard.heap_mark = machine.heap.size();
  guard.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(guard));
}

bool redo_clause(Impl& machine) {
  const Entry top = machine.stack.back();
  if (top.next >= top.candidates.size()) {
    machine.stack.pop_back();
    return false;
  }
  apply_catches(machine, top);
  const auto index = top.candidates[top.next];
  const auto name = top.pred_name;
  const auto arity = top.pred_arity;
  const auto goal = top.redo_goal;
  const auto cut = top.cut;
  const Predicate* predicate = predicate_in(machine, top.pred_home, name, arity);
  // 述語ごと消えると添字が外れる。枠を残すとやり直しが終わらない。
  if (predicate == nullptr || index >= predicate->clauses.size()) {
    machine.stack.pop_back();
    return false;
  }
  if (top.next + 1 >= top.candidates.size()) machine.stack.pop_back();
  else machine.stack.back().next += 1;
  enter_clause(machine, predicate->clauses[index], goal, cut, predicate->home, top.return_mod);
  return true;
}

bool redo_else(Impl& machine) {
  const Entry top = machine.stack.back();
  if (top.soft_id != 0 && top.cond_ok) {
    machine.stack.pop_back();
    return false;
  }
  apply_catches(machine, top);
  const IrTerm goal = top.else_goal;
  const auto env = top.env;
  const auto cont = top.cont;
  const auto cut = top.cut;
  machine.stack.pop_back();
  launch(machine, goal, cont, cut, env);
  return true;
}

bool redo_not(Impl& machine) {
  const Entry top = machine.stack.back();
  apply_catches(machine, top);
  const auto cont = top.cont;
  machine.stack.pop_back();
  machine.ip = cont;
  return true;
}

bool redo_repeat(Impl& machine) {
  apply_catches(machine, machine.stack.back());
  machine.ip = machine.stack.back().cont;
  return true;
}

bool backtrack(Impl& machine) {
  machine.failed = false;
  machine.exception = false;
  while (!machine.stack.empty()) {
    const Entry top = machine.stack.back();
    restore_state(machine, top.heap_mark, top.trail_mark, top.extras_mark);
    // 成功した catch は選択点ではない。記録は親の選択点まで残す。
    if (!(top.pick == Pick::Catch && !top.armed)) restore_operators(machine, top.effect_id);
    // 収集の床。束縛は消すが、呼び出し元の節走査はやり直さない。
    if (top.pick == Pick::Barrier) return false;
    if (top.pick == Pick::Catch || top.pick == Pick::Cleanup || top.pick == Pick::Once) {
      machine.stack.pop_back();
      continue;
    }
    // 節が失敗したときは、成功の戻りが走らない。ここで呼び出し元へ戻す。
    if (top.pick == Pick::CallMod) {
      machine.calling = top.return_mod;
      machine.stack.pop_back();
      continue;
    }
    // 節の成功は RestoreMod で呼び出し元へ戻している。本体の ; や repeat は作った節のモジュールで続ける。
    machine.calling = top.resume_mod;
    if (top.pick == Pick::Clauses && redo_clause(machine)) return true;
    if (top.pick == Pick::Retract && redo_retract(machine)) return true;
    // Clauses は節の本体を実行する。参照と列挙は項を返すだけで、本体は走らせない。
    if (top.pick == Pick::ClauseScan && redo_clause_scan(machine)) return true;
    if (top.pick == Pick::PredGen && redo_pred_gen(machine)) return true;
    if (top.pick == Pick::ElseGoal && redo_else(machine)) return true;
    if (top.pick == Pick::Negation && redo_not(machine)) return true;
    if (top.pick == Pick::Repeat && redo_repeat(machine)) return true;
    // アトムの候補は節ではない。やり直しを節の添字に載せると、別の述語を引いてしまう。
    if (top.pick == Pick::AtomGen && redo_atoms(machine)) return true;
    if (top.pick == Pick::StreamGen && redo_stream_prop(machine)) return true;
    if (top.pick == Pick::LengthGen && redo_length(machine)) return true;
    if (top.pick != Pick::Clauses && top.pick != Pick::Retract && top.pick != Pick::ClauseScan &&
        top.pick != Pick::PredGen && top.pick != Pick::ElseGoal && top.pick != Pick::Negation &&
        top.pick != Pick::Repeat && top.pick != Pick::AtomGen && top.pick != Pick::StreamGen &&
        top.pick != Pick::LengthGen && top.pick != Pick::CallMod) {
      machine.stack.pop_back();
    }
  }
  return false;
}

}
