// ADR 0010 の詰め直し。新しい位置は rank(i) = i より下にある印の付いた語の数で、64 語の区画ごとの累計と
// popcount で引く。印のビット表は写しの間も変わらないので、ヒープを下から 1 回走査する途中でも rank を引ける。
// 写し先 rank(i) は i 以下なので、まだ読んでいない語を上書きしない。

#include "gc.hpp"

#include "internal.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace mosa {
namespace {

// ビット表の rank。区画 b より下の印の数を前もって数えておく。
class Rank {
 public:
  explicit Rank(const std::vector<std::uint64_t>& bits) : bits_(bits), before_(bits.size() + 1, 0) {
    for (std::size_t block = 0; block < bits.size(); ++block) {
      before_[block + 1] = before_[block] + static_cast<std::size_t>(std::popcount(bits[block]));
    }
  }
  // index は表の範囲（bits.size() * 64 未満）の中で呼ぶ。
  [[nodiscard]] std::size_t of(std::size_t index) const {
    const std::uint64_t below = (std::uint64_t{1} << (index & 63)) - 1;
    return before_[index >> 6] + static_cast<std::size_t>(std::popcount(bits_[index >> 6] & below));
  }
  [[nodiscard]] bool live(std::size_t index) const { return ((bits_[index >> 6] >> (index & 63)) & 1U) != 0; }
  [[nodiscard]] std::size_t total() const { return before_.back(); }

 private:
  const std::vector<std::uint64_t>& bits_;
  std::vector<std::size_t> before_;
};

class Rewriter final : public GcVisitor {
 public:
  // 2 つの Rank と kept_before の表をここで確保する。どれもトレイルとヒープを書き換える前に済ませ、bad_alloc で
  // 抜けてもヒープを壊さない（ADR 0010 の停止）。kept_before の中身は compact_trail が埋める。
  Rewriter(Impl& machine, const GcMarks& marks)
      : base_(std::bit_cast<std::uint64_t>(machine.heap.data())),
        used_(marks.used),
        extras_size_(machine.extras.size()),
        heap_(marks.live),
        extras_(marks.extras_live),
        kept_before_(machine.trail.slots.size() + 1, 0) {}

  void cell(Cell& slot) override { slot = Cell{relocate(slot.raw())}; }
  void frame(std::uint64_t& index) override {
    if (index < used_) index = heap_.of(static_cast<std::size_t>(index));
  }
  void heap_mark(std::size_t& mark) override { mark = heap_.of(std::min(mark, used_)); }
  void trail_mark(std::size_t& mark) override { mark = kept_before_[std::min(mark, kept_before_.size() - 1)]; }
  void extras_mark(std::size_t& mark) override { mark = extras_.of(std::min(mark, extras_size_)); }
  void weak_address(std::uint64_t& raw) override {
    const auto at = index_of(raw);
    raw = at && heap_.live(*at) ? relocate(raw) : 0;
  }
  // 添字 0 は生きた語でありうるので、死んだ語は SIZE_MAX にする。どの生きた添字とも一致しない。
  void weak_index(std::size_t& index) override {
    index = index < used_ && heap_.live(index) ? heap_.of(index) : std::numeric_limits<std::size_t>::max();
  }

  // ポインタのタグの語だけを書き換える。ヒープの外を指すセルは項ではないので、そのまま残す。
  [[nodiscard]] std::uint64_t relocate(std::uint64_t raw) const {
    const auto tag = static_cast<Tag>(raw & 7U);
    if (tag == Tag::Atom || tag == Tag::Int || tag == Tag::Big) return raw;
    const auto at = index_of(raw);
    if (!at) return raw;
    return (base_ + heap_.of(*at) * sizeof(std::uint64_t)) | (raw & 7U);
  }
  [[nodiscard]] std::optional<std::size_t> index_of(std::uint64_t raw) const {
    const std::uint64_t address = raw & ~std::uint64_t{7};
    if (address < base_ || address - base_ >= used_ * sizeof(std::uint64_t)) return std::nullopt;
    return static_cast<std::size_t>((address - base_) / sizeof(std::uint64_t));
  }
  [[nodiscard]] const Rank& heap() const { return heap_; }
  [[nodiscard]] const Rank& extras() const { return extras_; }
  [[nodiscard]] std::uint64_t base() const { return base_; }
  [[nodiscard]] std::vector<std::size_t>& kept_before() { return kept_before_; }

