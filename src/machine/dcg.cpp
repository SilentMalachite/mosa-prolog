// 文法規則（-->）を節に、文法規則の本体をゴールに訳す（ADR 0024）。13211-3 の本文は手元に無い（iso-notes「13211-3」）。
//   instantiation_error（頭部、プッシュバック、終端記号のリストの末尾が変数）、type_error(callable, Culprit)、
//   type_error(list, Culprit)
// テスト: tests/pl/dcg.pl、tests/cpp/dcg_test.cpp

#include "dcg.hpp"

#include <array>
#include <string_view>
#include <vector>

namespace mosa {
namespace {

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

class Translator {
 public:
  explicit Translator(Impl& machine) : m_(machine) {}

  std::optional<Cell> translate(Cell body, Cell s0, Cell s);
  DcgOut rule(Cell rule);
  DcgOut result(std::optional<Cell> term) const {
    DcgOut out;
    if (term) out.term = *term;
    out.formal = formal_;
    out.not_callable = not_callable_;
    return out;
  }

 private:
  Impl& m_;
  std::optional<Cell> formal_;
  bool not_callable_ = false;

  std::optional<Cell> single(Cell body, Cell s0, Cell s);
  std::optional<Cell> control(Cell body, std::string_view name, std::uint16_t arity, Cell s0, Cell s);
  std::optional<Cell> terminals(Cell list, Cell s0, Cell s);
  Cell nonterminal(Cell callable, Cell s0, Cell s);

