// 利用者向けの文面（ADR 0007、SPEC 第7節）。文面は lib/messages_ja.pl と lib/messages_en.pl の事実に置く。
// テスト: tests/cpp/session_test.cpp、tests/cpp/full_stop_test.cpp（char_conversion の下の文面）、
//   tests/cpp/data/debugger（transcript_debug_spy_after_warning。文面が利用者の機械に何も読まないこと）
#include "mosa/messages.hpp"

#include "internal.hpp"

#include "mosa/exe_path.hpp"
#include "mosa/heap.hpp"
#include "mosa/heap_option.hpp"
#include "mosa/path_text.hpp"
#include "mosa/ui_language.hpp"
#include "mosa/writer.hpp"

#include <array>
#include <initializer_list>
#include <string>
#include <vector>

namespace mosa {
namespace {

// カタログが無い、鍵が無い、書式が失敗したときの形。引数は writeq で書く。
std::string fallback_text(Machine& machine, std::string_view key, std::span<const Cell> args) {
  WriteOptions options;
  options.numbervars = true;
  options.fullwidth = engine_impl(machine).japanese_punct;
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

std::string catalog_module(Machine& machine) {
  return engine_impl(machine).message_lang == MessageLanguage::Ja ? "messages_ja" : "messages_en";
}

// 目録のゴール catch((M:message(Key, MosaFormat), with_output_to(atom(MosaText), format:format(MosaFormat, Args))),
// _, fail) を項で作り、利用者の表で書く。query は同じ表で読み直すので、利用者が : や = を変えても同じ項に戻る（レビュー
// L19）。字面を手で書くと、op(0, xfy, :) の後は M:message が読めなかった。問い合わせはヒープを捨てるので、項ではなく
// 字面で渡す。numbervars は偽にする。'$VAR'(1) を B と書くと、読み直したときに変数になる。旗 japanese_punctuation も
// 同じ旗で書く。偽で書くと、旗が真のとき '：' は : に化け、'、' は読めずに鍵の文面に落ちる（レビュー M8）。
std::string goal_text(Machine& machine, std::string_view key, std::span<const Cell> args) {
  Impl& impl = engine_impl(machine);
  const auto atom = [&impl](std::string_view name) { return impl.heap.atom(impl.atoms.intern(name)); };
  const auto term = [&impl](std::string_view name, std::initializer_list<Cell> parts) {
    return impl.heap.structure(impl.atoms.intern(name), std::span<const Cell>(parts.begin(), parts.size()));
  };
  Cell list = atom("[]");
  for (std::size_t index = args.size(); index > 0; --index) list = impl.heap.list(args[index - 1], list);
  const Cell format = impl.heap.fresh_ref();
  const Cell text = impl.heap.fresh_ref();
  const Cell lookup = term(":", {atom(catalog_module(machine)), term("message", {atom(key), format})});
  const Cell render =
      term("with_output_to", {term("atom", {text}), term(":", {atom("format"), term("format", {format, list})})});
  const Cell goal = term("catch", {term(",", {lookup, render}), impl.heap.fresh_ref(), atom("fail")});
  WriteOptions options;
  options.fullwidth = impl.japanese_punct;
  const std::array<VarName, 2> names{VarName{"MosaFormat", format}, VarName{"MosaText", text}};
  const std::string written = write_term(impl.heap, impl.operators, goal, options, names);
  return written + full_stop_after(written);
}

// 文面を作る間は char_conversion を止め、抜けるとき戻す。目録の指示とゴールは Mosa が書いた字面で、カタログも Mosa の
// 文書だからである（レビュー L17 の続き。止めないと a を b に写す変換の下で library が librbry に化け、鍵の形に落ちた）。
// render_message は問い合わせの外でしか呼ばれないので、止めた旗を利用者のゴールが見ることは無い。
class ConversionPause {
 public:
  explicit ConversionPause(CharConv& conv) : conv_(conv), was_(conv.enabled) { conv_.enabled = false; }
  ~ConversionPause() { conv_.enabled = was_; }
  ConversionPause(const ConversionPause&) = delete;
  ConversionPause& operator=(const ConversionPause&) = delete;

 private:
  CharConv& conv_;
  bool was_;
};

// 文面を作る問い合わせと読み込みは、利用者のゴールではない。デバッグ中でも箱を作らず、ポートの行を出さない（ADR 0028）。
// 止めないと、トレース中に投げられた例外の警告を作るゴール（with_output_to/2 と format/2）までが追われる。
class DebugPause {
 public:
  explicit DebugPause(Impl& machine) : machine_(machine), was_(machine.debug_on) { machine_.debug_on = false; }
  ~DebugPause() { machine_.debug_on = was_; }
  DebugPause(const DebugPause&) = delete;
  DebugPause& operator=(const DebugPause&) = delete;

 private:
  Impl& machine_;
  bool was_;
};

}  // namespace

MessageLanguage language_from_lang(std::optional<std::string_view> lang) {
  if (lang && lang->starts_with("ja")) return MessageLanguage::Ja;
  return MessageLanguage::En;
}

MessageLanguage default_message_language() {
  const auto lang = environment_path("LANG");
  std::optional<std::string> text;
  if (lang) text = utf8_from_path(*lang);
  return message_language_from(text ? std::optional<std::string_view>{*text} : std::nullopt,
                               system_ui_language_is_japanese());
}

MessageLanguage message_language_from(std::optional<std::string_view> lang, bool ui_japanese) {
  if (lang && !lang->empty()) return language_from_lang(lang);
  return ui_japanese ? MessageLanguage::Ja : MessageLanguage::En;
}

namespace {

// 利用者の機械の項を、文面を作る機械へ写す。原子の番号は機械ごとなので、名前で引き直す。共有する部分と巡回は
// ExportedTerm の Share の節点が保つ。
Cell carry_term(Impl& from, Impl& to, Cell term) {
  ExportedTerm copy = export_term(from, term);
  for (ExportedTerm::Node& node : copy.nodes) {
    if (node.kind == ExportedTerm::Kind::Atom || node.kind == ExportedTerm::Kind::Str) {
      node.atom = to.atoms.intern(from.atoms.name(node.atom));
    }
  }
  return import_term(to, copy);
}

// 文面の表を machine に読み、そこで format/3 を動かす。machine は render_only の機械である。
std::string render_here(Machine& machine, std::string_view key, std::span<const Cell> args) {
  const std::string fallback = fallback_text(machine, key, args);
  const ConversionPause pause(engine_impl(machine).char_conv);
  const DebugPause quiet(engine_impl(machine));
  try {
    // ゴールは読み込みより先に、利用者の表で字面にする。args は GC の根に無いので、読み込みの指示が GC を起こすと
    // 動いた後の番地を指す（L19 の Codex レビュー）。ヒープに項を作るので、溢れたときは下の catch が受ける。
    const std::string goal = goal_text(machine, key, args);
    // 読み込みは指示でしか書けないので、問い合わせの前に済ませる。カタログは何も公開しないので、
    // use_module でも user の名前は増えない。2 度目からは読み直さない。
    // 指示の字面も Mosa のものなので、ISO の核で読む。カタログと library(format) は、use_module が library(...) の
    // ファイルとして ISO の核で読む（レビュー L19、ADR 0006 の改訂 1）。利用者が先に読んだライブラリも同じく読まれている。
    {
      CoreOperatorScope core(engine_impl(machine));
      if (!machine.load_program(":- use_module(library(" + catalog_module(machine) + ")).\n").ok) return fallback;
      core.finish();
    }
    const Stop stop = machine.query(goal);
    if (stop.kind != StopKind::Success) return fallback;
    for (const Binding& binding : stop.bindings) {
      if (binding.name != "MosaText") continue;
      const Cell text = machine.heap().deref(binding.variable);
      if (text.tag() != Tag::Atom) return fallback;
      return std::string{machine.atoms().name(static_cast<std::uint32_t>(text.raw() >> 3))};
    }
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    // 小さいヒープでカタログが読めなくても、表示は落とさない。
  }
  return fallback;
}

}  // namespace

// 利用者の機械には文面の表を読まない（ADR 0007 の改訂 3）。表は library(format) を、それは library(lists) を読むので、
// 読むと最初の警告の後だけ lists:append/3 や spy(lists:foo/1) が通るようになる（PR #4 の敵対的レビュー、持ち主の決定 B）。
// 新しい render_only の機械に、書き出しが見る演算子の表と旗 japanese_punctuation を写すので、文面は前と変わらない。
std::string render_message(Machine& machine, std::string_view key, std::span<const Cell> args) {
  Impl& impl = engine_impl(machine);
  if (impl.render_only) return render_here(machine, key, args);
  try {
    auto renderer = Machine::create();
    Impl& other = engine_impl(*renderer);
    other.message_lang = impl.message_lang;
    other.render_only = true;
    other.operators = impl.operators;
    other.japanese_punct = impl.japanese_punct;
    // 写した項は GC の根に無い。render_here は読み込みの前に字面にするので、その間に GC は起きない。
    std::vector<Cell> cells;
    cells.reserve(args.size());
    for (const Cell arg : args) cells.push_back(carry_term(impl, other, arg));
    return render_here(*renderer, key, cells);
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
    return fallback_text(machine, key, args);
  }
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
  } catch (...) {
    if (!is_heap_exhaustion()) throw;
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
