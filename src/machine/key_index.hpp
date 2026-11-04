#pragma once

// 鍵（module、name、arity）から列の位置を引く索引（ADR 0018、レビュー M25）。述語表と読み込みの写しが使う。

#include "../module/module.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mosa {

// 開番地法（線形探査）。枠は位置だけを持ち、鍵は列の要素の module・name・arity と比べるので、列は呼び手が渡す。
// 枠の数は 2 の冪で要素の 2 倍以上なので、探査は必ず空の枠で止まる。std::unordered_map は鍵ごとにノードを取り
// （libc++ で 60 バイトほど）、剰余とノードを辿る。位置は 32 ビットに収める（要素 1 つが 100 バイトを超えるので、
// 2^32 個はメモリに載らない）。
template <class Item>
class KeyIndex {
 public:
  static constexpr std::size_t kNoSlot = SIZE_MAX;

  [[nodiscard]] std::size_t slots() const { return slots_.size(); }
  // key を持つ要素の枠。無ければ kNoSlot。
  [[nodiscard]] std::size_t find(const std::vector<Item>& items, PredKey key) const {
    if (slots_.empty()) return kNoSlot;
    const std::size_t mask = slots_.size() - 1;
    for (std::size_t slot = home(key);; slot = (slot + 1) & mask) {
      const std::uint32_t at = slots_[slot];
      if (at == kEmpty) return kNoSlot;
      const Item& item = items[at];
      if (item.name == key.name && item.module == key.module && item.arity == key.arity) return slot;
    }
  }
  [[nodiscard]] std::uint32_t at(std::size_t slot) const { return slots_[slot]; }
  void repoint(std::size_t slot, std::uint32_t at) { slots_[slot] = at; }

  // 要素を count 個にする前に呼ぶ。足りなければ倍に伸ばし、今の列から入れ直す。新しい枠を作り終えてから
  // 入れ替えるので、確保に失敗しても前の索引が残る。
  void reserve(const std::vector<Item>& items, std::size_t count) {
    if (count * 2 <= slots_.size()) return;
    std::size_t size = std::max<std::size_t>(16, slots_.size());
    while (size < count * 2) size *= 2;
    std::vector<std::uint32_t> fresh(size, kEmpty);
    slots_.swap(fresh);
    shift_ = 64 - static_cast<unsigned>(std::countr_zero(size));
    rebuild(items);
  }
  // 位置 at の要素の鍵はまだ索引に無いこと。枠は reserve が先に取っておく。
  void insert(const std::vector<Item>& items, std::uint32_t at) {
    const std::size_t mask = slots_.size() - 1;
    std::size_t slot = home(key_at(items, at));
    while (slots_[slot] != kEmpty) slot = (slot + 1) & mask;
    slots_[slot] = at;
  }
  // 枠 hole を空ける（線形探査の後ろ詰め）。墓標を置くと、消すたびに探査が長くなる。next の要素は自分の家から
  // next までのどこかで探されるので、hole がその間にあれば hole へ寄せ、空いた next を次の hole にする。
  // 要素はまだ同じ位置にあること。
  void remove(const std::vector<Item>& items, std::size_t hole) {
    const std::size_t mask = slots_.size() - 1;
    for (std::size_t next = (hole + 1) & mask; slots_[next] != kEmpty; next = (next + 1) & mask) {
      const std::size_t wanted = home(key_at(items, slots_[next]));
      if (((next - wanted) & mask) < ((next - hole) & mask)) continue;
      slots_[hole] = slots_[next];
      hole = next;
    }
    slots_[hole] = kEmpty;
  }
  // 列から位置 from の要素を外した後に呼ぶ。後ろの要素は 1 つずつ前へずれているので、それぞれの枠を家から辿り、
  // 前の位置（at + 1）を持つ枠を 1 つ下げる。鍵では比べない（枠の位置がまだ前のままで、列の別の要素を指す）。
  // 位置は枠ごとに違うので、前から順に下げても取り違えない。全部の枠をなめると、表が縮んだ後も消すたびに一番
  // 大きかった時の枠の数だけかかり、末尾の要素を消しても O(1) にならない（M25 の敵対的レビュー B1、B2）。
  void shift_down(const std::vector<Item>& items, std::uint32_t from) {
    const std::size_t mask = slots_.size() - 1;
    for (std::size_t at = from; at < items.size(); ++at) {
      const auto old = static_cast<std::uint32_t>(at + 1);
      std::size_t slot = home(key_at(items, static_cast<std::uint32_t>(at)));
      while (slots_[slot] != old && slots_[slot] != kEmpty) slot = (slot + 1) & mask;
      if (slots_[slot] == old) slots_[slot] = static_cast<std::uint32_t>(at);
    }
  }
  // 列から作り直す。確保はしない。同じ鍵の要素が 2 つあれば、後ろの位置が残る（読み込みの写しの鎖の頭）。
  void rebuild(const std::vector<Item>& items) {
    std::fill(slots_.begin(), slots_.end(), kEmpty);
    if (slots_.empty()) return;
    for (std::size_t at = 0; at < items.size(); ++at) {
      const auto position = static_cast<std::uint32_t>(at);
      const std::size_t slot = find(items, key_at(items, position));
      if (slot == kNoSlot) insert(items, position);
      else repoint(slot, position);
    }
  }

 private:
  static constexpr std::uint32_t kEmpty = UINT32_MAX;

  [[nodiscard]] static PredKey key_at(const std::vector<Item>& items, std::uint32_t at) {
    return PredKey{items[at].module, items[at].name, items[at].arity};
  }
  // 掛けて上のビットを取る（Fibonacci hashing）。名前とアリティを 1 語に寄せ、モジュールは別の奇数を掛けて混ぜる。
  // アトムの番号は近い値が続くが、掛け算で上のビットへ散る。
  [[nodiscard]] std::size_t home(PredKey key) const {
    const std::uint64_t packed = (std::uint64_t{key.name} | std::uint64_t{key.arity} << 32) ^
                                 std::uint64_t{key.module} * 0xD6E8FEB86659FD93ULL;
    return static_cast<std::size_t>((packed * 0x9E3779B97F4A7C15ULL) >> shift_);
  }

  std::vector<std::uint32_t> slots_;
  unsigned shift_ = 64;
};

}  // namespace mosa
