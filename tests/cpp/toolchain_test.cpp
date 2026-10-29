// ツールチェーンの検出（SPEC 第6節、計画 Task 8）。偽のコンパイラは tests/tmp/toolchain の空ファイルで、
// 探索は find_toolchain_in に PATH と環境を引数で渡して引く。起動しないので、版と Windows の環境はここでは見ない。
#include "harness.hpp"
#include "mosa/path_text.hpp"
#include "mosa/toolchain.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

const std::filesystem::path kTmp = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "toolchain";

// 前の実行の残りが答えを変えないよう、毎回空にしてから作る。
std::filesystem::path fresh_dir(std::string_view name) {
  const std::filesystem::path dir = kTmp / std::filesystem::path{name};
  std::error_code error;
  std::filesystem::remove_all(dir, error);
  std::filesystem::create_directories(dir);
  return dir;
}

void touch(const std::filesystem::path& dir, std::string_view name) {
  std::ofstream{dir / std::filesystem::path{name}, std::ios::binary} << "";
}

bool picks(const mosa::ToolchainSearch& search, const std::filesystem::path& expected) {
  const auto found = mosa::find_toolchain_in(search);
  return found && found->cxx == expected;
}

void cxx_flag_wins() {
  const auto bin = fresh_dir("bin_flag");
  for (const std::string_view name : {"my-c++", "mosa-c++", "env-c++", "g++", "clang++"}) touch(bin, name);
  mosa::ToolchainSearch search;
  search.cxx_flag = "my-c++";
  search.mosa_cxx = "mosa-c++";
  search.cxx_env = "env-c++";
  search.path = {bin};
  search.os = mosa::HostOs::linux;
  mosa_test::check(picks(search, bin / "my-c++"), "cxx_flag_wins: --cxx before MOSA_CXX and CXX");
  search.cxx_flag = mosa::utf8_from_path(bin / "my-c++");
  search.path = {};
  mosa_test::check(picks(search, bin / "my-c++"), "cxx_flag_wins: a path is used without PATH");
  search.cxx_flag = "no-such-c++";
  search.path = {bin};
  mosa_test::check(!mosa::find_toolchain_in(search), "cxx_flag_wins: a missing --cxx does not fall through");
  // 見つからないときの文面は、名指しされたものとその出どころを named_compiler から取る。
  const auto named = mosa::named_compiler(search);
  mosa_test::check(named && named->program == "no-such-c++" && named->source == "--cxx", "cxx_flag_wins: named --cxx");
  search.cxx_flag.reset();
  mosa_test::check(mosa::named_compiler(search) && mosa::named_compiler(search)->source == "MOSA_CXX",
                   "cxx_flag_wins: named MOSA_CXX next");
  search.mosa_cxx.reset();
  mosa_test::check(mosa::named_compiler(search) && mosa::named_compiler(search)->program == "env-c++" &&
                       mosa::named_compiler(search)->source == "CXX",
                   "cxx_flag_wins: named CXX last");
  search.cxx_env.reset();
  mosa_test::check(!mosa::named_compiler(search), "cxx_flag_wins: nothing named");
}

void mosa_cxx_before_cxx() {
  const auto bin = fresh_dir("bin_env");
  for (const std::string_view name : {"mosa-c++", "env-c++", "g++"}) touch(bin, name);
  mosa::ToolchainSearch search;
  search.mosa_cxx = "mosa-c++";
  search.cxx_env = "env-c++";
  search.path = {bin};
  search.os = mosa::HostOs::linux;
  mosa_test::check(picks(search, bin / "mosa-c++"), "mosa_cxx_before_cxx: MOSA_CXX before CXX");
  search.mosa_cxx.reset();
  mosa_test::check(picks(search, bin / "env-c++"), "mosa_cxx_before_cxx: CXX before the OS default");
  search.cxx_env = "no-such-c++";
  mosa_test::check(!mosa::find_toolchain_in(search), "mosa_cxx_before_cxx: a missing CXX does not fall through");
}

