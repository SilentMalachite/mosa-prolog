#include "mosa/repl.hpp"

#include "internal.hpp"

#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/messages.hpp"
#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/version.hpp"
#include "mosa/writer.hpp"

#include <array>
#include <cstdio>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct Cursor {
  std::string_view data;
  std::size_t at = 0;
  bool from_cin = false;

  bool line(std::string& out) {
    if (from_cin) {
      if (!std::getline(std::cin, out)) return false;
      out.push_back('\n');
      return true;
    }
    if (at >= data.size()) return false;
    const auto end = data.find('\n', at);
    if (end == std::string_view::npos) {
      out = std::string{data.substr(at)};
      at = data.size();
    } else {
      out = std::string{data.substr(at, end - at + 1)};
      at = end + 1;
    }
    return true;
  }

  int get() {
    if (from_cin) return std::cin.get();
    if (at >= data.size()) return EOF;
    return static_cast<unsigned char>(data[at++]);
  }
};

struct Options {
  std::vector<std::string> files;
  std::string goal;
  bool has_goal = false;
  bool top_halt = false;
  bool version = false;
  bool bad = false;
};

Options parse_args(std::span<const std::string_view> args) {
  Options options;
  for (std::size_t index = 1; index < args.size(); ++index) {
    const auto arg = args[index];
    if (arg == "--version") {
      options.version = true;
      continue;
    }
    if (arg == "-g") {
      if (options.has_goal || index + 1 >= args.size()) {
        options.bad = true;
        return options;
      }
      options.has_goal = true;
      options.goal = std::string{args[++index]};
      continue;
    }
    if (arg == "-t") {
      if (options.top_halt || index + 1 >= args.size() || args[index + 1] != "halt") {
        options.bad = true;
        return options;
      }
      options.top_halt = true;
      ++index;
      continue;
    }
    if (!arg.empty() && arg.front() == '-') {
      options.bad = true;
      return options;
    }
    options.files.emplace_back(arg);
  }
  return options;
}

const char* usage = "mosa: 使い方: mosa [ファイル...] [-g ゴール] [-t halt]\n";

struct Echo {
  SessionResult* result = nullptr;
  std::ostream* out = nullptr;
  std::ostream* err = nullptr;

  void say(std::string_view text) const {
    result->out.append(text);
    if (out != nullptr) {
      *out << text;
      out->flush();
    }
  }

  void warn(std::string_view text) const {
    result->err.append(text);
    if (err != nullptr) {
      *err << text;
      err->flush();
    }
  }
};

std::string answer_line(const Machine& machine, const Stop& stop, bool period) {
  std::string text;
  if (stop.kind == StopKind::Success) text = format_answer(machine, stop);
  else if (stop.kind == StopKind::Failure) text = "false";
  else text = "false";
  if (period) text += '.';
  text += '\n';
  return text;
}

// 上限に達したマシンのヒープではカタログを読めないので、新しいマシンで文面を作る。
void warn_resource(Machine& machine, Echo& echo) {
  echo.warn(render_message_fresh(engine_impl(machine).message_lang, "heap_limit") + "\n");
}

void recover_heap(Machine& machine) { machine.heap().truncate(0); }

void warn_exception(Machine& machine, const Stop& stop, Echo& echo) {
  const std::array<Cell, 1> args{stop.exception};
  echo.warn(render_message(machine, "goal_raised", args) + "\n");
}

// 理由の鍵の文面を先に作り、それを行と列とトークンの文面に差し込む。入力の終わりでトークンは無い。
void warn_syntax(Machine& machine, const SyntaxFailure& failure, Echo& echo) {
  const std::string reason = render_message(machine, syntax_key(failure.reason), {});
  Heap& heap = machine.heap();
  std::vector<Cell> args{heap.integer(static_cast<std::int64_t>(failure.line)),
                         heap.integer(static_cast<std::int64_t>(failure.column)),
                         heap.atom(machine.atoms().intern(reason))};
  if (!failure.token.empty()) args.push_back(heap.atom(machine.atoms().intern(failure.token)));
  echo.warn(render_message(machine, failure.token.empty() ? "syntax_error_at_end" : "syntax_error", args) + "\n");
}

bool directive_text(Machine& machine, std::string_view text) {
  return machine.load_program(text).ok;
}

