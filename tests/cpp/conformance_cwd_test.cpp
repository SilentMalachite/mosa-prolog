// 準拠スイートの相対パス。Logtalk の tester はテストのディレクトリで動かすので、{open(d1, write, S)} の d1 は
// そのディレクトリに出来て ^^clean_file(d1) で消える。mosatest ではスクラッチがその代わりで、カレントには何も残らない。

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

bool path_exists(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::exists(path, error) && !error;
}

bool has_exact_line(const std::string& body, std::string_view line) {
  std::istringstream input(body);
  std::string current;
  while (std::getline(input, current)) {
    if (current == line) return true;
  }
  return false;
}

// 書いたファイルを ^^file_path で読み戻せれば、相対の open はスクラッチに出来ている。
// このランナーは cleanup をテストごとに呼ぶので、書きと読みを 1 つのテストにまとめる。
constexpr std::string_view kTests =
    "test(rel_open, true(C == x)) :-\n"
    "    {open(stray_rel, write, S), write(S, x), close(S)},\n"
    "    ^^file_path(stray_rel, P), {open(P, read, R), get_char(R, C), close(R)}.\n"
    "cleanup :- ^^clean_file(stray_rel).\n";

void check_relative_open(const std::filesystem::path& root) {
  const auto suite = root / "tree";
  const auto report = root / "report.md";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "open_4" / "tests.lgt", kTests),
                   "write tests");
  std::error_code error;
  const auto before = std::filesystem::current_path(error);
  mosa_test::check(!error, "cwd before");
  const int code = mosa::run_iso_suite(suite.string(), report.string());
  const std::string body = read_text(report);
  mosa_test::check(code == 0, "relative open exits 0");
  mosa_test::check(has_exact_line(body, "合格数: 1"), "relative open passes");
  mosa_test::check(std::filesystem::current_path(error) == before && !error, "cwd restored");
  mosa_test::check(!path_exists(before / "stray_rel"), "cwd gains no file");
  mosa_test::check(!path_exists(std::filesystem::path{MOSA_SOURCE_DIR} / "stray_rel"), "source root gains no file");
}

}  // namespace

int main() {
  const std::filesystem::path root =
      std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "cwd-fixture";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  check_relative_open(root);
  std::filesystem::remove_all(root, error);
  mosa_test::check(!path_exists(root), "fixture removed");
  return mosa_test::failures();
}
