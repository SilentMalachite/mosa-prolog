// C++ コンパイラと CMake の検出（SPEC 第6節「Toolchain handling」、計画 Task 8）。OS の分岐は host_os だけで、
// 残りは HostOs を引数に取る普通のコードにする。Windows の道も macOS でコンパイルされ、文字列処理はテストで引ける。
// 子プロセスは process.cpp の run_process だけで起こす（出力はログファイル経由で読む）。
// テスト: tests/cpp/toolchain_test.cpp
#include "mosa/toolchain.hpp"

#include "mosa/exe_path.hpp"
#include "mosa/path_text.hpp"
#include "mosa/process.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <random>
#include <system_error>

namespace mosa {

#if defined(_WIN32)
HostOs host_os() { return HostOs::windows; }
#elif defined(__APPLE__)
HostOs host_os() { return HostOs::macos; }
#else
HostOs host_os() { return HostOs::linux; }
#endif

namespace {

using Environment = std::vector<std::pair<std::string, std::string>>;

bool is_file(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) && !error;
}

// Windows は拡張子の無い名前に .exe を補う（cl と書いても cl.exe を見つける）。CreateProcessW と同じく、
// 拡張子の無いファイルそのものは起こせないので選ばない。PATHEXT の他の拡張子はコンパイラに要らない。
std::optional<std::filesystem::path> existing(const std::filesystem::path& path, HostOs os) {
  if (os == HostOs::windows && !path.has_extension()) {
    std::filesystem::path exe = path;
    exe += ".exe";
    if (is_file(exe)) return exe;
    return std::nullopt;
  }
  if (is_file(path)) return path;
  return std::nullopt;
}

// ディレクトリを含む名前はそのパスだけを見る。素の名前だけを PATH の前から順に探す（シェルと同じ）。
std::optional<std::filesystem::path> resolve_program(std::string_view name, std::span<const std::filesystem::path> dirs,
                                                     HostOs os) {
  if (name.empty()) return std::nullopt;
  const std::filesystem::path program = path_from_utf8(name);
  if (program.has_parent_path()) return existing(program, os);
  for (const std::filesystem::path& dir : dirs) {
    if (auto found = existing(dir / program, os)) return found;
  }
  return std::nullopt;
}

std::vector<std::string_view> default_compilers(HostOs os) {
  switch (os) {
    case HostOs::macos:
      return {"clang++"};
    case HostOs::linux:
      return {"g++", "clang++"};
    case HostOs::windows:
      return {"cl"};
  }
  return {};
}

std::string trimmed(std::string_view line) {
  constexpr std::string_view kSpace = " \t\r\n";
  const auto begin = line.find_first_not_of(kSpace);
  if (begin == std::string_view::npos) return {};
  const auto end = line.find_last_not_of(kSpace);
  return std::string{line.substr(begin, end - begin + 1)};
}

// 行ごとに f を呼ぶ。行末の \r は切る（Windows の出力も同じ道で読む）。
template <typename F>
void each_line(std::string_view text, F&& f) {
  while (!text.empty()) {
    const auto end = text.find('\n');
    std::string_view line = text.substr(0, end);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    f(line);
    if (end == std::string_view::npos) break;
    text.remove_prefix(end + 1);
  }
}

std::string first_nonempty_line(std::string_view text) {
  std::string first;
  each_line(text, [&](std::string_view line) {
    if (first.empty()) first = trimmed(line);
  });
  return first;
}

}  // namespace

std::filesystem::path default_runtime_dir() {
  const auto self = executable_path();
  return self ? self->parent_path() : std::filesystem::path{};
}

std::optional<Toolchain> find_toolchain_in(const ToolchainSearch& search) {
  Toolchain toolchain;
  // 名前を与えられた最初のものだけを見る。見つからないときに既定へ落ちると、頼んでいないコンパイラでビルドしてしまう。
  const std::optional<std::string>* named = nullptr;
  for (const std::optional<std::string>* given : {&search.cxx_flag, &search.mosa_cxx, &search.cxx_env}) {
    if (given->has_value()) {
      named = given;
      break;
    }
  }
  std::optional<std::filesystem::path> cxx;
  if (named != nullptr) {
    cxx = resolve_program(**named, search.path, search.os);
  } else {
    for (const std::string_view name : default_compilers(search.os)) {
      if ((cxx = resolve_program(name, search.path, search.os))) break;
    }
  }
  if (!cxx) return std::nullopt;
  toolchain.cxx = *cxx;
  toolchain.cmake = resolve_program("cmake", search.path, search.os);
  toolchain.runtime_dir = search.runtime_dir;
  return toolchain;
}

