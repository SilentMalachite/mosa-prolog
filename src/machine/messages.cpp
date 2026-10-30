// 利用者向けの文面（ADR 0007、SPEC 第7節）。文面は lib/messages_ja.pl と lib/messages_en.pl の事実に置く。
// テスト: tests/cpp/session_test.cpp
#include "mosa/messages.hpp"

#include "internal.hpp"

#include "mosa/exe_path.hpp"
#include "mosa/heap.hpp"
#include "mosa/heap_option.hpp"
#include "mosa/path_text.hpp"
#include "mosa/writer.hpp"

#include <string>
#include <vector>

namespace mosa {
namespace {

// カタログが無い、鍵が無い、書式が失敗したときの形。引数は writeq で書く。
std::string fallback_text(Machine& machine, std::string_view key, std::span<const Cell> args) {
  WriteOptions options;
  options.numbervars = true;
  std::string text{key};
  text += ':';
  for (const Cell arg : args) {
    text += ' ';
    text += write_term(machine.heap(), machine.operators(), arg, options, {});
  }
  return text;
}

// 原子の引数の代わりの表示。fallback_text と同じ形で、引数は書いたまま並べる。
std::string plain_text(std::string_view key, std::span<const std::string> args) {
  std::string text{key};
  text += ':';
  for (const std::string& arg : args) {
    text += ' ';
    text += arg;
  }
  return text;
}

// 問い合わせはヒープを捨てるので、引数は先に字面にして問い合わせの中で読み直す。
// numbervars は偽にする。'$VAR'(1) を B と書くと、読み直したときに変数になる。
std::string catalog_module(Machine& machine) {
  return engine_impl(machine).message_lang == MessageLanguage::Ja ? "messages_ja" : "messages_en";
}

std::string goal_text(Machine& machine, std::string_view key, std::span<const Cell> args) {
  const std::string module = catalog_module(machine);
  std::string list = "[";
  for (std::size_t index = 0; index < args.size(); ++index) {
    if (index > 0) list += ", ";
    list += write_term(machine.heap(), machine.operators(), args[index], WriteOptions{}, {});
  }
  list += "]";
  return "catch((" + module + ":message(" + std::string{key} +
         ", MosaFormat), with_output_to(atom(MosaText), format:format(MosaFormat, " + list +
         "))), _, fail).";
}

}  // namespace

MessageLanguage language_from_lang(std::optional<std::string_view> lang) {
  if (lang && lang->starts_with("ja")) return MessageLanguage::Ja;
  return MessageLanguage::En;
}

MessageLanguage default_message_language() {
  const auto lang = environment_path("LANG");
  if (!lang) return MessageLanguage::En;
  return language_from_lang(utf8_from_path(*lang));
}

std::string render_message(Machine& machine, std::string_view key, std::span<const Cell> args) {
  const std::string fallback = fallback_text(machine, key, args);
  const std::string goal = goal_text(machine, key, args);
  try {
    // 読み込みは指示でしか書けないので、問い合わせの前に済ませる。カタログは何も公開しないので、
    // use_module でも user の名前は増えない。2 度目からは読み直さない。
    if (!machine.load_program(":- use_module(library(" + catalog_module(machine) + ")).\n").ok) return fallback;
    const Stop stop = machine.query(goal);
    if (stop.kind != StopKind::Success) return fallback;
    for (const Binding& binding : stop.bindings) {
      if (binding.name != "MosaText") continue;
      const Cell text = machine.heap().deref(binding.variable);
      if (text.tag() != Tag::Atom) return fallback;
      return std::string{machine.atoms().name(static_cast<std::uint32_t>(text.raw() >> 3))};
    }
  } catch (const ResourceLimit&) {
    // 小さいヒープでカタログが読めなくても、表示は落とさない。
  }
  return fallback;
}

std::string render_message_fresh(MessageLanguage language, std::string_view key, std::span<const std::string> args) {
  try {
    auto machine = Machine::create();
    Impl& impl = engine_impl(*machine);
    impl.message_lang = language;
    impl.render_only = true;
    std::vector<Cell> cells;
    for (const std::string& arg : args) cells.push_back(impl.heap.atom(impl.atoms.intern(arg)));
    return render_message(*machine, key, cells);
  } catch (const ResourceLimit&) {
    return plain_text(key, args);
  }
}

std::string heap_limit_text(const Impl& machine) {
  return message_text(machine, "heap_limit_size", {heap_limit_mib_text(machine.heap.limit())});
}

std::string message_text(const Impl& machine, std::string_view key, std::initializer_list<std::string_view> args) {
  const std::vector<std::string> texts(args.begin(), args.end());
  if (machine.render_only) return plain_text(key, texts);
  return render_message_fresh(machine.message_lang, key, texts);
}

}
