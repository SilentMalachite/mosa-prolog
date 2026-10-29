// ADR 0009、計画 Task 7。生成コードの制御構文が積む選択点。インタプリタの同じ構文（cut.cpp、control.cpp）と
// 同じ Entry を同じ順で積むので、has_more と backtrack の見え方が変わらない。やり直しは継続枠の状態で生成関数を
// 呼び直す（native_redo.cpp）。

#include "internal.hpp"

#include <bit>
#include <cstdint>
#include <utility>

namespace mosa::rt {

std::size_t choice_mark(Machine::Impl& machine) { return machine.stack.size(); }

// 継続枠の語 1 の下位 32 ビットが状態、語 4 が生成関数（resume_frame の置き方）。枠から読めば、生成コードが
// 状態と関数を二度書かずに済む。
void push_branch(Machine::Impl& machine, std::uint64_t frame, std::size_t cut) {
  Entry entry;
  entry.pick = Pick::Native;
  entry.native = std::bit_cast<NativeProc>(machine.heap.word_at(frame + 4));
  entry.redo_goal = frame;
  entry.candidates.push_back(static_cast<std::uint32_t>(machine.heap.word_at(frame + 1)));
  entry.cut = cut;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(entry));
}

// (A *-> B ; C) の C。条件が成功したら soft_commit が印を付け、やり直しは C を飛ばす（Pick::ElseGoal の soft_id と同じ）。
std::size_t push_soft(Machine::Impl& machine, std::uint64_t frame, std::size_t cut) {
  const int soft_id = machine.next_soft++;
  push_branch(machine, frame, cut);
  machine.stack.back().soft_id = soft_id;
  return static_cast<std::size_t>(soft_id);
}

void soft_commit(Machine::Impl& machine, std::size_t soft_id) {
  for (Entry& entry : machine.stack) {
    if (entry.soft_id == static_cast<int>(soft_id)) entry.cond_ok = true;
  }
}

}  // namespace mosa::rt
