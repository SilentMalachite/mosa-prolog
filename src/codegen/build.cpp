// mosac build の翻訳 → コンパイル → リンク（SPEC 第6節「build and run pipeline」、計画 Task 9）。
// CMake があればそれで、無いか MOSA_NO_CMAKE=1 なら生成コードを 1 ファイルずつコンパイルして mosac がリンクの命令を組む。
// 既定は -O2 と静的リンク（Windows は /MT）、--debug は -O0 -g と節の追跡。子プロセスは run_process だけで起こし、
// Toolchain::env（Windows の vcvarsall の環境）を渡す。中間物は .mosac-cache/<hash>/（cache.cpp）に置く。
// 生成コードがコンパイルできなければ mosac の誤りとして内部エラーの鍵だけを出し、生の誤りは --verbose のときだけ出す。
// テスト: tests/mosac/build_test.cmake

#include "build.hpp"

#include "mosa/exe_path.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/process.hpp"

#include <fstream>
#include <iostream>
#include <iterator>

namespace mosa::codegen {
namespace {

using Args = std::vector<std::string>;

// これより空きが少なければ、コンパイルの失敗を生成コードの誤りでなくディスクの問題として出す。
constexpr std::uintmax_t kLowSpace = std::uintmax_t{64} << 20;

bool is_msvc(const std::filesystem::path& cxx) {
  std::string stem = utf8_from_path(cxx.stem());
  for (char& unit : stem) unit = (unit >= 'A' && unit <= 'Z') ? static_cast<char>(unit - 'A' + 'a') : unit;
  return stem == "cl";
}

void append(Args& out, const Args& items) { out.insert(out.end(), items.begin(), items.end()); }

// 警告の旗は emit の CMakeLists.txt と同じにし、どちらの道でも同じコードが通るかを揃える。
Args compile_flags(const Toolchain& toolchain, const RuntimeLayout& runtime, bool debug) {
  const std::string include = utf8_from_path(runtime.include_dir);
  if (is_msvc(toolchain.cxx)) {
    Args flags{"/nologo", "/std:c++20", "/EHsc", "/utf-8", "/permissive-", "/Zc:__cplusplus", "/W4", "/WX", "/MT"};
    append(flags, debug ? Args{"/Od", "/Z7"} : Args{"/O2"});
    flags.push_back("/I" + include);
    return flags;
  }
  Args flags{"-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror"};
  append(flags, debug ? Args{"-O0", "-g"} : Args{"-O2"});
  flags.push_back("-I" + include);
  return flags;
}

// オブジェクトの後ろに置く旗。libmosa は静的ライブラリなので、それ自身はいつも静的に入る。Linux は C++ の
// 実行時ライブラリも静的にする。macOS の libc++ は OS の一部で、静的にはできない。
Args link_flags(const Toolchain& toolchain, const RuntimeLayout& runtime) {
  const std::string library = utf8_from_path(runtime.library);
  if (is_msvc(toolchain.cxx)) return Args{"/nologo", library, "shell32.lib"};
  if (host_os() == HostOs::linux) return Args{library, "-static-libstdc++", "-static-libgcc"};
  return Args{library};
}

// CMake の構成の旗。libmosa は Release の /MT なので、--debug でも実行時ライブラリは /MT に固める。
Args cmake_defines(const Toolchain& toolchain, const RuntimeLayout& runtime, bool debug) {
  const bool msvc = is_msvc(toolchain.cxx);
  Args defines{std::string{"-DCMAKE_BUILD_TYPE="} + (debug ? "Debug" : "Release"),
               "-DCMAKE_CXX_COMPILER=" + utf8_from_path(toolchain.cxx),
               std::string{"-DCMAKE_CXX_FLAGS_RELEASE="} + (msvc ? "/O2" : "-O2"),
               std::string{"-DCMAKE_CXX_FLAGS_DEBUG="} + (msvc ? "/Od /Z7" : "-O0 -g"),
               "-DMOSA_RUNTIME_DIR=" + utf8_from_path(runtime.library.parent_path())};
  // 既定の Visual Studio の生成器はコンパイラを自分で選び、vcvarsall の環境も -DCMAKE_CXX_COMPILER も使わない。
  if (msvc) append(defines, Args{"-G", "NMake Makefiles", "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"});
  if (!msvc && host_os() == HostOs::linux) {
    defines.emplace_back("-DCMAKE_EXE_LINKER_FLAGS=-static-libstdc++ -static-libgcc");
  }
  return defines;
}

// Windows で cl 以外のコンパイラを CMake に渡すと、既定の Visual Studio の生成器はそれを無視して MSVC で作る。
// 頼まれたコンパイラで作るため、その組み合わせは直接の道にする。
bool use_cmake(const Toolchain& toolchain) {
  const auto no_cmake = environment_path("MOSA_NO_CMAKE");
  if (no_cmake && utf8_from_path(*no_cmake) == "1") return false;
  if (host_os() == HostOs::windows && !is_msvc(toolchain.cxx)) return false;
  return toolchain.cmake.has_value();
}

std::string command_text(const Args& argv) {
  std::string text;
  for (const std::string& arg : argv) {
    if (!text.empty()) text += ' ';
    text += arg.find(' ') == std::string::npos ? arg : "\"" + arg + "\"";
  }
  return text;
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream in{path, std::ios::binary};
  return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

struct Job {
  const BuildRequest& request;
  const Toolchain& toolchain;
  const RuntimeLayout& runtime;
  std::filesystem::path dir;
  std::string hash;
  std::filesystem::path exe_name;
};

// 生成コードがコンパイルできないのは mosac の誤り（SPEC 第6節）。ただし空きが無いせいなら環境の問題として出す。
void report_build_failure(const Job& job) {
  std::error_code error;
  const std::filesystem::space_info space = std::filesystem::space(job.dir, error);
  if (!error && space.available < kLowSpace) {
    std::cerr << mosac_message("mosac_disk_full", {utf8_from_path(job.dir)}) << '\n';
    return;
  }
  std::cerr << mosac_message("mosac_internal_error", {".mosac-cache/" + job.hash + "/"}) << '\n';
}

// 出力は logs/<名前>.log に書き、失敗したときだけ --verbose で見せる。
bool run_step(const Job& job, const Args& argv, std::string_view log_name) {
  if (job.request.verbose) std::cerr << mosac_message("mosac_verbose_run", {command_text(argv)}) << '\n';
  const std::filesystem::path log = job.dir / "logs" / path_from_utf8(std::string{log_name} + ".log");
  const std::optional<int> code = run_process(argv, log, job.toolchain.env);
  if (!code) {
    std::cerr << mosac_message("mosac_cannot_start", {argv.front()}) << '\n';
    return false;
  }
  if (*code == 0) return true;
  if (job.request.verbose) std::cerr << read_text(log);
  report_build_failure(job);
  return false;
}

std::optional<std::filesystem::path> build_direct(const Job& job, std::span<const ProjectFile> files) {
  const bool msvc = is_msvc(job.toolchain.cxx);
  const std::string cxx = utf8_from_path(job.toolchain.cxx);
  Args link{cxx};
  for (const ProjectFile& file : files) {
    const std::filesystem::path source = path_from_utf8(file.name);
    if (source.extension() != ".cpp") continue;
    const std::string stem = utf8_from_path(source.stem());
    const std::string object = utf8_from_path(job.dir / "obj" / path_from_utf8(stem + (msvc ? ".obj" : ".o")));
    const std::string input = utf8_from_path(job.dir / "src" / source);
    Args compile{cxx};
    append(compile, compile_flags(job.toolchain, job.runtime, job.request.debug));
    append(compile, msvc ? Args{"/c", input, "/Fo" + object} : Args{"-c", input, "-o", object});
    if (!run_step(job, compile, "compile-" + stem)) return std::nullopt;
    link.push_back(object);
  }
  const std::filesystem::path exe = job.dir / "build" / job.exe_name;
  append(link, link_flags(job.toolchain, job.runtime));
  append(link, msvc ? Args{"/Fe" + utf8_from_path(exe)} : Args{"-o", utf8_from_path(exe)});
  if (msvc && job.request.debug) append(link, Args{"/link", "/DEBUG"});
  if (!run_step(job, link, "link")) return std::nullopt;
  return exe;
}

std::optional<std::filesystem::path> build_cmake(const Job& job) {
  const std::string cmake = utf8_from_path(*job.toolchain.cmake);
  const std::string build = utf8_from_path(job.dir / "build");
  Args configure{cmake, "-S", utf8_from_path(job.dir / "src"), "-B", build};
  append(configure, cmake_defines(job.toolchain, job.runtime, job.request.debug));
  if (!run_step(job, configure, "configure")) return std::nullopt;
  const std::string config = job.request.debug ? "Debug" : "Release";
  if (!run_step(job, Args{cmake, "--build", build, "--config", config}, "build")) return std::nullopt;
  // 多構成の生成器（環境変数 CMAKE_GENERATOR で Visual Studio を選んだときなど）は構成名のディレクトリへ出す。
  for (const std::filesystem::path& candidate : {job.dir / "build" / config / job.exe_name, job.dir / "build" / job.exe_name}) {
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) return candidate;
  }
  report_build_failure(job);
  return std::nullopt;
}

// 目録と key.txt を書く。前に失敗した同じ鍵の中間物は上書きする。
bool stage(const Job& job, const CacheKey& key, std::span<const ProjectFile> files) {
  for (const char* sub : {"src", "logs", "obj", "build"}) {
    std::error_code error;
    std::filesystem::create_directories(job.dir / sub, error);
    if (error) {
      report_write_error(job.dir / sub, error);
      return false;
    }
  }
  if (const auto error = write_text(job.dir / "key.txt", key.text)) {
    report_write_error(job.dir / "key.txt", *error);
    return false;
  }
  for (const ProjectFile& file : files) {
    const std::filesystem::path target = job.dir / "src" / path_from_utf8(file.name);
    if (const auto error = write_text(target, file.text)) {
      report_write_error(target, *error);
      return false;
    }
  }
  return true;
}

// できあがりは .part に写してから名前を替える。途中で止まった写しを当たりと取り違えない。
bool publish(const Job& job, const std::filesystem::path& built) {
  const std::filesystem::path final_exe = job.dir / job.exe_name;
  std::filesystem::path part = final_exe;
  part += ".part";
  std::error_code error;
  std::filesystem::copy_file(built, part, std::filesystem::copy_options::overwrite_existing, error);
  if (!error) std::filesystem::rename(part, final_exe, error);
  if (error) report_write_error(final_exe, error);
  return !error;
}

Built build_translated(const BuildRequest& request, const Project& project, const Toolchain& toolchain,
                       const RuntimeLayout& runtime) {
  const bool cmake = use_cmake(toolchain);
  const std::string target = target_name(request.source);
  CacheKeyInputs inputs{project.files, &toolchain, &runtime, cmake ? "cmake " + utf8_from_path(*toolchain.cmake) : "direct",
                        cmake ? cmake_defines(toolchain, runtime, request.debug) : compile_flags(toolchain, runtime, request.debug),
                        target};
  if (!cmake) append(inputs.flags, link_flags(toolchain, runtime));
  const auto key = cache_key(inputs);
  if (!key) {
    std::cerr << mosac_message("mosac_runtime_missing", {utf8_from_path(runtime.library)}) << '\n';
    return {};
  }
  if (request.clean) {
    std::error_code error;
    std::filesystem::remove_all(cache_root(), error);
    if (error) {
      report_write_error(cache_root(), error);
      return {};
    }
  }
  const Job job{request, toolchain, runtime, cache_root() / key->hash, key->hash, path_from_utf8(target + exe_suffix())};
  if (cache_hit(job.dir, *key, job.exe_name)) {
    if (request.verbose) std::cerr << mosac_message("mosac_cache_hit", {utf8_from_path(job.dir)}) << '\n';
    return Built{0, job.dir / job.exe_name};
  }
  if (!stage(job, *key, project.files)) return {};
  const auto built = cmake ? build_cmake(job) : build_direct(job, project.files);
  if (!built || !publish(job, *built)) return {};
  return Built{0, job.dir / job.exe_name};
}

std::string_view install_hint(HostOs os) {
  switch (os) {
    case HostOs::macos:
      return "toolchain_hint_macos";
    case HostOs::linux:
      return "toolchain_hint_linux";
    case HostOs::windows:
      return "toolchain_hint_windows";
  }
  return "toolchain_hint_linux";
}

}  // namespace

std::string mosac_message(std::string_view key, std::vector<std::string> args) {
  return render_message_fresh(default_message_language(), key, args);
}

void report_write_error(const std::filesystem::path& path, std::error_code error) {
  std::string_view key = "mosac_cannot_write";
  if (error == std::errc::no_space_on_device) {
    key = "mosac_disk_full";
  } else if (error == std::errc::permission_denied || error == std::errc::operation_not_permitted ||
             error == std::errc::read_only_file_system) {
    key = "mosac_no_permission";
  }
  std::cerr << mosac_message(key, {utf8_from_path(path)}) << '\n';
}

void report_missing_toolchain(const ToolchainQuery& query) {
  // 名指しのコンパイラが無いのは綴りか設定の誤りで、導入手順は答えにならない。
  if (const auto named = named_compiler(host_toolchain_search(query))) {
    std::cerr << mosac_message("toolchain_named_not_found", {named->program, std::string{named->source}}) << '\n';
  } else {
    std::cerr << mosac_message("toolchain_not_found") << '\n' << mosac_message(install_hint(host_os())) << '\n';
  }
}

std::optional<RuntimeLayout> runtime_layout(const Toolchain& toolchain) {
  // libmosa の名前は本体の CMakeLists.txt の OUTPUT_NAME mosa による（MSVC は mosa.lib、ほかは libmosa.a）。
  const std::filesystem::path library = is_msvc(toolchain.cxx) ? "mosa.lib" : "libmosa.a";
  RuntimeLayout layout{toolchain.runtime_dir / "include", toolchain.runtime_dir / library};
  std::error_code error;
  if (!std::filesystem::is_regular_file(layout.library, error)) return std::nullopt;
  if (!std::filesystem::is_regular_file(layout.include_dir / "mosa" / "runtime.hpp", error)) return std::nullopt;
  return layout;
}

std::string exe_suffix() { return host_os() == HostOs::windows ? ".exe" : ""; }

std::filesystem::path with_exe_suffix(const std::filesystem::path& path) {
  std::string extension = utf8_from_path(path.extension());
  for (char& unit : extension) unit = (unit >= 'A' && unit <= 'Z') ? static_cast<char>(unit - 'A' + 'a') : unit;
  if (host_os() != HostOs::windows || extension == ".exe") return path;
  std::filesystem::path out = path;
  out += ".exe";
  return out;
}

Built build_cached(const BuildRequest& request) {
  ProjectOptions options;
  options.runtime_dir = utf8_from_path(default_runtime_dir());
  options.trace = request.debug;
  // Prolog の誤りは翻訳の段で止め、コンパイラを探しにも行かない（SPEC 第6節のエラーの扱い）。
  const Project project = translate_program(request.source, options);
  if (!project.ok) {
    std::cerr << project.diagnostic << '\n';
    return {};
  }
  const ToolchainQuery query{request.cxx};
  const auto toolchain = find_toolchain(query);
  if (!toolchain) {
    report_missing_toolchain(query);
    return {};
  }
  const auto runtime = runtime_layout(*toolchain);
  if (!runtime) {
    std::cerr << mosac_message("mosac_runtime_missing", {utf8_from_path(toolchain->runtime_dir)}) << '\n';
    return {};
  }
  return build_translated(request, project, *toolchain, *runtime);
}

}  // namespace mosa::codegen
