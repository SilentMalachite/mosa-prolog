// 7.8.3 call/1 (+callable_term)、8.15.4 call/2..8 (+callable_term, ?term, ...)
//   instantiation_error, type_error(callable, Goal)
// 8.15.1 (\+)/1 (+callable_term)、8.15.2 once/1 (+callable_term)
//   instantiation_error, type_error(callable, Goal)
// 8.15.3 repeat/0
// 8.17.3 halt/0、8.17.4 halt/1 (+integer)
//   instantiation_error, type_error(integer, Code)、representation_error(max_integer | min_integer)（Code が C++ の
//   int の外。Big も int の外の int64 も同じ。iso-notes「8.17.4 halt/1」の決定 A。halt_code_problem は IR の
//   halt/1（catch.cpp の arm_halt）と共に使う）
// forall/2 (+callable_term, +callable_term) と if/3 (+callable_term, +callable_term, +callable_term) は非 ISO
//   instantiation_error, type_error(callable, Goal)
// phrase/2,3 は phrase.cpp（ここは振り分けだけ）
// テスト: tests/pl/control.pl、tests/pl/bigint.pl（halt_*）

#include "../machine/internal.hpp"

#include <array>
#include <limits>
#include <optional>
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

Cell with_args(Impl& machine, Cell goal, const Goal& frame) {
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

// call(user:p, X) は user:p(X) である。文脈の切り替えは :/2 に任せ、ここでは G にだけ足す。
// :/2 の段ごとに再帰すると、実行時に組み立てた深い M:…:G で C++ のスタックが尽きる（iso-notes 7.1）。
// 段を外側から積み、内側から組み直す。割り当ての順は前の再帰と同じ。
Cell appended(Impl& machine, Cell goal, const Goal& frame) {
  std::vector<Cell> layers;
  goal = machine.heap.deref(goal);
  while (is_qualified(machine, goal)) {
    layers.push_back(goal);
    goal = machine.heap.deref(machine.heap.arg(goal, 1));
  }
  Cell built = with_args(machine, goal, frame);
  for (auto layer = layers.rbegin(); layer != layers.rend(); ++layer) {
    const std::array<Cell, 2> parts{machine.heap.arg(*layer, 0), built};
    built = machine.heap.structure(machine.heap.functor_atom(*layer), parts);
  }
  return built;
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

// \+ G と once(G) の G は call/1 として動く（8.15.1.1、8.15.2.1）。(write(a), 1) のように呼べない部分があれば、
// 床を積んで何かを動かす前に type_error(callable, Culprit) にする（7.8.3.3、レビュー M16）。誤りを出したら false。
bool called_goal(Impl& machine, const Goal& goal, Cell inner, Cell culprit, Compiled& out) {
  if (machine.heap.unbound(inner)) {
    instantiation(machine, goal);
    return false;
  }
  if (!is_callable(machine, inner)) {
    not_callable(machine, goal, culprit);
    return false;
  }
  out = compile_goal(machine.heap, inner);
  if (!body_callable(*out.term)) {
    not_callable(machine, goal, culprit);
    return false;
  }
  return true;
}

// 訳して調べた G を \+ として動かす。G が成功すれば床ごと切って失敗し（Ctrl::NegDone）、失敗すれば続きへ進む。
void launch_negation(Impl& machine, const Goal& goal, const Compiled& compiled) {
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
  const auto at = push_extra(machine, std::move(done));
  launch(machine, compiled.term, at, boundary, compiled.env, goal.module);
}

bool run_not(Impl& machine, const Goal& goal) {
  const Cell inner = machine.heap.deref(goal.arg(machine, 0));
  Compiled compiled;
  if (called_goal(machine, goal, inner, inner, compiled)) launch_negation(machine, goal, compiled);
  return true;
}

bool run_once(Impl& machine, const Goal& goal) {
  const Cell inner = machine.heap.deref(goal.arg(machine, 0));
  Compiled compiled;
  if (!called_goal(machine, goal, inner, inner, compiled)) return true;
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
  const auto at = push_extra(machine, std::move(done));
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
  int status = 0;
  if (const auto problem = halt_code_problem(machine, code, status)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return true;
  }
  machine.halt_now = true;
  machine.halt_code = status;
  machine.ip = kNoFrame;
  return true;
}

}  // namespace

// 終了状態を OS に渡すのは C++ の int なので、その外の整数は切り詰めずに表現の誤りにする。7.12.2 g の Flag で、
// 正の側は max_integer、負の側は min_integer（int の上限と下限の意味で使う）。Big は符号だけで側が決まる。
std::optional<Cell> halt_code_problem(Impl& machine, Cell code, int& status) {
  if (!is_integer_tag(code.tag())) return formal_type(machine, "integer", code);
  const bool big = code.tag() == Tag::Big;
  const bool negative = integer_negative(machine, code);
  if (big || machine.heap.integer_value(code) > std::numeric_limits<int>::max() ||
      machine.heap.integer_value(code) < std::numeric_limits<int>::min()) {
    return formal_representation(machine, negative ? "min_integer" : "max_integer");
  }
  status = static_cast<int>(machine.heap.integer_value(code));
  return std::nullopt;
}

// forall/2 は \+ (Cond, \+ Action) を呼び手の文脈で走らせる（iso-notes「Section 7 forall/2」の決定 A）。
// (Cond, \+ Action) を一度だけ訳して調べる。\+ Action は呼べる葉なので、呼べない部分があればそれは Cond にあり、
// 誤りは Cond で出す。Action の誤りは、その \+ に届いたときに \+ が出す（レビュー M16）。
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
  Compiled compiled;
  if (called_goal(machine, goal, both, parts[0], compiled)) launch_negation(machine, goal, compiled);
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
  if (name == "phrase" && (goal.arity == 2 || goal.arity == 3)) return run_phrase(machine, goal);
  if (name == "subsumes_term" && goal.arity == 2) return run_subsumes_term(machine, goal);
  if (name == "$dcg_translate" && goal.arity == 2) return run_dcg_translate(machine, goal);
  if (name == "halt" && goal.arity <= 1) return run_halt(machine, goal);
  return false;
}

}
