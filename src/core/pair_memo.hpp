#pragma once

// 単一化（src/core/unify.cpp）と標準順序の比較（src/builtins/order.cpp）が、入った複合項の組を覚える表。同じ組を
// 2 度辿らないので、巡回する項でも止まり、共有の多い項でも 2 乗にならない。表はヒープの外の C++ の物で、組の数だけ
// 伸びる。同じ組に戻らない歩みの組は複合項のセルより少なく、使っているヒープの語の数を超えない（100 万要素の
// リストの L = M で、組 1,000,000 に対してヒープ 6,000,020 語）。互いに素な周期の巡回する 2 つの項では組が周期の
// 積になり、上限を何倍も越えて伸びた（レビュー H-02、p=2000 で組 4,002,000 に対してヒープ 104,125 語）。
// そこで、使っているヒープの語の数を超えた組だけを、足すたびに上限に数える（ADR 0010 の改訂 4）。全部を数えると、
// 普通の大きなリストの L = M がヒープの約 2 倍を要るようになる。
// 標準順序の深さ優先の歩みは、組ごとに入った順の番号も覚える（PairNumbers、ADR 0029）。
#include "mosa/heap.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mosa {

namespace pair_detail {

using Pair = std::pair<std::uint64_t, std::uint64_t>;

struct PairHash {
  std::size_t operator()(const Pair& pair) const {
    return std::hash<std::uint64_t>{}(pair.first ^ (pair.second * 0x9E3779B97F4A7C15ULL));
  }
};

// 表がヒープの外に bytes を持つとき。free_bytes までは数えない。単一化と比較はヒープに割り当てないので、heap.size() は
// 歩みの間ずっと同じである。
inline void charge_bytes(const Heap& heap, std::size_t bytes, std::size_t free_bytes) {
  if (bytes <= free_bytes) return;
  const std::size_t words = (bytes - free_bytes + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t);
  const std::size_t used = heap.size() + heap.held() + heap.state();
  if (used >= heap.limit() || words > heap.limit() - used) throw ResourceLimit("heap_limit");
}

// pairs 組目を足したとき。使っているヒープの語の数までの組は数えない。
inline void charge(const Heap& heap, std::size_t pairs, std::size_t pair_bytes) {
  charge_bytes(heap, pairs * pair_bytes, heap.size() * pair_bytes);
}

}  // namespace pair_detail

class PairMemo {
 public:
  using Pair = pair_detail::Pair;

  // 1 組の目安のバイト数。組、次の節点への指し手、覚えたハッシュの値、桶の指し手 1 つ（libc++ と libstdc++ の形）。
  // 確保の目安で、バイトまでは合わせない（ADR 0010 の改訂 2 と同じ）。
  static constexpr std::size_t kPairBytes = sizeof(Pair) + 3 * sizeof(void*);

  PairMemo() = default;
  // 組ごとに表の外にも持つ物（幅優先の待ち行列の 1 組など）があれば、そのバイト数も 1 組に足して数える。
  explicit PairMemo(std::size_t extra_bytes) : pair_bytes_(kPairBytes + extra_bytes) {}

  // 新しい組なら覚えて true、前に覚えた組なら false。数える分が上限を越えるなら ResourceLimit("heap_limit")。
  bool insert(const Heap& heap, std::uint64_t left, std::uint64_t right) {
    // 処理系によっては空の表でも確保するので、要る時まで作らない。
    if (!set_) set_.emplace();
    if (!set_->emplace(left, right).second) return false;
    pair_detail::charge(heap, set_->size(), pair_bytes_);
    return true;
  }

  [[nodiscard]] bool active() const { return set_.has_value(); }

 private:
  std::size_t pair_bytes_ = kPairBytes;
  std::optional<std::unordered_set<Pair, pair_detail::PairHash>> set_;
};

// 組ごとに、入った順の番号を覚える表（標準順序の深さ優先の歩み、ADR 0029）。PairMemo と同じく数える。番号は
// 組が比べている途中かを見分けるのに使う（途中の組は今の道に載っている組だけである）。標準の連想配列は 1 組ごとに
// 節点を確保し、番号を足すと節点が大きくなって、大きな写しの == が 3 割遅くなった。そこで 1 本の配列に開番地で置く。
class PairNumbers {
 public:
  // 歩みが組ごとに表の外に持つ物（道の番号 1 つと、深い所の枠 1 つ）の目安。
  static constexpr std::size_t kWalkBytes = 32;

  // 新しい組なら number で覚えて空を返す。前に入った組なら、その番号を返す。
  // 前に入った組は表を伸ばさずに返す。前は先に伸ばしたので、表が半分に届いた所で前の組を引くだけでも伸ばし、ヒープの
  // 上限の判定を 1 組早く通った（レビュー R-L01）。
  std::optional<std::uint64_t> enter(const Heap& heap, std::uint64_t left, std::uint64_t right, std::uint64_t number) {
    if (!slots_.empty()) {
      const Slot& known = find(slots_, left, right);
      if (known.mark != 0) return known.mark - 1;
    }
    if ((count_ + 1) * 2 > slots_.size()) grow(heap);
    Slot& slot = find(slots_, left, right);
    slot = {left, right, number + 1};
    ++count_;
    charge(heap, slots_.size());
    return std::nullopt;
  }

  [[nodiscard]] bool active() const { return !slots_.empty(); }

 private:
  // mark は番号に 1 を足した値で、0 は空の枠。
  struct Slot {
    std::uint64_t left;
    std::uint64_t right;
    std::uint64_t mark;
  };

  static constexpr std::size_t kFirstSlots = 1024;
  // 使っているヒープの語の数までの組は、1 組 48 バイト（枠 2 つ）として数えない（ADR 0010 の改訂 4）。
  static constexpr std::size_t kFreePairBytes = 2 * sizeof(Slot);

  // 確保した枠の数で数える。表は半分までしか埋めず、倍に広げる間は古い枠も残る。
  void charge(const Heap& heap, std::size_t slots) const {
    pair_detail::charge_bytes(heap, slots * sizeof(Slot) + count_ * kWalkBytes, heap.size() * kFreePairBytes);
  }

  // その組の枠か、組が無ければ置くべき空の枠。枠の数は 2 の冪で、半分より多くは埋まらない。
  static Slot& find(std::vector<Slot>& slots, std::uint64_t left, std::uint64_t right) {
    std::uint64_t mixed = (left ^ (right * 0x9E3779B97F4A7C15ULL)) * 0xD6E8FEB86659FD93ULL;
    mixed ^= mixed >> 32;
    const std::size_t mask = slots.size() - 1;
    std::size_t at = static_cast<std::size_t>(mixed) & mask;
    while (slots[at].mark != 0 && (slots[at].left != left || slots[at].right != right)) at = (at + 1) & mask;
    return slots[at];
  }

  void grow(const Heap& heap) {
    const std::size_t size = slots_.empty() ? kFirstSlots : slots_.size() * 2;
    charge(heap, size + slots_.size());
    std::vector<Slot> wider(size, Slot{0, 0, 0});
    for (const Slot& slot : slots_) {
      if (slot.mark != 0) find(wider, slot.left, slot.right) = slot;
    }
    slots_.swap(wider);
  }

  std::vector<Slot> slots_;
  std::size_t count_ = 0;
};

}  // namespace mosa
