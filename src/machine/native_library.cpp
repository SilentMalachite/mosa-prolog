// 同梱ライブラリ（計画 Task 10、ADR 0009 Review Focus 5）。mosac の実行ファイルは lib/*.pl の翻訳済みの単位と原文を
// 持ち、use_module(library(Name)) は SPEC 第5節の探索（$MOSA_LIB、実行ファイルの横の lib/）より先にこの一覧を引く。
// 一覧はプロセスで 1 つにする。文面のために作る新しいマシン（render_message_fresh）も同じカタログを読めなければ、
// 単独の実行ファイルの heap_limit や cannot_open_file の文面が鍵のままになる。
// 原文は擬似パス library/<Name>.pl で埋め込み、読み込み器のファイルの仕組み（prepared、Module::file、include/1 の
// 相対パス）をそのまま使う。読み終えたら単位を付ける（ADR 0009「load_source が返った後」）。
// ライブラリの中の素のパスの use_module(util) も、同じ擬似ディレクトリ library/ に解決するので同じ一覧で引く。
// テスト: tests/mosac/standalone_test.cmake（standalone_use_lists、standalone_uncaught_message、standalone_plain_use）、
// tests/cpp/native_test.cpp（check_bundled_library）

#include "internal.hpp"

#include "mosa/path_text.hpp"

#include <algorithm>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

namespace mosa {
namespace {

struct BundledLibrary {
  std::string name;
  std::string source;  // 一覧が原文を持つ。生成した main の文字列の寿命に頼らない
  std::optional<rt::NativeUnit> unit;
};

// deque にするのは、unit->source が自分の source を指す string_view だからである。vector は伸びるときに要素を
// 移すので、先に入れたライブラリの unit->source が宙に浮く。deque は push_back で要素の番地を変えない。
std::deque<BundledLibrary>& bundled_libraries() {
  static std::deque<BundledLibrary> libraries;
  return libraries;
}

// library('lists.pl') も library(lists) も同じ lists.pl を指す（find_library と同じ規則）。
std::string library_stem(std::string_view name) {
  if (name.size() > 3 && name.ends_with(".pl")) return std::string{name.substr(0, name.size() - 3)};
  return std::string{name};
}

const BundledLibrary* find_bundled(std::string_view name) {
  const std::string stem = library_stem(name);
  for (const BundledLibrary& library : bundled_libraries()) {
    if (library.name == stem) return &library;
  }
  return nullptr;
}

void add_bundled(std::string_view name, std::string_view source, std::optional<rt::NativeUnit> unit) {
  // 同じ名前は後から足した物が勝たない。先に入れた一覧のまま、原文の寿命だけを確かにする。
  if (find_bundled(name) != nullptr) return;
  bundled_libraries().push_back(BundledLibrary{library_stem(name), std::string{source}, std::nullopt});
  BundledLibrary& added = bundled_libraries().back();
  if (unit) {
    unit->source = added.source;
    added.unit = *unit;
  }
}

}  // namespace

std::filesystem::path bundled_library_path(std::string_view name) {
  return path_from_utf8("library") / path_from_utf8(library_stem(name) + ".pl");
}

std::optional<std::filesystem::path> locate_bundled_library(Impl& machine, std::string_view name) {
  const BundledLibrary* library = find_bundled(name);
  if (library == nullptr) return std::nullopt;
  const std::filesystem::path file = bundled_library_path(name);
  const std::string key = utf8_from_path(canonical_key(file));
  const bool embedded = std::any_of(machine.embedded.begin(), machine.embedded.end(),
                                    [&](const EmbeddedSource& source) { return source.key == key; });
  if (!embedded) machine.embedded.push_back(EmbeddedSource{key, library->source});
  return file;
}

// source_path は埋め込みに無い名前に .pl を足さない（ディスクにも無いので）。ここで足してから擬似パスと比べる。
std::optional<std::string> locate_bundled_plain(Impl& machine, std::filesystem::path& file) {
  std::filesystem::path with_pl = file;
  if (with_pl.extension().empty()) with_pl.replace_extension(".pl");
  const std::string stem = utf8_from_path(with_pl.stem());
  if (find_bundled(stem) == nullptr || canonical_key(with_pl) != canonical_key(bundled_library_path(stem))) {
    return std::nullopt;
  }
  file = *locate_bundled_library(machine, stem);
  return stem;
}

void attach_bundled_library(Impl& machine, std::string_view name) {
  const BundledLibrary* library = find_bundled(name);
  if (library == nullptr || !library->unit) return;
  const std::size_t slot = prepare_native_unit(machine, *library->unit);
  attach_native_unit(machine, slot, *library->unit);
}

namespace rt {

void bundle_library(std::string_view name, std::string_view source) { add_bundled(name, source, std::nullopt); }

void bundle_library(std::string_view name, const NativeUnit& unit) { add_bundled(name, unit.source, unit); }

}  // namespace rt

}  // namespace mosa
