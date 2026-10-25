#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <exception>
#include <string>

namespace {

std::string binding_text(mosa::Machine& machine, const mosa::Stop& stop) {
  return mosa::write_term(machine.heap(), machine.operators(),
                          machine.heap().deref(stop.bindings[0].variable), mosa::WriteOptions{}, {});
}

void check_one_fact() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(a).").ok, "load fact");
  auto yes = machine->query("p(a).");
  mosa_test::check(yes.kind == mosa::StopKind::Success && !yes.more, "fact hits");
  auto no = machine->query("p(b).");
  mosa_test::check(no.kind == mosa::StopKind::Failure, "fact misses");
  auto bound = machine->query("p(X).");
  mosa_test::check(bound.kind == mosa::StopKind::Success && bound.bindings.size() == 1, "fact binds");
  mosa_test::check(bound.bindings[0].name == "X", "name X");
  mosa_test::check(binding_text(*machine, bound) == "a", "value a");
}

void check_choice_and_nrev() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(1). p(2).").ok, "two facts");
  auto first = machine->query("p(X).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more, "first choice");
  auto second = machine->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && !second.more, "second choice");
  auto branch = machine->query("(true ; fail).");
  mosa_test::check(branch.kind == mosa::StopKind::Success && branch.more, "disjunction");
  auto branch_next = machine->next();
  mosa_test::check(branch_next.kind == mosa::StopKind::Failure, "disjunction ends");
  mosa_test::check(machine->load_program(
                       "nrev([], []). nrev([H|T], R) :- nrev(T, S), append(S, [H], R). "
                       "append([], L, L). append([H|T], L, [H|R]) :- append(T, L, R).")
                       .ok,
                   "load nrev");
  auto rev = machine->query("nrev([1, 2, 3], R).");
  mosa_test::check(rev.kind == mosa::StopKind::Success && rev.bindings.size() == 1 &&
                       rev.bindings[0].name == "R",
                   "nrev binds");
  mosa_test::check(binding_text(*machine, rev) == "[3, 2, 1]", "nrev text");
}

void check_cut() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p :- !, fail. p.").ok, "load cut");
  mosa_test::check(machine->query("p.").kind == mosa::StopKind::Failure, "cut drops second clause");
  mosa_test::check(machine->load_program("q(X) :- (X = 1 ; X = 2), !.").ok, "load local cut");
  auto once = machine->query("q(X).");
  mosa_test::check(once.kind == mosa::StopKind::Success && !once.more && binding_text(*machine, once) == "1",
                   "cut drops sibling");
  mosa_test::check(machine->load_program("r(X) :- true -> X = 1 ; X = 2. s(X) :- fail -> X = 1 ; X = 2.").ok,
                   "load if then");
  auto then = machine->query("r(X).");
  mosa_test::check(then.kind == mosa::StopKind::Success && !then.more && binding_text(*machine, then) == "1",
                   "then commits");
  auto otherwise = machine->query("s(X).");
  mosa_test::check(otherwise.kind == mosa::StopKind::Success && !otherwise.more &&
                       binding_text(*machine, otherwise) == "2",
                   "else runs");
}

void check_catch() {
  auto machine = mosa::Machine::create();
  auto caught = machine->query("catch(throw(a), a, true).");
  mosa_test::check(caught.kind == mosa::StopKind::Success, "catch same ball");
  auto missed = machine->query("catch(throw(b), a, true).");
  mosa_test::check(missed.kind == mosa::StopKind::Exception, "catch misses");
  try {
    auto tiny = mosa::Machine::create(8);
    auto boom = tiny->query("catch(p(a), _, true).");
    mosa_test::check(boom.kind != mosa::StopKind::Success && boom.kind != mosa::StopKind::Exception,
                     "resource is not a prolog ball");
  } catch (const mosa::ResourceLimit&) {
    mosa_test::check(true, "resource is not a prolog ball");
  }
}

void check_index() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(a). p(b).").ok, "index load");
  auto one = machine->query("p(a).");
  mosa_test::check(one.kind == mosa::StopKind::Success && !one.more, "single candidate");
  auto any = machine->query("p(X).");
  mosa_test::check(any.kind == mosa::StopKind::Success && any.more, "variable tries all");
  mosa_test::check(machine->load_program("q(a). q(X) :- X = c.").ok, "var head kept");
  auto q = machine->query("q(a).");
  mosa_test::check(q.kind == mosa::StopKind::Success && q.more, "variable head remains");
}

}  // namespace

int main() {
  check_one_fact();
  check_choice_and_nrev();
  check_cut();
  check_catch();
  check_index();
  return mosa_test::failures();
}
