// レビュー M21（ADR 0015 の改訂 2）。小さな列は長さぴったりの vector に置くので、1 節の述語が持つ場所は 1 つで
// 済むこと。kVectorLimit を超えた列は deque へ移り、半分まで減れば戻ること。両端の追加と削除、縮め、写しが、
// 節の順と番号を保つこと。容量の検査は reserve(n) がちょうど n を取ることに頼る。規格は n 以上しか約束しないが、
// CI の 3 つの標準ライブラリ（libc++、libstdc++、MSVC）はどれもちょうど n を取る。
#include "harness.hpp"

#include "machine/clause_list.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace {

using mosa::Clause;
using mosa::ClauseList;
using mosa_test::check;

constexpr std::size_t kLimit = ClauseList::kVectorLimit;

Clause numbered(std::size_t id) {
  Clause clause;
  clause.id = id;
  return clause;
}

std::vector<std::size_t> ids(const ClauseList& list) {
  std::vector<std::size_t> out;
  for (const Clause& clause : list) out.push_back(clause.id);
  return out;
}

// 添字と反復が同じ節を同じ順に返し、番号が狭義に増えること。
bool consistent(const ClauseList& list) {
  const auto seen = ids(list);
  if (seen.size() != list.size()) return false;
  for (std::size_t at = 0; at < seen.size(); ++at) {
    if (list[at].id != seen[at]) return false;
    if (at > 0 && seen[at - 1] >= seen[at]) return false;
  }
  return seen.empty() || (list.front().id == seen.front() && list.back().id == seen.back());
}

void check_one_clause() {
  ClauseList appended;
  appended.push_back(numbered(1));
  check(!appended.chunked() && appended.capacity() == 1, "one assertz holds one slot");
  ClauseList prepended;
  prepended.push_front(numbered(1));
  check(!prepended.chunked() && prepended.capacity() == 1, "one asserta holds one slot");
  // 1 節の数え上げ（retract して assertz し直す）は、場所を手放さずに使い直す。
  appended.erase(appended.begin());
  check(appended.empty() && appended.capacity() == 1, "an emptied one-clause list keeps its slot");
  appended.push_back(numbered(2));
  check(ids(appended) == std::vector<std::size_t>{2} && appended.capacity() == 1, "the slot is used again");
}

void check_both_ends() {
  ClauseList list;
  for (std::size_t step = 0; step < 1000; ++step) {
    list.push_back(numbered(1000 + step));
    list.push_front(numbered(999 - step));
  }
  check(list.size() == 2000 && consistent(list), "asserta and assertz keep clause order");
  check(list.front().id == 0 && list.back().id == 1999, "both ends hold the extreme ids");
}

void check_modes() {
  // kVectorLimit 節までは vector で、余りは節の半分と 1 つを超えない。超えたら deque へ移る。
  ClauseList list;
  bool small = true;
  for (std::size_t step = 0; step < kLimit; ++step) {
    list.push_back(numbered(step));
    small = small && !list.chunked() && list.capacity() * 2 <= list.size() * 3 + 2;
  }
  check(small && list.capacity() == kLimit, "a small list stays one tight vector");
  list.push_back(numbered(kLimit));
  check(list.chunked() && consistent(list) && list.size() == kLimit + 1, "a long list moves to the deque");
  list.erase(list.begin(), list.begin() + static_cast<std::ptrdiff_t>(kLimit / 2));
  check(list.chunked() && list.size() == kLimit / 2 + 1, "above half the limit the list stays chunked");
  list.erase(list.end() - 1);
  check(!list.chunked() && list.capacity() == kLimit / 2 && consistent(list), "at half the limit it moves back");
  check(list.front().id == kLimit / 2 && list.back().id == kLimit - 1, "moving back keeps the clauses");
  ClauseList prepended;
  for (std::size_t step = 0; step <= kLimit; ++step) prepended.push_front(numbered(1000 - step));
  check(prepended.chunked() && consistent(prepended), "asserta moves a long list to the deque too");
}

void check_queue() {
  // 待ち行列（末尾へ足して先頭を消す）でも、vector は小さいまま。
  ClauseList list;
  bool bounded = true;
  for (std::size_t step = 0; step < 10000; ++step) {
    list.push_back(numbered(step));
    if (step >= 3) list.erase(list.begin());
    bounded = bounded && !list.chunked() && list.capacity() <= 8;
  }
  check(ids(list) == std::vector<std::size_t>{9997, 9998, 9999}, "queue keeps the newest clauses");
  check(bounded, "queue keeps a small vector");
}

