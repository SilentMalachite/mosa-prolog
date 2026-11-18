// 13211-1 に無い（ADR 0028）。デバッガの箱。番兵の選択点 Pick::DebugBox、出口の枠 Ctrl::DebugExit、語 3 を出口の枠に
// した元のゴール枠の写し、中に選択点を残して抜けたときの再試行の印 Pick::DebugRedo で、1 つの呼び出しの出入りを
// 捕まえる。catch/3 の open_catch と disarm_catch（ADR 0012）と同じ形にしてある。
// 投げる誤りは無い（箱の中のゴールの誤りは、そのゴールが投げる）。
// テスト: tests/cpp/data/debugger（transcript_debug_*）、tests/cpp/debugger_test.cpp
#include "debug_port.hpp"
#include "internal.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace mosa {
namespace {

struct Unboxed {
  std::string_view name;
  std::uint16_t arity;
};

// 箱を持たないゴール（計画の設計との違い 3）。launch が IR の節点にする物と同じ名前の枠と、デバッガの 9 つの述語。
constexpr auto kUnboxed = std::to_array<Unboxed>({
    {"true", 0},  {"fail", 0},    {"false", 0},   {"!", 0},       {",", 2},        {";", 2},
    {"->", 2},    {"*->", 2},     {":", 2},       {"catch", 3},   {"throw", 1},    {"halt", 0},
    {"halt", 1},  {"trace", 0},   {"notrace", 0}, {"debug", 0},   {"nodebug", 0},  {"spy", 1},
    {"nospy", 1}, {"nospyall", 0}, {"leash", 1},  {"debugging", 0},
});

bool unboxed(std::string_view name, std::uint16_t arity) {
  if (name.starts_with('$')) return true;
  // call/1..8 は中のゴールが箱を持つので、利用者には中のゴールが見える。
  if (name == "call" && arity >= 1 && arity <= kCallArityMax) return true;
  return std::any_of(kUnboxed.begin(), kUnboxed.end(),
                     [&](const Unboxed& item) { return item.arity == arity && item.name == name; });
}

// stack[box_index] が箱 number の番兵か。番兵が外れた後、同じ位置に別の枠が積まれうるので番号も照らす。
bool box_at(const Impl& machine, std::size_t box_index, std::uint64_t number) {
  return box_index < machine.stack.size() && machine.stack[box_index].pick == Pick::DebugBox &&
         machine.stack[box_index].next == number;
}

// 元の枠の語 3 だけを出口の枠にした写し。継続は Goal の値ではなく枠の語 3 から読み直される所がある（節に入るとき、
// 節と非決定的な組込のやり直し）ので、写しの語 3 を替える。元の枠は書き換えない。repeat と \+ が同じ枠を動かし直す
// とき、古い出口の枠を指したまま残らないようにするため。
std::uint64_t copy_frame(Impl& machine, std::uint64_t original, std::uint64_t exit_frame) {
  const std::size_t words = kFrameHead + read_goal(machine, original).arity;
  const std::uint64_t at = machine.heap.allocate_words(words);
  for (std::size_t index = 0; index < words; ++index) {
    machine.heap.set_word(at + index, machine.heap.word_at(original + index));
  }
  machine.heap.set_word(at + 3, exit_frame);
  return at;
}

void push_redo(Impl& machine, std::size_t box_index, std::uint64_t number) {
  Entry redo;
  redo.pick = Pick::DebugRedo;
  redo.next = number;
  redo.cut = box_index;
  redo.heap_mark = machine.heap.size();
  redo.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(redo));
}

// 再試行の印を外し、次に戻る選択点の水位まで束縛を戻してから再試行のポートを出す（計画の設計との違い 4。SWI-Prolog
// と同じく c(_G1) と見える）。印の下には必ず箱の中の選択点があるので、その水位は番兵の水位より上にある。
bool redo_box(Impl& machine) {
  const std::size_t box_index = machine.stack.back().cut;
  const std::uint64_t number = machine.stack.back().next;
  machine.stack.pop_back();
  if (!machine.stack.empty()) {
    const Entry& next = machine.stack.back();
    restore_state(machine, next.heap_mark, next.trail_mark, next.extras_mark);
  }
  if (box_at(machine, box_index, number)) {
    machine.stack[box_index].armed = true;
    // 今の debug_port は Go だけを返す。
    static_cast<void>(debug_port(machine, DebugPort::Redo, box_index, nullptr));
  }
  return false;
}