int play_goal(Machine& machine, std::string goal, bool interactive, Cursor& cursor, Echo& echo, bool& halted) {
  halted = false;
  try {
    Stop stop = machine.query(goal);
    while (true) {
      if (stop.kind == StopKind::Halt) {
        halted = true;
        return stop.halt_code;
      }
      if (stop.kind == StopKind::Exception) {
        warn_exception(machine, stop, echo);
        return interactive ? 0 : 2;
      }
      if (stop.kind == StopKind::Failure) {
        // consult/1 の失敗は、ゴールを失敗させる前にパス付きの理由を出す。
        const auto diag = machine.diagnostic();
        if (!diag.empty()) {
          echo.warn(diag);
          echo.warn("\n");
        }
        echo.say("false.\n");
        return 1;
      }
      if (!interactive || !stop.more) {
        echo.say(answer_line(machine, stop, true));
        return 0;
      }
      // ; を読む前に答えを出す。終了時にまとめて書くと、対話では次の入力を判断できない。
      echo.say(format_answer(machine, stop));
      const int ch = cursor.get();
      if (ch == ';') {
        echo.say("\n");
        stop = machine.next();
        continue;
      }
      echo.say(".\n");
      return 0;
    }
  } catch (const ResourceLimit&) {
    recover_heap(machine);
    warn_resource(machine, echo);
    return interactive ? 0 : 2;
  }
}

bool blank(std::string_view text) {
  for (const char ch : text) {
    if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') return false;
  }
  return true;
}

void prompt_loop(Machine& machine, Cursor& cursor, Echo& echo) {
  std::string buffer;
  echo.say("?- ");
  while (true) {
    std::string line;
    if (!cursor.line(line)) return;
    if (buffer.empty() && blank(line)) continue;
    buffer += line;
    ReadOutcome outcome;
    try {
      Reader reader(buffer, machine.heap(), machine.operators(), current_double_quotes(engine_impl(machine)),
                    &engine_impl(machine).char_conv);
      reader.set_fullwidth(engine_impl(machine).japanese_punct);
      outcome = reader.next();
    } catch (const ResourceLimit&) {
      recover_heap(machine);
      warn_resource(machine, echo);
      buffer.clear();
      echo.say("?- ");
      continue;
    }
    if (outcome.failure && outcome.failure->reason == SyntaxReason::IncompleteTerm) continue;
    if (outcome.failure) {
      warn_syntax(machine, *outcome.failure, echo);
      buffer.clear();
      echo.say("?- ");
      continue;
    }
    if (!outcome.term) {
      buffer.clear();
      echo.say("?- ");
      continue;
    }
    const std::string text = buffer;
    buffer.clear();
    const Cell term = outcome.term->term;
    // help は REPL の命令で、述語ではない。user の名前を増やさない（SPEC 第4節）。
    if (term.tag() == Tag::Atom && machine.atoms().name(static_cast<std::uint32_t>(term.raw() >> 3)) == "help") {
      echo.say(render_message(machine, "help_japanese_variables", {}) + "\n");
      echo.say("?- ");
      continue;
    }
    const bool directive = term.tag() == Tag::Str && machine.heap().functor_arity(machine.heap().deref(term)) == 1 &&
                           (machine.atoms().name(machine.heap().functor_atom(machine.heap().deref(term))) == ":-" ||
                            machine.atoms().name(machine.heap().functor_atom(machine.heap().deref(term))) == "?-");
    if (directive) {
      if (!directive_text(machine, text)) echo.warn(std::string{machine.diagnostic()} + "\n");
    } else {
      const std::string goal =
          write_term(machine.heap(), machine.operators(), term, {}, outcome.term->names) + ".";
      bool halted = false;
      const int code = play_goal(machine, goal, true, cursor, echo, halted);
      if (halted) {
        echo.result->code = code;
        return;
      }
    }
    echo.say("?- ");
  }
}

}  // namespace

SessionResult run_session(std::span<const std::string_view> args, std::string_view input, bool tty,
                           std::size_t heap_limit, std::ostream* live_out, std::ostream* live_err) {
  SessionResult result;
  Echo echo;
  echo.result = &result;
  echo.out = live_out;
  echo.err = live_err;
  const Options options = parse_args(args);
  if (options.version) {
    echo.say(version_line() + "\n");
    return result;
  }
  if (options.bad || (options.top_halt && !options.has_goal)) {
    echo.warn(usage);
    result.code = 1;
    return result;
  }
  if (!tty && !options.has_goal && options.files.empty()) {
    echo.warn(usage);
    result.code = 1;
    return result;
  }
  Cursor cursor;
  cursor.data = input;
  cursor.from_cin = tty && input.empty();
  auto machine = Machine::create(heap_limit);
  for (const std::string& file : options.files) {
    try {
      if (!machine->consult(file)) {
        echo.warn(std::string{machine->diagnostic()} + "\n");
        result.code = 1;
        return result;
      }
    } catch (const ResourceLimit&) {
      recover_heap(*machine);
      warn_resource(*machine, echo);
      result.code = 2;
      return result;
    }
  }
  if (!tty && !options.has_goal) {
    echo.warn(usage);
    result.code = 1;
    return result;
  }
  if (options.has_goal) {
    const bool interactive = tty && !options.top_halt;
    bool halted = false;
    const int code = play_goal(*machine, options.goal, interactive, cursor, echo, halted);
    result.code = code;
    if (halted || !interactive || code == 2) return result;
    result.code = 0;
  }
  if (tty && !options.top_halt) prompt_loop(*machine, cursor, echo);
  return result;
}

}
