// tests.lgt は Logtalk として動かさない。ファイルごとに 1 台の機械で実行する。
// halt はプロセスを終えず、印を戻してから次のテストへ進む。

#include "lgt_suite.hpp"

#include "lgt_io.hpp"
#include "internal.hpp"
#include "std_capture.hpp"

#include "mosa/machine.hpp"
#include "mosa/path_text.hpp"
#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/writer.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace mosa {
namespace {


bool thrown_matches(Machine& machine, Cell ball, std::string_view formal) {
  const Heap& heap = machine.heap();
  ball = heap.deref(ball);
  if (!lgt_functor_is(heap, ball, "error", 2)) return false;
  const std::string text = std::string{formal} + ".";
  Reader reader(text, machine.heap(), machine.operators());
  const ReadOutcome read = reader.next();
  if (!read.term) return false;
  return lgt_subsumes(machine.heap(), read.term->term, heap.arg(ball, 0));
}

std::string goal_text(const LgtCase& item, const LgtWant& want) {
  std::string goal = item.body;
  // 表明が失敗しても、本体の次の解は試さない。
  if (want.kind == LgtKind::Assert) goal = "(" + item.body + ", !), (" + want.detail + ")";
  goal.push_back('.');
  return goal;
}

bool routes_to_judge(Machine& machine, std::string_view body, const LgtWant& want) {
  if (want.kind != LgtKind::True && want.kind != LgtKind::Assert && want.kind != LgtKind::False &&
      want.kind != LgtKind::Error) {
    return true;
  }
  if (lgt_text_has_special(machine, body)) return true;
  return want.kind == LgtKind::Assert && lgt_text_has_special(machine, want.detail);
}

bool defined_here(const Machine& machine, std::string_view name) {
  const Impl& impl = engine_impl(machine);
  for (const Predicate& predicate : impl.predicates) {
    if (predicate.arity == 0 && !predicate.abolished && live_count(predicate) > 0 &&
        impl.atoms.name(predicate.name) == name) {
      return true;
    }
  }
  return false;
}

struct Query {
  bool dead = false;
  Stop stop;
};

Query run_goal(Machine& machine, std::string_view goal) {
  Query query;
  try {
    query.stop = machine.query(goal);
  } catch (const ResourceLimit&) {
    query.dead = true;
    return query;
  }
  // 次のクエリが停止の印で空振りしないように、結果を取ったあとで戻す。
  Impl& impl = engine_impl(machine);
  impl.halt_now = false;
  impl.halt_pending = false;
  return query;
}

std::string actual_text(const Machine& machine, const Stop& stop) {
  if (stop.kind == StopKind::Success || stop.kind == StopKind::Halt) return "true";
  if (stop.kind != StopKind::Exception) return "fail";
  const Heap& heap = machine.heap();
  const Cell term = heap.deref(stop.exception);
  if (lgt_functor_is(heap, term, "error", 2)) {
    return write_term(heap, machine.operators(), heap.arg(term, 0), lgt_write_options(), {});
  }
  return write_term(heap, machine.operators(), term, lgt_write_options(), {});
}

bool succeeded(Machine& machine, const LgtWant& want, const Stop& stop) {
  // halt はそのテストの成功にする。期待の形が違ってもプロセスは終わらせない。
  if (stop.kind == StopKind::Halt) return true;
  if (want.kind == LgtKind::True || want.kind == LgtKind::Assert) return stop.kind == StopKind::Success;
  if (want.kind == LgtKind::False) return stop.kind == StopKind::Failure;
  if (want.kind != LgtKind::Error || stop.kind != StopKind::Exception) return false;
  return thrown_matches(machine, stop.exception, want.detail);
}

struct CaseEnd {
  bool pass = false;
  bool dead = false;
  std::string actual;
};

bool call_hook(Machine& machine, bool enabled, std::string_view goal) {
  if (!enabled) return true;
  return !run_goal(machine, goal).dead;
}

CaseEnd run_one(Machine& machine, const LgtCase& item, bool setup, bool cleanup, const LgtIo* io) {
  CaseEnd end;
  try {
    if (!call_hook(machine, setup, "setup.")) {
      end.dead = true;
      end.actual = "aborted";
      return end;
    }
    const LgtWant want = classify_lgt_outcome(machine, item.outcome);
    if (want.kind == LgtKind::Other) {
      end.actual = "unsupported";
      end.dead = !call_hook(machine, cleanup, "cleanup.");
      return end;
    }
    if (routes_to_judge(machine, item.body, want)) {
      const LgtEnd judged = judge_lgt_case(machine, item.body, want, io);
      end.pass = judged.pass;
      end.dead = judged.dead;
      end.actual = judged.actual;
      if (!call_hook(machine, cleanup, "cleanup.")) end.dead = true;
      return end;
    }
    // 差分テストは本体を節 '$mosa_diff_goal'(K, ...) にしてあるので、その問い合わせを流す（計画 Task 11）。
    const Query query =
        run_goal(machine, item.diff_goal.empty() ? goal_text(item, want) : item.diff_goal + ".");
    if (query.dead) {
      end.dead = true;
      end.actual = "aborted";
      return end;
    }
    end.pass = succeeded(machine, want, query.stop);
    if (!end.pass) end.actual = actual_text(machine, query.stop);
    if (!call_hook(machine, cleanup, "cleanup.")) end.dead = true;
    return end;
  } catch (const ResourceLimit&) {
    end.dead = true;
    end.actual = "aborted";
    return end;
  }
}

bool load_ok(Machine& machine, std::string_view program) {
  if (program.empty()) return true;
  try {
    return machine.load_program(program).ok;
  } catch (const ResourceLimit&) {
    return false;
  }
}

void note_failure(LgtRun& run, const LgtCase& item, std::string_view actual) {
  run.failures.push_back(item.name + " " + item.outcome + " " + std::string{actual});
  ++run.failed;
}

}  // namespace

