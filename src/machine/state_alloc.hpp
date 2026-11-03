#pragma once

// ADR 0010 の改訂 3（レビュー M17 の敵対的レビュー A1）。選択点が持つ候補の列は、1 つの鍵に節が多いと選択点
// 1 つで数百 KB になる。確保と解放のたびに Heap::state へ語を足し引きするアロケータで持ち、写しも移しも
// 壊しも数え違えない。持ち主の無い（既定で作った）列は数えない。

#include "mosa/heap.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace mosa {

template <class T>
class StateAllocator {
 public:
  using value_type = T;
  // 列を移す・写す・入れ替えるときは数える先も一緒に移す。確保した先と返す先が同じ Heap になる。
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  StateAllocator() = default;
  explicit StateAllocator(Heap& heap) : owner_(&heap) {}
  // 標準ライブラリが別の型の列（MSVC の検査用の代理など）へ移し替えるときに暗黙に使う。
  template <class U>
  StateAllocator(const StateAllocator<U>& other) : owner_(other.owner()) {}

  [[nodiscard]] T* allocate(std::size_t count) {
    T* items = std::allocator<T>{}.allocate(count);
    if (owner_ != nullptr) owner_->add_state(words(count));
    return items;
  }
  void deallocate(T* items, std::size_t count) {
    if (owner_ != nullptr) owner_->drop_state(words(count));
    std::allocator<T>{}.deallocate(items, count);
  }
  [[nodiscard]] Heap* owner() const { return owner_; }

  friend bool operator==(const StateAllocator& left, const StateAllocator& right) {
    return left.owner_ == right.owner_;
  }

 private:
  static std::size_t words(std::size_t count) {
    return (count * sizeof(T) + sizeof(std::uint64_t) - 1) / sizeof(std::uint64_t);
  }

  Heap* owner_ = nullptr;
};

// 選択点の候補（節の番号、述語の添字、生成関数の状態）。
using IdList = std::vector<std::size_t, StateAllocator<std::size_t>>;

}  // namespace mosa
