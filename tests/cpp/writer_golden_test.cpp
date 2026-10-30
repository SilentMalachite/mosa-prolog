// writer の出力を固定する（ADR 0011 は出力を 1 バイトも変えないと決めた）。決まった種で項を作り、
// 選択肢（quoted、ignore_ops、numbervars、variable_names）の組ごとに書いて、golden のファイルと比べる。
// 使い方: writer_golden_test <golden のパス> [--write-golden]。--write-golden はファイルを書き直す。
// 被演算子や引数になる演算子アトム（`- /1` の不具合、7.10.5）は、直すまで集まりから外す（計画の Task 8）。
// 種の乱数は std::mt19937 の生の出力だけを使う。分布の型は処理系ごとに結果が違うため。

#include "harness.hpp"
#include "mosa/reader.hpp"
#include "mosa/writer.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

// 演算子でないアトムだけを置く。引用の要否、図形、空、日本語、制御文字を混ぜる。
constexpr std::array<std::string_view, 16> kAtoms{
    "a", "foo_Bar", "A", "_x", "[]", "{}", "hello world", "東京",
    "it's", "\n", "", "!", "&&", "/*x", ".", "x\\y"};

constexpr std::array<std::string_view, 6> kFunctors{"f", "g", "Foo", "東", "{}", "."};

// 書き方を手で選びたい項。リストや括弧の境目を狙う。
constexpr std::array<std::string_view, 24> kCrafted{
    "-(1).", "-(1.0).", "-(-(1)).", "1-(-1).", "- (-1).", "-(a).", "- - a.", "\\+ (a,b).",
    "f((a,b)).", "f((a:-b)).", "[a|b].", "{a,b}.", "(a:-b,c;d->e).", "2- (-2)^2.", "- (2^2).",
    "(-2)^2.", "-(2)^2.", "1 rem 2.", "a=..b.", "\\+ \\+ a.", "1*(2+3)*4.", "[(a:-b)].",
    "[a,b|c].", "'$VAR'(1)+'$VAR'(27)."};

class Maker {
 public:
  Maker(mosa::Heap& heap, const mosa::OperatorTable& ops) : heap_(heap), ops_(ops.entries()) {
    for (int index = 0; index < 3; ++index) vars.push_back(heap_.fresh_ref());
  }

  std::vector<mosa::Cell> vars;

  [[nodiscard]] const std::vector<mosa::OpDef>& ops() const { return ops_; }

  mosa::Cell rooted(const mosa::OpDef& op, int depth) {
    const bool binary = op.specifier == mosa::Specifier::Xfx || op.specifier == mosa::Specifier::Xfy ||
                        op.specifier == mosa::Specifier::Yfx;
    std::vector<mosa::Cell> args;
    args.push_back(term(depth - 1));
    if (binary) args.push_back(term(depth - 1));
    return heap_.structure(heap_.atoms().intern(op.name), args);
  }

  mosa::Cell term(int depth) {
    if (depth <= 0) return leaf();
    switch (pick(7)) {
      case 0:
      case 1: return rooted(ops_[pick(ops_.size())], depth);
      case 2: return functor(depth);
      case 3: return list(depth);
      case 4: return heap_.structure(heap_.atoms().intern("{}"), std::array<mosa::Cell, 1>{term(depth - 1)});
      case 5: return numbervar();
      default: return leaf();
    }
  }

 private:
  std::size_t pick(std::size_t count) { return static_cast<std::size_t>(rng_()) % count; }

  mosa::Cell leaf() {
    switch (pick(4)) {
      case 0: return heap_.atom(heap_.atoms().intern(kAtoms[pick(kAtoms.size())]));
      case 1: {
        constexpr std::array<std::int64_t, 5> ints{0, 7, -3, -1, 4611686018427387904};
        return heap_.integer(ints[pick(ints.size())]);
      }
      case 2: {
        constexpr std::array<double, 5> floats{1.5, -2.25, 1.0e20, -0.0, 0.1};
        return heap_.floating(floats[pick(floats.size())]);
      }
      default: return vars[pick(vars.size())];
    }
  }

  mosa::Cell functor(int depth) {
    const std::size_t arity = 1 + pick(2);
    std::vector<mosa::Cell> args;
    for (std::size_t index = 0; index < arity; ++index) args.push_back(term(depth - 1));
    return heap_.structure(heap_.atoms().intern(kFunctors[pick(kFunctors.size())]), args);
  }