  Cell deref(Cell cell) const { return m_.heap.deref(cell); }
  Cell arg(Cell cell, std::size_t index) const { return m_.heap.arg(cell, index); }
  Cell atom(std::string_view name) { return m_.heap.atom(m_.atoms.intern(name)); }
  Cell make(std::string_view name, std::span<const Cell> args) {
    return m_.heap.structure(m_.atoms.intern(name), args);
  }
  Cell pair(std::string_view name, Cell left, Cell right) { return make(name, std::array<Cell, 2>{left, right}); }
  Cell unify(Cell left, Cell right) { return pair("=", left, right); }
  // ゴールの後で S0 = S を足す。{}、!、\+ は何も読まない。
  Cell then_same(Cell goal, Cell s0, Cell s) { return pair(",", goal, unify(s0, s)); }
  bool is_functor(Cell cell, std::string_view name, std::uint16_t arity) const {
    return cell.tag() == Tag::Str && m_.heap.functor_arity(cell) == arity &&
           m_.atoms.name(m_.heap.functor_atom(cell)) == name;
  }
  bool is_nil(Cell cell) const { return cell.tag() == Tag::Atom && m_.atoms.name(atom_id(cell)) == "[]"; }
  std::nullopt_t fail(Cell formal, bool not_callable = false) {
    formal_ = formal;
    not_callable_ = not_callable;
    return std::nullopt;
  }
  // Items の後ろに Tail を付けたリスト。
  Cell appended(const std::vector<Cell>& items, Cell tail) {
    for (auto item = items.rbegin(); item != items.rend(); ++item) tail = m_.heap.list(*item, tail);
    return tail;
  }
  // プッシュバックと終端記号のリストは真のリストでなければならない。末尾が変数なら何を読むか決まらない。
  std::optional<WalkedList> proper(Cell list) {
    WalkedList walked = walk_list(m_, list);
    if (walked.shape == ListShape::Variable || walked.shape == ListShape::Partial) {
      return fail(formal_instantiation(m_));
    }
    if (walked.shape == ListShape::Bad) return fail(formal_type(m_, "list", deref(list)));
    return walked;
  }
};

// 連言は右へ伸びる鎖をループで辿る。実行時に組み立てた長い本体の phrase/2 で C++ のスタックが尽きないように
// （iso-notes 7.1）。左へ入れ子の連言と、ほかの制御構造は入れ子の深さだけ再帰する。
std::optional<Cell> Translator::translate(Cell body, Cell s0, Cell s) {
  std::vector<Cell> parts;
  Cell from = s0;
  body = deref(body);
  while (is_functor(body, ",", 2)) {
    const Cell mid = m_.heap.fresh_ref();
    const auto left = single(arg(body, 0), from, mid);
    if (!left) return std::nullopt;
    parts.push_back(*left);
    from = mid;
    body = deref(arg(body, 1));
  }
  const auto last = single(body, from, s);
  if (!last) return std::nullopt;
  Cell built = *last;
  for (auto part = parts.rbegin(); part != parts.rend(); ++part) built = pair(",", *part, built);
  return built;
}

std::optional<Cell> Translator::single(Cell body, Cell s0, Cell s) {
  body = deref(body);
  // 変数の本体は、実行時に束縛された本体を訳して呼ぶ。
  if (m_.heap.unbound(body)) return make("phrase", std::array<Cell, 3>{body, s0, s});
  if (body.tag() == Tag::Lis || is_nil(body)) return terminals(body, s0, s);
  if (body.tag() == Tag::Atom) {
    if (m_.atoms.name(atom_id(body)) == "!") return then_same(atom("!"), s0, s);
    return nonterminal(body, s0, s);
  }
  if (body.tag() != Tag::Str) return fail(formal_type(m_, "callable", body), true);
  const auto name = m_.atoms.name(m_.heap.functor_atom(body));
  const auto arity = m_.heap.functor_arity(body);
  if (const auto done = control(body, name, arity, s0, s)) return done;
  if (formal_) return std::nullopt;
  // 非終端記号と call/N は、引数の後ろに S0 と S を足す。
  return nonterminal(body, s0, s);
}

// 制御構造の本体。制御構造でなければ、誤りを立てずに nullopt を返す。
std::optional<Cell> Translator::control(Cell body, std::string_view name, std::uint16_t arity, Cell s0, Cell s) {
  if (arity == 2 && name == ",") return translate(body, s0, s);
  if (arity == 2 && (name == ";" || name == "|")) {
    const auto left = translate(arg(body, 0), s0, s);
    if (!left) return std::nullopt;
    const auto right = translate(arg(body, 1), s0, s);
    if (!right) return std::nullopt;
    return pair(";", *left, *right);
  }
  if (arity == 2 && name == "->") {
    const Cell mid = m_.heap.fresh_ref();
    const auto condition = translate(arg(body, 0), s0, mid);
    if (!condition) return std::nullopt;
    const auto then = translate(arg(body, 1), mid, s);
    if (!then) return std::nullopt;
    return pair("->", *condition, *then);
  }
  if (arity == 1 && name == "\\+") {
    const auto inner = translate(arg(body, 0), s0, m_.heap.fresh_ref());
    if (!inner) return std::nullopt;
    return then_same(make("\\+", std::array<Cell, 1>{*inner}), s0, s);
  }
  if (arity == 1 && name == "{}") return then_same(make("call", std::array<Cell, 1>{arg(body, 0)}), s0, s);
  if (arity == 2 && name == ":") {
    const auto inner = translate(arg(body, 1), s0, s);
    if (!inner) return std::nullopt;
    return pair(":", arg(body, 0), *inner);
  }
  return std::nullopt;
}

// 終端記号は頭部へ畳み込まず、本体の単一化にする。前にあるカットや {} が単一化より先に走る（ADR 0024）。
std::optional<Cell> Translator::terminals(Cell list, Cell s0, Cell s) {
  const auto walked = proper(list);
  if (!walked) return std::nullopt;
  return unify(s0, appended(walked->items, s));
}

Cell Translator::nonterminal(Cell callable, Cell s0, Cell s) {
  std::vector<Cell> args;
  std::uint32_t name = 0;
  if (callable.tag() == Tag::Atom) {
    name = atom_id(callable);
  } else if (callable.tag() == Tag::Lis) {
    name = m_.atoms.intern(".");
    args.push_back(m_.heap.list_head(callable));
    args.push_back(m_.heap.list_tail(callable));
  } else {
    name = m_.heap.functor_atom(callable);
    for (std::uint16_t index = 0; index < m_.heap.functor_arity(callable); ++index) args.push_back(arg(callable, index));
  }
  args.push_back(s0);
  args.push_back(s);
  return m_.heap.structure(name, args);
}

// 頭部、プッシュバック、本体の順に調べる。H, PB --> B は H(S0, S) :- B(S0, S1), S = PB ++ S1。
DcgOut Translator::rule(Cell term) {
  Cell head = deref(arg(term, 0));
  std::optional<Cell> pushback;
  if (is_functor(head, ",", 2)) {
    pushback = arg(head, 1);
    head = deref(arg(head, 0));
  }
  if (m_.heap.unbound(head)) return result(fail(formal_instantiation(m_)));
  if (head.tag() != Tag::Atom && head.tag() != Tag::Str && head.tag() != Tag::Lis) {
    return result(fail(formal_type(m_, "callable", head), true));
  }
  std::optional<WalkedList> pushed;
  if (pushback) {
    pushed = proper(*pushback);
    if (!pushed) return result(std::nullopt);
  }
  const Cell s0 = m_.heap.fresh_ref();
  const Cell s = m_.heap.fresh_ref();
  const Cell end = pushed ? m_.heap.fresh_ref() : s;
  auto goal = translate(arg(term, 1), s0, end);
  if (!goal) return result(std::nullopt);
  if (pushed) goal = pair(",", *goal, unify(s, appended(pushed->items, end)));
  return result(pair(":-", nonterminal(head, s0, s), *goal));
}

}  // namespace

DcgOut dcg_rule(Impl& machine, Cell rule) { return Translator{machine}.rule(rule); }

DcgOut dcg_body(Impl& machine, Cell body, Cell s0, Cell s) {
  Translator translator{machine};
  const auto goal = translator.translate(body, s0, s);
  return translator.result(goal);
}

}  // namespace mosa