void default_order_linux() {
  // clang++ が PATH の前にあっても、Linux は名前の順（g++ が先）で選ぶ。
  const auto first = fresh_dir("bin_clang");
  const auto second = fresh_dir("bin_gcc");
  touch(first, "clang++");
  touch(first, "cmake");
  touch(second, "g++");
  mosa::ToolchainSearch search;
  search.path = {first, second};
  search.os = mosa::HostOs::linux;
  search.runtime_dir = kTmp / "runtime";
  const auto found = mosa::find_toolchain_in(search);
  mosa_test::check(found && found->cxx == second / "g++", "default_order_linux: g++ before clang++");
  mosa_test::check(found && found->cmake == first / "cmake", "default_order_linux: cmake from PATH");
  mosa_test::check(found && found->runtime_dir == kTmp / "runtime", "default_order_linux: runtime_dir is kept");
  mosa_test::check(found && found->env.empty() && found->cxx_version.empty(), "default_order_linux: nothing run");
  search.path = {first};
  mosa_test::check(picks(search, first / "clang++"), "default_order_linux: clang++ without g++");
  search.os = mosa::HostOs::macos;
  search.path = {second, first};
  mosa_test::check(picks(search, first / "clang++"), "default_order_linux: macOS takes clang++ only");
  search.path = {second};
  mosa_test::check(!mosa::find_toolchain_in(search), "default_order_linux: macOS ignores g++");
  const auto msvc = fresh_dir("bin_msvc");
  touch(msvc, "cl.exe");
  touch(msvc, "cmake.exe");
  search.os = mosa::HostOs::windows;
  search.path = {first, msvc};
  const auto windows = mosa::find_toolchain_in(search);
  mosa_test::check(windows && windows->cxx == msvc / "cl.exe", "default_order_linux: Windows takes cl.exe");
  mosa_test::check(windows && windows->cmake == msvc / "cmake.exe", "default_order_linux: Windows cmake.exe");
}

void nothing_found() {
  const auto empty = fresh_dir("bin_empty");
  touch(empty, "README.md");
  for (const mosa::HostOs os : {mosa::HostOs::macos, mosa::HostOs::linux, mosa::HostOs::windows}) {
    mosa::ToolchainSearch search;
    search.os = os;
    mosa_test::check(!mosa::find_toolchain_in(search), "nothing_found: no PATH");
    search.path = {empty, kTmp / "no_such_dir"};
    mosa_test::check(!mosa::find_toolchain_in(search), "nothing_found: empty and missing directories");
  }
  // CMake は任意。コンパイラだけがあれば見つかったことにする。
  const auto only_cxx = fresh_dir("bin_only_cxx");
  touch(only_cxx, "g++");
  mosa::ToolchainSearch search;
  search.path = {only_cxx};
  const auto found = mosa::find_toolchain_in(search);
  mosa_test::check(found && !found->cmake, "nothing_found: CMake may be absent");
}

// Windows の道の文字列処理。ここでは起動せず、vswhere と set の出力の形だけを確かめる。
void windows_text() {
  const auto dirs =
      mosa::split_path_list("C:\\a;;C:\\Program Files\\b;\"C:\\Program Files\\c\";", mosa::HostOs::windows);
  mosa_test::check(dirs.size() == 3 && dirs[1] == std::filesystem::path{"C:\\Program Files\\b"}, "split ; on Windows");
  mosa_test::check(dirs.size() == 3 && dirs[2] == std::filesystem::path{"C:\\Program Files\\c"},
                   "split strips the quotes around a Windows entry");
  const auto posix = mosa::split_path_list("/usr/bin::/bin", mosa::HostOs::linux);
  mosa_test::check(posix.size() == 2 && posix[0] == std::filesystem::path{"/usr/bin"}, "split : elsewhere");
  const auto install = mosa::parse_vswhere_output("\r\nC:\\VS\\2022\\BuildTools\r\nC:\\VS\\2019\r\n");
  mosa_test::check(install && *install == std::filesystem::path{"C:\\VS\\2022\\BuildTools"}, "vswhere first line");
  mosa_test::check(!mosa::parse_vswhere_output("\r\n  \r\n"), "vswhere found nothing");
  const auto env = mosa::parse_set_dump("=C:=C:\\work\r\nINCLUDE=C:\\a;C:\\b\r\nPath=C:\\x=y\r\nnoequals\r\n");
  mosa_test::check(env.size() == 2, "set dump skips =C: and lines without =");
  mosa_test::check(env.size() == 2 && env[0] == std::pair<std::string, std::string>{"INCLUDE", "C:\\a;C:\\b"},
                   "set dump NAME=VALUE");
  mosa_test::check(env.size() == 2 && env[1].second == "C:\\x=y", "set dump splits at the first =");
  // cmd に渡すパスは OS の区切りのまま（Windows では \）。utf8_from_path は / にそろえる。
  const std::filesystem::path script = std::filesystem::path{"dir"} / "x.bat";
  const std::string expected = mosa::host_os() == mosa::HostOs::windows ? "dir\\x.bat" : "dir/x.bat";
  mosa_test::check(mosa::native_path_text(script) == expected, "native separators for cmd");
  mosa_test::check(mosa::utf8_from_path(script) == "dir/x.bat", "utf8_from_path keeps /");
  // cl の見出しは、ファイルに向けた出力では使い方の行の後ろに来る。
  const std::string_view cl_output =
      "usage: cl [ option... ] filename... [ /link linkoption... ]\r\n"
      "Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35229 for x64\r\n"
      "Copyright (C) Microsoft Corporation.  All rights reserved.\r\n";
  mosa_test::check(mosa::msvc_banner_line(cl_output) == "Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35229 for x64",
                   "cl banner after the usage line");
  mosa_test::check(mosa::msvc_banner_line("\r\nsomething else\r\n") == "something else",
                   "cl banner falls back to the first non-empty line");
}