std::vector<std::filesystem::path> split_path_list(std::string_view text, HostOs os) {
  const char separator = os == HostOs::windows ? ';' : ':';
  std::vector<std::filesystem::path> dirs;
  while (true) {
    const auto end = text.find(separator);
    std::string entry = trimmed(text.substr(0, end));
    // Windows の PATH は空白を含む項目を "..." で囲むことがある。
    if (os == HostOs::windows && entry.size() >= 2 && entry.front() == '"' && entry.back() == '"') {
      entry = entry.substr(1, entry.size() - 2);
    }
    if (!entry.empty()) dirs.push_back(path_from_utf8(entry));
    if (end == std::string_view::npos) break;
    text.remove_prefix(end + 1);
  }
  return dirs;
}

std::optional<std::filesystem::path> parse_vswhere_output(std::string_view text) {
  const std::string first = first_nonempty_line(text);
  if (first.empty()) return std::nullopt;
  return path_from_utf8(first);
}

std::vector<std::pair<std::string, std::string>> parse_set_dump(std::string_view text) {
  Environment env;
  each_line(text, [&](std::string_view line) {
    // =C:=C:\work のような項目はドライブごとの作業場所で、名前ではない。値の中の = は残すので最初の = で切る。
    if (line.empty() || line.front() == '=') return;
    const auto equals = line.find('=');
    if (equals == std::string_view::npos) return;
    env.emplace_back(std::string{line.substr(0, equals)}, std::string{line.substr(equals + 1)});
  });
  return env;
}

namespace {

struct Captured {
  int code = 0;
  std::string text;
};

// 同時に動く mosac どうしで名前がぶつからないよう、時刻と乱数と回数を混ぜる。
std::filesystem::path scratch_file(std::string_view extension) {
  static std::atomic<std::uint32_t> counter{0};
  std::error_code error;
  const std::filesystem::path dir = std::filesystem::temp_directory_path(error);
  const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::string name = "mosac-" + std::to_string(ticks) + "-" + std::to_string(std::random_device{}()) + "-" +
                           std::to_string(counter++) + std::string{extension};
  return (error ? std::filesystem::path{} : dir) / name;
}

// run_process は出力をファイルにしか向けられない。そこへ書かせて読み、消す。
std::optional<Captured> capture(std::span<const std::string> argv) {
  const std::filesystem::path log = scratch_file(".log");
  const std::optional<int> code = run_process(argv, log);
  Captured captured;
  {
    std::ifstream in{log, std::ios::binary};
    captured.text.assign(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});
  }
  std::error_code error;
  std::filesystem::remove(log, error);
  if (!code) return std::nullopt;
  captured.code = *code;
  return captured;
}

// 名前の比べ方は Windows に合わせる（ファイル名も環境変数名も大文字小文字を区別しない）。ASCII だけを畳む。
bool same_name(std::string_view left, std::string_view right) {
  const auto lower = [](char letter) {
    return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
  };
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (lower(left[index]) != lower(right[index])) return false;
  }
  return true;
}

bool is_msvc(const std::filesystem::path& cxx) { return same_name(utf8_from_path(cxx.stem()), "cl"); }

// gcc と clang は --version の 1 行目。cl は --version を知らず、引数なしで標準エラーに見出しを出して 0 以外で終わるので、
// 終了コードを見ずに見出しの 1 行目を取る。起こせないか 0 以外で終わる gcc/clang は使えないものとして扱う
// （macOS の /usr/bin/clang++ は Command Line Tools が無いと案内を出して失敗するだけの殻）。
std::optional<std::string> probe_version(const std::filesystem::path& cxx) {
  const bool msvc = is_msvc(cxx);
  std::vector<std::string> argv{utf8_from_path(cxx)};
  if (!msvc) argv.emplace_back("--version");
  const auto captured = capture(argv);
  if (!captured || (!msvc && captured->code != 0)) return std::nullopt;
  std::string line = first_nonempty_line(captured->text);
  if (line.empty()) return std::nullopt;
  return line;
}

