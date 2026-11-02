// 述語の節の列（ADR 0015）。Predicate::clauses を書くのはこのファイルだけにし、番号の範囲と生きた節の数と
// 第1引数の索引を書く所で一緒に保つ。選択点の候補は番号なので、asserta も削除も開いた走査を付け直さない。

#include "internal.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace mosa {
namespace {

std::size_t count_live(const ClauseList& clauses) {
  return static_cast<std::size_t>(
      std::count_if(clauses.begin(), clauses.end(), [](const Clause& clause) { return !clause.dead(); }));
}

// 記憶は返さない。捨てるのは読み直しと真ん中の削除で、どちらもすぐ後の呼び出しが同じくらいの索引を作り直す。
void drop_index(FirstArgIndex& index) {
  index.built = false;
  index.buckets.clear();
  index.keyless.clear();
}

// 節の鍵の組。鍵の無い節は keyless。at_back は末尾へ足すか。
void index_add(FirstArgIndex& index, const Clause& clause, bool at_back) {
  if (!index.built) return;
  const auto key = clause_key(clause);
  IdBucket& ids = key ? index.buckets[*key] : index.keyless;
  if (at_back) ids.push_back(clause.id);
  else ids.push_front(clause.id);
}

// 組の first なら先頭、last なら末尾の番号が id なら外す。どちらでも外せなければ偽。
bool pop_end(IdBucket& ids, std::size_t id, bool first, bool last) {
  return (first && ids.pop_front_id(id)) || (last && ids.pop_back_id(id));
}

// 節の列の先頭か末尾の節は、番号が最小か最大なので、その組（か keyless）の同じ端にある。真ん中の節は組の中を
// 探すことになるので、索引を捨てて次の束縛した呼び出しに作り直させる（真ん中を消すのは sweep_clauses だけで、
// そこでも捨てる）。端に無いのは索引が節の列と食い違うときだけで、そのときも捨てれば作り直した索引は必ず合う。
void index_remove(Predicate& predicate, std::size_t position) {
  FirstArgIndex& index = predicate.index;
  if (!index.built) return;
  const Clause& clause = predicate.clauses[position];
  const bool first = position == 0;
  const bool last = position + 1 == predicate.clauses.size();
  const auto key = clause_key(clause);
  if (!key) {
    if (!pop_end(index.keyless, clause.id, first, last)) drop_index(index);
    return;
  }
  const auto found = index.buckets.find(*key);
  if (found == index.buckets.end() || !pop_end(found->second, clause.id, first, last)) {
    drop_index(index);
    return;
  }
  // 鍵ごとに足して消す使い方（待ち行列など）で、空の組が溜まらないようにする。
  if (found->second.empty()) index.buckets.erase(found);
}

}  // namespace

// 足す節はいつも生きた節として数える。写した節に died が残っていても、足した世代からは見える節にする。
void append_clause(Predicate& predicate, Clause clause, std::uint64_t born) {
  clause.id = predicate.next_id++;
  clause.born = born;
  clause.died = kAlive;
  ++predicate.live;
  predicate.clauses.push_back(std::move(clause));
  index_add(predicate.index, predicate.clauses.back(), true);
}

void prepend_clause(Predicate& predicate, Clause clause, std::uint64_t born) {
  clause.id = --predicate.first_id;
  clause.born = born;
  clause.died = kAlive;
  ++predicate.live;
  predicate.clauses.push_front(std::move(clause));
  index_add(predicate.index, predicate.clauses.front(), false);
}

// 戻す写しは前の番号と世代のまま持つ。開いた走査の候補とカーソルがその番号を指したままで、どの節が見えるかも
// 世代で決まるので、振り直さない。写しは死んだ節を含みうるので、生きた節は数え直し、死んだ節は掃除の数に入れる。
void replace_clauses(Predicate& predicate, ClauseList clauses) {
  predicate.clauses = std::move(clauses);
  predicate.live = count_live(predicate.clauses);
  predicate.dead_since = predicate.clauses.size() - predicate.live;
  predicate.held_dead = 0;
  drop_index(predicate.index);
  if (predicate.clauses.empty()) return;
  // 述語を作り直した後に戻すと、写しの番号が今の範囲の外にありうる。広げておけば次の番号と重ならない。
  predicate.first_id = std::min(predicate.first_id, predicate.clauses.front().id);
  predicate.next_id = std::max(predicate.next_id, predicate.clauses.back().id + 1);
}

// 範囲は戻さない。置き換えの後で足す節が、消した節の番号を使い回さない。
void clear_clauses(Predicate& predicate) {
  predicate.clauses.clear();
  predicate.live = 0;
  predicate.dead_since = 0;
  predicate.held_dead = 0;
  drop_index(predicate.index);
}

void erase_clause_id(Predicate& predicate, std::size_t id) {
  const auto position = clause_position(predicate, id);
  if (!position) return;
  index_remove(predicate, *position);
  const auto at = predicate.clauses.begin() + static_cast<std::ptrdiff_t>(*position);
  if (!at->dead()) --predicate.live;
  predicate.clauses.erase(at);
}

// 残す節を前へ詰めて末尾を一度に切るので、消す節がいくつでも 1 回は節の数に比例する。番号は変わらないので、
// 開いた走査の候補とカーソルは付け直さない。真ん中から消すと索引の組の中を探すことになるので、索引は捨てる。
// 残した死んだ節を数え直しに入れないと、走査が閉じた後も次の半分の削除まで残り、未束縛の呼び出しがなめ続ける。
void sweep_clauses(Predicate& predicate, const std::vector<bool>& held, ScanRecord holder) {
  ClauseList& clauses = predicate.clauses;
  std::size_t kept = 0;
  std::size_t kept_dead = 0;
  for (std::size_t position = 0; position < clauses.size(); ++position) {
    if (clauses[position].dead() && !held[position]) continue;
    if (clauses[position].dead()) ++kept_dead;
    if (kept != position) clauses[kept] = std::move(clauses[position]);
    ++kept;
  }
  predicate.dead_since = kept_dead;
  predicate.held_dead = kept_dead;
  predicate.held_by = holder;
  if (kept == clauses.size()) return;
  clauses.erase(clauses.begin() + static_cast<std::ptrdiff_t>(kept), clauses.end());
  drop_index(predicate.index);
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