namespace {

bool guard_holds(const std::vector<LgtGuard>& guard, const std::vector<int>& chosen) {
  for (const LgtGuard& item : guard) {
    if (item.group < 0 || static_cast<std::size_t>(item.group) >= chosen.size()) return false;
    if (chosen[static_cast<std::size_t>(item.group)] != item.arm) return false;
  }
  return true;
}

std::string selected_text(const LgtFile& file, const std::vector<int>& chosen) {
  std::string text;
  for (const LgtPiece& piece : file.pieces) {
    if (guard_holds(piece.guard, chosen)) text += piece.text;
  }
  return text;
}

bool choose_gates(Machine& machine, const LgtFile& file, std::vector<int>& chosen) {
  chosen.assign(file.gates.size(), -1);
  for (std::size_t index = 0; index < file.gates.size(); ++index) {
    const LgtGate& gate = file.gates[index];
    // 選ばれていない枝の中の条件は実行しない。
    if (!guard_holds(gate.parent, chosen)) continue;
    for (std::size_t arm = 0; arm < gate.arms.size(); ++arm) {
      const LgtArm& item = gate.arms[arm];
      if (item.otherwise) {
        chosen[index] = static_cast<int>(arm);
        break;
      }
      const LgtCond cond = lgt_condition(machine, item.cond);
      if (cond == LgtCond::Dead) return false;
      if (cond != LgtCond::Yes) continue;
      chosen[index] = static_cast<int>(arm);
      break;
    }
  }
  return true;
}

void mark_load(LgtRun& run, const LgtCase& item, bool held) {
  if (!held || item.outside) run.outside.push_back(item.name);
  else note_failure(run, item, "load");
}

void apply_condition(Machine& machine, const LgtCase& item, bool& dead, bool& skip, LgtRun& run) {
  if (item.condition.empty()) return;
  const LgtCond cond = lgt_condition(machine, item.condition);
  if (cond == LgtCond::Dead) {
    note_failure(run, item, "aborted");
    dead = true;
    skip = true;
    return;
  }
  if (cond == LgtCond::Bad) {
    note_failure(run, item, "unsupported");
    skip = true;
    return;
  }
  if (cond != LgtCond::Yes) {
    run.outside.push_back(item.name);
    skip = true;
  }
}

// lgtunit の run_test/2 は 1 件ごとに現在の入出力を保存して戻す。^^suppress_text_output を次の件へ漏らさない。
// テストが閉じた流れは戻せないので、その側だけ標準の流れにする。
struct CurrentStreams {
  std::size_t input = 0;
  std::size_t output = 1;
};

CurrentStreams save_current(Machine& machine) {
  const Impl& impl = engine_impl(machine);
  return CurrentStreams{impl.current_in, impl.current_out};
}

void restore_current(Machine& machine, const CurrentStreams& saved) {
  Impl& impl = engine_impl(machine);
  const auto usable = [&impl](std::size_t index) { return index < impl.streams.size() && !impl.streams[index].closed; };
  impl.current_in = usable(saved.input) ? saved.input : 0;
  impl.current_out = usable(saved.output) ? saved.output : 1;
}

// 1 件を流し、結果の字面を run.results に残す。標準出力と標準エラーはその間だけ受けて字面に付ける
// （差分テストが両側で比べる。スイートの実行では件の出力が端末に出なくなるだけで、合否は変わらない）。
void run_case(Machine& machine, const LgtCase& item, bool setup, bool cleanup, const LgtIo* io, LgtRun& run,
              bool& dead) {
  const CurrentStreams saved = save_current(machine);
  CaseEnd end;
  std::string output;
  {
    StdCapture capture;
    end = run_one(machine, item, setup, cleanup, io);
    output = capture.text();
  }
  restore_current(machine, saved);
  if (end.pass) ++run.passed;
  else note_failure(run, item, end.actual);
  run.results.push_back(item.name + " " + (end.pass ? "pass" : end.actual) + output);
  dead = end.dead;
}

void run_cases(Machine& machine, const LgtFile& file, const std::vector<int>& chosen, bool setup, bool cleanup,
               const LgtIo* io, LgtRun& run) {
  bool dead = false;
  for (const LgtCase& item : file.cases) {
    if (!guard_holds(item.guard, chosen) || item.outside) {
      run.outside.push_back(item.name);
      continue;
    }
    if (dead) {
      note_failure(run, item, "aborted");
      continue;
    }
    bool skip = false;
    apply_condition(machine, item, dead, skip, run);
    if (skip) continue;
    run_case(machine, item, setup, cleanup, io, run, dead);
  }
}

}  // namespace

