// 8.8.1 clause/2 (+head, ?body)
//   instantiation_error, type_error(callable), permission_error(access, private_procedure, PI)
// 8.8.2 current_predicate/1 (?predicate_indicator)
//   type_error(predicate_indicator), type_error(atom), type_error(integer),
//   domain_error(not_less_than_zero)
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
// 7.4.2 dynamic/1 は指示であり、ゴールとしても呼べる。修飾は名付けたモジュール。
// 修飾した頭部は名付けたモジュールのローカル述語。取り込みだけの席は modify/static_procedure。
// テスト: tests/pl/database.pl, tests/pl/clause.pl, tests/pl/modules.pl

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
  // retractall は頭部だけ。retract(Head) は本体を true と単一化する。
  bool head_only = false;
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

bool refuse_builtin(Impl& machine, const Goal& goal, std::uint32_t name, std::uint16_t arity) {
  // 組込は述語表に無い。無い述語として足すと、本体に届かない隠れ節になる。
  if (!is_builtin_procedure(machine, name, arity)) return false;
  raise_permission(machine, goal, name, arity);
  return true;
}

bool neck_static(const Impl& machine, std::uint32_t name, std::uint16_t arity) {
  // :-/1 は指示、:-/2 は節の首。組込表に無いが assert では静的。
  // 表へ足すと retract と clause の誤り種別まで変わる。
  if (machine.atoms.name(name) != ":-") return false;
  return arity == 1 || arity == 2;
}

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

