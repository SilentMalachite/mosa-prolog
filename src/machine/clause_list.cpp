// 節の列の置き場の切り替え（ADR 0015 の改訂 2、レビュー M21）。

#include "clause_list.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace mosa {
namespace {

// vector の余りがこれより少なければ縮めない。retract して assertz し直す 1 節の数え上げのような述語が、変更の
// たびに確保と解放を繰り返さないようにする（M21 の敵対的レビュー B1）。残るのは述語ごとに 3 節分までである。
constexpr std::size_t kSpareSlots = 4;

// 節を写さず移して、長さぴったりの vector を作る。
template <typename Source>
std::vector<Clause> exact_vector(Source& source) {
  std::vector<Clause> out;
  out.reserve(source.size());
  out.insert(out.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
  return out;
}

}  // namespace

ClauseList::ClauseList(const ClauseList& other) {
  if (other.size() <= kVectorLimit) {
    slots_.reserve(other.size());
    slots_.insert(slots_.end(), other.begin(), other.end());
  } else {
    chunked_ = std::make_unique<std::deque<Clause>>(other.begin(), other.end());
  }
}

ClauseList& ClauseList::operator=(const ClauseList& other) {
  if (this == &other) return *this;
  ClauseList copied(other);
  *this = std::move(copied);
  return *this;
}

ClauseList::ClauseList(ClauseList&& other) noexcept
    : slots_(std::move(other.slots_)), chunked_(std::move(other.chunked_)) {
  other.slots_.clear();
}

ClauseList& ClauseList::operator=(ClauseList&& other) noexcept {
  if (this == &other) return *this;
  slots_ = std::move(other.slots_);
  chunked_ = std::move(other.chunked_);
  other.slots_.clear();
  return *this;
}

void ClauseList::push_back(Clause clause) {
  make_room();
  if (chunked_) chunked_->push_back(std::move(clause));
  else slots_.push_back(std::move(clause));
}

void ClauseList::push_front(Clause clause) {
  make_room();
  if (chunked_) chunked_->push_front(std::move(clause));
  else slots_.insert(slots_.begin(), std::move(clause));
}

// vector が一杯なら、kVectorLimit までは容量の半分を足し、超えるなら deque へ移す。容量を自分で決めるのは、標準の
// 伸び方（libc++ と libstdc++ は 2 倍、MSVC は 1.5 倍）に任せると余りが OS で変わるから。deque は先に空の節で
// 長さを取り、それから移す。移す範囲から deque を作ると、MSVC は 1 節ずつ確保と移動を交互に行うので、途中の
// bad_alloc が移した節を失う（M21 の敵対的レビュー B2）。空の節は何も確保せず、移す代入は noexcept である。
void ClauseList::make_room() {
  if (chunked_ || slots_.size() < slots_.capacity()) return;
  if (slots_.size() < kVectorLimit) {
    const std::size_t grown = slots_.size() + std::max<std::size_t>(slots_.size() / 2, 1);
    slots_.reserve(std::min(grown, kVectorLimit));
    return;
  }
  auto chunked = std::make_unique<std::deque<Clause>>(slots_.size());
  std::move(slots_.begin(), slots_.end(), chunked->begin());
  chunked_ = std::move(chunked);
  slots_ = std::vector<Clause>{};
}

void ClauseList::erase(const_iterator first, const_iterator last) {
  if (first == last) return;
  const auto from = static_cast<std::ptrdiff_t>(first.position());
  const auto to = static_cast<std::ptrdiff_t>(last.position());
  if (chunked_) chunked_->erase(chunked_->begin() + from, chunked_->begin() + to);
  else slots_.erase(slots_.begin() + from, slots_.begin() + to);
  settle();
}

// deque の節が kVectorLimit の半分以下になるか、vector の節が容量の半分未満になり余りが kSpareSlots 以上になったら、
// 長さぴったりの vector へ移す。そこまでに外した節の数に比例する手間なので、均して O(1) になる。deque が塊を
// 返すのと同じく、消した節の分のメモリを返す。
void ClauseList::settle() {
  if (chunked_) {
    if (chunked_->size() > kVectorLimit / 2) return;
    slots_ = exact_vector(*chunked_);
    chunked_.reset();
    return;
  }
  const std::size_t spare = slots_.capacity() - slots_.size();
  if (spare <= slots_.size() || spare < kSpareSlots) return;
  slots_ = exact_vector(slots_);
}

}  // namespace mosa
