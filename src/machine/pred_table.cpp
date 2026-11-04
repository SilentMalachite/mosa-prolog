// 述語表の足し消し（ADR 0018、レビュー M25）。
#include "pred_table.hpp"

#include <cstddef>
#include <cstdint>
#include <utility>

namespace mosa {

// 確保が要るのは索引を伸ばすときと述語を足すときで、どちらも索引へ入れる前に済ませる。投げても表と索引はずれない。
Predicate& PredicateTable::add(Predicate made) {
  index_.reserve(predicates_, predicates_.size() + 1);
  predicates_.push_back(std::move(made));
  index_.insert(predicates_, static_cast<std::uint32_t>(predicates_.size() - 1));
  return predicates_.back();
}

// 枠を空けるのは述語を動かす前で、ほかの枠の鍵をまだ同じ位置で読める。後ろの述語は 1 つずつ前へずれるので、
// 直すのは後ろの述語の枠だけで、vector のずらしと同じ数で済む。
void PredicateTable::erase(PredKey key) {
  const std::size_t slot = index_.find(predicates_, key);
  if (slot == Index::kNoSlot) return;
  const std::uint32_t at = index_.at(slot);
  index_.remove(predicates_, slot);
  predicates_.erase(predicates_.begin() + static_cast<std::ptrdiff_t>(at));
  index_.shift_down(predicates_, at);
}

}  // namespace mosa
