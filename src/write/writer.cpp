#include "mosa/writer.hpp"

#include "mosa/bigint.hpp"
#include "mosa/char_class.hpp"
#include "printer.hpp"
#include "spelling.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mosa::printing {
namespace {

using spelling::bare_atom;
using spelling::continues_name;
using spelling::final_cp;
using spelling::first_cp;
using spelling::format_float;
using spelling::quote_atom;

bool is_infix(Specifier specifier) {
  return specifier == Specifier::Xfx || specifier == Specifier::Xfy || specifier == Specifier::Yfx;
}

bool is_prefix(Specifier specifier) { return specifier == Specifier::Fx || specifier == Specifier::Fy; }

bool is_postfix(Specifier specifier) { return specifier == Specifier::Xf || specifier == Specifier::Yf; }

std::string made_name(int number) { return "_G" + std::to_string(number); }

}  // namespace

void Printer::visit(WriteRoots& roots) {
  // map の鍵はその場で書き換えられない。初めの訪問で写しを作り、同じ replace の間の GC はその写しを訪ねる。
  if (!held_) {
    held_counts_.assign(counts_.begin(), counts_.end());
    held_assigned_.assign(assigned_.begin(), assigned_.end());
    held_ = true;
  }
  roots.cell(hooked_);
  for (Work& item : work_) {
    roots.cell(item.term);
    if (item.step != Step::ListRest) continue;
    roots.cell(item.back);
    roots.cell(item.anchor);
  }
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
  index_names();
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
  // write_terms は同じ Printer で続けて書く。作った名前の表（assigned_、next_number_）は次の項へ持ち越し、
  // 空白の待ちと直前の 0 の位置は前の項の字面のものなので捨てる。
  out_.clear();
  pending_ = Pending{};
  zero_end_ = std::string::npos;
  // 被演算子として書くときは、演算子の中の被演算子と同じ道を通り、演算子の原子も括弧に入れる（レビュー M27）。
  // 全体は深さ 1。push_term は depth_ より 1 深く積むので、最初だけ 0 から始める。
  depth_ = 0;
  if (options_.operand_priority) push_operand(term, *options_.operand_priority, true);
  else push_term(term, 1200, true);
  while (!work_.empty() && !aborted_) {
    Work item = std::move(work_.back());
    work_.pop_back();
    run(item);
  }
  return std::move(out_);
}

void Printer::run(Work& item) {
  depth_ = item.depth;
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
    case Step::ListRest: list_rest(item); return;
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
    if (call_form || glue(pending_.left, first_cp(text)) || zero_quote(text)) out_ += ' ';
  }
  out_ += text;
}

void Printer::await(Space kind, std::size_t id) {
  pending_ = Pending{true, kind, final_cp(out_), id};
}

bool Printer::left_glue(std::size_t mark, std::string_view next) const {
  // 左の被演算子が何も書かなかったときは、その前の字句と比べない。
  return out_.size() > mark && (glue(final_cp(out_), first_cp(next)) || zero_quote(next));
}

// 書いたばかりの整数 0 の直後の ' は 0'c の文字コードに読まれる（M8 の Codex レビュー）。字面の 0 は名前や
// 演算子の一部でもありうるので、字句として書いた場所で覚える。10' や 1.0' は読み手が分けるので詰めたまま。
bool Printer::zero_quote(std::string_view next) const {
  return !next.empty() && next.front() == '\'' && zero_end_ == out_.size();
}

