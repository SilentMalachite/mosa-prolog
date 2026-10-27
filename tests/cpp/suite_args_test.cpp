// mosatest --suite の引数。報告先を省けるのは、リポジトリ直下から同梱の tests/conformance を渡したときだけ。
// 別のスイートを渡して docs/conformance.md と .json を上書きしないよう、それ以外は --report を求める。

#include "harness.hpp"
#include "mosa/suite.hpp"

#include <array>
#include <filesystem>
#include <span>
#include <string_view>
#include <system_error>

namespace {

mosa::SuiteArgs parse(std::span<const std::string_view> args) { return mosa::parse_suite_args(args); }

void check_bundled_default() {
  const auto args = std::to_array<std::string_view>({"tests/conformance"});
  const mosa::SuiteArgs parsed = parse(args);
  mosa_test::check(parsed.ok, "bundled suite needs no report");
  mosa_test::check(parsed.dir == "tests/conformance", "bundled dir");
  mosa_test::check(parsed.report == "docs/conformance.md", "bundled default report");
}

void check_bundled_absolute() {
  const std::string dir = (std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "conformance").string();
  const std::array<std::string_view, 1> args{dir};
  const mosa::SuiteArgs parsed = parse(args);
  mosa_test::check(parsed.ok && parsed.report == "docs/conformance.md", "bundled suite by absolute path");
}

void check_other_needs_report() {
  const auto args = std::to_array<std::string_view>({"tests/pl"});
  mosa_test::check(!parse(args).ok, "other suite without report is refused");
  const auto missing = std::to_array<std::string_view>({"no/such/suite"});
  mosa_test::check(!parse(missing).ok, "missing suite without report is refused");
}

void check_explicit_report() {
  const auto args = std::to_array<std::string_view>({"tests/pl", "--report", "tests/tmp/other.md"});
  const mosa::SuiteArgs parsed = parse(args);
  mosa_test::check(parsed.ok, "explicit report accepted");
  mosa_test::check(parsed.dir == "tests/pl", "explicit dir");
  mosa_test::check(parsed.report == "tests/tmp/other.md", "explicit report");
  const auto bundled = std::to_array<std::string_view>({"tests/conformance", "--report", "tests/tmp/b.md"});
  mosa_test::check(parse(bundled).report == "tests/tmp/b.md", "explicit report wins for bundled suite");
}

void check_malformed() {
  const auto empty = std::array<std::string_view, 0>{};
  mosa_test::check(!parse(empty).ok, "no dir");
  const auto dangling = std::to_array<std::string_view>({"tests/pl", "--report"});
  mosa_test::check(!parse(dangling).ok, "report without path");
  const auto blank = std::to_array<std::string_view>({"tests/pl", "--report", ""});
  mosa_test::check(!parse(blank).ok, "empty report path");
  const auto extra = std::to_array<std::string_view>({"tests/pl", "--report", "a.md", "b"});
  mosa_test::check(!parse(extra).ok, "trailing argument");
  const auto unknown = std::to_array<std::string_view>({"tests/pl", "--out", "a.md"});
  mosa_test::check(!parse(unknown).ok, "unknown option");
}

}  // namespace

int main() {
  // 既定の報告先はカレントからの相対なので、リポジトリ直下で判定する。
  std::error_code error;
  const auto previous = std::filesystem::current_path(error);
  std::filesystem::current_path(MOSA_SOURCE_DIR, error);
  mosa_test::check(!error, "enter source dir");
  check_bundled_default();
  check_bundled_absolute();
  check_other_needs_report();
  check_explicit_report();
  check_malformed();
  std::filesystem::current_path(previous, error);
  return mosa_test::failures();
}