std::optional<std::string> environment_text(std::string_view name) {
  const auto value = environment_path(name);
  if (!value) return std::nullopt;
  return utf8_from_path(*value);
}

// set は PATH を Path と書く。
std::string lookup(const Environment& env, std::string_view name) {
  for (const auto& [key, value] : env) {
    if (same_name(key, name)) return value;
  }
  return {};
}

// vcvarsall.bat を呼んだ後の set を取る。cmd の引用の規則は run_process の引用（CommandLineToArgvW の逆）と違い、
// "..." を含む 1 つの命令を渡すと \" に化ける。そこで命令は一時の .bat に書き、cmd にはそのパスだけを渡す。
// Windows の CI（計画 Task 12）でまだ確かめていない。x64 の固定と、日本語を含むパスの符号ページも同じ。
std::optional<Environment> vcvars_environment(const std::filesystem::path& vcvarsall) {
  if (!is_file(vcvarsall)) return std::nullopt;
  const std::filesystem::path script = scratch_file(".bat");
  {
    std::ofstream out{script, std::ios::binary};
    out << "@echo off\r\ncall \"" << utf8_from_path(vcvarsall) << "\" x64 >nul\r\nif errorlevel 1 exit /b 1\r\nset\r\n";
    if (!out) return std::nullopt;
  }
  const std::string shell = environment_text("ComSpec").value_or("cmd");
  const std::vector<std::string> argv{shell, "/d", "/c", utf8_from_path(script)};
  const auto dumped = capture(argv);
  std::error_code error;
  std::filesystem::remove(script, error);
  if (!dumped || dumped->code != 0) return std::nullopt;
  Environment env = parse_set_dump(dumped->text);
  if (lookup(env, "INCLUDE").empty()) return std::nullopt;
  return env;
}

// vswhere は Visual Studio 2017 以降の決まった場所にある。C++ の道具を入れた最新の版の場所を問う。
std::optional<Environment> msvc_environment() {
  const auto program_files = environment_path("ProgramFiles(x86)");
  if (!program_files) return std::nullopt;
  const auto vswhere = *program_files / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
  if (!is_file(vswhere)) return std::nullopt;
  const std::vector<std::string> argv{utf8_from_path(vswhere), "-latest", "-products", "*", "-requires",
                                      "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property",
                                      "installationPath"};
  const auto listed = capture(argv);
  if (!listed || listed->code != 0) return std::nullopt;
  const auto install = parse_vswhere_output(listed->text);
  if (!install) return std::nullopt;
  return vcvars_environment(*install / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat");
}

ToolchainSearch host_search(const ToolchainQuery& query) {
  ToolchainSearch search;
  search.cxx_flag = query.cxx;
  search.mosa_cxx = environment_text("MOSA_CXX");
  search.cxx_env = environment_text("CXX");
  search.os = host_os();
  if (const auto path = environment_text("PATH")) search.path = split_path_list(*path, search.os);
  search.runtime_dir = default_runtime_dir();
  return search;
}

}  // namespace

std::optional<Toolchain> find_toolchain(const ToolchainQuery& query) {
  ToolchainSearch search = host_search(query);
  Environment env;
  // Windows の既定だけは vswhere で探し、cl.exe と cmake.exe は vcvarsall を通した後の PATH から引く。
  // vswhere が何も返さなければ今の PATH を見る（開発者用のプロンプトから動かしたとき）。
  const bool named = search.cxx_flag || search.mosa_cxx || search.cxx_env;
  if (search.os == HostOs::windows && !named) {
    if (auto msvc = msvc_environment()) {
      env = std::move(*msvc);
      search.path = split_path_list(lookup(env, "PATH"), search.os);
    }
  }
  auto found = find_toolchain_in(search);
  if (!found) return std::nullopt;
  auto version = probe_version(found->cxx);
  if (!version) return std::nullopt;
  found->cxx_version = std::move(*version);
  found->env = std::move(env);
  return found;
}

}
