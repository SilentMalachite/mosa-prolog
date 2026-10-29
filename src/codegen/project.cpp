// 計画 Task 5、Task 10。mosac emit の目録を作る。翻訳のときの読み込みは宣言だけを実行し（TranslationLog）、
// 開いたファイルの原文を main.cpp に埋め込む。単位はモジュールごとに 1 つで、名前は C++ の名前空間
// mosa_gen::m<モジュール> になる。同じモジュールの単位は 1 つしか出さないので、unit() の名前はぶつからない。
//
// 単位の名前の決め方（Task 5 と Task 10 の持ち越し。すべて単射）
//  - C++ の名前空間: mosa_gen::m<mangle(モジュール名)>。mangle は _ を _5f、ほかの非英数字を _xx にするので __ を作らない
//  - プログラムの単位の NativeUnit::name と埋め込みの鍵: 根のファイルのディレクトリからの相対パス（embed_keys）
//  - 同梱ライブラリ: 埋め込みの鍵は、そのライブラリの根（library(Name) を見つけた $MOSA_LIB か lib/）からの相対パスを
//    擬似ディレクトリ library/ の下に置いた物（library/Name.pl、library/sub/util.pl。モジュールも include した物も同じ）。
//    ライブラリの名前はその鍵から library/ と .pl を除いた物（Name、sub/util）で、use_module(library(X)) の X と同じ綴り。
//    実行時は library(X) も、ライブラリの中の素のパス 'sub/util' も、この鍵に解決する（bundled_library_path、
//    locate_bundled_plain）。NativeUnit::name は library(Name)、翻訳済みの単位のファイルは pl/libmosa_pl_<Name>.a
//    （MSVC は mosa_pl_<Name>.lib）。モジュール名ではなく名前で結ぶのは、use_module(library(X)) の X がファイル名だからである
//  - 同じ鍵に 2 つのファイルが来たら（プログラムの library/x.pl と同梱の x など）翻訳の段で断る（mosac_duplicate_key）

#include "codegen.hpp"
#include "emit_state.hpp"
#include "project.hpp"

#include "mosa/path_text.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace mosa::codegen {
namespace {

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

}  // namespace

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

namespace {

std::optional<std::uint32_t> module_of_key(const Impl& machine, std::string_view key) {
  for (const Module& module : machine.modules) {
    if (module.file == key) return module.name;
  }
  return std::nullopt;
}

std::string relative_or_same(const std::filesystem::path& file, const std::filesystem::path& base) {
  const std::filesystem::path relative = file.lexically_relative(base);
  return relative.empty() ? utf8_from_path(file) : utf8_from_path(relative);
}

}  // namespace

// 埋め込みの鍵。翻訳した機械の絶対パスを実行ファイルに残さない（利用者名が漏れ、同じ原文から同じ物ができなくなる）。
// プログラムのファイルは根のディレクトリからの相対、ライブラリのファイル（モジュールも include した物も）はそのライブラリの
// 根からの相対を擬似ディレクトリ library/ の下に置く。下のディレクトリも残すので、ライブラリの中の素のパス 'sub/util' が
// 実行時に library/sub/util.pl に解決して同じ鍵を引く。相対にできないパス（Windows の別のドライブなど）だけは絶対のまま残す。
std::vector<std::string> embed_keys(const TranslationLog& log) {
  const std::filesystem::path root_dir = path_from_utf8(log.files.front().key).parent_path();
  std::vector<std::string> keys;
  for (const TranslationLog::File& file : log.files) {
    const std::filesystem::path path = path_from_utf8(file.key);
    if (!file.library) {
      keys.push_back(relative_or_same(path, root_dir));
      continue;
    }
    const std::filesystem::path relative = path.lexically_relative(path_from_utf8(file.library_root));
    keys.push_back(relative.empty() ? file.key : utf8_from_path(path_from_utf8("library") / relative));
  }
  return keys;
}

// 鍵 library/<名前>.pl の <名前>。鍵が library/ の下に無い（絶対のまま残した）ときだけ語幹で代える。
std::string library_name(std::string_view key) {
  std::filesystem::path relative = path_from_utf8(key).lexically_relative(path_from_utf8("library"));
  if (relative.empty() || *relative.begin() == std::filesystem::path{".."}) relative = path_from_utf8(key).filename();
  relative.replace_extension();
  return utf8_from_path(relative);
}

