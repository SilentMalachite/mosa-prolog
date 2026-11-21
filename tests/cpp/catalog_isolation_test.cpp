// レビュー 2026-11-20 の H-28。mosac の翻訳は、捕まえなかった例外の文面の表（ADR 0007）を同梱するために
// library(messages_ja) と library(messages_en)（推移的に format と lists）を読む。前はそれをプログラムの機械で読んだので、
// 利用者のモジュール lists、format、messages_* を lib/ の物が置き換え、plan_units がそれをライブラリとして飛ばして、
// 利用者のモジュールの単位を出さなかった（IR のまま動いた）。ここはコンパイラを起こさずに目録を見る。
// 実際のビルドと --debug の追跡は tests/mosac/build_test.cmake の catalog_names。
#include "harness.hpp"

#include "codegen/codegen.hpp"
#include "mosa/path_text.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using mosa::codegen::Project;
using mosa_test::check;

bool has(const std::string& text, std::string_view part) { return text.find(part) != std::string::npos; }

void write_file(const std::filesystem::path& path, std::string_view text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out{path, std::ios::binary};
  out << text;
}

const std::string* file_text(const Project& project, std::string_view name) {
  for (const auto& file : project.files) {
    if (file.name == name) return &file.text;
  }
  return nullptr;
}

// 同梱する名前の数。翻訳済み（libraries）と原文だけ（source_only）の両方を数える。
std::size_t bundled(const Project& project, std::string_view name) {
  const auto count = [name](const std::vector<std::string>& names) {
    return static_cast<std::size_t>(std::count(names.begin(), names.end(), name));
  };
  return count(project.libraries) + count(project.source_only);
}

std::filesystem::path base() { return std::filesystem::path{MOSA_SOURCE_DIR} / "tests/tmp/catalog_isolation"; }

Project translate(const std::filesystem::path& main) {
  return mosa::codegen::translate_program(mosa::utf8_from_path(main), {});
}

// 文面の表が読むモジュールと同じ名前の利用者のモジュールも、ほかの名前と同じに単位 u1 になる。ファイルの名前も
// モジュールと同じにする（報告の再現の形。鍵は lists.pl で、同梱の library/lists.pl とはぶつからない）。文面の表と、
// そこから読む format と lists は今までどおり 1 つずつ同梱する（実行ファイルは例外の文面をそこから作る）。
void user_module_named(std::string_view module, std::string_view exports, std::string_view clauses,
                       std::string_view main_body) {
  const std::string name{module};
  const auto dir = base() / (name + "_" + std::string{exports.substr(0, exports.find('/'))});
  write_file(dir / (name + ".pl"), ":- module(" + name + ", [" + std::string{exports} + "]).\n" + std::string{clauses});
  write_file(dir / "main.pl", ":- use_module(" + name + ").\nmain :- " + std::string{main_body} + ".\n");
  const Project project = translate(dir / "main.pl");
  const std::string label = name + " exporting " + std::string{exports};
  check(project.ok && project.warnings.empty(), label + ": translated: " + project.diagnostic + project.warnings);
  check(file_text(project, "u1.cpp") != nullptr, label + ": the user's module has a unit");
  const std::string* main_file = file_text(project, "main.cpp");
  check(main_file != nullptr && has(*main_file, "#include \"u1.hpp\"  // " + name), label + ": u1 is the module");
  check(main_file != nullptr && has(*main_file, "\"" + name + ".pl\""), label + ": the user's file is embedded");
  const std::string* unit = file_text(project, "u1.cpp");
  // 単位の節のコメントは writeq の形なので、公開した述語の名前と開き括弧で見る。
  check(unit != nullptr && has(*unit, "// % " + std::string{exports.substr(0, exports.find('/'))} + "("),
        label + ": the unit holds the user's clauses");
  for (const std::string_view library : {"messages_ja", "messages_en", "format", "lists"}) {
    check(bundled(project, library) == 1, label + ": bundles " + std::string{library} + " once");
  }
}

void user_modules() {
  user_module_named("lists", "mine/1", "mine(1).\n", "mine(X), write(X), nl");
  // 報告の変種: library(lists) と同じ append/3 を公開する。
  user_module_named("lists", "append/3", "append([], L, L).\nappend([H|T], L, [H|R]) :- append(T, L, R).\n",
                    "append([a], [b], L), write(L), nl");
  user_module_named("format", "mine/1", "mine(2).\n", "mine(X), write(X), nl");
  user_module_named("messages_ja", "mine/1", "mine(3).\n", "mine(X), write(X), nl");
  user_module_named("messages_en", "mine/1", "mine(4).\n", "mine(X), write(X), nl");
}

// プログラムが自分で library(lists) を読んでも、文面の表が読む lists と同じファイルなので同梱は 1 つで、
// 鍵の重なり（mosac_duplicate_key）にもならない。
void program_loads_lists() {
  const auto dir = base() / "own_lists";
  write_file(dir / "main.pl", ":- use_module(library(lists)).\nmain :- append([a], [b], L), write(L), nl.\n");
  const Project project = translate(dir / "main.pl");
  check(project.ok && project.warnings.empty(), "own lists: translated: " + project.diagnostic + project.warnings);
  check(bundled(project, "lists") == 1 && bundled(project, "messages_ja") == 1, "own lists: bundled once");
  check(file_text(project, "u1.cpp") == nullptr, "own lists: library(lists) is not a program unit");
}

// 節の本体にだけ書いた library(lists) は、ゴールのライブラリとしてプログラムの機械に読む（実行時と同じ）。文面の表を
// 別の機械で読むのはその後で、同じファイルは記録に足さない。順が逆なら lib/lists.pl を 2 度記録し、mosac_duplicate_key
// で翻訳が止まる。
void goal_loads_lists() {
  const auto dir = base() / "goal_lists";
  write_file(dir / "main.pl", "main :- use_module(library(lists)), lists:append([a], [b], L), write(L), nl.\n");
  const Project project = translate(dir / "main.pl");
  check(project.ok && project.warnings.empty(), "goal lists: translated: " + project.diagnostic + project.warnings);
  check(bundled(project, "lists") == 1 && bundled(project, "format") == 1, "goal lists: bundled once");
}

// 文面の表はプログラムの機械に何も足さない。プログラムが読んでいない lists の lists:append/3 は、mosa でも
// existence_error になる呼び出しなので、未定義の呼び出しの警告になる（前は文面の表が読んだ lists で黙っていた）。
void program_machine_untouched() {
  const auto dir = base() / "untouched";
  write_file(dir / "main.pl", "main :- lists:append([a], [b], L), write(L), nl.\n");
  const Project project = translate(dir / "main.pl");
  check(project.ok && has(project.warnings, "lists:append/3"), "untouched: warns: " + project.warnings);
}

}  // namespace

int main() {
  std::filesystem::remove_all(base());
  user_modules();
  program_loads_lists();
  goal_loads_lists();
  program_machine_untouched();
  return mosa_test::failures();
}
