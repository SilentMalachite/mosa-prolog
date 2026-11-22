// clause/2 と current_predicate/1 は database_scan.cpp にある。
// 8.9.1 asserta/1 (+clause)
// 8.9.2 assertz/1 (+clause)
//   instantiation_error, type_error(callable), permission_error(modify, static_procedure, PI)
// 8.9.3 retract/1 (+clause)
//   上と同じ誤り。解は節の順で、消した節は失敗しても戻さない。
// 8.9.4 abolish/1 (+predicate_indicator)
//   instantiation_error, type_error(predicate_indicator), type_error(atom), type_error(integer),
//   domain_error(not_less_than_zero), representation_error(max_arity),
//   permission_error(modify, static_procedure, PI)。項数の Big（フェーズ10）は負なら定義域、正なら表現の誤り。
// 8.9.5 retractall/1 (@callable)（Cor.2）
//   instantiation_error, type_error(callable), permission_error(modify, static_procedure, PI)
// 7.4.2 dynamic/1 は指示だけ。読み込み側で受ける。
// テスト: tests/pl/database.pl, tests/pl/clause.pl, tests/pl/bigint.pl

#include "database_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
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

std::optional<Cell> bound_first(const Impl& machine, Cell head) {
  head = machine.heap.deref(head);
  Cell first{};
  if (head.tag() == Tag::Lis) first = machine.heap.list_head(head);
  else if (head.tag() == Tag::Str && machine.heap.functor_arity(head) >= 1) first = machine.heap.arg(head, 0);
  else return std::nullopt;
  first = machine.heap.deref(first);
  if (machine.heap.unbound(first)) return std::nullopt;
  return first;
}

IndexKey head_key(const Impl& machine, Cell head) {
  const auto first = bound_first(machine, head);
  return first ? first_key(machine, *first) : IndexKey{};
}

Entry scan_entry(Pick pick, const Goal& goal, PredKey key, std::uint64_t generation) {
  Entry entry;
  entry.pick = pick;
  entry.generation = generation;
  entry.redo_goal = goal.at;
  entry.pred_module = key.module;
  entry.pred_name = key.name;
  entry.pred_arity = key.arity;
  return entry;
}

// 枠は移すだけにし、やり直すたびに写さない（レビュー M3）。
void push_if_more(Impl& machine, Entry&& scan) {
  if (scan.cursor == kNoClause) return;
  scan.heap_mark = machine.heap.size();
  scan.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(scan));
}

}  // namespace database

using database::HeadName;
using database::strip_target;
using database::run_clause;
using database::run_current_predicate;
using database::callable_term;
using database::erase_predicate;
using database::head_indicator;
using database::head_key;
using database::kill_clause;
using database::purge_dead;
using database::push_if_more;
using database::raise_instantiation;
using database::raise_type;
using database::scan_entry;
using database::step_scan;
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
  // 7.6.2 は変数の本体を call/1 に変える。8.9.1.3 の具体化エラーは頭部だけ。
  // retract((H :- B)) の B は、clause/2 と同じく変数を call/1 に包んだ本体と単一化する（unify_stored）。
  if (machine.heap.unbound(view.body)) return false;
  if (!callable_term(machine, view.body)) {
    raise_type(machine, goal, "callable", view.body);
    return true;
  }
  return false;
}

// 数はゴールにならない。変数と匿名は call できるので、ここでは許す。
bool goal_form(const IrTerm& term) {
  return goal_leaves_all(term, [](IrTag tag) { return !is_number_ir(tag); });
}

// abolish 済みの述語へ足すときは、同じ枠を新しい動的な述語として使い直す。番号の範囲も続くので、開いた走査の
// 番号が新しい定義の節を指さない。
void revive(Impl& machine, Predicate& predicate) {
  if (!predicate.abolished) return;
  begin_definition(machine, predicate, true);
  predicate.abolished = false;
  predicate.dynamic = true;
  predicate.dynamic_declared = false;
  predicate.meta.clear();
  adopt_meta(machine, predicate);
}

