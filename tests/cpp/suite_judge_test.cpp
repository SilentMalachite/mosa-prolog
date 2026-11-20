// Logtalk のスイートの判定器は、落ちるべき試験を不合格にし、プロセスごと落ちない（レビュー 2026-11-20 の W1）。
// 1 件ごとに 1 ファイルのスイートを tests/tmp/suite-judge に書いて run_iso_suite で流し、報告の不合格の行を見る。
// C-04: 誤りを期待する試験で forall/2 の生成側が halt すると、例外の項の無い Stop を読んでプロセスが落ちた。

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
  std::string_view clause;
  std::string_view failed;
};

void check_fails(const std::filesystem::path& root, const Case& item) {
  const std::string name{item.name};
  const auto suite = root / name;
  const auto report = root / (name + "-report.md");
  const std::string text = ":- object(tests, extends(lgtunit)).\n\t" + std::string{item.clause} + "\n:- end_object.\n";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_1" / "tests.lgt", text),
                   name + ": write the suite");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  mosa_test::check(code == 1, name + ": the suite fails");
  const std::string line = "predicates/atom_1/tests.lgt " + std::string{item.failed} + "\n";
  mosa_test::check(failed_lines(body).find(line) != std::string::npos, name + ": " + line + " in\n" + body);
}

void check_halt_in_forall() {
  const std::array<Case, 3> cases{{
      {"c04_error", "test(c04_error, error(instantiation_error)) :- forall(halt(0), true).",
       "c04_error error(instantiation_error) halt(0)"},
      {"c04_errors", "test(c04_errors, errors([instantiation_error, type_error(_, _)])) :- forall(halt(3), true).",
       "c04_errors errors([instantiation_error,type_error(_,_)]) halt(3)"},
      {"c04_false", "test(c04_false, false) :- forall(halt(0), true).", "c04_false false halt(0)"},
  }};
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-judge";
  Fixture fixture(root);
  for (const Case& item : cases) check_fails(root, item);
}

}  // namespace

int main() {
  check_halt_in_forall();
  return mosa_test::failures();
}
