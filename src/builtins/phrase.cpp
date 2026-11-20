// phrase/2 (+callable_term, ?list)、phrase/3 (+callable_term, ?list, ?list)。13211-3 の文法規則の部（本文は
// 手元に無い。iso-notes「13211-3」）。ADR 0024。本体を文法規則の本体として訳し、call/1 と同じく呼ぶ。
//   instantiation_error, type_error(callable, GRBody), type_error(list, S0), type_error(list, S)、
//   GRBody の M: の層か訳した本体の制御構造が巡回すれば resource_error(cyclic_term)（iso-notes 7.3.3）、
//   非終端記号に S0 と S を足すと項数が 1024 を超えれば representation_error(max_arity)（7.11.2.1、レビュー H-11）
// テスト: tests/pl/dcg.pl

#include "../machine/dcg.hpp"
#include "../machine/internal.hpp"

#include <optional>

namespace mosa {
namespace {

// M:G の G。呼べるかどうかはこれで決まる。M: の層が巡回すれば nullopt（iso-notes 7.3.3、2026-11-17 の持ち主の
// 決定 A）。前は回り続けた。
std::optional<Cell> unqualified(const Impl& machine, Cell body) {
  body = machine.heap.deref(body);
  LayerWatch watch(body);
  while (body.tag() == Tag::Str && machine.heap.functor_arity(body) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(body)) == ":") {
    body = machine.heap.deref(machine.heap.arg(body, 1));
    if (watch.returned_to(body)) return std::nullopt;
  }
  return body;
}

bool raised(Impl& machine, const Goal& goal, Cell formal) {
  raise_formal(machine, formal, goal.name, goal.arity);
  return true;
}

// 部分リストと変数はよい。何を読み、何を残すかは本体が決める。
bool bad_list(const Impl& machine, Cell list) { return walk_list(machine, list).shape == ListShape::Bad; }

}  // namespace

bool run_phrase(Impl& machine, const Goal& goal) {
  const Cell body = machine.heap.deref(goal.arg(machine, 0));
  const auto bare = unqualified(machine, body);
  if (!bare) return raised(machine, goal, formal_resource(machine, "cyclic_term"));
  const Cell target = *bare;
  if (machine.heap.unbound(target)) return raised(machine, goal, formal_instantiation(machine));
  if (target.tag() != Tag::Atom && target.tag() != Tag::Str && target.tag() != Tag::Lis) {
    return raised(machine, goal, formal_type(machine, "callable", body));
  }
  const Cell s0 = machine.heap.deref(goal.arg(machine, 1));
  const Cell s = goal.arity == 3 ? machine.heap.deref(goal.arg(machine, 2))
                                 : machine.heap.atom(machine.atoms.intern("[]"));
  if (bad_list(machine, s0)) return raised(machine, goal, formal_type(machine, "list", s0));
  if (bad_list(machine, s)) return raised(machine, goal, formal_type(machine, "list", s));
  const DcgOut translated = dcg_body(machine, body, s0, s);
  if (translated.formal) {
    // 呼べない部分があれば、call/1 と同じく渡された本体全体を Culprit にする（7.8.3.3）。
    if (translated.not_callable) return raised(machine, goal, formal_type(machine, "callable", body));
    return raised(machine, goal, *translated.formal);
  }
  // 巡回する本体は、前は戻り辺の phrase/3 が動くたびに展開して訳した（ADR 0024）。今は何も動かす前に止める。
  if (translated.cyclic) return raised(machine, goal, formal_resource(machine, "cyclic_term"));
  const auto boundary = machine.stack.size();
  const Compiled compiled = compile_goal(machine.heap, translated.term);
  if (!body_callable(*compiled.term)) return raised(machine, goal, formal_type(machine, "callable", body));
  if (raise_cyclic_goal(machine, compiled, goal.name, goal.arity)) return true;
  // 床を boundary に置くので、本体の中のカットは phrase に閉じる。
  launch(machine, compiled.term, goal.cont, boundary, compiled.env, goal.module);
  return true;
}

}  // namespace mosa