void abolish_open(Impl& machine, Predicate& predicate) {
  const PredKey key{predicate.module, predicate.name, predicate.arity};
  retire_clauses(machine, predicate);
  if (predicate.clauses.empty()) {
    erase_predicate(machine, key.module, key.name, key.arity);
    return;
  }
  // 枠を残しても定義は無くなる。失敗した読み込みが、abolish の前の写しをこの枠へ戻さないようにする（ADR 0017）。
  begin_definition(machine, predicate, true);
  predicate.abolished = true;
}

bool unify_stored(Impl& machine, const Clause& clause, const ClauseView& view) {
  // 呼び出し側の変数は、この関数が足す項より前にある。候補ごとに束縛を戻す水位。
  const auto floor = machine.heap.size();
  std::vector<Cell> env = clause_env(machine, clause);
  // 共有の項（ADR 0022）は頭と本体がそれぞれ組む。本体の分を先に組むと、頭の合わない節や retractall/1 でも本体の
  // 大きさだけかかる（M30 の Codex レビュー 2 回目）。
  if (!unify_visible(machine, floor, view.head, materialize(machine, clause.head, env, clause.shared.get()))) {
    return false;
  }
  if (view.head_only) return true;
  // retract(Head) は本体 true の節だけを消す。他の規則の本体を組んでから比べると、本体の大きさだけ無駄になる。
  if (!view.rule && clause.body->tag != IrTag::True) return false;
  // 8.9.3 は clause/2 と同じ本体と単一化する。IR のままだと変数の本体がどの本体とも合う（7.6.2）。
  // env はもう全スロットを持つので、形が変わらない本体は ir_to_term の走査を省いて直接組む。
  const Cell body = shown_differs(*clause.body, true)
                        ? ir_to_term(machine, *clause.body, env, true, shared_terms(clause))
                        : materialize(machine, *clause.body, env, clause.shared.get());
  if (!view.rule) {
    // 事実の正規形は本体 true。規則は retract(Head) では消さない。
    return unify_visible(machine, floor, machine.heap.atom(machine.atoms.intern("true")), body);
  }
  return unify_visible(machine, floor, view.body, body);
}

// 第1引数を束縛した retract はその鍵の候補だけを、未束縛ならどの節もカーソルで歩く（ADR 0015 とその改訂）。
Entry retract_scan(Impl& machine, const Goal& goal, PredKey key, const ClauseView& view) {
  Entry scan = scan_entry(Pick::Retract, goal, key, machine.db_generation);
  scan.key = head_key(machine, view.head);
  return scan;
}

// scan.generation は最初の呼び出しの世代で、やり直しても進めない。次に合う節があれば、束縛する前に scan を積む。
// unify_stored は水位より前の変数の束縛を全部トレイルに残すので、やり直しはこの答えの束縛を戻してから次を試す。
bool take_retract(Impl& machine, const Goal& goal, Entry scan, bool redo) {
  const PredKey key{scan.pred_module, scan.pred_name, scan.pred_arity};
  Predicate* predicate = find_pred(machine, key);
  if (predicate == nullptr) return false;
  // 引数は最初の呼び出しで検査済み。M: は外すだけにし、節を作り直してヒープを使わない。
  ClauseView view = clause_view(machine, skip_module(machine, goal.arg(machine, 0)));
  view.head = named_head(machine, skip_module(machine, view.head), key.name);
  const ScanView usable{scan.generation, true};
  const auto match = [&machine, &view](const Clause& clause) { return unify_stored(machine, clause, view); };
  const auto position = step_scan(machine, *predicate, scan, redo, usable, match);
  if (!position) return false;
  const std::size_t id = predicate->clauses[*position].id;
  push_if_more(machine, std::move(scan));
  if (!unify_stored(machine, predicate->clauses[*position], view)) return false;
  kill_clause(machine, *predicate, key, id);
  succeed(machine, goal);
  return true;
}

