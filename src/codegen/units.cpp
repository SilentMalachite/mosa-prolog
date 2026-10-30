// 計画 Task 11。mosac emit --units <ディレクトリ>。試験のファイルごとに差分の原文（src/machine/diff.hpp の diff_source）を
// 組み立て、翻訳のときの読み込み（TranslationLog）で読んで、モジュールごとの単位を出す。どのファイルもモジュール user
// なので、名前空間は mosa_gen::u<N>_m<モジュール>（N はディレクトリの中の番号）で分け、unit() の名前をぶつけない。
// main.cpp は原文の断片と単位の表（rt::DiffEntry）を constexpr で持ち、rt::diff_main に渡すだけである。
// 同梱ライブラリは原文だけ。bundle_library の一覧はプロセスで 1 つなので、翻訳済みを渡すとインタプリタ側にも付いてしまう。
// 埋め込みの鍵は今のディレクトリからの相対で、実行ファイルを同じディレクトリ（リポジトリ直下）で動かす前提である。

#include "codegen.hpp"
#include "emit_state.hpp"
#include "project.hpp"

#include "../machine/diff.hpp"
#include "mosa/messages.hpp"
#include "mosa/heap_option.hpp"
#include "mosa/path_text.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace mosa::codegen {
namespace {

struct Library {
  std::string name;
  std::string text;
};

// 1 つの試験のファイルから実行ファイルに入れる物。keys、texts、spaces は同じ添字で、[0] が試験のファイル。
struct Entry {
  std::vector<std::string> keys;
  std::vector<std::string> texts;
  std::vector<std::string> spaces;  // 単位の名前空間（mosa_gen:: の後）。単位の無いファイルは空
  std::size_t predicates = 0;
  std::size_t translated = 0;
};

std::string message(std::string_view key, std::vector<std::string> args = {}) {
  return render_message_fresh(default_message_language(), key, args);
}

Project fail_with(std::string diagnostic) {
  Project project;
  project.diagnostic = std::move(diagnostic);
  return project;
}

// 正規化したパスを今のディレクトリからの相対にする。実行ファイルは同じディレクトリで動かし、同じ正規化で引く。
std::string cwd_key(std::string_view canonical) {
  std::error_code error;
  const auto cwd = canonical_key(std::filesystem::current_path(error));
  const std::filesystem::path relative = path_from_utf8(canonical).lexically_relative(cwd);
  return relative.empty() ? std::string{canonical} : utf8_from_path(relative);
}

// 翻訳のときの読み込み。.pl は組み立てた原文をそのパスとして埋め込み consult する（FileNote が原文をこの鍵で書き留め、
// test/3 は横取りで節にしない）。.lgt は実行器と同じく文字だけで読む。読めなければ偽で、diagnostic に理由を入れる。
bool read_for_translation(Machine& machine, const std::filesystem::path& file, const DiffSource& source,
                          const std::string& key, std::string& diagnostic) {
  Impl& impl = engine_impl(machine);
  try {
    if (source.lgt) {
      if (load_source(impl, source.source).ok) return true;
    } else {
      hook_test_facts(machine, file);
      rt::embed(machine, key, source.source);
      if (machine.consult(key)) return true;
    }
    diagnostic = std::string{machine.diagnostic()};
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    diagnostic = heap_limit_text(impl);
  }
  return false;
}

void emit_unit(Machine& machine, std::uint32_t module, std::span<const PredKey> interpreted, std::size_t number,
               Project& project, Entry& entry, std::size_t index) {
  const Impl& impl = engine_impl(machine);
  const std::string space = "u" + std::to_string(number) + "_" + unit_stem(impl, module);
  const EmitResult emitted = emit_module(machine, module, entry.keys[index], interpreted, false, space);
  project.files.push_back(ProjectFile{space + ".cpp", emitted.source});
  project.files.push_back(ProjectFile{space + ".hpp", emitted.header});
  entry.spaces[index] = space;
  entry.predicates += emitted.predicates;
  entry.translated += emitted.translated;
}

void add_file(Entry& entry, std::string key, std::string text) {
  entry.keys.push_back(std::move(key));
  entry.texts.push_back(std::move(text));
  entry.spaces.emplace_back();
}

// .pl: 翻訳のときに開いたファイルのうちプログラムの物を埋め込み、モジュールごとの単位を出す。ライブラリは同梱の一覧へ。
void plan_pl(Machine& machine, const TranslationLog& log, std::size_t number, Project& project, Entry& entry,
             std::vector<Library>& libraries) {
  const std::vector<UnitPlan> units = plan_units(engine_impl(machine), log);
  const std::vector<PredKey> interpreted = opaque_preds(log);
  const std::vector<std::string> keys = embed_keys(log);
  std::vector<std::size_t> files;  // entry の添字 → log.files の添字
  for (std::size_t index = 0; index < log.files.size(); ++index) {
    const TranslationLog::File& file = log.files[index];
    if (file.library) {
      libraries.push_back(Library{library_name(keys[index]), file.text});
      continue;
    }
    add_file(entry, cwd_key(file.key), file.text);
    files.push_back(index);
  }
  for (const UnitPlan& unit : units) {
    for (std::size_t at = 0; at < files.size(); ++at) {
      if (files[at] == unit.file) emit_unit(machine, unit.module, interpreted, number, project, entry, at);
    }
  }
}

// 文面の表（ADR 0007）は、実行ファイルの報告と捕まえなかった例外の文面にどの試験も読む。翻訳のときに読んで原文を
// 同梱の一覧に入れる（推移的に library(format)、library(lists) も入る）。読めなければ mosac の隣に lib/ が無い。
bool catalog_libraries(std::vector<Library>& libraries, std::string& fatal) {
  auto machine = Machine::create();
  Impl& impl = engine_impl(*machine);
  TranslationLog log;
  impl.translation = &log;
  const bool loaded = load_catalogs(*machine);
  impl.translation = nullptr;
  if (!loaded) {
    fatal = message_text(impl, "mosac_catalog_missing", {std::string{machine->diagnostic()}});
    return false;
  }
  if (log.files.empty()) return true;
  const std::vector<std::string> keys = embed_keys(log);
  for (std::size_t index = 0; index < log.files.size(); ++index) {
    if (log.files[index].library) libraries.push_back(Library{library_name(keys[index]), log.files[index].text});
  }
  return true;
}

// 試験のファイル 1 つを翻訳する。どの機械でも読めない原文（source.loads が偽）は両方の側が同じに失敗するので、
// 黙って単位の無い欄にする。組み立てられないか翻訳のときだけ読めなければ、単位の無い欄にして標準エラーに知らせる。
Entry translate_one(const std::filesystem::path& file, std::size_t number, Project& project,
                    std::vector<Library>& libraries) {
  Entry entry;
  const std::string key = cwd_key(utf8_from_path(canonical_key(file)));
  const DiffSource source = diff_source(file);
  if (source.error.empty() && !source.loads) {
    add_file(entry, key, source.source);
    return entry;
  }
  auto machine = Machine::create();
  Impl& impl = engine_impl(*machine);
  TranslationLog log;
  impl.translation = &log;
  std::string diagnostic = source.error;
  const bool loaded = diagnostic.empty() && read_for_translation(*machine, file, source, key, diagnostic);
  impl.translation = nullptr;
  if (!loaded || (!source.lgt && log.files.empty())) {
    std::cerr << message("mosac_units_skipped", {utf8_from_path(file), diagnostic}) << '\n';
    add_file(entry, key, source.source);
    return entry;
  }
  if (source.lgt) {
    add_file(entry, key, source.source);
    emit_unit(*machine, impl.user_module, {}, number, project, entry, 0);
    return entry;
  }
  plan_pl(*machine, log, number, project, entry, libraries);
  return entry;
}

std::string array_text(std::string_view type, std::string_view name, std::size_t count, const std::string& items) {
  const std::string head = "constexpr std::array<" + std::string{type} + ", " + std::to_string(count) + "> " + std::string{name};
  if (count == 0) return head + "{};\n";
  return head + "{{\n" + items + "}};\n";
}

std::string units_main(const std::vector<Entry>& entries, const std::vector<Library>& libraries) {
  std::string out = "// mosatest --diff の実行ファイルの入口（計画 Task 11）。手で直さない。\n\n";
  for (const Entry& entry : entries) {
    for (const std::string& space : entry.spaces) {
      if (!space.empty()) out += "#include \"" + space + ".hpp\"\n";
    }
  }
  out += "\n#include \"mosa/runtime.hpp\"\n\nnamespace {\n\n";
  out += "// 原文の断片。鍵は今のディレクトリからの相対で、実行ファイルは同じディレクトリで動かす。\n";
  std::size_t arrays = 0;
  std::string tables;
  std::string entry_items;
  for (std::size_t number = 0; number < entries.size(); ++number) {
    const Entry& entry = entries[number];
    std::string items;
    for (std::size_t index = 0; index < entry.keys.size(); ++index, ++arrays) {
      out += source_array(arrays, entry.texts[index]);
      const std::string unit = entry.spaces[index].empty() ? "nullptr" : "&mosa_gen::" + entry.spaces[index] + "::unit";
      items += "    {" + string_literal(entry.keys[index]) + ", kFile" + std::to_string(arrays) + ", " + unit + "},\n";
    }
    const std::string name = "kFiles" + std::to_string(number);
    tables += array_text("mosa::rt::DiffFile", name, entry.keys.size(), items);
    entry_items += "    {" + name + ", " + std::to_string(entry.predicates) + ", " + std::to_string(entry.translated) + "},\n";
  }
  std::string library_items;
  for (const Library& library : libraries) {
    out += source_array(arrays, library.text);
    library_items += "    {" + string_literal(library.name) + ", kFile" + std::to_string(arrays++) + "},\n";
  }
  out += "\n" + tables + array_text("mosa::rt::DiffLibrary", "kLibraries", libraries.size(), library_items);
  out += array_text("mosa::rt::DiffEntry", "kEntries", entries.size(), entry_items);
  return out + "\n}  // namespace\n\nint main(int argc, char** argv) { return mosa::rt::diff_main(argc, argv, kLibraries, kEntries); }\n";
}

// 同じ名前は先に読んだ物（同じ lib/ の同じ原文）を残し、名前の順に並べて同じ目録を出す。
std::vector<Library> unique_libraries(std::vector<Library> libraries) {
  std::vector<Library> out;
  for (Library& library : libraries) {
    const bool seen = std::any_of(out.begin(), out.end(), [&](const Library& item) { return item.name == library.name; });
    if (!seen) out.push_back(std::move(library));
  }
  std::sort(out.begin(), out.end(), [](const Library& left, const Library& right) { return left.name < right.name; });
  return out;
}

}  // namespace

Project translate_units(std::string_view dir, const ProjectOptions& options) {
  const std::vector<std::filesystem::path> files = diff_files(path_from_utf8(dir));
  if (files.empty()) return fail_with(message("diff_no_files", {std::string{dir}}));
  Project project;
  project.ok = true;
  std::vector<Entry> entries;
  std::vector<Library> libraries;
  std::string fatal;
  if (!catalog_libraries(libraries, fatal)) return fail_with(fatal);
  for (std::size_t number = 0; number < files.size(); ++number) {
    entries.push_back(translate_one(files[number], number, project, libraries));
  }
  std::string sources = "main.cpp";
  for (const Entry& entry : entries) {
    for (const std::string& space : entry.spaces) {
      if (!space.empty()) sources += " " + space + ".cpp";
    }
  }
  project.files.push_back(ProjectFile{"main.cpp", units_main(entries, unique_libraries(std::move(libraries)))});
  project.files.push_back(ProjectFile{"CMakeLists.txt", cmake_plain_text("mosa_diff", sources, options.runtime_dir)});
  return project;
}

}  // namespace mosa::codegen
