// 節から呼んだ ^^ の補助と、出力引数に値が入っている ^^（レビュー 2026-11-20 の M-54）。前は節の ^^ を名前の無い
// 字面に書いて読み直したので、出力の変数を見つけられずに失敗し、束縛も呼び手へ返さなかった。出力引数に値が入って
// いると、比べずに失敗した。lgtunit の補助は普通の述語なので、出力引数は値と単一化する。値が違えば失敗する。
// 1 件ごとに 1 ファイルのスイートを tests/tmp/suite-super に書いて run_iso_suite で流し、分母と合格数と不合格の行を見る。

#include "harness.hpp"
#include "mosa/suite.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

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

struct Suite {
  std::string_view name;
  std::string_view clauses;
  int denominator;
  int passed;
  std::vector<std::string_view> failed;
};

void check_suite(const std::filesystem::path& root, const Suite& item) {
  const std::string name{item.name};
  const auto suite = root / name;
  const std::string text = ":- object(tests, extends(lgtunit)).\n" + std::string{item.clauses} + "\n:- end_object.\n";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_1" / "tests.lgt", text),
                   name + ": write the suite");
  const auto report = root / (name + "-report.md");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string shown = " in\n" + body;
  mosa_test::check(code == (item.failed.empty() ? 0 : 1), name + ": exit code" + shown);
  mosa_test::check(body.find("\n分母: " + std::to_string(item.denominator) + "\n") != std::string::npos,
                   name + ": denominator " + std::to_string(item.denominator) + shown);
  mosa_test::check(body.find("\n合格数: " + std::to_string(item.passed) + "\n") != std::string::npos,
                   name + ": passed " + std::to_string(item.passed) + shown);
  const std::string failed = failed_lines(body);
  for (const std::string_view line : item.failed) {
    const std::string full = "predicates/atom_1/tests.lgt " + std::string{line} + "\n";
    mosa_test::check(failed.find(full) != std::string::npos, name + ": " + full + shown);
  }
}

// 節の ^^ は生きたゴールの出力引数を単一化し、後戻りで外す。前は s1、s2、s3、s5 が失敗した。
// s4 と s6 は値が違えば失敗する守りで、前後で同じ。s7 は後戻りで束縛が消える守り。
void check_clause(const std::filesystem::path& root) {
  check_suite(root, {"super_clause",
                     "\thelper(P) :- ^^file_path(foo, P).\n"
                     "\ttwice :- ^^file_path(foo, P), ^^file_path(foo, P).\n"
                     "\tother(P) :- ^^file_path(bar, P).\n"
                     "\tout(Cs) :- ^^text_output_contents(Cs).\n"
                     "\tundo(P) :- (^^file_path(foo, P), fail ; true).\n"
                     "\ttest(s1, true) :- helper(P), atom(P).\n"
                     "\ttest(s2, true) :- helper(P), ^^file_path(foo, Q), P == Q.\n"
                     "\ttest(s3, true) :- twice.\n"
                     "\ttest(s4, true) :- ^^file_path(foo, P), other(P).\n"
                     "\ttest(s5, true) :- ^^set_text_output(''), write(abc), out(Cs), Cs == [a, b, c].\n"
                     "\ttest(s6, true) :- helper(wrong).\n"
                     "\ttest(s7, true) :- undo(P), var(P).\n",
                     7, 5, {"s4 true fail", "s6 true fail"}});
}

// 本体の ^^ は、値の入った出力引数を比べ、中の変数を束縛して返す。前は b1、b3、b4 が失敗した。
// b2 と b5 は値が違えば失敗する守りで、前後で同じ。
void check_bound(const std::filesystem::path& root) {
  check_suite(root, {"super_bound",
                     "\ttest(b1, true) :- ^^file_path(foo, P), ^^file_path(foo, P).\n"
                     "\ttest(b2, true) :- ^^file_path(foo, P), ^^file_path(bar, P).\n"
                     "\ttest(b3, true) :- ^^file_path(foo, _).\n"
                     "\ttest(b4, true) :- ^^set_text_output(''), write(abc), ^^text_output_contents([a|T]),"
                     " T == [b, c].\n"
                     "\ttest(b5, true) :- ^^file_path(foo, wrong).\n",
                     5, 3, {"b2 true fail", "b5 true fail"}});
}

// 本体の ^^ の束縛は、後戻りで外れた枝のものを最後に読み直さない。前は最後の ^^ の束縛だけを別に持って読み直したので、
// 外れた枝の ^^ が最後になる r2 が落ちた。r1 は束縛を全部持つと落ちる守りで、r3 は前後で同じ守り。
void check_backtrack(const std::filesystem::path& root) {
  check_suite(root, {"super_backtrack",
                     "\ttest(r1, true(var(P))) :- (^^file_path(foo, P), fail ; true), ^^file_path(bar, Q), atom(Q).\n"
                     "\ttest(r2, true(var(P))) :- (^^file_path(foo, P), fail ; true), atom(a).\n"
                     "\ttest(r3, true) :- (^^file_path(foo, P), fail ; true), var(P).\n",
                     3, 3, {}});
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-super";
  {
    Fixture fixture(root);
    check_clause(root);
    check_bound(root);
    check_backtrack(root);
  }
  return mosa_test::failures();
}
