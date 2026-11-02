// 8.8.1 clause/2 (+head, ?body)
//   instantiation_error, type_error(callable), permission_error(access, private_procedure, PI)
// 8.8.2 current_predicate/1 (?predicate_indicator)
//   type_error(predicate_indicator), type_error(atom), type_error(integer),
//   domain_error(not_less_than_zero)
// 節と述語を読むだけの組込。節を変える組込は database.cpp にある。
// テスト: tests/pl/database.pl, tests/pl/clause.pl

#include "database_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace mosa {

using database::HeadName;
using database::bound_first;
using database::callable_term;
using database::collect_ids;
using database::head_indicator;
using database::push_if_more;
using database::raise_instantiation;
using database::raise_type;
using database::scan_entry;
using database::step_scan;
using database::succeed;
using database::strip_target;

namespace {

void raise_private(Impl& machine, const Goal& goal, std::uint32_t name, std::uint16_t arity) {
  // modify/static_procedure は改変の誤り。参照の非公開は access/private_procedure。
  const Cell indicator = cell_functor(machine, name, arity);
  raise_formal(machine, formal_permission(machine, "access", "private_procedure", indicator), goal.name, goal.arity);
}

bool clause_head_ok(Impl& machine, const Goal& goal, HeadName& indicator, std::uint32_t& module) {
  Cell head = goal.arg(machine, 0);
  if (!strip_target(machine, goal, head, module, false)) return false;
  if (machine.heap.unbound(head)) {
    raise_instantiation(machine, goal);
    return false;
  }
  if (!callable_term(machine, head)) {
    raise_type(machine, goal, "callable", head);
    return false;
  }
  const Cell body = machine.heap.deref(goal.arg(machine, 1));
  if (!machine.heap.unbound(body) && !callable_term(machine, body)) {
    raise_type(machine, goal, "callable", body);
    return false;
  }
  indicator = head_indicator(machine, head);
  if (!is_builtin_procedure(machine, indicator.name, indicator.arity)) return true;
  raise_private(machine, goal, indicator.name, indicator.arity);
  return false;
}

// floor は節を組む前の水位。それより前の変数（呼び出しの変数）の束縛は全部トレイルに残す。do_unify は上の選択点
// より新しい変数を残さないので、選択点を積む前に束縛すると、やり直しで前の答えの束縛が残る（レビュー M18）。
bool unify_clause_ref(Impl& machine, const Clause& clause, Cell head, Cell body) {
  const auto floor = machine.heap.size();
  std::vector<Cell> slots;
  const Cell made_head = ir_to_term(machine, clause.head, slots, false);
  const Cell made_body = ir_to_term(machine, *clause.body, slots, true);
  return unify_visible(machine, floor, head, made_head) && unify_visible(machine, floor, body, made_body);
}

// 候補の列は呼び出しごとに 1 つ。次の枠へ移すだけにし、やり直すたびに写さない（レビュー M3）。PredGen は節を
// 走査しないので世代を持たない。
void push_scan(Impl& machine, Pick pick, const Goal& goal, PredKey key, std::vector<std::size_t>&& matches,
               std::size_t next, std::size_t heap_mark, std::size_t trail_mark) {
  Entry entry;
  entry.pick = pick;
  entry.candidates = std::move(matches);
  entry.next = next;
  entry.redo_goal = goal.at;
  entry.pred_module = key.module;
  entry.pred_name = key.name;
  entry.pred_arity = key.arity;
  // 水位は単一化の前。成功した項を含めると、やり直しで束縛が残る。
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

// 第1引数を束縛した clause/2 は key_candidates の候補だけを試しに単一化し、合う節の列で走査する。未束縛なら
// どの節も候補なので、列を作らずにカーソルで進む（ADR 0015）。
Entry clause_scan(Impl& machine, const Goal& goal, const Predicate& predicate, PredKey key, Cell head, Cell body) {
  const auto generation = machine.db_generation;
  Entry scan = scan_entry(Pick::ClauseScan, goal, key, generation);
  const auto first = bound_first(machine, head);
  if (!first) {
    scan.by_cursor = true;
    return scan;
  }
  const auto match = [&machine, head, body](const Clause& clause) {
    return unify_clause_ref(machine, clause, head, body);
  };
  scan.candidates = collect_ids(machine, predicate, key_candidates(machine, predicate, *first, generation), match);
  return scan;
}

// scan.generation は最初の呼び出しの世代で、やり直しても進めない。次に合う節があれば、束縛する前に scan を積む。
bool take_clause(Impl& machine, const Goal& goal, Entry scan, bool redo) {
  const PredKey key{scan.pred_module, scan.pred_name, scan.pred_arity};
  const Predicate* predicate = find_pred(machine, key);
  if (predicate == nullptr) return false;
  // M: は最初の呼び出しで検査済み。節の頭部と比べるのは内側の項。
  const Cell head = named_head(machine, skip_module(machine, goal.arg(machine, 0)), key.name);
  const Cell body = goal.arg(machine, 1);
  // 走査を始めた後に消した節も、走査の世代には見えるので試す（7.5.4）。
  const auto generation = scan.generation;
  const auto usable = [generation](const Clause& clause) { return visible(clause, generation); };
  const auto match = [&machine, head, body](const Clause& clause) {
    return unify_clause_ref(machine, clause, head, body);
  };
  const auto position = step_scan(machine, *predicate, scan, redo, usable, match);
  if (!position) return false;
  push_if_more(machine, std::move(scan));
  if (!unify_clause_ref(machine, predicate->clauses[*position], head, body)) return false;
  succeed(machine, goal);
  return true;
}


}  // namespace

namespace database {

void run_clause(Impl& machine, const Goal& goal) {
  HeadName indicator;
  std::uint32_t module = 0;
  if (!clause_head_ok(machine, goal, indicator, module)) return;
  const Predicate* predicate = owned_pred(machine, module, indicator.name, indicator.arity);
  if (predicate == nullptr || predicate->abolished || live_count(*predicate) == 0) {
    machine.failed = true;
    return;
  }
  const PredKey key{predicate->module, indicator.name, indicator.arity};
  const Cell head = named_head(machine, skip_module(machine, goal.arg(machine, 0)), key.name);
  Entry scan = clause_scan(machine, goal, *predicate, key, head, goal.arg(machine, 1));
  if (!take_clause(machine, goal, std::move(scan), false)) machine.failed = true;
}

}  // namespace database

namespace {

struct WantedPred {
  bool name_set = false;
  bool arity_set = false;
  bool arity_high = false;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  // M:PI の M が変数なら、どのモジュールの述語も候補にする。
  bool any_module = false;
  std::uint32_t module = 0;
};

bool qualified_term(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(cell)) == ":";
}

bool slash_indicator(const Impl& machine, Cell pred) {
  return pred.tag() == Tag::Str && machine.heap.functor_arity(pred) == 2 &&
         machine.atoms.name(machine.heap.functor_atom(pred)) == "/";
}

bool accept_indicator(Impl& machine, const Goal& goal, Cell pred, WantedPred& wanted) {
  pred = machine.heap.deref(pred);
  if (machine.heap.unbound(pred)) return true;
  if (!slash_indicator(machine, pred)) {
    raise_type(machine, goal, "predicate_indicator", pred);
    return false;
  }
  const Cell name = machine.heap.deref(machine.heap.arg(pred, 0));
  const Cell arity = machine.heap.deref(machine.heap.arg(pred, 1));
  if (!machine.heap.unbound(name) && name.tag() != Tag::Atom) {
    raise_type(machine, goal, "atom", name);
    return false;
  }
  if (!machine.heap.unbound(arity) && arity.tag() != Tag::Int && arity.tag() != Tag::IntBox) {
    raise_type(machine, goal, "integer", arity);
    return false;
  }
  if (!machine.heap.unbound(arity)) {
    const auto value = machine.heap.integer_value(arity);
    if (value < 0) {
      raise_formal(machine, formal_domain(machine, "not_less_than_zero", arity), goal.name, goal.arity);
      return false;
    }
    // アリティは 16bit。超える述語は表に無く、検査順に表現エラーは無い。
    if (value > 65535) wanted.arity_high = true;
    else {
      wanted.arity_set = true;
      wanted.arity = static_cast<std::uint16_t>(value);
    }
  }
  if (!machine.heap.unbound(name)) {
    wanted.name_set = true;
    wanted.name = static_cast<std::uint32_t>(name.raw() >> 3);
  }
  return true;
}

std::vector<std::size_t> pred_matches(const Impl& machine, const WantedPred& wanted) {
  std::vector<std::size_t> found;
  if (wanted.arity_high) return found;
  for (std::size_t index = 0; index < machine.predicates.size(); ++index) {
    const Predicate& predicate = machine.predicates[index];
    if (predicate.abolished) continue;
    if (!wanted.any_module && predicate.module != wanted.module) continue;
    if (wanted.name_set && predicate.name != wanted.name) continue;
    if (wanted.arity_set && predicate.arity != wanted.arity) continue;
    found.push_back(index);
  }
  return found;
}

bool take_pred(Impl& machine, const Goal& goal, std::vector<std::size_t> matches, std::size_t begin) {
  const Cell wanted = goal.arg(machine, 0);
  // やり直しでも引数の形は同じ。M:PI には M も束縛する。
  const bool qualified = qualified_term(machine, wanted);
  for (std::size_t cursor = begin; cursor < matches.size(); ++cursor) {
    const auto index = matches[cursor];
    if (index >= machine.predicates.size()) continue;
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const Predicate& predicate = machine.predicates[index];
    Cell indicator = cell_functor(machine, predicate.name, predicate.arity);
    if (qualified) {
      const std::array<Cell, 2> parts{machine.heap.atom(predicate.module), indicator};
      indicator = machine.heap.structure(machine.atoms.intern(":"), parts);
    }
    if (!do_unify(machine, wanted, indicator)) {
      restore_state(machine, heap_mark, trail_mark, extras_mark);
      continue;
    }
    if (cursor + 1 < matches.size()) {
      push_scan(machine, Pick::PredGen, goal, PredKey{}, std::move(matches), cursor + 1, heap_mark, trail_mark);
    }
    succeed(machine, goal);
    return true;
  }
  return false;
}


}  // namespace

