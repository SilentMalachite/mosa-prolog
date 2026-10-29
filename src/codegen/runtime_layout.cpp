// 実行時ライブラリの場所（SPEC 第6節、計画 Task 10）。mosac は自分の実行ファイルの場所から 2 つの形を見る。
//  - ビルド木の形: mosac の隣に libmosa、include/、pl/ がある（本体の CMakeLists.txt の mosa_lib_dir と mosa_pl_units）
//  - インストールの形: <prefix>/bin/mosac から <prefix>/lib/mosa/<OS>-<arch>-<compiler>/ を引く
// 鍵 <OS>-<arch>-<compiler> は見つけたコンパイラの族（clang、gcc、msvc）で作る。mosac を作ったコンパイラと違う族を
// --cxx で指せば、その族のディレクトリが無いので ABI の不一致として止め、--runtime-source を案内する。
// --runtime-source は同梱の源（<prefix>/lib/mosa/source/、ビルド木ならリポジトリの根）を CMake で作り、
// .mosac-cache/runtime/<hash>/prefix/ に install して、そこをインストールの形として使う。鍵はコンパイラと源の中身。
// テスト: tests/mosac/installed_layout_test.cmake（インストールの形）、tests/mosac/build_test.cmake（ビルド木の形）。
// --runtime-source は手で確かめる（libmosa を作るのに数分かかる）。

#include "build.hpp"

#include "mosa/path_text.hpp"
#include "mosa/process.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

