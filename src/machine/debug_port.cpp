// 13211-1 に無い（ADR 0028）。デバッガのポートの行、深さ、ポートの名前の取り置き、進み方（creep、skip、leap）と
// leash による見せ方、止まったポートで読むコマンド（c s l f r a n）。
// 行は「空白、ポートの名前、": ("、深さ、") "、ゴール」。止まるポートは " ? " で終えて 1 行読み、止まらないポートは
// 改行で終える。ゴールは writeq と同じく quoted(true)、numbervars(true) に max_depth(10) を足して書く。
// 投げる誤りは無い。テスト: tests/cpp/data/debugger（transcript_debug_*）、tests/cpp/debugger_test.cpp
#include "debug_port.hpp"

#include "builtin_api.hpp"

#include "mosa/writer.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {
namespace {

constexpr std::array<std::string_view, 5> kPortKeys{"debug_port_call", "debug_port_exit", "debug_port_redo",
                                                    "debug_port_fail", "debug_port_exception"};
// ポートの行のゴールを切り詰める深さ（設計の「ポートの行」）。
constexpr int kPortDepth = 10;

// message_text は呼ぶたびに別の機械を作るので、言語ごとに 1 度だけ引いて覚える（gc.cpp の trace_form と同じ）。
// render_message は動いている問い合わせを捨てるので、問い合わせの途中のここでは使えない。
const std::string& port_name(Impl& machine, DebugPort port) {
  DebugState& state = machine.debug;
  if (!state.names_lang || *state.names_lang != machine.message_lang) {
    for (std::size_t index = 0; index < kPortKeys.size(); ++index) {
      state.port_names[index] = message_text(machine, kPortKeys[index]);
    }
    state.names_lang = machine.message_lang;
  }
  return state.port_names[static_cast<std::size_t>(port)];
}

// 開いている（まだ抜けていない）箱の数。指令の中では、その指令が脇に置いたスタックの箱だけを数える。
std::size_t box_depth(const Impl& machine, std::size_t box_index) {
  std::size_t depth = 0;
  for (std::size_t index = 0; index <= box_index && index < machine.stack.size(); ++index) {
    const Entry& entry = machine.stack[index];
    if (entry.pick == Pick::DebugBox && entry.armed) ++depth;
  }
  return depth;
}

// 枠の引数を今の束縛のまま項にする。項はヒープの上の捨て物で、次の GC か巻き戻しで消える。
Cell goal_term(Impl& machine, const Goal& goal) {
  if (goal.arity == 0) return machine.heap.atom(goal.name);
  std::vector<Cell> args;
  args.reserve(goal.arity);
  for (std::size_t index = 0; index < goal.arity; ++index) args.push_back(goal.arg(machine, index));
  return machine.heap.structure(goal.name, args);
}

// portray/1 は呼ばない。ポートの中で利用者のゴールを動かさないため。演算子はゴールの文脈モジュールの見え方で書く
// （emit_written と同じ、ADR 0006）。ゴールと投げられた項は 1 つの番号付けで書き、同じ変数は同じ _G の名前になる。
std::string goal_text(Impl& machine, std::size_t box_index, const Cell* ball) {
  const Goal goal = read_goal(machine, machine.stack[box_index].redo_goal);
  WriteOptions options;
  options.quoted = true;
  options.numbervars = true;
  options.max_depth = kPortDepth;
  options.fullwidth = machine.japanese_punct;
  const Module* found = goal.module == machine.user_module ? nullptr : find_module(machine, goal.module);
  const OperatorTable local = found == nullptr ? OperatorTable{} : found->ops;
  const OperatorView view = found == nullptr ? OperatorView{machine.operators} : OperatorView{machine.operators, &local};
  std::vector<Cell> terms{goal_term(machine, goal)};
  if (ball != nullptr) terms.push_back(*ball);
  const std::vector<std::string> texts = write_terms(machine.heap, view, terms, options, {});
  std::string text = texts.front();
  if (texts.size() > 1) text += " " + texts[1];
  return text;
}

// 箱のゴールにスパイポイントがあるか。leap の箱と同じく、呼んだ文脈から spy/1 と同じ引き当てで見る。
bool box_spied(Impl& machine, std::size_t box_index) {
  const Goal goal = read_goal(machine, machine.stack[box_index].redo_goal);
  return spied_call(machine, goal.module, goal.name, goal.arity);
}

// 進み方でポートを見せるかを決める。leap はスパイポイントの述語の箱だけを見せる。端末でない mosa でも、書かれるのは
// それだけになる（設計の「コマンド」）。skip は skip_box の箱だけを見せ、着いたら creep に戻ってそこで止まる。
bool port_visible(Impl& machine, std::size_t box_index) {
  DebugState& state = machine.debug;
  if (state.mode == DebugMode::Creep) return true;
  if (state.mode == DebugMode::Leap) return box_spied(machine, box_index);
  if (machine.stack[box_index].next != state.skip_box) return false;
  state.skip_box = 0;
  state.mode = DebugMode::Creep;
  return true;
}

// 行の最初の空白でない文字。空の行と改行だけの行は creep。
char command_char(std::string_view line) {
  for (const char letter : line) {
    if (letter != ' ' && letter != '\t' && letter != '\n' && letter != '\r') return letter;
  }
  return '\0';
}

// コマンドの表（タスク 6）。ポートで働きが変わるのは s（呼出と再試行だけ skip）、f（失敗では creep）、r（呼出では
// creep）。a は進み方を変えない（abort の後もデバッグの状態は変えない、設計の「コマンド」）。
PortAction apply_command(Impl& machine, DebugPort port, std::size_t box_index, char command) {
  DebugState& state = machine.debug;
  if (command == 'a') return PortAction::Abort;
  if (command == 'n') {
    // 進み方も leap に戻す。後の spy/1 が creep で動き出さないように（計画の設計との違い 12）。
    set_debug(machine, false, DebugMode::Leap);
    return PortAction::Go;
  }
  state.mode = DebugMode::Creep;
  if (command == 'l') state.mode = DebugMode::Leap;
  if (command == 's' && (port == DebugPort::Call || port == DebugPort::Redo)) {
    state.mode = DebugMode::Skip;
    state.skip_box = machine.stack[box_index].next;
  }
  if (command == 'f' && port != DebugPort::Fail) return PortAction::Fail;
  if (command == 'r' && port != DebugPort::Call) return PortAction::Retry;
  return PortAction::Go;
}

}  // namespace

