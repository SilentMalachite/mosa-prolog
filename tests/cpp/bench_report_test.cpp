// mosatest --bench --report（計画 Task 13）。引数の --report と、docs/bench.json の形の書き足し。同じ file の行だけを
// 置き換え、ほかの行は残し、file の順に並べる。CI（ci.yml の Benchmarks）は 1 行 1 プログラムの形を sed で読む。
// 一時ファイルは tests/tmp の下だけに作る。

#include "bench_report.hpp"
#include "harness.hpp"
#include "mosa/bench.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>

namespace {

std::string read_all(const std::filesystem::path& file) {
  std::ifstream in{file, std::ios::binary};
  return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

void check_args() {
  const auto args = std::to_array<std::string_view>({"bench/tak.pl", "--count", "100", "--report", "out.json"});
  const mosa::BenchArgs parsed = mosa::parse_bench_args(args);
  mosa_test::check(parsed.ok, "report accepted");
  mosa_test::check(parsed.count == 100, "count kept with report");
  mosa_test::check(parsed.report == "out.json", "report path");
  const auto plain = std::to_array<std::string_view>({"bench/tak.pl"});
  mosa_test::check(mosa::parse_bench_args(plain).report.empty(), "no report by default");
  const auto missing = std::to_array<std::string_view>({"bench/tak.pl", "--report"});
  mosa_test::check(!mosa::parse_bench_args(missing).ok, "report needs a value");
}

void check_upsert(const std::filesystem::path& json) {
  std::error_code error;
  std::filesystem::remove(json, error);
  mosa_test::check(mosa::write_bench_report(json, {"bench/tak.pl", 100, 2503, 327, "7.6"}), "first write");
  mosa_test::check(mosa::write_bench_report(json, {"bench/nrev.pl", 20000, 3147, 149, "21.1"}), "second write");
  mosa_test::check(mosa::write_bench_report(json, {"bench/nrev.pl", 20000, 3000, 150, "20.0"}), "replace");
  const std::string text = read_all(json);
  const std::string nrev =
      "  {\"file\":\"bench/nrev.pl\",\"count\":20000,\"interpreter_ms\":3000,\"translated_ms\":150,\"ratio\":20.0},\n";
  const std::string tak =
      "  {\"file\":\"bench/tak.pl\",\"count\":100,\"interpreter_ms\":2503,\"translated_ms\":327,\"ratio\":7.6}\n";
  mosa_test::check(text.find(nrev + tak + " ]\n}\n") != std::string::npos, "sorted, replaced, last without comma");
  mosa_test::check(text.find("3147") == std::string::npos, "old nrev line gone");
  mosa_test::check(text.starts_with("{\"generated\":"), "generated note first");
}

void check_quote(const std::filesystem::path& json) {
  std::error_code error;
  std::filesystem::remove(json, error);
  mosa_test::check(mosa::write_bench_report(json, {"bench\\a\"b.pl", 1, 2, 3, "0.7"}), "quoted write");
  mosa_test::check(mosa::write_bench_report(json, {"bench\\a\"b.pl", 1, 5, 3, "1.7"}), "quoted replace");
  const std::string text = read_all(json);
  mosa_test::check(text.find("{\"file\":\"bench\\\\a\\\"b.pl\",\"count\":1,\"interpreter_ms\":5,") != std::string::npos,
                   "backslash and quote escaped");
  mosa_test::check(text.find("\"interpreter_ms\":2,") == std::string::npos, "escaped key replaced");
}

}  // namespace

int main() {
  const std::filesystem::path dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "bench_report_test";
  std::error_code error;
  std::filesystem::create_directories(dir, error);
  mosa_test::check(!error, "create tests/tmp/bench_report_test");
  check_args();
  check_upsert(dir / "bench.json");
  check_quote(dir / "quoted.json");
  return mosa_test::failures();
}
