#include "mosa/reader.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct Decoded {
  char32_t code = 0;
  std::size_t bytes = 0;
  bool valid = false;
};

int continuation(std::string_view text, std::size_t index) {
  if (index >= text.size()) return -1;
  const auto byte = static_cast<unsigned char>(text[index]);
  if ((byte & 0xC0U) != 0x80U) return -1;
  return static_cast<int>(byte & 0x3FU);
}

Decoded decode_utf8(std::string_view text, std::size_t index) {
  if (index >= text.size()) return {};
  const auto lead = static_cast<unsigned char>(text[index]);
  if (lead <= 0x7FU) return Decoded{lead, 1, true};
  const int first = continuation(text, index + 1);
  if (first < 0) return {};
  if (lead >= 0xC2U && lead <= 0xDFU) {
    const auto code = static_cast<char32_t>(((lead & 0x1FU) << 6) | first);
    return Decoded{code, 2, true};
  }
  const int second = continuation(text, index + 2);
  if (second < 0) return {};
  if (lead >= 0xE0U && lead <= 0xEFU) {
    const auto code = static_cast<char32_t>(((lead & 0x0FU) << 12) | (first << 6) | second);
    return Decoded{code, 3, true};
  }
  const int third = continuation(text, index + 3);
  if (third < 0 || lead < 0xF0U || lead > 0xF4U) return {};
  const auto code = static_cast<char32_t>(((lead & 0x07U) << 18) | (first << 12) | (second << 6) | third);
  return Decoded{code, 4, true};
}

bool is_prefix(Specifier specifier) {
  return specifier == Specifier::Fx || specifier == Specifier::Fy;
}

bool is_infix(Specifier specifier) {
  return specifier == Specifier::Xfx || specifier == Specifier::Xfy || specifier == Specifier::Yfx;
}

bool is_postfix(Specifier specifier) {
  return specifier == Specifier::Xf || specifier == Specifier::Yf;
}

bool y_on_left(Specifier specifier) {
  return specifier == Specifier::Yfx || specifier == Specifier::Yf;
}

}  // namespace

Reader::Reader(std::string_view source, Heap& heap, OperatorView operators, DoubleQuotes quotes,
               const CharConv* conv)
    : lexer_(source, conv), heap_(heap), operators_(operators), quotes_(quotes) {}

Scan Reader::peek() {
  if (!has_pending_) {
    pending_ = lexer_.next();
    has_pending_ = true;
  }
  return pending_;
}

Scan Reader::consume() {
  Scan scan = peek();
  has_pending_ = false;
  return scan;
}

bool Reader::is_closer(std::string_view text) {
  return text == ")" || text == "]" || text == "}";
}

Reader::Parsed Reader::failure_of(const Scan& scan) const {
  Parsed parsed;
  parsed.failure = scan.failure;
  return parsed;
}

Reader::Parsed Reader::bad_token(const Token& token) const {
  Parsed parsed;
  const auto reason =
      token.kind == TokenKind::End ? SyntaxReason::IncompleteTerm : SyntaxReason::UnexpectedToken;
  parsed.failure = SyntaxFailure{token.line, token.column, token.text, reason};
  return parsed;
}

Reader::Parsed Reader::max_arity(const Token& token) const {
  Parsed parsed;
  parsed.failure = SyntaxFailure{token.line, token.column, token.text, SyntaxReason::MaxArity};
  return parsed;
}

Reader::Parsed Reader::atom_named(std::string_view name) {
  Parsed parsed;
  parsed.term = heap_.atom(heap_.atoms().intern(name));
  return parsed;
}

Cell Reader::intern_variable(std::string name) {
  // `_` だけは出現ごとに別の REF とし、名前表にもシングルトンにも入れない。
  if (name == "_") {
    const Cell cell = heap_.fresh_ref();
    occurrences_.push_back(cell);
    return cell;
  }
  for (VarRec& var : vars_) {
    if (var.name != name) continue;
    var.count += 1;
    return var.cell;
  }
  VarRec created{std::move(name), heap_.fresh_ref(), 1};
  const Cell cell = created.cell;
  vars_.push_back(std::move(created));
  occurrences_.push_back(cell);
  return cell;
}

