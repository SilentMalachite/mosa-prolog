// Logtalk のスイートの判定器は、落ちるべき試験を不合格にし、プロセスごと落ちない（レビュー 2026-11-20 の W1）。
// 1 件ごとに 1 ファイルのスイートを tests/tmp/suite-judge に書いて run_iso_suite で流し、報告の不合格の行を見る。
// C-04: 誤りを期待する試験で forall/2 の生成側が halt すると、例外の項の無い Stop を読んでプロセスが落ちた。
// H-36: halt と、選択点の後でスクラッチの機械に回したゴールの例外と halt が失敗と数えられ、false と all が通った。
// H-37: assertion のある本体では、ball/1 が正しく投げても通らず、成功した本体が ball/1 と error/1 で通った。
// H-38: true(Assertion) の本体の最上位の ; が切りの外に出て、表明が失敗すると本体の次の解で通った。

#include "harness.hpp"
#include "mosa/suite.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

bool write_text(const std::filesystem::path& path, std::string_view text) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return false;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(output);
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

// 報告の「不合格:」から「別欄:」までの行。別欄の行を不合格と取り違えない。
std::string failed_lines(std::string_view report) {
  const auto begin = report.find("不合格:\n");
  if (begin == std::string_view::npos) return {};
  const auto start = begin + std::string_view{"不合格:\n"}.size();
  const auto stop = report.find("別欄:", start);
  return std::string{report.substr(start, stop == std::string_view::npos ? std::string_view::npos : stop - start)};
}

struct Fixture {
  std::filesystem::path path;
  explicit Fixture(std::filesystem::path root) : path(std::move(root)) {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
  Fixture(const Fixture&) = delete;
  Fixture& operator=(const Fixture&) = delete;
  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};

// 不合格の行は「ファイル 名前 期待 実際」。実際まで見るので、別の理由で落ちたのとも区別できる。
struct Case {
  std::string_view name;
  std::string_view clauses;
  std::string_view failed;
};

std::filesystem::path write_suite(const std::filesystem::path& root, const std::string& name, std::string_view clauses) {
  const auto suite = root / name;
  const std::string text = ":- object(tests, extends(lgtunit)).\n" + std::string{clauses} + "\n:- end_object.\n";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_1" / "tests.lgt", text),
                   name + ": write the suite");
  return suite;
}

void check_fails(const std::filesystem::path& root, const Case& item) {
  const std::string name{item.name};
  const auto suite = write_suite(root, name, item.clauses);
  const auto report = root / (name + "-report.md");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  mosa_test::check(code == 1, name + ": the suite fails");
  const std::string line = "predicates/atom_1/tests.lgt " + std::string{item.failed} + "\n";
  mosa_test::check(failed_lines(body).find(line) != std::string::npos, name + ": " + line + " in\n" + body);
}

void check_passes(const std::filesystem::path& root, std::string_view name_view, std::string_view clauses) {
  const std::string name{name_view};
  const auto suite = write_suite(root, name, clauses);
  const auto report = root / (name + "-report.md");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  mosa_test::check(code == 0 && failed_lines(body).empty(), name + ": the suite passes\n" + body);
}

void check_halt_in_forall(const std::filesystem::path& root) {
  const std::array<Case, 3> cases{{
      {"c04_error", "\ttest(c04_error, error(instantiation_error)) :- forall(halt(0), true).",
       "c04_error error(instantiation_error) halt(0)"},
      {"c04_errors", "\ttest(c04_errors, errors([instantiation_error, type_error(_, _)])) :- forall(halt(3), true).",
       "c04_errors errors([instantiation_error,type_error(_,_)]) halt(3)"},
      {"c04_false", "\ttest(c04_false, false) :- forall(halt(0), true).", "c04_false false halt(0)"},
  }};
  for (const Case& item : cases) check_fails(root, item);
}

