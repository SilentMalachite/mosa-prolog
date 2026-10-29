// 計画 Task 5。mosac emit の目録を作る。翻訳のときの読み込みは宣言だけを実行し（TranslationLog）、
// 開いたファイルの原文を main.cpp に埋め込む。単位はモジュールごとに 1 つで、名前は C++ の名前空間
// mosa_gen::m<モジュール> になる。同じモジュールの単位は 1 つしか出さないので、unit() の名前はぶつからない。

#include "codegen.hpp"
#include "emit_state.hpp"
#include "project.hpp"

#include "mosa/path_text.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace mosa::codegen {
namespace {

// MSVC は 1 つの文字列の字面の長さに上限がある。原文はこの大きさで切り、起動時につなぐ。
constexpr std::size_t kChunk = 2048;

struct UnitPlan {
  std::uint32_t module = 0;
  std::size_t file = 0;
};

DisplayNames display_names(const TranslationLog& log, std::string_view root) {
  std::error_code error;
  const auto cwd = canonical_key(std::filesystem::current_path(error));
  DisplayNames names;
  for (std::size_t index = 0; index < log.files.size(); ++index) {
    if (index == 0) {
      names.emplace_back(root);
      continue;
    }
    const auto relative = path_from_utf8(log.files[index].key).lexically_relative(cwd);
    names.push_back(relative.empty() ? log.files[index].key : utf8_from_path(relative));
  }
  return names;
}

// 根のファイルのモジュール（module/2 が無ければ user）を先に、読んだほかのモジュールのファイルを後に並べる。
// 起動時は根を読むと include/1 や use_module/1 がほかのファイルを埋め込みから読むので、後の単位は付けるだけになる。
std::vector<UnitPlan> plan_units(const Impl& machine, const TranslationLog& log) {
  std::uint32_t root = machine.user_module;
  for (const Module& module : machine.modules) {
    if (module.file == log.files.front().key) root = module.name;
  }
  std::vector<UnitPlan> units{UnitPlan{root, 0}};
  for (const Module& module : machine.modules) {
    const auto file = log_file(log, module.file);
    if (module.name == root || !file || log.files[*file].library) continue;
    units.push_back(UnitPlan{module.name, *file});
  }
  return units;
}

std::string unit_stem(const Impl& machine, std::uint32_t module) { return "m" + mangle(machine.atoms.name(module)); }

std::string source_array(std::size_t index, std::string_view text) {
  std::string items;
  std::size_t count = 0;
  for (std::size_t at = 0; at < text.size(); at += kChunk, ++count) {
    items += "    " + string_literal(text.substr(at, kChunk)) + ",\n";
  }
  return "constexpr std::array<std::string_view, " + std::to_string(count) + "> kFile" + std::to_string(index) +
         "{\n" + items + "};\n";
}

std::string main_body(const Impl& machine, const TranslationLog& log, const std::vector<UnitPlan>& units) {
  std::string out = "int main(int argc, char** argv) {\n  auto machine = mosa::Machine::create(std::size_t{1} << 20);\n";
  out += "  mosa::rt::start(*machine, argc, argv);\n";
  for (std::size_t index = 0; index < log.files.size(); ++index) {
    if (log.files[index].library) continue;
    const std::string name = "file" + std::to_string(index);
    out += "  const std::string " + name + " = joined(kFile" + std::to_string(index) + ");\n";
    out += "  mosa::rt::embed(*machine, " + string_literal(log.files[index].key) + ", " + name + ");\n";
  }
  out += "  bool loaded = true;\n";
  for (const UnitPlan& unit : units) {
    out += "  loaded = loaded && mosa::rt::install_unit(*machine, mosa_gen::" + unit_stem(machine, unit.module) +
           "::unit(file" + std::to_string(unit.file) + "));\n";
  }
  return out + "  return mosa::rt::finish(*machine, loaded);\n}\n";
}

// 起動の順は rt::start、embed、install_unit、finish（runtime.hpp）。標準ヘッダも runtime.hpp から得る。
std::string main_text(const Impl& machine, const TranslationLog& log, const std::vector<UnitPlan>& units) {
  std::string out = "// mosac が出した実行ファイルの入口（計画 Task 5）。手で直さない。\n\n";
  for (const UnitPlan& unit : units) out += "#include \"" + unit_stem(machine, unit.module) + ".hpp\"\n";
  out += "\n#include \"mosa/runtime.hpp\"\n\nnamespace {\n\n// 読み込んだファイルの原文。鍵は翻訳のときに正規化したパスである。\n";
  for (std::size_t index = 0; index < log.files.size(); ++index) {
    if (!log.files[index].library) out += source_array(index, log.files[index].text);
  }
  out += "\nstd::string joined(std::span<const std::string_view> parts) {\n  std::string text;\n";
  out += "  for (const std::string_view part : parts) text += part;\n  return text;\n}\n\n}  // namespace\n\n";
  return out + main_body(machine, log, units);
}

// 角括弧の引数は \ も " も $ も展開しない。Windows のパスもそのまま書ける。
std::string cmake_path(std::string_view path) {
  return "[==[" + utf8_from_path(path_from_utf8(path)) + "]==]";
}

std::string cmake_text(const Impl& machine, const std::vector<UnitPlan>& units, std::string_view target,
                       std::string_view runtime_dir) {
  const std::string name{target};
  std::string sources = "main.cpp";
  for (const UnitPlan& unit : units) sources += " " + unit_stem(machine, unit.module) + ".cpp";
  std::string out = "# mosac が出した（計画 Task 5）。手で直さない。\ncmake_minimum_required(VERSION 3.25)\n";
  out += "project(" + name + " LANGUAGES CXX)\n\nset(CMAKE_CXX_STANDARD 20)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
  out += "set(CMAKE_CXX_EXTENSIONS OFF)\n";
  out += "# libmosa と同じ静的な MSVC の実行時ライブラリ（/MT）。mosac build は構成によらず /MT を渡す。\n";
  out += "if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)\n";
  out += "  set(CMAKE_MSVC_RUNTIME_LIBRARY \"MultiThreaded$<$<CONFIG:Debug>:Debug>\")\nendif()\n";
  out += "\n# libmosa と include/ の場所。既定は emit した mosac のビルド木である。\n";
  out += "if(DEFINED ENV{MOSA_RUNTIME_DIR})\n  set(mosa_runtime_default \"$ENV{MOSA_RUNTIME_DIR}\")\nelse()\n";
  out += "  set(mosa_runtime_default " + cmake_path(runtime_dir) + ")\nendif()\n";
  out += "set(MOSA_RUNTIME_DIR \"${mosa_runtime_default}\" CACHE PATH \"libmosa and include/\")\n";
  out += "find_library(MOSA_RUNTIME_LIBRARY NAMES mosa PATHS \"${MOSA_RUNTIME_DIR}\" NO_DEFAULT_PATH REQUIRED)\n\n";
  out += "add_executable(" + name + " " + sources + ")\n";
  out += "target_include_directories(" + name + " PRIVATE \"${MOSA_RUNTIME_DIR}/include\")\n";
  out += "target_link_libraries(" + name + " PRIVATE \"${MOSA_RUNTIME_LIBRARY}\")\n";
  out += "# libmosa の CommandLineToArgvW（src/platform/command_line.cpp）。静的ライブラリからは伝わらない。\n";
  out += "if(WIN32)\n  target_link_libraries(" + name + " PRIVATE shell32)\nendif()\n";
  out += "# 本体と同じ旗。生成コードの日本語の注釈のため MSVC には /utf-8 を渡す。\n";
  out += "target_compile_options(" + name + " PRIVATE\n";
  out += "  $<$<CXX_COMPILER_ID:MSVC>:/W4;/utf-8;/permissive-;/Zc:__cplusplus>\n";
  out += "  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>)\n";
  out += "# 警告を誤りにするのは頼まれたときだけ。新しいコンパイラの警告で利用者のビルドを止めない。\n";
  out += "option(MOSA_WARNINGS_AS_ERRORS \"Treat warnings as errors\" OFF)\n";
  out += "if(\"$ENV{MOSA_WERROR}\" STREQUAL \"1\")\n  set(MOSA_WARNINGS_AS_ERRORS ON)\nendif()\n";
  out += "if(MOSA_WARNINGS_AS_ERRORS)\n";
  return out + "  target_compile_options(" + name + " PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/WX,-Werror>)\nendif()\n";
}

Project fail_with(std::string diagnostic) {
  Project project;
  project.diagnostic = std::move(diagnostic);
  return project;
}

bool any_opaque(const TranslationLog& log) {
  return std::any_of(log.files.begin(), log.files.end(), [](const TranslationLog::File& file) { return file.opaque; });
}

// 読めなかったファイルに節がある述語。生成関数を出さず、起動時の読み込みが IR のまま動かす。
std::vector<PredKey> opaque_preds(const TranslationLog& log) {
  std::vector<PredKey> keys;
  for (const TranslationLog::Site& site : log.sites) {
    if (log.files[site.file].opaque) keys.push_back(site.pred);
  }
  return keys;
}

std::vector<std::string> warnings(Impl& machine, const TranslationLog& log, std::string_view path,
                                  const ProjectOptions& options) {
  const DisplayNames names = display_names(log, path);
  std::vector<std::string> lines = export_warnings(machine, log, names);
  // 読めなかった項の定義は翻訳器に見えない。呼び出しの警告はそれを未定義と取り違えるので出さない。
  if (options.check_calls && !any_opaque(log)) {
    for (std::string& line : call_warnings(machine, log, names)) lines.push_back(std::move(line));
  }
  return lines;
}

Project build_project(Machine& machine, const TranslationLog& log, std::string_view path,
                      const ProjectOptions& options) {
  const Impl& impl = engine_impl(machine);
  const std::vector<UnitPlan> units = plan_units(impl, log);
  const std::vector<PredKey> interpreted = opaque_preds(log);
  Project project;
  project.ok = true;
  for (const UnitPlan& unit : units) {
    const EmitResult emitted = emit_module(machine, unit.module, log.files[unit.file].key, interpreted, options.trace);
    const std::string stem = unit_stem(impl, unit.module);
    project.files.push_back(ProjectFile{stem + ".cpp", emitted.source});
    project.files.push_back(ProjectFile{stem + ".hpp", emitted.header});
  }
  project.files.push_back(ProjectFile{"main.cpp", main_text(impl, log, units)});
  project.files.push_back(ProjectFile{"CMakeLists.txt", cmake_text(impl, units, target_name(path), options.runtime_dir)});
  return project;
}

}  // namespace

