// 計画 Task 6。mosatest --bench。同じファイルを `mosa FILE -- N`（インタプリタ）と、mosac emit + CMake の Release で
// 作った実行ファイル `<名前> N` で 5 回ずつ流し、1 回ごとの壁時計（起動から終わりまで）の中央値と比を出す。
// 両側とも同じ入口（initialization(main, main)）と同じ N で、起動と原文の読み込みの時間を同じに含める。
// 時間はこのプロセスの steady_clock で測る。statistics/2 はフェーズ7。文面は lib/messages_*.pl の鍵（ADR 0007）。
// 翻訳側は常に Release で作る（比の分子を最適化の有無で動かさない）。libmosa は mosatest の隣のビルド木のもの。

#include "mosa/bench.hpp"

#include "../codegen/codegen.hpp"
#include "mosa/exe_path.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/process.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace mosa {
namespace {

constexpr int kRuns = 5;
// 作業場所の中で消して作り直す唯一の名前。--work に ~ などを渡されても、この道具が作ったもの以外は消さない。
constexpr std::string_view kOutDir = "mosatest-bench-out";

std::string message(std::string_view key, std::vector<std::string> args = {}) {
  return render_message_fresh(default_message_language(), key, args);
}

// 途中の結果を待たせずに見せる。インタプリタ側だけで十数秒かかる。
void say(const std::string& text) { std::cout << text << std::endl; }

// mosa と mosac は mosatest の隣にある（ビルド木の形。mosac が runtime_dir を引くのと同じ前提）。拡張子は自分と同じ。
std::filesystem::path sibling(const std::filesystem::path& self, std::string_view name) {
  return self.parent_path() / path_from_utf8(std::string{name} + utf8_from_path(self.extension()));
}

struct Timed {
  std::optional<int> code;
  std::int64_t micros = 0;
};

Timed timed_run(const std::vector<std::string>& argv) {
  const auto start = std::chrono::steady_clock::now();
  const std::optional<int> code = run_process(argv, {});
  const auto end = std::chrono::steady_clock::now();
  return Timed{code, std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()};
}

// kRuns 回流して中央値（マイクロ秒）。起こせないか終了コードが 0 でなければ診断を出して nullopt。
// 終了コードは毎回見る。ヒープ上限や正しさの確かめの失敗を時間として数えない。
std::optional<std::int64_t> median_of_runs(const std::vector<std::string>& argv) {
  std::vector<std::int64_t> times;
  for (int run = 0; run < kRuns; ++run) {
    const Timed timed = timed_run(argv);
    if (!timed.code) {
      std::cerr << message("bench_cannot_start", {argv.front()}) << '\n';
      return std::nullopt;
    }
    if (*timed.code != 0) {
      std::cerr << message("bench_run_failed", {argv.front(), std::to_string(*timed.code)}) << '\n';
      return std::nullopt;
    }
    times.push_back(timed.micros);
  }
  std::sort(times.begin(), times.end());
  return times[times.size() / 2];
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

// mosac emit、configure、build。前回の目録が残ると失敗した emit が見えないので、kOutDir を消してから作る。
bool build_translated(const BenchArgs& args, const std::filesystem::path& work, const std::filesystem::path& mosac) {
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
  return step({utf8_from_path(mosac), "emit", args.file, "-o", out}, work / "emit.log", "mosac emit") &&
         step({args.cmake, "-S", out, "-B", build, "-DCMAKE_BUILD_TYPE=Release"}, work / "configure.log",
              "cmake configure") &&
         step({args.cmake, "--build", build, "--config", "Release"}, work / "build.log", "cmake build");
}

// 多構成の生成器（Visual Studio）は構成名のディレクトリへ出す。
std::optional<std::filesystem::path> built_exe(const std::filesystem::path& build, const std::filesystem::path& name) {
  for (const std::filesystem::path& candidate : {build / "Release" / name, build / name}) {
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) return candidate;
  }
  return std::nullopt;
}

std::string millis(std::int64_t micros) { return std::to_string((micros + 500) / 1000); }

// 小数 1 桁。小さな N では訳した側が 1 マイクロ秒を切りうるので、0 で割らない。
std::string ratio_text(std::int64_t interpreter, std::int64_t translated) {
  const std::int64_t divisor = std::max<std::int64_t>(translated, 1);
  const std::int64_t tenths = (interpreter * 10 + divisor / 2) / divisor;
  return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

std::optional<std::int64_t> parse_count(std::string_view text) {
  std::int64_t value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value < 1) return std::nullopt;
  return value;
}

}  // namespace

BenchArgs parse_bench_args(std::span<const std::string_view> args) {
  BenchArgs parsed;
  if (args.empty() || args.front().empty() || args.front().front() == '-') return parsed;
  parsed.file = std::string{args.front()};
  if (args.size() % 2 != 1) return parsed;
  for (std::size_t index = 1; index < args.size(); index += 2) {
    const std::string_view value = args[index + 1];
    if (value.empty()) return parsed;
    if (args[index] == "--count") {
      const auto count = parse_count(value);
      if (!count) return parsed;
      parsed.count = *count;
    } else if (args[index] == "--work") {
      parsed.work = std::string{value};
    } else if (args[index] == "--cmake") {
      parsed.cmake = std::string{value};
    } else {
      return parsed;
    }
  }
  parsed.ok = true;
  return parsed;
}

int run_bench(const BenchArgs& args) {
  const auto self = executable_path();
  if (!self) {
    std::cerr << message("bench_not_found", {"mosa"}) << '\n';
    return 1;
  }
  const std::string count = std::to_string(args.count);
  say(message("bench_header", {args.file, count, std::to_string(kRuns)}));
  const auto interpreter = median_of_runs({utf8_from_path(sibling(*self, "mosa")), args.file, "--", count});
  if (!interpreter) return 1;
  say(message("bench_interpreter", {millis(*interpreter)}));
  // 既定の作業場所は自分の隣（ビルド木）。共有の /tmp の決まった名前は、ほかの利用者が先に作って目録や
  // 実行ファイルを差し替えられるので使わない。
  const std::filesystem::path work =
      args.work.empty() ? self->parent_path() / "mosatest-bench" : path_from_utf8(args.work);
  if (!build_translated(args, work, sibling(*self, "mosac"))) return 1;
  const std::string exe_name = codegen::target_name(args.file) + utf8_from_path(self->extension());
  const auto exe = built_exe(work / kOutDir / "build", path_from_utf8(exe_name));
  if (!exe) {
    std::cerr << message("bench_not_found", {exe_name}) << '\n';
    return 1;
  }
  const auto translated = median_of_runs({utf8_from_path(*exe), count});
  if (!translated) return 1;
  say(message("bench_translated", {millis(*translated)}));
  say(message("bench_ratio", {ratio_text(*interpreter, *translated)}));
  return 0;
}

}
