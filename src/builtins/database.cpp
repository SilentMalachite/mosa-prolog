// 8.9.1 asserta/1 (+clause)
// 8.9.2 assertz/1 (+clause)
//   instantiation_error, type_error(callable), permission_error(modify, static_procedure, PI)
// 8.9.3 retract/1 (+clause)
//   上と同じ誤り。解は節の順で、消した節は失敗しても戻さない。
// 8.9.4 abolish/1 (+predicate_indicator)
//   instantiation_error, type_error(predicate_indicator), type_error(atom), type_error(integer),
//   domain_error(not_less_than_zero), representation_error(max_arity),
//   permission_error(modify, static_procedure, PI)
// 8.9.5 retractall/1 (@callable)（Cor.2）
//   instantiation_error, type_error(callable), permission_error(modify, static_procedure, PI)
// 7.4.2 dynamic/1 は指示だけ。読み込み側で受ける。
// テスト: tests/pl/database.pl

#include "../machine/internal.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct HeadName {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

struct ClauseView {
  Cell head{};
  Cell body{};
  bool rule = false;
};

bool callable_term(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Atom || cell.tag() == Tag::Str || cell.tag() == Tag::Lis;
}

void raise_instantiation(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

void raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
}

void raise_permission(Impl& machine, const Goal& goal, std::uint32_t name, std::uint16_t arity) {
  const Cell indicator = cell_functor(machine, name, arity);
  raise_formal(machine, formal_permission(machine, "modify", "static_procedure", indicator), goal.name,
               goal.arity);
}

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

Predicate* find_predicate(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  for (Predicate& predicate : machine.predicates) {
    if (predicate.name == name && predicate.arity == arity) return &predicate;
  }
  return nullptr;
}

HeadName head_indicator(Impl& machine, Cell head) {
  head = machine.heap.deref(head);
  HeadName indicator;
  if (head.tag() == Tag::Atom) {
    indicator.name = static_cast<std::uint32_t>(head.raw() >> 3);
    return indicator;
  }
  if (head.tag() == Tag::Lis) {
    indicator.name = machine.atoms.intern(".");
    indicator.arity = 2;
    return indicator;
  }
  indicator.name = machine.heap.functor_atom(head);
  indicator.arity = machine.heap.functor_arity(head);
  return indicator;
}

ClauseView clause_view(Impl& machine, Cell term) {
  ClauseView view;
  term = machine.heap.deref(term);
  if (term.tag() == Tag::Str && machine.heap.functor_arity(term) == 2 &&
      machine.atoms.name(machine.heap.functor_atom(term)) == ":-") {
    view.rule = true;
    view.head = machine.heap.deref(machine.heap.arg(term, 0));
    view.body = machine.heap.deref(machine.heap.arg(term, 1));
    return view;
  }
  view.head = term;
  return view;
}

bool reject_clause(Impl& machine, const Goal& goal, Cell term) {
  if (machine.heap.unbound(term)) {
    raise_instantiation(machine, goal);
    return true;
  }
  const ClauseView view = clause_view(machine, term);
  if (machine.heap.unbound(view.head)) {
    raise_instantiation(machine, goal);
    return true;
  }
  if (!callable_term(machine, view.head)) {
    raise_type(machine, goal, "callable", view.head);
    return true;
  }
  if (!view.rule) return false;
  if (machine.heap.unbound(view.body)) {
    raise_instantiation(machine, goal);
    return true;
  }
  if (!callable_term(machine, view.body)) {
    raise_type(machine, goal, "callable", view.body);
    return true;
  }
  return false;
}

void note_removed(Impl& machine, std::uint32_t name, std::uint16_t arity, std::size_t removed) {
  for (Entry& entry : machine.stack) {
    if (entry.pred_name != name || entry.pred_arity != arity) continue;
    if (entry.pick != Pick::Clauses && entry.pick != Pick::Retract) continue;
    std::vector<std::size_t> kept;
    kept.reserve(entry.candidates.size());
    std::size_t next = entry.next;
    for (std::size_t cursor = 0; cursor < entry.candidates.size(); ++cursor) {
      auto index = entry.candidates[cursor];
      if (index == removed) {
        // 消費済みの枠を落とすと next が一つ戻る。未消費なら次の枠が同じ位置に残る。
        if (cursor < next) --next;
        continue;
      }
      if (index > removed) --index;
      kept.push_back(index);
    }
    entry.candidates = std::move(kept);
    entry.next = next;
  }
}