// CMake の名前に使える文字だけなら原文の名前、ほかは app にする。
std::string target_name(std::string_view root) {
  const std::string stem = utf8_from_path(path_from_utf8(root).stem());
  bool plain = !stem.empty();
  for (const char unit : stem) {
    const bool alnum = (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') || (unit >= '0' && unit <= '9');
    plain = plain && (alnum || unit == '_' || unit == '-');
  }
  // CMake が予約する目標の名前は configure で通らない。
  constexpr std::array<std::string_view, 5> reserved{"all", "clean", "install", "help", "test"};
  const bool taken = std::find(reserved.begin(), reserved.end(), stem) != reserved.end();
  return plain && !taken ? stem : "app";
}

std::optional<std::size_t> log_file(const TranslationLog& log, std::string_view key) {
  if (key.empty()) return std::nullopt;
  for (std::size_t index = 0; index < log.files.size(); ++index) {
    if (log.files[index].key == key) return index;
  }
  return std::nullopt;
}

Project translate_program(std::string_view path, const ProjectOptions& options) {
  auto machine = Machine::create();
  Impl& impl = engine_impl(*machine);
  TranslationLog log;
  impl.translation = &log;
  try {
    if (!machine->consult(path)) return fail_with(std::string{machine->diagnostic()});
  } catch (const ResourceLimit&) {
    return fail_with(message_text(impl, "heap_limit"));
  }
  impl.translation = nullptr;
  if (log.files.empty()) return fail_with(message_text(impl, "cannot_open_file", {path}));
  std::string diagnostic;
  for (const std::string& line : warnings(impl, log, path, options)) diagnostic += (diagnostic.empty() ? "" : "\n") + line;
  if (!diagnostic.empty()) return fail_with(std::move(diagnostic));
  return build_project(*machine, log, path, options);
}

}  // namespace mosa::codegen
