// ソースは既に UTF-8。utf8 / UTF-8 / utf-8 の encoding/1 だけ読み込みを通す。
// latin1 は未知の指示のまま、ファイル全体が load になる。
// 一時ファイルは tests/tmp/suite-encoding。

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

void check_encoding(const std::filesystem::path& root) {
  const auto suite = root / "encoding";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "encoding-report.md";
  mosa_test::check(write_text(prolog / "unicode/builtins/tests.lgt",
                              ":- encoding('UTF-8').\ntest(enc_utf8, true).\n"),
                   "write UTF-8");
  mosa_test::check(write_text(prolog / "unicode/case_variables/tests.lgt",
                              ":- encoding(utf8).\ntest(enc_plain, true).\n"),
                   "write utf8");
  mosa_test::check(write_text(prolog / "unicode/escape_sequences/tests.lgt",
                              ":- encoding('utf-8').\ntest(enc_dash, true).\n"),
                   "write utf-8");
  mosa_test::check(write_text(prolog / "predicates/atom_1/tests.lgt",
                              ":- encoding(latin1).\ntest(enc_latin1, true).\n"),
                   "write latin1");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  const bool utf8_ok = failed.find("enc_utf8") == std::string::npos &&
                       failed.find("enc_plain") == std::string::npos &&
                       failed.find("enc_dash") == std::string::npos;
  const bool latin_fails = has_exact_line(failed, "predicates/atom_1/tests.lgt enc_latin1 true load");
  if (code != 1 || !has_exact_line(body, "分母: 4") || !has_exact_line(body, "合格数: 3") || !utf8_ok ||
      !latin_fails) {
    show("encoding report", body);
  }
  mosa_test::check(code == 1, "encoding exits 1");
  mosa_test::check(has_exact_line(body, "分母: 4"), "encoding denominator");
  mosa_test::check(has_exact_line(body, "合格数: 3"), "three utf-8 names pass");
  mosa_test::check(utf8_ok, "utf8 names are not load failures");
  mosa_test::check(latin_fails, "latin1 still fails to load");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-encoding";
  {
    Fixture fixture(root);
    check_encoding(root);
  }
  mosa_test::check(!std::filesystem::exists(root), "fixture removed");
  return mosa_test::failures();
}