PortAction debug_port(Impl& machine, DebugPort port, std::size_t box_index, const Cell* ball) {
  // n と nodebug の後は、まだ開いている箱もポートを出さない。
  if (!machine.debug_on || !machine.debug.console) return PortAction::Go;
  if (!port_visible(machine, box_index)) return PortAction::Go;
  std::string line = " " + port_name(machine, port) + ": (" + std::to_string(box_depth(machine, box_index)) + ") " +
                     goal_text(machine, box_index, ball);
  const DebugConsole& console = *machine.debug.console;
  // 読む口が無ければ（端末でない mosa）、どのポートも止まらないポートとして書くだけで進む。leash に無いポートも同じ。
  const auto bit = static_cast<std::uint8_t>(1U << static_cast<unsigned>(port));
  const bool stops = static_cast<bool>(console.read_line) && (machine.debug.leashed & bit) != 0;
  line += stops ? " ? " : "\n";
  if (console.write) console.write(line);
  if (!stops) return PortAction::Go;
  std::string command;
  // 入力の終わりでも行は閉じる。そのときは creep で進む。
  if (!console.read_line(command)) {
    if (console.write) console.write("\n");
    return PortAction::Go;
  }
  return apply_command(machine, port, box_index, command_char(command));
}

}  // namespace mosa
