// mosac build の翻訳 → コンパイル → リンク（SPEC 第6節「build and run pipeline」、計画 Task 9）。
// CMake があればそれで、無いか MOSA_NO_CMAKE=1 なら生成コードを 1 ファイルずつコンパイルして mosac がリンクの命令を組む。
// 既定は -O2 と静的リンク（Windows は /MT）、--debug は -O0 -g と節の追跡。子プロセスは run_process だけで起こし、
// Toolchain::env（Windows の vcvarsall の環境）を渡す。中間物は .mosac-cache/<hash>/（cache.cpp）に置く。
// 実行時ライブラリと翻訳済みの lib/*.pl の単位の場所は runtime_layout.cpp（計画 Task 10）。
// 生成コードがコンパイルできなければ mosac の誤りとして内部エラーの鍵だけを出し、生の誤りは --verbose のときだけ出す。
// テスト: tests/mosac/build_test.cmake

#include "build.hpp"

#include "mosa/exe_path.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/process.hpp"

#include <algorithm>
#include <charconv>
#include <iostream>

namespace mosa::codegen {
namespace {

using Args = std::vector<std::string>;

// これより空きが少なければ、コンパイルの失敗を生成コードの誤りでなくディスクの問題として出す。
constexpr std::uintmax_t kLowSpace = std::uintmax_t{64} << 20;

void append(Args& out, const Args& items) { out.insert(out.end(), items.begin(), items.end()); }

bool env_is_one(std::string_view name) {
  const auto value = environment_path(name);
  return value && utf8_from_path(*value) == "1";
}

// 警告を誤りにするのは MOSA_WERROR=1 のときだけ（このリポジトリのテストが立てる）。利用者のビルドでは、
// 新しいコンパイラが増やした警告を「内部エラー」にしない。emit の CMakeLists.txt の MOSA_WARNINGS_AS_ERRORS と同じ。
bool warnings_as_errors() { return env_is_one("MOSA_WERROR"); }

// 最適化の旗。NDEBUG は Release の libmosa と揃える。
std::string release_flags(bool msvc) { return msvc ? "/O2 /DNDEBUG" : "-O2 -DNDEBUG"; }
std::string debug_flags(bool msvc) { return msvc ? "/Od /Z7" : "-O0 -g"; }

Args words(std::string_view text) {
  Args out;
  for (std::size_t at = 0; at < text.size();) {
    const auto end = std::min(text.find(' ', at), text.size());
    if (end > at) out.emplace_back(text.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

Args compile_flags(const Toolchain& toolchain, const RuntimeLayout& runtime, bool debug) {
  const std::string include = utf8_from_path(runtime.include_dir);
  const bool msvc = is_msvc(toolchain.cxx);
  Args flags = msvc ? Args{"/nologo", "/std:c++20", "/EHsc", "/utf-8", "/permissive-", "/Zc:__cplusplus", "/W4", "/MT"}
                    : Args{"-std=c++20", "-Wall", "-Wextra", "-Wpedantic"};
  if (warnings_as_errors()) flags.emplace_back(msvc ? "/WX" : "-Werror");
  append(flags, words(debug ? debug_flags(msvc) : release_flags(msvc)));
  flags.push_back((msvc ? "/I" : "-I") + include);
  return flags;
}

// 結ぶ翻訳済みのライブラリ（計画 Task 10）。use_module された物と文面の表だけで、libmosa の記号を引くので libmosa より前に置く。
Args unit_archives(const Toolchain& toolchain, const RuntimeLayout& runtime, std::span<const std::string> libraries) {
  Args out;
  for (const std::string& name : libraries) out.push_back(utf8_from_path(unit_archive(toolchain, runtime, name)));
  return out;
}

// オブジェクトの後ろに置く旗。libmosa は静的ライブラリなので、それ自身はいつも静的に入る。Linux は C++ の
// 実行時ライブラリも静的にする。macOS の libc++ は OS の一部で、静的にはできない。
Args link_flags(const Toolchain& toolchain, const RuntimeLayout& runtime, std::span<const std::string> libraries) {
  Args flags = is_msvc(toolchain.cxx) ? Args{"/nologo"} : Args{};
  append(flags, unit_archives(toolchain, runtime, libraries));
  flags.push_back(utf8_from_path(runtime.library));
  if (is_msvc(toolchain.cxx)) flags.emplace_back("shell32.lib");
  if (!is_msvc(toolchain.cxx) && host_os() == HostOs::linux) append(flags, Args{"-static-libstdc++", "-static-libgcc"});
  return flags;
}

// CMake の構成の旗。libmosa は Release の /MT なので、--debug でも実行時ライブラリは /MT に固める。
Args cmake_defines(const Toolchain& toolchain, const RuntimeLayout& runtime, bool debug) {
  const bool msvc = is_msvc(toolchain.cxx);
  const bool static_cxx = !msvc && host_os() == HostOs::linux;
  // CMAKE_CXX_FLAGS と CMAKE_EXE_LINKER_FLAGS は明示する。最初の configure が利用者の CXXFLAGS や LDFLAGS を
  // 読むと、キャッシュの鍵に入らない旗でビルドが変わる。
  Args defines{std::string{"-DCMAKE_BUILD_TYPE="} + (debug ? "Debug" : "Release"),
               "-DCMAKE_CXX_COMPILER=" + utf8_from_path(toolchain.cxx),
               "-DCMAKE_CXX_FLAGS=",
               std::string{"-DCMAKE_EXE_LINKER_FLAGS="} + (static_cxx ? "-static-libstdc++ -static-libgcc" : ""),
               "-DCMAKE_CXX_FLAGS_RELEASE=" + release_flags(msvc),
               "-DCMAKE_CXX_FLAGS_DEBUG=" + debug_flags(msvc),
               std::string{"-DMOSA_WARNINGS_AS_ERRORS="} + (warnings_as_errors() ? "ON" : "OFF"),
               "-DMOSA_RUNTIME_DIR=" + utf8_from_path(runtime.root)};
  // 既定の Visual Studio の生成器はコンパイラを自分で選び、vcvarsall の環境も -DCMAKE_CXX_COMPILER も使わない。
  if (msvc) append(defines, Args{"-G", "NMake Makefiles", "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded"});
  return defines;
}

// Windows で cl 以外のコンパイラを CMake に渡すと、既定の Visual Studio の生成器はそれを無視して MSVC で作る。
// 頼まれたコンパイラで作るため、その組み合わせは直接の道にする。
bool use_cmake(const Toolchain& toolchain) {
  if (env_is_one("MOSA_NO_CMAKE")) return false;
  if (host_os() == HostOs::windows && !is_msvc(toolchain.cxx)) return false;
  return toolchain.cmake.has_value() && cmake_new_enough(toolchain.cmake_version);
}

std::string command_text(const Args& argv) {
  std::string text;
  for (const std::string& arg : argv) {
    if (!text.empty()) text += ' ';
    text += arg.find(' ') == std::string::npos ? arg : "\"" + arg + "\"";
  }
  return text;
}

struct Job {
  const BuildRequest& request;
  const Toolchain& toolchain;
  const RuntimeLayout& runtime;
  std::span<const std::string> libraries;  // 結ぶ翻訳済みのライブラリの名前（Project::libraries）
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

// configure の失敗は生成コードでなく CMake か環境の問題（古い生成器、壊れたコンパイラなど）。ログの場所と、
// CMake を使わない道（MOSA_NO_CMAKE=1）を案内する。
enum class Stage { generated, configure };

// 出力は logs/<名前>.log に書き、失敗したときだけ --verbose で見せる。
bool run_step(const Job& job, const Args& argv, std::string_view log_name, Stage stage = Stage::generated) {
  if (job.request.verbose) std::cerr << mosac_message("mosac_verbose_run", {command_text(argv)}) << '\n';
  const std::filesystem::path log = job.dir / "logs" / path_from_utf8(std::string{log_name} + ".log");
  const std::optional<int> code = run_process(argv, log, job.toolchain.env);
  if (!code) {
    std::cerr << mosac_message("mosac_cannot_start", {argv.front()}) << '\n';
    return false;
  }
  if (*code == 0) return true;
  if (job.request.verbose) std::cerr << read_text(log);
  if (stage == Stage::configure) {
    std::cerr << mosac_message("mosac_cmake_configure_failed", {utf8_from_path(log)}) << '\n';
  } else {
    report_build_failure(job);
  }
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
  append(link, link_flags(job.toolchain, job.runtime, job.libraries));
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
  if (!run_step(job, configure, "configure", Stage::configure)) return std::nullopt;
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

// 目録と key.txt を書く。外れのときはハッシュのディレクトリを消して作り直し、前に失敗した中間物も、
// 中に置かれた記号リンクも辿らない。.mosac-cache そのものが記号リンクなら使わない。
bool stage(const Job& job, const CacheKey& key, std::span<const ProjectFile> files) {
  if (!cache_root_usable()) return false;
  std::error_code removed;
  std::filesystem::remove_all(job.dir, removed);
  if (removed) {
    report_write_error(job.dir, removed);
    return false;
  }
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

// 結ぶ翻訳済みのライブラリが全部あるか。無い物は名前を出す（実行時ライブラリの置き場所が欠けている）。
bool units_present(const Toolchain& toolchain, const RuntimeLayout& runtime, std::span<const std::string> libraries) {
  for (const std::string& name : libraries) {
    std::error_code error;
    const std::filesystem::path archive = unit_archive(toolchain, runtime, name);
    if (std::filesystem::is_regular_file(archive, error)) continue;
    std::cerr << mosac_message("mosac_unit_missing", {utf8_from_path(archive)}) << '\n';
    return false;
  }
  return true;
}

// --verbose のとき、結ぶ翻訳済みのライブラリの書庫と、原文だけを同梱したライブラリ（mosac の隣の lib/ と違う版で、
// IR のまま動く）を出す。遅い理由と結び忘れを利用者が見られるようにする。テスト: standalone_use_lists、standalone_plain_use。
void report_libraries(const Toolchain& toolchain, const RuntimeLayout& runtime, const Project& project) {
  std::string archives;
  for (const std::string& name : project.libraries) {
    archives += (archives.empty() ? "" : " ") + utf8_from_path(unit_archive(toolchain, runtime, name));
  }
  std::cerr << mosac_message("mosac_verbose_units", {archives}) << '\n';
  if (project.source_only.empty()) return;
  std::string names;
  for (const std::string& name : project.source_only) names += (names.empty() ? "" : " ") + name;
  std::cerr << mosac_message("mosac_verbose_source_only", {names}) << '\n';
}

Built build_translated(const BuildRequest& request, const Project& project, const Toolchain& toolchain,
                       const RuntimeLayout& runtime) {
  const bool cmake = use_cmake(toolchain);
  const std::string target = target_name(request.source);
  const std::string mode = cmake ? "cmake " + utf8_from_path(*toolchain.cmake) + " " + toolchain.cmake_version : "direct";
  if (!units_present(toolchain, runtime, project.libraries)) return {};
  if (request.verbose) report_libraries(toolchain, runtime, project);
  CacheKeyInputs inputs{project.files, &toolchain, &runtime, {}, mode,
                        cmake ? cmake_defines(toolchain, runtime, request.debug)
                              : compile_flags(toolchain, runtime, request.debug),
                        target};
  for (const std::string& name : project.libraries) inputs.units.push_back(unit_archive(toolchain, runtime, name));
  if (!cmake) append(inputs.flags, link_flags(toolchain, runtime, project.libraries));
  const auto key = cache_key(inputs);
  if (!key) {
    std::cerr << mosac_message("mosac_runtime_missing", {utf8_from_path(runtime.library)}) << '\n';
    return {};
  }
  const Job job{request, toolchain, runtime, project.libraries, cache_root() / key->hash, key->hash,
                path_from_utf8(target + exe_suffix())};
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

// emit の CMakeLists.txt は cmake_minimum_required(VERSION 3.25) なので、それより古い CMake では configure が
// 必ず失敗する。そのときは直接の道で作る。
bool cmake_new_enough(std::string_view line) {
  const auto at = line.find("version ");
  if (at == std::string_view::npos) return false;
  const std::string_view text = line.substr(at + 8);
  int major = 0;
  int minor = 0;
  const auto first = std::from_chars(text.data(), text.data() + text.size(), major);
  if (first.ec != std::errc{} || first.ptr == text.data() + text.size() || *first.ptr != '.') return false;
  const auto second = std::from_chars(first.ptr + 1, text.data() + text.size(), minor);
  if (second.ec != std::errc{}) return false;
  return major > 3 || (major == 3 && minor >= 25);
}

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
  // 名指しのコンパイラが無いのは綴りか設定の誤りで、導入手順は答えにならない。名指しの cl は vcvarsall を
  // 通さないので、開発者用のプロンプトの外では PATH に無いのが普通である。その道を案内する。
  if (const auto named = named_compiler(host_toolchain_search(query))) {
    std::cerr << mosac_message("toolchain_named_not_found", {named->program, std::string{named->source}}) << '\n';
    if (is_msvc(path_from_utf8(named->program))) std::cerr << mosac_message("toolchain_msvc_needs_prompt") << '\n';
  } else {
    std::cerr << mosac_message("toolchain_not_found") << '\n' << mosac_message(install_hint(host_os())) << '\n';
  }
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
  options.interactive = request.interactive;
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
  if (needs_developer_prompt(toolchain->cxx, !toolchain->env.empty(), environment_path("INCLUDE").has_value())) {
    std::cerr << mosac_message("toolchain_msvc_needs_prompt") << '\n';
    return {};
  }
  // --clean は実行時ライブラリより先に消す。--runtime-source はそれを .mosac-cache/runtime/ に作るので、後で消すと
  // 作ったばかりの物を消し、結ぶときに無くなる（最終レビュー）。
  if (request.clean) {
    std::error_code error;
    std::filesystem::remove_all(cache_root(), error);
    if (error) {
      report_write_error(cache_root(), error);
      return {};
    }
  }
  // 見つからない理由（置き場所が無い、ABI が合わない、源から作れない）は runtime_layout が出し終えている。
  const auto runtime = runtime_layout(request, *toolchain);
  if (!runtime) return {};
  return build_translated(request, project, *toolchain, *runtime);
}

}  // namespace mosa::codegen