// 名指しの cl の案内（持ち主の決定 A）と、vcvarsall の .bat、cmd /u の UTF-16LE の読み方。
void windows_msvc_env() {
  mosa_test::check(mosa::is_msvc("cl") && mosa::is_msvc("C:/VS/bin/CL.EXE") && !mosa::is_msvc("clang-cl"), "is_msvc");
  mosa_test::check(mosa::needs_developer_prompt("cl", false, false), "a named cl without INCLUDE needs the prompt");
  mosa_test::check(!mosa::needs_developer_prompt("cl", true, false), "the vcvarsall environment is enough");
  mosa_test::check(!mosa::needs_developer_prompt("cl.exe", false, true), "a Developer Command Prompt sets INCLUDE");
  mosa_test::check(!mosa::needs_developer_prompt("g++", false, false), "other compilers never need it");
  // TMP=C:/Temp のように / で書いたパスも、cmd には OS の区切りで渡す（POSIX では / のまま）。
  const std::filesystem::path slashed = mosa::path_from_utf8("dir/x.bat");
  const std::string native = mosa::host_os() == mosa::HostOs::windows ? "dir\\x.bat" : "dir/x.bat";
  mosa_test::check(mosa::native_path_text(slashed) == native, "native separators for a path written with /");
  const std::string script = mosa::vcvars_script(mosa::path_from_utf8("vc/vcvarsall.bat"));
  const std::string call = mosa::host_os() == mosa::HostOs::windows ? "vc\\vcvarsall.bat" : "vc/vcvarsall.bat";
  mosa_test::check(script == "@echo off\r\ncall \"" + call + "\" x64 >nul\r\nif errorlevel 1 exit /b 1\r\nset\r\n",
                   "vcvarsall script text");
  // (x86) の & ( ) は引用の中で文字のまま渡るので通し、cmd が展開する % と引用を閉じる " だけを拒む。
  const auto x86 = mosa::path_from_utf8("C:/Program Files (x86)/Microsoft Visual Studio/A&B/vcvarsall.bat");
  mosa_test::check(mosa::cmd_quotable_path(x86), "a vcvarsall path with ( ) & can be quoted for cmd");
  mosa_test::check(!mosa::cmd_quotable_path(mosa::path_from_utf8("C:/VS/%USERNAME%/vcvarsall.bat")),
                   "a vcvarsall path with % is refused");
  mosa_test::check(!mosa::cmd_quotable_path(mosa::path_from_utf8("C:/VS/a\"b/vcvarsall.bat")),
                   "a vcvarsall path with a double quote is refused");
  // 「TEMP=C:\テスト」と 𠀋（サロゲート対）の UTF-16LE。
  const std::string utf16{"T\0=\0C\0:\0\\\0\xc6\x30\xb9\x30\xc8\x30\x40\xd8\x0b\xdc\r\0\n\0", 24};
  mosa_test::check(mosa::utf8_from_utf16le(utf16) == "T=C:\\テスト\xf0\xa0\x80\x8b\r\n", "UTF-16LE to UTF-8");
  mosa_test::check(mosa::utf8_from_utf16le(std::string{"\x00\xd8" "A\0", 4}) == "\xef\xbf\xbd" "A", "lone surrogate");
  mosa_test::check(mosa::utf8_from_utf16le(std::string{"A\0B", 3}) == "A", "odd trailing byte");
  const auto env = mosa::parse_set_dump(mosa::utf8_from_utf16le(utf16));
  mosa_test::check(env.size() == 1 && env[0].second == "C:\\テスト\xf0\xa0\x80\x8b", "set dump from cmd /u");
}

}  // namespace

int main() {
  cxx_flag_wins();
  mosa_cxx_before_cxx();
  default_order_linux();
  nothing_found();
  windows_text();
  windows_msvc_env();
  return mosa_test::failures();
}