Cell Reader::make_string(std::string_view text) {
  if (quotes_ == DoubleQuotes::Atom) return heap_.atom(heap_.atoms().intern(text));
  const auto nil = heap_.atom(heap_.atoms().intern("[]"));
  std::vector<Cell> items;
  for (std::size_t index = 0; index < text.size();) {
    const Decoded decoded = decode_utf8(text, index);
    if (!decoded.valid) break;
    if (quotes_ == DoubleQuotes::Chars) items.push_back(heap_.atom(heap_.atoms().intern_char(decoded.code)));
    else items.push_back(heap_.integer(static_cast<std::int64_t>(decoded.code)));
    index += decoded.bytes;
  }
  Cell list = nil;
  for (auto item = items.rbegin(); item != items.rend(); ++item) list = heap_.list(*item, list);
  if (items.empty()) return nil;
  return list;
}

bool Reader::can_start(const Token& token) const {
  switch (token.kind) {
    case TokenKind::Integer:
    case TokenKind::Float:
    case TokenKind::Variable:
    case TokenKind::String:
    case TokenKind::Name:
      return true;
    case TokenKind::Solo:
      if (token.text == "(" || token.text == "[" || token.text == "{") return true;
      // カットはアトム（6.3.1.3）。始められないと前置が引数を取らず、\+ ! が項にならない。
      if (token.text == "!") return true;
      return find_prefix(token.text, 1200).has_value();
    default:
      return false;
  }
}

std::optional<OpDef> Reader::find_prefix(std::string_view name, int max_priority) const {
  std::optional<OpDef> best;
  for (const OpDef& op : operators_.lookup(name)) {
    if (!is_prefix(op.specifier) || op.priority > max_priority) continue;
    if (!best || op.priority > best->priority) best = op;
  }
  return best;
}

std::optional<OpDef> Reader::find_follower(std::string_view name, int left_priority,
                                           int max_priority) const {
  std::optional<OpDef> best;
  for (const OpDef& op : operators_.lookup(name)) {
    if (!is_infix(op.specifier) && !is_postfix(op.specifier)) continue;
    if (op.priority > max_priority) continue;
    const bool too_loose = y_on_left(op.specifier) ? left_priority > op.priority : left_priority >= op.priority;
    if (too_loose) continue;
    if (!best || op.priority > best->priority) best = op;
  }
  return best;
}

Reader::Parsed Reader::parse_literal() {
  const Token token = consume().token;
  Parsed parsed;
  if (token.kind == TokenKind::Integer) parsed.term = heap_.integer(token.integer);
  else if (token.kind == TokenKind::Float) parsed.term = heap_.floating(token.floating);
  else if (token.kind == TokenKind::Variable) parsed.term = intern_variable(token.text);
  else parsed.term = make_string(token.text);
  return parsed;
}

Reader::Parsed Reader::apply_prefix(const OpDef& op) {
  const int arg_max = op.specifier == Specifier::Fy ? op.priority : op.priority - 1;
  Parsed arg = parse_expr(arg_max);
  if (arg.failure) return arg;
  Parsed parsed;
  parsed.term = heap_.structure(heap_.atoms().intern(op.name), std::array<Cell, 1>{arg.term});
  parsed.priority = op.priority;
  return parsed;
}

Reader::Parsed Reader::parse_name(int max_priority) {
  const Token token = consume().token;
  const Scan next = peek();
  if (next.failure) return failure_of(next);
  // 空白を挟まない '(' だけが関数表記。`\+ (true, fail)` は前置の引数である。
  if (next.token.kind == TokenKind::Solo && next.token.text == "(" && !next.token.after_layout) {
    consume();
    return parse_arguments(token.text);
  }
  // 6.3.4.1: 名前 - の次の数は、空白を挟んでも負の定数。前置演算子の -(1) は関数表記で上が取る。
  if (token.kind == TokenKind::Name && token.text == "-" &&
      (next.token.kind == TokenKind::Integer || next.token.kind == TokenKind::Float)) {
    const Token number = consume().token;
    Parsed parsed;
    if (number.kind == TokenKind::Integer) parsed.term = heap_.integer(-number.integer);
    else parsed.term = heap_.floating(-number.floating);
    return parsed;
  }
  if (const std::optional<OpDef> prefix = find_prefix(token.text, max_priority)) {
    if (can_start(next.token)) return apply_prefix(*prefix);
  }
  return atom_named(token.text);
}

