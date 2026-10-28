// lgtunit の suppress_text_output/0 と text_output_contents/1,2。意味は固定コミット 4decb64 の lgtunit.lgt。
// 一時ファイルは tests/tmp/suite-output。

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

// output_suppressed は suite の sics_write_term_3_103 と同じ形。suppress しても誤りはそのまま出る。
// output_restored は lgtunit の run_test/2 と同じく、前のテストの suppress が次へ漏れないこと。
// output_contents_closes は stream_property を先に置かない。選択点が開いたままだと後のゴールは別の機械で動く。
std::string output_text() {
  return "test(output_suppressed, error(instantiation_error)) :-\n"
         "    ^^suppress_text_output,\n"
         "    {write_term(1, [quoted(_)])}.\n"
         "test(output_suppressed_redirects, true(S \\== U)) :-\n"
         "    stream_property(U, alias(user_output)),\n"
         "    ^^suppress_text_output,\n"
         "    current_output(S).\n"
         "test(output_restored, true(S == U)) :-\n"
         "    stream_property(U, alias(user_output)),\n"
         "    current_output(S).\n"
         "test(output_contents, true(Cs == [f,o,o])) :-\n"
         "    ^^set_text_output(''),\n"
         "    {write(foo)},\n"
         "    ^^text_output_contents(Cs).\n"
         "test(output_contents_closes, true) :-\n"
         "    ^^set_text_output(''),\n"
         "    {write(foo)},\n"
         "    ^^text_output_contents(_),\n"
         "    current_output(S),\n"
         "    stream_property(S, alias(user_output)).\n"
         "test(output_contents_stream, true(Cs == [b,a,r])) :-\n"
         "    ^^set_text_output(out, ''),\n"
         "    {write(out, bar)},\n"
         "    ^^text_output_contents(out, Cs).\n"
         "test(output_contents_utf8, true(Cs == ['日','本'])) :-\n"
         "    ^^set_text_output(out, ''),\n"
         "    {write(out, '日本')},\n"
         "    ^^text_output_contents(out, Cs).\n";
}

const char* const output_names[] = {"output_suppressed",      "output_suppressed_redirects", "output_restored", "output_contents",
                                    "output_contents_closes", "output_contents_stream",      "output_contents_utf8"};

void check_output(const std::filesystem::path& root) {
  const auto suite = root / "output";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "output-report.md";
  mosa_test::check(write_text(prolog / "predicates" / "write_term_3" / "tests.lgt", output_text()),
                   "write output");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || !has_exact_line(body, "合格数: 7")) show("output report", body);
  mosa_test::check(code == 0, "output exits 0");
  mosa_test::check(has_exact_line(body, "分母: 7"), "output denominator");
  mosa_test::check(has_exact_line(body, "合格数: 7"), "output passed");
  for (const char* name : output_names) {
    mosa_test::check(failed.find(std::string{name} + " ") == std::string::npos, name);
  }
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-output";
  {
    Fixture fixture(root);
    check_output(root);
  }
  return mosa_test::failures();
}