namespace {

// 同じ鍵で 2 つのファイルは埋め込めない（実行ファイルの中で区別できない）。先に入れた方が黙って勝つのではなく、翻訳の段で
// 両方のファイルを挙げて断る。鍵は正規化して比べる。同梱ライブラリの名前は鍵から作るので、名前の重なりも同じ比べ方で分かる。
std::optional<std::string> duplicate_diagnostic(const Impl& machine, const ProjectPlan& plan, const DisplayNames& names) {
  std::vector<std::string> canonical;
  for (const std::string& key : plan.keys) canonical.push_back(utf8_from_path(canonical_key(path_from_utf8(key))));
  for (std::size_t first = 0; first < canonical.size(); ++first) {
    for (std::size_t second = first + 1; second < canonical.size(); ++second) {
      if (canonical[first] != canonical[second]) continue;
      return message_text(machine, "mosac_duplicate_key", {names[first], names[second], plan.keys[second]});
    }
  }
  for (std::size_t first = 0; first < plan.libraries.size(); ++first) {
    for (std::size_t second = first + 1; second < plan.libraries.size(); ++second) {
      if (plan.libraries[first].name != plan.libraries[second].name) continue;
      const std::size_t file = plan.libraries[second].file;
      return message_text(machine, "mosac_duplicate_key", {names[plan.libraries[first].file], names[file], plan.keys[file]});
    }
  }
  return std::nullopt;
}

// 翻訳済みの単位を結ぶのは、翻訳のときに読んだ原文が mosac の隣の lib/<語幹>.pl と同じバイトのときだけ。
// $MOSA_LIB の別の版を読んだときは、単位の名前空間（モジュール名）も指紋も合わないので原文だけを同梱する。
bool shipped_identical(const ProjectOptions& options, std::string_view name, std::string_view text) {
  if (options.runtime_dir.empty()) return false;
  const std::filesystem::path shipped = path_from_utf8(options.runtime_dir) / "lib" / path_from_utf8(std::string{name} + ".pl");
  std::ifstream in{shipped, std::ios::binary};
  if (!in) return false;
  const std::string bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  return bytes == text;
}

// ライブラリとして開いたモジュールのファイル（library(_) の物と、そこから素のパスで開いた物）が同梱ライブラリ。名前は
// 鍵から（library_name）。文面の表（messages_ja、messages_en）も translate_program が読んでいるのでここに入る。
// 名前の順に並べ、同じプログラムからいつも同じ目録を出す（キャッシュの鍵）。
std::vector<LibraryPlan> plan_libraries(const Impl& machine, const TranslationLog& log, const ProjectOptions& options,
                                        std::span<const std::string> keys) {
  std::vector<LibraryPlan> libraries;
  for (std::size_t index = 0; index < log.files.size(); ++index) {
    const TranslationLog::File& file = log.files[index];
    const auto module = module_of_key(machine, file.key);
    if (!file.library || !module) continue;
    const std::string name = library_name(keys[index]);
    libraries.push_back(LibraryPlan{name, index, *module, shipped_identical(options, name, file.text)});
  }
  std::sort(libraries.begin(), libraries.end(),
            [](const LibraryPlan& left, const LibraryPlan& right) { return left.name < right.name; });
  return libraries;
}

Project fail_with(std::string diagnostic) {
  Project project;
  project.diagnostic = std::move(diagnostic);
  return project;
}

bool any_opaque(const TranslationLog& log) {
  return std::any_of(log.files.begin(), log.files.end(), [](const TranslationLog::File& file) { return file.opaque; });
}

}  // namespace

// 読めなかったファイルに節がある述語。生成関数を出さず、起動時の読み込みが IR のまま動かす。
std::vector<PredKey> opaque_preds(const TranslationLog& log) {
  std::vector<PredKey> keys;
  for (const TranslationLog::Site& site : log.sites) {
    if (log.files[site.file].opaque) keys.push_back(site.pred);
  }
  return keys;
}

