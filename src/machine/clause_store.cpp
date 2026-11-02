// 述語の節の列（ADR 0015）。Predicate::clauses を書くのはこのファイルだけにし、番号の範囲と生きた節の数を
// 書く所で一緒に保つ。選択点の候補は番号なので、asserta も削除も開いた走査を付け直さない。

#include "internal.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>

namespace mosa {
namespace {

std::size_t count_live(const ClauseList& clauses) {
  return static_cast<std::size_t>(
      std::count_if(clauses.begin(), clauses.end(), [](const Clause& clause) { return !clause.dead(); }));
}

}  // namespace

// 足す節はいつも生きた節として数える。写した節に died が残っていても、足した世代からは見える節にする。
void append_clause(Predicate& predicate, Clause clause, std::uint64_t born) {
  clause.id = predicate.next_id++;
  clause.born = born;
  clause.died = kAlive;
  ++predicate.live;
  predicate.clauses.push_back(std::move(clause));
}

void prepend_clause(Predicate& predicate, Clause clause, std::uint64_t born) {
  clause.id = --predicate.first_id;
  clause.born = born;
  clause.died = kAlive;
  ++predicate.live;
  predicate.clauses.push_front(std::move(clause));
}

// 戻す写しは前の番号と世代のまま持つ。開いた走査の候補とカーソルがその番号を指したままで、どの節が見えるかも
// 世代で決まるので、振り直さない。写しは死んだ節を含みうるので、生きた節は数え直す。
void replace_clauses(Predicate& predicate, ClauseList clauses) {
  predicate.clauses = std::move(clauses);
  predicate.live = count_live(predicate.clauses);
  if (predicate.clauses.empty()) return;
  // 述語を作り直した後に戻すと、写しの番号が今の範囲の外にありうる。広げておけば次の番号と重ならない。
  predicate.first_id = std::min(predicate.first_id, predicate.clauses.front().id);
  predicate.next_id = std::max(predicate.next_id, predicate.clauses.back().id + 1);
}

// 範囲は戻さない。置き換えの後で足す節が、消した節の番号を使い回さない。
void clear_clauses(Predicate& predicate) {
  predicate.clauses.clear();
  predicate.live = 0;
}

void erase_clause_id(Predicate& predicate, std::size_t id) {
  const auto position = clause_position(predicate, id);
  if (!position) return;
  const auto at = predicate.clauses.begin() + static_cast<std::ptrdiff_t>(*position);
  if (!at->dead()) --predicate.live;
  predicate.clauses.erase(at);
}

std::optional<std::size_t> clause_position(const Predicate& predicate, std::size_t id) {
  const std::size_t position = clause_from(predicate, id);
  if (position >= predicate.clauses.size() || predicate.clauses[position].id != id) return std::nullopt;
  return position;
}

// 真ん中から消していなければ番号は詰まっていて、先頭からの差がそのまま位置になる。番号は狭義に増えるので、
// 先頭からの差の位置の節の番号は id 以上で、答えはそこを超えない。外れたらそこまでを二分探索する。
std::size_t clause_from(const Predicate& predicate, std::size_t id) {
  const ClauseList& clauses = predicate.clauses;
  if (clauses.empty() || id <= clauses.front().id) return 0;
  if (id > clauses.back().id) return clauses.size();
  const std::size_t guess = id - clauses.front().id;
  if (guess < clauses.size() && clauses[guess].id == id) return guess;
  const auto end = clauses.begin() + static_cast<std::ptrdiff_t>(std::min(guess, clauses.size()));
  const auto found = std::lower_bound(clauses.begin(), end, id,
                                      [](const Clause& clause, std::size_t wanted) { return clause.id < wanted; });
  return static_cast<std::size_t>(std::distance(clauses.begin(), found));
}

// 見えない節は、走査より後に assertz した節（末尾に並ぶので、走査の終わりに一度だけ飛ばす）と、走査より前に
// 消して古い走査のために残る節だけなので、一つずつ飛ばす。
std::size_t next_visible(const Predicate& predicate, std::size_t from, std::uint64_t generation) {
  const ClauseList& clauses = predicate.clauses;
  while (from < clauses.size() && !visible(clauses[from], generation)) ++from;
  return from;
}

std::size_t live_count(const Predicate& predicate) { return predicate.live; }

}  // namespace mosa
