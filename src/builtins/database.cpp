// clause/2 と current_predicate/1 は database_scan.cpp にある。
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
// テスト: tests/pl/database.pl, tests/pl/clause.pl

#include "database_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {

namespace database {

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

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

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

bool strip_target(Impl& machine, const Goal& goal, Cell& term, std::uint32_t& module, bool clause) {
  module = goal.module;
  const auto formal = clause ? strip_clause(machine, term, module) : strip_module(machine, term, module);
  if (!formal) return true;
  raise_formal(machine, *formal, goal.name, goal.arity);
  return false;
}

}  // namespace database

using database::HeadName;
using database::strip_target;
using database::run_clause;
using database::run_current_predicate;
using database::callable_term;
using database::head_indicator;
using database::raise_instantiation;
using database::raise_type;
using database::succeed;

namespace {

struct ClauseView {
  Cell head{};
  Cell body{};
  bool rule = false;
  // retractall は頭部だけ。retract(Head) は本体を true と単一化する。
  bool head_only = false;
};


void raise_permission(Impl& machine, const Goal& goal, std::uint32_t module, std::uint32_t name,
                      std::uint16_t arity) {
  const Cell indicator = module_indicator(machine, module, name, arity);
  raise_formal(machine, formal_permission(machine, "modify", "static_procedure", indicator), goal.name,
               goal.arity);
}

bool refuse_builtin(Impl& machine, const Goal& goal, std::uint32_t name, std::uint16_t arity) {
  // 組込は述語表に無い。無い述語として足すと、本体に届かない隠れ節になる。
  if (!is_builtin_procedure(machine, name, arity)) return false;
  raise_permission(machine, goal, machine.user_module, name, arity);
  return true;
}

bool neck_static(const Impl& machine, std::uint32_t name, std::uint16_t arity) {
  // :-/1 は指示、:-/2 は節の首。組込表に無いが assert では静的。
  // 表へ足すと retract と clause の誤り種別まで変わる。
  if (machine.atoms.name(name) != ":-") return false;
  return arity == 1 || arity == 2;
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

bool reject_clause(Impl& machine, const Goal& goal, Cell term, bool allow_body_var) {
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
    // retract((H :- _)) は本体を束縛する。assert の変数本体は具体化エラーのまま。
    if (allow_body_var) return false;
    raise_instantiation(machine, goal);
    return true;
  }
  if (!callable_term(machine, view.body)) {
    raise_type(machine, goal, "callable", view.body);
    return true;
  }
  return false;
}

// 数はゴールにならない。変数と匿名は call できるので、ここでは許す。
bool goal_form(const IrTerm& term) {
  switch (term.tag) {
    case IrTag::Int:
    case IrTag::Float: return false;
    case IrTag::And:
    case IrTag::Or:
    case IrTag::IfThen:
    case IrTag::SoftIf:
      return term.args.size() >= 2 && goal_form(term.args[0]) && goal_form(term.args[1]);
    case IrTag::Catch:
      return term.args.size() >= 3 && goal_form(term.args[0]) && goal_form(term.args[2]);
    case IrTag::Call: return term.args.empty() || goal_form(term.args[0]);
    default: return true;
  }
}

// candidates が節の添字である選択点だけ。述語の添字を足すと asserta が別の述語を飛ばす。
bool scans_clauses(Pick pick) {
  return pick == Pick::Clauses || pick == Pick::Retract || pick == Pick::ClauseScan;
}

// 同じ名前の述語はモジュールごとにある。名前だけで比べると、別のモジュールの走査をずらす。
bool scans_pred(const Entry& entry, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  return scans_clauses(entry.pick) && entry.pred_module == module && entry.pred_name == name &&
         entry.pred_arity == arity;
}

void note_removed(Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity,
                  std::size_t removed) {
  for (Entry& entry : machine.stack) {
    if (!scans_pred(entry, module, name, arity)) continue;
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

void note_asserta(Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  for (Entry& entry : machine.stack) {
    if (!scans_pred(entry, module, name, arity)) continue;
    // 先頭に足しても、開いている走査は新しい節を見ない。古い添字だけ 1 つずれる。
    for (std::size_t& index : entry.candidates) ++index;
  }
}

void erase_clause(Predicate& predicate, std::size_t index) {
  predicate.clauses.erase(predicate.clauses.begin() + static_cast<std::ptrdiff_t>(index));
}

void shift_pred_index(Entry& entry, std::size_t removed) {
  std::vector<std::size_t> kept;
  kept.reserve(entry.candidates.size());
  std::size_t next = entry.next;
  for (std::size_t cursor = 0; cursor < entry.candidates.size(); ++cursor) {
    auto index = entry.candidates[cursor];
    if (index == removed) {
      if (cursor < next) --next;
      continue;
    }
    if (index > removed) --index;
    kept.push_back(index);
  }
  entry.candidates = std::move(kept);
  entry.next = next;
}

void erase_predicate(Impl& machine, std::uint32_t module, std::uint32_t name, std::uint16_t arity) {
  std::size_t removed = machine.predicates.size();
  for (std::size_t index = 0; index < machine.predicates.size(); ++index) {
    const Predicate& predicate = machine.predicates[index];
    if (predicate.module != module || predicate.name != name || predicate.arity != arity) continue;
    removed = index;
    break;
  }
  for (Entry& entry : machine.stack) {
    if (scans_pred(entry, module, name, arity)) {
      entry.candidates.clear();
      entry.next = 0;
      continue;
    }
    // current_predicate の候補は述語ベクタの添字。節の添字とは別にずらす。
    if (entry.pick == Pick::PredGen && removed < machine.predicates.size()) shift_pred_index(entry, removed);
  }
  if (removed >= machine.predicates.size()) return;
  machine.predicates.erase(machine.predicates.begin() + static_cast<std::ptrdiff_t>(removed));
}

// ADR 0008。開いた走査がまだ試していない候補にこの節があるか。試し終えた節と今動いている節は消してよい。
bool held_by_scan(const Impl& machine, PredKey key, std::size_t index) {
  for (const Entry& entry : machine.stack) {
    if (!scans_pred(entry, key.module, key.name, key.arity)) continue;
    for (std::size_t cursor = entry.next; cursor < entry.candidates.size(); ++cursor) {
      if (entry.candidates[cursor] == index) return true;
    }
  }
  return false;
}

void kill_clause(Impl& machine, Predicate& predicate, PredKey key, std::size_t index) {
  if (held_by_scan(machine, key, index)) {
    predicate.clauses[index].dead = true;
    return;
  }
  erase_clause(predicate, index);
  note_removed(machine, key.module, key.name, key.arity, index);
}

// 死んだ節は、それを持つ走査が閉じた後の最初の変更で消す。節が尽きた abolish 済みの述語は表からも消す。
// 添字をずらすので、候補を集める前に呼ぶ。
Predicate* purge_dead(Impl& machine, Predicate* predicate) {
  if (predicate == nullptr) return nullptr;
  const PredKey key{predicate->module, predicate->name, predicate->arity};
  for (std::size_t index = predicate->clauses.size(); index > 0; --index) {
    if (predicate->clauses[index - 1].dead && !held_by_scan(machine, key, index - 1)) {
      erase_clause(*predicate, index - 1);
      note_removed(machine, key.module, key.name, key.arity, index - 1);
    }
  }
  if (!predicate->abolished || !predicate->clauses.empty()) return predicate;
  erase_predicate(machine, key.module, key.name, key.arity);
  return nullptr;
}

// abolish 済みの述語へ足すときは、同じ枠を新しい動的な述語として使い直す。走査の添字が二つの述語を指さない。
void revive(Impl& machine, Predicate& predicate) {
  if (!predicate.abolished) return;
  predicate.abolished = false;
  predicate.dynamic = true;
  predicate.dynamic_declared = false;
  predicate.meta.clear();
  adopt_meta(machine, predicate);
}

void abolish_open(Impl& machine, Predicate& predicate) {
  const PredKey key{predicate.module, predicate.name, predicate.arity};
  for (std::size_t index = predicate.clauses.size(); index > 0; --index) {
    kill_clause(machine, predicate, key, index - 1);
  }
  if (predicate.clauses.empty()) {
    erase_predicate(machine, key.module, key.name, key.arity);
    return;
  }
  predicate.abolished = true;
}

bool unify_stored(Impl& machine, const Clause& clause, const ClauseView& view) {
  // 呼び出し側の変数は、この関数が足す項より前にある。候補ごとに束縛を戻す水位。
  const auto floor = machine.heap.size();
  std::vector<Cell> env;
  env.reserve(clause.vars);
  for (std::uint16_t index = 0; index < clause.vars; ++index) env.push_back(machine.heap.fresh_ref());
  if (!unify_visible(machine, floor, view.head, materialize(machine, clause.head, env))) return false;
  if (view.head_only) return true;
  const Cell body = materialize(machine, clause.body, env);
  if (!view.rule) {
    // 事実の正規形は本体 true。規則は retract(Head) では消さない。
    return unify_visible(machine, floor, machine.heap.atom(machine.atoms.intern("true")), body);
  }
  return unify_visible(machine, floor, view.body, body);
}

std::vector<std::size_t> collect_matches(Impl& machine, const Predicate& predicate, const ClauseView& view) {
  std::vector<std::size_t> found;
  for (std::size_t index = 0; index < predicate.clauses.size(); ++index) {
    if (predicate.clauses[index].dead) continue;
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const bool matched = unify_stored(machine, predicate.clauses[index], view);
    if (matched) found.push_back(index);
    restore_state(machine, heap_mark, trail_mark, extras_mark);
  }
  return found;
}

void push_retract(Impl& machine, const Goal& goal, PredKey key, const std::vector<std::size_t>& matches,
                  std::size_t next, std::size_t heap_mark, std::size_t trail_mark) {
  Entry entry;
  entry.pick = Pick::Retract;
  entry.candidates = matches;
  entry.next = next;
  entry.redo_goal = goal.at;
  entry.pred_module = key.module;
  entry.pred_name = key.name;
  entry.pred_arity = key.arity;
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

bool take_retract(Impl& machine, const Goal& goal, PredKey key, const std::vector<std::size_t>& matches,
                  std::size_t begin) {
  Predicate* predicate = find_pred(machine, key);
  if (predicate == nullptr) return false;
  // 引数は最初の呼び出しで検査済み。M: は外すだけにし、節を作り直してヒープを使わない。
  ClauseView view = clause_view(machine, skip_module(machine, goal.arg(machine, 0)));
  view.head = named_head(machine, skip_module(machine, view.head), key.name);
  for (std::size_t cursor = begin; cursor < matches.size(); ++cursor) {
    const auto index = matches[cursor];
    // 別の retract が先に消した節は、もう無い節である。
    if (index >= predicate->clauses.size() || predicate->clauses[index].dead) continue;
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    if (!unify_stored(machine, predicate->clauses[index], view)) {
      restore_state(machine, heap_mark, trail_mark, extras_mark);
      continue;
    }
    if (cursor + 1 < matches.size()) {
      push_retract(machine, goal, key, matches, cursor + 1, heap_mark, trail_mark);
    }
    kill_clause(machine, *predicate, key, index);
    succeed(machine, goal);
    return true;
  }
  return false;
}

void run_assert(Impl& machine, const Goal& goal, bool at_front) {
  Cell term = goal.arg(machine, 0);
  std::uint32_t module = 0;
  if (!strip_target(machine, goal, term, module, true)) return;
  if (reject_clause(machine, goal, term, false)) return;
  const ClauseView view = clause_view(machine, term);
  const HeadName head = head_indicator(machine, view.head);
  // 別名で取り込んだ述語へ足す節は、取り込み元の名前の頭で持つ。検査と誤りは呼び手の書いた名前で出す。
  std::uint32_t source_name = head.name;
  Predicate* existing = purge_dead(machine, owned_pred(machine, module, source_name, head.arity));
  term = named_head(machine, term, source_name);
  Clause clause;
  // 呼び出し後の束縛が節へ漏れないよう、データベースに見える前に IR の変数番号へ切る。
  if (!compile_clause(machine.heap, term, clause)) {
    raise_type(machine, goal, "callable", view.head);
    return;
  }
  // 8.9.1.3 は、非公開より先に本体がゴールへ変換できるかを見る。
  if (view.rule && !goal_form(clause.body)) {
    raise_type(machine, goal, "callable", view.body);
    return;
  }
  if (refuse_builtin(machine, goal, head.name, head.arity)) return;
  // 無い :- をこの後で動的に作ると、節の首へ足せてしまう。宣言済みでも静的のまま。
  if (neck_static(machine, head.name, head.arity)) {
    raise_permission(machine, goal, machine.user_module, head.name, head.arity);
    return;
  }
  if (existing != nullptr && !existing->dynamic) {
    raise_permission(machine, goal, existing->module, source_name, head.arity);
    return;
  }
  if (existing != nullptr) revive(machine, *existing);
  if (existing == nullptr) {
    Predicate created;
    created.module = module;
    created.name = source_name;
    created.arity = head.arity;
    created.dynamic = true;
    machine.predicates.push_back(std::move(created));
    existing = &machine.predicates.back();
    adopt_meta(machine, *existing);
  }
  if (at_front) {
    existing->clauses.insert(existing->clauses.begin(), std::move(clause));
    note_asserta(machine, existing->module, source_name, head.arity);
  } else {
    existing->clauses.push_back(std::move(clause));
  }
  succeed(machine, goal);
}

void run_retract(Impl& machine, const Goal& goal) {
  Cell term = goal.arg(machine, 0);
  std::uint32_t module = 0;
  if (!strip_target(machine, goal, term, module, true)) return;
  if (reject_clause(machine, goal, term, true)) return;
  const ClauseView view = clause_view(machine, term);
  HeadName head = head_indicator(machine, view.head);
  if (refuse_builtin(machine, goal, head.name, head.arity)) return;
  Predicate* predicate = purge_dead(machine, owned_pred(machine, module, head.name, head.arity));
  if (predicate == nullptr || predicate->abolished) {
    machine.failed = true;
    return;
  }
  if (!predicate->dynamic) {
    raise_permission(machine, goal, predicate->module, head.name, head.arity);
    return;
  }
  const PredKey key{predicate->module, head.name, head.arity};
  ClauseView source = view;
  source.head = named_head(machine, view.head, head.name);
  const auto matches = collect_matches(machine, *predicate, source);
  if (!take_retract(machine, goal, key, matches, 0)) machine.failed = true;
}

void run_retractall(Impl& machine, const Goal& goal) {
  Cell head_cell = goal.arg(machine, 0);
  std::uint32_t module = 0;
  if (!strip_target(machine, goal, head_cell, module, false)) return;
  if (machine.heap.unbound(head_cell)) {
    raise_instantiation(machine, goal);
    return;
  }
  if (!callable_term(machine, head_cell)) {
    raise_type(machine, goal, "callable", head_cell);
    return;
  }
  HeadName head = head_indicator(machine, head_cell);
  if (refuse_builtin(machine, goal, head.name, head.arity)) return;
  Predicate* predicate = purge_dead(machine, owned_pred(machine, module, head.name, head.arity));
  if (predicate == nullptr || predicate->abolished) {
    succeed(machine, goal);
    return;
  }
  if (!predicate->dynamic) {
    raise_permission(machine, goal, predicate->module, head.name, head.arity);
    return;
  }
  const PredKey key{predicate->module, head.name, head.arity};
  ClauseView view;
  view.head = named_head(machine, head_cell, head.name);
  view.head_only = true;
  const auto matches = collect_matches(machine, *predicate, view);
  for (std::size_t cursor = matches.size(); cursor > 0; --cursor) {
    const auto index = matches[cursor - 1];
    Predicate* current = find_pred(machine, key);
    if (current == nullptr || index >= current->clauses.size()) continue;
    kill_clause(machine, *current, key, index);
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
  Cell pred = goal.arg(machine, 0);
  std::uint32_t module = 0;
  if (!strip_target(machine, goal, pred, module, false)) return;
  HeadName indicator;
  if (!parse_indicator(machine, goal, pred, indicator)) return;
  if (refuse_builtin(machine, goal, indicator.name, indicator.arity)) return;
  Predicate* predicate = purge_dead(machine, owned_pred(machine, module, indicator.name, indicator.arity));
  if (predicate != nullptr && predicate->abolished) predicate = nullptr;
  if (predicate != nullptr && !predicate->dynamic) {
    raise_permission(machine, goal, predicate->module, indicator.name, indicator.arity);
    return;
  }
  if (predicate != nullptr) abolish_open(machine, *predicate);
  succeed(machine, goal);
}


}  // namespace

// 読み込みが abolish 済みの枠を使い直すときにも、同じ片付けをする（ADR 0008）。
Predicate* purge_dead_clauses(Impl& machine, Predicate* predicate) { return purge_dead(machine, predicate); }

bool redo_retract(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::Retract) return false;
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  machine.stack.pop_back();
  return take_retract(machine, read_goal(machine, top.redo_goal), PredKey{top.pred_module, top.pred_name, top.pred_arity},
                      top.candidates, top.next);
}


bool run_database(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "clause" && goal.arity == 2) {
    run_clause(machine, goal);
    return true;
  }
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
  if (name == "current_predicate") {
    run_current_predicate(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
