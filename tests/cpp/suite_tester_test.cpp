// tester.lgt に直接書かれた節と dynamic・multifile の指令は、テストの前に user へ読む（lgtunit は tester.lgt を
// 読み込んでからテストを流す）。initialization の中身（logtalk_load と halt）は今と同じく走らせない。
// 一時ファイルは tests/tmp/suite-tester。

#include "harness.hpp"
#include "mosa/suite.hpp"

#include <filesystem>
#include <fstream>
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

std::string_view line_at(std::string_view text, std::size_t start) {
  const auto end = text.find('\n', start);
  auto line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  return line;
}

bool has_exact_line(std::string_view text, std::string_view expected) {
  std::size_t start = 0;
  while (start <= text.size()) {
    if (line_at(text, start) == expected) return true;
    const auto end = text.find('\n', start);
    if (end == std::string_view::npos) break;
    start = end + 1;
  }
  return false;
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

// format_2 と format_3 の tester.lgt と同じ形（節、multifile と dynamic、logtalk_load と halt を呼ぶ initialization）。
std::string tester_text() {
  return ":- multifile(portray/1).\n"
         ":- dynamic(portray/1).\n"
         "portray(X) :- X == zz, write(yy).\n"
         ":- initialization((logtalk_load(lgtunit(loader)), halt)).\n";
}

std::string tests_text() {
  return ":- object(tests, extends(lgtunit)).\n"
         "\ttest(t1, true) :- {clause(portray(zz), _)}.\n"
         "\ttest(t2, true) :- {predicate_property(portray(_), dynamic)}.\n"
         ":- end_object.\n";
}

void check_tester_clauses(const std::filesystem::path& root) {
  const auto suite = root / "tester";
  const auto dir = suite / "logtalk" / "prolog" / "predicates" / "atom_1";
  const auto report = root / "tester-report.md";
  mosa_test::check(write_text(dir / "tester.lgt", tester_text()), "write tester.lgt");
  mosa_test::check(write_text(dir / "tests.lgt", tests_text()), "write tests.lgt");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  if (code != 0 || !has_exact_line(body, "合格数: 2")) std::cerr << "tester report\n" << body << '\n';
  mosa_test::check(has_exact_line(body, "分母: 2"), "tester denominator");
  mosa_test::check(has_exact_line(body, "合格数: 2"), "tester clause and dynamic directive are loaded");
  // halt が走れば次の件は halt の結果になり、合格数が 2 にならない。終了コードも 0 のまま。
  mosa_test::check(code == 0, "tester exits 0");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-tester";
  {
    Fixture fixture(root);
    check_tester_clauses(root);
  }
  mosa_test::check(!std::filesystem::exists(root), "fixture removed");
  return mosa_test::failures();
}
