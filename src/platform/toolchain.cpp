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

// 実行時ライブラリの置き場所 <OS>-<arch>-<compiler> の arch（計画 Task 10）。綴りは本体の CMakeLists.txt が
// CMAKE_SYSTEM_PROCESSOR から作る物と同じにする。交差コンパイルは目標外なので、mosac 自身の arch でよい。
#if defined(__aarch64__) || defined(_M_ARM64)
std::string_view host_arch() { return "arm64"; }
#elif defined(__x86_64__) || defined(_M_X64)
std::string_view host_arch() { return "x86_64"; }
#else
std::string_view host_arch() { return "unknown"; }
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
  std::filesystem::path found = path;
  if (os == HostOs::windows && !path.has_extension()) found += ".exe";
  if (!is_file(found)) return std::nullopt;
  // 相対の PATH の項目や --cxx bin/g++ で見つけたものも、作業場所を移して起こすビルドで同じものを指すよう絶対にする。
  std::error_code error;
  const std::filesystem::path absolute = std::filesystem::absolute(found, error);
  return error ? found : absolute;
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

std::optional<NamedCompiler> named_compiler(const ToolchainSearch& search) {
  if (search.cxx_flag) return NamedCompiler{*search.cxx_flag, "--cxx"};
  if (search.mosa_cxx) return NamedCompiler{*search.mosa_cxx, "MOSA_CXX"};
  if (search.cxx_env) return NamedCompiler{*search.cxx_env, "CXX"};
  return std::nullopt;
}