// 箱の中に試す物が残っていない。backtrack が番兵の水位まで戻した後なので、ゴールは呼んだときの形に見える。
// 抜けた番兵には必ず上に再試行の印があり、それを先に外して armed に戻すので、抜けたまま着くことは無い。
bool fail_box(Impl& machine) {
  const std::size_t box_index = machine.stack.size() - 1;
  if (machine.stack.back().armed) static_cast<void>(debug_port(machine, DebugPort::Fail, box_index, nullptr));
  machine.stack.pop_back();
  return false;
}

}  // namespace

// 安い判定から順に見る。leap の速さ（ADR 0028 の 1.2 倍）を守るためである。
bool debug_wants_box(Impl& machine, const Goal& goal) {
  // skip と leap の箱はまだ作らない。
  if (machine.debug.mode != DebugMode::Creep) return false;
  const Module* module = find_module(machine, goal.module);
  if (module != nullptr && module->library) return false;
  return !unboxed(machine.atoms.name(goal.name), goal.arity);
}

void open_box(Impl& machine, const Goal& goal) {
  Entry box;
  box.pick = Pick::DebugBox;
  box.redo_goal = goal.at;
  box.next = machine.debug.next_box++;
  box.armed = true;
  box.heap_mark = machine.heap.size();
  box.trail_mark = machine.trail.slots.size();
  push_frame(machine, std::move(box));
  run_boxed(machine, machine.stack.size() - 1);
}

// 写しは作った歩みの中で動かす。Impl にヒープの番地を持つと、次の歩みの前の GC の安全点で古くなる（計画の設計との
// 違い 11）。写しを step に戻さないので、step が写しにもう一度箱を作ることも無い。
void run_boxed(Impl& machine, std::size_t box_index) {
  const std::uint64_t original = machine.stack[box_index].redo_goal;
  Extra exit;
  exit.ctrl = Ctrl::DebugExit;
  exit.trim_to = box_index;
  exit.soft_id = machine.stack[box_index].next;
  exit.cont = read_goal(machine, original).cont;
  const std::uint64_t exit_frame = push_extra(machine, std::move(exit));
  const std::uint64_t copy = copy_frame(machine, original, exit_frame);
  // 今の debug_port は Go だけを返す。
  static_cast<void>(debug_port(machine, DebugPort::Call, box_index, nullptr));
  machine.ip = copy;
  const Goal boxed = read_goal(machine, copy);
  if (run_builtin(machine, boxed)) return;
  enter_user(machine, boxed);
}

void exit_box(Impl& machine, std::size_t extra_index) {
  const Extra& extra = machine.extras[extra_index];
  const std::size_t box_index = extra.trim_to;
  const std::uint64_t number = extra.soft_id;
  machine.ip = extra.cont;
  // 番兵が切られた後（計画の設計との違い 7）は外す物が無い。元の枠を指すのは番兵だけなので、行も書けない。
  if (!box_at(machine, box_index, number)) return;
  // 今の debug_port は Go だけを返す。
  static_cast<void>(debug_port(machine, DebugPort::Exit, box_index, nullptr));
  // 上に選択点が無ければ箱の中へ戻る道は無い。外して、決定的な答えに選択点を残さない（disarm_catch と同じ）。
  if (box_index + 1 == machine.stack.size()) {
    trim_stack(machine, box_index);
    return;
  }
  machine.stack[box_index].armed = false;
  push_redo(machine, box_index, number);
}

bool backtrack_box(Impl& machine) {
  if (machine.stack.back().pick == Pick::DebugRedo) return redo_box(machine);
  return fail_box(machine);
}

}  // namespace mosa