// assertion があると本体はランナーの道を通る。member/2 の後は選択点が残り、後のゴールはスクラッチの機械で動く。
// gen/1 の 2 つ目の節は、X == 2 が失敗した後のやり直しで halt する。
void check_stops_are_not_failures(const std::filesystem::path& root) {
  const std::array<Case, 6> cases{{
      {"h36_halt_false", "\ttest(h36_halt_false, false) :- assertion(true), halt(0).", "h36_halt_false false halt(0)"},
      {"h36_halt_all", "\ttest(h36_halt_all, all(true)) :- assertion(true), halt(0).",
       "h36_halt_all all(true) halt(0)"},
      {"h36_live_throw", "\ttest(h36_live_throw, false) :- member(_X, [1, 2]), assertion(true), throw(x).",
       "h36_live_throw false x"},
      {"h36_live_throw_all", "\ttest(h36_live_throw_all, all(true)) :- member(_X, [1, 2]), assertion(true), throw(x).",
       "h36_live_throw_all all(true) x"},
      {"h36_live_halt", "\ttest(h36_live_halt, false) :- member(_X, [1, 2]), assertion(true), halt(0).",
       "h36_live_halt false halt(0)"},
      {"h36_retry_halt", "\tgen(1).\n\tgen(_) :- halt(0).\n\ttest(h36_retry_halt, false) :- gen(X), assertion(true), X == 2.",
       "h36_retry_halt false halt(0)"},
  }};
  for (const Case& item : cases) check_fails(root, item);
  // スクラッチの機械の誤りの項は、元の機械で読み直して期待の誤りと比べる。前は失敗と数えて不合格だった。
  check_passes(root, "h36_live_error",
               "\ttest(h36_live_error, error(type_error(atom, f(x)))) :- member(_X, [1, 2]), assertion(true), "
               "atom_length(f(x), _).");
}

// 本体が成功したら、期待の項をゴールとして走らせない。期待どおりに投げなかったので、実際は true である。
// ball/1 は投げられた項そのものを包摂で比べる。halt には投げられた項が無いので、読まずに halt(0) と書く。
void check_thrown_outcomes(const std::filesystem::path& root) {
  const std::array<Case, 5> cases{{
      {"h37_ball_true", "\ttest(h37_ball_true, ball(true)) :- assertion(true).", "h37_ball_true ball(true) true"},
      {"h37_error_true", "\ttest(h37_error_true, error(true)) :- assertion(true).", "h37_error_true error(true) true"},
      {"h37_errors_true", "\ttest(h37_errors_true, errors([true])) :- assertion(true).",
       "h37_errors_true errors([true]) true"},
      {"h37_ball_wrong", "\ttest(h37_ball_wrong, ball(y)) :- assertion(true), throw(x).", "h37_ball_wrong ball(y) x"},
      {"h37_ball_halt", "\ttest(h37_ball_halt, ball(_)) :- assertion(true), halt(0).", "h37_ball_halt ball(_) halt(0)"},
  }};
  for (const Case& item : cases) check_fails(root, item);
  check_passes(root, "h37_ball", "\ttest(h37_ball, ball(x)) :- assertion(true), throw(x).");
  check_passes(root, "h37_ball_pattern", "\ttest(h37_ball_pattern, ball(f(_))) :- assertion(true), throw(f(1)).");
  // member/2 の選択点の後なので、球はスクラッチの機械で投げられ、字面で元の機械に読み直してから比べる。
  check_passes(root, "h37_live_ball", "\ttest(h37_live_ball, ball(x)) :- member(_X, [1, 2]), assertion(true), throw(x).");
}

// 平たい道の true(Assertion) は、本体の最初の解だけで表明を見る。最上位の ; も、その切りの内に入れる。
void check_first_solution_only(const std::filesystem::path& root) {
  check_fails(root, {"h38_disjunction", "\ttest(h38_disjunction, true(X == 1)) :- X = 2 ; X = 1.",
                     "h38_disjunction true(X==1) fail"});
  check_passes(root, "h38_first", "\ttest(h38_first, true(X == 2)) :- X = 2 ; X = 1.");
  check_passes(root, "h38_conjunction", "\ttest(h38_conjunction, true(Y == 3)) :- X = 1, Y is X + 2.");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-judge";
  {
    Fixture fixture(root);
    check_halt_in_forall(root);
    check_stops_are_not_failures(root);
    check_thrown_outcomes(root);
    check_first_solution_only(root);
  }
  return mosa_test::failures();
}