 private:
  std::uint64_t base_;
  std::size_t used_;
  std::size_t extras_size_;
  Rank heap_;
  Rank extras_;
  std::vector<std::size_t> kept_before_;
};

[[nodiscard]] WordKind kind_of(const GcMarks& marks, std::size_t index) {
  return static_cast<WordKind>((marks.kinds[index >> 5] >> ((index & 31) * 2)) & 3U);
}

// 印の付いた語を下から順に写す。種類に従って、セルの番地と枠・extras の添字を新しい値にする。
void slide_heap(Impl& machine, const GcMarks& marks, const Rewriter& rewriter) {
  std::size_t to = 0;
  for (std::size_t block = 0; block < marks.live.size(); ++block) {
    std::uint64_t bits = marks.live[block];
    while (bits != 0) {
      const std::size_t from = block * 64 + static_cast<std::size_t>(std::countr_zero(bits));
      bits &= bits - 1;
      std::uint64_t value = machine.heap.word_at(from);
      switch (kind_of(marks, from)) {
        case WordKind::Cell: value = rewriter.relocate(value); break;
        case WordKind::FrameIndex:
          if (value < marks.used) value = rewriter.heap().of(static_cast<std::size_t>(value));
          break;
        case WordKind::ExtrasIndex:
          if (value < machine.extras.size()) value = rewriter.extras().of(static_cast<std::size_t>(value));
          break;
        case WordKind::Raw: break;
      }
      machine.heap.set_word(to, value);
      ++to;
    }
  }
  machine.heap.reset_size_after_gc(to);
}

// 制御枠から届いた項目だけを順序を保って残す。env と cont も新しいヒープへ合わせる。
void compact_extras(Impl& machine, Rewriter& rewriter) {
  std::size_t to = 0;
  for (std::size_t from = 0; from < machine.extras.size(); ++from) {
    if (!rewriter.extras().live(from)) continue;
    Extra& extra = machine.extras[from];
    for (Cell& slot : extra.env) rewriter.cell(slot);
    rewriter.frame(extra.cont);
    if (to != from) machine.extras[to] = std::move(extra);
    ++to;
  }
  machine.extras.resize(to);
}

// 印の付いた語を指す項目だけを残す。kept_before[k] は旧位置 k より前に残った数で、trail_mark の写しに使う。
// 表は Rewriter が確保済みなので、ここでは何も確保しない（縮める resize も確保しない）。
void compact_trail(Impl& machine, const GcMarks& marks, Rewriter& rewriter) {
  const auto base = std::bit_cast<std::uint64_t>(machine.heap.data());
  const Rank& rank = rewriter.heap();
  auto& slots = machine.trail.slots;
  std::vector<std::size_t>& kept_before = rewriter.kept_before();
  std::size_t to = 0;
  for (std::size_t from = 0; from < slots.size(); ++from) {
    kept_before[from] = to;
    const auto address = std::bit_cast<std::uint64_t>(slots[from]);
    if (address < base || address - base >= marks.used * sizeof(std::uint64_t)) continue;
    const auto at = static_cast<std::size_t>((address - base) / sizeof(std::uint64_t));
    if (!rank.live(at)) continue;
    slots[to] = std::bit_cast<std::uint64_t*>(base + rank.of(at) * sizeof(std::uint64_t));
    ++to;
  }
  kept_before[slots.size()] = to;
  slots.resize(to);
}

// Trail::mark は水位（data() + heap_mark）なので、heap_mark と同じく rank で写す。
void move_trail_mark(Impl& machine, const Rewriter& rewriter, std::size_t used) {
  if (machine.trail.mark == nullptr) return;
  const auto offset = std::bit_cast<std::uint64_t>(machine.trail.mark) - rewriter.base();
  const auto at = std::min(static_cast<std::size_t>(offset / sizeof(std::uint64_t)), used);
  machine.trail.mark = machine.heap.data() + rewriter.heap().of(at);
}

}  // namespace

void compact_live(Impl& machine, const GcMarks& marks) {
  Rewriter rewriter(machine, marks);
  compact_trail(machine, marks, rewriter);
  visit_roots(machine, rewriter);
  move_trail_mark(machine, rewriter, marks.used);
  slide_heap(machine, marks, rewriter);
  compact_extras(machine, rewriter);
}

}  // namespace mosa
