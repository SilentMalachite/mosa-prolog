// Muse のレビューの中。docs/agreement.json の既存の中身が壊れていても、write_agreement は落ちずに捨てて書き直す。
// 前は std::stoll が "-" や 20 桁の数で例外を投げ、負の数は std::size_t で巨大な件数になった。
// 一時ファイルは tests/tmp/agreement-json。

#include "harness.hpp"

#include "machine/diff.hpp"
#include "mosa/diff.hpp"
#include "mosa/path_text.hpp"

#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

void put(const std::filesystem::path& path, std::string_view text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void check_broken(const std::filesystem::path& json, std::string_view label, std::string_view old_text) {
  put(json, old_text);
  mosa::DiffStats stats;
  stats.files = 3;
  const bool written = mosa::write_agreement(json, "tests/pl", stats);
  const std::string text = read_text(json);
  mosa_test::check(written, std::string{label} + ": written");
  mosa_test::check(text.find("\"tests/pl\"") != std::string::npos, std::string{label} + ": new label, got " + text);
  mosa_test::check(text.find("\"x\"") == std::string::npos, std::string{label} + ": broken section dropped, got " + text);
}

// レビュー L-40。欄の名前は、日本語のディレクトリでもどの OS でも UTF-8 の字面になる。Windows の generic_string は
// ANSI コードページへ写すので、前は日本語の名前を写せずに投げるか、別の字に替えた。
void check_label() {
  std::string label;
  try {
    label = mosa::diff_label(mosa::path_from_utf8("tests/日本語/"));
  } catch (const std::exception& error) {
    label = std::string{"threw: "} + error.what();
  }
  mosa_test::check(label == "tests/日本語", "UTF-8 label, got " + label);
}

// レビュー L-39。欄の名前の制御文字は逃がして書き、読むときに戻す。前は " と \ だけを逃がしたので、タブの入った
// ディレクトリの名前で JSON が読めず、読み手も \t を t と取った。同じ名前で書き直すと 1 つの欄を置き換える。
void check_control_label(const std::filesystem::path& json) {
  std::error_code error;
  std::filesystem::remove(json, error);
  mosa::DiffStats stats;
  stats.files = 1;
  mosa_test::check(mosa::write_agreement(json, "dir\ttab\n\x01", stats), "control label written");
  stats.files = 2;
  mosa_test::check(mosa::write_agreement(json, "dir\ttab\n\x01", stats), "control label rewritten");
  mosa_test::check(mosa::write_agreement(json, "tests/pl", stats), "second label written");
  const std::string text = read_text(json);
  mosa_test::check(text.find("\"dir\\ttab\\n\\u0001\":{\"files\":2,") != std::string::npos,
                   "control label escaped and replaced, got " + text);
  mosa_test::check(text.find("\"files\":1,") == std::string::npos, "old section replaced, got " + text);
  mosa_test::check(text.find('\t') == std::string::npos && text.find('\x01') == std::string::npos, "no raw control");
  mosa_test::check(text.find("\"tests/pl\"") != std::string::npos, "other label kept");
}

// レビュー L-39。--report が .md なら、隣に書く .md と同じファイルになるので、作る前に断る。work も作らない。
void check_report_clash(const std::filesystem::path& root) {
  const std::filesystem::path dir = root / "clash_dir";
  std::filesystem::create_directories(dir);
  put(dir / "a.pl", "test(a, true, true).\n");
  for (const char* name : {"clash.md", "clash.MD"}) {
    mosa::DiffArgs args;
    args.dir = mosa::utf8_from_path(dir);
    args.report = mosa::utf8_from_path(root / name);
    args.work = mosa::utf8_from_path(root / "clash_work");
    args.cmake = mosa::utf8_from_path(root / "no-such-cmake");
    args.ok = true;
    mosa_test::check(mosa::run_diff(args) == 1, std::string{name} + ": refused");
    std::error_code error;
    mosa_test::check(!std::filesystem::exists(root / "clash_work", error), std::string{name} + ": nothing built");
    mosa_test::check(!std::filesystem::exists(root / name, error), std::string{name} + ": no report");
  }
}

}  // namespace

int main() {
  const std::filesystem::path root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "agreement-json";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  mosa_test::check(!error, "fixture dir");
  const auto json = root / "agreement.json";
  check_broken(json, "dash only", R"({"directories":{"x":{"files":-}}})");
  check_broken(json, "too many digits", R"({"directories":{"x":{"files":123456789012345678901}}})");
  check_broken(json, "negative", R"({"directories":{"x":{"files":-1}}})");
  check_label();
  check_control_label(root / "control.json");
  check_report_clash(root);
  std::filesystem::remove_all(root, error);
  return mosa_test::failures();
}