std::optional<Toolchain> find_toolchain_in(const ToolchainSearch& search) {
  Toolchain toolchain;
  // 名前を与えられた最初のものだけを見る。見つからないときに既定へ落ちると、頼んでいないコンパイラでビルドしてしまう。
  std::optional<std::filesystem::path> cxx;
  if (const auto named = named_compiler(search)) {
    cxx = resolve_program(named->program, search.path, search.os);
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
    // 空の項目（POSIX では作業場所の意味）はわざと捨てる。どこで動かしたかでコンパイラが変わらないように。
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

// cmd は / を切り替えの印と読み、C:/a/b.bat を命令 C: と引数に分ける（CI run 40890625972 の Windows で vcvarsall の
// set が空になり cl が見つからなかった）。cmd に渡すパスだけは OS の区切りのまま出す。
// 環境変数から作ったパス（TMP=C:/Temp など）は / を含みうるので、写しを make_preferred で OS の区切りにする。
std::string native_path_text(const std::filesystem::path& path) {
  std::filesystem::path native = path;
  native.make_preferred();
  const std::u8string bytes = native.u8string();
  std::string text;
  text.reserve(bytes.size());
  for (const char8_t byte : bytes) text.push_back(static_cast<char>(byte));
  return text;
}

// cl を引数なしで起こしてファイルに向けると、使い方の行が見出しより先に出る。Microsoft で始まる最初の行を見出しとし、
// 無ければ最初の空でない行。
std::string msvc_banner_line(std::string_view text) {
  std::string banner;
  each_line(text, [&](std::string_view line) {
    if (banner.empty() && trimmed(line).starts_with("Microsoft")) banner = trimmed(line);
  });
  return banner.empty() ? first_nonempty_line(text) : banner;
}

namespace {

struct Captured {
  int code = 0;
  std::string text;
};

void append_utf8(std::string& out, std::uint32_t code) {
  const auto unit = [&](std::uint32_t value) { out.push_back(static_cast<char>(value)); };
  if (code < 0x80) {
    unit(code);
  } else if (code < 0x800) {
    unit(0xC0 | code >> 6);
    unit(0x80 | (code & 0x3F));
  } else if (code < 0x10000) {
    unit(0xE0 | code >> 12);
    unit(0x80 | (code >> 6 & 0x3F));
    unit(0x80 | (code & 0x3F));
  } else {
    unit(0xF0 | code >> 18);
    unit(0x80 | (code >> 12 & 0x3F));
    unit(0x80 | (code >> 6 & 0x3F));
    unit(0x80 | (code & 0x3F));
  }
}

// 同時に動く mosac どうしで名前がぶつからないよう、時刻と乱数と回数を混ぜる。
// 一時の場所が取れなければ nullopt。作業場所に相対の名前で書き散らさない。
std::optional<std::filesystem::path> scratch_file(std::string_view extension) {
  static std::atomic<std::uint32_t> counter{0};
  std::error_code error;
  const std::filesystem::path dir = std::filesystem::temp_directory_path(error);
  if (error || dir.empty()) return std::nullopt;
  const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::string name = "mosac-" + std::to_string(ticks) + "-" + std::to_string(std::random_device{}()) + "-" +
                           std::to_string(counter++) + std::string{extension};
  return dir / name;
}

// run_process は出力をファイルにしか向けられない。そこへ書かせて読み、消す。
std::optional<Captured> capture(std::span<const std::string> argv) {
  const auto scratch = scratch_file(".log");
  if (!scratch) return std::nullopt;
  const std::filesystem::path& log = *scratch;
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

// gcc と clang は --version の 1 行目。cl は --version を知らず、引数なしで標準エラーに使い方と見出しを出して 0 以外で
// 終わるので、終了コードを見ずに見出しの行（msvc_banner_line）を取る。起こせないか 0 以外で終わる gcc/clang は
// 使えないものとして扱う（macOS の /usr/bin/clang++ は Command Line Tools が無いと案内を出して失敗するだけの殻）。
std::optional<std::string> probe_version(const std::filesystem::path& cxx) {
  const bool msvc = is_msvc(cxx);
  std::vector<std::string> argv{utf8_from_path(cxx)};
  if (!msvc) argv.emplace_back("--version");
  const auto captured = capture(argv);
  if (!captured || (!msvc && captured->code != 0)) return std::nullopt;
  std::string line = msvc ? msvc_banner_line(captured->text) : first_nonempty_line(captured->text);
  if (line.empty()) return std::nullopt;
  return line;
}

}  // namespace

bool is_msvc(const std::filesystem::path& cxx) { return same_name(utf8_from_path(cxx.stem()), "cl"); }

// 名指しの cl は vcvarsall を通さない（持ち主の決定 A、2026-10-30）。INCLUDE が無ければ標準ヘッダが見つからず、
// コンパイルの失敗が「内部エラー」に見えるので、その前に開発者用のプロンプトを案内する。
bool needs_developer_prompt(const std::filesystem::path& cxx, bool has_vcvars_env, bool include_set) {
  return is_msvc(cxx) && !has_vcvars_env && !include_set;
}

// cmd は引用の中でも %NAME% を展開し、" は引用を閉じるので、この 2 つを含むパスは .bat に書けない。
// ( ) & は call の行の引用の中なら文字のまま渡り、実際の VS は Program Files (x86) の下にあるので拒まない。
bool cmd_quotable_path(const std::filesystem::path& path) {
  return native_path_text(path).find_first_of("%\"") == std::string::npos;
}

// cmd は .bat を OEM の符号ページで読むが、中身は ASCII の命令と vcvarsall のパスだけである。set の出力は
// cmd /u で UTF-16LE にさせて読む（vcvars_environment）。OEM のままだと日本語の TEMP や USERPROFILE が化ける。
std::string vcvars_script(const std::filesystem::path& vcvarsall) {
  return "@echo off\r\ncall \"" + native_path_text(vcvarsall) + "\" x64 >nul\r\nif errorlevel 1 exit /b 1\r\nset\r\n";
}

// 対になっていないサロゲートは U+FFFD にする。奇数の長さの最後の 1 バイトは捨てる。
std::string utf8_from_utf16le(std::string_view bytes) {
  std::string out;
  const auto unit_at = [&](std::size_t index) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index])) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[index + 1])) << 8;
  };
  for (std::size_t at = 0; at + 1 < bytes.size(); at += 2) {
    std::uint32_t code = unit_at(at);
    const bool high = code >= 0xD800 && code < 0xDC00;
    const std::uint32_t next = at + 3 < bytes.size() ? unit_at(at + 2) : 0;
    if (high && next >= 0xDC00 && next < 0xE000) {
      code = 0x10000 + ((code - 0xD800) << 10) + (next - 0xDC00);
      at += 2;
    } else if (code >= 0xD800 && code < 0xE000) {
      code = 0xFFFD;
    }
    append_utf8(out, code);
  }
  return out;
}

