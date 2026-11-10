// Muse のレビューの中。docs/agreement.json の既存の中身が壊れていても、write_agreement は落ちずに捨てて書き直す。
// 前は std::stoll が "-" や 20 桁の数で例外を投げ、負の数は std::size_t で巨大な件数になった。
// 一時ファイルは tests/tmp/agreement-json。

#include "harness.hpp"

#include "machine/diff.hpp"

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
  std::filesystem::remove_all(root, error);
  return mosa_test::failures();
}
