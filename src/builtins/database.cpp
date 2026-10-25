// 8.9 asserta/1 assertz/1 retract/1 abolish/1 retractall/1。7.4.2 dynamic/1。
// 未決 5 の clause/2 と未決 6 の current_predicate/1 は登録しない。
// asserta(+clause) assertz(+clause) retract(+clause)
//   instantiation_error, type_error(callable), permission_error(modify, static_procedure, PI)
// abolish(+predicate_indicator) retractall(+term)
//   instantiation_error, type_error(predicate_indicator), permission_error(modify, static_procedure, PI)
// dynamic(+predicate_indicator)
// テスト: tests/pl/database.pl

#include "../machine/internal.hpp"

#include "mosa/writer.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {
namespace {

constexpr std::uint8_t kRetractGen = 4;
constexpr std::uint16_t kMaxArity = 1024;

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

std::uint16_t clause_arity(const Clause& clause) {
  if (clause.head.tag == IrTag::Atom) return 0;
  return clause.head.arity;
}

void instantiation(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

void type_at(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
}

void permission_static(Impl& machine, const Goal& goal, std::uint32_t name, std::uint16_t arity) {
  const Cell pi = cell_functor(machine, name, arity);
  raise_formal(machine, formal_permission(machine, "modify", "static_procedure", pi), goal.name, goal.arity);
}

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

bool is_callable(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Atom || cell.tag() == Tag::Str || cell.tag() == Tag::Lis;
}

bool is_rule(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == ":-";
}

Predicate* find_pred(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  for (Predicate& predicate : machine.predicates) {
    if (predicate.name == name && predicate.arity == arity) return &predicate;
  }
  return nullptr;
}

bool can_modify(const Impl& machine, std::uint32_t name, std::uint16_t arity) {
  const Predicate* predicate = nullptr;
  for (const Predicate& item : machine.predicates) {
    if (item.name == name && item.arity == arity) predicate = &item;
  }
  return predicate != nullptr && predicate->dynamic;
}

void shift_front(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  for (Entry& entry : machine.stack) {
    if (entry.pred_name != name || entry.pred_arity != arity) continue;
    for (std::size_t& index : entry.candidates) ++index;
  }
}

void shift_removed(Impl& machine, std::uint32_t name, std::uint16_t arity, std::size_t removed) {
  for (Entry& entry : machine.stack) {
    if (entry.pred_name != name || entry.pred_arity != arity) continue;
    std::vector<std::size_t> kept;
    std::size_t dropped = 0;
    for (std::size_t pos = 0; pos < entry.candidates.size(); ++pos) {
      std::size_t index = entry.candidates[pos];
      if (index == removed) {
        if (pos < entry.next) ++dropped;
        continue;
      }
      if (index > removed) --index;
      kept.push_back(index);
    }
    entry.candidates = std::move(kept);
    if (entry.next >= dropped) entry.next -= dropped;
    else entry.next = 0;
  }
}

Cell clause_term(Impl& machine, const Clause& clause) {
  std::vector<Cell> env;
  env.reserve(clause.vars);
  for (std::uint16_t index = 0; index < clause.vars; ++index) env.push_back(machine.heap.fresh_ref());
  const Cell head = materialize(machine, clause.head, env);
  if (clause.body.tag == IrTag::True) return head;
  const Cell body = materialize(machine, clause.body, env);
  const std::array<Cell, 2> args{head, body};
  return machine.heap.structure(machine.atoms.intern(":-"), args);
}

bool prepare_clause(Impl& machine, const Goal& goal, Cell term, Clause& clause, std::uint32_t& name,
                    std::uint16_t& arity) {
  term = machine.heap.deref(term);
  if (machine.heap.unbound(term)) {
    instantiation(machine, goal);
    return false;
  }
  if (is_rule(machine, term)) {
    const Cell head = machine.heap.deref(machine.heap.arg(term, 0));
    const Cell body = machine.heap.deref(machine.heap.arg(term, 1));
    if (machine.heap.unbound(head) || machine.heap.unbound(body)) {
      instantiation(machine, goal);
      return false;
    }
    if (!is_callable(machine, head)) {
      type_at(machine, goal, "callable", head);
      return false;
    }
    if (!is_callable(machine, body)) {
      type_at(machine, goal, "callable", body);
      return false;
    }
  } else if (!is_callable(machine, term)) {
    type_at(machine, goal, "callable", term);
    return false;
  }
  if (!compile_clause(machine.heap, term, clause)) {
    type_at(machine, goal, "callable", term);
    return false;
  }
  name = clause.head.atom;
  arity = clause_arity(clause);
  if (!can_modify(machine, name, arity)) {
    permission_static(machine, goal, name, arity);
    return false;
  }
  return true;
}

void insert_clause(Impl& machine, std::uint32_t name, std::uint16_t arity, Clause clause, bool at_front) {
  Predicate* predicate = find_pred(machine, name, arity);
  if (predicate == nullptr) return;
  if (at_front) {
    predicate->clauses.insert(predicate->clauses.begin(), std::move(clause));
    shift_front(machine, name, arity);
    return;
  }
  predicate->clauses.push_back(std::move(clause));
}

void run_assert(Impl& machine, const Goal& goal, bool at_front) {
  Clause clause;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  if (!prepare_clause(machine, goal, goal.arg(machine, 0), clause, name, arity)) return;
  insert_clause(machine, name, arity, std::move(clause), at_front);
  succeed(machine, goal);
}

bool take_match(Impl& machine, const Goal& goal, Predicate& predicate, std::size_t index) {
  if (index >= predicate.clauses.size()) return false;
  const Cell expected = clause_term(machine, predicate.clauses[index]);
  if (!do_unify(machine, goal.arg(machine, 0), expected)) return false;
  const auto name = predicate.name;
  const auto arity = predicate.arity;
  predicate.clauses.erase(predicate.clauses.begin() + static_cast<std::ptrdiff_t>(index));
  shift_removed(machine, name, arity, index);
  return true;
}

bool prove_retract(Impl& machine, const Goal& goal) {
  while (!machine.stack.empty() && machine.stack.back().gen_kind == kRetractGen) {
    Entry& top = machine.stack.back();
    if (top.next >= top.candidates.size()) {
      machine.stack.pop_back();
      return false;
    }
    const auto pos = top.next;
    const auto index = top.candidates[pos];
    const auto name = top.pred_name;
    const auto arity = top.pred_arity;
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    Predicate* predicate = find_pred(machine, name, arity);
    const bool matched = predicate != nullptr && take_match(machine, goal, *predicate, index);
    if (matched) {
      succeed(machine, goal);
      return true;
    }
    restore_state(machine, heap_mark, trail_mark, extras_mark);
    if (pos < machine.stack.back().candidates.size() && machine.stack.back().next == pos) {
      machine.stack.back().next = pos + 1;
    }
  }
  return false;
}

void run_retract(Impl& machine, const Goal& goal) {
  Clause clause;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  if (!prepare_clause(machine, goal, goal.arg(machine, 0), clause, name, arity)) return;
  Predicate* predicate = find_pred(machine, name, arity);
  if (predicate == nullptr || predicate->clauses.empty()) {
    machine.failed = true;
    return;
  }
  Entry entry;
  entry.pick = Pick::AtomGen;
  entry.gen_kind = kRetractGen;
  entry.pred_name = name;
  entry.pred_arity = arity;
  entry.redo_goal = goal.at;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  entry.candidates.reserve(predicate->clauses.size());
  for (std::size_t index = 0; index < predicate->clauses.size(); ++index) entry.candidates.push_back(index);
  push_frame(machine, std::move(entry));
  if (!prove_retract(machine, goal)) machine.failed = true;
}

void run_retractall(Impl& machine, const Goal& goal) {
  Clause clause;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  if (!prepare_clause(machine, goal, goal.arg(machine, 0), clause, name, arity)) return;
  Predicate* predicate = find_pred(machine, name, arity);
  if (predicate == nullptr) {
    succeed(machine, goal);
    return;
  }
  for (std::size_t index = predicate->clauses.size(); index-- > 0;) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const Cell expected = clause_term(machine, predicate->clauses[index]);
    const bool matched = do_unify(machine, goal.arg(machine, 0), expected);
    restore_state(machine, heap_mark, trail_mark, extras_mark);
    if (!matched) continue;
    predicate->clauses.erase(predicate->clauses.begin() + static_cast<std::ptrdiff_t>(index));
    shift_removed(machine, name, arity, index);
  }
  succeed(machine, goal);
}

std::optional<Cell> read_pi(Impl& machine, Cell term, std::uint32_t& name, std::uint16_t& arity) {
  term = machine.heap.deref(term);
  if (machine.heap.unbound(term)) return formal_instantiation(machine);
  if (!(term.tag() == Tag::Str && machine.heap.functor_arity(term) == 2 &&
        machine.atoms.name(machine.heap.functor_atom(term)) == "/")) {
    return formal_type(machine, "predicate_indicator", term);
  }
  const Cell functor = machine.heap.deref(machine.heap.arg(term, 0));
  const Cell count = machine.heap.deref(machine.heap.arg(term, 1));
  if (machine.heap.unbound(functor) || machine.heap.unbound(count)) return formal_instantiation(machine);
  if (functor.tag() != Tag::Atom) return formal_type(machine, "atom", functor);
  if (count.tag() != Tag::Int && count.tag() != Tag::IntBox) return formal_type(machine, "integer", count);
  const auto value = machine.heap.integer_value(count);
  if (value < 0) return formal_domain(machine, "not_less_than_zero", count);
  if (value > kMaxArity) return formal_representation(machine, "max_arity");
  name = atom_id(functor);
  arity = static_cast<std::uint16_t>(value);
  return std::nullopt;
}

void mark_dynamic(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  Predicate* predicate = find_pred(machine, name, arity);
  if (predicate == nullptr) {
    machine.predicates.push_back(Predicate{});
    predicate = &machine.predicates.back();
    predicate->name = name;
    predicate->arity = arity;
  }
  predicate->dynamic = true;
}

bool walk_indicators(Impl& machine, Cell term, std::vector<std::pair<std::uint32_t, std::uint16_t>>& found,
                     Cell& error) {
  term = machine.heap.deref(term);
  if (term.tag() == Tag::Str && machine.heap.functor_arity(term) == 2 &&
      machine.atoms.name(machine.heap.functor_atom(term)) == ",") {
    return walk_indicators(machine, machine.heap.arg(term, 0), found, error) &&
           walk_indicators(machine, machine.heap.arg(term, 1), found, error);
  }
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  if (const std::optional<Cell> problem = read_pi(machine, term, name, arity)) {
    error = *problem;
    return false;
  }
  found.emplace_back(name, arity);
  return true;
}

void run_abolish(Impl& machine, const Goal& goal) {
  const Cell term = machine.heap.deref(goal.arg(machine, 0));
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  if (const std::optional<Cell> problem = read_pi(machine, term, name, arity)) {
    raise_formal(machine, *problem, goal.name, goal.arity);
    return;
  }
  Predicate* predicate = find_pred(machine, name, arity);
  if (predicate == nullptr) {
    succeed(machine, goal);
    return;
  }
  if (!predicate->dynamic) {
    permission_static(machine, goal, name, arity);
    return;
  }
  for (std::size_t index = predicate->clauses.size(); index-- > 0;) shift_removed(machine, name, arity, index);
  machine.predicates.erase(machine.predicates.begin() +
                           static_cast<std::ptrdiff_t>(predicate - machine.predicates.data()));
  succeed(machine, goal);
}

void run_dynamic(Impl& machine, const Goal& goal) {
  std::vector<std::pair<std::uint32_t, std::uint16_t>> found;
  Cell error;
  if (!walk_indicators(machine, goal.arg(machine, 0), found, error)) {
    raise_formal(machine, error, goal.name, goal.arity);
    return;
  }
  for (const auto& [name, arity] : found) {
    const Predicate* predicate = find_pred(machine, name, arity);
    if (predicate != nullptr && !predicate->dynamic && !predicate->clauses.empty()) {
      permission_static(machine, goal, name, arity);
      return;
    }
  }
  for (const auto& [name, arity] : found) mark_dynamic(machine, name, arity);
  succeed(machine, goal);
}

bool is_dynamic_directive(const Impl& machine, Cell directive, Cell& body) {
  directive = machine.heap.deref(directive);
  if (!(directive.tag() == Tag::Str && machine.heap.functor_arity(directive) == 1 &&
        machine.atoms.name(machine.heap.functor_atom(directive)) == ":-")) {
    return false;
  }
  body = machine.heap.deref(machine.heap.arg(directive, 0));
  return body.tag() == Tag::Str && machine.heap.functor_arity(body) == 1 &&
         machine.atoms.name(machine.heap.functor_atom(body)) == "dynamic";
}

}  // namespace