bool lgt_guard_holds(const std::vector<LgtGuard>& guard, const std::vector<int>& chosen) {
  return guard_holds(guard, chosen);
}

bool lgt_choose_gates(Machine& machine, const LgtFile& file, std::vector<int>& chosen) {
  return choose_gates(machine, file, chosen);
}

std::string lgt_selected_text(const LgtFile& file, const std::vector<int>& chosen) { return selected_text(file, chosen); }

std::string lgt_goal_text(const LgtCase& item, const LgtWant& want) { return goal_text(item, want); }

bool lgt_routes_to_judge(Machine& machine, std::string_view body, const LgtWant& want) {
  return routes_to_judge(machine, body, want);
}

LgtRun execute_lgt(const LgtFile& file, std::string_view tests_path, std::string_view relative_dir,
                   const TestLoader& loader) {
  // Logtalk の tester はテストのディレクトリで動くので、{open(d1, write, S)} の相対パスはそこに出来る。
  // スクラッチをその代わりにし、カレントを移して戻す。消す前に戻すのは、使用中のディレクトリを
  // Windows が消させないため。
  struct Clean {
    LgtIo io;
    std::filesystem::path previous;
    explicit Clean(LgtIo value) : io(std::move(value)) {
      if (io.scratch.empty()) return;
      std::error_code error;
      std::filesystem::create_directories(io.scratch, error);
      if (!error) previous = std::filesystem::current_path(error);
      if (!error) std::filesystem::current_path(io.scratch, error);
      if (error) previous.clear();
    }
    Clean(const Clean&) = delete;
    Clean& operator=(const Clean&) = delete;
    ~Clean() {
      std::error_code error;
      if (!previous.empty()) std::filesystem::current_path(previous, error);
      lgt_io_cleanup(io);
    }
  } clean{lgt_io_context(tests_path, relative_dir)};
  LgtRun run;
  auto machine = Machine::create();
  std::vector<int> chosen;
  if (!choose_gates(*machine, file, chosen)) {
    for (const LgtCase& item : file.cases) note_failure(run, item, "aborted");
    return run;
  }
  // 読み方は呼び手が選ぶ（計画 Task 11）。差分テストはインタプリタと翻訳で同じ原文を別の読み方で読む。
  const bool loaded = loader ? loader(*machine, path_from_utf8(tests_path)) : load_ok(*machine, selected_text(file, chosen));
  if (!loaded) {
    for (const LgtCase& item : file.cases) mark_load(run, item, guard_holds(item.guard, chosen));
    run.results.push_back("load failed " + std::string{machine->diagnostic()});
    return run;
  }
  // =~= はこのファイルの機械だけ。読み込みの節には入れない。
  install_lgt_ops(*machine, file.operators);
  // テストより先に隣の Prolog を載せる。tester.lgt 自体は実行しない。
  lgt_load_tester(*machine, clean.io);
  // 隣の Prolog の定義も見てから取り込む。同じ名前はテスト側を残す。
  lgt_import_libraries(*machine);
  // 定義の無い setup は存在エラーにしない。テストの assert で後から足した節は、ここには入らない。
  const bool setup = defined_here(*machine, "setup");
  const bool cleanup = defined_here(*machine, "cleanup");
  // 節の中の ^^ は判定器を通らない。腕を付けたままクエリで閉じる。
  lgt_arm_super(*machine, clean.io);
  run_cases(*machine, file, chosen, setup, cleanup, &clean.io, run);
  lgt_disarm_super(*machine);
  return run;
}

}  // namespace mosa