namespace mosa::codegen {
namespace {

std::string_view os_name(HostOs os) {
  switch (os) {
    case HostOs::macos:
      return "macos";
    case HostOs::linux:
      return "linux";
    case HostOs::windows:
      return "windows";
  }
  return "unknown";
}

// コンパイラの族。cl は msvc、`--version` の 1 行目に clang があれば clang（Apple clang、clang-cl も）、ほかは gcc。
std::string compiler_family(const Toolchain& toolchain) {
  if (is_msvc(toolchain.cxx)) return "msvc";
  std::string version = toolchain.cxx_version;
  for (char& unit : version) unit = (unit >= 'A' && unit <= 'Z') ? static_cast<char>(unit - 'A' + 'a') : unit;
  return version.find("clang") != std::string::npos ? "clang" : "gcc";
}

bool is_file(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) && !error;
}

bool is_dir(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_directory(path, error) && !error;
}

// root に libmosa と include/mosa/runtime.hpp があればその形。pl/ は結ぶときに個別に確かめる（無いライブラリの名前を出す）。
std::optional<RuntimeLayout> layout_at(const std::filesystem::path& root, const Toolchain& toolchain) {
  const std::filesystem::path library = is_msvc(toolchain.cxx) ? "mosa.lib" : "libmosa.a";
  RuntimeLayout layout{root, root / "include", root / library, root / "pl"};
  if (!is_file(layout.library) || !is_file(layout.include_dir / "mosa" / "runtime.hpp")) return std::nullopt;
  return layout;
}

// 同梱の源。インストールの形では <prefix>/lib/mosa/source/、ビルド木の形では mosac の親（リポジトリの根）。
std::optional<std::filesystem::path> bundled_source_dir(const Toolchain& toolchain) {
  const std::filesystem::path self = toolchain.runtime_dir;
  for (const std::filesystem::path& candidate : {self.parent_path() / "lib" / "mosa" / "source", self.parent_path()}) {
    if (is_file(candidate / "CMakeLists.txt") && is_file(candidate / "include" / "mosa" / "runtime.hpp") &&
        is_dir(candidate / "src" / "machine") && is_dir(candidate / "lib")) {
      return candidate;
    }
  }
  return std::nullopt;
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream in{path, std::ios::binary};
  return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

struct RuntimeJob {
  const BuildRequest& request;
  const Toolchain& toolchain;
  std::filesystem::path dir;
};

bool run_runtime_step(const RuntimeJob& job, const std::vector<std::string>& argv, std::string_view log_name) {
  if (job.request.verbose) std::cerr << mosac_message("mosac_verbose_run", {argv.front() + " ..."}) << '\n';
  const std::filesystem::path log = job.dir / "logs" / path_from_utf8(std::string{log_name} + ".log");
  const std::optional<int> code = run_process(argv, log, job.toolchain.env);
  if (!code) {
    std::cerr << mosac_message("mosac_cannot_start", {argv.front()}) << '\n';
    return false;
  }
  if (*code == 0) return true;
  if (job.request.verbose) std::cerr << read_text(log);
  std::cerr << mosac_message("mosac_runtime_build_failed", {utf8_from_path(log)}) << '\n';
  return false;
}

// configure、build、install の 3 段。構成は Release に固め、テストは作らない（MOSA_BUILD_TESTS=OFF）。
// 入れるのは runtime の部品（libmosa、include/、pl/）だけで、bin/ の 3 コマンドは要らない。
bool build_runtime(const RuntimeJob& job, const std::filesystem::path& source, const std::filesystem::path& prefix) {
  const std::string cmake = utf8_from_path(*job.toolchain.cmake);
  const std::string build = utf8_from_path(job.dir / "build");
  const bool msvc = is_msvc(job.toolchain.cxx);
  std::vector<std::string> configure{cmake, "-S", utf8_from_path(source), "-B", build, "-DCMAKE_BUILD_TYPE=Release",
                                     "-DCMAKE_CXX_COMPILER=" + utf8_from_path(job.toolchain.cxx),
                                     "-DMOSA_BUILD_TESTS=OFF", "-DCMAKE_CXX_FLAGS="};
  if (msvc) configure.insert(configure.end(), {"-G", "NMake Makefiles", "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"});
  if (!run_runtime_step(job, configure, "configure")) return false;
  if (!run_runtime_step(job, {cmake, "--build", build, "--config", "Release", "--target", "mosa_lib", "mosa_pl_units"},
                        "build")) {
    return false;
  }
  return run_runtime_step(job, {cmake, "--install", build, "--prefix", utf8_from_path(prefix), "--component", "runtime",
                                "--config", "Release"},
                          "install");
}

bool prepare_dir(const std::filesystem::path& dir) {
  std::error_code error;
  std::filesystem::remove_all(dir, error);
  if (!error) std::filesystem::create_directories(dir / "logs", error);
  if (error) report_write_error(dir, error);
  return !error;
}

std::optional<RuntimeLayout> runtime_from_source(const BuildRequest& request, const Toolchain& toolchain) {
  if (!toolchain.cmake || !cmake_new_enough(toolchain.cmake_version)) {
    std::cerr << mosac_message("mosac_runtime_source_needs_cmake") << '\n';
    return std::nullopt;
  }
  const auto source = bundled_source_dir(toolchain);
  const auto digest = source ? tree_digest(*source) : std::nullopt;
  if (!source || !digest) {
    const std::filesystem::path shown = source ? *source : toolchain.runtime_dir.parent_path() / "lib" / "mosa" / "source";
    std::cerr << mosac_message("mosac_runtime_source_missing", {utf8_from_path(shown)}) << '\n';
    return std::nullopt;
  }
  const std::string abi = runtime_abi(toolchain);
  const std::string key = "mosac-runtime 1\ncxx " + utf8_from_path(toolchain.cxx) + "\nversion " + toolchain.cxx_version +
                          "\nabi " + abi + "\nsource " + hex16(*digest) + "\n";
  if (!cache_root_usable()) return std::nullopt;
  const RuntimeJob job{request, toolchain, cache_root() / "runtime" / hex16(fnv1a(key, 0xcbf29ce484222325ULL))};
  const std::filesystem::path prefix = job.dir / "prefix";
  const std::filesystem::path root = prefix / "lib" / "mosa" / abi;
  if (is_file(job.dir / "key.txt") && read_text(job.dir / "key.txt") == key) {
    if (const auto layout = layout_at(root, toolchain)) return layout;
  }
  if (request.verbose) std::cerr << mosac_message("mosac_runtime_building", {utf8_from_path(job.dir)}) << '\n';
  if (!prepare_dir(job.dir) || !build_runtime(job, *source, prefix)) return std::nullopt;
  // 鍵は最後に書く。途中で止まった物を当たりにしない。
  if (const auto error = write_text(job.dir / "key.txt", key)) {
    report_write_error(job.dir / "key.txt", *error);
    return std::nullopt;
  }
  const auto layout = layout_at(root, toolchain);
  if (!layout) std::cerr << mosac_message("mosac_runtime_missing", {utf8_from_path(root)}) << '\n';
  return layout;
}

}  // namespace

std::string runtime_abi(const Toolchain& toolchain) {
  return std::string{os_name(host_os())} + "-" + std::string{host_arch()} + "-" + compiler_family(toolchain);
}

std::filesystem::path unit_archive(const Toolchain& toolchain, const RuntimeLayout& layout, std::string_view name) {
  const std::string file = is_msvc(toolchain.cxx) ? "mosa_pl_" + std::string{name} + ".lib"
                                                  : "libmosa_pl_" + std::string{name} + ".a";
  return layout.units_dir / path_from_utf8(file);
}

std::optional<RuntimeLayout> runtime_layout(const BuildRequest& request, const Toolchain& toolchain) {
  if (request.runtime_source) return runtime_from_source(request, toolchain);
  if (const auto layout = layout_at(toolchain.runtime_dir, toolchain)) return layout;
  const std::filesystem::path installed = toolchain.runtime_dir.parent_path() / "lib" / "mosa";
  const std::filesystem::path root = installed / runtime_abi(toolchain);
  if (const auto layout = layout_at(root, toolchain)) return layout;
  // lib/mosa/ はあるのにこの鍵のディレクトリが無いなら、コンパイラの族が合っていない。
  if (is_dir(installed)) {
    std::cerr << mosac_message("mosac_runtime_abi_missing", {utf8_from_path(root)}) << '\n';
  } else {
    std::cerr << mosac_message("mosac_runtime_missing", {utf8_from_path(toolchain.runtime_dir)}) << '\n';
  }
  return std::nullopt;
}

}  // namespace mosa::codegen
