#include "../machine/internal.hpp"

#include <array>
#include <vector>

namespace mosa {
namespace {

bool is_callable(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Atom || cell.tag() == Tag::Str || cell.tag() == Tag::Lis;
}

void instantiation(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

void not_callable(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_type(machine, "callable", culprit), goal.name, goal.arity);
}

bool is_qualified(const Impl& machine, Cell cell) {
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == ":";
}

// M:G の G。引数を足す相手であり、callable かどうかもこれで決まる。
Cell unqualified(const Impl& machine, Cell goal) {
  goal = machine.heap.deref(goal);
  while (is_qualified(machine, goal)) goal = machine.heap.deref(machine.heap.arg(goal, 1));
  return goal;
}

Cell appended(Impl& machine, Cell goal, const Goal& frame) {
  goal = machine.heap.deref(goal);
  // call(user:p, X) は user:p(X) である。文脈の切り替えは :/2 に任せ、ここでは G にだけ足す。
  if (is_qualified(machine, goal)) {
    const Cell inner = appended(machine, machine.heap.arg(goal, 1), frame);
    const std::array<Cell, 2> parts{machine.heap.arg(goal, 0), inner};
    return machine.heap.structure(machine.heap.functor_atom(goal), parts);
  }
  std::vector<Cell> args;
  std::uint32_t name = 0;
  if (goal.tag() == Tag::Atom) {
    name = static_cast<std::uint32_t>(goal.raw() >> 3);
  } else if (goal.tag() == Tag::Str) {
    name = machine.heap.functor_atom(goal);
    for (std::uint16_t index = 0; index < machine.heap.functor_arity(goal); ++index) {
      args.push_back(machine.heap.arg(goal, index));
    }
  } else if (goal.tag() == Tag::Lis) {
    name = machine.atoms.intern(".");
    args.push_back(machine.heap.list_head(goal));
    args.push_back(machine.heap.list_tail(goal));
  }
  for (std::uint16_t index = 1; index < frame.arity; ++index) args.push_back(frame.arg(machine, index));
  return machine.heap.structure(name, args);
}

bool run_call(Impl& machine, const Goal& goal) {
  const Cell head = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(head)) {
    instantiation(machine, goal);
    return true;
  }
  Cell callable = head;
  if (goal.arity > 1) {
    const Cell target = unqualified(machine, head);
    if (machine.heap.unbound(target)) {
      instantiation(machine, goal);
      return true;
    }
    if (target.tag() != Tag::Atom && target.tag() != Tag::Str && target.tag() != Tag::Lis) {
      not_callable(machine, goal, head);
      return true;
    }
    callable = appended(machine, head, goal);
  }
  if (!is_callable(machine, callable)) {
    not_callable(machine, goal, callable);
    return true;
  }
  const auto boundary = machine.stack.size();
  const Compiled compiled = compile_goal(machine.heap, callable);
  // (fail, 1) は複合項なので callable だが、ゴールには変換できない（7.8.3、Cor.2）。
  if (!body_callable(*compiled.term)) {
    not_callable(machine, goal, callable);
    return true;
  }
  launch(machine, compiled.term, goal.cont, boundary, compiled.env, goal.module);
  return true;
}

bool run_not(Impl& machine, const Goal& goal) {
  const Cell inner = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(inner)) {
    instantiation(machine, goal);
    return true;
  }
  if (!is_callable(machine, inner)) {
    not_callable(machine, goal, inner);
    return true;
  }
  Entry barrier;
  barrier.pick = Pick::Negation;
  barrier.cont = goal.cont;
  barrier.heap_mark = machine.heap.size();
  barrier.trail_mark = machine.trail.slots.size();
  push_frame(machine, barrier);
  const auto boundary = machine.stack.size();
  Extra done;
  done.ctrl = Ctrl::NegDone;
  done.trim_to = boundary - 1;
  machine.extras.push_back(std::move(done));
  const auto at = machine.heap.allocate_words(kFrameHead);
  machine.heap.set_word(at, 0);
  machine.heap.set_word(at + 1, machine.extras.size() - 1);
  machine.heap.set_word(at + 2, 0);
  machine.heap.set_word(at + 3, kNoFrame);
  const Compiled compiled = compile_goal(machine.heap, inner);
  launch(machine, compiled.term, at, boundary, compiled.env, goal.module);
  return true;
}

