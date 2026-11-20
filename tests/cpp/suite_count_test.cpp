// Logtalk のスイートの分母は、ファイルに書かれた試験をすべて数える（レビュー 2026-11-20 の H-40）。
// 1 件ごとに 1 ファイルのスイートを tests/tmp/suite-count に書いて run_iso_suite で流し、報告の分母と合格数と不合格の行を見る。
// test/1 は test(Name, true) と同じで、前は読む側が黙って捨てていたので分母にも不合格にも出なかった。

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

// 不合格の行は「ファイル 名前 期待 実際」。期待の行は、ファイルの部分を除いて書く。
struct Count {
  std::string_view name;
  std::string_view text;
  int denominator;
  int passed;
  std::vector<std::string_view> failed;
};

void check_count(const std::filesystem::path& root, const Count& item) {
  const std::string name{item.name};
  const auto suite = root / name;
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_1" / "tests.lgt", item.text),
                   name + ": write the suite");
  const auto report = root / (name + "-report.md");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string label = " in\n" + body;
  mosa_test::check(code == (item.failed.empty() ? 0 : 1), name + ": exit code " + std::to_string(code) + label);
  mosa_test::check(body.find("\n分母: " + std::to_string(item.denominator) + "\n") != std::string::npos,
                   name + ": denominator " + std::to_string(item.denominator) + label);
  mosa_test::check(body.find("\n合格数: " + std::to_string(item.passed) + "\n") != std::string::npos,
                   name + ": passed " + std::to_string(item.passed) + label);
  const std::string failed = failed_lines(body);
  for (const std::string_view line : item.failed) {
    const std::string full = "predicates/atom_1/tests.lgt " + std::string{line} + "\n";
    mosa_test::check(failed.find(full) != std::string::npos, name + ": " + full + label);
  }
}

std::string object(std::string_view clauses) {
  return ":- object(tests, extends(lgtunit)).\n" + std::string{clauses} + "\n:- end_object.\n";
}

// 本体の無い test(c). も、本体のある test(a) :- fail. も 1 件ずつ数える。
void check_test_1() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-count";
  Fixture fixture(root);
  check_count(root, {"test_1", object("\ttest(a) :- fail.\n\ttest(b, true).\n\ttest(c).\n"), 3, 2, {"a true fail"}});
  // 読み込みが落ちたファイルは、test/1 だけでも、その数だけ load で不合格になる。前は分母に何も足さなかった。
  check_count(root, {"test_1_load", object("\tatom_length(_, _).\n\ttest(d) :- true.\n\ttest(e).\n"), 2, 0,
                     {"d true load", "e true load"}});
}

}  // namespace

int main() {
  check_test_1();
  return mosa_test::failures();
}
