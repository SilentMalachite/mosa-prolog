// レビュー 2026-11-20 の H-27（ADR 0030）。プログラムの単位の名前空間とファイル名は、モジュール名ではなく位置から
// 付ける。前は m<mangle(モジュール)> で、utable はキーワード mutable に、ain と 'AIN' は入口の main.cpp に（APFS と
// NTFS は大文字小文字を区別しない）、foo と 'Foo' は 1 つのファイルになり、漢字 30 字の名前は 255 バイトを超える
// ファイル名になった。ここはコンパイラを起こさず、どのファイルシステムでも同じに目録の名前を見る。
// 実際のビルドは tests/mosac/build_test.cmake の module_names。
#include "harness.hpp"

#include "codegen/codegen.hpp"
#include "mosa/path_text.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>

namespace {

using mosa::codegen::Project;
using mosa_test::check;

bool has(const std::string& text, std::string_view part) { return text.find(part) != std::string::npos; }

void write_file(const std::filesystem::path& path, std::string_view text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out{path, std::ios::binary};
  out << text;
}

const std::string* text_of(const Project& project, std::string_view name) {
  for (const auto& file : project.files) {
    if (file.name == name) return &file.text;
  }
  return nullptr;
}

// ASCII の大文字だけを小文字にする。APFS と NTFS の既定が同じファイルとみなす名前は、少なくともこれで等しくなる。
std::string folded(std::string_view name) {
  std::string out{name};
  for (char& unit : out) {
    if (unit >= 'A' && unit <= 'Z') unit = static_cast<char>(unit - 'A' + 'a');
  }
  return out;
}

// 目録の名前は大文字小文字をそろえても互いに異なり（入口の main.cpp ともぶつからず）、255 バイトに収まる。
void check_names(const Project& project, const std::string& what) {
  std::set<std::string> seen;
  for (const auto& file : project.files) {
    check(seen.insert(folded(file.name)).second, what + ": two files fold to " + folded(file.name));
    check(file.name.size() <= 255, what + ": a file name over 255 bytes: " + file.name);
  }
}

// 単位 space のファイルがあり、その名前空間が mosa_gen::<space> である。
void check_unit(const Project& project, const std::string& space) {
  const std::string* source = text_of(project, space + ".cpp");
  const std::string* header = text_of(project, space + ".hpp");
  check(source != nullptr && has(*source, "namespace mosa_gen::" + space + " {"), "unit " + space + ".cpp");
  check(header != nullptr && has(*header, "namespace mosa_gen::" + space + " {"), "unit " + space + ".hpp");
}

const std::string k_kanji = [] {
  std::string name;
  for (int count = 0; count < 15; ++count) name += "漢字";
  return name;
}();

// mosac emit、build、run の目録。単位 k は plan_units の位置（0 が根のファイルのモジュール user）。
void check_program() {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/h27_names";
  std::filesystem::remove_all(dir);
  const std::array<std::string, 6> modules{"utable", "ain", "AIN", "foo", "Foo", k_kanji};
  std::string app;
  for (std::size_t index = 0; index < modules.size(); ++index) {
    const std::string file = "m" + std::to_string(index);
    write_file(dir / (file + ".pl"), ":- module('" + modules[index] + "', [p" + file + "/0]).\np" + file + ".\n");
    app += ":- use_module(" + file + ").\n";
  }
  write_file(dir / "app.pl", app + "main :- pm0, pm1, pm2, pm3, pm4, pm5.\n");
  const Project project = mosa::codegen::translate_program(mosa::utf8_from_path(dir / "app.pl"), {});
  check(project.ok, "h27 program translates: " + project.diagnostic);
  check_names(project, "h27 program");
  const std::string* main_text = text_of(project, "main.cpp");
  check(main_text != nullptr, "h27 program has main.cpp");
  if (main_text == nullptr) return;
  check(has(*main_text, "#include \"u0.hpp\"  // user\n"), "h27 root unit u0 names its module");
  check(has(*main_text, "mosa::rt::install_unit(*machine, mosa_gen::u0::unit(file0))"), "h27 root unit installed");
  check_unit(project, "u0");
  for (std::size_t index = 0; index < modules.size(); ++index) {
    const std::string space = "u" + std::to_string(index + 1);
    check_unit(project, space);
    check(has(*main_text, "#include \"" + space + ".hpp\"  // " + modules[index] + "\n"), space + " names its module");
    check(has(*main_text, "mosa_gen::" + space + "::unit(file"), space + " installed");
  }
}

// mosac emit --units。試験のファイル N が読んだプログラムのファイルの位置 i から u<N>_<i>（0 が試験のファイル）。
void check_units() {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/h27_units";
  std::filesystem::remove_all(dir);
  write_file(dir / "fix/lower.pl", ":- module(foo, [lo/1]).\nlo(1).\n");
  write_file(dir / "fix/upper.pl", ":- module('Foo', [up/1]).\nup(2).\n");
  write_file(dir / "case.pl", ":- use_module('fix/lower').\n:- use_module('fix/upper').\n"
                              "test(h27_case, (lo(X), up(Y)), solutions([(X = 1, Y = 2)])).\n");
  const Project project = mosa::codegen::translate_units(mosa::utf8_from_path(dir), {});
  check(project.ok, "h27 units translate: " + project.diagnostic);
  check_names(project, "h27 units");
  const std::string* main_text = text_of(project, "main.cpp");
  check(main_text != nullptr, "h27 units have main.cpp");
  if (main_text == nullptr) return;
  for (const std::string space : {"u0_0", "u0_1", "u0_2"}) {
    check_unit(project, space);
    check(has(*main_text, "&mosa_gen::" + space + "::unit"), space + " in the table");
  }
}

}  // namespace

int main() {
  check_program();
  check_units();
  return mosa_test::failures() == 0 ? 0 : 1;
}
