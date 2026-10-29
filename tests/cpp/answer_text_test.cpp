// 計画 Task 11。差分テストの答えの字面（src/machine/answer_text.cpp）。共有と別名が見えること、環が _ で止まること、
// 葉の引用が writeq と同じこと、深い項（f を 200000 段）を既定のスタックで書けることを確かめる。

#include "answer_text.hpp"
#include "harness.hpp"
#include "mosa/reader.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace {

using mosa_test::check;

struct Held {
  mosa::AtomTable atoms;
  mosa::Heap heap;
  mosa::OperatorTable ops;
  mosa::ReadTerm term{};
  bool ok = false;

  explicit Held(std::string_view source) : heap(atoms), ops(mosa::OperatorTable::iso_core()) {
    mosa::Reader reader(source, heap, ops);
    auto outcome = reader.next();
    if (!outcome.term) return;
    term = std::move(*outcome.term);
    ok = true;
  }
  [[nodiscard]] std::string text(mosa::Cell cell) const { return mosa::answer_text(heap, ops, cell); }
};

void sharing_and_leaves() {
  const Held shared("'$a'(f(X, Y, X), Y, _).");
  check(shared.ok && shared.text(shared.term.term) == "'$a'(f(_G1,_G2,_G1),_G2,_G3)", "sharing by first occurrence");
  const Held leaves("'$a'('A', -1, -(1), {x}, a+b, [], [a|T], \"ab\", 1.5).");
  check(leaves.ok, "leaves read");
  if (!leaves.ok) return;
  const std::string text = leaves.text(leaves.term.term);
  check(text.starts_with("'$a'('A',-1,-(1),{}(x),+(a,b),[],[a|_G1],"), "leaves quoted like writeq: " + text);
  check(text.ends_with(",1.5)"), "float leaf: " + text);
}

// X = [a,b|X] と、途中の節へ戻る環 T = [b|T] の [a|T]。writer と同じく戻った所を _ にする。
void cycles() {
  Held whole("X = [a, b | X].");
  check(whole.ok, "cyclic list read");
  if (!whole.ok) return;
  const mosa::Cell var = whole.heap.deref(whole.heap.arg(whole.term.term, 0));
  whole.heap.bind(var, whole.heap.arg(whole.term.term, 1));
  check(whole.text(var) == "[a,b|_]", "whole list cycle: " + whole.text(var));
  Held inner("t(T, [a | T], [b | T]).");
  check(inner.ok, "inner cycle read");
  if (!inner.ok) return;
  const mosa::Cell tail = inner.heap.deref(inner.heap.arg(inner.term.term, 0));
  inner.heap.bind(tail, inner.heap.arg(inner.term.term, 2));
  check(inner.text(inner.heap.arg(inner.term.term, 1)) == "[a,b|_]", "inner list cycle: " +
                                                                           inner.text(inner.heap.arg(inner.term.term, 1)));
  Held nested("s(S, f(S)).");
  check(nested.ok, "cyclic structure read");
  if (!nested.ok) return;
  const mosa::Cell s = nested.heap.deref(nested.heap.arg(nested.term.term, 0));
  nested.heap.bind(s, nested.heap.arg(nested.term.term, 1));
  check(nested.text(s) == "f(_)", "structure cycle: " + nested.text(s));
}

// 深さ 200000 の f(f(...a...))。write_term の再帰なら 8 MB のスタックで落ちる。
void deep() {
  mosa::AtomTable atoms;
  mosa::Heap heap(atoms);
  const mosa::OperatorTable ops = mosa::OperatorTable::iso_core();
  const std::uint32_t f = atoms.intern("f");
  constexpr std::size_t depth = 200000;
  mosa::Cell term = heap.atom(atoms.intern("a"));
  for (std::size_t level = 0; level < depth; ++level) {
    const std::array<mosa::Cell, 1> arg{term};
    term = heap.structure(f, arg);
  }
  const std::string text = mosa::answer_text(heap, ops, term);
  // f( が depth 回、a、) が depth 回。
  check(text.size() == 3 * depth + 1 && text.starts_with("f(f(f(") && text.find('a') == 2 * depth &&
            text.ends_with(")))"),
        "deep term written: size " + std::to_string(text.size()));
}

}  // namespace

int main() {
  sharing_and_leaves();
  cycles();
  deep();
  return mosa_test::failures();
}
