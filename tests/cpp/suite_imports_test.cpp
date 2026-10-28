// 試験器はテストの機械の user へ lists、apply、between、format、strings、japanese を取り込む（iso-notes Section 5 Autoload の決定 A）。
// tests.lgt が同じ名前を自分で定義していれば、その名前だけ取り込まない。一時ファイルは tests/tmp/suite-imports。

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

std::string after_heading(std::string_view text, std::string_view heading) {
  const auto begin = text.find(heading);
  if (begin == std::string_view::npos) return {};
  auto start = begin + heading.size();
  if (start < text.size() && text[start] == '\n') ++start;
  return std::string{text.substr(start)};
}

std::string between_headings(std::string_view text, std::string_view from, std::string_view to) {
  const std::string chunk = after_heading(text, from);
  const auto stop = chunk.find(to);
  if (stop == std::string::npos) return chunk;
  return chunk.substr(0, stop);
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

void show(std::string_view label, std::string_view text) { std::cerr << label << '\n' << text << '\n'; }

// libs_local_wins の last/2 は library(lists) にもある。取り込みと衝突すると読み込みごと落ちる。
std::string imports_text() {
  return "last(_, mine).\n"
         "test(libs_between, true) :-\n"
         "    between(1, 2, _).\n"
         "test(libs_curly, true(L == [a,b])) :-\n"
         "    {append([a], [b], L)}.\n"
         "test(libs_apply, true) :-\n"
         "    {maplist(integer, [1,2])}.\n"
         "test(libs_strings, true(X == 'ABC')) :-\n"
         "    {upcase_atom(abc, X)}.\n"
         "test(libs_msort, true(L == [a,b,b])) :-\n"
         "    msort([b,a,b], L).\n"
         "test(libs_local_wins, true(X == mine)) :-\n"
         "    last([1,2], X).\n";
}

const char* const import_names[] = {"libs_between", "libs_curly", "libs_apply",
                                    "libs_strings", "libs_msort", "libs_local_wins"};

void check_imports(const std::filesystem::path& root) {
  const auto suite = root / "imports";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "imports-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "char_code_2" / "tests.lgt", imports_text()), "write imports");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || !has_exact_line(body, "合格数: 6")) show("imports report", body);
  mosa_test::check(code == 0, "imports exits 0");
  mosa_test::check(has_exact_line(body, "分母: 6"), "imports denominator");
  mosa_test::check(has_exact_line(body, "合格数: 6"), "imports passed");
  for (const char* name : import_names) {
    mosa_test::check(failed.find(std::string{name} + " ") == std::string::npos, name);
  }
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-imports";
  {
    Fixture fixture(root);
    check_imports(root);
  }
  return mosa_test::failures();
}
