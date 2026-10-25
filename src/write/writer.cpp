#include "mosa/writer.hpp"

#include "mosa/char_class.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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

Decoded decode_at(std::string_view text, std::size_t index) {
  if (index >= text.size()) return {};
  const auto lead = static_cast<unsigned char>(text[index]);
  if (lead <= 0x7FU) return Decoded{lead, 1, true};
  const int first = continuation(text, index + 1);
  if (first < 0) return {};
  if (lead <= 0xDFU && lead >= 0xC2U) {
    return Decoded{static_cast<char32_t>(((lead & 0x1FU) << 6) | first), 2, true};
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

std::optional<char32_t> first_cp(std::string_view text) {
  const Decoded decoded = decode_at(text, 0);
  if (!decoded.valid) return std::nullopt;
  return decoded.code;
}

std::optional<char32_t> last_cp(std::string_view text) {
  if (text.empty()) return std::nullopt;
  std::size_t index = 0;
  Decoded last;
  while (index < text.size()) {
    const Decoded decoded = decode_at(text, index);
    if (!decoded.valid) return std::nullopt;
    last = decoded;
    index += decoded.bytes;
  }
  return last.code;
}

bool continues_name(char32_t code) {
  const CharClass kind = classify(code);
  return kind == CharClass::Small || kind == CharClass::Capital || kind == CharClass::Underline ||
         kind == CharClass::Digit || kind == CharClass::Mark;
}

bool every_code(std::string_view text, bool (*ok)(char32_t)) {
  if (text.empty()) return false;
  for (std::size_t index = 0; index < text.size();) {
    const Decoded decoded = decode_at(text, index);
    if (!decoded.valid || !ok(decoded.code)) return false;
    index += decoded.bytes;
  }
  return true;
}

bool is_graphic_token(std::string_view text) {
  return every_code(text, [](char32_t code) { return classify(code) == CharClass::Graphic; });
}

bool is_small_name(std::string_view text) {
  const auto first = first_cp(text);
  if (!first || classify(*first) != CharClass::Small) return false;
  const Decoded decoded = decode_at(text, 0);
  return every_code(text.substr(decoded.bytes), continues_name) || text.size() == decoded.bytes;
}

bool is_variable_spelling(std::string_view text) {
  const auto first = first_cp(text);
  if (!first) return false;
  const CharClass kind = classify(*first);
  if (kind != CharClass::Capital && kind != CharClass::Underline) return false;
  const Decoded decoded = decode_at(text, 0);
  if (text.size() == decoded.bytes) return true;
  return every_code(text.substr(decoded.bytes), continues_name);
}

bool is_solo(std::string_view text) {
  const Decoded decoded = decode_at(text, 0);
  return decoded.valid && decoded.bytes == text.size() && classify(decoded.code) == CharClass::Solo;
}

std::uint32_t atom_of(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

bool is_nil(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Atom && heap.atoms().name(atom_of(cell)) == "[]";
}

bool is_infix(Specifier specifier) {
  return specifier == Specifier::Xfx || specifier == Specifier::Xfy || specifier == Specifier::Yfx;
}

bool is_prefix(Specifier specifier) { return specifier == Specifier::Fx || specifier == Specifier::Fy; }

bool is_postfix(Specifier specifier) { return specifier == Specifier::Xf || specifier == Specifier::Yf; }

std::string quote_atom(std::string_view text) {
  std::string out = "'";
  for (std::size_t index = 0; index < text.size();) {
    const Decoded decoded = decode_at(text, index);
    if (!decoded.valid) break;
    if (decoded.code == U'\\') out += "\\\\";
    else if (decoded.code == U'\'') out += "\\'";
    else if (decoded.code == U'\n') out += "\\n";
    else if (decoded.code == U'\t') out += "\\t";
    else out.append(text.substr(index, decoded.bytes));
    index += decoded.bytes;
  }
  out += '\'';
  return out;
}

std::string format_float(double value) {
  if (value == 0.0 && std::signbit(value)) return "-0.0";
  char buffer[128];
  const std::to_chars_result result =
      std::to_chars(buffer, buffer + sizeof buffer, value, std::chars_format::general);
  std::string text = result.ec == std::errc{} ? std::string(buffer, result.ptr) : "0.0";
  for (char& ch : text) {
    if (ch == 'E') ch = 'e';
  }
  if (text.find('.') == std::string::npos) {
    const auto exponent = text.find('e');
    if (exponent == std::string::npos) text += ".0";
    else text.insert(exponent, ".0");
  }
  return text;
}

class Printer {
 public:
  Printer(const Heap& heap, const OperatorTable& operators, WriteOptions options,
          std::span<const VarName> names)
      : heap_(heap), operators_(operators), options_(options), names_(names) {}

  void collect(Cell term) { count(term); }
  [[nodiscard]] std::string print(Cell term) { return render(term, 1200, true); }

 private:
  void count(Cell term);
  [[nodiscard]] std::string render(Cell term, int parent, bool allow_equal);
  [[nodiscard]] std::string render_op(const OpDef& op, Cell term);
  [[nodiscard]] std::string render_list(Cell term);
  [[nodiscard]] std::string render_functor(Cell term, std::string_view name);
  [[nodiscard]] std::optional<OpDef> as_operator(Cell term) const;
  [[nodiscard]] std::string spell(std::string_view name) const;
  [[nodiscard]] std::string spell_functor(std::string_view name) const;
  [[nodiscard]] const VarName* var_name(Cell term) const;
  [[nodiscard]] std::string var_text(Cell term);
  [[nodiscard]] static bool glue(std::string_view left, std::string_view right);
  [[nodiscard]] static bool needs_paren(int priority, int parent, bool allow_equal);

  const Heap& heap_;
  const OperatorTable& operators_;
  WriteOptions options_;
  std::span<const VarName> names_;
  std::unordered_map<std::uint64_t, int> counts_;
  std::unordered_map<std::uint64_t, int> assigned_;
  std::vector<std::uint64_t> open_;
  int next_number_ = 1;

  [[nodiscard]] bool opened(std::uint64_t raw) const {
    for (const auto item : open_) {
      if (item == raw) return true;
    }
    return false;
  }
};

void Printer::count(Cell term) {
  // 名前のある REF は束縛を追わない。答えの X = [X] がここで止まらないようにする。
  if (term.tag() == Tag::Ref && var_name(term) != nullptr) return;
  term = heap_.deref(term);
  if (heap_.unbound(term)) {
    if (var_name(term) == nullptr) counts_[term.raw()] += 1;
    return;
  }
  if (term.tag() != Tag::Lis && term.tag() != Tag::Str) return;
  if (opened(term.raw())) return;
  open_.push_back(term.raw());
  if (term.tag() == Tag::Lis) {
    count(heap_.list_head(term));
    count(heap_.list_tail(term));
  } else {
    const auto arity = heap_.functor_arity(term);
    for (std::uint16_t index = 0; index < arity; ++index) count(heap_.arg(term, index));
  }
  open_.pop_back();
}

const VarName* Printer::var_name(Cell term) const {
  for (const VarName& var : names_) {
    if (var.cell.raw() == term.raw()) return &var;
  }
  return nullptr;
}

std::string Printer::var_text(Cell term) {
  if (const VarName* var = var_name(term)) return var->name;
  if (counts_[term.raw()] <= 1) return "_";
  const auto found = assigned_.find(term.raw());
  const int number = found == assigned_.end() ? next_number_++ : found->second;
  if (found == assigned_.end()) assigned_.emplace(term.raw(), number);
  return "_G" + std::to_string(number);
}

bool Printer::glue(std::string_view left, std::string_view right) {
  const auto end = last_cp(left);
  const auto start = first_cp(right);
  if (!end || !start) return false;
  if (classify(*end) == CharClass::Graphic && classify(*start) == CharClass::Graphic) return true;
  return continues_name(*end) && continues_name(*start);
}

bool Printer::needs_paren(int priority, int parent, bool allow_equal) {
  if (allow_equal) return priority > parent;
  return priority >= parent;
}

std::string Printer::spell(std::string_view name) const {
  if (!options_.quoted) return std::string{name};
  if (name.empty() || is_variable_spelling(name)) return quote_atom(name);
  if (name == "[]" || name == "{}") return std::string{name};
  if (is_small_name(name) || is_graphic_token(name) || is_solo(name)) return std::string{name};
  return quote_atom(name);
}

std::string Printer::spell_functor(std::string_view name) const {
  // '.' と '{}' を関数表記のまま書くと、リストや中括弧として読まれる。
  if (options_.quoted && (name == "." || name == "{}")) return quote_atom(name);
  return spell(name);
}

std::optional<OpDef> Printer::as_operator(Cell term) const {
  if (options_.ignore_ops || term.tag() != Tag::Str) return std::nullopt;
  const auto arity = heap_.functor_arity(term);
  const std::string_view name = heap_.atoms().name(heap_.functor_atom(term));
  std::optional<OpDef> prefix;
  std::optional<OpDef> postfix;
  std::optional<OpDef> infix;
  for (const OpDef& op : operators_.lookup(name)) {
    if (arity == 1 && is_prefix(op.specifier) && (!prefix || op.priority > prefix->priority)) prefix = op;
    if (arity == 1 && is_postfix(op.specifier) && (!postfix || op.priority > postfix->priority)) postfix = op;
    if (arity == 2 && is_infix(op.specifier) && (!infix || op.priority > infix->priority)) infix = op;
  }
  if (arity == 2) return infix;
  if (prefix) return prefix;
  return postfix;
}

std::string Printer::render_op(const OpDef& op, Cell term) {
  const std::string name = spell(op.name);
  if (is_prefix(op.specifier) || is_postfix(op.specifier)) {
    const bool allow = op.specifier == Specifier::Fy || op.specifier == Specifier::Yf;
    const std::string arg = render(heap_.arg(term, 0), op.priority, allow);
    const bool space = is_prefix(op.specifier) ? glue(name, arg) : glue(arg, name);
    // `\+(` は関数表記になる。空白を置くと '(' は括弧であり、前置の引数のまま読み戻せる。
    const bool call_form = is_prefix(op.specifier) && !arg.empty() && arg.front() == '(';
    if (is_prefix(op.specifier)) return (space || call_form) ? name + " " + arg : name + arg;
    return space ? arg + " " + name : arg + name;
  }
  const std::string left = render(heap_.arg(term, 0), op.priority, op.specifier == Specifier::Yfx);
  const std::string right = render(heap_.arg(term, 1), op.priority, op.specifier == Specifier::Xfy);
  std::string out = left;
  if (glue(left, name)) out += ' ';
  out += name;
  if (name == "," || glue(name, right)) out += ' ';
  out += right;
  return out;
}

std::string Printer::render_list(Cell term) {
  if (options_.ignore_ops) {
    const std::string head = render(heap_.list_head(term), 999, true);
    const std::string tail = render(heap_.list_tail(term), 999, true);
    return spell_functor(".") + "(" + head + ", " + tail + ")";
  }
  std::string out = "[";
  bool first = true;
  Cell current = term;
  while (true) {
    if (!first) out += ", ";
    first = false;
    out += render(heap_.list_head(current), 999, true);
    const Cell raw_tail = heap_.list_tail(current);
    // 名前表の REF は束縛より先に書く。先に辿ると X = [a|X] が [a|_] になる。
    if (raw_tail.tag() == Tag::Ref && var_name(raw_tail) != nullptr) {
      const std::string rest = render(raw_tail, 999, true);
      if (glue(out, "|")) out += ' ';
      out += '|';
      if (glue("|", rest)) out += ' ';
      out += rest;
      break;
    }
    const Cell tail = heap_.deref(raw_tail);
    if (is_nil(heap_, tail)) break;
    if (opened(tail.raw())) {
      out += "|_";
      break;
    }
    if (tail.tag() != Tag::Lis) {
      const std::string rest = render(tail, 999, true);
      if (glue(out, "|")) out += ' ';
      out += '|';
      if (glue("|", rest)) out += ' ';
      out += rest;
      break;
    }
    current = tail;
  }
  out += ']';
  return out;
}

std::string Printer::render_functor(Cell term, std::string_view name) {
  std::string out = spell_functor(name);
  out += '(';
  const auto arity = heap_.functor_arity(term);
  for (std::uint16_t index = 0; index < arity; ++index) {
    if (index != 0) out += ", ";
    out += render(heap_.arg(term, index), 999, true);
  }
  out += ')';
  return out;
}

std::optional<std::string> numbervar_text(const Heap& heap, Cell term) {
  if (term.tag() != Tag::Str || heap.functor_arity(term) != 1) return std::nullopt;
  if (heap.atoms().name(heap.functor_atom(term)) != "$VAR") return std::nullopt;
  const Cell arg = heap.deref(heap.arg(term, 0));
  if (arg.tag() != Tag::Int && arg.tag() != Tag::IntBox) return std::nullopt;
  const auto value = heap.integer_value(arg);
  if (value < 0) return std::nullopt;
  const auto index = static_cast<std::uint64_t>(value);
  std::string name(1, static_cast<char>('A' + static_cast<int>(index % 26U)));
  const auto suffix = index / 26U;
  if (suffix != 0) name += std::to_string(suffix);
  return name;
}

std::string Printer::render(Cell term, int parent, bool allow_equal) {
  if (term.tag() == Tag::Ref && var_name(term) != nullptr) return var_name(term)->name;
  term = heap_.deref(term);
  if (heap_.unbound(term)) return var_text(term);
  if (term.tag() == Tag::Atom) return spell(heap_.atoms().name(atom_of(term)));
  if (term.tag() == Tag::Int || term.tag() == Tag::IntBox) return std::to_string(heap_.integer_value(term));
  if (term.tag() == Tag::Float) return format_float(heap_.float_value(term));
  // '$VAR'(N) は関数表記より先。N は 0 以上の整数だけが変数名になる。
  if (options_.numbervars) {
    if (const std::optional<std::string> named = numbervar_text(heap_, term)) return *named;
  }
  if (term.tag() != Tag::Lis && term.tag() != Tag::Str) return {};
  if (opened(term.raw())) return "_";
  open_.push_back(term.raw());
  std::string text;
  if (term.tag() == Tag::Lis) text = render_list(term);
  else if (const std::optional<OpDef> op = as_operator(term)) {
    if (needs_paren(op->priority, parent, allow_equal)) {
      open_.pop_back();
      return "(" + render(term, 1200, true) + ")";
    }
    text = render_op(*op, term);
  } else {
    const std::string_view name = heap_.atoms().name(heap_.functor_atom(term));
    if (!options_.ignore_ops && name == "{}" && heap_.functor_arity(term) == 1) {
      text = "{" + render(heap_.arg(term, 0), 1200, true) + "}";
    } else {
      text = render_functor(term, name);
    }
  }
  open_.pop_back();
  return text;
}

}  // namespace

std::string write_term(const Heap& heap, const OperatorTable& operators, Cell term, WriteOptions options,
                       std::span<const VarName> names) {
  Printer printer(heap, operators, options, names);
  printer.collect(term);
  return printer.print(term);
}

}
