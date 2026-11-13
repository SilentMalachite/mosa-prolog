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

// 1 つのフォルダに合格 passed 件と不合格 failed 件を置き、終了コードを返す。
int run_code(const std::filesystem::path& root, std::string_view name, int passed, int failed) {
  const auto suite = root / name;
  std::string body;
  for (int index = 0; index < passed; ++index) body += "test(ok" + std::to_string(index) + ", true) :- {true}.\n";
  for (int index = 0; index < failed; ++index) body += "test(ng" + std::to_string(index) + ", true) :- {fail}.\n";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_length_2" / "tests.lgt", body),
                   "write tests");
  const auto report = root / (std::string{name} + ".md");
  return mosa::run_iso_suite(suite.string(), report.string());
}

std::string run_with(const std::filesystem::path& root, std::string_view name, int passed, int failed) {
  (void)run_code(root, name, passed, failed);
  return read_text(root / (std::string{name} + ".json"));
}

void check_colors(const std::filesystem::path& root) {
  mosa_test::check(run_with(root, "all", 2, 0) == badge("100.0% (2/2)", "brightgreen"), "all pass is green");
  mosa_test::check(run_with(root, "ninety", 9, 1) == badge("90.0% (9/10)", "brightgreen"), "90 is green");
  mosa_test::check(run_with(root, "quarter", 3, 1) == badge("75.0% (3/4)", "yellow"), "75 is yellow");
  mosa_test::check(run_with(root, "half", 1, 1) == badge("50.0% (1/2)", "red"), "50 is red");
  mosa_test::check(run_with(root, "none", 0, 0) == badge("no tests", "lightgrey"), "zero denominator");
}

// 合格率の後退は終了コード 1（全体レビュー推奨3、SPEC 第8節「合格率を下げる変更はマージしない」）。
// 前回の JSON が物差しで、90% の下限とは別に比べる。後退した回は JSON を書き換えず、物差しを残す。
void check_regression(const std::filesystem::path& root) {
  mosa_test::check(run_code(root, "gate", 19, 1) == 0, "first run has no baseline");
  mosa_test::check(read_text(root / "gate.json") == badge("95.0% (19/20)", "brightgreen"), "baseline written");
  mosa_test::check(run_code(root, "gate", 18, 2) == 1, "90 percent still fails as a regression");
  mosa_test::check(read_text(root / "gate.json") == badge("95.0% (19/20)", "brightgreen"), "baseline kept");
  mosa_test::check(run_code(root, "gate", 18, 2) == 1, "regression keeps failing");
  mosa_test::check(run_code(root, "gate", 20, 0) == 0, "improvement passes");
  mosa_test::check(read_text(root / "gate.json") == badge("100.0% (20/20)", "brightgreen"), "baseline raised");
  mosa_test::check(run_code(root, "gate", 10, 0) == 0, "equal rate passes");
  mosa_test::check(read_text(root / "gate.json") == badge("100.0% (10/10)", "brightgreen"), "equal rate rewrites");
  mosa_test::check(write_text(root / "broken.json", "not a badge\n"), "seed broken badge");
  mosa_test::check(run_code(root, "broken", 9, 1) == 0, "unreadable baseline only keeps the 90 floor");
  mosa_test::check(read_text(root / "broken.json") == badge("90.0% (9/10)", "brightgreen"), "broken badge replaced");
}

// 報告書は生成物でコミットする。^^file_path の絶対パスが不合格行へそのまま出ると、
// 機械ごとに差分が出て再現しない（全体レビュー推奨3）。書く前に相対へ直す。
void check_report_paths(const std::filesystem::path& root) {
  const auto suite = root / "paths";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_length_2" / "tests.lgt",
                              "test(pathy, true) :- ^^file_path(missing, Path), open(Path, read, _).\n"),
                   "write path tests");
  const auto report = root / "paths.md";
  (void)mosa::run_iso_suite(suite.string(), report.string());
  const std::string text = read_text(report);
  mosa_test::check(text.find("existence_error(source_sink,") != std::string::npos, "path failure recorded");
  mosa_test::check(text.find(std::string{MOSA_SOURCE_DIR}) == std::string::npos, "report has no absolute path");
  mosa_test::check(text.find("tests/tmp/suite/predicates/atom_length_2/missing") != std::string::npos,
                   "report path is relative");
}

// 全体レビュー推奨3。halt は期待の形を問わず合格にしない。前は StopKind::Halt なら
// succeeded が無条件に true で、false や error 期待でも偽合格した。halt の後も次の件は流す。
void check_halt_rows(const std::filesystem::path& root) {
  const auto suite = root / "halt";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_length_2" / "tests.lgt",
                              "test(halt_true, true) :- {halt}.\n"
                              "test(halt_false, false) :- {halt(3)}.\n"
                              "test(after_halt, true) :- {true}.\n"),
                   "write halt tests");
  const auto report = root / "halt.md";
  mosa_test::check(mosa::run_iso_suite(suite.string(), report.string()) == 1, "halt rows fail the suite");
  const std::string text = read_text(report);
  mosa_test::check(text.find("halt_true true halt(0)") != std::string::npos, "halt under true fails: " + text);
  mosa_test::check(text.find("halt_false false halt(3)") != std::string::npos, "halt under false fails: " + text);
  mosa_test::check(text.find("合格数: 1") != std::string::npos, "the row after halt still runs: " + text);
}

void check_absent(const std::filesystem::path& root) {
  const auto dir = root / "absent";
  std::error_code error;
  std::filesystem::create_directories(dir, error);
  mosa_test::check(!error, "absent dir");
  mosa_test::check(mosa::run_iso_suite(dir.string(), (root / "absent.md").string()) == 1, "absent returns 1");
  mosa_test::check(!path_exists(root / "absent.json"), "absent writes no badge");
}

// Muse のレビューの中。報告書を書けない場所なら、合格率が 90 以上でも 1 で終える。前は黙って 0 で終えた。
void check_unwritable(const std::filesystem::path& root) {
  const auto suite = root / "unwritable";
  mosa_test::check(write_text(suite / "logtalk" / "prolog" / "predicates" / "atom_length_2" / "tests.lgt",
                              "test(ok, true) :- {true}.\n"),
                   "write unwritable tests");
  const auto report = root / "no-such-folder" / "unwritable.md";
  mosa_test::check(mosa::run_iso_suite(suite.string(), report.string()) == 1, "unwritable report returns 1");
}

}  // namespace

int main() {
  const std::filesystem::path root =
      std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "badge-fixture";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  check_colors(root);
  check_regression(root);
  check_report_paths(root);
  check_halt_rows(root);
  check_absent(root);
  check_unwritable(root);
  std::filesystem::remove_all(root, error);
  mosa_test::check(!path_exists(root), "fixture removed");
  return mosa_test::failures();
}