  mosa::Cell list(int depth) {
    const std::size_t length = pick(4);
    std::array<mosa::Cell, 3> tails{heap_.atom(heap_.atoms().intern("[]")), vars[0],
                                    heap_.atom(heap_.atoms().intern("a"))};
    mosa::Cell out = tails[pick(tails.size())];
    for (std::size_t index = 0; index < length; ++index) out = heap_.list(term(depth - 1), out);
    return out;
  }

  mosa::Cell numbervar() {
    std::array<mosa::Cell, 6> args{heap_.integer(0),  heap_.integer(25), heap_.integer(27),
                                   heap_.integer(-1), heap_.atom(heap_.atoms().intern("x")),
                                   heap_.atom(heap_.atoms().intern("Foo"))};
    return heap_.structure(heap_.atoms().intern("$VAR"), std::array<mosa::Cell, 1>{args[pick(args.size())]});
  }

  mosa::Heap& heap_;
  std::vector<mosa::OpDef> ops_;
  std::mt19937 rng_{20261030U};
};

// ISO の核に後置と語の前置を足す。後置の括弧と空白も固定するため。
mosa::OperatorTable golden_ops() {
  auto ops = mosa::OperatorTable::iso_core();
  (void)ops.define(100, mosa::Specifier::Xf, "$post");
  (void)ops.define(150, mosa::Specifier::Yf, "##");
  (void)ops.define(1150, mosa::Specifier::Fx, "dyn");
  return ops;
}

std::vector<mosa::Cell> collect_terms(Maker& maker, mosa::Heap& heap, const mosa::OperatorTable& ops) {
  std::vector<mosa::Cell> terms;
  for (const std::string_view text : kCrafted) {
    mosa::Reader reader(text, heap, ops);
    const auto outcome = reader.next();
    mosa_test::check(outcome.term.has_value(), text);
    if (outcome.term) terms.push_back(outcome.term->term);
  }
  for (const mosa::OpDef& op : maker.ops()) {
    for (int count = 0; count < 3; ++count) terms.push_back(maker.rooted(op, 3));
  }
  for (int count = 0; count < 120; ++count) terms.push_back(maker.term(3));
  return terms;
}

std::string escaped(std::string_view text) {
  std::string out;
  for (const char ch : text) {
    if (ch == '\\') out += "\\\\";
    else if (ch == '\n') out += "\\n";
    else out += ch;
  }
  return out;
}

std::vector<std::string> render_all() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  const auto ops = golden_ops();
  Maker maker(heap, ops);
  const std::vector<mosa::Cell> terms = collect_terms(maker, heap, ops);
  const std::vector<mosa::VarName> names{{"X", maker.vars[0]}, {"_Y", maker.vars[1]}};
  std::vector<std::string> lines;
  for (std::size_t index = 0; index < terms.size(); ++index) {
    for (unsigned combo = 0; combo < 16; ++combo) {
      mosa::WriteOptions options;
      options.quoted = (combo & 1U) != 0;
      options.ignore_ops = (combo & 2U) != 0;
      options.numbervars = (combo & 4U) != 0;
      const bool named = (combo & 8U) != 0;
      const std::string text = mosa::write_term(heap, ops, terms[index], options,
                                                named ? std::span<const mosa::VarName>(names)
                                                      : std::span<const mosa::VarName>());
      lines.push_back(std::to_string(index) + ":" + std::to_string(combo) + ":" + escaped(text));
    }
  }
  return lines;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: writer_golden_test <golden> [--write-golden]\n";
    return 2;
  }
  const std::vector<std::string> lines = render_all();
  if (argc > 2 && std::string_view(argv[2]) == "--write-golden") {
    std::ofstream out(argv[1], std::ios::binary);
    for (const std::string& line : lines) out << line << '\n';
    return out ? mosa_test::failures() : 1;
  }
  std::ifstream in(argv[1], std::ios::binary);
  mosa_test::check(static_cast<bool>(in), "golden file opens");
  std::string line;
  std::size_t index = 0;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (index < lines.size() && lines[index] != line) {
      mosa_test::check(false, "golden line " + std::to_string(index + 1) + ": want [" + line + "] got [" +
                                  lines[index] + "]");
    }
    ++index;
  }
  mosa_test::check(index == lines.size(), "golden line count");
  return mosa_test::failures();
}
