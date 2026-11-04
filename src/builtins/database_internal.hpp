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

// retract と clause/2 の走査（ADR 0015 とその改訂）。候補は列にせず、第1引数の鍵（未束縛なら Any）で次に合う節を
// 探しながらカーソルで進む。どちらも次の答えの節があるときだけ選択点を積む。

// 頭部の第1引数が束縛されていれば、その引数（deref 済み）。アリティ 0 の頭部と未束縛の引数は nullopt。
[[nodiscard]] std::optional<Cell> bound_first(const Impl& machine, Cell head);
// 頭部 head の走査の鍵。第1引数が束縛されていなければ Any。
[[nodiscard]] IndexKey head_key(const Impl& machine, Cell head);
// 走査の枠の雛形。カーソルは呼び手の一歩が入れ、水位は push_if_more が入れる。
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

// walk の次の候補のうち、match が合う最初の節の位置。無ければ clauses.size()。束縛は戻す。
template <typename Match>
std::size_t next_match(Impl& machine, const Predicate& predicate, ClauseWalk& walk, const Match& match) {
  for (std::size_t at = walk.next(); at < predicate.clauses.size(); at = walk.next()) {
    if (try_match(machine, predicate.clauses[at], match)) return at;
  }
  return predicate.clauses.size();
}

// 一歩。合う節を探し、次に合う節を先に試して scan.cursor にする（無ければ kNoClause）。先に試すのは束縛する前なので、
// 今の答えの束縛と混ざらない。やり直しでカーソルの節がまだ使えれば、前の一歩で同じ状態から試して合った節なので
// 試し直さない（歩みの最初の候補がその節になる）。合う節は 2 回（先読みと束縛）、合わない節は 1 回だけ単一化する。
// 今の答えにする節の位置を返し、束縛はしない。redo はやり直しの一歩か。scan.next は組の添字の手掛かり。
template <typename Match>
std::optional<std::size_t> step_scan(Impl& machine, const Predicate& predicate, Entry& scan, bool redo, ScanView view,
                                     const Match& match) {
  const ClauseList& clauses = predicate.clauses;
  const std::size_t from = redo ? scan.cursor : 0;
  const std::size_t at = clause_from(predicate, from);
  const bool known =
      redo && at < clauses.size() && clauses[at].id == scan.cursor && scan_usable(clauses[at], view);
  ClauseWalk walk(predicate, scan.key, from, view, redo ? scan.next : kNoHint);
  const std::size_t position = known ? walk.next() : next_match(machine, predicate, walk, match);
  if (position >= clauses.size()) {
    scan.cursor = kNoClause;
    return std::nullopt;
  }
  const std::size_t after = next_match(machine, predicate, walk, match);
  scan.cursor = after < clauses.size() ? clauses[after].id : kNoClause;
  scan.next = walk.hint();
  return position;
}

}