namespace {

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
  ProjectPlan plan{plan_units(impl, log), {}, embed_keys(log)};
  plan.libraries = plan_libraries(impl, log, options, plan.keys);
  if (const auto duplicate = duplicate_diagnostic(impl, plan, display_names(log, path))) return fail_with(*duplicate);
  const std::vector<PredKey> interpreted = opaque_preds(log);
  Project project;
  project.ok = true;
  for (const UnitPlan& unit : plan.units) {
    const EmitResult emitted = emit_module(machine, unit.module, plan.keys[unit.file], interpreted, options.trace);
    const std::string stem = unit_stem(impl, unit.module);
    project.files.push_back(ProjectFile{stem + ".cpp", emitted.source});
    project.files.push_back(ProjectFile{stem + ".hpp", emitted.header});
  }
  for (const LibraryPlan& library : plan.libraries) {
    (library.prebuilt ? project.libraries : project.source_only).push_back(library.name);
  }
  project.files.push_back(ProjectFile{"main.cpp", main_text(impl, log, plan, options.interactive)});
  project.files.push_back(ProjectFile{"CMakeLists.txt", cmake_text(impl, plan, target_name(path), options.runtime_dir)});
  return project;
}

// mosac emit --library lib/<名前>.pl。根のモジュールの単位 <名前>.cpp だけを出す。原文は実行ファイルの main.cpp が
// 持つので、単位は今までどおり unit(source) の形で受ける。use_module した先（library(_)）は別の単位なので出さない。
Project build_library(Machine& machine, const TranslationLog& log, std::string_view path) {
  const Impl& impl = engine_impl(machine);
  const auto module = module_of_key(impl, log.files.front().key);
  if (!module) return fail_with(message_text(impl, "mosac_library_not_module", {path}));
  const std::string name = utf8_from_path(path_from_utf8(path).stem());
  const EmitResult emitted = emit_module(machine, *module, "library(" + name + ")", opaque_preds(log), false);
  Project project;
  project.ok = true;
  project.files.push_back(ProjectFile{name + ".cpp", emitted.source});
  return project;
}

}  // namespace

// 文面の表（ADR 0007）は、捕まえなかった例外の文面にどの実行ファイルも読む。翻訳のときにも読んで同梱の一覧に
// 入れる（推移的に library(format)、library(lists) も入る）。読めなければ mosac の隣の lib/ が無い。
bool load_catalogs(Machine& machine) {
  for (const std::string_view catalog : {"messages_ja", "messages_en"}) {
    if (!machine.load_program(":- use_module(library(" + std::string{catalog} + ")).\n").ok) return false;
  }
  return true;
}

// CMake の名前に使える文字だけなら原文の名前、ほかは app にする。
std::string target_name(std::string_view root) {
  const std::string stem = utf8_from_path(path_from_utf8(root).stem());
  bool plain = !stem.empty();
  for (const char unit : stem) {
    const bool alnum = (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') || (unit >= '0' && unit <= '9');
    plain = plain && (alnum || unit == '_' || unit == '-');
  }
  // CMake が予約する目標の名前は configure で通らない（生成器が作る目標と、CMake 自身の目標）。
  constexpr std::array<std::string_view, 13> reserved{"all",          "clean",   "install",      "help",
                                                      "test",         "package", "package_source", "edit_cache",
                                                      "rebuild_cache", "ALL_BUILD", "ZERO_CHECK", "RUN_TESTS",
                                                      "INSTALL"};
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
    if (!options.library && !load_catalogs(*machine)) {
      return fail_with(message_text(impl, "mosac_catalog_missing", {std::string{machine->diagnostic()}}));
    }
  } catch (const ResourceLimit&) {
    return fail_with(message_text(impl, "heap_limit"));
  }
  impl.translation = nullptr;
  if (log.files.empty()) return fail_with(message_text(impl, "cannot_open_file", {path}));
  std::string diagnostic;
  for (const std::string& line : warnings(impl, log, path, options)) diagnostic += (diagnostic.empty() ? "" : "\n") + line;
  if (!diagnostic.empty()) return fail_with(std::move(diagnostic));
  if (options.library) return build_library(*machine, log, path);
  return build_project(*machine, log, path, options);
}

}  // namespace mosa::codegen
