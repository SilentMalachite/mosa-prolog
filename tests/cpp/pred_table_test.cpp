// レビュー M25（ADR 0018）。述語表の索引が、足す・途中を消す・まとめて消すの後も全部の述語を自分の位置で引き、
// 無い鍵では null を返し、serial の昇順を保つこと。読み込みの写しの鎖が、同じ鍵の写しだけを新しい順に辿ること。
#include "harness.hpp"

#include "machine/load_internal.hpp"
#include "machine/pred_table.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace {

using mosa::LoadSnapshots;
using mosa::PredicateTable;
using mosa::PredKey;
using mosa::Snapshot;
using mosa_test::check;

// 同じ名前を 2 つのモジュールと 3 つのアリティに散らし、鍵の一部だけが違う述語を並べる。
PredKey key_at(std::size_t at) {
  return PredKey{static_cast<std::uint32_t>(1 + at % 2), static_cast<std::uint32_t>(at / 6),
                 static_cast<std::uint16_t>(at / 2 % 3)};
}

void fill(PredicateTable& table, std::size_t count) {
  for (std::size_t at = 0; at < count; ++at) {
    mosa::Predicate made;
    const PredKey key = key_at(at);
    made.module = key.module;
    made.name = key.name;
    made.arity = key.arity;
    made.serial = at + 1;
    table.add(std::move(made));
  }
}

// 表のどの述語も自分の鍵で自分を引き、表は serial の昇順。
bool consistent(PredicateTable& table) {
  std::size_t previous = 0;
  for (mosa::Predicate& predicate : table) {
    if (table.find(mosa::key_of(predicate)) != &predicate || predicate.serial <= previous) return false;
    previous = predicate.serial;
  }
  return true;
}

bool holds(const PredicateTable& table, std::size_t at) {
  const mosa::Predicate* found = table.find(key_at(at));
  return found != nullptr && found->serial == at + 1;
}

void check_find() {
  PredicateTable table;
  fill(table, 3000);
  check(table.size() == 3000 && consistent(table), "every predicate is found at its place");
  check(table.index_slots() >= 2 * table.size() && table.index_slots() <= 4 * table.size(),
        "the index has two to four slots a predicate");
  bool all = true;
  for (std::size_t at = 0; at < 3000; ++at) all = all && holds(table, at);
  check(all, "every key gives its own predicate");
  check(table.find(PredKey{3, 0, 0}) == nullptr, "another module is not found");
  check(table.find(PredKey{1, 0, 7}) == nullptr, "another arity is not found");
  check(table.find(PredKey{1, 9999, 0}) == nullptr, "another name is not found");
}

void check_erase() {
  PredicateTable table;
  fill(table, 1000);
  table.erase(key_at(10));
  table.erase(key_at(0));
  table.erase(key_at(999));
  table.erase(key_at(10));
  table.erase(PredKey{7, 7, 7});
  check(table.size() == 997 && consistent(table), "erasing shifts the rest and keeps the index right");
  check(table.find(key_at(10)) == nullptr && table.find(key_at(0)) == nullptr && table.find(key_at(999)) == nullptr,
        "an erased key is not found");
  check(holds(table, 1) && holds(table, 11) && holds(table, 998), "the neighbours are still found");
  mosa::Predicate again;
  const PredKey key = key_at(10);
  again.module = key.module;
  again.name = key.name;
  again.arity = key.arity;
  again.serial = 5000;
  table.add(std::move(again));
  const mosa::Predicate* back = table.find(key);
  check(back != nullptr && back->serial == 5000 && consistent(table), "an erased key can be added again at the end");
}

// 線形探査の後ろ詰めが、消した枠の後ろに続く述語を探せる所へ寄せること。散らした順に 3 つに 2 つを消す。
void check_erase_many() {
  PredicateTable table;
  fill(table, 2000);
  std::vector<bool> gone(2000, false);
  for (std::size_t step = 0; step < 2000; ++step) {
    const std::size_t at = step * 7919 % 2000;
    if (at % 3 == 0) continue;
    table.erase(key_at(at));
    gone[at] = true;
  }
  bool right = true;
  for (std::size_t at = 0; at < 2000; ++at) right = right && (gone[at] ? table.find(key_at(at)) == nullptr : holds(table, at));
  check(right && consistent(table) && table.size() == 667, "scattered erases keep every other key reachable");
}