bool redo_retract(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().gen_kind != kRetractGen) return false;
  apply_catches(machine, machine.stack.back());
  const Goal goal = read_goal(machine, machine.stack.back().redo_goal);
  return prove_retract(machine, goal);
}

int apply_dynamic_directive(Impl& machine, Cell directive) {
  Cell body;
  if (!is_dynamic_directive(machine, directive, body)) return 0;
  std::vector<std::pair<std::uint32_t, std::uint16_t>> found;
  Cell error;
  if (!walk_indicators(machine, machine.heap.arg(body, 0), found, error)) {
    machine.diagnostic = write_term(machine.heap, machine.operators, error, {}, {});
    return -1;
  }
  for (const auto& [pred_name, arity] : found) {
    const Predicate* predicate = find_pred(machine, pred_name, arity);
    if (predicate != nullptr && !predicate->dynamic && !predicate->clauses.empty()) {
      machine.diagnostic = write_term(machine.heap, machine.operators,
                                      formal_permission(machine, "modify", "static_procedure",
                                                        cell_functor(machine, pred_name, arity)),
                                      {}, {});
      return -1;
    }
  }
  for (const auto& [pred_name, arity] : found) mark_dynamic(machine, pred_name, arity);
  return 1;
}

bool run_database(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "assertz" && goal.arity == 1) {
    run_assert(machine, goal, false);
    return true;
  }
  if (name == "asserta" && goal.arity == 1) {
    run_assert(machine, goal, true);
    return true;
  }
  if (name == "retract" && goal.arity == 1) {
    run_retract(machine, goal);
    return true;
  }
  if (name == "retractall" && goal.arity == 1) {
    run_retractall(machine, goal);
    return true;
  }
  if (name == "abolish" && goal.arity == 1) {
    run_abolish(machine, goal);
    return true;
  }
  if (name == "dynamic" && goal.arity == 1) {
    run_dynamic(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
