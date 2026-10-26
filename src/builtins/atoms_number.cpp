// 8.16.7 number_chars(?number, ?character_list)
// 8.16.8 number_codes(?number, ?character_code_list)
// エラー: instantiation_error, type_error(number), type_error(list),
//   type_error(character), type_error(integer), representation_error(character_code),
//   syntax_error(読み筋)
// 数の字句はフェーズ1のリーダを使う。新しい数の構文は足さない。
// 8.16.7 は数の前のレイアウト文だけを読み飛ばす。数の中の空白は字句のまま拒否する。
// 0' の直後の空白だけは文字コードの本体なので、レイアウト文に数えない。
// テスト: tests/pl/atoms.pl

#include "../machine/internal.hpp"

#include "mosa/char_class.hpp"
#include "mosa/lexer.hpp"
#include "mosa/writer.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

bool is_int_cell(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

bool is_number_cell(Cell cell) { return is_int_cell(cell) || cell.tag() == Tag::Float; }

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

void instantiation(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
}

void type_at(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
}

void succeed(Impl& machine, const Goal& goal) { machine.ip = goal.cont; }

bool has_var_item(const Impl& machine, const WalkedList& walked) {
  for (const Cell item : walked.items) {
    if (machine.heap.unbound(item)) return true;
  }
  return false;
}

bool reject_number_items(Impl& machine, const Goal& goal, const WalkedList& walked, bool codes) {
  for (const Cell item : walked.items) {
    if (machine.heap.unbound(item)) continue;
    if (codes) {
      if (!is_int_cell(item)) {
        type_at(machine, goal, "integer", item);
        return true;
      }
      if (!scalar_code(machine.heap.integer_value(item))) {
        raise_formal(machine, formal_representation(machine, "character_code"), goal.name, goal.arity);
        return true;
      }
      continue;
    }
    char32_t code = 0;
    if (!one_char_atom(machine, item, code)) {
      type_at(machine, goal, "character", item);
      return true;
    }
  }
  return false;
}

std::string join_items(const Impl& machine, const WalkedList& walked, bool codes) {
  std::string text;
  for (const Cell item : walked.items) {
    if (codes) append_utf8_code(text, static_cast<char32_t>(machine.heap.integer_value(item)));
    else text.append(machine.atoms.name(atom_id(item)));
  }
  return text;
}

Cell spelling_list(Impl& machine, std::string_view spelling, bool codes) {
  const CodeText text = decode_codes(spelling);
  std::vector<Cell> items;
  items.reserve(text.size());
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (codes) items.push_back(machine.heap.integer(static_cast<std::int64_t>(text.codes[index])));
    else items.push_back(machine.heap.atom(machine.atoms.intern_char(text.codes[index])));
  }
  return list_from_cells(machine, items);
}

struct SignedText {
  bool negative = false;
  std::string_view body;
};

SignedText strip_sign(std::string_view text) {
  // 数トークンの前に付く符号は '-' だけ。'+' はフェーズ1の数の字句に無い。
  SignedText out;
  out.body = text;
  if (!text.empty() && text.front() == '-') {
    out.negative = true;
    out.body.remove_prefix(1);
  }
  return out;
}

// 符号は has_layout のあとで外す。0' の本体の空白は、先頭の '-' のうしろでも文字である。
bool char_code_space(const std::vector<char32_t>& codes, std::size_t index) {
  std::size_t origin = 0;
  if (!codes.empty() && codes[0] == U'-') origin = 1;
  if (index != origin + 2 || origin + 1 >= codes.size()) return false;
  return codes[origin] == U'0' && codes[origin + 1] == U'\'' && codes[index] == U' ';
}

bool has_layout(std::string_view text) {
  const CodeText decoded = decode_codes(text);
  for (std::size_t index = 0; index < decoded.size(); ++index) {
    if (classify(decoded.codes[index]) != CharClass::Layout) continue;
    if (char_code_space(decoded.codes, index)) continue;
    return true;
  }
  return false;
}

// 先頭の空白とコメントだけを落とす。符号と数字の間は残し、後段が拒否する。
std::string_view skip_layout_text(std::string_view text) {
  const CodeText codes = decode_codes(text);
  std::size_t index = 0;
  const std::size_t count = codes.codes.size();
  while (index < count) {
    const char32_t code = codes.codes[index];
    if (classify(code) == CharClass::Layout) {
      ++index;
      continue;
    }
    if (code == U'%') {
      ++index;
      while (index < count && codes.codes[index] != U'\n') ++index;
      continue;
    }
    if (code == U'/' && index + 1 < count && codes.codes[index + 1] == U'*') {
      index += 2;
      while (index + 1 < count && !(codes.codes[index] == U'*' && codes.codes[index + 1] == U'/')) ++index;
      if (index + 1 >= count) return {};
      index += 2;
      continue;
    }
    break;
  }
  if (index >= count) return {};
  return text.substr(codes.cuts[index]);
}