void check_erase_if() {
  PredicateTable table;
  fill(table, 1000);
  table.erase_if([](const mosa::Predicate& predicate) { return predicate.serial % 3 == 0 || predicate.serial < 5; });
  bool right = true;
  for (std::size_t at = 0; at < 1000; ++at) {
    const bool dropped = (at + 1) % 3 == 0 || at + 1 < 5;
    right = right && (dropped ? table.find(key_at(at)) == nullptr : holds(table, at));
  }
  check(right && consistent(table), "erase_if drops exactly the chosen predicates");
  table.erase_if([](const mosa::Predicate&) { return true; });
  check(table.empty() && table.find(key_at(1)) == nullptr, "erase_if can empty the table");
}

// 写しが増えて索引が伸びると、索引は列から作り直す。同じ鍵の写しが並んでも、作り直した索引は鍵ごとに最後の写しを
// 指し、鎖はその鍵の写しを新しい順に辿ること（M25 の敵対的レビュー C1。最初の 5 件では作り直しが起きない）。
void check_snapshot_growth() {
  LoadSnapshots shots;
  const auto key_of_step = [](std::size_t step) {
    return PredKey{static_cast<std::uint32_t>(1 + step % 2), static_cast<std::uint32_t>(step % 7), 1};
  };
  for (std::size_t step = 0; step < 300; ++step) shots.push_back(Snapshot(key_of_step(step), nullptr, step));
  bool right = true;
  for (std::size_t first = 0; first < 14; ++first) {
    std::vector<std::size_t> want;
    for (std::size_t step = 300; step-- > 0;) {
      if (key_of_step(step).module == key_of_step(first).module && step % 7 == first % 7) want.push_back(step);
    }
    std::vector<std::size_t> chain;
    const PredKey key = key_of_step(first);
    for (std::size_t at = shots.last_of(key); at != LoadSnapshots::kNone; at = shots.earlier(at)) chain.push_back(at);
    right = right && chain == want && shots[chain.front()].taken_at == chain.front();
  }
  check(right, "after the index grows, every chain still starts at the last snapshot of its key");
}

void check_snapshots() {
  // 鍵 a・b・a・c・a の順に写す。a の鎖は 4、2、0 と辿り、b と c は 1 つずつ。
  const PredKey a{1, 10, 1};
  const PredKey b{1, 10, 2};
  const PredKey c{2, 10, 1};
  LoadSnapshots shots;
  for (const PredKey key : std::vector<PredKey>{a, b, a, c, a}) shots.push_back(Snapshot(key, nullptr, 0));
  std::vector<std::size_t> chain;
  for (std::size_t at = shots.last_of(a); at != LoadSnapshots::kNone; at = shots.earlier(at)) chain.push_back(at);
  check(chain == std::vector<std::size_t>{4, 2, 0}, "the chain of a key walks its snapshots newest first");
  check(shots.last_of(b) == 1 && shots.earlier(1) == LoadSnapshots::kNone, "a key with one snapshot");
  check(shots.last_of(c) == 3 && shots[3].module == 2, "keys that differ in the module are apart");
  check(shots.last_of(PredKey{1, 11, 1}) == LoadSnapshots::kNone && shots.size() == 5, "a key without snapshots");
  std::vector<std::uint16_t> order;
  for (const Snapshot& shot : std::as_const(shots)) order.push_back(shot.arity);
  check(order == std::vector<std::uint16_t>{1, 2, 1, 1, 1}, "the list keeps the order of the snapshots");
}

}  // namespace

int main() {
  check_find();
  check_erase();
  check_erase_many();
  check_erase_if();
  check_snapshots();
  check_snapshot_growth();
  return mosa_test::failures();
}
