// 計画 Task 5、Task 10。mosac emit の main.cpp と CMakeLists.txt の文面。
// main は rt::start、同梱ライブラリの bundle_library、embed、install_unit、finish の順（runtime.hpp）。
// 同梱ライブラリの単位は mosac emit --library が lib/<名前>.pl から出した物で、名前空間はモジュールと同じ
// mosa_gen::m<mangle(モジュール)>。main.cpp はその unit() を自分で宣言し、見出しは要らない。

#include "codegen.hpp"
#include "emit_state.hpp"
#include "project.hpp"

#include "mosa/path_text.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::codegen {
namespace {

// MSVC は 1 つの文字列の字面の長さに上限がある。原文はこの大きさで切り、起動時につなぐ。
constexpr std::size_t kChunk = 2048;

std::string source_array(std::size_t index, std::string_view text) {
  std::string items;
  std::size_t count = 0;
  for (std::size_t at = 0; at < text.size(); at += kChunk, ++count) {
    items += "    " + string_literal(text.substr(at, kChunk)) + ",\n";
  }
  return "constexpr std::array<std::string_view, " + std::to_string(count) + "> kFile" + std::to_string(index) +
         "{\n" + items + "};\n";
}

const LibraryPlan* library_of(const ProjectPlan& plan, std::size_t file) {
  for (const LibraryPlan& library : plan.libraries) {
    if (library.file == file) return &library;
  }
  return nullptr;
}

// ファイルごとに、原文の std::string を作ってから、ライブラリは bundle_library、ほかは embed に渡す。
std::string file_lines(const Impl& machine, const TranslationLog& log, const ProjectPlan& plan) {
  std::string out;
  for (std::size_t index = 0; index < log.files.size(); ++index) {
    const std::string name = "file" + std::to_string(index);
    out += "  const std::string " + name + " = joined(kFile" + std::to_string(index) + ");\n";
    if (const LibraryPlan* library = library_of(plan, index)) {
      const std::string unit = library->prebuilt ? "mosa_gen::" + unit_stem(machine, library->module) + "::unit(" + name + ")"
                                                 : name;
      out += "  mosa::rt::bundle_library(" + string_literal(library->name) + ", " + unit + ");\n";
      continue;
    }
    out += "  mosa::rt::embed(*machine, " + string_literal(plan.keys[index]) + ", " + name + ");\n";
  }
  return out;
}

std::string main_body(const Impl& machine, const TranslationLog& log, const ProjectPlan& plan) {
  std::string out = "int main(int argc, char** argv) {\n  auto machine = mosa::Machine::create(std::size_t{1} << 20);\n";
  out += "  mosa::rt::start(*machine, argc, argv);\n" + file_lines(machine, log, plan) + "  bool loaded = true;\n";
  for (const UnitPlan& unit : plan.units) {
    out += "  loaded = loaded && mosa::rt::install_unit(*machine, mosa_gen::" + unit_stem(machine, unit.module) +
           "::unit(file" + std::to_string(unit.file) + "));\n";
  }
  return out + "  return mosa::rt::finish(*machine, loaded);\n}\n";
}

std::string library_declarations(const Impl& machine, const ProjectPlan& plan) {
  std::string out;
  for (const LibraryPlan& library : plan.libraries) {
    if (!library.prebuilt) continue;
    if (out.empty()) out = "// 同梱ライブラリの単位の入口（計画 Task 10）。mosac emit --library が lib/<名前>.pl から出した。\n";
    out += "namespace mosa_gen::" + unit_stem(machine, library.module) +
           " {\n[[nodiscard]] mosa::rt::NativeUnit unit(std::string_view source);\n}\n";
  }
  return out.empty() ? out : out + "\n";
}

// 角括弧の引数は \ も " も $ も展開しない。Windows のパスもそのまま書ける。
std::string cmake_path(std::string_view path) {
  return "[==[" + utf8_from_path(path_from_utf8(path)) + "]==]";
}

// MOSA_RUNTIME_DIR の既定。ビルド木（mosac の隣に include/ がある）ならそこ、インストールの形なら
// <mosac の親>/lib/mosa/<OS>-<arch>-<compiler>/（SPEC 第6節）。鍵の綴りは本体の CMakeLists.txt と mosac の runtime_abi と同じ。
std::string cmake_runtime_default(std::string_view runtime_dir) {
  std::string out = "if(DEFINED ENV{MOSA_RUNTIME_DIR})\n  set(mosa_runtime_default \"$ENV{MOSA_RUNTIME_DIR}\")\n";
  out += "elseif(EXISTS " + cmake_path(std::string{runtime_dir} + "/include/mosa/runtime.hpp") + ")\n";
  out += "  set(mosa_runtime_default " + cmake_path(runtime_dir) + ")\nelse()\n";
  out += "  string(TOLOWER \"${CMAKE_SYSTEM_NAME}\" mosa_os)\n  if(mosa_os STREQUAL \"darwin\")\n    set(mosa_os macos)\n  endif()\n";
  out += "  string(TOLOWER \"${CMAKE_SYSTEM_PROCESSOR}\" mosa_arch)\n";
  out += "  if(mosa_arch MATCHES \"^(amd64|x86_64|x64)$\")\n    set(mosa_arch x86_64)\n";
  out += "  elseif(mosa_arch MATCHES \"^(arm64|aarch64)$\")\n    set(mosa_arch arm64)\n  endif()\n";
  out += "  if(CMAKE_CXX_COMPILER_ID MATCHES \"Clang\")\n    set(mosa_cxx clang)\n";
  out += "  elseif(CMAKE_CXX_COMPILER_ID STREQUAL \"GNU\")\n    set(mosa_cxx gcc)\n";
  out += "  elseif(CMAKE_CXX_COMPILER_ID STREQUAL \"MSVC\")\n    set(mosa_cxx msvc)\n";
  out += "  else()\n    string(TOLOWER \"${CMAKE_CXX_COMPILER_ID}\" mosa_cxx)\n  endif()\n";
  // 角括弧の引数には続けて書けないので、いちど変数に受けてから鍵を足す。
  out += "  set(mosa_runtime_base " + cmake_path(std::string{runtime_dir} + "/../lib/mosa") + ")\n";
  out += "  set(mosa_runtime_default \"${mosa_runtime_base}/${mosa_os}-${mosa_arch}-${mosa_cxx}\")\nendif()\n";
  return out;
}

std::string cmake_units(const ProjectPlan& plan, const std::string& name) {
  std::string out = "# 同梱ライブラリの翻訳済みの単位（計画 Task 10）。use_module された物と文面の表だけを結ぶ。\n";
  out += "set(mosa_units)\n";
  for (std::size_t index = 0; index < plan.libraries.size(); ++index) {
    if (!plan.libraries[index].prebuilt) continue;
    const std::string variable = "MOSA_UNIT_" + std::to_string(index);
    out += "find_library(" + variable + " NAMES mosa_pl_" + plan.libraries[index].name +
           " PATHS \"${MOSA_RUNTIME_DIR}/pl\" NO_DEFAULT_PATH REQUIRED)\n";
    out += "list(APPEND mosa_units \"${" + variable + "}\")\n";
  }
  return out + "target_link_libraries(" + name + " PRIVATE ${mosa_units} \"${MOSA_RUNTIME_LIBRARY}\")\n";
}

}  // namespace

