// ADR 0011 の深い項の確かめ。深さ 1,000,000 の項と長いリストを C++ の再帰なしで扱う。
// machine_test から分けたのは、MOSA_GC_STRESS=1 では GC が項全体をなめる回数が深さに比例して 2 乗になり、
// CI の GC ストレスのジョブでこれだけを N=1000 の段に置くため（machine の他の確かめは N=1 のまま）。
#include "harness.hpp"
#include "mosa/ir.hpp"
#include "mosa/machine.hpp"

#include <string>
#include <utility>

namespace {

// ADR 0011: REPL の答え（format_answer）は深さ 1,000,000 の項と長いリストを C++ の再帰なしで書く。
void check_answer_deep() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("nest(0, X, X) :- !. nest(N, X, Y) :- M is N - 1, nest(M, f(X), Y). "
                                         "mk(0, []) :- !. mk(N, [N|T]) :- M is N - 1, mk(M, T).")
                       .ok,
                   "answer deep: load");
  const auto nested = machine->query("nest(1000000, a, T).");
  const std::string text = mosa::format_answer(*machine, nested);
  mosa_test::check(text.size() == 4U + 3U * 1000000U + 1U && text.starts_with("T = f(f(") &&
                       text.substr(4U + 2U * 1000000U - 2U, 4) == "f(a)" && text.ends_with(")))"),
                   "answer deep: nested f");
  const auto listed = machine->query("mk(1000000, L).");
  const std::string list = mosa::format_answer(*machine, listed);
  mosa_test::check(list.starts_with("L = [1000000,999999,") && list.ends_with(",3,2,1]"), "answer deep: long list");
}

// ADR 0011: 長さ 1,000,000 のリストの IrTerm を、C++ の再帰なしで複製し、比べ、破棄する。
void check_ir_deep_copy_destroy() {
  constexpr int length = 1000000;
  mosa::IrTerm list;
  list.tag = mosa::IrTag::Atom;
  for (int index = length; index >= 1; --index) {
    mosa::IrTerm item;
    item.tag = mosa::IrTag::Int;
    item.integer = index;
    mosa::IrTerm cell;
    cell.tag = mosa::IrTag::List;
    cell.args.push_back(item);
    cell.args.push_back(std::move(list));
    list = std::move(cell);
  }
  bool same = true;
  int count = 0;
  {
    const mosa::IrTerm copy = list;
    const mosa::IrTerm* left = &list;
    const mosa::IrTerm* right = &copy;
    while (same && left->tag == mosa::IrTag::List) {
      same = right->tag == mosa::IrTag::List && left->args[0].integer == right->args[0].integer;
      left = &left->args[1];
      right = &right->args[1];
      ++count;
    }
    same = same && right->tag == left->tag;
  }
  mosa_test::check(same && count == length, "ir deep copy: same list");
  mosa::IrTerm assigned;
  assigned = list;
  list = std::move(list.args[1]);
  mosa_test::check(list.tag == mosa::IrTag::List && list.args[0].integer == 2 && assigned.args[0].integer == 1,
                   "ir deep copy: assign and move from a child");
}

// ADR 0011: 200,000 要素のリストを持つ事実の節を組み立てる（Build::data）。
void check_compile_long_list() {
  std::string program = "data([";
  for (int index = 1; index <= 200000; ++index) program += (index == 1 ? "" : ",") + std::to_string(index);
  program += "]).";
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(program).ok, "compile long list: load");
}

}  // namespace

int main() {
  check_answer_deep();
  check_ir_deep_copy_destroy();
  check_compile_long_list();
  return mosa_test::failures();
}
