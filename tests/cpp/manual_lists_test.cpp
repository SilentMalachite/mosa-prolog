// mosatest --manual が述語の表と比べる処理系の一覧（リリース 0.1.0 Task 5）。組込の一覧、library の公開リスト、
// 表に書けるライブラリ、--mosa の版の突き合わせ、引数を確かめる。ライブラリは組み上げで写した隣の lib/ と、読めない
// ライブラリの見本 tests/manual/bad-lib/lib（MOSA_SOURCE_DIR）を読む。表の読み取りは manual_parse_test.cpp
// （修正 4 でファイルが 800 行を超えるので分けた）。

#include "harness.hpp"
#include "mosa/exe_path.hpp"
#include "mosa/machine.hpp"
#include "mosa/manual.hpp"
#include "mosa/version.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using mosa_test::check;

bool same(const std::vector<mosa::PredicateIndicator>& entries,
          const std::vector<mosa::PredicateIndicator>& wanted) {
  return entries == wanted;
}

bool has_indicator(const std::vector<mosa::PredicateIndicator>& all, std::string_view name, int arity) {
  for (const mosa::PredicateIndicator& item : all) {
    if (item.name == name && item.arity == arity) return true;
  }
  return false;
}

// 引用した原子にする。名前に ' と \ があっても読み戻せる。std::quoted と ADL でぶつからない名前にする。
std::string quoted_name(std::string_view name) {
  std::string out = "'";
  for (const char letter : name) {
    if (letter == '\'' || letter == '\\') out += '\\';
    out += letter;
  }
  return out + "'";
}

// 一覧のどれもが predicate_property(H, built_in) を満たす。表の system はこの性質の名前を並べる。
// :/2 だけは問えない。predicate_property は M:G をモジュールの限定として外すので、頭 _:_ は束縛の無い頭になる。
void manual_builtin_indicators() {
  const std::vector<mosa::PredicateIndicator> all = mosa::builtin_indicators();
  for (int arity = 1; arity <= 8; ++arity) check(has_indicator(all, "call", arity), "builtins: call/1..8");
  check(has_indicator(all, "halt", 0) && has_indicator(all, "halt", 1), "builtins: halt/0 and halt/1");
  check(has_indicator(all, ",", 2) && has_indicator(all, "length", 2), "builtins: ,/2 and length/2");
  check(has_indicator(all, ":", 2), "builtins: :/2");
  check(!has_indicator(all, "use_module", 1) && !has_indicator(all, "dynamic", 1), "builtins: no load directives");
  bool internal = false;
  for (const mosa::PredicateIndicator& item : all) internal = internal || item.name.starts_with('$');
  check(!internal, "builtins: no $ helpers");
  const auto machine = mosa::Machine::create();
  for (const mosa::PredicateIndicator& item : all) {
    if (item.name == ":" && item.arity == 2) continue;
    const std::string goal = "functor(H, " + quoted_name(item.name) + ", " + std::to_string(item.arity) +
                             "), predicate_property(H, built_in).";
    const mosa::Stop stop = machine->query(goal);
    check(stop.kind == mosa::StopKind::Success, "builtins: built_in holds for " + item.name);
  }
}

// mosatest の隣の lib/。mosatest --manual が mosa の隣の lib/ を読むのと同じ置き方（組み上げで写す）。
std::filesystem::path beside_lib() {
  const std::optional<std::filesystem::path> self = mosa::executable_path();
  return self ? self->parent_path() / "lib" : std::filesystem::path{"lib"};
}

void manual_library_exports() {
  const std::filesystem::path lib = beside_lib();
  const mosa::LibraryExports between = mosa::library_exports("between", lib);
  check(between.exports && same(*between.exports, {{"between", 3}, {"succ", 2}, {"plus", 3}}),
        "exports: library(between)");
  const mosa::LibraryExports lists = mosa::library_exports("lists", lib);
  check(lists.exports && has_indicator(*lists.exports, "length", 2) && has_indicator(*lists.exports, "append", 3),
        "exports: library(lists)");
  const mosa::LibraryExports japanese = mosa::library_exports("japanese", lib);
  check(japanese.exports && has_indicator(*japanese.exports, "ひらがな_カタカナ", 2), "exports: library(japanese)");
  for (const std::string_view name : {"apply", "format", "strings"}) {
    check(mosa::library_exports(name, lib).exports.has_value(), "exports: library " + std::string{name});
  }
  for (const std::string_view name : {"system", "nope"}) {
    const mosa::LibraryExports none = mosa::library_exports(name, lib);
    check(!none.exports && !none.error.empty(), "exports: none, with a reason, for " + std::string{name});
  }
  const std::filesystem::path bad = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "manual" / "bad-lib" / "lib";
  for (const std::string_view name : {"broken", "nomodule", "renamed"}) {
    const mosa::LibraryExports none = mosa::library_exports(name, bad);
    check(!none.exports && !none.error.empty(), "exports: unloadable " + std::string{name});
  }
  check(!mosa::library_exports("between", bad).exports, "exports: only the given directory is searched");
}

void manual_libraries_and_version() {
  const auto names = mosa::manual_libraries(beside_lib());
  check(names == std::vector<std::string>{"apply", "between", "dcg", "format", "japanese", "lists", "strings"},
        "libraries: the *.pl beside mosatest without messages_*");
  check(!mosa::manual_libraries(beside_lib() / "nope").has_value(), "libraries: unreadable directory");
  const std::string line = mosa::version_line();
  check(mosa::same_version(line + "\n") && mosa::same_version(line + "\r\n") && mosa::same_version(line),
        "version: the same line");
  check(!mosa::same_version("cmake version 3.31.0\n") && !mosa::same_version("") &&
            !mosa::same_version(line + "\nmore\n") && !mosa::same_version(line + "-dev\n"),
        "version: another line");
}

void manual_args() {
  const std::vector<std::string_view> all{"docs/manual", "--all-tables", "--mosa", "build/mosa"};
  const mosa::ManualArgs parsed = mosa::parse_manual_args(all);
  check(parsed.ok && parsed.all_tables && parsed.dir == "docs/manual" && parsed.mosa == "build/mosa",
        "args: --all-tables with --mosa");
  const std::vector<std::string_view> plain{"docs/manual"};
  check(mosa::parse_manual_args(plain).ok && !mosa::parse_manual_args(plain).all_tables, "args: no --all-tables");
  const std::vector<std::string_view> bare{"--all-tables"};
  check(!mosa::parse_manual_args(bare).ok, "args: --all-tables without a directory");
}

}  // namespace

int main() {
  manual_builtin_indicators();
  manual_library_exports();
  manual_libraries_and_version();
  manual_args();
  return mosa_test::failures() == 0 ? 0 : 1;
}
