#include "mosa/repl.hpp"

#include "internal.hpp"

#include "mosa/console.hpp"
#include "mosa/heap.hpp"
#include "mosa/heap_option.hpp"
#include "mosa/machine.hpp"
#include "mosa/messages.hpp"
#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/version.hpp"
#include "mosa/writer.hpp"

#include <array>
#include <cstdio>
#include <iostream>
#include <optional>
#include <span>
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
      // read/1 と同じ読み取り口を通す。Windows のコンソールは UTF-16 で読む（SPEC 第4節）。
      out.clear();
      for (int ch = standard_input_get(); ch != EOF; ch = standard_input_get()) {
        out.push_back(static_cast<char>(ch));
        if (ch == '\n') return true;
      }
      if (out.empty()) return false;
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

  // 答えの後の ; の促しの答え。1 行で読み、最初の空白でない字を返す。空の行は '\n'、入力の終わりは EOF。
  // 1 バイトずつ読むと、; の後の Enter が次の促しの答えになる（レビュー L15）。read/1 が終止符を確かめて返した字
  // （foo. の後の改行）は答えではないので先に捨てる。
  int answer() {
    if (from_cin) standard_input_drop_returned_line();
    std::string text;
    if (!line(text)) return EOF;
    for (const char ch : text) {
      if (ch != ' ' && ch != '\t' && ch != '\r') return static_cast<unsigned char>(ch);
    }
    return '\n';
  }
};

struct Options {
  std::vector<std::string> files;
  std::vector<std::string> goals;  // -g は書いた順に動かす（SWI-Prolog と同じく繰り返してよい。レビュー L10）
  std::vector<std::string> program_args;
  bool top_halt = false;
  bool version = false;
  bool bad = false;
  std::optional<std::size_t> heap_max;  // --heap-max の MiB を語にした値
};

