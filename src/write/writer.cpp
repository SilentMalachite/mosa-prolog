#include "mosa/writer.hpp"

#include "mosa/char_class.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
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

// ! と ; 以外の単独文字は、引用しないと括弧や区切りとして読まれる。
// 図形の先頭の /* は読み戻すとブロック注釈になる。途中の /* と */ は図形のまま読める。
// 単独の . は終止符なので、図形でも引用しないとアトムとして読めない。
bool bare_atom(std::string_view name) {
  if (name == "[]" || name == "{}") return true;
  if (is_small_name(name)) return true;
  if (is_graphic_token(name)) return name != "." && !name.starts_with("/*");
  return name == "!" || name == ";";
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

// 十六進は制御文字だけ。引用の外で字句にならない文字でも、字形があれば生のまま。
bool unprintable(char32_t code) {
  return code < 0x20 || code == 0x7F || (code >= 0x80 && code < 0xA0);
}

// スイートは \x2\ と \x11\ の字面を比べる。桁を揃えると不一致になる。
std::string hex_escape(char32_t code) {
  constexpr std::string_view digits = "0123456789abcdef";
  std::string body;
  auto value = static_cast<std::uint32_t>(code);
  do {
    body.push_back(digits[value & 0xFU]);
    value >>= 4;
  } while (value != 0);
  std::string out = "\\x";
  for (auto it = body.rbegin(); it != body.rend(); ++it) out.push_back(*it);
  out.push_back('\\');
  return out;
}

std::string quote_atom(std::string_view text) {
  std::string out = "'";
  for (std::size_t index = 0; index < text.size();) {
    const Decoded decoded = decode_at(text, index);
    if (!decoded.valid) break;
    if (decoded.code == U'\\') out += "\\\\";
    else if (decoded.code == U'\'') out += "\\'";
    else if (decoded.code == U'\n') out += "\\n";
    else if (decoded.code == U'\t') out += "\\t";
    else if (unprintable(decoded.code)) out += hex_escape(decoded.code);
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

// 書き出しの仕事。C++ の再帰を使わず work_ に積む（ADR 0011）。積む順は書く順の逆。
enum class Step : std::uint8_t { Term, Text, Infix, Postfix, Bar, ListRest, Clear, Leave };

// 字句を書いた後、次の字句の前に空白を置くかの判断の種類。Prefix は `(` で始まる引数にも空白を置く。
enum class Space : std::uint8_t { None, Glue, Prefix };

struct Work {
  Step step = Step::Term;
  Cell term{};
  int parent = 1200;
  bool allow_equal = true;
  // 括弧を足し直す再入では、失敗した portray を繰り返さない。
  bool allow_hook = true;
  Space then = Space::None;
  // Infix と Postfix で、左の被演算子を書き始めたときの out_ の長さ。
  std::size_t mark = 0;
  // Text、Infix、Bar が始める待ちの番号。Clear はこの番号の待ちだけを取り消す。
  std::size_t id = 0;
  std::string text;
};

// 待ち: 直前の字句と、この後に最初に書く字句の間の空白。古い writer の glue(左の字面, 右の字面) と同じく、
// 右の字面が空のまま終われば空白は置かない（Clear が取り消す）。
struct Pending {
  bool active = false;
  Space kind = Space::None;
  std::optional<char32_t> left;
  std::size_t id = 0;
};

std::optional<char32_t> final_cp(std::string_view text) {
  if (text.empty()) return std::nullopt;
  std::size_t start = text.size() - 1;
  while (start > 0 && text.size() - start < 4 && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U) {
    --start;
  }
  const Decoded decoded = decode_at(text, start);
  if (!decoded.valid || start + decoded.bytes != text.size()) return std::nullopt;
  return decoded.code;
}

class Printer final : public WriteHeld {
 public:
  Printer(const Heap& heap, OperatorView operators, WriteOptions options,
          std::span<const VarName> names, WriteReplace replace, void* context)
      : heap_(heap), operators_(operators), options_(options), names_(names), replace_(replace),
        context_(context) {
    // replace の間の GC は名前のセルも動かす。呼び手の表は const なので、写しを GC に見せる。
    if (replace_ != nullptr) {
      own_names_.assign(names.begin(), names.end());
      names_ = own_names_;
    }
  }

  void collect(Cell term);
  [[nodiscard]] std::string print(Cell term);
  void visit(WriteRoots& roots) override;

 private:
  void run(Work& item);
  void settle();
  void render(const Work& item);
  void render_compound(Cell term, int parent, bool allow_equal);
  void push_op(const OpDef& op, Cell term);
  void push_prefix(const OpDef& op, Cell term, std::string name);
  void push_operand(Cell arg, int priority, bool allow_equal);
  void push_list(Cell term);
  void list_rest(Cell cell);
  void push_bar(Cell rest);
  void push_functor(Cell term, std::string_view name);
  void push_term(Cell term, int parent, bool allow_equal, bool allow_hook = true);
  void push_text(std::string text, Space then = Space::None, std::size_t id = 0);
  void push_clear(std::size_t id);
  void push_comma(std::size_t id);
  void emit(std::string_view text);
  void await(Space kind, std::size_t id);
  [[nodiscard]] bool left_glue(std::size_t mark, std::string_view next) const;
  [[nodiscard]] std::optional<OpDef> as_operator(Cell term) const;
  [[nodiscard]] bool operator_atom(Cell term) const;
  [[nodiscard]] bool unsigned_number(Cell term) const;
  [[nodiscard]] bool leftmost_unsigned_number(Cell term, int priority, bool allow_equal) const;
  [[nodiscard]] std::string spell(std::string_view name) const;
  [[nodiscard]] std::string spell_operator(std::string_view name) const;
  [[nodiscard]] std::string spell_functor(std::string_view name) const;
  [[nodiscard]] const VarName* var_name(Cell term) const;
  [[nodiscard]] std::string var_text(Cell term);
  [[nodiscard]] static bool glue(std::optional<char32_t> end, std::optional<char32_t> start);
  [[nodiscard]] static bool needs_paren(int priority, int parent, bool allow_equal);

  const Heap& heap_;
  OperatorView operators_;
  WriteOptions options_;
  std::span<const VarName> names_;
  std::vector<VarName> own_names_;
  std::unordered_map<std::uint64_t, int> counts_;
  std::unordered_map<std::uint64_t, int> assigned_;
  // 今書いている項の祖先。循環項はここで _ にして止める。
  std::unordered_set<std::uint64_t> open_;
  std::vector<Work> work_;
  std::string out_;
  Pending pending_;
  std::size_t next_id_ = 1;
  int next_number_ = 1;
  WriteReplace replace_ = nullptr;
  void* context_ = nullptr;
  bool aborted_ = false;
  // replace に渡している項。GC の訪問が書き換える。
  Cell hooked_{};
  // replace の間に GC が来たら真。鍵の写しを訪ね、戻った後で settle が表を作り直す。
  bool held_ = false;
  std::vector<std::pair<std::uint64_t, int>> held_counts_;
  std::vector<std::pair<std::uint64_t, int>> held_assigned_;
};

void Printer::visit(WriteRoots& roots) {
  // map の鍵はその場で書き換えられない。初めの訪問で写しを作り、同じ replace の間の GC はその写しを訪ねる。
  if (!held_) {
    held_counts_.assign(counts_.begin(), counts_.end());
    held_assigned_.assign(assigned_.begin(), assigned_.end());
    held_ = true;
  }
  roots.cell(hooked_);
  for (Work& item : work_) roots.cell(item.term);
  for (VarName& var : own_names_) roots.cell(var.cell);
  // 書き終えて残りの仕事から届かない変数は二度と書かないので、弱い参照で落としてよい。
  for (auto& entry : held_counts_) roots.weak(entry.first);
  for (auto& entry : held_assigned_) roots.weak(entry.first);
}

void Printer::settle() {
  if (!held_) return;
  held_ = false;
  counts_.clear();
  for (const auto& [raw, count] : held_counts_) {
    if (raw != 0) counts_.emplace(raw, count);
  }
  assigned_.clear();
  for (const auto& [raw, number] : held_assigned_) {
    if (raw != 0) assigned_.emplace(raw, number);
  }
  held_counts_.clear();
  held_assigned_.clear();
  // 祖先の集まりは積んだ Leave の項と同じなので、動いた番地で数え直す。
  open_.clear();
  for (const Work& item : work_) {
    if (item.step == Step::Leave) open_.insert(item.term.raw());
  }
}

void Printer::collect(Cell root) {
  // 2 番目は「祖先から外す」の印。名前のある REF は束縛を追わない。答えの X = [X] がここで止まらないようにする。
  std::vector<std::pair<Cell, bool>> stack{{root, false}};
  while (!stack.empty()) {
    auto [term, leave] = stack.back();
    stack.pop_back();
    if (leave) {
      open_.erase(term.raw());
      continue;
    }
    if (term.tag() == Tag::Ref && var_name(term) != nullptr) continue;
    term = heap_.deref(term);
    if (heap_.unbound(term)) {
      if (var_name(term) == nullptr) counts_[term.raw()] += 1;
      continue;
    }
    if (term.tag() != Tag::Lis && term.tag() != Tag::Str) continue;
    if (!open_.insert(term.raw()).second) continue;
    stack.emplace_back(term, true);
    if (term.tag() == Tag::Lis) {
      stack.emplace_back(heap_.list_tail(term), false);
      stack.emplace_back(heap_.list_head(term), false);
    } else {
      const auto arity = heap_.functor_arity(term);
      for (std::uint16_t index = arity; index > 0; --index) stack.emplace_back(heap_.arg(term, index - 1U), false);
    }
  }
}

std::string Printer::print(Cell term) {
  push_term(term, 1200, true);
  while (!work_.empty() && !aborted_) {
    Work item = std::move(work_.back());
    work_.pop_back();
    run(item);
  }
  return std::move(out_);
}

void Printer::run(Work& item) {
  switch (item.step) {
    case Step::Term: render(item); return;
    case Step::Text:
      emit(item.text);
      if (item.then != Space::None) await(item.then, item.id);
      return;
    case Step::Infix:
      if (left_glue(item.mark, item.text)) out_ += ' ';
      emit(item.text);
      await(Space::Glue, item.id);
      return;
    case Step::Postfix:
      if (left_glue(item.mark, item.text)) out_ += ' ';
      emit(item.text);
      return;
    case Step::Bar:
      if (glue(final_cp(out_), U'|')) out_ += ' ';
      emit("|");
      await(Space::Glue, item.id);
      return;
    case Step::ListRest: list_rest(item.term); return;
    case Step::Clear:
      if (pending_.active && pending_.id == item.id) pending_.active = false;
      return;
    case Step::Leave: open_.erase(item.term.raw()); return;
  }
}

void Printer::emit(std::string_view text) {
  if (text.empty()) return;
  if (pending_.active) {
    pending_.active = false;
    // `\+(` は関数表記になる。空白を置くと '(' は括弧であり、前置の引数のまま読み戻せる。
    const bool call_form = pending_.kind == Space::Prefix && text.front() == '(';
    if (call_form || glue(pending_.left, first_cp(text))) out_ += ' ';
  }
  out_ += text;
}

void Printer::await(Space kind, std::size_t id) {
  pending_ = Pending{true, kind, final_cp(out_), id};
}

bool Printer::left_glue(std::size_t mark, std::string_view next) const {
  // 左の被演算子が何も書かなかったときは、その前の字句と比べない。
  return out_.size() > mark && glue(final_cp(out_), first_cp(next));
}

void Printer::push_term(Cell term, int parent, bool allow_equal, bool allow_hook) {
  Work item;
  item.term = term;
  item.parent = parent;
  item.allow_equal = allow_equal;
  item.allow_hook = allow_hook;
  work_.push_back(std::move(item));
}

void Printer::push_text(std::string text, Space then, std::size_t id) {
  Work item;
  item.step = Step::Text;
  item.text = std::move(text);
  item.then = then;
  item.id = id;
  work_.push_back(std::move(item));
}

void Printer::push_clear(std::size_t id) {
  Work item;
  item.step = Step::Clear;
  item.id = id;
  work_.push_back(std::move(item));
}

void Printer::push_comma(std::size_t id) {
  // コンマは単独字なので、直後の空白は糊付けが要るときだけ置く。space_args は右が空でも置く。
  if (options_.space_args) push_text(", ");
  else push_text(",", Space::Glue, id);
}

const VarName* Printer::var_name(Cell term) const {
  for (const VarName& var : names_) {
    if (var.cell.raw() == term.raw()) return &var;
  }
  return nullptr;
}

std::string Printer::var_text(Cell term) {
  if (const VarName* var = var_name(term)) return var->name;
  if (options_.underscore_once && counts_[term.raw()] <= 1) return "_";
  const auto found = assigned_.find(term.raw());
  const int number = found == assigned_.end() ? next_number_++ : found->second;
  if (found == assigned_.end()) assigned_.emplace(term.raw(), number);
  return "_G" + std::to_string(number);
}

bool Printer::glue(std::optional<char32_t> end, std::optional<char32_t> start) {
  if (!end || !start) return false;
  if (classify(*end) == CharClass::Graphic && classify(*start) == CharClass::Graphic) return true;
  return continues_name(*end) && continues_name(*start);
}

bool Printer::needs_paren(int priority, int parent, bool allow_equal) {
  if (allow_equal) return priority > parent;
  return priority >= parent;
}

std::string Printer::spell(std::string_view name) const {
  if (!options_.quoted || bare_atom(name)) return std::string{name};
  return quote_atom(name);
}

std::string Printer::spell_operator(std::string_view name) const {
  // アトムの ',' と '|' と '.' は引用する。演算子の位置まで引用すると字面が変わる。
  if (name == "," || name == "|" || name == ".") return std::string{name};
  return spell(name);
}

std::string Printer::spell_functor(std::string_view name) const {
  // '.'( はリストとして読まれる。{}( は複合項として読み戻せるので引用しない。
  if (options_.quoted && name == ".") return quote_atom(name);
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

bool Printer::unsigned_number(Cell term) const {
  term = heap_.deref(term);
  if (term.tag() == Tag::Int || term.tag() == Tag::IntBox) return heap_.integer_value(term) >= 0;
  return term.tag() == Tag::Float && !std::signbit(heap_.float_value(term));
}

bool Printer::operator_atom(Cell term) const {
  term = heap_.deref(term);
  if (term.tag() != Tag::Atom) return false;
  return !operators_.lookup(heap_.atoms().name(atom_of(term))).empty();
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

// 書く順をなぞって左端の被演算子を降り、字面の先頭が符号なしの数になるかを見る。括弧、前置演算子、
// 関数表記、リスト、{}、変数、'$VAR' は数以外の字で始まるので止める。降りの中で同じ項に戻るのは循環で、
// writer はそこに _ を書く。portray が置き換える字面は見ない（7.10.5 で portray の出力は利用者が決める）。
bool Printer::leftmost_unsigned_number(Cell term, int priority, bool allow_equal) const {
  std::unordered_set<std::uint64_t> seen;
  while (true) {
    if (term.tag() == Tag::Ref && var_name(term) != nullptr) return false;
    term = heap_.deref(term);
    if (unsigned_number(term)) return true;
    if (term.tag() != Tag::Str || open_.contains(term.raw()) || !seen.insert(term.raw()).second) return false;
    if (options_.numbervars && numbervar_text(heap_, term)) return false;
    const std::optional<OpDef> op = as_operator(term);
    if (!op || is_prefix(op->specifier) || needs_paren(op->priority, priority, allow_equal)) return false;
    priority = op->priority;
    allow_equal = op->specifier == Specifier::Yfx || op->specifier == Specifier::Yf;
    term = heap_.arg(term, 0);
  }
}

void Printer::render(const Work& item) {
  Cell term = item.term;
  if (item.allow_hook && replace_ != nullptr) {
    std::string swapped;
    // replace は GC を走らせうる。訪問が hooked_ を新しい番地に直すので、戻った後はそちらを読む。
    hooked_ = term;
    const WriteCut cut = replace_(context_, term, swapped, *this);
    term = hooked_;
    hooked_ = Cell{};
    settle();
    if (cut == WriteCut::Stop) {
      aborted_ = true;
      return;
    }
    // 成功した portray は、変数名や '$VAR' より先にその字面を使う。
    if (cut == WriteCut::Swap) return emit(swapped);
  }
  if (term.tag() == Tag::Ref && var_name(term) != nullptr) return emit(var_name(term)->name);
  term = heap_.deref(term);
  if (heap_.unbound(term)) return emit(var_text(term));
  if (term.tag() == Tag::Atom) return emit(spell(heap_.atoms().name(atom_of(term))));
  if (term.tag() == Tag::Int || term.tag() == Tag::IntBox) return emit(std::to_string(heap_.integer_value(term)));
  if (term.tag() == Tag::Float) return emit(format_float(heap_.float_value(term)));
  // '$VAR'(N) は関数表記より先。N は 0 以上の整数だけが変数名になる。
  if (options_.numbervars) {
    if (const std::optional<std::string> named = numbervar_text(heap_, term)) return emit(*named);
  }
  if (term.tag() != Tag::Lis && term.tag() != Tag::Str) return;
  if (open_.contains(term.raw())) return emit("_");
  render_compound(term, item.parent, item.allow_equal);
}

void Printer::render_compound(Cell term, int parent, bool allow_equal) {
  const std::optional<OpDef> op = term.tag() == Tag::Str ? as_operator(term) : std::nullopt;
  if (op && needs_paren(op->priority, parent, allow_equal)) {
    // この項の portray はもう失敗している。括弧の中でもう一度呼ぶと副作用が二重になる。
    emit("(");
    push_text(")");
    push_term(term, 1200, true, false);
    return;
  }
  open_.insert(term.raw());
  Work leave;
  leave.step = Step::Leave;
  leave.term = term;
  work_.push_back(std::move(leave));
  if (term.tag() == Tag::Lis) return push_list(term);
  if (op) return push_op(*op, term);
  const std::string_view name = heap_.atoms().name(heap_.functor_atom(term));
  if (!options_.ignore_ops && name == "{}" && heap_.functor_arity(term) == 1) {
    emit("{");
    push_text("}");
    return push_term(heap_.arg(term, 0), 1200, true);
  }
  push_functor(term, name);
}

void Printer::push_op(const OpDef& op, Cell term) {
  std::string name = spell_operator(op.name);
  if (is_prefix(op.specifier)) return push_prefix(op, term, std::move(name));
  Work tail;
  tail.text = std::move(name);
  tail.mark = out_.size();
  if (is_postfix(op.specifier)) {
    tail.step = Step::Postfix;
    work_.push_back(std::move(tail));
    return push_operand(heap_.arg(term, 0), op.priority, op.specifier == Specifier::Yf);
  }
  const std::size_t id = next_id_++;
  push_clear(id);
  push_operand(heap_.arg(term, 1), op.priority, op.specifier == Specifier::Xfy);
  tail.step = Step::Infix;
  tail.id = id;
  work_.push_back(std::move(tail));
  push_operand(heap_.arg(term, 0), op.priority, op.specifier == Specifier::Yfx);
}

// 6.3.4.2: 演算子のアトムは演算子の被演算子になれない。括弧に入れないと、読み手は (-)/1 の - /1 を
// 前置演算子の項として読もうとして拒む（7.10.5 の往復）。引数とリストの要素は 6.3.3.1 で許されるので付けない。
void Printer::push_operand(Cell arg, int priority, bool allow_equal) {
  const bool wrap = operator_atom(arg);
  if (wrap) push_text(")");
  push_term(arg, priority, allow_equal);
  if (wrap) push_text("(");
}

void Printer::push_prefix(const OpDef& op, Cell term, std::string name) {
  const Cell arg = heap_.arg(term, 0);
  // 空白だけだと次の字が演算子になり、引数のアトムとして読み戻せない。
  // 6.3.4.1: `- 1` も負の数と読まれる。-(1) は数を括弧で包んで前置演算子のまま読み戻す。
  // 字面が数で始まる被演算子も同じ。-(1^2) を -1^2 と書くと (-1)^2 と読まれる（7.10.5 の往復）。
  const bool wrap = operator_atom(arg) ||
                    (op.name == "-" && leftmost_unsigned_number(arg, op.priority, op.specifier == Specifier::Fy));
  const std::size_t id = next_id_++;
  push_clear(id);
  if (wrap) push_text(")");
  push_term(arg, op.priority, op.specifier == Specifier::Fy);
  if (wrap) push_text("(");
  emit(name);
  await(Space::Prefix, id);
}

void Printer::push_list(Cell term) {
  if (options_.ignore_ops) {
    emit(spell_functor(".") + "(");
    const std::size_t id = next_id_++;
    push_text(")");
    push_clear(id);
    push_term(heap_.list_tail(term), 999, true);
    push_comma(id);
    return push_term(heap_.list_head(term), 999, true);
  }
  emit("[");
  Work rest;
  rest.step = Step::ListRest;
  rest.term = term;
  work_.push_back(std::move(rest));
  push_term(heap_.list_head(term), 999, true);
}

void Printer::list_rest(Cell cell) {
  const Cell raw_tail = heap_.list_tail(cell);
  // 名前表の REF は束縛より先に書く。先に辿ると X = [a|X] が [a|_] になる。
  if (raw_tail.tag() == Tag::Ref && var_name(raw_tail) != nullptr) return push_bar(raw_tail);
  const Cell tail = heap_.deref(raw_tail);
  if (is_nil(heap_, tail)) return emit("]");
  if (open_.contains(tail.raw())) return emit("|_]");
  if (tail.tag() != Tag::Lis) return push_bar(tail);
  Work rest;
  rest.step = Step::ListRest;
  rest.term = tail;
  work_.push_back(std::move(rest));
  const std::size_t id = next_id_++;
  push_clear(id);
  push_term(heap_.list_head(tail), 999, true);
  push_comma(id);
}

void Printer::push_bar(Cell rest) {
  const std::size_t id = next_id_++;
  push_text("]");
  push_clear(id);
  push_term(rest, 999, true);
  Work bar;
  bar.step = Step::Bar;
  bar.id = id;
  work_.push_back(std::move(bar));
}

void Printer::push_functor(Cell term, std::string_view name) {
  emit(spell_functor(name) + "(");
  push_text(")");
  const auto arity = heap_.functor_arity(term);
  for (std::uint16_t index = arity; index > 1; --index) {
    const std::size_t id = next_id_++;
    push_clear(id);
    push_term(heap_.arg(term, index - 1U), 999, true);
    push_comma(id);
  }
  push_term(heap_.arg(term, 0), 999, true);
}

}  // namespace

std::string write_term(const Heap& heap, OperatorView operators, Cell term, WriteOptions options,
                       std::span<const VarName> names, WriteReplace replace, void* context) {
  Printer printer(heap, operators, options, names, replace, context);
  if (options.underscore_once) printer.collect(term);
  return printer.print(term);
}

}
