// 準拠率バッジの JSON。報告書と同じ名前で拡張子だけ .json にし、shields.io の endpoint 形式で書く。
// README のバッジがリポジトリの JSON を読むので、内容は 1 行に固定して差分を小さくする。

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

std::string badge(std::string_view message, std::string_view color) {
  return std::string{R"({"schemaVersion":1,"label":"ISO conformance","message":")"} + std::string{message} +
         R"(","color":")" + std::string{color} + "\"}\n";
}

// 1 つのフォルダに合格 passed 件と不合格 failed 件を置き、出来た JSON を返す。
std::string run_with(const std::filesystem::path& root, std::string_view name, int passed, int failed) {
  const auto suite = root / name;
  std::string body;
  for (int index = 0; index < passed; ++index) body += "test(ok" + std::to_string(index) + ", true) :- {true}.\n";
  for (int index = 0; index < failed; ++index) body += "test(ng" + std::to_string(index) + ", true) :- {fail}.\n";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_length_2" / "tests.lgt", body),
                   "write tests");
  const auto report = root / (std::string{name} + ".md");
  (void)mosa::run_iso_suite(suite.string(), report.string());
  return read_text(root / (std::string{name} + ".json"));
}

void check_colors(const std::filesystem::path& root) {
  mosa_test::check(run_with(root, "all", 2, 0) == badge("100.0% (2/2)", "brightgreen"), "all pass is green");
  mosa_test::check(run_with(root, "ninety", 9, 1) == badge("90.0% (9/10)", "brightgreen"), "90 is green");
  mosa_test::check(run_with(root, "quarter", 3, 1) == badge("75.0% (3/4)", "yellow"), "75 is yellow");
  mosa_test::check(run_with(root, "half", 1, 1) == badge("50.0% (1/2)", "red"), "50 is red");
  mosa_test::check(run_with(root, "none", 0, 0) == badge("no tests", "lightgrey"), "zero denominator");
}

void check_absent(const std::filesystem::path& root) {
  const auto dir = root / "absent";
  std::error_code error;
  std::filesystem::create_directories(dir, error);
  mosa_test::check(!error, "absent dir");
  mosa_test::check(mosa::run_iso_suite(dir.string(), (root / "absent.md").string()) == 1, "absent returns 1");
  mosa_test::check(!path_exists(root / "absent.json"), "absent writes no badge");
}

}  // namespace

int main() {
  const std::filesystem::path root =
      std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "badge-fixture";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  check_colors(root);
  check_absent(root);
  std::filesystem::remove_all(root, error);
  mosa_test::check(!path_exists(root), "fixture removed");
  return mosa_test::failures();
}