namespace {

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

// 素の "cmd" は作業場所の cmd.exe を先に拾いうる。ComSpec、無ければ SystemRoot の下の決まった場所だけを使う。
std::optional<std::filesystem::path> command_shell() {
  if (const auto comspec = environment_path("ComSpec")) return comspec;
  const auto root = environment_path("SystemRoot");
  if (!root) return std::nullopt;
  return *root / "System32" / "cmd.exe";
}

// vcvarsall.bat を呼んだ後の set を取る。cmd の引用の規則は run_process の引用（CommandLineToArgvW の逆）と違い、
// "..." を含む 1 つの命令を渡すと \" に化ける。そこで命令は一時の .bat に書き、cmd にはそのパスだけを渡す。
// cmd に見せるパスは native_path_text（\ のまま）。/ にそろえた形だと cmd が C: で切って命令と読む。
// set の出力は cmd /u で UTF-16LE にし、utf8_from_utf16le で読む。x64 は固定である。
std::optional<Environment> vcvars_environment(const std::filesystem::path& vcvarsall) {
  // 書けないパスなら一時のパスと同じく諦め、呼び手は今の PATH を見る。
  if (!is_file(vcvarsall) || !cmd_quotable_path(vcvarsall)) return std::nullopt;
  const auto scratch = scratch_file(".bat");
  // cmd /c は引数の中の & ^ ( ) % ! < > | を命令として読む。そういうパスなら諦め、呼び手は今の PATH を見る。
  if (!scratch || utf8_from_path(*scratch).find_first_of("&^()%!<>|") != std::string::npos) return std::nullopt;
  const std::filesystem::path& script = *scratch;
  const auto shell = command_shell();
  if (!shell) return std::nullopt;
  {
    std::ofstream out{script, std::ios::binary};
    out << vcvars_script(vcvarsall);
    if (!out) return std::nullopt;
  }
  const std::vector<std::string> argv{native_path_text(*shell), "/d", "/u", "/c", native_path_text(script)};
  const auto dumped = capture(argv);
  std::error_code error;
  std::filesystem::remove(script, error);
  if (!dumped || dumped->code != 0) return std::nullopt;
  Environment env = parse_set_dump(utf8_from_utf16le(dumped->text));
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
                                      "installationPath", "-utf8"};
  const auto listed = capture(argv);
  if (!listed || listed->code != 0) return std::nullopt;
  const auto install = parse_vswhere_output(listed->text);
  if (!install) return std::nullopt;
  return vcvars_environment(*install / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat");
}

}  // namespace

ToolchainSearch host_toolchain_search(const ToolchainQuery& query) {
  ToolchainSearch search;
  search.cxx_flag = query.cxx;
  search.mosa_cxx = environment_text("MOSA_CXX");
  search.cxx_env = environment_text("CXX");
  search.os = host_os();
  if (const auto path = environment_text("PATH")) search.path = split_path_list(*path, search.os);
  search.runtime_dir = default_runtime_dir();
  return search;
}

std::optional<Toolchain> find_toolchain(const ToolchainQuery& query) {
  ToolchainSearch search = host_toolchain_search(query);
  Environment env;
  // Windows の既定だけは vswhere で探し、cl.exe と cmake.exe は vcvarsall を通した後の PATH から引く。
  // vswhere が何も返さなければ今の PATH を見る（開発者用のプロンプトから動かしたとき）。
  if (search.os == HostOs::windows && !named_compiler(search)) {
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
  // mosac build が CMake の版で道を選ぶ（3.25 未満は直接の道、計画 Task 9）。
  if (found->cmake) {
    const std::vector<std::string> argv{utf8_from_path(*found->cmake), "--version"};
    const auto captured = capture(argv);
    if (captured && captured->code == 0) found->cmake_version = first_nonempty_line(captured->text);
  }
  return found;
}

}
