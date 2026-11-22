// 実行時ライブラリの場所（SPEC 第6節、計画 Task 10）。mosac は自分の実行ファイルの場所から 2 つの形を見る。
//  - ビルド木の形: mosac の隣に libmosa、include/、pl/ がある（本体の CMakeLists.txt の mosa_lib_dir と mosa_pl_units）
//  - インストールの形: <prefix>/bin/mosac から <prefix>/lib/mosa/<OS>-<arch>-<compiler>/ を引く
// 鍵 <OS>-<arch>-<compiler> は見つけたコンパイラの族（clang、gcc、msvc）で作る。mosac を作ったコンパイラと違う族を
// --cxx で指せば、その族のディレクトリが無いので ABI の不一致として止め、--runtime-source を案内する。
// --runtime-source は同梱の源（<prefix>/lib/mosa/source/、ビルド木ならリポジトリの根）を CMake で作り、
// .mosac-cache/runtime/<hash>/w-<16 進 8 桁>/prefix/ に install して（<hash>/current がその w-* を指す）、そこを
// インストールの形として使う。鍵はコンパイラと、源のうち install の source の部品（CMakeLists.txt、src/、include/、
// lib/）の中身。ビルド木の根を木ごと鍵にすると .git や build/ や .mosac-cache 自身が入って当たらない。
// テスト: tests/mosac/installed_layout_test.cmake（インストールの形）、tests/mosac/build_test.cmake（ビルド木の形）、
// tests/mosac/runtime_source_test.cmake（--runtime-source を 2 回。libmosa を作るので MOSA_SLOW_TESTS=ON のときだけ）。

#include "build.hpp"

#include "mosa/path_text.hpp"
#include "mosa/process.hpp"

#include <iostream>
#include <thread>

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

