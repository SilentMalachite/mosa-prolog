#pragma once

// database.cpp と database_scan.cpp だけが共有する。ほかの組込からは使わない。
#include "../machine/internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace mosa::database {

struct HeadName {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

[[nodiscard]] bool callable_term(const Impl& machine, Cell cell);
void raise_instantiation(Impl& machine, const Goal& goal);
void raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit);
void succeed(Impl& machine, const Goal& goal);
[[nodiscard]] HeadName head_indicator(Impl& machine, Cell head);
// 引数の M: を外し、行き先のモジュールを返す。偽なら誤りを投げ済み。M: が無ければ文脈モジュール。
[[nodiscard]] bool strip_target(Impl& machine, const Goal& goal, Cell& term, std::uint32_t& module, bool clause);
void run_clause(Impl& machine, const Goal& goal);
void run_current_predicate(Impl& machine, const Goal& goal);

// retract と clause/2 の走査（ADR 0015）。第1引数を束縛した呼び出しは key_candidates の候補から合う節の番号の列を
// 作り、未束縛なら列を作らずカーソルで進む。どちらも次の答えの節があるときだけ選択点を積む。

// 頭部の第1引数が束縛されていれば、その引数（deref 済み）。アリティ 0 の頭部と未束縛の引数は nullopt。
[[nodiscard]] std::optional<Cell> bound_first(const Impl& machine, Cell head);
// 走査の枠の雛形。候補の列かカーソルは呼び手が入れ、水位は push_if_more が入れる。
[[nodiscard]] Entry scan_entry(Pick pick, const Goal& goal, PredKey key, std::uint64_t generation);
// まだ試す節があれば、今の水位で枠を積む。束縛の前に呼ぶので、やり直しは今の答えの束縛を戻してから次を試す。
void push_if_more(Impl& machine, Entry&& scan);

// 節を試しに単一化し、束縛を戻して合ったかを返す。
template <typename Match>
bool try_match(Impl& machine, const Clause& clause, const Match& match) {
  const auto heap_mark = machine.heap.size();
  const auto trail_mark = machine.trail.slots.size();
  const auto extras_mark = machine.extras.size();
  const bool matched = match(clause);
  restore_state(machine, heap_mark, trail_mark, extras_mark);
  return matched;
}

// ids の節のうち合う節の番号を、順を保って返す。合う節だけを列にするので、列に次があるかで次の答えがあるかが
// 分かる（今と同じ true. と true ;）。
template <typename Match>
IdList collect_ids(Impl& machine, const Predicate& predicate, const IdList& ids, const Match& match) {
  IdList found = id_list(machine);
  for (const std::size_t id : ids) {
    const auto position = clause_position(predicate, id);
    if (position && try_match(machine, predicate.clauses[*position], match)) found.push_back(id);
  }
  return found;
}

// 位置 from から先で、usable かつ match が合う最初の節の位置。無ければ clauses.size()。束縛は戻す。
template <typename Usable, typename Match>
std::size_t find_match(Impl& machine, const Predicate& predicate, std::size_t from, const Usable& usable,
                       const Match& match) {
  for (; from < predicate.clauses.size(); ++from) {
    const Clause& clause = predicate.clauses[from];
    if (usable(clause) && try_match(machine, clause, match)) return from;
  }
  return predicate.clauses.size();
}

// 列の一歩。列は合う節だけなので、使える最初の節を試さずに答えにする。使えない候補を先に飛ばしておき、列に次が
// あるかで次の答えがあるかが分かるようにする。消された節は戻らないので、飛ばした候補を後で試すことは無い。
template <typename Usable>
std::optional<std::size_t> step_list(const Predicate& predicate, Entry& scan, const Usable& usable) {
  const auto usable_at = [&predicate, &usable](std::size_t id) -> std::optional<std::size_t> {
    const auto position = clause_position(predicate, id);
    if (position && usable(predicate.clauses[*position])) return position;
    return std::nullopt;
  };
  std::optional<std::size_t> position;
  while (!position && scan.next < scan.candidates.size()) position = usable_at(scan.candidates[scan.next++]);
  while (scan.next < scan.candidates.size() && !usable_at(scan.candidates[scan.next])) ++scan.next;
  return position;
}

// カーソルの一歩（未束縛の第1引数）。合う節を探し、次に合う節を先に試して scan.cursor にする（無ければ kNoClause）。
// 先に試すのは束縛する前なので、今の答えの束縛と混ざらない。やり直しでカーソルの節がまだ使えれば、前の一歩で
// 同じ状態から試して合った節なので試し直さない。合う節は 2 回（先読みと束縛）、合わない節は 1 回だけ単一化する。
template <typename Usable, typename Match>
std::optional<std::size_t> step_cursor(Impl& machine, const Predicate& predicate, Entry& scan, bool redo,
                                       const Usable& usable, const Match& match) {
  const ClauseList& clauses = predicate.clauses;
  std::size_t position = redo ? clause_from(predicate, scan.cursor) : 0;
  const bool known =
      redo && position < clauses.size() && clauses[position].id == scan.cursor && usable(clauses[position]);
  if (!known) position = find_match(machine, predicate, position, usable, match);
  if (position >= clauses.size()) {
    scan.cursor = kNoClause;
    return std::nullopt;
  }
  const std::size_t after = find_match(machine, predicate, position + 1, usable, match);
  scan.cursor = after < clauses.size() ? clauses[after].id : kNoClause;
  return position;
}

// 今の答えにする節の位置を返し、scan を次の答えまで進める。束縛はしない。redo はやり直しの一歩か。
template <typename Usable, typename Match>
std::optional<std::size_t> step_scan(Impl& machine, const Predicate& predicate, Entry& scan, bool redo,
                                     const Usable& usable, const Match& match) {
  if (scan.by_cursor) return step_cursor(machine, predicate, scan, redo, usable, match);
  return step_list(predicate, scan, usable);
}

}
