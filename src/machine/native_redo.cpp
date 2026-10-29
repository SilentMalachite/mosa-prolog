// ADR 0009。生成関数の選択点（Pick::Native）と継続枠（Ctrl::Native）。
// どちらも生成関数を同じ枠と別の状態で呼び直すだけで、C++ の呼び出しはトランポリンの 1 歩を越えて積まない。

#include "internal.hpp"

#include <bit>
#include <cstdint>
#include <span>
#include <utility>

namespace mosa {

// 枠の語 4 に関数の番地を語として置く。番地が 1 語に収まらない環境では枠の形が成り立たない。
static_assert(sizeof(rt::NativeProc) == sizeof(std::uint64_t));

void call_native(Impl& machine, rt::NativeProc proc, std::uint64_t frame) {
  // enter_user の boundary と同じ。節の選択点を積む前の高さが、この呼び出しのカット境界である。
  machine.native_cut = machine.stack.size();
  proc(machine, frame, rt::kCall);
}

void resume_native(Impl& machine) {
  const auto word = machine.heap.word_at(machine.ip + 1);
  const auto proc = std::bit_cast<rt::NativeProc>(machine.heap.word_at(machine.ip + 4));
  // 再開の状態ではカット境界を枠の語 2 から読むので、native_cut は設定しない。
  proc(machine, machine.ip, static_cast<std::uint32_t>(word));
}

bool redo_native(Impl& machine) {
  const Entry& top = machine.stack.back();
  // *-> の条件が一度成功した else は飛ばす（redo_else と同じ）。
  if (top.next >= top.candidates.size() || (top.soft_id != 0 && top.cond_ok)) {
    machine.stack.pop_back();
    return false;
  }
  apply_catches(machine, top);
  const auto state = static_cast<std::uint32_t>(top.candidates[top.next]);
  const auto proc = top.native;
  const auto frame = top.redo_goal;
  const auto cut = top.cut;
  // 最後の候補なら選択点を外してから入る。残すと、その節のカットより下に空の選択点が残る。
  if (top.next + 1 >= top.candidates.size()) machine.stack.pop_back();
  else machine.stack.back().next += 1;
  machine.native_cut = cut;
  proc(machine, frame, state);
  return true;
}

namespace rt {

std::uint64_t resume_frame(Machine::Impl& machine, NativeProc proc, std::uint32_t state, std::size_t cut,
                           std::uint64_t cont, std::span<const Cell> vars) {
  const auto at = machine.heap.allocate_words(kNativeVars + vars.size());
  auto* words = const_cast<std::uint64_t*>(machine.heap.data()) + at;
  words[0] = 0;
  words[1] = kNativeBit | (static_cast<std::uint64_t>(vars.size()) << 32) | state;
  words[2] = cut;
  words[3] = cont;
  words[4] = std::bit_cast<std::uint64_t>(proc);
  for (std::size_t index = 0; index < vars.size(); ++index) words[kNativeVars + index] = vars[index].raw();
  return at;
}

void push_alternative(Machine::Impl& machine, const Regs& regs, std::uint64_t frame, NativeProc proc,
                      std::span<const std::uint32_t> states) {
  Entry entry;
  entry.pick = Pick::Native;
  entry.native = proc;
  entry.redo_goal = frame;
  entry.candidates.assign(states.begin(), states.end());
  entry.cut = regs.cut;
  entry.heap_mark = machine.heap.size();
  entry.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(entry));
}

}  // namespace rt
}  // namespace mosa
