// 計画 Task 11。差分テストの原文の組み立てと、1 ファイルの実行。
// tests/pl: 原文は元のバイト列に節 '$mosa_diff_goal'(K, 名前のある変数...) :- ゴール. を足した物。test/3 の事実は
// mosatest と同じ横取り（hook_test_facts）で両方の側とも節にしない。ゴールは suite.cpp と同じ read_test_file で取る。
// .lgt: 原文は実行器が load_program に渡す物（lgt_selected_text）に、判定器を通らない case の節を足した物。
// 節の本体は演算子に頼らず書くので、読む側の表が op/3 で変わっていても同じ項に戻る。

#include "diff.hpp"

#include "answer_text.hpp"
#include "internal.hpp"
#include "std_capture.hpp"
#include "test_file.hpp"

#include "mosa/path_text.hpp"
#include "mosa/reader.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mosa {
namespace {

constexpr std::string_view kGoal = "'$mosa_diff_goal'";

std::optional<std::string> read_bytes(const std::filesystem::path& file) {
  std::ifstream input(file, std::ios::binary);
  if (!input) return std::nullopt;
  return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string head_text(std::size_t index, const std::vector<std::string>& names) {
  std::string head = std::string{kGoal} + "(" + std::to_string(index);
  for (const std::string& name : names) head += ", " + name;
  return head + ")";
}

struct Clause {
  std::size_t arity = 0;
  std::string text;
};

// 同じ述語の節は続けて置く。アリティごとの '$mosa_diff_goal' が交ざると、読み込み器が節の離れを誤りにする。
std::string clause_text(std::vector<Clause> clauses) {
  std::stable_sort(clauses.begin(), clauses.end(),
                   [](const Clause& left, const Clause& right) { return left.arity < right.arity; });
  std::string text = "\n% mosatest --diff が足した節（計画 Task 11）。手で直さない。\n";
  for (const Clause& clause : clauses) text += clause.text;
  return text;
}

std::string with_newline(std::string text) {
  if (!text.empty() && text.back() != '\n') text.push_back('\n');
  return text;
}

WriteOptions canonical_options() {
  WriteOptions options;
  options.ignore_ops = true;
  options.underscore_once = true;
  return options;
}

DiffSource pl_source(const std::filesystem::path& file, const std::string& bytes) {
  DiffSource out;
  auto scratch = Machine::create();
  std::vector<Exam> exams;
  try {
    // 指示の出力は捨てる。両方の側が読むときに改めて出る。
    StdCapture quiet;
    if (!read_test_file(*scratch, bytes, utf8_from_path(file.parent_path()), utf8_from_path(file), exams, out.error)) {
      return out;
    }
  } catch (const ResourceLimit&) {
    out.error = message_text(engine_impl(*scratch), "heap_limit");
    return out;
  }
  std::vector<Clause> clauses;
  for (std::size_t index = 0; index < exams.size(); ++index) {
    const Exam& exam = exams[index];
    DiffGoal goal{exam.name, exam.goal, head_text(index + 1, exam.names), {}, exam.solutions, exam.enumerate};
    clauses.push_back(Clause{exam.names.size() + 1, goal.query + " :- " + exam.canonical + ".\n"});
    out.goals.push_back(std::move(goal));
  }
  out.source = with_newline(bytes) + clause_text(std::move(clauses));
  return out;
}

// 本体の問い合わせの字面を実行器と同じ読み方で読み、節の頭部（名前のある変数）と本体（演算子に頼らない字面）にする。
std::optional<Clause> lgt_clause(Machine& machine, std::size_t index, const std::string& text, std::string& head) {
  Impl& impl = engine_impl(machine);
  Reader reader(text, impl.heap, impl.operators, current_double_quotes(impl), &impl.char_conv);
  reader.set_fullwidth(impl.japanese_punct);
  const ReadOutcome outcome = reader.next();
  if (!outcome.term) return std::nullopt;
  std::vector<std::string> names;
  for (const VarName& name : outcome.term->names) names.push_back(name.name);
  head = head_text(index, names);
  const std::string body = write_term(impl.heap, impl.operators, outcome.term->term, canonical_options(), outcome.term->names);
  return Clause{names.size() + 1, head + " :- " + body + ".\n"};
}

void lgt_case(Machine& machine, LgtCase& item, std::size_t index, DiffSource& out, std::vector<Clause>& clauses) {
  DiffGoal goal{item.name, item.body, {}, {}, 1};
  const LgtWant want = classify_lgt_outcome(machine, item.outcome);
  if (want.kind == LgtKind::Other) {
    goal.shape = "unsupported";
  } else if (lgt_routes_to_judge(machine, item.body, want)) {
    goal.shape = "judge";
  } else if (auto clause = lgt_clause(machine, index, lgt_goal_text(item, want), goal.query)) {
    clauses.push_back(std::move(*clause));
    item.diff_goal = goal.query;
  } else {
    goal.query.clear();
    goal.shape = "unreadable";
  }
  out.goals.push_back(std::move(goal));
}

DiffSource lgt_source(const std::string& bytes) {
  DiffSource out;
  out.lgt = true;
  out.file = read_lgt_file(bytes);
  if (!out.file.error.empty()) {
    out.error = out.file.error;
    return out;
  }
  auto scratch = Machine::create();
  std::vector<int> chosen;
  std::vector<Clause> clauses;
  try {
    StdCapture quiet;
    // 門が決まらないファイルは実行器がどの case も aborted にする。足す節は無く、両方の側が同じに空振りする。
    if (!lgt_choose_gates(*scratch, out.file, chosen)) return out;
    out.source = lgt_selected_text(out.file, chosen);
    // 読めない原文（古い形の succeeds/1 の節が離れているファイルなど）は両方の側が同じに読めない。
    // 節を足しても意味が無いので、読み込みの失敗の字面だけを比べる。
    if (!scratch->load_program(out.source).ok) {
      out.loads = false;
      return out;
    }
    install_lgt_ops(*scratch, out.file.operators);
    for (std::size_t index = 0; index < out.file.cases.size(); ++index) {
      LgtCase& item = out.file.cases[index];
      if (!lgt_guard_holds(item.guard, chosen) || item.outside) continue;
      lgt_case(*scratch, item, index + 1, out, clauses);
    }
  } catch (const ResourceLimit&) {
    out.error = "heap_limit";
    return out;
  }
  out.source = with_newline(std::move(out.source)) + clause_text(std::move(clauses));
  return out;
}

// 答えは束縛を 1 つの項 '$a'(V1, ..., Vn)（問い合わせの変数の順）にまとめ、問い合わせの変数の名前を使わずに書く。
// 生成コードは頭部の変数を問い合わせのセルに直に結ぶが、IR は節の新しいセルを間に置くので、名前で書くと束縛の中の
// 問い合わせの変数（L = [a,b|L] と [a,b|_]、t(X,W,Y) と t(_G1,_G2,_G1)）の見え方がセルの同一性で変わる。1 回の書き出し
// の中では名前の無い変数が最初の出現の順の _G<n> になるので、束縛どうしの共有（f(A), g(A)）と束縛の無い変数の別名
// （X = Y）は残り、同じ代入なら同じ字面になる（ADR 0009 の Review Focus 1 の結び方の違いを隠さない）。
// 字面は write_term ではなく answer_text（深い項でも C++ のスタックを使わない。answer_text.hpp の注）。
std::string render(Machine& machine, const Stop& stop) {
  if (stop.kind == StopKind::Halt) return "halt(" + std::to_string(stop.halt_code) + ")";
  if (stop.kind == StopKind::Failure) return "false";
  Impl& impl = engine_impl(machine);
  if (stop.kind == StopKind::Exception) {
    return "exception: " + answer_text(impl.heap, impl.operators, stop.exception);
  }
  if (stop.bindings.empty()) return "true";
  std::string names;
  std::vector<Cell> parts;
  for (const Binding& binding : stop.bindings) {
    names += (names.empty() ? "" : ",") + binding.name;
    parts.push_back(binding.variable);
  }
  // 項はヒープの先に置くだけで、次の next() の巻き戻しが捨てる。
  const Cell answer = impl.heap.structure(impl.atoms.intern("$a"), parts);
  return "{" + names + "} = " + answer_text(impl.heap, impl.operators, answer);
}

std::string run_goal(Machine& machine, const DiffGoal& goal) {
  StdCapture capture;
  std::string text;
  try {
    Stop stop = machine.query((goal.query.empty() ? goal.plain : goal.query) + ".");
    text = render(machine, stop);
    for (std::size_t index = 1; index < goal.solutions && stop.kind == StopKind::Success; ++index) {
      stop = machine.next();
      text += "\n" + render(machine, stop);
    }
    // solutions([...]) は期待の数の後にもう 1 つ求め、片方だけが余分な解を持つ違いも見せる。
    if (goal.enumerate && stop.kind == StopKind::Success) {
      stop = machine.next();
      text += "\n" + render(machine, stop);
    }
  } catch (const ResourceLimit&) {
    text = "resource_limit";
  }
  // halt は次の問い合わせを空振りさせる。lgt の実行器と同じく印を戻す。
  Impl& impl = engine_impl(machine);
  impl.halt_now = false;
  impl.halt_pending = false;
  return text + capture.text();
}

std::vector<DiffLine> pl_results(const std::filesystem::path& file, const DiffSource& source, const TestLoader& loader) {
  auto machine = Machine::create();
  std::vector<DiffLine> lines;
  std::string load;
  {
    StdCapture capture;
    const bool loaded = loader(*machine, file);
    // 横取りは読み込みのため。ゴールが consult するファイルの test/3 は読む側の事実のまま、両方の側で同じに扱う。
    engine_impl(*machine).term_hook = nullptr;
    load = (loaded ? std::string{"ok"} : "failed " + std::string{machine->diagnostic()}) + capture.text();
    if (!loaded) return {DiffLine{"load", load}};
  }
  for (const DiffGoal& goal : source.goals) lines.push_back(DiffLine{goal.name, run_goal(*machine, goal)});
  // 読み込みの出力は最後に置く。ゴールの行の添字が goals と同じになる。
  lines.push_back(DiffLine{"load", load});
  return lines;
}

// 実行器の 1 行は「名前 合否か実際の結果」。名前は空白を含まない。
DiffLine split_label(const std::string& line) {
  const std::size_t space = line.find(' ');
  if (space == std::string::npos) return DiffLine{line, {}};
  return DiffLine{line.substr(0, space), line.substr(space + 1)};
}

std::vector<DiffLine> lgt_results(const std::filesystem::path& file, const std::filesystem::path& dir,
                                  const DiffSource& source, const TestLoader& loader) {
  std::error_code error;
  const auto folder = std::filesystem::relative(file.parent_path(), dir / "logtalk" / "prolog", error);
  std::vector<DiffLine> lines;
  StdCapture capture;
  try {
    const LgtRun run = execute_lgt(source.file, utf8_from_path(file), error ? std::string{} : folder.generic_string(), loader);
    for (const std::string& line : run.results) lines.push_back(split_label(line));
    for (const std::string& line : run.failures) lines.push_back(DiffLine{"failure", line});
    lines.push_back(DiffLine{"summary", "passed " + std::to_string(run.passed) + " failed " + std::to_string(run.failed)});
  } catch (const ResourceLimit&) {
    lines.push_back(DiffLine{"summary", "resource_limit"});
  }
  // 実行器は case ごとに出力を捕まえる。ここに残るのは読み込みなどの間の出力で、まとめの行に付ける。
  lines.back().text += capture.text();
  return lines;
}

}  // namespace

std::vector<std::filesystem::path> diff_files(const std::filesystem::path& dir) {
  std::error_code error;
  const std::filesystem::path prolog = dir / "logtalk" / "prolog";
  if (std::filesystem::is_directory(prolog, error) && !error) return lgt_suite_files(prolog);
  if (!std::filesystem::is_directory(dir, error) || error) return {};
  return test_files(dir);
}

DiffSource diff_source(const std::filesystem::path& file) {
  const auto bytes = read_bytes(file);
  if (!bytes) {
    DiffSource out;
    const std::array<std::string, 1> args{utf8_from_path(file)};
    out.error = render_message_fresh(default_message_language(), "cannot_open_file", args);
    return out;
  }
  if (file.extension() == ".lgt") return lgt_source(*bytes);
  return pl_source(file, *bytes);
}

void hook_test_facts(Machine& machine, const std::filesystem::path& file) {
  Impl& impl = engine_impl(machine);
  const std::string key = utf8_from_path(canonical_key(file));
  // origin は consult なら渡したままのパス、埋め込みの単位なら正規化したパスなので、正規化して比べる。
  // 正規化はファイルシステムを見るので、test/3 の形だと分かった項にだけ行う。
  impl.term_hook = [&impl, key](const ReadTerm& read, std::string_view at) {
    if (!is_test_fact(impl.heap, read.term)) return false;
    return utf8_from_path(canonical_key(path_from_utf8(at))) == key;
  };
}

std::vector<DiffLine> run_diff_file(const std::filesystem::path& file, const std::filesystem::path& dir,
                                    const DiffSource& source, const TestLoader& loader) {
  if (source.lgt) return lgt_results(file, dir, source, loader);
  return pl_results(file, source, loader);
}

}  // namespace mosa