void note_asserta(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  for (Entry& entry : machine.stack) {
    if (entry.pred_name != name || entry.pred_arity != arity) continue;
    if (entry.pick != Pick::Clauses && entry.pick != Pick::Retract) continue;
    // 先頭に足しても、開いている走査は新しい節を見ない。古い添字だけ 1 つずれる。
    for (std::size_t& index : entry.candidates) ++index;
  }
}

void erase_clause(Predicate& predicate, std::size_t index) {
  predicate.clauses.erase(predicate.clauses.begin() + static_cast<std::ptrdiff_t>(index));
}

void erase_predicate(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  for (Entry& entry : machine.stack) {
    if (entry.pred_name != name || entry.pred_arity != arity) continue;
    if (entry.pick != Pick::Clauses && entry.pick != Pick::Retract) continue;
    entry.candidates.clear();
    entry.next = 0;
  }
  for (std::size_t index = 0; index < machine.predicates.size(); ++index) {
    const Predicate& predicate = machine.predicates[index];
    if (predicate.name != name || predicate.arity != arity) continue;
    machine.predicates.erase(machine.predicates.begin() + static_cast<std::ptrdiff_t>(index));
    return;
  }
}

bool unify_stored(Impl& machine, const Clause& clause, const ClauseView& view) {
  std::vector<Cell> env;
  env.reserve(clause.vars);
  for (std::uint16_t index = 0; index < clause.vars; ++index) env.push_back(machine.heap.fresh_ref());
  if (!do_unify(machine, view.head, materialize(machine, clause.head, env))) return false;
  if (!view.rule) return true;
  return do_unify(machine, view.body, materialize(machine, clause.body, env));
}

std::vector<std::size_t> collect_matches(Impl& machine, const Predicate& predicate, const ClauseView& view) {
  std::vector<std::size_t> found;
  for (std::size_t index = 0; index < predicate.clauses.size(); ++index) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const bool matched = unify_stored(machine, predicate.clauses[index], view);
    if (matched) found.push_back(index);
    restore_state(machine, heap_mark, trail_mark, extras_mark);
  }
  return found;
}

void push_retract(Impl& machine, const Goal& goal, std::uint32_t name, std::uint16_t arity,
                  const std::vector<std::size_t>& matches, std::size_t next, std::size_t heap_mark,
                  std::size_t trail_mark) {
  Entry entry;
  entry.pick = Pick::Retract;
  entry.candidates = matches;
  entry.next = next;
  entry.redo_goal = goal.at;
  entry.pred_name = name;
  entry.pred_arity = arity;
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

bool take_retract(Impl& machine, const Goal& goal, std::uint32_t name, std::uint16_t arity,
                  const std::vector<std::size_t>& matches, std::size_t begin) {
  Predicate* predicate = find_predicate(machine, name, arity);
  if (predicate == nullptr) return false;
  const ClauseView view = clause_view(machine, goal.arg(machine, 0));
  for (std::size_t cursor = begin; cursor < matches.size(); ++cursor) {
    const auto index = matches[cursor];
    if (index >= predicate->clauses.size()) continue;
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    if (!unify_stored(machine, predicate->clauses[index], view)) {
      restore_state(machine, heap_mark, trail_mark, extras_mark);
      continue;
    }
    if (cursor + 1 < matches.size()) {
      push_retract(machine, goal, name, arity, matches, cursor + 1, heap_mark, trail_mark);
    }
    erase_clause(*predicate, index);
    note_removed(machine, name, arity, index);
    succeed(machine, goal);
    return true;
  }
  return false;
}

void run_assert(Impl& machine, const Goal& goal, bool at_front) {
  const Cell term = machine.heap.deref(goal.arg(machine, 0));
  if (reject_clause(machine, goal, term)) return;
  const ClauseView view = clause_view(machine, term);
  const HeadName head = head_indicator(machine, view.head);
  Predicate* existing = find_predicate(machine, head.name, head.arity);
  if (existing != nullptr && !existing->dynamic) {
    raise_permission(machine, goal, head.name, head.arity);
    return;
  }
  Clause clause;
  // 呼び出し後の束縛が節へ漏れないよう、データベースに見える前に IR の変数番号へ切る。
  if (!compile_clause(machine.heap, term, clause)) {
    raise_type(machine, goal, "callable", view.head);
    return;
  }
  if (existing == nullptr) {
    Predicate created;
    created.name = head.name;
    created.arity = head.arity;
    created.dynamic = true;
    machine.predicates.push_back(std::move(created));
    existing = &machine.predicates.back();
  }
  if (at_front) {
    existing->clauses.insert(existing->clauses.begin(), std::move(clause));
    note_asserta(machine, head.name, head.arity);
  } else {
    existing->clauses.push_back(std::move(clause));
  }
  succeed(machine, goal);
}

void run_retract(Impl& machine, const Goal& goal) {
  const Cell term = machine.heap.deref(goal.arg(machine, 0));
  if (reject_clause(machine, goal, term)) return;
  const ClauseView view = clause_view(machine, term);
  const HeadName head = head_indicator(machine, view.head);
  Predicate* predicate = find_predicate(machine, head.name, head.arity);
  if (predicate == nullptr) {
    machine.failed = true;
    return;
  }
  if (!predicate->dynamic) {
    raise_permission(machine, goal, head.name, head.arity);
    return;
  }
  const auto matches = collect_matches(machine, *predicate, view);
  if (!take_retract(machine, goal, head.name, head.arity, matches, 0)) machine.failed = true;
}

void run_retractall(Impl& machine, const Goal& goal) {
  const Cell head_cell = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(head_cell)) {
    raise_instantiation(machine, goal);
    return;
  }
  if (!callable_term(machine, head_cell)) {
    raise_type(machine, goal, "callable", head_cell);
    return;
  }
  const HeadName head = head_indicator(machine, head_cell);
  Predicate* predicate = find_predicate(machine, head.name, head.arity);
  if (predicate == nullptr) {
    succeed(machine, goal);
    return;
  }
  if (!predicate->dynamic) {
    raise_permission(machine, goal, head.name, head.arity);
    return;
  }
  ClauseView view;
  view.head = head_cell;
  const auto matches = collect_matches(machine, *predicate, view);
  for (std::size_t cursor = matches.size(); cursor > 0; --cursor) {
    const auto index = matches[cursor - 1];
    Predicate* current = find_predicate(machine, head.name, head.arity);
    if (current == nullptr || index >= current->clauses.size()) continue;
    erase_clause(*current, index);
    note_removed(machine, head.name, head.arity, index);
  }
  succeed(machine, goal);
}