void run_assert(Impl& machine, const Goal& goal, bool at_front) {
  Cell term = goal.arg(machine, 0);
  std::uint32_t module = 0;
  if (!strip_target(machine, goal, term, module, true)) return;
  if (reject_clause(machine, goal, term)) return;
  const ClauseView view = clause_view(machine, term);
  const HeadName head = head_indicator(machine, view.head);
  // 別名で取り込んだ述語へ足す節は、取り込み元の名前の頭で持つ。検査と誤りは呼び手の書いた名前で出す。
  std::uint32_t source_name = head.name;
  Predicate* existing = purge_dead(machine, owned_pred(machine, module, source_name, head.arity));
  term = named_head(machine, term, source_name);
  Clause clause;
  // 呼び出し後の束縛が節へ漏れないよう、データベースに見える前に IR の変数番号へ切る。
  if (!compile_asserted(machine.heap, term, clause)) {
    raise_type(machine, goal, "callable", view.head);
    return;
  }
  // 8.9.1.3 は、非公開より先に本体がゴールへ変換できるかを見る。
  if (view.rule && !goal_form(*clause.body)) {
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
    created.defined_by_goal = true;
    existing = &add_predicate(machine, std::move(created));
    adopt_meta(machine, *existing);
  }
  // 番号で持つ開いた走査は、先頭に足しても付け直さない。新しい番号はどの候補にも無く、カーソルより後ろに
  // 足した節も世代が走査より新しいので、走査は見ない。
  if (at_front) prepend_clause(*existing, std::move(clause), ++machine.db_generation);
  else append_clause(*existing, std::move(clause), ++machine.db_generation);
  existing->goal_changed_at = machine.db_generation;
  succeed(machine, goal);
}

void run_retract(Impl& machine, const Goal& goal) {
  Cell term = goal.arg(machine, 0);
  std::uint32_t module = 0;
  if (!strip_target(machine, goal, term, module, true)) return;
  if (reject_clause(machine, goal, term)) return;
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
  if (!take_retract(machine, goal, retract_scan(machine, goal, key, source), false)) machine.failed = true;
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
  // 消すのは全部済ませてからなので、試すのは今生きている節（世代は今のもの）。第1引数を束縛していれば、その鍵の
  // 候補だけを歩く。
  const auto match = [&machine, &view](const Clause& clause) { return unify_stored(machine, clause, view); };
  IdList matches = id_list(machine);
  ClauseWalk walk(*predicate, head_key(machine, view.head), 0, ScanView{machine.db_generation, true});
  for (std::size_t at = walk.next(); at < predicate->clauses.size(); at = walk.next()) {
    if (database::try_match(machine, predicate->clauses[at], match)) matches.push_back(predicate->clauses[at].id);
  }
  // 後ろから消すと、全部消すときはどの節も消す時点で末尾にあり、すぐ消える。節を消しても述語は表に残る。
  for (std::size_t cursor = matches.size(); cursor > 0; --cursor) {
    kill_clause(machine, *predicate, key, matches[cursor - 1]);
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
  if (!is_integer_tag(arity.tag())) {
    raise_type(machine, goal, "integer", arity);
    return false;
  }
  // Big は int64 の外なので、負なら負の int64 と、正なら上限を越える int64 と同じ誤りになる。
  const bool big = arity.tag() == Tag::Big;
  if (integer_negative(machine, arity)) {
    raise_formal(machine, formal_domain(machine, "not_less_than_zero", arity), goal.name, goal.arity);
    return false;
  }
  if (big || machine.heap.integer_value(arity) > 1024) {
    raise_formal(machine, formal_representation(machine, "max_arity"), goal.name, goal.arity);
    return false;
  }
  indicator.name = static_cast<std::uint32_t>(name.raw() >> 3);
  indicator.arity = static_cast<std::uint16_t>(machine.heap.integer_value(arity));
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

bool redo_retract(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::Retract) return false;
  apply_catches(machine, machine.stack.back());
  // 枠を外す前に枠ごと移す。候補の列を写すと、消すたびに残りの候補の数だけかかる（レビュー M3）。
  Entry scan = std::move(machine.stack.back());
  machine.stack.pop_back();
  const Goal goal = read_goal(machine, scan.redo_goal);
  return take_retract(machine, goal, std::move(scan), true);
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