struct RuntimeJob {
  const BuildRequest& request;
  const Toolchain& toolchain;
  std::filesystem::path dir;  // このビルドの作業用のディレクトリ runtime/<hash>/w-*/（動かさない）
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

// 1 回きりの大きなビルドなので、機械の論理コアの数で並列にする。数が取れなければ 1。
std::string parallel_jobs() {
  const unsigned count = std::thread::hardware_concurrency();
  return std::to_string(count == 0 ? 1U : count);
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
  if (!run_runtime_step(job, {cmake, "--build", build, "--config", "Release", "--parallel", parallel_jobs(), "--target",
                              "mosa_lib", "mosa_pl_units"},
                        "build")) {
    return false;
  }
  return run_runtime_step(job, {cmake, "--install", build, "--prefix", utf8_from_path(prefix), "--component", "runtime",
                                "--config", "Release"},
                          "install");
}

// Task 9 の cache_hit と同じ守り。runtime/、<hash>/、その current のどれかが記号リンクなら、当たりにも作り直しの
// 対象にもしない。中身を差し替えた物を結ばせないためである。
bool runtime_cache_plain(const std::filesystem::path& dir) {
  for (const std::filesystem::path& path : {dir.parent_path(), dir, dir / "current"}) {
    std::error_code error;
    if (!std::filesystem::is_symlink(std::filesystem::symlink_status(path, error))) continue;
    std::cerr << mosac_message("mosac_cache_symlink", {utf8_from_path(path)}) << '\n';
    return false;
  }
  return true;
}

bool runtime_cached(const std::filesystem::path& dir, std::string_view key) {
  return plain(dir / "key.txt", std::filesystem::file_type::regular) && read_text(dir / "key.txt") == key;
}

// 鍵と、入れた実行時ライブラリの場所 <dir>/prefix/lib/mosa/<abi>/。
struct RuntimeKey {
  std::string text;
  std::string abi;
  std::filesystem::path root(const std::filesystem::path& dir) const { return dir / "prefix" / "lib" / "mosa" / abi; }
};

bool runtime_ready(const std::filesystem::path& dir, const RuntimeKey& key, const Toolchain& toolchain) {
  return runtime_cached(dir, key.text) && layout_at(key.root(dir), toolchain).has_value();
}

// 新しい作業用のディレクトリで作り、鍵は最後に書く（途中で止まった物を当たりにしない）。できたら current で指す
// （レビュー M-44。前は置き場所を消して作り直したので、同じ鍵で同時に作るとほかのビルドの作りかけを消した）。
// 失敗した物は指さず、ログは知らせに出したパスに残る。返すのは current の指す物で、先に終えたビルドの物でもある。
std::optional<std::filesystem::path> make_runtime(const BuildRequest& request, const Toolchain& toolchain,
                                                  const std::filesystem::path& source, const std::filesystem::path& entry,
                                                  const RuntimeKey& key) {
  std::error_code error;
  const auto dir = private_dir(entry, error);
  if (dir) std::filesystem::create_directories(*dir / "logs", error);
  if (!dir || error) {
    report_write_error(entry, error);
    return std::nullopt;
  }
  const RuntimeJob job{request, toolchain, *dir};
  bool built = build_runtime(job, source, *dir / "prefix");
  if (const auto failed = built ? write_text(*dir / "key.txt", key.text) : std::nullopt) {
    report_write_error(*dir / "key.txt", *failed);
    built = false;
  }
  const auto placed = built ? publish_dir(
                                  entry, *dir,
                                  [&](const std::filesystem::path& work) { return runtime_ready(work, key, toolchain); },
                                  error)
                            : std::nullopt;
  if (built && !placed) report_write_error(entry / "current", error);
  if (!placed) mark_failed(*dir);
  return placed;
}

std::optional<RuntimeLayout> runtime_from_source(const BuildRequest& request, const Toolchain& toolchain) {
  if (!toolchain.cmake || !cmake_new_enough(toolchain.cmake_version)) {
    std::cerr << mosac_message("mosac_runtime_source_needs_cmake") << '\n';
    return std::nullopt;
  }
  const auto source = bundled_source_dir(toolchain);
  const auto digest = source ? source_digest(*source) : std::nullopt;
  if (!source || !digest) {
    const std::filesystem::path shown = source ? *source : toolchain.runtime_dir.parent_path() / "lib" / "mosa" / "source";
    std::cerr << mosac_message("mosac_runtime_source_missing", {utf8_from_path(shown)}) << '\n';
    return std::nullopt;
  }
  const std::string abi = runtime_abi(toolchain);
  const RuntimeKey key{"mosac-runtime 3\ncxx " + utf8_from_path(toolchain.cxx) + "\nversion " + toolchain.cxx_version +
                           "\nabi " + abi + "\nsource " + hex16(*digest) + "\n",
                       abi};
  if (!cache_root_usable()) return std::nullopt;
  const std::filesystem::path entry = cache_root() / "runtime" / hex16(fnv1a(key.text, kFnvBasis));
  if (!runtime_cache_plain(entry)) return std::nullopt;
  if (const auto dir = current_dir(entry); dir && runtime_ready(*dir, key, toolchain)) {
    if (request.verbose) std::cerr << mosac_message("mosac_runtime_cached", {utf8_from_path(key.root(*dir))}) << '\n';
    return layout_at(key.root(*dir), toolchain);
  }
  if (request.verbose) std::cerr << mosac_message("mosac_runtime_building", {utf8_from_path(entry)}) << '\n';
  const auto placed = make_runtime(request, toolchain, *source, entry, key);
  if (!placed) return std::nullopt;
  // current の指す物から作る。プログラムの鍵は実行時ライブラリのパスを含み、そのディレクトリは動かないので当たり続ける。
  const auto layout = layout_at(key.root(*placed), toolchain);
  if (!layout) std::cerr << mosac_message("mosac_runtime_missing", {utf8_from_path(key.root(*placed))}) << '\n';
  return layout;
}

}  // namespace

std::string host_os_arch() { return std::string{os_name(host_os())} + "-" + std::string{host_arch()}; }

std::string runtime_abi(const Toolchain& toolchain) { return host_os_arch() + "-" + compiler_family(toolchain); }

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