Reader::Parsed Reader::parse_prefix(int max_priority) {
  const Scan scan = peek();
  if (scan.failure) return failure_of(scan);
  const Token& token = scan.token;
  if (token.kind == TokenKind::End || token.kind == TokenKind::EndFullStop) return bad_token(token);
  if (token.kind == TokenKind::Solo && token.text == "(") return parse_paren();
  if (token.kind == TokenKind::Solo && token.text == "[") return parse_list();
  if (token.kind == TokenKind::Solo && token.text == "{") return parse_curly();
  if (token.kind == TokenKind::Integer || token.kind == TokenKind::Float ||
      token.kind == TokenKind::Variable || token.kind == TokenKind::String) {
    return parse_literal();
  }
  if (token.kind == TokenKind::Solo && is_closer(token.text)) return bad_token(token);
  if (token.kind == TokenKind::Name || token.kind == TokenKind::Solo) return parse_name(max_priority);
  return bad_token(token);
}

Reader::Parsed Reader::parse_rest(Parsed left, int max_priority) {
  while (!left.failure) {
    const Scan scan = peek();
    if (scan.failure) return failure_of(scan);
    if (scan.token.kind != TokenKind::Name && scan.token.kind != TokenKind::Solo) return left;
    const std::optional<OpDef> op = find_follower(scan.token.text, left.priority, max_priority);
    if (!op) return left;
    consume();
    if (is_postfix(op->specifier)) {
      left.term = heap_.structure(heap_.atoms().intern(op->name), std::array<Cell, 1>{left.term});
      left.priority = op->priority;
      continue;
    }
    const int right_max = op->specifier == Specifier::Xfy ? op->priority : op->priority - 1;
    Parsed right = parse_expr(right_max);
    if (right.failure) return right;
    left.term = heap_.structure(heap_.atoms().intern(op->name), std::array<Cell, 2>{left.term, right.term});
    left.priority = op->priority;
  }
  return left;
}

Reader::Parsed Reader::parse_expr(int max_priority) {
  Parsed left = parse_prefix(max_priority);
  if (left.failure) return left;
  return parse_rest(std::move(left), max_priority);
}

void Reader::absorb_full_stop(const Scan& offending) {
  // 終止符そのものが不正なら、字句はドットだけ消費し確認の空白を残してある。
  if (!offending.failure &&
      (offending.token.kind == TokenKind::End || offending.token.kind == TokenKind::EndFullStop)) {
    return;
  }
  has_pending_ = false;
  // 数の失敗はドットの上で張り付く。外さないと、そのドットが次の項に残る。
  if (offending.failure && !lexer_.release_failure()) return;
  while (true) {
    const std::size_t before = lexer_.offset();
    const Scan scan = lexer_.next();
    if (scan.failure) {
      if (!lexer_.release_failure() || lexer_.offset() == before) return;
      continue;
    }
    if (scan.token.kind == TokenKind::End || scan.token.kind == TokenKind::EndFullStop) return;
    if (lexer_.offset() == before) return;
  }
}

ReadOutcome Reader::next() {
  vars_.clear();
  occurrences_.clear();
  const Scan first = peek();
  if (first.failure) {
    ReadOutcome outcome;
    outcome.failure = first.failure;
    absorb_full_stop(first);
    return outcome;
  }
  if (first.token.kind == TokenKind::End) {
    ReadOutcome outcome;
    outcome.eof = true;
    return outcome;
  }
  Parsed parsed = parse_expr(1200);
  if (parsed.failure) {
    ReadOutcome outcome;
    outcome.failure = parsed.failure;
    if (has_pending_) absorb_full_stop(pending_);
    return outcome;
  }
  const Scan end = peek();
  if (end.failure) {
    ReadOutcome outcome;
    outcome.failure = end.failure;
    absorb_full_stop(end);
    return outcome;
  }
  if (end.token.kind != TokenKind::EndFullStop) {
    ReadOutcome outcome;
    outcome.failure = bad_token(end.token).failure;
    absorb_full_stop(end);
    return outcome;
  }
  consume();
  ReadTerm term;
  term.term = parsed.term;
  term.line = first.token.line;
  term.column = first.token.column;
  for (const VarRec& var : vars_) {
    term.names.push_back(VarName{var.name, var.cell});
    if (var.count == 1) term.singletons.push_back(VarName{var.name, var.cell});
  }
  term.variables = occurrences_;
  ReadOutcome outcome;
  outcome.term = std::move(term);
  return outcome;
}

}
