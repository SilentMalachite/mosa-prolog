// term_to_atom(?term, ?atom)。非 ISO（SPEC 第7節の段階 G、レビュー M11）で、SWI-Prolog の pl-read.c の
// atom_to_term に合わせる。
//   Atom が変数なら Term を quoted(true) で書き（numbervars と変数名は使わない）、その原子と単一化する。両方が変数でも
//   誤りにしない。Atom が束縛されていれば、その字面（原子、数、文字コードのリスト、文字のリスト）を 1 つの項として
//   読み、Term と単一化する。終止符は無くてよく、最初の項の後ろは読まない。演算子は呼び手の文脈モジュールの見え方
//   （ADR 0006）、旗 double_quotes・char_conversion・japanese_punctuation は read/1 と同じ。
//   instantiation_error（部分リスト、要素が変数）、type_error(string, Atom)（字面にならない。SWI-Prolog と同じ型名）、
//   syntax_error(Key)（ADR 0007。項が閉じる前に字面が終われば incomplete_term。空の字面も同じで、SWI-Prolog 9.3.5
//   以降の end_of_string に当たる）。
//   SWI-Prolog は [] を空の字面とみなすが、ISO の [] は原子なので、Mosa では字面 "[]" として読む。
//   Atom の数は Big でもよい（フェーズ10）。文字コードのリストの Big は、範囲の外の int64 と同じく
//   type_error(string, Atom)。
// テスト: tests/pl/utilities.pl、tests/pl/japanese.pl、tests/pl/bigint.pl
#include "../machine/internal.hpp"
#include "../module/module.hpp"

#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mosa {
namespace {

void raise(Impl& machine, const Goal& goal, Cell formal) { raise_formal(machine, formal, goal.name, goal.arity); }

std::string written(Impl& machine, const Goal& goal, Cell term) {
  WriteOptions options;
  options.quoted = true;
  options.fullwidth = machine.japanese_punct;
  return write_term(machine.heap, op_view(machine, goal.module), term, options, {});
}

// リストの要素がどれも文字コードか、どれも 1 字の原子なら、その字面。nullopt は字面にならないとき。
std::optional<std::string> list_text(Impl& machine, const WalkedList& list) {
  std::string text;
  const bool codes = !list.items.empty() && machine.heap.deref(list.items.front()).tag() != Tag::Atom;
  for (const Cell item : list.items) {
    const Cell cell = machine.heap.deref(item);
    char32_t code = 0;
    if (codes) {
      // Big はどの符号でも Unicode の範囲の外にあり、範囲の外の int64 と同じく字面にならない。
      if (cell.tag() != Tag::Int && cell.tag() != Tag::IntBox) return std::nullopt;
      const std::int64_t value = machine.heap.integer_value(cell);
      if (!scalar_code(value)) return std::nullopt;
      code = static_cast<char32_t>(value);
    } else if (!one_char_atom(machine, cell, code)) {
      return std::nullopt;
    }
    append_utf8_code(text, code);
  }
  return text;
}

// 束縛された Atom の字面。誤りを投げたら nullopt。
std::optional<std::string> source_text(Impl& machine, const Goal& goal, Cell source) {
  if (source.tag() == Tag::Atom) return std::string{machine.atoms.name(static_cast<std::uint32_t>(source.raw() >> 3))};
  if (is_integer_tag(source.tag()) || source.tag() == Tag::Float) {
    return written(machine, goal, source);
  }
  const WalkedList list = walk_list(machine, source);
  if (list.shape == ListShape::Partial) {
    raise(machine, goal, formal_instantiation(machine));
    return std::nullopt;
  }
  if (list.shape == ListShape::Proper) {
    const auto unbound = [&machine](Cell item) { return machine.heap.unbound(machine.heap.deref(item)); };
    if (std::any_of(list.items.begin(), list.items.end(), unbound)) {
      raise(machine, goal, formal_instantiation(machine));
      return std::nullopt;
    }
    if (auto text = list_text(machine, list)) return text;
  }
  raise(machine, goal, formal_type(machine, "string", source));
  return std::nullopt;
}

// 終止符を足して 1 項を読む。改行を挟むのは、字面の終わりの % の注釈に終止符を飲ませないため。誤りが足した
// 行にあれば、字面は項が閉じる前に終わった。
void read_text(Impl& machine, const Goal& goal, std::string_view text) {
  const std::string source = std::string{text} + "\n. ";
  Reader reader(source, machine.heap, op_view(machine, goal.module), current_double_quotes(machine),
                &machine.char_conv);
  reader.set_fullwidth(machine.japanese_punct);
  const ReadOutcome outcome = reader.next();
  if (!outcome.term) {
    const auto lines = static_cast<std::size_t>(std::count(text.begin(), text.end(), '\n')) + 1;
    SyntaxReason reason = SyntaxReason::IncompleteTerm;
    if (outcome.failure && outcome.failure->line <= lines) reason = outcome.failure->reason;
    return raise(machine, goal, formal_syntax(machine, machine.heap.atom(machine.atoms.intern(syntax_key(reason)))));
  }
  if (do_unify(machine, goal.arg(machine, 0), outcome.term->term)) machine.ip = goal.cont;
  else machine.failed = true;
}

void run_term_to_atom(Impl& machine, const Goal& goal) {
  const Cell source = machine.heap.deref(goal.arg(machine, 1));
  if (machine.heap.unbound(source)) {
    const std::string text = written(machine, goal, goal.arg(machine, 0));
    if (do_unify(machine, source, machine.heap.atom(machine.atoms.intern(text)))) machine.ip = goal.cont;
    else machine.failed = true;
    return;
  }
  if (const auto text = source_text(machine, goal, source)) read_text(machine, goal, *text);
}

}  // namespace

bool run_term_text(Impl& machine, const Goal& goal) {
  if (goal.arity == 2 && machine.atoms.name(goal.name) == "term_to_atom") {
    run_term_to_atom(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