Options parse_args(std::span<const std::string_view> args) {
  Options options;
  for (std::size_t index = 1; index < args.size(); ++index) {
    const auto arg = args[index];
    // -- より後ろは全部プログラムの引数（旗 argv）。mosa のオプションとして読まない。
    if (arg == "--") {
      options.program_args.assign(args.begin() + static_cast<std::ptrdiff_t>(index) + 1, args.end());
      return options;
    }
    if (arg == "--version") {
      options.version = true;
      continue;
    }
    if (arg == "--heap-max") {
      // 値は 1 以上の十進整数 MiB。無い、または解釈できなければ使い方の誤り。
      const auto words = index + 1 < args.size() ? parse_heap_max_mib(args[index + 1]) : std::nullopt;
      if (!words) {
        options.bad = true;
        return options;
      }
      options.heap_max = words;
      ++index;
      continue;
    }
    if (arg == "-g") {
      if (index + 1 >= args.size()) {
        options.bad = true;
        return options;
      }
      options.goals.emplace_back(args[++index]);
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
  if (period) text += full_stop_after(text);
  text += '\n';
  return text;
}

// 上限に達したマシンのヒープではカタログを読めないので、新しいマシンで文面を作る。
void warn_resource(Machine& machine, Echo& echo) {
  echo.warn(heap_limit_text(engine_impl(machine)) + "\n");
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

// abort で止まった問い合わせは、ここで捨てる（ADR 0028）。abort_now を残すと、次のプロンプトの指令（load_program）の
// 入れ子の run_loop がそれを見て止まる。後始末は abort が止まる前に run_loop が動かし終えている（ADR 0028）ので、
// drop_run は残りを選択点ごと捨てる。知らせは 1 度だけ書く。文面は message_text で引く。
void report_abort(Machine& machine, Echo& echo) {
  drop_run(engine_impl(machine));
  echo.warn(message_text(engine_impl(machine), "debug_aborted") + "\n");
}

bool directive_text(Machine& machine, std::string_view text) {
  return machine.load_program(text).ok;
}

// 選択点を残した答えで終わるゴールは、SWI-Prolog の toplevel と '$run_init_goal' と同じく答えの後で切り、
// setup_call_cleanup/3 の後始末を動かす（ADR 0016、敵対的レビュー A3）。その例外はゴールの例外として出す。
int close_answer(Machine& machine, bool interactive, Echo& echo, bool& halted) {
  const Stop closed = close_query(engine_impl(machine));
  if (closed.kind == StopKind::Halt) {
    halted = true;
    return closed.halt_code;
  }
  // 後始末のゴールのポートの a。ゴールが失敗したのと同じ値にする（-g の一括は 1 で終わる）。
  if (closed.kind == StopKind::Abort) {
    report_abort(machine, echo);
    return 1;
  }
  if (closed.kind == StopKind::Exception) {
    warn_exception(machine, closed, echo);
    return interactive ? 0 : 2;
  }
  return 0;
}

// written は goal が Mosa の書いた字面であること。char_conversion なしで読み直す（レビュー L17）。
int play_goal(Machine& machine, std::string goal, bool written, bool interactive, Cursor& cursor, Echo& echo,
              bool& halted) {
  halted = false;
  try {
    Stop stop = written ? query_written(machine, goal) : machine.query(goal);
    while (true) {
      if (stop.kind == StopKind::Halt) {
        halted = true;
        return stop.halt_code;
      }
      // -g のゴールの abort は、そのゴールの失敗と同じに扱う（設計の「コマンド」）。答えの行は書かない。
      if (stop.kind == StopKind::Abort) {
        report_abort(machine, echo);
        return 1;
      }
      if (stop.kind == StopKind::Exception) {
        warn_exception(machine, stop, echo);
        return interactive ? 0 : 2;
      }
      if (stop.kind == StopKind::Failure) {
        // consult/1 の失敗は、ゴールを失敗させる前にパス付きの理由を出す。ゴールがその場で書いた理由は繰り返さない
        // （フェーズ12の問い 1 の A）。
        const auto diag = machine.diagnostic();
        if (!diag.empty() && diag != engine_impl(machine).reported_diagnostic) {
          echo.warn(diag);
          echo.warn("\n");
        }
        echo.say("false.\n");
        return 1;
      }
      if (!interactive || !stop.more) {
        echo.say(answer_line(machine, stop, true));
        return close_answer(machine, interactive, echo, halted);
      }
      // ; を読む前に答えを出す。終了時にまとめて書くと、対話では次の入力を判断できない。
      const std::string answer = format_answer(machine, stop);
      echo.say(answer);
      const int ch = cursor.answer();
      if (ch == ';') {
        echo.say("\n");
        stop = machine.next();
        continue;
      }
      echo.say(full_stop_after(answer) + "\n");
      return close_answer(machine, interactive, echo, halted);
    }
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    recover_heap(machine);
    warn_resource(machine, echo);
    return interactive ? 0 : 2;
  }
}

// 文面は run_main_goal が作る。mosac の実行ファイルも同じ関数で終わる（計画 Task 5）。
int play_main(Machine& machine, const ExportedTerm& goal, Echo& echo) {
  std::string warning;
  const int code = run_main_goal(machine, goal, warning);
  if (!warning.empty()) echo.warn(warning);
  return code;
}

ReadOutcome read_goal_text(Machine& machine, std::string_view text) {
  Reader reader(text, machine.heap(), machine.operators(), current_double_quotes(engine_impl(machine)),
                &engine_impl(machine).char_conv);
  reader.set_fullwidth(engine_impl(machine).japanese_punct);
  return reader.next();
}

// -g のゴールは末尾の . を省いてよい（SWI-Prolog と同じ）。終わりが無ければ改行と . を足す。
// 改行を挟むのは、行末の % 注釈を閉じ、記号の文字と . がつながって一つの原子にならないようにするため。
// 読めないゴールは構文エラーを出して nullopt。query は読めない文を false にするので、先に読んで見分ける。
std::optional<std::string> command_goal(Machine& machine, const std::string& goal, Echo& echo) {
  try {
    const ReadOutcome outcome = read_goal_text(machine, goal);
    if (!outcome.failure) return goal;
    // 足した . でも読めなければ、利用者の書いた文の位置で知らせる。足した 2 行目を見せない。
    if (outcome.failure->reason == SyntaxReason::IncompleteTerm &&
        !read_goal_text(machine, goal + "\n.").failure) {
      return goal + "\n.";
    }
    warn_syntax(machine, *outcome.failure, echo);
    return std::nullopt;
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    // 読むだけで溢れる小さな上限。判定を諦め、play_goal に資源の報告を任せる。
    recover_heap(machine);
  }
  return goal;
}

bool blank(std::string_view text) {
  for (const char ch : text) {
    if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') return false;
  }
  return true;
}

// 読んだゴールを、play_goal が同じ旗で読み直す字面にする。旗が真なら '、' などを引用しないと , に読まれる（レビュー M8）。
// 読んだ項の名前の無い変数は _ だけで、どれも 1 度しか出ない。_G1 と書くと名前の付いた問い合わせの変数として読み直され、
// 答えに _G1 = a や _G1 = _G2 が出る（レビュー L18、M26 の敵対的レビュー A1）。
std::string goal_text(const Machine& machine, Cell term, std::span<const VarName> names) {
  WriteOptions options;
  options.fullwidth = engine_impl(machine).japanese_punct;
  // 読み直しは char_conversion なしなので、_ が写されていても別々の無名変数のまま戻る（M26 の Codex レビュー）。
  options.underscore_once = true;
  const std::string text = write_term(machine.heap(), machine.operators(), term, options, names);
  return text + full_stop_after(text);
}

// 指示の halt/0,1 は読み込みを止める。ファイルの指示（consult_files）と同じく、その値で終える。
// 止まった読み込みの診断は空なので、警告にしない。終えるなら true。ヒープの上限で止まった指示は、読み込みを
// load_program が巻き戻したので、ゴール（play_goal）と同じく知らせて次の入力へ進む。前は run_session の catch まで
// 抜け、対話を 2 で終えた（2 度目の Muse レビューの修正の Opus 再レビュー A1）。
bool prompt_directive(Machine& machine, std::string_view text, Echo& echo) {
  try {
    if (directive_text(machine, text)) return false;
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    recover_heap(machine);
    warn_resource(machine, echo);
    return false;
  }
  if (engine_impl(machine).halt_now) {
    echo.result->code = engine_impl(machine).halt_code;
    return true;
  }
  // 指令の中の abort は読み込みを戻し、診断は空にしてある。空の行の代わりに知らせを書いて次の入力へ進む。
  if (engine_impl(machine).abort_now) {
    report_abort(machine, echo);
    return false;
  }
  echo.warn(std::string{machine.diagnostic()} + "\n");
  return false;
}

// 促し。デバッグ中は、今どの進み方かを学ぶ人に見せる（creep は [trace]、leap と skip は [debug]。ADR 0028）。
std::string prompt_text(const Machine& machine) {
  const Impl& impl = engine_impl(machine);
  if (!impl.debug_on) return "?- ";
  return impl.debug.mode == DebugMode::Creep ? "[trace] ?- " : "[debug] ?- ";
}

// ためた行から 1 項読む。上限に達したら回復して知らせ、nullopt を返す。
std::optional<ReadOutcome> read_buffer(Machine& machine, const std::string& buffer, Echo& echo) {
  try {
    Reader reader(buffer, machine.heap(), machine.operators(), current_double_quotes(engine_impl(machine)),
                  &engine_impl(machine).char_conv);
    reader.set_fullwidth(engine_impl(machine).japanese_punct);
    return reader.next();
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    recover_heap(machine);
    warn_resource(machine, echo);
    return std::nullopt;
  }
}

// 読んだ項から、動かすものを文字列で取る。ゴールも指令も文字列から読み直して動かすので、項はもう要らない。
// help は REPL の命令で、述語ではない。user の名前を増やさない（SPEC 第4節）。
enum class Entry { Help, Directive, Goal };

Entry entry_of(Machine& machine, const ReadTerm& read, std::string& goal) {
  const Cell term = read.term;
  if (term.tag() == Tag::Atom && machine.atoms().name(static_cast<std::uint32_t>(term.raw() >> 3)) == "help") {
    return Entry::Help;
  }
  if (term.tag() == Tag::Str && machine.heap().functor_arity(machine.heap().deref(term)) == 1) {
    const std::string_view name = machine.atoms().name(machine.heap().functor_atom(machine.heap().deref(term)));
    if (name == ":-" || name == "?-") return Entry::Directive;
  }
  goal = goal_text(machine, term, read.names);
  return Entry::Goal;
}

void prompt_loop(Machine& machine, Cursor& cursor, Echo& echo) {
  std::string buffer;
  echo.say(prompt_text(machine));
  while (true) {
    std::string line;
    if (!cursor.line(line)) return;
    if (buffer.empty() && blank(line)) continue;
    buffer += line;
    // 読みが作った項は、どの道でもこの高さへ切り詰めて捨てる。前は続きの行ごとにためた入力の全体を読み直して
    // 項を積み、構文の誤りの項も残したので、--heap-max で止まった（レビュー M-12）。読む間に GC は動かない。
    const std::size_t mark = machine.heap().size();
    const std::optional<ReadOutcome> outcome = read_buffer(machine, buffer, echo);
    std::string goal;
    const std::optional<Entry> entry =
        outcome && outcome->term ? std::optional<Entry>{entry_of(machine, *outcome->term, goal)} : std::nullopt;
    if (machine.heap().size() >= mark) machine.heap().truncate(mark);
    if (outcome && outcome->failure && outcome->failure->reason == SyntaxReason::IncompleteTerm) continue;
    if (outcome && outcome->failure) warn_syntax(machine, *outcome->failure, echo);
    const std::string text = buffer;
    buffer.clear();
    if (!entry) {
      echo.say(prompt_text(machine));
      continue;
    }
    if (*entry == Entry::Help) {
      echo.say(render_message(machine, "help_japanese_variables", {}) + "\n");
    } else if (*entry == Entry::Directive) {
      if (prompt_directive(machine, text, echo)) return;
    } else {
      bool halted = false;
      const int code = play_goal(machine, goal, true, true, cursor, echo, halted);
      if (halted) {
        echo.result->code = code;
        return;
      }
    }
    echo.say(prompt_text(machine));
  }
}

// 読めなければ終了コードを返す。指示の halt/0,1 は読み込みを止め、その値で終わる。後のファイル、main、-g は動かさない。
std::optional<int> consult_files(Machine& machine, const std::vector<std::string>& files, Echo& echo) {
  for (const std::string& file : files) {
    try {
      if (machine.consult(file)) continue;
      if (engine_impl(machine).halt_now) return engine_impl(machine).halt_code;
      // 起動時の読み込みの abort は、読み込みの失敗と同じ値で終わる。診断は空なので、代わりに知らせを書く。
      if (engine_impl(machine).abort_now) {
        report_abort(machine, echo);
        return 1;
      }
      echo.warn(std::string{machine.diagnostic()} + "\n");
      return 1;
    } catch (...) {
      if (!is_heap_exhaustion()) throw;
      recover_heap(machine);
      warn_resource(machine, echo);
      return 2;
    }
  }
  return std::nullopt;
}

}  // namespace

// initialization(G, main) の入口。成功は 0、失敗と abort は 1、例外は文面を出して 2、halt/1 の値はそのまま。
// 答えの行は出さない。実行ファイルと同じ出力にするため。
int run_main_goal(Machine& machine, const ExportedTerm& goal, std::string& warning) {
  try {
    // 字面を読まないので、読み込みの後の指令が変換や演算子を変えても、ゴールは指令の項のまま（レビュー L17、L23）。
    Stop stop = query_exported(machine, goal);
    // 選択点を残した成功も答えの後で切り、後始末を動かす（ADR 0016、敵対的レビュー A3）。
    if (stop.kind == StopKind::Success) stop = close_query(engine_impl(machine));
    if (stop.kind == StopKind::Halt) return stop.halt_code;
    // 入口の abort は、入口の失敗と同じく 1 で終わる（設計の「コマンド」）。実行ファイルには口が無いので起きない。
    if (stop.kind == StopKind::Abort) {
      warning += message_text(engine_impl(machine), "debug_aborted") + "\n";
      return 1;
    }
    if (stop.kind == StopKind::Exception) {
      const std::array<Cell, 1> args{stop.exception};
      warning += render_message(machine, "goal_raised", args) + "\n";
      return 2;
    }
    return stop.kind == StopKind::Failure ? 1 : 0;
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    recover_heap(machine);
    warning += heap_limit_text(engine_impl(machine)) + "\n";
    return 2;
  }
}

namespace {

void run_with_machine(const Options& options, std::string_view input, bool tty, std::size_t limit, Echo& echo,
                      SessionResult& result) {
  Cursor cursor;
  cursor.data = input;
  cursor.from_cin = tty && input.empty();
  auto machine = Machine::create(limit);
  engine_impl(*machine).program_argv = options.program_args;
  // ファイルの指令や main のゴールの consult/1 も書くので、読み込みより前に向ける。機械は echo より先に消える。
  engine_impl(*machine).load_report = [&echo](std::string_view text) { echo.warn(text); };
  // デバッガの口（ADR 0028）。ポートの行は警告と同じ所に書く。コマンドは ; の問いと同じ Cursor から、端末のときだけ
  // 読む。端末でない標準入力は user_input の物なので、先に読まない。機械は cursor と echo より先に消える。
  DebugConsole console;
  console.write = [&echo](std::string_view text) { echo.warn(text); };
  // read/1 が終止符を確かめて返した字（foo. の後の改行）はコマンドではないので、; の促しと同じく先に捨てる
  // （レビュー L15）。捨てないと read/1 の次の止まるポートがその改行を creep と読み、止まらなかった（敵対的レビュー、
  // 2026-11-19）。get_char/1 などが読み残した行の残りは返された字ではないので、; の促しと同じくコマンドとして読む。
  if (tty) {
    console.read_line = [&cursor](std::string& line) {
      if (cursor.from_cin) standard_input_drop_returned_line();
      return cursor.line(line);
    };
  }
  engine_impl(*machine).debug.console = std::move(console);
  if (const auto code = consult_files(*machine, options.files, echo)) {
    result.code = *code;
    return;
  }
  // main は入口で、終わったらプロセスも終わる。REPL にも -g にも進まない。
  if (const auto& main_goal = engine_impl(*machine).main_goal) {
    result.code = play_main(*machine, *main_goal, echo);
    return;
  }
  // 対話では -g をプロンプトに打ったのと同じに全部動かす。一括では成功しなかった最初のゴールの値で終え、
  // 後のゴールは動かさない（SWI-Prolog と同じ）。読めないゴールは例外と同じ扱い。
  const bool interactive = tty && !options.top_halt;
  for (const std::string& text : options.goals) {
    bool halted = false;
    const auto goal = command_goal(*machine, text, echo);
    const int code =
        goal ? play_goal(*machine, *goal, false, interactive, cursor, echo, halted) : (interactive ? 0 : 2);
    if (halted || (!interactive && code != 0)) {
      result.code = code;
      return;
    }
  }
  // 端末でない入力と -t halt は、読み込みと -g の後で 0 で終える。mosac の実行ファイルと同じ（レビュー L10）。
  if (interactive) prompt_loop(*machine, cursor, echo);
}

// 呼び手の機械はこのセッションより長く生きる（finish_interactive はヒープの上限の例外を受けた後も使う）。抜けるとき、
// 例外でも書き先を空に戻さないと、後のゴールの読み込みの失敗が消えた echo へ書く。
class LoadReportScope {
 public:
  LoadReportScope(Impl& machine, const Echo& echo) : machine_(machine) {
    machine_.load_report = [&echo](std::string_view text) { echo.warn(text); };
  }
  ~LoadReportScope() { machine_.load_report = nullptr; }
  LoadReportScope(const LoadReportScope&) = delete;
  LoadReportScope& operator=(const LoadReportScope&) = delete;

 private:
  Impl& machine_;
};

}  // namespace

SessionResult run_prepared_session(Machine& machine, std::ostream* live_out, std::ostream* live_err) {
  SessionResult result;
  Echo echo;
  echo.result = &result;
  echo.out = live_out;
  echo.err = live_err;
  const LoadReportScope report(engine_impl(machine), echo);
  Cursor cursor;
  cursor.from_cin = true;
  prompt_loop(machine, cursor, echo);
  return result;
}

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
  // 端末でない入力で、ファイルも -g も -t halt も無ければ、何をするのかが分からない。
  if (options.bad || (!tty && options.goals.empty() && options.files.empty() && !options.top_halt)) {
    echo.warn(render_message_fresh(default_message_language(), "usage") + "\n");
    result.code = 1;
    return result;
  }
  // --heap-max が最優先。無ければ呼んだ側の値（mosa の main は MOSA_HEAP_MAX を読んで渡す）。
  const std::size_t limit = options.heap_max.value_or(heap_limit);
  // Machine::create が予約を断られた場合も含め、上限と OS のメモリ不足は同じ止め方にする。
  try {
    run_with_machine(options, input, tty, limit, echo, result);
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    echo.warn(render_message_fresh(default_message_language(), heap_stop_key(),
                                   std::array{heap_limit_mib_text(limit)}) + "\n");
    result.code = 2;
  }
  return result;
}

}
