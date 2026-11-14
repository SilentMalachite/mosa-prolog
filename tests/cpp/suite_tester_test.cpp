// tester.lgt に直接書かれた節と dynamic・multifile の指令は、テストの前に user へ読む（lgtunit は tester.lgt を
// 読み込んでからテストを流す）。initialization は今と同じく走らせず、:- if から :- endif の中の項は読まない。
// initialization を走らせないことは assertz(ran) の initialization で確かめる。logtalk_load と halt の
// initialization は、logtalk_load がどこにも無いので走っても halt に届かず、それだけでは確かめられない。
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

// 3〜6 行目は format_2 と format_3 の tester.lgt と同じ形（節、multifile と dynamic、logtalk_load と halt を呼ぶ
// initialization）。前の 2 行は走れば跡が残る initialization で、mosa は読み込みの initialization の 1 つが例外を
// 投げると残りを走らせないので、logtalk_load の initialization より前に置く。後ろの 3 行は条件の中の節。
std::string tester_text() {
  return ":- dynamic(ran/0).\n"
         ":- initialization(assertz(ran)).\n"
         ":- multifile(portray/1).\n"
         ":- dynamic(portray/1).\n"
         "portray(X) :- X == zz, write(yy).\n"
         ":- initialization((logtalk_load(lgtunit(loader)), halt)).\n"
         ":- if(true).\n"
         "inside_if.\n"
         ":- endif.\n";
}

std::string tests_text() {
  return ":- object(tests, extends(lgtunit)).\n"
         "\ttest(t1, true) :- {clause(portray(zz), _)}.\n"
         "\ttest(t2, true) :- {predicate_property(portray(_), dynamic)}.\n"
         "\ttest(t3, true) :- {predicate_property(portray(_), multifile)}.\n"
         "\ttest(t4, fail) :- {ran}.\n"
         "\ttest(t5, fail) :- {current_predicate(inside_if/0)}.\n"
         ":- end_object.\n";
}

std::string failed_section(std::string_view text) {
  const auto begin = text.find("不合格:");
  if (begin == std::string_view::npos) return {};
  const auto end = text.find("別欄:", begin);
  return std::string{text.substr(begin, end == std::string_view::npos ? std::string_view::npos : end - begin)};
}

bool case_failed(std::string_view failed, std::string_view name) {
  return failed.find(std::string{"tests.lgt "} + std::string{name} + " ") != std::string_view::npos;
}

void check_tester_clauses(const std::filesystem::path& root) {
  const auto suite = root / "tester";
  const auto dir = suite / "logtalk" / "prolog" / "predicates" / "atom_1";
  const auto report = root / "tester-report.md";
  mosa_test::check(write_text(dir / "tester.lgt", tester_text()), "write tester.lgt");
  mosa_test::check(write_text(dir / "tests.lgt", tests_text()), "write tests.lgt");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = failed_section(body);
  if (code != 0 || !has_exact_line(body, "合格数: 5")) std::cerr << "tester report\n" << body << '\n';
  mosa_test::check(has_exact_line(body, "分母: 5"), "tester denominator");
  mosa_test::check(has_exact_line(body, "合格数: 5"), "tester passes all");
  mosa_test::check(!case_failed(failed, "t1"), "tester clause is loaded");
  mosa_test::check(!case_failed(failed, "t2"), "dynamic directive is honored");
  mosa_test::check(!case_failed(failed, "t3"), "multifile directive is honored");
  mosa_test::check(!case_failed(failed, "t4"), "initialization does not run");
  mosa_test::check(!case_failed(failed, "t5"), "clause inside :- if is not loaded");
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
