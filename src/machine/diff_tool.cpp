// 計画 Task 11。mosatest --diff。mosac emit --units で試験のディレクトリの目録を出し、CMake で実行ファイル mosa_diff を
// 作って動かす（--bench と同じ段取り。bench.cpp）。比べるのは実行ファイルの中（rt::diff_main）で、ここは段取りだけ。
// 作業場所は --bench と同じ考え方で、既定は自分の隣（ビルド木）の mosatest-diff。共有の /tmp の決まった名前は、
// ほかの利用者が先に作って目録や実行ファイルを差し替えられるので使わない。

#include "mosa/diff.hpp"

#include "mosa/exe_path.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/process.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace mosa {
namespace {

// 作業場所の中で消して作り直す唯一の名前。--work に ~ などを渡されても、この道具が作ったもの以外は消さない。
constexpr std::string_view kOutDir = "mosatest-diff-out";
constexpr std::string_view kTarget = "mosa_diff";

std::string message(std::string_view key, std::vector<std::string> args = {}) {
  return render_message_fresh(default_message_language(), key, args);
}

// mosac は mosatest の隣にある（ビルド木の形）。拡張子は自分と同じ。
std::filesystem::path sibling(const std::filesystem::path& self, std::string_view name) {
  return self.parent_path() / path_from_utf8(std::string{name} + utf8_from_path(self.extension()));
}

// 出力は work の下のログに書き、失敗したときだけその場所を出す。
bool step(const std::vector<std::string>& argv, const std::filesystem::path& log, std::string_view label) {
  const std::optional<int> code = run_process(argv, log);
  if (!code) {
    std::cerr << message("bench_cannot_start", {argv.front()}) << '\n';
    return false;
  }
  if (*code != 0) {
    std::cerr << message("bench_step_failed", {std::string{label}, std::to_string(*code), utf8_from_path(log)}) << '\n';
    return false;
  }
  return true;
}

// emit、configure、build。前回の目録が残ると失敗した emit が見えないので、kOutDir を消してから作る。
bool build_diff(const DiffArgs& args, const std::filesystem::path& work, const std::filesystem::path& mosac) {
  std::error_code error;
  std::filesystem::remove_all(work / kOutDir, error);
  error.clear();
  std::filesystem::create_directories(work, error);
  if (error) {
    std::cerr << message("bench_cannot_write", {utf8_from_path(work)}) << '\n';
    return false;
  }
  const std::string out = utf8_from_path(work / kOutDir);
  const std::string build = utf8_from_path(work / kOutDir / "build");
  return step({utf8_from_path(mosac), "emit", "--units", args.dir, "-o", out}, work / "emit.log", "mosac emit") &&
         step({args.cmake, "-S", out, "-B", build, "-DCMAKE_BUILD_TYPE=" + args.config}, work / "configure.log",
              "cmake configure") &&
         step({args.cmake, "--build", build, "--config", args.config}, work / "build.log", "cmake build");
}

// 多構成の生成器（Visual Studio）は構成名のディレクトリへ出す。
std::optional<std::filesystem::path> built_exe(const std::filesystem::path& build, std::string_view config,
                                               const std::filesystem::path& name) {
  for (const std::filesystem::path& candidate : {build / path_from_utf8(config) / name, build / name}) {
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) return candidate;
  }
  return std::nullopt;
}

// 報告先を省けるのは、リポジトリ直下から同梱の試験のディレクトリを渡したときだけ（--suite と同じ考え方）。
bool bundled_dir(std::string_view dir) {
  constexpr std::array<std::string_view, 2> bundled{"tests/pl", "tests/conformance"};
  for (const std::string_view candidate : bundled) {
    std::error_code error;
    if (std::filesystem::equivalent(path_from_utf8(dir), path_from_utf8(candidate), error) && !error) return true;
  }
  return false;
}

}  // namespace

DiffArgs parse_diff_args(std::span<const std::string_view> args) {
  DiffArgs parsed;
  if (args.empty() || args.front().empty() || args.front().front() == '-') return parsed;
  parsed.dir = std::string{args.front()};
  if (args.size() % 2 != 1) return parsed;
  for (std::size_t index = 1; index < args.size(); index += 2) {
    const std::string value{args[index + 1]};
    if (value.empty()) return parsed;
    if (args[index] == "--report") parsed.report = value;
    else if (args[index] == "--work") parsed.work = value;
    else if (args[index] == "--cmake") parsed.cmake = value;
    else if (args[index] == "--config") parsed.config = value;
    else return parsed;
  }
  if (parsed.report.empty() && bundled_dir(parsed.dir)) parsed.report = "docs/agreement.json";
  parsed.ok = true;
  return parsed;
}

int run_diff(const DiffArgs& args) {
  const auto self = executable_path();
  if (!self) {
    std::cerr << message("bench_not_found", {"mosac"}) << '\n';
    return 1;
  }
  const std::filesystem::path work =
      args.work.empty() ? self->parent_path() / "mosatest-diff" : path_from_utf8(args.work);
  if (!build_diff(args, work, sibling(*self, "mosac"))) return 1;
  const std::string exe_name = std::string{kTarget} + utf8_from_path(self->extension());
  const auto exe = built_exe(work / kOutDir / "build", args.config, path_from_utf8(exe_name));
  if (!exe) {
    std::cerr << message("bench_not_found", {exe_name}) << '\n';
    return 1;
  }
  std::vector<std::string> argv{utf8_from_path(*exe), args.dir};
  if (!args.report.empty()) {
    argv.push_back("--report");
    argv.push_back(args.report);
  }
  // 結果は実行ファイルが標準出力に出す。ログに取らず、そのまま見せる。
  const std::optional<int> code = run_process(argv, {});
  if (!code) {
    std::cerr << message("bench_cannot_start", {argv.front()}) << '\n';
    return 1;
  }
  return *code;
}

}  // namespace mosa
