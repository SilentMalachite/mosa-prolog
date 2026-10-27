// ^^file_path/2 は、隣の正規ファイルをスクラッチが空のときだけ写す。
// 同梱ディレクトリと tests/conformance には書かない。一時木は tests/tmp だけ。

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

// 隣は a で始まる。写せれば get_char は a。無い名前は existence_error のまま。
// 一度書いたスクラッチは、二度目の file_path で隣の a に戻さない。
std::string path_text() {
  return "test(copy_sibling, true(C == a)) :-\n"
         "    ^^file_path('terms.pl', Path),\n"
         "    {open(Path, read, Stream), get_char(Stream, C)}.\n"
         "test(no_sibling, error(existence_error(source_sink, Path))) :-\n"
         "    ^^file_path('gone.pl', Path),\n"
         "    {open(Path, read, _Stream)}.\n"
         "test(keep_written, true(C == z)) :-\n"
         "    ^^file_path('keep.pl', Path),\n"
         "    {open(Path, write, Out), put_char(Out, z), close(Out)},\n"
         "    ^^file_path('keep.pl', Again),\n"
         "    {open(Again, read, In), get_char(In, C)}.\n";
}

void check_copy(const std::filesystem::path& root) {
  const auto suite = root / "paths";
  const auto prolog = suite / "logtalk" / "prolog";
  const auto report = root / "paths-report.md";
  const auto dir = prolog / "predicates" / "set_stream_position_2";
  const std::string terms = "a(1).\n";
  const std::string keep = "a(9).\n";
  mosa_test::check(write_text(dir / "tests.lgt", path_text()), "write tests");
  mosa_test::check(write_text(dir / "terms.pl", terms), "write terms");
  mosa_test::check(write_text(dir / "keep.pl", keep), "write keep");
  const auto conformance = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "conformance";
  const auto before_dir = regular_files(dir);
  const auto before_conformance = regular_files(conformance);
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  const std::string failed = between_headings(body, "不合格:", "別欄:");
  if (code != 0 || failed.find_first_not_of(" \t\r\n") != std::string::npos) show("path report", body);
  mosa_test::check(code == 0, "copy exits 0");
  mosa_test::check(has_exact_line(body, "分母: 3"), "copy denominator");
  mosa_test::check(has_exact_line(body, "合格数: 3"), "copy passed");
  mosa_test::check(failed.find("copy_sibling") == std::string::npos, "sibling char is a");
  mosa_test::check(failed.find("no_sibling") == std::string::npos, "missing sibling stays existence_error");
  mosa_test::check(failed.find("keep_written") == std::string::npos, "second file_path keeps the write");
  mosa_test::check(read_text(dir / "terms.pl") == terms, "terms bytes unchanged");
  mosa_test::check(read_text(dir / "keep.pl") == keep, "keep bytes unchanged");
  mosa_test::check(regular_files(dir) == before_dir, "tests dir gains no file");
  mosa_test::check(regular_files(conformance) == before_conformance, "conformance gains no file");
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite-path";
  const std::filesystem::path scratch = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "suite";
  {
    Fixture fixture(root);
    std::error_code error;
    std::filesystem::remove_all(scratch, error);
    check_copy(root);
  }
  mosa_test::check(!std::filesystem::exists(root), "fixture removed");
  mosa_test::check(directory_empty(scratch), "suite scratch empty");
  return mosa_test::failures();
}
