// 文法規則の読み込み時の誤り（ADR 0024）。tests/pl/dcg.pl は読み込みが失敗することしか見られないので、ここで
// 診断の文に出る Formal を比べる。Context は比べない。

#include "harness.hpp"
#include "mosa/machine.hpp"

#include <string>
#include <string_view>

namespace {

void check_refused(std::string_view program, std::string_view formal) {
  auto machine = mosa::Machine::create();
  const mosa::LoadStatus status = machine->load_program(program);
  const std::string label{program};
  mosa_test::check(!status.ok, label + ": refused");
  mosa_test::check(status.message.find(formal) != std::string::npos, label + ": " + std::string{formal});
}

void check_loaded() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("a --> [x], b.\nb --> [].\n").ok, "a grammar rule loads");
}

}  // namespace

int main() {
  check_loaded();
  check_refused("X --> [a].\n", "instantiation_error");
  check_refused("1 --> [a].\n", "type_error(callable,1)");
  check_refused("a --> [x], 1.\n", "type_error(callable,1)");
  check_refused("a --> [x|b].\n", "type_error(list,[x|b])");
  check_refused("a --> [x|_].\n", "instantiation_error");
  check_refused("a, x --> [a].\n", "type_error(list,x)");
  check_refused("a, [x|_] --> [a].\n", "instantiation_error");
  // 書き出しの表の Name//Arity は Name/(Arity+2) と同じ検査を受ける（ADR 0024 改訂 3）。
  check_refused(":- module(m_neg, [x//(-1)]).\n", "domain_error(not_less_than_zero,-1)");
  check_refused(":- module(m_big, [x//1023]).\n", "representation_error(max_arity)");
  check_refused(":- module(m_atom, [1//0]).\n", "type_error(atom,1)");
  return mosa_test::failures();
}