bool parse_indicator(Impl& machine, const Goal& goal, Cell pred, HeadName& indicator) {
  pred = machine.heap.deref(pred);
  if (machine.heap.unbound(pred)) {
    raise_instantiation(machine, goal);
    return false;
  }
  const bool shaped = pred.tag() == Tag::Str && machine.heap.functor_arity(pred) == 2 &&
                      machine.atoms.name(machine.heap.functor_atom(pred)) == "/";
  if (!shaped) {
    raise_type(machine, goal, "predicate_indicator", pred);
    return false;
  }
  const Cell name = machine.heap.deref(machine.heap.arg(pred, 0));
  const Cell arity = machine.heap.deref(machine.heap.arg(pred, 1));
  if (machine.heap.unbound(name) || machine.heap.unbound(arity)) {
    raise_instantiation(machine, goal);
    return false;
  }
  if (name.tag() != Tag::Atom) {
    raise_type(machine, goal, "atom", name);
    return false;
  }
  if (arity.tag() != Tag::Int && arity.tag() != Tag::IntBox) {
    raise_type(machine, goal, "integer", arity);
    return false;
  }
  const auto value = machine.heap.integer_value(arity);
  if (value < 0) {
    raise_formal(machine, formal_domain(machine, "not_less_than_zero", arity), goal.name, goal.arity);
    return false;
  }
  if (value > 1024) {
    raise_formal(machine, formal_representation(machine, "max_arity"), goal.name, goal.arity);
    return false;
  }
  indicator.name = static_cast<std::uint32_t>(name.raw() >> 3);
  indicator.arity = static_cast<std::uint16_t>(value);
  return true;
}

void run_abolish(Impl& machine, const Goal& goal) {
  HeadName indicator;
  if (!parse_indicator(machine, goal, goal.arg(machine, 0), indicator)) return;
  Predicate* predicate = find_predicate(machine, indicator.name, indicator.arity);
  if (predicate != nullptr && !predicate->dynamic) {
    raise_permission(machine, goal, indicator.name, indicator.arity);
    return;
  }
  if (predicate != nullptr) erase_predicate(machine, indicator.name, indicator.arity);
  succeed(machine, goal);
}

}  // namespace

bool redo_retract(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::Retract) return false;
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  machine.stack.pop_back();
  return take_retract(machine, read_goal(machine, top.redo_goal), top.pred_name, top.pred_arity, top.candidates,
                       top.next);
}

bool run_database(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (goal.arity != 1) return false;
  if (name == "asserta") {
    run_assert(machine, goal, true);
    return true;
  }
  if (name == "assertz") {
    run_assert(machine, goal, false);
    return true;
  }
  if (name == "retract") {
    run_retract(machine, goal);
    return true;
  }
  if (name == "retractall") {
    run_retractall(machine, goal);
    return true;
  }
  if (name == "abolish") {
    run_abolish(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
