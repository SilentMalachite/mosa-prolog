#include "mosa/repl.hpp"

#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/version.hpp"
#include "mosa/writer.hpp"

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

std::string reason_text(SyntaxReason reason) {
  switch (reason) {
    case SyntaxReason::InvalidUtf8: return "不正な UTF-8";
    case SyntaxReason::IllegalCharacter: return "使えない文字";
    case SyntaxReason::UnterminatedQuote: return "引用が閉じていない";
    case SyntaxReason::UnterminatedComment: return "コメントが閉じていない";
    case SyntaxReason::BadEscape: return "不正なエスケープ";
    case SyntaxReason::IntegerOverflow: return "整数が範囲を超えている";
    case SyntaxReason::UnexpectedToken: return "予期しないトークン";
    case SyntaxReason::MaxArity: return "引数が多すぎる";
    case SyntaxReason::IncompleteTerm: return "項が閉じていません";
  }
  return "予期しないトークン";
}

std::string answer_line(const Machine& machine, const Stop& stop, bool period) {
  std::string text;
  if (stop.kind == StopKind::Success) text = format_answer(machine, stop);
  else if (stop.kind == StopKind::Failure) text = "false";
  else text = "false";
  if (period) text += '.';
  text += '\n';
  return text;
}

void warn_resource(SessionResult& result) { result.err += "警告: ヒープの上限を超えました\n"; }

void recover_heap(Machine& machine) { machine.heap().truncate(0); }

void warn_exception(Machine& machine, const Stop& stop, std::string& err) {
  err += "警告: ゴールは例外を送出しました: ";
  err += write_term(machine.heap(), machine.operators(), stop.exception, {}, {});
  err += '\n';
}

bool directive_text(Machine& machine, std::string_view text) {
  return machine.load_program(text).ok;
}

int play_goal(Machine& machine, std::string goal, bool interactive, Cursor& cursor, SessionResult& result,
              bool& halted) {
  halted = false;
  try {
    Stop stop = machine.query(goal);
    while (true) {
      if (stop.kind == StopKind::Halt) {
        halted = true;
        return stop.halt_code;
      }
      if (stop.kind == StopKind::Exception) {
        warn_exception(machine, stop, result.err);
        return interactive ? 0 : 2;
      }
      if (stop.kind == StopKind::Failure) {
        result.out += "false.\n";
        return 1;
      }
      if (!interactive || !stop.more) {
        result.out += answer_line(machine, stop, true);
        return 0;
      }
      result.out += format_answer(machine, stop);
      const int ch = cursor.get();
      if (ch == ';') {
        result.out += '\n';
        stop = machine.next();
        continue;
      }
      result.out += ".\n";
      return 0;
    }
  } catch (const ResourceLimit&) {
    recover_heap(machine);
    warn_resource(result);
    return interactive ? 0 : 2;
  }
}

bool blank(std::string_view text) {
  for (const char ch : text) {
    if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') return false;
  }
  return true;
}

void prompt_loop(Machine& machine, Cursor& cursor, SessionResult& result) {
  std::string buffer;
  result.out += "?- ";
  while (true) {
    std::string line;
    if (!cursor.line(line)) return;
    if (buffer.empty() && blank(line)) continue;
    buffer += line;
    ReadOutcome outcome;
    try {
      Reader reader(buffer, machine.heap(), machine.operators());
      outcome = reader.next();
    } catch (const ResourceLimit&) {
      recover_heap(machine);
      warn_resource(result);
      buffer.clear();
      result.out += "?- ";
      continue;
    }
    if (outcome.failure && outcome.failure->reason == SyntaxReason::IncompleteTerm) continue;
    if (outcome.failure) {
      result.err += reason_text(outcome.failure->reason);
      result.err += '\n';
      buffer.clear();
      result.out += "?- ";
      continue;
    }
    if (!outcome.term) {
      buffer.clear();
      result.out += "?- ";
      continue;
    }
    const std::string text = buffer;
    buffer.clear();
    const Cell term = outcome.term->term;
    const bool directive = term.tag() == Tag::Str && machine.heap().functor_arity(machine.heap().deref(term)) == 1 &&
                           (machine.atoms().name(machine.heap().functor_atom(machine.heap().deref(term))) == ":-" ||
                            machine.atoms().name(machine.heap().functor_atom(machine.heap().deref(term))) == "?-");
    if (directive) {
      if (!directive_text(machine, text)) result.err += std::string{machine.diagnostic()} + "\n";
    } else {
      const std::string goal =
          write_term(machine.heap(), machine.operators(), term, {}, outcome.term->names) + ".";
      bool halted = false;
      const int code = play_goal(machine, goal, true, cursor, result, halted);
      if (halted) {
        result.code = code;
        return;
      }
    }
    result.out += "?- ";
  }
}

}  // namespace

SessionResult run_session(std::span<const std::string_view> args, std::string_view input, bool tty,
                           std::size_t heap_limit) {
  SessionResult result;
  const Options options = parse_args(args);
  if (options.version) {
    result.out = version_line() + "\n";
    return result;
  }
  if (options.bad || (options.top_halt && !options.has_goal)) {
    result.err = usage;
    result.code = 1;
    return result;
  }
  if (!tty && !options.has_goal && options.files.empty()) {
    result.err = usage;
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
        result.err += std::string{machine->diagnostic()};
        result.err += '\n';
        result.code = 1;
        return result;
      }
    } catch (const ResourceLimit&) {
      recover_heap(*machine);
      warn_resource(result);
      result.code = 2;
      return result;
    }
  }
  if (!tty && !options.has_goal) {
    result.err = usage;
    result.code = 1;
    return result;
  }
  if (options.has_goal) {
    const bool interactive = tty && !options.top_halt;
    bool halted = false;
    const int code = play_goal(*machine, options.goal, interactive, cursor, result, halted);
    result.code = code;
    if (halted || !interactive || code == 2) return result;
    result.code = 0;
  }
  if (tty && !options.top_halt) prompt_loop(*machine, cursor, result);
  return result;
}

}
