#include "internal.hpp"

#include <array>
#include <bit>
#include <cstdint>

namespace mosa {
namespace {

const std::uint64_t* watermark(const Impl& machine) {
  if (machine.stack.empty()) return machine.heap.data();
  return machine.heap.data() + machine.stack.back().heap_mark;
}

// まだ書いた通りの組だけを戻す。あとから define が優先度を替えていたら、その指示を残す。
// 固有層の優先度 0 は消去ではなく印なので、0 以外と同じく「書いた値なら戻す」で扱う。
void undo_touch(OperatorTable& table, const OpTouch& touch, bool local) {
  if (touch.priority == 0 && !local) {
    if (!touch.had || table.stored(touch.name, touch.specifier)) return;
    table.put_stored(touch.old_priority, touch.old_specifier, touch.name);
    return;
  }
  const auto current = table.stored(touch.name, touch.specifier);
  if (!current || current->priority != touch.priority) return;
  table.erase_stored(touch.name, touch.specifier);
  if (touch.had) table.put_stored(touch.old_priority, touch.old_specifier, touch.name);
}

OperatorTable* op_layer(Impl& machine, std::uint32_t module) {
  if (module == machine.user_module) return &machine.operators;
  Module* found = find_module(machine, module);
  return found == nullptr ? nullptr : &found->ops;
}

}  // namespace

void restore_state(Impl& machine, std::size_t heap_mark, std::size_t trail_mark, std::size_t extras_mark) {
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
  note_catches(machine, entry);
  machine.stack.push_back(std::move(entry));
}

void trail_operators(Impl& machine, std::uint32_t module, int priority, Specifier specifier,
                     const std::vector<std::string>& names) {
  if (machine.stack.empty()) return;
  const auto effect_id = machine.stack.back().effect_id;
  if (effect_id == 0) return;
  const OperatorTable* table = op_layer(machine, module);
  if (table == nullptr) return;
  const bool local = module != machine.user_module;
  OpUndo undo;
  undo.effect_id = effect_id;
  undo.module = module;
  for (const std::string& name : names) {
    OpTouch touch;
    touch.name = name;
    touch.priority = priority;
    touch.specifier = specifier;
    const auto old = priority == 0 && !local ? table->stored(name, specifier) : table->stored_class(name, specifier);
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
    OperatorTable* table = op_layer(machine, undo.module);
    if (table == nullptr) continue;
    const bool local = undo.module != machine.user_module;
    for (auto it = undo.touches.rbegin(); it != undo.touches.rend(); ++it) undo_touch(*table, *it, local);
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

void enter_clause(Impl& machine, const Predicate& predicate, std::size_t which, std::uint64_t goal, std::size_t cut) {
  const Clause& clause = predicate.clauses[which];
  // meta 引数の限定は呼び手の文脈。枠は書き換えず、節に入るたびに作る。やり直しで枠より新しい項を指さないため。
  const auto context = predicate.meta.empty() ? 0 : read_goal(machine, goal).module;
  std::vector<Cell> env;
  env.reserve(clause.vars);
  for (std::uint16_t index = 0; index < clause.vars; ++index) env.push_back(machine.heap.fresh_ref());
  if (clause.head.tag == IrTag::Fun) {
    for (std::size_t index = 0; index < clause.head.args.size(); ++index) {
      const Cell expected = materialize(machine, clause.head.args[index], env);
      Cell actual = Cell{machine.heap.word_at(goal + kFrameHead + index)};
      if (!predicate.meta.empty()) actual = meta_argument(machine, predicate, context, index, actual);
      if (!do_unify(machine, expected, actual)) {
        machine.failed = true;
        return;
      }
    }
  }
  const auto cont = machine.heap.word_at(goal + 3);
  // 本体のゴールは節の述語のモジュールで動く（ADR 0006）。呼び手の文脈は受け継がない。
  launch(machine, clause.body, cont, cut, env, predicate.module);
}

void enter_user(Impl& machine, const Goal& goal) {
  Predicate* predicate = resolve_pred(machine, goal.module, goal.name, goal.arity);
  if (predicate == nullptr) {
    // 旗 unknown=fail は、無い述語を失敗にする。error のときは従来の存在エラー。
    if (machine.unknown_fail) {
      machine.failed = true;
      return;
    }
    Cell pi = cell_functor(machine, goal.name, goal.arity);
    // user の外では、どこで引けなかったかを示す（iso-notes 第5節の決定 A）。
    if (goal.module != machine.user_module) {
      pi = machine.heap.structure(machine.atoms.intern(":"), std::array<Cell, 2>{machine.heap.atom(goal.module), pi});
    }
    raise_formal(machine, formal_existence(machine, "procedure", pi), goal.name, goal.arity);
    return;
  }
  if (predicate->clauses.empty()) {
    // 節を消し切っても動的述語は残る。呼び出しは失敗であり、存在エラーではない。
    machine.failed = true;
    return;
  }
  const bool has_arg = goal.arity >= 1 && !wraps_first(*predicate);
  const Cell first = has_arg ? machine.heap.deref(goal.arg(machine, 0)) : Cell{};
  const auto candidates = select_clauses(machine, *predicate, has_arg, first);
  if (candidates.empty()) {
    machine.failed = true;
    return;
  }
  const auto boundary = machine.stack.size();
  if (candidates.size() > 1) {
    Entry entry;
    entry.pick = Pick::Clauses;
    entry.candidates = candidates;
    entry.next = 1;
    entry.redo_goal = goal.at;
    // 別名で呼んでも、やり直しは定義の名前で引き直す。
    entry.pred_module = predicate->module;
    entry.pred_name = predicate->name;
    entry.pred_arity = predicate->arity;
    entry.cut = boundary;
    entry.heap_mark = machine.heap.size();
    entry.trail_mark = machine.trail.slots.size();
    push_frame(machine, std::move(entry));
  }
  enter_clause(machine, *predicate, candidates[0], goal.at, boundary);
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
  const Predicate* predicate = find_pred(machine, PredKey{top.pred_module, name, arity});
  // 述語ごと消えると添字が外れる。枠を残すとやり直しが終わらない。
  if (predicate == nullptr || index >= predicate->clauses.size()) {
    machine.stack.pop_back();
    return false;
  }
  if (top.next + 1 >= top.candidates.size()) machine.stack.pop_back();
  else machine.stack.back().next += 1;
  enter_clause(machine, *predicate, index, goal, cut);
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
  const auto module = top.module;
  machine.stack.pop_back();
  launch(machine, goal, cont, cut, env, module);
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
    if (top.pick != Pick::Clauses && top.pick != Pick::Retract && top.pick != Pick::ClauseScan &&
        top.pick != Pick::PredGen && top.pick != Pick::ElseGoal && top.pick != Pick::Negation &&
        top.pick != Pick::Repeat && top.pick != Pick::AtomGen && top.pick != Pick::StreamGen) {
      machine.stack.pop_back();
    }
  }
  return false;
}

}
