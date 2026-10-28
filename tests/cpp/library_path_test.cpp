// library(...) の探索（SPEC 第5節、計画の Task 6）。MOSA_LIB の有無は ctest が 2 通りの環境で与える。
// 引数なしは MOSA_LIB を外した環境、env は MOSA_LIB=tests/tmp/library_path/lib_env の環境で動く。
#include "harness.hpp"
#include "mosa/exe_path.hpp"
#include "mosa/library_path.hpp"
#include "mosa/machine.hpp"
#include "mosa/path_text.hpp"

#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace {

const std::filesystem::path kTmp = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "library_path";

void write_file(const std::filesystem::path& path, std::string_view text) {
  std::filesystem::create_directories(path.parent_path());
  // 名前に日本語があるので、ファイルは UTF-8 から作ったパスで開く。
  std::ofstream{path, std::ios::binary} << text;
}

bool mentions(const mosa::Machine& machine, std::string_view text) {
  return std::string{machine.diagnostic()}.find(text) != std::string::npos;
}

// Atom の名前は UTF-8 である。Windows の std::string からのパスは ANSI コードページで読まれ、日本語が化ける。
void check_path_text() {
  const auto path = mosa::path_from_utf8("dir/家系.pl");
  mosa_test::check(path.filename() == std::filesystem::path{u8"家系.pl"}, "path_from_utf8 reads UTF-8");
  mosa_test::check(mosa::utf8_from_path(path) == "dir/家系.pl", "utf8_from_path writes UTF-8");
  mosa_test::check(mosa::utf8_from_path(mosa::path_from_utf8("")).empty(), "empty path");
}

void check_executable_path() {
  const auto path = mosa::executable_path();
  mosa_test::check(path.has_value(), "executable_path is found");
  mosa_test::check(path && path->stem() == "library_path_test", "executable_path names this test");
  mosa_test::check(path && std::filesystem::exists(*path), "executable_path exists");
  // POST_BUILD が lib/ を実行ファイルの隣へ写す。テストも mosa と同じディレクトリにできる。
  mosa_test::check(path && std::filesystem::exists(path->parent_path() / "lib" / "README.md"),
                   "lib/ is copied next to the executables");
}

void check_search_order() {
  const auto env_dir = kTmp / "lib_a";
  const auto exe_dir = kTmp / "exe";
  write_file(env_dir / "x.pl", ":- module(x, []).\n");
  write_file(exe_dir / "lib" / "x.pl", ":- module(x, []).\n");
  write_file(exe_dir / "lib" / "y.pl", ":- module(y, []).\n");
  mosa_test::check(mosa::find_library_in("x", env_dir, exe_dir) == env_dir / "x.pl", "library_env: MOSA_LIB first");
  mosa_test::check(mosa::find_library_in("y", env_dir, exe_dir) == exe_dir / "lib" / "y.pl",
                   "missing in MOSA_LIB falls back to the executable's lib/");
  mosa_test::check(mosa::find_library_in("y", std::nullopt, exe_dir) == exe_dir / "lib" / "y.pl",
                   "library_exe: no MOSA_LIB");
  mosa_test::check(mosa::find_library_in("x.pl", env_dir, exe_dir) == env_dir / "x.pl", "an explicit .pl is kept");
  mosa_test::check(!mosa::find_library_in("nosuch", env_dir, exe_dir), "library_missing: not found");
  mosa_test::check(!mosa::find_library_in("x", std::nullopt, std::nullopt), "nothing to search");
}

void check_library_errors() {
  auto missing = mosa::Machine::create();
  mosa_test::check(!missing->load_program(":- use_module(library(nosuch)).").ok, "library(nosuch) fails");
  mosa_test::check(mentions(*missing, "existence_error(source_sink,library(nosuch))"),
                   "existence_error(source_sink, library(nosuch))");
  auto unbound = mosa::Machine::create();
  mosa_test::check(!unbound->load_program(":- use_module(library(_)).").ok, "library(_) fails");
  mosa_test::check(mentions(*unbound, "instantiation_error"), "library(_): instantiation_error");
  auto number = mosa::Machine::create();
  mosa_test::check(!number->load_program(":- use_module(library(1)).").ok, "library(1) fails");
  mosa_test::check(mentions(*number, "type_error(atom,1)"), "library(1): type_error(atom, 1)");
}

void check_without_env() {
  mosa_test::check(!mosa::environment_path("MOSA_LIB"), "MOSA_LIB is unset");
  check_search_order();
  check_library_errors();
}

void check_with_env() {
  const auto env_dir = kTmp / "lib_env";
  // equivalent は両方が在るときだけ比べられる。先にディレクトリを作る。
  write_file(env_dir / mosa::path_from_utf8("環境.pl"), ":- module(環境, [e/0]).\ne.\n");
  const auto value = mosa::environment_path("MOSA_LIB");
  std::error_code error;
  mosa_test::check(value && std::filesystem::equivalent(*value, env_dir, error) && !error, "MOSA_LIB is read");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(":- use_module(library(環境)).").ok, "use_module(library(環境)) loads");
  mosa_test::check(machine->query("e.").kind == mosa::StopKind::Success, "library export is imported");
}

}  // namespace

int main(int argc, char** argv) {
  const std::span<char*> args{argv, static_cast<std::size_t>(argc)};
  check_path_text();
  check_executable_path();
  if (args.size() > 1 && std::string_view{args[1]} == "env") {
    check_with_env();
  } else {
    check_without_env();
  }
  return mosa_test::failures();
}
