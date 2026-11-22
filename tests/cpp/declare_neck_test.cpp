// レビュー 2026-11-20 の M-14（iso-notes 7.4、7.4.2.1、8.9.1.3）。:-/1 は指示、:-/2 は節の首で、assertz/1 は静的な
// 手続きとして断る。dynamic/1、discontiguous/1、multifile/1、meta_predicate/1 の宣言も同じく
// permission_error(modify, static_procedure, PI) で断る。前は組込だけを見て、dynamic((:-)/1) が動的な :- を作った。

#include "harness.hpp"
#include "mosa/machine.hpp"

#include <array>
#include <string>
#include <string_view>

namespace {

void check_refused(std::string_view directive, std::string_view arity) {
  auto machine = mosa::Machine::create();
  const std::string source = ":- " + std::string{directive} + ".\n";
  const std::string label = std::string{directive};
  mosa_test::check(!machine->load_program(source).ok, label + ": the load fails");
  const std::string want = "permission_error(modify,static_procedure,(:-)/" + std::string{arity} + ")";
  mosa_test::check(machine->diagnostic().find(want) != std::string::npos,
                   label + ": diagnostic [" + std::string{machine->diagnostic()} + "]");
  // 宣言は何も残さない。:- は今も静的で、assertz/1 は同じ誤りで断る。
  mosa_test::check(machine->query("current_predicate((:-)/_).").kind == mosa::StopKind::Failure,
                   label + ": no :- procedure");
}

}  // namespace

int main() {
  const std::array<std::string_view, 4> names{"dynamic", "discontiguous", "multifile", "meta_predicate"};
  for (const std::string_view name : names) {
    const bool meta = name == "meta_predicate";
    check_refused(std::string{name} + (meta ? "(':-'(0))" : "((:-)/1)"), "1");
    check_refused(std::string{name} + (meta ? "(':-'(0, 0))" : "((:-)/2)"), "2");
  }
  return mosa_test::failures();
}