namespace database {

void run_current_predicate(Impl& machine, const Goal& goal) {
  WantedPred wanted;
  wanted.module = goal.module;
  Cell pred = machine.heap.deref(goal.arg(machine, 0));
  if (qualified_term(machine, pred)) {
    const Cell module = machine.heap.deref(machine.heap.arg(pred, 0));
    wanted.any_module = machine.heap.unbound(module);
    if (!wanted.any_module && module.tag() != Tag::Atom) {
      raise_type(machine, goal, "atom", module);
      return;
    }
    // 無いモジュールには述語も無い。列挙は失敗で、存在エラーにはしない。
    if (!wanted.any_module) wanted.module = static_cast<std::uint32_t>(module.raw() >> 3);
    pred = machine.heap.arg(pred, 1);
  }
  if (!accept_indicator(machine, goal, pred, wanted)) return;
  if (!take_pred(machine, goal, pred_matches(machine, wanted), 0)) machine.failed = true;
}

}  // namespace database

// 枠を外す前に枠ごと移す。候補の列を写すと、解ごとに残りの候補の数だけかかる（レビュー M3）。
bool redo_clause_scan(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::ClauseScan) return false;
  apply_catches(machine, machine.stack.back());
  Entry scan = std::move(machine.stack.back());
  machine.stack.pop_back();
  const Goal goal = read_goal(machine, scan.redo_goal);
  return take_clause(machine, goal, std::move(scan), true);
}

bool redo_pred_gen(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::PredGen) return false;
  Entry& top = machine.stack.back();
  apply_catches(machine, top);
  const auto at = top.redo_goal;
  const auto next = top.next;
  std::vector<std::size_t> candidates = std::move(top.candidates);
  machine.stack.pop_back();
  return take_pred(machine, read_goal(machine, at), std::move(candidates), next);
}

}  // namespace mosa