bool run_once(Impl& machine, const Goal& goal) {
  const Cell inner = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(inner)) {
    instantiation(machine, goal);
    return true;
  }
  if (!is_callable(machine, inner)) {
    not_callable(machine, goal, inner);
    return true;
  }
  Entry barrier;
  barrier.pick = Pick::Once;
  barrier.heap_mark = machine.heap.size();
  barrier.trail_mark = machine.trail.slots.size();
  push_frame(machine, barrier);
  const auto boundary = machine.stack.size();
  Extra done;
  done.ctrl = Ctrl::OnceDone;
  done.trim_to = boundary - 1;
  done.cont = goal.cont;
  machine.extras.push_back(std::move(done));
  const auto at = machine.heap.allocate_words(kFrameHead);
  machine.heap.set_word(at, 0);
  machine.heap.set_word(at + 1, machine.extras.size() - 1);
  machine.heap.set_word(at + 2, 0);
  machine.heap.set_word(at + 3, kNoFrame);
  const Compiled compiled = compile_goal(machine.heap, inner);
  launch(machine, compiled.term, at, boundary, compiled.env, goal.module);
  return true;
}

bool run_repeat(Impl& machine, const Goal& goal) {
  Entry again;
  again.pick = Pick::Repeat;
  again.cont = goal.cont;
  again.heap_mark = machine.heap.size();
  again.trail_mark = machine.trail.slots.size();
  push_frame(machine, again);
  machine.ip = goal.cont;
  return true;
}

bool run_halt(Impl& machine, const Goal& goal) {
  if (goal.arity == 0) {
    machine.halt_now = true;
    machine.halt_code = 0;
    machine.ip = kNoFrame;
    return true;
  }
  const Cell code = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(code)) {
    instantiation(machine, goal);
    return true;
  }
  if (code.tag() != Tag::Int && code.tag() != Tag::IntBox) {
    raise_formal(machine, formal_type(machine, "integer", code), goal.name, goal.arity);
    return true;
  }
  machine.halt_now = true;
  machine.halt_code = static_cast<int>(machine.heap.integer_value(code));
  machine.ip = kNoFrame;
  return true;
}

}  // namespace

// forall/2 は \+ (Cond, \+ Action) を呼び手の文脈で走らせる（iso-notes「Section 7 forall/2」の決定 A）。
// 前もって調べるのは Cond だけ。Action の誤りは、その \+ に届いたときに出る。
bool run_forall(Impl& machine, const Goal& goal) {
  std::array<Cell, 2> parts{machine.heap.deref(goal.arg(machine, 0)), machine.heap.deref(goal.arg(machine, 1))};
  if (machine.heap.unbound(parts[0])) {
    instantiation(machine, goal);
    return true;
  }
  if (!is_callable(machine, parts[0])) {
    not_callable(machine, goal, parts[0]);
    return true;
  }
  const auto negation = machine.atoms.intern("\\+");
  parts[1] = machine.heap.structure(negation, std::array<Cell, 1>{parts[1]});
  const Cell both = machine.heap.structure(machine.atoms.intern(","), parts);
  const Cell term = machine.heap.structure(negation, std::array<Cell, 1>{both});
  const Compiled compiled = compile_goal(machine.heap, term);
  if (!body_callable(*compiled.term)) {
    not_callable(machine, goal, both);
    return true;
  }
  launch(machine, compiled.term, goal.cont, machine.stack.size(), compiled.env, goal.module);
  return true;
}

// if/3 は (Cond *-> Then ; Else) を呼び手の文脈で走らせる（iso-notes「Section 3 if/3」の決定 B）。
// 述語なので call/1 と同じく切断は内側に閉じる。呼べない引数の誤りは、call/1 と同じく渡された項全体で出す。
bool run_if(Impl& machine, const Goal& goal) {
  const std::array<Cell, 3> parts{machine.heap.deref(goal.arg(machine, 0)), machine.heap.deref(goal.arg(machine, 1)),
                                  machine.heap.deref(goal.arg(machine, 2))};
  const Cell soft = machine.heap.structure(machine.atoms.intern("*->"), std::array<Cell, 2>{parts[0], parts[1]});
  const Cell term = machine.heap.structure(machine.atoms.intern(";"), std::array<Cell, 2>{soft, parts[2]});
  const Compiled compiled = compile_goal(machine.heap, term);
  if (!body_callable(*compiled.term)) {
    not_callable(machine, goal, machine.heap.structure(goal.name, parts));
    return true;
  }
  launch(machine, compiled.term, goal.cont, machine.stack.size(), compiled.env, goal.module);
  return true;
}

bool run_control(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "call" && goal.arity >= 1 && goal.arity <= 8) return run_call(machine, goal);
  if (name == "\\+" && goal.arity == 1) return run_not(machine, goal);
  if (name == "once" && goal.arity == 1) return run_once(machine, goal);
  if (name == "repeat" && goal.arity == 0) return run_repeat(machine, goal);
  if (name == "forall" && goal.arity == 2) return run_forall(machine, goal);
  if (name == "if" && goal.arity == 3) return run_if(machine, goal);
  if (name == "halt" && goal.arity <= 1) return run_halt(machine, goal);
  return false;
}

}
