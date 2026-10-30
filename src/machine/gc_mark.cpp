// ADR 0010 の印付け。C++ の再帰は使わず、セルの語と枠の 2 つのスタックで辿る。深いリストや長い継続の鎖でも
// C++ のスタックを使い切らない。語の種類（WordKind）は、その語に最初に届いた道が決める。
// 項: Str は関数語を Raw、引数を Cell。Lis は 2 語とも Cell。Float と IntBox は 1 語を Raw。Ref は語を Cell として値を辿る。
// 枠: 語 0 が 0 でなければゴール枠、0 で語 1 の第 63 ビットが立てば Native 枠、ほかは制御枠（step と同じ見分け方）。

#include "gc.hpp"

#include "internal.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace mosa {
namespace {

constexpr std::uint64_t k_count_mask = 0x7FFFFFFFU;

class Marker final : public GcVisitor {
 public:
  Marker(Impl& machine, GcMarks& marks) : machine_(machine), marks_(marks) {}

  void cell(Cell& slot) override { mark_value(slot); }
  void frame(std::uint64_t& index) override {
    if (index != kNoFrame) frames_.push_back(index);
  }
  void heap_mark(std::size_t& /*mark*/) override {}
  void trail_mark(std::size_t& /*mark*/) override {}
  void extras_mark(std::size_t& /*mark*/) override {}
  void weak_address(std::uint64_t& /*raw*/) override {}
  void weak_index(std::size_t& /*index*/) override {}

  void drain();

 private:
  [[nodiscard]] bool marked(std::size_t index) const {
    return ((marks_.live[index >> 6] >> (index & 63)) & 1U) != 0;
  }
  void set(std::size_t index, WordKind kind);
  void mark_cell_word(std::size_t index);
  void mark_value(Cell value);
  void mark_structure(std::size_t at);
  void mark_frame(std::size_t at);
  void mark_extra(std::size_t index);
  [[nodiscard]] std::uint64_t word(std::size_t index) const { return machine_.heap.word_at(index); }

  Impl& machine_;
  GcMarks& marks_;
  std::vector<std::size_t> cells_;
  std::vector<std::uint64_t> frames_;
};

void Marker::set(std::size_t index, WordKind kind) {
  marks_.live[index >> 6] |= std::uint64_t{1} << (index & 63);
  marks_.kinds[index >> 5] |= static_cast<std::uint64_t>(kind) << ((index & 31) * 2);
}

// 印の無い語だけを積む。どの語も積むのは 1 回なので、循環した項でも止まる。
void Marker::mark_cell_word(std::size_t index) {
  if (index >= marks_.used || marked(index)) return;
  set(index, WordKind::Cell);
  cells_.push_back(index);
  if (cells_.size() > marks_.peak_cells) marks_.peak_cells = cells_.size();
}

void Marker::mark_structure(std::size_t at) {
  if (marked(at)) return;
  set(at, WordKind::Raw);
  const std::uint16_t arity = functor_arity(word(at));
  // 最後の引数を最後に取り出すよう、後ろから積む。右に入れ子の鎖 f(_, f(_, ...)) でもスタックは引数の数で済む。
  for (std::size_t index = arity; index >= 1; --index) mark_cell_word(at + index);
}

// ヒープの外を指すセル（既定の Cell{} など）は項ではない。辿らず、詰め直しも書き換えない。
void Marker::mark_value(Cell value) {
  const Tag tag = value.tag();
  if (tag == Tag::Atom || tag == Tag::Int || tag == Tag::Big) return;
  const auto base = std::bit_cast<std::uint64_t>(machine_.heap.data());
  const std::uint64_t address = value.raw() & ~std::uint64_t{7};
  if (address < base || address - base >= marks_.used * sizeof(std::uint64_t)) return;
  const std::size_t at = static_cast<std::size_t>((address - base) / sizeof(std::uint64_t));
  switch (tag) {
    case Tag::Ref: mark_cell_word(at); return;
    case Tag::Lis:
      // 尾を先に積み、頭を先に取り出す。頭を積んだまま尾を辿ると、リストの長さだけ頭がスタックに溜まる。
      mark_cell_word(at + 1);
      mark_cell_word(at);
      return;
    case Tag::Str: mark_structure(at); return;
    case Tag::Float:
    case Tag::IntBox:
      if (!marked(at)) set(at, WordKind::Raw);
      return;
    default: return;
  }
}

void Marker::mark_extra(std::size_t index) {
  if (index >= machine_.extras.size()) return;
  auto& bits = marks_.extras_live[index >> 6];
  const std::uint64_t bit = std::uint64_t{1} << (index & 63);
  if ((bits & bit) != 0) return;
  bits |= bit;
  Extra& extra = machine_.extras[index];
  for (Cell& slot : extra.env) mark_value(slot);
  frame(extra.cont);
}

// 枠は多くの継続から共有される。語 0 の印で、辿り済みの枠を二度辿らない。
void Marker::mark_frame(std::size_t at) {
  if (at + kFrameHead > marks_.used || marked(at)) return;
  const std::uint64_t head = word(at);
  const std::uint64_t info = word(at + 1);
  set(at, WordKind::Raw);
  set(at + 2, WordKind::Raw);
  set(at + 3, WordKind::FrameIndex);
  if (head == 0 && (info & rt::kNativeBit) == 0) {
    set(at + 1, WordKind::ExtrasIndex);
    mark_extra(static_cast<std::size_t>(info));
    return;
  }
  set(at + 1, WordKind::Raw);
  std::size_t first = at + kFrameHead;
  std::size_t count = goal_word_arity(info);
  if (head == 0) {
    if (at + rt::kNativeVars > marks_.used) return;
    set(at + 4, WordKind::Raw);
    first = at + rt::kNativeVars;
    count = static_cast<std::size_t>((info >> 32) & k_count_mask);
  }
  for (std::size_t index = 0; index < count; ++index) mark_cell_word(first + index);
  const std::uint64_t cont = word(at + 3);
  if (cont != kNoFrame) frames_.push_back(cont);
}

void Marker::drain() {
  while (!cells_.empty() || !frames_.empty()) {
    if (!cells_.empty()) {
      const std::size_t index = cells_.back();
      cells_.pop_back();
      mark_value(Cell{word(index)});
      continue;
    }
    const std::uint64_t at = frames_.back();
    frames_.pop_back();
    if (at < marks_.used) mark_frame(static_cast<std::size_t>(at));
  }
}

}  // namespace

void mark_live(Impl& machine, GcMarks& marks) {
  marks.used = machine.heap.size();
  // used の位置（水位がヒープの末尾を指すとき）まで rank を引けるよう、区画を 1 つ多く取る。
  marks.live.assign(marks.used / 64 + 1, 0);
  marks.kinds.assign(marks.used / 32 + 1, 0);
  marks.extras_live.assign(machine.extras.size() / 64 + 1, 0);
  Marker marker(machine, marks);
  visit_roots(machine, marker);
  marker.drain();
}

}  // namespace mosa