// below は今書いている仕事の深さからの差。引数と演算子の被演算子は 1 つ深く、括弧を足し直す再入は同じ深さ。
void Printer::push_term(Cell term, int parent, bool allow_equal, bool allow_hook, int below) {
  Work item;
  item.depth = depth_ + below;
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

void Printer::index_names() {
  name_index_.clear();
  for (std::size_t index = 0; index < names_.size(); ++index) name_index_.emplace(names_[index].cell.raw(), index);
}

const VarName* Printer::var_name(Cell term) const {
  // 表を作るのは構築時と settle（GC でセルが動いた後）だけなので、引けなければ名前は無い。
  // 前の線形の保険は、名前の無い変数のたびに表を全走査して答えの書き出しを 2 乗にしていた
  // （Claude のレビューの M3）。
  const auto found = name_index_.find(term.raw());
  if (found == name_index_.end()) return nullptr;
  const VarName& var = names_[found->second];
  return var.cell.raw() == term.raw() ? &var : nullptr;
}

std::string Printer::var_text(Cell term) {
  if (const VarName* var = var_name(term)) return var->name;
  if (options_.underscore_once && counts_[term.raw()] <= 1) return "_";
  if (const auto found = assigned_.find(term.raw()); found != assigned_.end()) return made_name(found->second);
  // 7.10.4 の variable_names は名前を付けるだけで、ほかの変数の字面を選ばない。付けた名前と同じ番号を作ると、
  // 読み戻したとき 2 つの変数が 1 つになる（レビュー M9）。項に出ない名前も飛ばし、同じ表で分けて書く字面とも重ねない。
  while (given_.contains(made_name(next_number_))) ++next_number_;
  assigned_.emplace(term.raw(), next_number_);
  return made_name(next_number_++);
}

bool Printer::glue(std::optional<char32_t> end, std::optional<char32_t> start) {
  if (!end || !start) return false;
  // 引用の中の '' は引用符 1 字なので、続く 2 つの引用原子は 1 つに読まれる（L16）。' で始まり終わる字句は引用原子だけ。
  if (*end == U'\'' && *start == U'\'') return true;
  if (classify(*end) == CharClass::Graphic && classify(*start) == CharClass::Graphic) return true;
  return continues_name(*end) && continues_name(*start);
}

// max_depth(N) で、深さ depth の部分項を ... と書くか（render と同じ判定）。
bool Printer::cut_at(int depth) const { return options_.max_depth > 0 && depth > options_.max_depth; }

bool Printer::needs_paren(int priority, int parent, bool allow_equal) {
  if (allow_equal) return priority > parent;
  return priority >= parent;
}

std::string Printer::spell(std::string_view name) const {
  // 深さで切った字面 ... と原子 '...' を分ける。max_depth は ISO に無いので、制限が無ければ writeq のまま。
  if (options_.quoted && options_.max_depth > 0 && name == "...") return quote_atom(name);
  if (!options_.quoted || bare_atom(name, options_.fullwidth)) return std::string{name};
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

// 字面が - で始まらない数。符号の立った NaN も 1.5NaN と書くので符号なしに数える。数えないと -(NaN) を
// -1.5NaN と書き、負の数として読み戻される（レビュー L22）。
bool Printer::unsigned_number(Cell term) const {
  term = heap_.deref(term);
  if (term.tag() == Tag::Int || term.tag() == Tag::IntBox) return heap_.integer_value(term) >= 0;
  // Big の字面も負なら - で始まるので、- 1 と -1 の空白と括弧の判断は int64 と同じになる。
  if (term.tag() == Tag::Big) return !heap_.big_negative(term);
  if (term.tag() != Tag::Float) return false;
  const double value = heap_.float_value(term);
  return std::isnan(value) || !std::signbit(value);
}

bool Printer::operator_atom(Cell term) const {
  // 名前のある変数は名前で書くので、束縛された原子を見ると (Y) と余計な括弧が付く（M27 の敵対的レビュー A3）。
  if (term.tag() == Tag::Ref && var_name(term) != nullptr) return false;
  term = heap_.deref(term);
  if (term.tag() != Tag::Atom) return false;
  return !operators_.lookup(heap_.atoms().name(atom_of(term))).empty();
}

namespace {

// 番号が int64 の外でも、int64 と同じく字母は N mod 26、続く番号は N / 26 にする（N は 2^63 以上なので番号は 0 でない）。
std::optional<std::string> big_numbervar_text(const Heap& heap, Cell arg) {
  if (heap.big_negative(arg)) return std::nullopt;
  const BigQuotient parts = divide_truncate(heap.big_value(arg), BigInt(26));
  std::string name(1, static_cast<char>('A' + parts.remainder.to_int64().value_or(0)));
  name += to_text(parts.quotient);
  return name;
}

std::optional<std::string> numbervar_text(const Heap& heap, Cell term) {
  if (term.tag() != Tag::Str || heap.functor_arity(term) != 1) return std::nullopt;
  if (heap.atoms().name(heap.functor_atom(term)) != "$VAR") return std::nullopt;
  const Cell arg = heap.deref(heap.arg(term, 0));
  if (arg.tag() == Tag::Big) return big_numbervar_text(heap, arg);
  if (arg.tag() != Tag::Int && arg.tag() != Tag::IntBox) return std::nullopt;
  const auto value = heap.integer_value(arg);
  if (value < 0) return std::nullopt;
  const auto index = static_cast<std::uint64_t>(value);
  std::string name(1, static_cast<char>('A' + static_cast<int>(index % 26U)));
  const auto suffix = index / 26U;
  if (suffix != 0) name += std::to_string(suffix);
  return name;
}

}  // namespace

// 書く順をなぞって左端の被演算子を降り、字面の先頭が符号なしの数になるかを見る。括弧、前置演算子、
// 関数表記、リスト、{}、変数、'$VAR' は数以外の字で始まるので止める。降りの中で同じ項に戻るのは循環で、
// writer はそこに _ を書く。portray が置き換える字面は見ない（7.10.5 で portray の出力は利用者が決める）。
bool Printer::leftmost_unsigned_number(Cell term, int priority, bool allow_equal) const {
  std::unordered_set<std::uint64_t> seen;
  // 被演算子は depth_ より 1 深い。深さで切る所から先は ... と書くので、数では始まらない。
  for (int depth = depth_ + 1;; ++depth) {
    if (cut_at(depth)) return false;
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
  // 上限を越えた部分項は中を見ない。portray も呼ばない。0 は制限なし。
  if (options_.max_depth > 0 && item.depth > options_.max_depth) return emit("...");
  if (item.allow_hook && replace_ != nullptr) {
    std::string swapped;
    // replace は GC を走らせうる。訪問が hooked_ を新しい番地に直すので、戻った後はそちらを読む。
    hooked_ = term;
    const WriteCut cut = replace_(context_, term, swapped, *this);
    term = hooked_;
    hooked_ = Cell{};
    settle();
    if (cut == WriteCut::Stop || cut == WriteCut::Last) {
      // Last の字面も Swap と同じ emit を通し、前の字句との間の空白を同じに決める。
      if (cut == WriteCut::Last) emit(swapped);
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
  if (term.tag() == Tag::Int || term.tag() == Tag::IntBox) {
    emit(std::to_string(heap_.integer_value(term)));
    if (heap_.integer_value(term) == 0) zero_end_ = out_.size();
    return;
  }
  // Big は 0 にならないので、0' と読み違える字面の印（zero_end_）は要らない。
  if (term.tag() == Tag::Big) return emit(to_text(heap_.big_value(term)));
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
    push_term(term, 1200, true, false, 0);
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
  // 深さで切る被演算子は ... で、演算子の原子ではない。括弧を付けると、隠した中身が演算子の原子だと分かってしまう。
  const bool wrap = !cut_at(depth_ + 1) && operator_atom(arg);
  if (wrap) push_text(")");
  push_term(arg, priority, allow_equal);
  if (wrap) push_text("(");
}

void Printer::push_prefix(const OpDef& op, Cell term, std::string name) {
  const Cell arg = heap_.arg(term, 0);
  // 空白だけだと次の字が演算子になり、引数のアトムとして読み戻せない。
  // 6.3.4.1: `- 1` も負の数と読まれる。-(1) は数を括弧で包んで前置演算子のまま読み戻す。
  // 字面が数で始まる被演算子も同じ。-(1^2) を -1^2 と書くと (-1)^2 と読まれる（7.10.5 の往復）。
  const bool wrap = (!cut_at(depth_ + 1) && operator_atom(arg)) ||
                    (op.name == "-" && leftmost_unsigned_number(arg, op.priority, op.specifier == Specifier::Fy));
  const std::size_t id = next_id_++;
  push_clear(id);
  if (wrap) push_text(")");
  push_term(arg, op.priority, op.specifier == Specifier::Fy);
  if (wrap) push_text("(");
  emit(name);
  await(Space::Prefix, id);
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

}  // namespace mosa::printing

namespace mosa {

std::string write_term(const Heap& heap, OperatorView operators, Cell term, WriteOptions options,
                       std::span<const VarName> names, WriteReplace replace, void* context) {
  printing::Printer printer(heap, operators, options, names, replace, context);
  if (options.underscore_once) printer.collect(term);
  return printer.print(term);
}

std::vector<std::string> write_terms(const Heap& heap, OperatorView operators, std::span<const Cell> terms,
                                     WriteOptions options, std::span<const VarName> names) {
  printing::Printer printer(heap, operators, options, names, nullptr, nullptr);
  if (options.underscore_once) {
    for (const Cell term : terms) printer.collect(term);
  }
  std::vector<std::string> texts;
  texts.reserve(terms.size());
  for (const Cell term : terms) texts.push_back(printer.print(term));
  return texts;
}

// Mosa が書いた字面は char_conversion なしで読み直す（query_written）ので、区切りは空白 1 つでよい。
std::string full_stop_after(std::string_view text) {
  const std::optional<char32_t> last = spelling::final_cp(text);
  return last && classify(*last) == CharClass::Graphic ? " ." : ".";
}

}
