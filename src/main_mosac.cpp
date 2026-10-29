// mosac（SPEC 第6節）。--version、emit、toolchain、build、run を受ける。dist は計画 Task 11 以降。
// 文面は lib/messages_*.pl の鍵（ADR 0007）。翻訳と目録の組み立ては src/codegen/project.cpp、
// build と run のコンパイルとキャッシュは src/codegen/build.cpp と cache.cpp。

#include "codegen/build.hpp"
#include "codegen/codegen.hpp"

#include "mosa/command_line.hpp"
#include "mosa/console.hpp"
#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/process.hpp"
#include "mosa/toolchain.hpp"
#include "mosa/version.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

struct EmitArgs {
  std::string source;
  std::string out;
  bool library = false;  // --library: lib/<名前>.pl の単位 <名前>.cpp だけを出す（計画 Task 10。本体の CMake が使う）
  bool units = false;    // --units: 試験のディレクトリをファイルごとの単位にし、差分の実行ファイルを出す（Task 11。mosatest --diff が使う）
};

// mosac emit [--library | --units] app.pl -o out/ の形だけを受ける。
std::optional<EmitArgs> parse_emit(const std::vector<std::string>& args) {
  EmitArgs parsed;
  for (std::size_t index = 2; index < args.size(); ++index) {
    if (args[index] == "-o" && index + 1 < args.size()) {
      parsed.out = args[++index];
    } else if (args[index] == "--library" && !parsed.units) {
      parsed.library = true;
    } else if (args[index] == "--units" && !parsed.library) {
      parsed.units = true;
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
  return mosa::codegen::mosac_message(key, std::move(args));
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

// mosac の在るディレクトリ。生成する CMakeLists.txt が実行時ライブラリの既定の場所をここから決め、翻訳器が
// 同梱ライブラリの原文 lib/<名前>.pl を比べる（計画 Task 10）。
std::string runtime_dir() { return mosa::utf8_from_path(mosa::default_runtime_dir()); }

int run_emit(const std::vector<std::string>& args) {
  const auto parsed = parse_emit(args);
  if (!parsed) {
    std::cerr << message("mosac_usage") << '\n';
    return 1;
  }
  mosa::codegen::ProjectOptions options;
  options.runtime_dir = runtime_dir();
  options.library = parsed->library;
  const mosa::codegen::Project project = parsed->units ? mosa::codegen::translate_units(parsed->source, options)
                                                       : mosa::codegen::translate_program(parsed->source, options);
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

// 見つかれば場所と版を標準出力へ、見つからなければ導入手順を標準エラーへ出して 1（SPEC 第6節の環境の問題）。
int run_toolchain(const std::vector<std::string>& args) {
  const auto query = parse_toolchain(args);
  if (!query) {
    std::cerr << message("mosac_usage") << '\n';
    return 1;
  }
  const auto found = mosa::find_toolchain(*query);
  if (!found) {
    mosa::codegen::report_missing_toolchain(*query);
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

struct BuildArgs {
  mosa::codegen::BuildRequest request;
  std::string out;
  bool keep = false;
  std::vector<std::string> program_args;
};

// 旗は原文の前にも後にも置ける。run は -- より後ろを実行ファイルの引数にし、build は -o を受ける。
std::optional<BuildArgs> parse_build(const std::vector<std::string>& args, bool run) {
  BuildArgs parsed;
  for (std::size_t index = 2; index < args.size(); ++index) {
    const std::string& arg = args[index];
    const bool has_value = index + 1 < args.size();
    if (run && arg == "--") {
      parsed.program_args.assign(args.begin() + static_cast<std::ptrdiff_t>(index) + 1, args.end());
      break;
    }
    if (!run && arg == "-o" && has_value) {
      parsed.out = args[++index];
    } else if (arg == "--cxx" && has_value) {
      parsed.request.cxx = args[++index];
    } else if (arg == "--debug" || arg == "--verbose" || arg == "--clean" || arg == "--runtime-source" ||
               (run && arg == "--keep") || (!run && arg == "--interactive")) {
      parsed.request.debug = parsed.request.debug || arg == "--debug";
      parsed.request.verbose = parsed.request.verbose || arg == "--verbose";
      parsed.request.clean = parsed.request.clean || arg == "--clean";
      parsed.request.runtime_source = parsed.request.runtime_source || arg == "--runtime-source";
      parsed.keep = parsed.keep || arg == "--keep";
      parsed.request.interactive = parsed.request.interactive || arg == "--interactive";
    } else if (parsed.request.source.empty() && !arg.empty() && arg.front() != '-') {
      parsed.request.source = arg;
    } else {
      return std::nullopt;
    }
  }
  if (parsed.request.source.empty()) return std::nullopt;
  return parsed;
}

// mosac build app.pl [-o app] [--interactive]。-o が無ければ原文の語幹を今の場所に作る。中間物はいつも .mosac-cache に
// 残すので --keep は run だけが受ける。--interactive は build だけが受け（作った物を利用者が動かす）、run は受けない。
int run_build(const std::vector<std::string>& args) {
  const auto parsed = parse_build(args, false);
  if (!parsed) {
    std::cerr << message("mosac_usage") << '\n';
    return 1;
  }
  const mosa::codegen::Built built = mosa::codegen::build_cached(parsed->request);
  if (built.code != 0) return built.code;
  const std::string out = parsed->out.empty() ? mosa::utf8_from_path(mosa::path_from_utf8(parsed->request.source).stem())
                                              : parsed->out;
  const std::filesystem::path target = mosa::codegen::with_exe_suffix(mosa::path_from_utf8(out));
  std::error_code error;
  std::filesystem::copy_file(built.exe, target, std::filesystem::copy_options::overwrite_existing, error);
  if (error) {
    mosa::codegen::report_write_error(target, error);
    return 1;
  }
  return 0;
}

// mosac run app.pl -- 引数...。実行ファイルを一時ディレクトリに写して起こし、終了コードをそのまま返す。
int run_run(const std::vector<std::string>& args) {
  const auto parsed = parse_build(args, true);
  if (!parsed) {
    std::cerr << message("mosac_usage") << '\n';
    return 1;
  }
  const mosa::codegen::Built built = mosa::codegen::build_cached(parsed->request);
  if (built.code != 0) return built.code;
  std::error_code error;
  std::error_code ignored;
  const auto temp = mosa::codegen::fresh_temp_dir(error);
  if (!temp) {
    mosa::codegen::report_write_error(std::filesystem::temp_directory_path(ignored), error);
    return 1;
  }
  const std::filesystem::path exe = *temp / built.exe.filename();
  std::filesystem::copy_file(built.exe, exe, error);
  if (error) {
    mosa::codegen::report_write_error(exe, error);
    std::filesystem::remove_all(*temp, ignored);
    return 1;
  }
  std::vector<std::string> argv{mosa::utf8_from_path(exe)};
  argv.insert(argv.end(), parsed->program_args.begin(), parsed->program_args.end());
  const std::optional<int> code = mosa::run_process(argv, {});
  if (parsed->keep) {
    std::cerr << message("mosac_kept", {mosa::utf8_from_path(*temp)}) << '\n';
  } else {
    std::filesystem::remove_all(*temp, error);
  }
  if (!code) {
    std::cerr << message("mosac_cannot_start", {argv.front()}) << '\n';
    return 1;
  }
  return *code;
}

}  // namespace

int main(int argc, char** argv) {
  mosa::prepare_console();
  const std::vector<std::string> args = mosa::command_line_utf8(argc, argv);
  // --version は副命令の位置だけで見る。mosac run app.pl -- --version の --version はプログラムの引数である。
  if (args.size() >= 2 && args[1] == "--version") {
    std::cout << mosa::version_line() << '\n';
    return 0;
  }
  if (args.size() >= 2 && args[1] == "emit") return run_emit(args);
  if (args.size() >= 2 && args[1] == "toolchain") return run_toolchain(args);
  if (args.size() >= 2 && args[1] == "build") return run_build(args);
  if (args.size() >= 2 && args[1] == "run") return run_run(args);
  std::cerr << message("mosac_unavailable") << '\n';
  return 1;
}
