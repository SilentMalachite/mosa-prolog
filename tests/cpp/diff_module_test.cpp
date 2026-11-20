// mosatest --diff は、モジュールのファイルを組み立てられないと報告する（レビュー 2026-11-20 の H-42）。
// 組み立ては試験のゴールを節 '$mosa_diff_goal'(K, ...) にしてファイルの後ろに足し、user から問い合わせる。
// :- module(m, []) の後ろに足した節は m に入るので、前は両方の側が同じ existence_error を返し、試験を流さずに
// 一致と数えた。ファイルの中で節を user に置く道は無い（SPEC 第 5 節）ので、組み立てない側に倒す。

#include "diff.hpp"
#include "harness.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

std::string fixture(const std::string& name, const std::string& text) {
  const std::string dir = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/diff_module";
  std::error_code error;
  std::filesystem::create_directories(mosa::path_from_utf8(dir), error);
  const std::string path = dir + "/" + name;
  std::ofstream file(mosa::path_from_utf8(path), std::ios::binary | std::ios::trunc);
  file << text;
  return path;
}

void check_catalogs() {
  for (const mosa::MessageLanguage language : {mosa::MessageLanguage::Ja, mosa::MessageLanguage::En}) {
    const std::string text = mosa::render_message_fresh(language, "diff_module_file", std::array<std::string, 1>{"m"});
    mosa_test::check(!text.empty() && text.find("diff_module_file") == std::string::npos && text.find('m') != std::string::npos,
                     "diff_module_file in both catalogs: " + text);
  }
}

}  // namespace

int main() {
  check_catalogs();
  const mosa::DiffSource module =
      mosa::diff_source(mosa::path_from_utf8(fixture("modfile.pl", ":- module(modfile, []).\ntest(mf_ok, true, true).\n"
                                                                 "test(mf_bad, fail, true).\n")));
  mosa_test::check(module.error.find("modfile") != std::string::npos, "a module file is not assembled: " + module.error);
  // モジュールを使うだけのファイルは、足した節が user に入るので今までどおり組み立てる。
  const mosa::DiffSource user = mosa::diff_source(mosa::path_from_utf8(
      fixture("uses.pl", ":- use_module(library(lists)).\ntest(uses_ok, append([a], [b], [a, b]), true).\n")));
  mosa_test::check(user.error.empty(), "a file that only uses a module is assembled: " + user.error);
  mosa_test::check(user.goals.size() == 1, "the one goal is assembled");
  return mosa_test::failures();
}
