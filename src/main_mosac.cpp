// mosac（SPEC 第6節）。この段では --version と emit と toolchain だけを受ける。build と run は計画 Task 9。
// 文面は lib/messages_*.pl の鍵（ADR 0007）。翻訳と目録の組み立ては src/codegen/project.cpp。

#include "codegen/codegen.hpp"

#include "mosa/command_line.hpp"
#include "mosa/console.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/toolchain.hpp"
#include "mosa/version.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct EmitArgs {
  std::string source;
  std::string out;
};

// mosac emit app.pl -o out/ の形だけを受ける。
std::optional<EmitArgs> parse_emit(const std::vector<std::string>& args) {
  EmitArgs parsed;
  for (std::size_t index = 2; index < args.size(); ++index) {
    if (args[index] == "-o" && index + 1 < args.size()) {
      parsed.out = args[++index];
    } else if (parsed.source.empty() && !args[index].empty() && args[index].front() != '-') {
      parsed.source = args[index];
    } else {
      return std::nullopt;
    }
  }
  if (parsed.source.empty() || parsed.out.empty()) return std::nullopt;
  return parsed;
}

std::string message(std::string_view key, std::vector<std::string> args = {}) {
  return mosa::render_message_fresh(mosa::default_message_language(), key, args);
}

// 失敗した書き出しのパスを返す。途中まで書いた目録は残るが、次の emit が上書きする。
std::optional<std::string> write_files(const std::filesystem::path& out, const std::vector<mosa::codegen::ProjectFile>& files) {
  std::error_code error;
  std::filesystem::create_directories(out, error);
  if (error) return mosa::utf8_from_path(out);
  for (const mosa::codegen::ProjectFile& file : files) {
    const std::filesystem::path target = out / mosa::path_from_utf8(file.name);
    std::ofstream stream{target, std::ios::binary};
    stream << file.text;
    if (!stream) return mosa::utf8_from_path(target);
  }
  return std::nullopt;
}

// libmosa と include/ の場所は mosac toolchain と同じ一つの決め方による（計画 Task 10 でインストールの形に変わる）。
std::string runtime_dir() { return mosa::utf8_from_path(mosa::default_runtime_dir()); }

int run_emit(const std::vector<std::string>& args) {
  const auto parsed = parse_emit(args);
  if (!parsed) {
    std::cerr << message("mosac_usage") << '\n';
    return 1;
  }
  mosa::codegen::ProjectOptions options;
  options.runtime_dir = runtime_dir();
  const mosa::codegen::Project project = mosa::codegen::translate_program(parsed->source, options);
  // Prolog の誤りは翻訳の段で止め、何も書かずコンパイラも起こさない（SPEC 第6節のエラーの扱い）。
  if (!project.ok) {
    std::cerr << project.diagnostic << '\n';
    return 1;
  }
  if (const auto failed = write_files(mosa::path_from_utf8(parsed->out), project.files)) {
    std::cerr << message("mosac_cannot_write", {*failed}) << '\n';
    return 1;
  }
  return 0;
}

// mosac toolchain [--cxx X] の形だけを受ける。
std::optional<mosa::ToolchainQuery> parse_toolchain(const std::vector<std::string>& args) {
  mosa::ToolchainQuery query;
  for (std::size_t index = 2; index < args.size(); ++index) {
    if (args[index] != "--cxx" || index + 1 >= args.size()) return std::nullopt;
    query.cxx = args[++index];
  }
  return query;
}

std::string_view install_hint(mosa::HostOs os) {
  switch (os) {
    case mosa::HostOs::macos:
      return "toolchain_hint_macos";
    case mosa::HostOs::linux:
      return "toolchain_hint_linux";
    case mosa::HostOs::windows:
      return "toolchain_hint_windows";
  }
  return "toolchain_hint_linux";
}

// 見つかれば場所と版を標準出力へ、見つからなければ導入手順を標準エラーへ出して 1（SPEC 第6節の環境の問題）。
int run_toolchain(const std::vector<std::string>& args) {
  const auto query = parse_toolchain(args);
  if (!query) {
    std::cerr << message("mosac_usage") << '\n';
    return 1;
  }
  const auto found = mosa::find_toolchain(*query);
  if (!found) {
    // 名指しのコンパイラが無いのは綴りか設定の誤りで、導入手順は答えにならない。
    if (const auto named = mosa::named_compiler(mosa::host_toolchain_search(*query))) {
      std::cerr << message("toolchain_named_not_found", {named->program, std::string{named->source}}) << '\n';
    } else {
      std::cerr << message("toolchain_not_found") << '\n' << message(install_hint(mosa::host_os())) << '\n';
    }
    return 1;
  }
  std::cout << message("toolchain_compiler", {mosa::utf8_from_path(found->cxx)}) << '\n'
            << message("toolchain_version", {found->cxx_version}) << '\n'
            << (found->cmake ? message("toolchain_cmake", {mosa::utf8_from_path(*found->cmake)})
                             : message("toolchain_cmake_missing"))
            << '\n'
            << message("toolchain_runtime", {mosa::utf8_from_path(found->runtime_dir)}) << '\n';
  if (!found->env.empty()) std::cout << message("toolchain_msvc_env", {std::to_string(found->env.size())}) << '\n';
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  mosa::prepare_console();
  const std::vector<std::string> args = mosa::command_line_utf8(argc, argv);
  for (std::size_t index = 1; index < args.size(); ++index) {
    if (args[index] == "--version") {
      std::cout << mosa::version_line() << '\n';
      return 0;
    }
  }
  if (args.size() >= 2 && args[1] == "emit") return run_emit(args);
  if (args.size() >= 2 && args[1] == "toolchain") return run_toolchain(args);
  std::cerr << message("mosac_unavailable") << '\n';
  return 1;
}
