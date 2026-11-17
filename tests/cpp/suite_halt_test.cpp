// 試験のファイルの読み込みが halt/0,1 で止まったとき、mosatest F と mosatest --diff の組み立ては理由を書いて失敗する。
// 前は読み込みの出口が halt の診断を空にする（台帳 L74）ので、mosatest F は「F: 」だけを書いて 1 で終え、--diff は
// 組み立てた扱いで先へ進み、両方の側の同じ空振りを一致として数えた（全体の Muse レビュー、2026-11-17）。
// ctest は LANG を ja_JP.UTF-8 にして流すので、英語のカタログの鍵は言語を決めて別に確かめる（Opus レビュー A1）。

#include "diff.hpp"
#include "harness.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/suite.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>

namespace {

// フォルダとファイルの名前に halt を含めない。理由の字面の halt(N) を探すので、パスに当たると確かめにならない。
std::string fixture(const std::string& name, const std::string& text) {
  const std::string dir = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/suite_stop";
  std::error_code error;
  std::filesystem::create_directories(mosa::path_from_utf8(dir), error);
  const std::string path = dir + "/" + name;
  std::ofstream file(mosa::path_from_utf8(path), std::ios::binary);
  file << text;
  return path;
}

void check_suite(const std::string& name, const std::string& text, const std::string& reason) {
  const std::string path = fixture(name, text);
  std::ostringstream captured;
  auto* saved = std::cerr.rdbuf(captured.rdbuf());
  const int code = mosa::run_suite(path);
  std::cerr.rdbuf(saved);
  const std::string shown = captured.str();
  mosa_test::check(code == 1, name + ": a stopped load fails");
  mosa_test::check(shown.find(reason) != std::string::npos, name + ": the reason is written: " + shown);
}

void check_diff(const std::string& name, const std::string& text, const std::string& reason) {
  const mosa::DiffSource source = mosa::diff_source(mosa::path_from_utf8(fixture(name, text)));
  mosa_test::check(source.error.find(reason) != std::string::npos, name + ": not assembled: " + source.error);
}

void check_catalogs() {
  const std::array<std::string, 1> code{"3"};
  for (const mosa::MessageLanguage language : {mosa::MessageLanguage::Ja, mosa::MessageLanguage::En}) {
    const std::string text = mosa::render_message_fresh(language, "suite_halted", code);
    mosa_test::check(text.find("halt(3)") != std::string::npos, "suite_halted in both catalogs: " + text);
  }
}

}  // namespace

int main() {
  check_catalogs();
  const std::string first = ":- halt(3).\ntest(a, true, true).\n";
  const std::string middle = "test(a, true, true).\n:- halt(3).\ntest(b, true, true).\n";
  const std::string bare = "test(a, true, true).\n:- halt.\n";
  // 初期化のゴールは読み込みの終わりに別の道（指示とは別の出口）で動く。
  const std::string late = ":- initialization(halt(5)).\ntest(a, true, true).\n";
  check_suite("first.pl", first, "halt(3)");
  check_suite("middle.pl", middle, "halt(3)");
  check_suite("bare.pl", bare, "halt(0)");
  check_suite("late.pl", late, "halt(5)");
  check_diff("first.pl", first, "halt(3)");
  check_diff("middle.pl", middle, "halt(3)");
  check_diff("bare.pl", bare, "halt(0)");
  check_diff("late.pl", late, "halt(5)");
  // .lgt は選んだ原文を別に読む（lgt_source）。前は読めない原文と同じ扱いで、誤りが空のまま一致と数えた（A4）。
  check_diff("stop.lgt", ":- object(tests,\n\textends(lgtunit)).\n\n\t:- halt(3).\n\n\ttest(a, true).\n\n:- end_object.\n",
             "halt(3)");
  return mosa_test::failures();
}