void check_shrink() {
  ClauseList back;
  for (std::size_t step = 0; step < 4096; ++step) back.push_back(numbered(step));
  back.erase(back.begin() + 1, back.end());
  check(ids(back) == std::vector<std::size_t>{0} && !back.chunked() && back.capacity() == 1,
        "erasing the tail gives memory back");
  ClauseList front;
  for (std::size_t step = 0; step < 4096; ++step) front.push_back(numbered(step));
  front.erase(front.begin(), front.end() - 1);
  check(ids(front) == std::vector<std::size_t>{4095} && !front.chunked() && front.capacity() == 1,
        "erasing the head gives memory back");
  ClauseList middle;
  for (std::size_t step = 0; step < 10; ++step) middle.push_back(numbered(step));
  middle.erase(middle.begin() + 3);
  check(middle.size() == 9 && consistent(middle) && middle[3].id == 4, "erasing a middle clause closes the gap");
  ClauseList big;
  for (std::size_t step = 0; step < 100; ++step) big.push_back(numbered(step));
  big.erase(big.begin() + 40, big.begin() + 60);
  check(big.size() == 80 && consistent(big) && big[40].id == 60, "erasing a middle range of a deque");
  middle.erase(middle.begin(), middle.end());
  check(middle.empty() && middle.capacity() == 0, "erasing every clause gives the vector back");
  middle.push_front(numbered(7));
  check(ids(middle) == std::vector<std::size_t>{7}, "an emptied list takes clauses again");
}

void check_iterators() {
  ClauseList list;
  for (std::size_t step = 0; step < 100; ++step) list.push_back(numbered(step * 2));
  const ClauseList& view = list;
  const auto found = std::lower_bound(view.begin(), view.end(), std::size_t{81},
                                      [](const Clause& clause, std::size_t id) { return clause.id < id; });
  check(found - view.begin() == 41 && found->id == 82, "lower_bound walks the positions");
  ClauseList::const_iterator from_mutable = list.begin() + 5;
  check(from_mutable == view.begin() + 5 && (view.end() - 1)->id == 198, "iterators compare by position");
}

void check_copies() {
  ClauseList small;
  for (std::size_t step = 0; step < 20; ++step) small.push_back(numbered(step));
  small.erase(small.begin(), small.begin() + 5);
  const ClauseList small_copy = small;
  check(ids(small_copy) == ids(small) && small_copy.capacity() == small_copy.size(), "a copy holds the clauses only");
  ClauseList list;
  for (std::size_t step = 0; step < 100; ++step) {
    list.push_front(numbered(1000 - step));
    list.push_back(numbered(1001 + step));
  }
  list.erase(list.begin(), list.begin() + 50);
  ClauseList copy = list;
  check(ids(copy) == ids(list) && consistent(copy), "a copy has the same clauses");
  ClauseList assigned;
  assigned.push_back(numbered(5000));
  assigned = copy;
  check(ids(assigned) == ids(copy), "copy assignment replaces the clauses");
  ClauseList moved = std::move(list);
  check(ids(moved) == ids(copy), "a move keeps the clauses");
  check(list.empty() && !list.chunked() && list.capacity() == 0, "a moved-from list is empty");
  ClauseList target;
  target.push_back(numbered(6000));
  target = std::move(moved);
  check(ids(target) == ids(copy), "move assignment keeps the clauses");
  check(moved.empty() && !moved.chunked() && moved.capacity() == 0, "a list moved from by assignment is empty");
  list.push_back(numbered(1));
  check(ids(list) == std::vector<std::size_t>{1}, "a moved-from list takes clauses again");
}

void check_copy_forms() {
  // deque に置いた列でも、写すときに kVectorLimit 節以下なら長さぴったりの vector になる。
  ClauseList list;
  for (std::size_t step = 0; step <= kLimit; ++step) list.push_back(numbered(step));
  list.erase(list.begin());
  check(list.chunked() && list.size() == kLimit, "the original stays in the deque");
  const ClauseList copy = list;
  check(!copy.chunked() && copy.capacity() == kLimit && ids(copy) == ids(list), "a short copy is a tight vector");
  // 自分への代入は何も変えない。別名を通すのは、直接書くと clang が -Wself-assign-overloaded で止めるから。
  const auto before = ids(list);
  const ClauseList& same = list;
  list = same;
  check(ids(list) == before && list.chunked(), "self copy assignment keeps the clauses");
  ClauseList& alias = list;
  list = std::move(alias);
  check(ids(list) == before && list.chunked(), "self move assignment keeps the clauses");
}

}  // namespace

int main() {
  check_one_clause();
  check_both_ends();
  check_modes();
  check_queue();
  check_shrink();
  check_iterators();
  check_copies();
  check_copy_forms();
  return mosa_test::failures();
}