std::string unit_stem(const Impl& machine, std::uint32_t module) { return "m" + mangle(machine.atoms.name(module)); }

std::string main_text(const Impl& machine, const TranslationLog& log, const ProjectPlan& plan) {
  std::string out = "// mosac が出した実行ファイルの入口（計画 Task 5、Task 10）。手で直さない。\n\n";
  for (const UnitPlan& unit : plan.units) out += "#include \"" + unit_stem(machine, unit.module) + ".hpp\"\n";
  out += "\n#include \"mosa/runtime.hpp\"\n\n" + library_declarations(machine, plan);
  out += "namespace {\n\n// 読み込んだファイルの原文。鍵は根のファイルからの相対か、同梱ライブラリの library/<名前>.pl である。\n";
  for (std::size_t index = 0; index < log.files.size(); ++index) out += source_array(index, log.files[index].text);
  out += "\nstd::string joined(std::span<const std::string_view> parts) {\n  std::string text;\n";
  out += "  for (const std::string_view part : parts) text += part;\n  return text;\n}\n\n}  // namespace\n\n";
  return out + main_body(machine, log, plan);
}

std::string cmake_text(const Impl& machine, const ProjectPlan& plan, std::string_view target,
                       std::string_view runtime_dir) {
  const std::string name{target};
  std::string sources = "main.cpp";
  for (const UnitPlan& unit : plan.units) sources += " " + unit_stem(machine, unit.module) + ".cpp";
  std::string out = "# mosac が出した（計画 Task 5）。手で直さない。\ncmake_minimum_required(VERSION 3.25)\n";
  out += "project(" + name + " LANGUAGES CXX)\n\nset(CMAKE_CXX_STANDARD 20)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
  out += "set(CMAKE_CXX_EXTENSIONS OFF)\n";
  out += "# libmosa と同じ静的な MSVC の実行時ライブラリ（/MT）。mosac build は構成によらず /MT を渡す。\n";
  out += "if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)\n";
  out += "  set(CMAKE_MSVC_RUNTIME_LIBRARY \"MultiThreaded$<$<CONFIG:Debug>:Debug>\")\nendif()\n";
  out += "\n# libmosa と include/ と pl/ の場所。既定は emit した mosac の形（ビルド木かインストール）で決める。\n";
  out += cmake_runtime_default(runtime_dir);
  out += "set(MOSA_RUNTIME_DIR \"${mosa_runtime_default}\" CACHE PATH \"libmosa, include/ and pl/\")\n";
  out += "find_library(MOSA_RUNTIME_LIBRARY NAMES mosa PATHS \"${MOSA_RUNTIME_DIR}\" NO_DEFAULT_PATH REQUIRED)\n\n";
  out += "add_executable(" + name + " " + sources + ")\n";
  out += "target_include_directories(" + name + " PRIVATE \"${MOSA_RUNTIME_DIR}/include\")\n";
  out += cmake_units(plan, name);
  out += "# libmosa の CommandLineToArgvW（src/platform/command_line.cpp）。静的ライブラリからは伝わらない。\n";
  out += "if(WIN32)\n  target_link_libraries(" + name + " PRIVATE shell32)\nendif()\n";
  out += "# 本体と同じ旗。生成コードの日本語の注釈のため MSVC には /utf-8 を渡す。/EHsc は mosac build が\n";
  out += "# CMAKE_CXX_FLAGS を空にしても残るよう、ここに置く（直接の道の旗と同じ）。\n";
  out += "target_compile_options(" + name + " PRIVATE\n";
  out += "  $<$<CXX_COMPILER_ID:MSVC>:/EHsc;/W4;/utf-8;/permissive-;/Zc:__cplusplus>\n";
  out += "  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>)\n";
  out += "# 警告を誤りにするのは頼まれたときだけ。新しいコンパイラの警告で利用者のビルドを止めない。\n";
  out += "option(MOSA_WARNINGS_AS_ERRORS \"Treat warnings as errors\" OFF)\n";
  out += "if(\"$ENV{MOSA_WERROR}\" STREQUAL \"1\")\n  set(MOSA_WARNINGS_AS_ERRORS ON)\nendif()\n";
  out += "if(MOSA_WARNINGS_AS_ERRORS)\n";
  return out + "  target_compile_options(" + name + " PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/WX,-Werror>)\nendif()\n";
}

}  // namespace mosa::codegen
