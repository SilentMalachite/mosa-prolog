// 文法規則（-->）を節に、文法規則の本体をゴールに訳す（ADR 0024）。13211-3 の本文は手元に無い（iso-notes「13211-3」）。
//   instantiation_error（頭部、プッシュバック、終端記号のリストの末尾が変数）、type_error(callable, Culprit)、
//   type_error(list, Culprit)
// テスト: tests/pl/dcg.pl、tests/cpp/dcg_test.cpp、tests/stack/deep_builtins.pl
//
// 本体は再帰せず、明示したスタックで訳す。phrase/2,3 は実行時に組み立てた本体を受けるので、制御構造の入れ子の深さで
// C++ のスタックが尽きてはならない（iso-notes 7.1、ADR 0011。Codex のレビューの P1）。

#include "dcg.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace mosa {
namespace {

std::uint32_t atom_id(Cell cell) { return static_cast<std::uint32_t>(cell.raw() >> 3); }

// Enter は本体を 1 つ訳し始める。ほかは制御構造の子を訳し終えた後に、子の結果から制御構造を組み直す。
enum class Step : std::uint8_t { Enter, Conj, Disj, IfThen, Not, Qualified };

struct Work {
  Step step = Step::Enter;
  // Enter では訳す本体。Qualified では M:B の M。
  Cell body{};
  Cell s0{};
  Cell s{};
  // 組み直す段では、訳している途中の祖先の印から外す制御構造。
  Cell node{};
};

class Translator {
 public:
  explicit Translator(Impl& machine) : m_(machine) {}

  std::optional<Cell> translate(Cell body, Cell s0, Cell s);
  DcgOut rule(Cell term);
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
  std::vector<Work> work_;
  std::vector<Cell> done_;
  // 訳している途中の制御構造（今の本体の祖先）。巡回する本体の戻り先を見つける。
  std::unordered_set<std::uint64_t> path_;

  bool enter(const Work& item);
  bool spread(Cell body, const Work& item);
  void leave(const Work& item);
  std::optional<Cell> single(Cell body, Cell s0, Cell s);
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
  // 変数の本体と巡回の戻り先は、実行時に束縛された本体を訳して呼ぶ。call/1 が巡回の戻り先を変数のゴールにして
  // 呼ぶときに訳すのと同じ（compile.cpp）。
  Cell late(Cell body, Cell s0, Cell s) { return make("phrase", std::array<Cell, 3>{body, s0, s}); }
  Cell pop() {
    const Cell top = done_.back();
    done_.pop_back();
    return top;
  }
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

// 子は左から訳すので、誤りは左にある部分で出る。
std::optional<Cell> Translator::translate(Cell body, Cell s0, Cell s) {
  work_.push_back(Work{Step::Enter, body, s0, s, {}});
  while (!work_.empty()) {
    const Work item = work_.back();
    work_.pop_back();
    if (item.step != Step::Enter) {
      leave(item);
    } else if (!enter(item)) {
      return std::nullopt;
    }
  }
  return pop();
}

bool Translator::enter(const Work& item) {
  const Cell body = deref(item.body);
  // 祖先と同じ制御構造に戻ったら巡回である。共有しているだけの部分項は祖先ではないので、ふつうに訳す（そこで
  // 遅らせると、中のカットが phrase に閉じて意味が変わる）。
  if (body.tag() == Tag::Str && path_.contains(body.raw())) {
    done_.push_back(late(body, item.s0, item.s));
    return true;
  }
  if (spread(body, item)) return true;
  const auto leaf = single(body, item.s0, item.s);
  if (!leaf) return false;
  done_.push_back(*leaf);
  return true;
}

// 制御構造なら、組み直す段と子を積んで true。子は後に積んだものから訳すので、左の子を最後に積む。
bool Translator::spread(Cell body, const Work& item) {
  if (body.tag() != Tag::Str) return false;
  const auto name = m_.atoms.name(m_.heap.functor_atom(body));
  const auto arity = m_.heap.functor_arity(body);
  const Cell left = arg(body, 0);
  if (arity == 2 && (name == "," || name == "->")) {
    const Cell mid = m_.heap.fresh_ref();
    work_.push_back(Work{name == "," ? Step::Conj : Step::IfThen, {}, item.s0, item.s, body});
    work_.push_back(Work{Step::Enter, arg(body, 1), mid, item.s, {}});
    work_.push_back(Work{Step::Enter, left, item.s0, mid, {}});
  } else if (arity == 2 && (name == ";" || name == "|")) {
    work_.push_back(Work{Step::Disj, {}, item.s0, item.s, body});
    work_.push_back(Work{Step::Enter, arg(body, 1), item.s0, item.s, {}});
    work_.push_back(Work{Step::Enter, left, item.s0, item.s, {}});
  } else if (arity == 1 && name == "\\+") {
    work_.push_back(Work{Step::Not, {}, item.s0, item.s, body});
    work_.push_back(Work{Step::Enter, left, item.s0, m_.heap.fresh_ref(), {}});
  } else if (arity == 2 && name == ":") {
    work_.push_back(Work{Step::Qualified, left, item.s0, item.s, body});
    work_.push_back(Work{Step::Enter, arg(body, 1), item.s0, item.s, {}});
  } else {
    return false;
  }
  path_.insert(body.raw());
  return true;
}

void Translator::leave(const Work& item) {
  path_.erase(item.node.raw());
  if (item.step == Step::Not) {
    done_.push_back(then_same(make("\\+", std::array<Cell, 1>{pop()}), item.s0, item.s));
    return;
  }
  if (item.step == Step::Qualified) {
    done_.push_back(pair(":", item.body, pop()));
    return;
  }
  const Cell right = pop();
  const Cell left = pop();
  const std::string_view name = item.step == Step::Conj ? "," : item.step == Step::Disj ? ";" : "->";
  done_.push_back(pair(name, left, right));
}

// 制御構造でない本体。
std::optional<Cell> Translator::single(Cell body, Cell s0, Cell s) {
  if (m_.heap.unbound(body)) return late(body, s0, s);
  if (body.tag() == Tag::Lis || is_nil(body)) return terminals(body, s0, s);
  if (body.tag() == Tag::Atom) {
    if (m_.atoms.name(atom_id(body)) == "!") return then_same(atom("!"), s0, s);
    return nonterminal(body, s0, s);
  }
  if (body.tag() != Tag::Str) return fail(formal_type(m_, "callable", body), true);
  if (is_functor(body, "{}", 1)) return then_same(make("call", std::array<Cell, 1>{arg(body, 0)}), s0, s);
  // 非終端記号と call/N は、引数の後ろに S0 と S を足す。
  return nonterminal(body, s0, s);
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
    for (std::uint16_t index = 0; index < m_.heap.functor_arity(callable); ++index) {
      args.push_back(arg(callable, index));
    }
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
