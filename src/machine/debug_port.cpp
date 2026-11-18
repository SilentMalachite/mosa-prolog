// 13211-1 に無い（ADR 0028）。デバッガのポートの行、深さ、ポートの名前の取り置き、進み方（creep、skip、leap）と
// leash による見せ方、止まったポートで読むコマンド。
//   働きのあるコマンド: 空の行と c、s、l、f、r、a、n（a は Impl::abort_now を立てる）。
//   書くだけのコマンド: g、w、+、-、h、?、知らない入力。書いた後で同じポートの行をもう一度書いて問う。
//   入力の終わり（端末の Ctrl-D）: 改行を 1 つ書いて n と同じにする（計画の設計との違い 8）。
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
// ポートの行のゴールを切り詰める深さ（設計の「ポートの行」）。0 は切らない（w）。
constexpr int kPortDepth = 10;
// 働きのあるコマンドの字。'\0' は空の行で creep。
constexpr std::string_view kActions{"cslfran"};
constexpr std::string_view kBlanks{" \t\n\r"};

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
std::string goal_text(Impl& machine, std::size_t box_index, const Cell* ball, int depth) {
  const Goal goal = read_goal(machine, machine.stack[box_index].redo_goal);
  WriteOptions options;
  options.quoted = true;
  options.numbervars = true;
  options.max_depth = depth;
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

// 行の最初の空白でない文字。空の行と改行だけの行は creep。
char command_char(std::string_view line) {
  const auto start = line.find_first_not_of(kBlanks);
  return start == std::string_view::npos ? '\0' : line[start];
}

// 知らない入力の知らせに書く字。UTF-8 の 1 字を割らずに切り出す（あ や全角の ｈ を打ったとき）。壊れたバイトは
// U+FFFD にして、知らせの行を UTF-8 のままにする。
std::string first_char(std::string_view line) {
  const auto start = line.find_first_not_of(kBlanks);
  if (start == std::string_view::npos) return {};
  const CodeText text = decode_codes(line.substr(start, 4));
  if (text.cuts[1] == 1 && static_cast<unsigned char>(line[start]) >= 0x80) return "\xEF\xBF\xBD";
  return std::string{text.slice(0, 1)};
}

void write_text(const Impl& machine, std::string_view text) {
  const DebugConsole& console = *machine.debug.console;
  if (console.write && !text.empty()) console.write(text);
}

// g。box_index から下の開いている箱を内側から、深さとゴールだけのポートの行の形で書く（設計のコマンドの表）。
void write_goals(Impl& machine, std::size_t box_index) {
  std::size_t depth = box_depth(machine, box_index);
  std::string text;
  for (std::size_t index = box_index + 1; index-- > 0 && depth > 0;) {
    const Entry& entry = machine.stack[index];
    if (entry.pick != Pick::DebugBox || !entry.armed) continue;
    text += " (" + std::to_string(depth) + ") " + goal_text(machine, index, nullptr, kPortDepth) + "\n";
    --depth;
  }
  write_text(machine, text);
}

// + と -。この箱のゴールの文脈から見た述語に、spy/1 と nospy/1 の Name/Arity と同じ置き方で置くか外す。知らせは
// コマンドが書く行なので、ポートの行と同じ口に書く。
void toggle_spy(Impl& machine, std::size_t box_index, bool on) {
  const Goal goal = read_goal(machine, machine.stack[box_index].redo_goal);
  write_text(machine, spy_command(machine, goal.module, goal.name, goal.arity, on));
}

// 書くだけのコマンドなら書いて真。進み方は変えない（leap で止まったポートの h の後も leap のまま）。偽なら働きの
// あるコマンドで、apply_command に渡す。w は深さを切らないポートの行のゴールで、例外のポートでは投げられた項も書く。
bool show_command(Impl& machine, std::size_t box_index, const Cell* ball, std::string_view line) {
  const char letter = command_char(line);
  if (letter == 'g') write_goals(machine, box_index);
  else if (letter == 'w') write_text(machine, goal_text(machine, box_index, ball, 0) + "\n");
  else if (letter == '+' || letter == '-') toggle_spy(machine, box_index, letter == '+');
  else if (letter == 'h' || letter == '?') write_text(machine, message_text(machine, "debug_help") + "\n");
  else if (letter == '\0' || kActions.find(letter) != std::string_view::npos) return false;
  else write_text(machine, message_text(machine, "debug_unknown_command", {first_char(line)}) + "\n");
  return true;
}

// 働きのあるコマンド（タスク 6）。ポートで働きが変わるのは s（呼出と再試行だけ skip）、f（失敗では creep）、r（呼出
// では creep）。a は進み方を変えない（abort の後もデバッグの状態は変えない、設計の「コマンド」）。
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

// 止まるポートで問う。書くだけのコマンドの後は同じ行をもう一度書いて問い、入力の終わりはどの問いでも n と同じ。
PortAction ask(Impl& machine, DebugPort port, std::size_t box_index, const Cell* ball, const std::string& line) {
  const DebugConsole& console = *machine.debug.console;
  std::string command;
  for (;;) {
    write_text(machine, line + " ? ");
    if (!console.read_line(command)) {
      // 端末は Ctrl-D の後に改行を映さないので、行を閉じてから続ける。
      write_text(machine, "\n");
      set_debug(machine, false, DebugMode::Leap);
      return PortAction::Go;
    }
    if (show_command(machine, box_index, ball, command)) continue;
    const PortAction action = apply_command(machine, port, box_index, command_char(command));
    // abort は halt と同じ形で止める。印は run_loop が次の歩みの前に見る。呼び手（箱の出入り、propagate_throw）は
    // この歩みでそれ以上何も動かさずに戻る。
    if (action == PortAction::Abort) {
      machine.abort_now = true;
      machine.ip = kNoFrame;
      end_skip(machine.debug);
    }
    return action;
  }
}

}  // namespace

// leap はスパイポイントの述語の箱だけを見せる。端末でない mosa でも、書かれるのはそれだけになる（設計の
// 「コマンド」）。skip は skip_box の箱だけを見せる。
bool debug_port_shown(Impl& machine, std::size_t box_index) {
  if (!machine.debug_on || !machine.debug.console) return false;
  const DebugState& state = machine.debug;
  if (state.mode == DebugMode::Creep) return true;
  if (state.mode == DebugMode::Leap) return box_spied(machine, box_index);
  return machine.stack[box_index].next == state.skip_box;
}

PortAction debug_port(Impl& machine, DebugPort port, std::size_t box_index, const Cell* ball) {
  // n と nodebug の後は、まだ開いている箱もポートを出さない。
  if (!debug_port_shown(machine, box_index)) return PortAction::Go;
  // skip は skip_box の箱のポートに着いたら creep に戻り、そこで止まる。
  end_skip(machine.debug);
  const std::string line = " " + port_name(machine, port) + ": (" + std::to_string(box_depth(machine, box_index)) +
                           ") " + goal_text(machine, box_index, ball, kPortDepth);
  // 読む口が無ければ（端末でない mosa）、どのポートも止まらないポートとして書くだけで進む。leash に無いポートも同じ。
  const auto bit = static_cast<std::uint8_t>(1U << static_cast<unsigned>(port));
  const bool stops = static_cast<bool>(machine.debug.console->read_line) && (machine.debug.leashed & bit) != 0;
  if (stops) return ask(machine, port, box_index, ball, line);
  write_text(machine, line + "\n");
  return PortAction::Go;
}

}  // namespace mosa
