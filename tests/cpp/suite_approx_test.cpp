// 判定器の =~=。式は固定コミット 4decb64 の lgtunit.lgt の (=~=)/2 と同じ。
// 一時ファイルは tests/tmp/suite-approx。

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

// approx_far と approx_length は不合格に数えられるのが正しい。
std::string approx_text() {
  return ":- uses(lgtunit, [op(700, xfx, =~=), (=~=)/2]).\n"
         "test(approx_pi, true(X =~= 3.1415927)) :-\n"
         "    {X is pi}.\n"
         "test(approx_zero, true(X =~= 0.0)) :-\n"
         "    {X is cos(pi / 2)}.\n"
         "test(approx_list, true([1.0, 2.0] =~= [1.0000001, 2.0])) :-\n"
         "    true.\n"
         "test(approx_far, true(1.0 =~= 1.1)) :-\n"
         "    true.\n"
         "test(approx_length, true([1.0] =~= [1.0, 2.0])) :-\n"
         "    true.\n";
}

void check_approx(const std::filesystem::path& root) {
  const auto suite = root / "approx";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "approx-report.md";
  mosa_test::check(write_text(prolog / "functions" / "pi_0" / "tests.lgt", approx_text()), "write approx");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (!has_exact_line(body, "合格数: 3")) show("approx report", body);
  mosa_test::check(code != 0, "approx reports failures");
  mosa_test::check(has_exact_line(body, "分母: 5"), "approx denominator");
  mosa_test::check(has_exact_line(body, "合格数: 3"), "approx passed");
  mosa_test::check(failed.find("approx_pi ") == std::string::npos, "approx_pi");
  mosa_test::check(failed.find("approx_zero ") == std::string::npos, "approx_zero");
  mosa_test::check(failed.find("approx_list ") == std::string::npos, "approx_list");
  mosa_test::check(failed.find("approx_far ") != std::string::npos, "approx_far fails");
  mosa_test::check(failed.find("approx_length ") != std::string::npos, "approx_length fails");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-approx";
  {
    Fixture fixture(root);
    check_approx(root);
  }
  return mosa_test::failures();
}
