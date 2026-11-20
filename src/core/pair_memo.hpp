#pragma once

// 単一化（src/core/unify.cpp）と標準順序の比較（src/builtins/order.cpp）が、入った複合項の組を覚える表。同じ組を
// 2 度辿らないので、巡回する項でも止まり、共有の多い項でも 2 乗にならない。表はヒープの外の C++ の物で、組の数だけ
// 伸びる。同じ組に戻らない歩みの組は複合項のセルより少なく、使っているヒープの語の数を超えない（100 万要素の
// リストの L = M で、組 1,000,000 に対してヒープ 6,000,020 語）。互いに素な周期の巡回する 2 つの項では組が周期の
// 積になり、上限を何倍も越えて伸びた（レビュー H-02、p=2000 で組 4,002,000 に対してヒープ 104,125 語）。
// そこで、使っているヒープの語の数を超えた組だけを、足すたびに上限に数える（ADR 0010 の改訂 4）。全部を数えると、
// 普通の大きなリストの L = M がヒープの約 2 倍を要るようになる。
#include "mosa/heap.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_set>
#include <utility>

namespace mosa {

class PairMemo {
 public:
  using Pair = std::pair<std::uint64_t, std::uint64_t>;

  // 1 組の目安のバイト数。組、次の節点への指し手、覚えたハッシュの値、桶の指し手 1 つ（libc++ と libstdc++ の形）。
  // 確保の目安で、バイトまでは合わせない（ADR 0010 の改訂 2 と同じ）。
  static constexpr std::size_t kPairBytes = sizeof(Pair) + 3 * sizeof(void*);

  // 新しい組なら覚えて true、前に覚えた組なら false。数える分が上限を越えるなら ResourceLimit("heap_limit")。
  bool insert(const Heap& heap, std::uint64_t left, std::uint64_t right) {
    // 処理系によっては空の表でも確保するので、要る時まで作らない。
    if (!set_) set_.emplace();
    if (!set_->emplace(left, right).second) return false;
    charge(heap, set_->size());
    return true;
  }

  [[nodiscard]] bool active() const { return set_.has_value(); }

 private:
  struct PairHash {
    std::size_t operator()(const Pair& pair) const {
      return std::hash<std::uint64_t>{}(pair.first ^ (pair.second * 0x9E3779B97F4A7C15ULL));
    }
  };

  // 単一化と比較はヒープに割り当てないので、heap.size() は歩みの間ずっと同じである。
  static void charge(const Heap& heap, std::size_t pairs) {
    const std::size_t free_pairs = heap.size();
    if (pairs <= free_pairs) return;
    const std::size_t words = ((pairs - free_pairs) * kPairBytes + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t);
    const std::size_t used = heap.size() + heap.held() + heap.state();
    if (used >= heap.limit() || words > heap.limit() - used) throw ResourceLimit("heap_limit");
  }

  std::optional<std::unordered_set<Pair, PairHash>> set_;
};

}  // namespace mosa
