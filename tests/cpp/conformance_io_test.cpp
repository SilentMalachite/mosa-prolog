// 準拠テストの一時出力と隣の Prolog。同梱の logtalk は読まない。一時木は tests/tmp だけ。

#include "harness.hpp"
#include "mosa/suite.hpp"

#include <algorithm>
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

bool path_exists(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::exists(path, error) && !error;
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

std::vector<std::string> regular_files(const std::filesystem::path& root) {
  std::vector<std::string> names;
  std::error_code error;
  if (!std::filesystem::is_directory(root, error) || error) return names;
  const auto options = std::filesystem::directory_options::skip_permission_denied;
  std::filesystem::recursive_directory_iterator cursor(root, options, error);
  if (error) return names;
  const std::filesystem::recursive_directory_iterator end;
  while (cursor != end) {
    std::error_code entry_error;
    if (cursor->is_regular_file(entry_error) && !entry_error) {
      const auto relative = std::filesystem::relative(cursor->path(), root, entry_error);
      if (!entry_error) names.push_back(relative.generic_string());
    }
    cursor.increment(error);
    if (error) break;
  }
  std::sort(names.begin(), names.end());
  return names;
}

bool directory_empty(const std::filesystem::path& path) {
  std::error_code error;
  if (!std::filesystem::exists(path, error) || error) return !error;
  return std::filesystem::is_empty(path, error) && !error;
}

void show(std::string_view label, std::string_view text) { std::cerr << label << '\n' << text << '\n'; }

std::string tester_text() {
  return ":- initialization((\n"
         "\tlogtalk_load_context(directory, Directory),\n"
         "\tatom_concat(Directory, 'main.pl', File),\n"
         "\t'$lgt_load_prolog_file'(File),\n"
         "\tlogtalk_load(missing)\n"
         ")).\n";
}

std::string loaded_text() { return "test(loaded, true(L == [1])) :- findall(X, {a(X)}, L).\n"; }

std::string nl_text() {
  return "test(nl_out, true(Assertion)) :-\n"
         "    ^^set_text_output(''),\n"
         "    {nl},\n"
         "    ^^text_output_assertion('\\n', Assertion).\n";
}

std::string missing_text() { return "test(missing, true) :- ^^not_in_list(a).\n"; }

std::string colon_tester() {
  return ":- initialization((\n"
         "\tlogtalk_load_context(directory, Directory),\n"
         "\tatom_concat(Directory, 'main.pl', File),\n"
         "\t'$lgt_load_prolog_file'(File),\n"
         "\tlogtalk_load(missing),\n"
         "\ttests::run\n"
         ")).\n";
}

std::string helper_text() {
  return "helper :- ^^clean_text_output.\n"
         "test(ok, true) :- {true}.\n";
}

void check_pair(const std::filesystem::path& root) {
  const auto suite = root / "io";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "io-report.md";
  const auto include = prolog / "directives" / "include_1";
  const auto nl = prolog / "predicates" / "nl_1";
  mosa_test::check(write_text(include / "tester.lgt", tester_text()), "write tester");
  mosa_test::check(write_text(include / "main.pl", "a(1).\n"), "write main");
  mosa_test::check(write_text(include / "tests.lgt", loaded_text()), "write loaded");
  mosa_test::check(write_text(nl / "tests.lgt", nl_text()), "write nl");
  const auto conformance = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "conformance";
  const auto before_fixture = regular_files(suite);
  const auto before_conformance = regular_files(conformance);
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || !has_exact_line(body, "合格数: 2")) show("io report", body);
  // 隣の Prolog と nl の一時出力。2 件とも合格しないと分母は 2 にならない。
  mosa_test::check(code == 0, "io exits 0");
  mosa_test::check(has_exact_line(body, "分母: 2"), "io denominator");
  mosa_test::check(has_exact_line(body, "合格数: 2"), "io passed");
  mosa_test::check(failed.find_first_not_of(" \t\r\n") == std::string::npos, "io adds no failure");
  mosa_test::check(regular_files(suite) == before_fixture, "fixture gains no file");
  mosa_test::check(regular_files(conformance) == before_conformance, "conformance gains no file");
}

void check_colon_run(const std::filesystem::path& root) {
  const auto suite = root / "colon-run";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "colon-run-report.md";
  const auto include = prolog / "directives" / "include_1";
  mosa_test::check(write_text(include / "tester.lgt", colon_tester()), "write colon tester");
  mosa_test::check(write_text(include / "main.pl", "a(1).\n"), "write colon main");
  mosa_test::check(write_text(include / "tests.lgt", loaded_text()), "write colon tests");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find("a/1") != std::string::npos) show("colon report", body);
  // tests::run は実行しない。:: を読めて main.pl を consult できれば a/1 がある。
  mosa_test::check(code == 0, "colon exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "colon denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "colon passed");
  mosa_test::check(failed.find("a/1") == std::string::npos, "colon consulted main");
  mosa_test::check(failed.find("run") == std::string::npos, "colon did not run tests");
}

void check_canonical(const std::filesystem::path& root) {
  const auto suite = root / "canon";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "canon-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "atom_1" / "tests.lgt", helper_text()), "write helper");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (failed.find("load") != std::string::npos || code != 0) show("canon report", body);
  // helper は呼ばない。節の ^^ が正準形なら load は落ちず、ok だけ合格する。
  mosa_test::check(code == 0, "canon exits 0");
  mosa_test::check(has_exact_line(body, "分母: 1"), "canon denominator");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "canon passed");
  mosa_test::check(failed.find("load") == std::string::npos, "canon is not a load failure");
}

void check_unknown(const std::filesystem::path& root) {
  const auto suite = root / "unknown";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "unknown-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "put_byte_2" / "tests.lgt", missing_text()), "write missing");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (failed.find("not_in_list/1") == std::string::npos) show("unknown report", body);
  // 未実装の ^^ は利用者述語にしない。存在エラーではなく、名前とアリティを書く。
  mosa_test::check(code == 1, "unknown exits 1");
  mosa_test::check(has_exact_line(body, "分母: 1"), "unknown denominator");
  mosa_test::check(has_exact_line(body, "合格数: 0"), "unknown passed");
  mosa_test::check(failed.find("not_in_list/1") != std::string::npos, "unknown names predicate");
  mosa_test::check(failed.find("existence_error") == std::string::npos, "unknown is not a user predicate");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "io-fixture";
  const std::filesystem::path scratch = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite";
  {
    Fixture fixture(root);
    std::error_code error;
    std::filesystem::remove_all(scratch, error);
    check_pair(root);
    check_colon_run(root);
    check_canonical(root);
    check_unknown(root);
  }
  mosa_test::check(!path_exists(root), "fixture removed");
  mosa_test::check(directory_empty(scratch), "suite scratch empty");
  return mosa_test::failures();
}