Predicate* find_predicate(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  return predicate_in(machine, machine.calling, name, arity);
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

void note_removed(Impl& machine, std::uint32_t name, std::uint16_t arity, std::size_t removed) {
  for (Entry& entry : machine.stack) {
    if (entry.pred_home != machine.calling || entry.pred_name != name || entry.pred_arity != arity) continue;
    if (!scans_clauses(entry.pick)) continue;
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
    if (entry.pred_home != machine.calling || entry.pred_name != name || entry.pred_arity != arity) continue;
    if (!scans_clauses(entry.pick)) continue;
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

void erase_predicate(Impl& machine, std::uint32_t name, std::uint16_t arity) {
  std::size_t removed = machine.predicates.size();
  for (std::size_t index = 0; index < machine.predicates.size(); ++index) {
    const Predicate& predicate = machine.predicates[index];
    if (predicate.home != machine.calling || predicate.name != name || predicate.arity != arity) continue;
    removed = index;
    break;
  }
  for (Entry& entry : machine.stack) {
    if (scans_clauses(entry.pick) && entry.pred_home == machine.calling && entry.pred_name == name &&
        entry.pred_arity == arity) {
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
  entry.pred_home = machine.calling;
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

// 修飾した操作のあいだだけ呼び出し元を合わせる。戻す前に継続は ip へ入る。
struct CallAs {
  Impl& machine;
  std::uint32_t saved;
  CallAs(Impl& machine, std::uint32_t home) : machine(machine), saved(machine.calling) { machine.calling = home; }
  ~CallAs() { machine.calling = saved; }
  CallAs(const CallAs&) = delete;
  CallAs& operator=(const CallAs&) = delete;
};

void run_assert(Impl& machine, const Goal& goal, bool at_front) {
  std::uint32_t home = 0;
  Cell term;
  if (!place_clause(machine, goal, machine.heap.deref(goal.arg(machine, 0)), home, term)) return;
  if (reject_clause(machine, goal, term, false)) return;
  const ClauseView view = clause_view(machine, term);
  const HeadName head = head_indicator(machine, view.head);
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
    raise_permission(machine, goal, head.name, head.arity);
    return;
  }
  Predicate* existing = predicate_in(machine, home, head.name, head.arity);
  if (existing != nullptr && !existing->dynamic) {
    raise_permission(machine, goal, head.name, head.arity);
    return;
  }
  if (existing == nullptr) {
    Predicate created;
    created.home = home;
    created.name = head.name;
    created.arity = head.arity;
    created.dynamic = true;
    stamp_load(machine, created);
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
  std::uint32_t home = 0;
  Cell term;
  if (!place_clause(machine, goal, machine.heap.deref(goal.arg(machine, 0)), home, term)) return;
  if (reject_clause(machine, goal, term, true)) return;
  // やり直しも同じ頭部を照合する。:/2 のままだと節と合わない。
  machine.heap.set_word(goal.at + kFrameHead, term.raw());
  [[maybe_unused]] const CallAs as{machine, home};
  const ClauseView view = clause_view(machine, term);
  const HeadName head = head_indicator(machine, view.head);
  if (refuse_builtin(machine, goal, head.name, head.arity)) return;
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
  std::uint32_t home = 0;
  Cell head_cell;
  if (!place_clause(machine, goal, machine.heap.deref(goal.arg(machine, 0)), home, head_cell)) return;
  [[maybe_unused]] const CallAs as{machine, home};
  if (machine.heap.unbound(head_cell)) {
    raise_instantiation(machine, goal);
    return;
  }
  if (!callable_term(machine, head_cell)) {
    raise_type(machine, goal, "callable", head_cell);
    return;
  }
  const HeadName head = head_indicator(machine, head_cell);
  if (refuse_builtin(machine, goal, head.name, head.arity)) return;
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
  view.head_only = true;
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

void run_dynamic(Impl& machine, const Goal& goal) {
  std::uint32_t home = 0;
  Cell pred;
  if (!place_clause(machine, goal, machine.heap.deref(goal.arg(machine, 0)), home, pred)) return;
  HeadName indicator;
  if (!parse_indicator(machine, goal, pred, indicator)) return;
  if (refuse_builtin(machine, goal, indicator.name, indicator.arity)) return;
  Predicate* existing = predicate_in(machine, home, indicator.name, indicator.arity);
  // 取り込みの席を動的にすると、次の assert が公開した側ではなく席へ書く。
  if (existing != nullptr && existing->imported) {
    raise_permission(machine, goal, indicator.name, indicator.arity);
    return;
  }
  // 節のある静的述語は動的にしない。空の席と、まだ無い述語は動的にする。
  if (existing != nullptr && !existing->dynamic && !existing->clauses.empty()) {
    raise_permission(machine, goal, indicator.name, indicator.arity);
    return;
  }
  if (existing == nullptr) {
    Predicate created;
    created.home = home;
    created.name = indicator.name;
    created.arity = indicator.arity;
    created.dynamic = true;
    created.dynamic_declared = true;
    stamp_load(machine, created);
    machine.predicates.push_back(std::move(created));
  } else {
    existing->dynamic = true;
    existing->dynamic_declared = true;
  }
  succeed(machine, goal);
}

void run_abolish(Impl& machine, const Goal& goal) {
  std::uint32_t home = 0;
  Cell pred;
  if (!place_clause(machine, goal, machine.heap.deref(goal.arg(machine, 0)), home, pred)) return;
  HeadName indicator;
  if (!parse_indicator(machine, goal, pred, indicator)) return;
  [[maybe_unused]] const CallAs as{machine, home};
  if (refuse_builtin(machine, goal, indicator.name, indicator.arity)) return;
  Predicate* predicate = find_predicate(machine, indicator.name, indicator.arity);
  if (predicate != nullptr && !predicate->dynamic) {
    raise_permission(machine, goal, indicator.name, indicator.arity);
    return;
  }
  if (predicate != nullptr) erase_predicate(machine, indicator.name, indicator.arity);
  succeed(machine, goal);
}

void raise_private(Impl& machine, const Goal& goal, std::uint32_t name, std::uint16_t arity) {
  // modify/static_procedure は改変の誤り。参照の非公開は access/private_procedure。
  const Cell indicator = cell_functor(machine, name, arity);
  raise_formal(machine, formal_permission(machine, "access", "private_procedure", indicator), goal.name, goal.arity);
}

bool clause_head_ok(Impl& machine, const Goal& goal, HeadName& indicator) {
  const Cell head = machine.heap.deref(goal.arg(machine, 0));
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

bool unify_clause_ref(Impl& machine, const Clause& clause, Cell head, Cell body) {
  std::vector<Cell> slots;
  const Cell made_head = ir_to_term(machine, clause.head, slots, false);
  const Cell made_body = ir_to_term(machine, clause.body, slots, true);
  return do_unify(machine, head, made_head) && do_unify(machine, body, made_body);
}

std::vector<std::size_t> matching_clauses(Impl& machine, const Predicate& predicate, Cell head, Cell body) {
  std::vector<std::size_t> found;
  for (std::size_t index = 0; index < predicate.clauses.size(); ++index) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    if (unify_clause_ref(machine, predicate.clauses[index], head, body)) found.push_back(index);
    restore_state(machine, heap_mark, trail_mark, extras_mark);
  }
  return found;
}

void push_scan(Impl& machine, Pick pick, const Goal& goal, std::uint32_t name, std::uint16_t arity,
               const std::vector<std::size_t>& matches, std::size_t next, std::size_t heap_mark,
               std::size_t trail_mark) {
  Entry entry;
  entry.pick = pick;
  entry.candidates = matches;
  entry.next = next;
  entry.redo_goal = goal.at;
  entry.pred_name = name;
  entry.pred_arity = arity;
  entry.pred_home = machine.calling;
  // 水位は単一化の前。成功した項を含めると、やり直しで束縛が残る。
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

bool take_clause(Impl& machine, const Goal& goal, std::uint32_t name, std::uint16_t arity,
                 const std::vector<std::size_t>& matches, std::size_t begin) {
  const Predicate* predicate = find_predicate(machine, name, arity);
  if (predicate == nullptr) return false;
  const Cell head = goal.arg(machine, 0);
  const Cell body = goal.arg(machine, 1);
  for (std::size_t cursor = begin; cursor < matches.size(); ++cursor) {
    const auto index = matches[cursor];
    if (index >= predicate->clauses.size()) continue;
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    if (!unify_clause_ref(machine, predicate->clauses[index], head, body)) {
      restore_state(machine, heap_mark, trail_mark, extras_mark);
      continue;
    }
    if (cursor + 1 < matches.size()) {
      push_scan(machine, Pick::ClauseScan, goal, name, arity, matches, cursor + 1, heap_mark, trail_mark);
    }
    succeed(machine, goal);
    return true;
  }
  return false;
}

void run_clause(Impl& machine, const Goal& goal) {
  std::uint32_t home = 0;
  Cell term;
  if (!place_clause(machine, goal, machine.heap.deref(goal.arg(machine, 0)), home, term)) return;
  machine.heap.set_word(goal.at + kFrameHead, term.raw());
  [[maybe_unused]] const CallAs as{machine, home};
  HeadName indicator;
  if (!clause_head_ok(machine, goal, indicator)) return;
  const Predicate* predicate = find_predicate(machine, indicator.name, indicator.arity);
  // 席が取り込みだけなら、公開した側の節は見せない。
  if (predicate != nullptr && predicate->imported) {
    raise_permission(machine, goal, indicator.name, indicator.arity);
    return;
  }
  if (predicate == nullptr || predicate->clauses.empty()) {
    machine.failed = true;
    return;
  }
  const auto matches =
      matching_clauses(machine, *predicate, goal.arg(machine, 0), goal.arg(machine, 1));
  if (!take_clause(machine, goal, indicator.name, indicator.arity, matches, 0)) machine.failed = true;
}

struct WantedPred {
  bool name_set = false;
  bool arity_set = false;
  bool arity_high = false;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

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
    if (predicate.home != machine.calling) continue;
    if (wanted.name_set && predicate.name != wanted.name) continue;
    if (wanted.arity_set && predicate.arity != wanted.arity) continue;
    found.push_back(index);
  }
  return found;
}

bool take_pred(Impl& machine, const Goal& goal, const std::vector<std::size_t>& matches, std::size_t begin) {
  const Cell wanted = goal.arg(machine, 0);
  for (std::size_t cursor = begin; cursor < matches.size(); ++cursor) {
    const auto index = matches[cursor];
    if (index >= machine.predicates.size()) continue;
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const Predicate& predicate = machine.predicates[index];
    const Cell indicator = cell_functor(machine, predicate.name, predicate.arity);
    if (!do_unify(machine, wanted, indicator)) {
      restore_state(machine, heap_mark, trail_mark, extras_mark);
      continue;
    }
    if (cursor + 1 < matches.size()) {
      push_scan(machine, Pick::PredGen, goal, 0, 0, matches, cursor + 1, heap_mark, trail_mark);
    }
    succeed(machine, goal);
    return true;
  }
  return false;
}

void run_current_predicate(Impl& machine, const Goal& goal) {
  WantedPred wanted;
  if (!accept_indicator(machine, goal, goal.arg(machine, 0), wanted)) return;
  const auto matches = pred_matches(machine, wanted);
  if (!take_pred(machine, goal, matches, 0)) machine.failed = true;
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

bool redo_clause_scan(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::ClauseScan) return false;
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  machine.stack.pop_back();
  return take_clause(machine, read_goal(machine, top.redo_goal), top.pred_name, top.pred_arity, top.candidates,
                      top.next);
}

bool redo_pred_gen(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::PredGen) return false;
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  machine.stack.pop_back();
  return take_pred(machine, read_goal(machine, top.redo_goal), top.candidates, top.next);
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
  if (name == "dynamic") {
    run_dynamic(machine, goal);
    return true;
  }
  if (name == "current_predicate") {
    run_current_predicate(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