bool min_magnitude(std::string_view body) {
  std::string digits;
  digits.reserve(body.size());
  for (const char ch : body) {
    if (ch != '_') digits.push_back(ch);
  }
  return digits == "9223372036854775808";
}

bool consumed_number(const Scan& first, const Scan& end, std::string_view body) {
  if (first.failure || end.failure || first.token.after_layout || first.token.byte != 0) return false;
  if (first.token.kind != TokenKind::Integer && first.token.kind != TokenKind::Float) return false;
  return end.token.kind == TokenKind::End && !end.token.after_layout && end.token.byte == body.size();
}

std::optional<Cell> parse_number(Impl& machine, std::string_view text) {
  text = skip_layout_text(text);
  if (has_layout(text)) return std::nullopt;
  const SignedText signed_text = strip_sign(text);
  if (signed_text.body.empty()) return std::nullopt;
  Lexer lexer(signed_text.body);
  const Scan first = lexer.next();
  if (first.failure && first.failure->reason == SyntaxReason::IntegerOverflow && signed_text.negative &&
      first.failure->token == signed_text.body && min_magnitude(signed_text.body)) {
    return machine.heap.integer(std::numeric_limits<std::int64_t>::min());
  }
  const Scan end = lexer.next();
  if (!consumed_number(first, end, signed_text.body)) return std::nullopt;
  if (first.token.kind == TokenKind::Integer) {
    auto value = first.token.integer;
    if (signed_text.negative && value > 0) value = -value;
    return machine.heap.integer(value);
  }
  double value = first.token.floating;
  if (signed_text.negative) value = -value;
  return machine.heap.floating(value);
}

void syntax_at(Impl& machine, const Goal& goal, std::string_view text) {
  const Cell detail = machine.heap.atom(machine.atoms.intern(text));
  raise_formal(machine, formal_syntax(machine, detail), goal.name, goal.arity);
}

void bind_parsed(Impl& machine, const Goal& goal, const WalkedList& walked, bool codes) {
  const std::string text = join_items(machine, walked, codes);
  const std::optional<Cell> number = parse_number(machine, text);
  if (!number) {
    syntax_at(machine, goal, text);
    return;
  }
  if (!do_unify(machine, goal.arg(machine, 0), *number)) machine.failed = true;
  else succeed(machine, goal);
}

void bind_spelling(Impl& machine, const Goal& goal, Cell number, bool codes) {
  const std::string spelling = write_term(machine.heap, machine.operators, number, {}, {});
  if (!do_unify(machine, goal.arg(machine, 1), spelling_list(machine, spelling, codes))) machine.failed = true;
  else succeed(machine, goal);
}

void bind_ground_number(Impl& machine, const Goal& goal, const WalkedList& walked, bool codes) {
  // 数が具体でも、リストが別の正当な数の綴りなら成功する。綴りでなければ syntax_error。
  const std::string text = join_items(machine, walked, codes);
  const std::optional<Cell> parsed = parse_number(machine, text);
  if (!parsed) {
    syntax_at(machine, goal, text);
    return;
  }
  if (!do_unify(machine, goal.arg(machine, 0), *parsed)) machine.failed = true;
  else succeed(machine, goal);
}

}  // namespace

void run_number_atoms(Impl& machine, const Goal& goal) {
  const bool codes = machine.atoms.name(goal.name) == "number_codes";
  const Cell number = machine.heap.deref(goal.arg(machine, 0));
  const Cell list = machine.heap.deref(goal.arg(machine, 1));
  const bool number_var = machine.heap.unbound(number);
  if (number_var && machine.heap.unbound(list)) {
    instantiation(machine, goal);
    return;
  }
  if (!number_var && !is_number_cell(number)) {
    type_at(machine, goal, "number", number);
    return;
  }
  WalkedList walked;
  if (!machine.heap.unbound(list)) {
    walked = walk_list(machine, list);
    if (walked.shape == ListShape::Bad || walked.shape == ListShape::Variable) {
      type_at(machine, goal, "list", list);
      return;
    }
    if (number_var && (walked.shape == ListShape::Partial || has_var_item(machine, walked))) {
      instantiation(machine, goal);
      return;
    }
    if (reject_number_items(machine, goal, walked, codes)) return;
  }
  if (number_var) {
    bind_parsed(machine, goal, walked, codes);
    return;
  }
  if (machine.heap.unbound(list) || walked.shape == ListShape::Partial || has_var_item(machine, walked)) {
    bind_spelling(machine, goal, number, codes);
    return;
  }
  bind_ground_number(machine, goal, walked, codes);
}

}  // namespace mosa
