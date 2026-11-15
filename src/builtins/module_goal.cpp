// モジュールを見るゴールの組み込み（SPEC 第5節、フェーズ12）。13211-1 に条項は無い。13211-2 での位置は未確認。
// strip_module(?Term, ?Module, ?Plain)
//   Term の頭の M: を、M が原子である間だけ外す。Module は一番内側のその M、M: が無ければ文脈モジュール。Plain は残り。
//   誤りは投げない（SWI-Prolog と同じ）。
// context_module(?M)
//   文脈モジュール（ゴール枠のモジュール）。誤りは投げない。
// どれも決定的。
// テスト: tests/pl/modules.pl（tests/pl/modules/ctx.pl）
#include "../machine/internal.hpp"

#include <cstdint>

namespace mosa {

namespace {

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

// M が変数や数の M:T は外さず、全体を Plain にする。モジュールがあるかどうかは見ない（:/2 と違い、呼ばないので）。
bool atom_qualified(const Impl& machine, Cell term) {
  if (term.tag() != Tag::Str || machine.heap.functor_arity(term) != 2) return false;
  if (machine.atoms.name(machine.heap.functor_atom(term)) != ":") return false;
  return machine.heap.deref(machine.heap.arg(term, 0)).tag() == Tag::Atom;
}

void finish(Impl& machine, const Goal& goal, bool unified) {
  if (unified) machine.ip = goal.cont;
  else machine.failed = true;
}

// 外側から順に外すので、重なった M: は一番内側が残る。束ねるのは外し終えてから。
void prove_strip_module(Impl& machine, const Goal& goal) {
  Cell term = machine.heap.deref(goal.arg(machine, 0));
  std::uint32_t module = goal.module;
  while (atom_qualified(machine, term)) {
    module = atom_id(machine.heap.deref(machine.heap.arg(term, 0)));
    term = machine.heap.deref(machine.heap.arg(term, 1));
  }
  finish(machine, goal,
         do_unify(machine, goal.arg(machine, 1), machine.heap.atom(module)) &&
             do_unify(machine, goal.arg(machine, 2), term));
}

// 枠のモジュールは、節の本体ならその節のモジュール、M:G なら M、meta_predicate の引数なら呼び手が包んだモジュール。
void prove_context_module(Impl& machine, const Goal& goal) {
  finish(machine, goal, do_unify(machine, goal.arg(machine, 0), machine.heap.atom(goal.module)));
}

}  // namespace

bool run_module_goal(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "strip_module" && goal.arity == 3) {
    prove_strip_module(machine, goal);
    return true;
  }
  if (name == "context_module" && goal.arity == 1) {
    prove_context_module(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
