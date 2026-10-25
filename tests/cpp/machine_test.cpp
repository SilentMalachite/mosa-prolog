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

void check_types() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("var(X).").kind == mosa::StopKind::Success, "var");
  mosa_test::check(machine->query("var(a).").kind == mosa::StopKind::Failure, "not var");
  mosa_test::check(machine->query("atom([]).").kind == mosa::StopKind::Success, "nil atom");
  mosa_test::check(machine->query("integer(1.0).").kind == mosa::StopKind::Failure, "float is not integer");
  mosa_test::check(machine->query("compound([]).").kind == mosa::StopKind::Failure, "nil not compound");
  mosa_test::check(machine->query("callable(1).").kind == mosa::StopKind::Failure, "callable number");
  mosa_test::check(machine->query("callable(X).").kind == mosa::StopKind::Failure, "callable var");
  mosa_test::check(machine->query("is_list([a]).").kind == mosa::StopKind::Success, "proper list");
  mosa_test::check(machine->query("is_list([a|b]).").kind == mosa::StopKind::Failure, "partial list");
  mosa_test::check(machine->query("is_list(X).").kind == mosa::StopKind::Failure, "unbound list");
  auto cycle = machine->query("(X = [X], is_list(X)).");
  mosa_test::check(cycle.kind == mosa::StopKind::Failure, "cyclic list");
  mosa_test::check(machine->query("(X = f(X), unify_with_occurs_check(Y, f(Y))).").kind ==
                       mosa::StopKind::Failure,
                   "occurs check fails");
  mosa_test::check(machine->query("missing(1).").kind == mosa::StopKind::Exception, "unknown predicate");
  mosa_test::check(machine->query("X = f(X).").kind == mosa::StopKind::Success, "no occurs on equals");
  mosa_test::check(machine->query("X = [X].").kind == mosa::StopKind::Success, "no occurs on list");
}

void check_order() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("(1.0 @< 1).").kind == mosa::StopKind::Success, "float before integer");
  mosa_test::check(machine->query("(1 == 1.0).").kind == mosa::StopKind::Failure, "term equality is not numeric");
  mosa_test::check(machine->query("(0.0 @< -0.0).").kind == mosa::StopKind::Failure, "signed zero equal");
  mosa_test::check(machine->query("compare(=, a, a).").kind == mosa::StopKind::Success, "compare same");
  mosa_test::check(machine->query("(a @< b).").kind == mosa::StopKind::Success, "atom order");
  mosa_test::check(machine->query("(X = f(X), Y = f(Y), X @< Y).").kind == mosa::StopKind::Failure,
                   "cyclic order stops");
}

void check_terms() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("functor(f(a, b), f, 2).").kind == mosa::StopKind::Success, "functor");
  mosa_test::check(machine->query("arg(1, f(a, b), a).").kind == mosa::StopKind::Success, "arg");
  mosa_test::check(machine->query("(f(a, b) =.. [f, a, b]).").kind == mosa::StopKind::Success, "univ");
  mosa_test::check(machine->query("(copy_term(f(A), f(B)), A = 1, var(B)).").kind == mosa::StopKind::Success,
                   "copy shares no var");
  mosa_test::check(machine->query("term_variables(f(A, A, B), [A, B]).").kind == mosa::StopKind::Success,
                   "term variables");
  mosa_test::check(machine->query("(T =.. ['.', a, []], T = [a], T == [a], is_list(T)).").kind ==
                       mosa::StopKind::Success,
                   "dot functor is a list");
  mosa_test::check(machine->query("(functor(T, '.', 2), arg(1, T, a), arg(2, T, []), T = [a]).").kind ==
                       mosa::StopKind::Success,
                   "functor dot builds a list");
}

void check_answer_and_consult() {
  auto machine = mosa::Machine::create();
  auto cycle = machine->query("X = [X].");
  mosa_test::check(mosa::format_answer(*machine, cycle) == "X = [X]", "cyclic answer");
  auto tail = machine->query("X = [a|X].");
  mosa_test::check(mosa::format_answer(*machine, tail) == "X = [a|X]", "cyclic tail name");
  auto named_tail = machine->query("(X = [a|Y], Y = b).");
  mosa_test::check(mosa::format_answer(*machine, named_tail) == "X = [a|Y], Y = b", "named list tail");
  auto alias = machine->query("X = Y.");
  mosa_test::check(mosa::format_answer(*machine, alias) == "X = Y", "alias once");
  auto both = machine->query("(X = Y, Y = 1).");
  mosa_test::check(mosa::format_answer(*machine, both) == "X = 1, Y = 1", "both ground");
  mosa_test::check(machine->load_program("p(1).").ok, "first p");
  mosa_test::check(machine->load_program("p(2).").ok, "replace p");
  auto replaced = machine->query("p(X).");
  mosa_test::check(replaced.kind == mosa::StopKind::Success && !replaced.more &&
                       binding_text(*machine, replaced) == "2",
                   "not doubled");
  auto broken = mosa::Machine::create();
  mosa_test::check(!broken->load_program("q(1). r(1). q(2).").ok, "discontiguous q");
  mosa_test::check(broken->diagnostic().find("述語 q/1 の節が離れている") != std::string_view::npos,
                   "discontiguous phrase");
}

void check_discontiguous() {
  auto joined = mosa::Machine::create();
  mosa_test::check(joined->load_program(":- discontiguous(p/1). p(1). q(1). p(2).").ok, "gap allowed");
  auto first = joined->query("p(X).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more && binding_text(*joined, first) == "1",
                   "first clause");
  auto second = joined->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && !second.more && binding_text(*joined, second) == "2",
                   "second clause");
  mosa_test::check(joined->load_program("p(9).").ok, "replace after directive");
  auto only = joined->query("p(X).");
  mosa_test::check(only.kind == mosa::StopKind::Success && !only.more && binding_text(*joined, only) == "9",
                   "not kept");

  auto other = mosa::Machine::create();
  mosa_test::check(!other->load_program(":- discontiguous(p/1). q(1). r(1). q(2).").ok, "other stays apart");
  mosa_test::check(other->diagnostic().find("述語 q/1 の節が離れている") != std::string_view::npos, "other phrase");

  auto arity = mosa::Machine::create();
  mosa_test::check(!arity->load_program(":- discontiguous(p/2). p(1). q(1). p(2).").ok, "arity stays apart");
  mosa_test::check(arity->diagnostic().find("述語 p/1 の節が離れている") != std::string_view::npos, "arity phrase");

  auto late = mosa::Machine::create();
  mosa_test::check(!late->load_program("p(1). q(1). p(2). :- discontiguous(p/1).").ok, "directive after gap");
  mosa_test::check(late->diagnostic().find("述語 p/1 の節が離れている") != std::string_view::npos, "late phrase");

  auto bad = mosa::Machine::create();
  mosa_test::check(!bad->load_program(":- discontiguous(1).").ok, "bad indicator");
  mosa_test::check(bad->diagnostic().find("述語指示子ではありません") != std::string_view::npos, "indicator phrase");

  auto empty = mosa::Machine::create();
  mosa_test::check(empty->load_program(":- discontiguous([]).").ok, "empty list");
}

void check_arith() {
  auto quot_machine = mosa::Machine::create();
  auto quot = quot_machine->query("X is -5 // 2.");
  const auto q = binding_text(*quot_machine, quot);
  mosa_test::check(quot.kind == mosa::StopKind::Success && q == "-2", "toward zero is -2");
  auto floor_machine = mosa::Machine::create();
  auto floor = floor_machine->query("X is div(-5, 2).");
  const auto d = binding_text(*floor_machine, floor);
  mosa_test::check(floor.kind == mosa::StopKind::Success && d == "-3", "floor div is -3");
  auto errors = mosa::Machine::create();
  mosa_test::check(errors->query("X is 1 / 0.").kind == mosa::StopKind::Exception, "zero divisor");
  mosa_test::check(errors->query("X is 9223372036854775807 + 1.").kind == mosa::StopKind::Exception,
                   "int overflow");
  mosa_test::check(errors->query("(1.0 =:= 1).").kind == mosa::StopKind::Success, "numeric equal");
}

void check_control() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("(call(!), fail ; true).").kind == mosa::StopKind::Success,
                   "cut stays inside call");
  mosa_test::check(machine->query("(\\+ fail).").kind == mosa::StopKind::Success, "not fail");
  mosa_test::check(machine->query("(\\+ true).").kind == mosa::StopKind::Failure, "not true");
  auto halted = machine->query("halt(3).");
  mosa_test::check(halted.kind == mosa::StopKind::Halt && halted.halt_code == 3, "halt code");
  auto again = mosa::Machine::create();
  mosa_test::check(again->query("call(1).").kind == mosa::StopKind::Exception, "call number");
  mosa_test::check(again->load_program(":- op(600, xfy, *->). p(1). p(2).").ok, "soft cut data");
  auto soft = again->query("(p(X) *-> true ; X = 9).");
  mosa_test::check(soft.kind == mosa::StopKind::Success && soft.more, "soft cut keeps solutions");
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

bool atom_named(mosa::Machine& machine, mosa::Cell cell, std::string_view name) {
  cell = machine.heap().deref(cell);
  if (cell.tag() != mosa::Tag::Atom) return false;
  return machine.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == name;
}

bool functor_named(mosa::Machine& machine, mosa::Cell cell, std::string_view name, int arity) {
  cell = machine.heap().deref(cell);
  return cell.tag() == mosa::Tag::Str && machine.heap().functor_arity(cell) == static_cast<unsigned>(arity) &&
         machine.atoms().name(machine.heap().functor_atom(cell)) == name;
}

bool dot_procedure(mosa::Machine& machine, mosa::Cell pi) {
  pi = machine.heap().deref(pi);
  if (!functor_named(machine, pi, "/", 2)) return false;
  const auto arity = machine.heap().deref(machine.heap().arg(pi, 1));
  return atom_named(machine, machine.heap().arg(pi, 0), ".") &&
         (arity.tag() == mosa::Tag::Int || arity.tag() == mosa::Tag::IntBox) &&
         machine.heap().integer_value(arity) == 2;
}

bool dot_missing(mosa::Machine& machine, const mosa::Stop& stop) {
  if (stop.kind != mosa::StopKind::Exception) return false;
  const mosa::Cell error = machine.heap().deref(stop.exception);
  if (!functor_named(machine, error, "error", 2)) return false;
  const mosa::Cell formal = machine.heap().deref(machine.heap().arg(error, 0));
  if (!functor_named(machine, formal, "existence_error", 2)) return false;
  if (!atom_named(machine, machine.heap().arg(formal, 0), "procedure")) return false;
  if (!dot_procedure(machine, machine.heap().arg(formal, 1))) return false;
  const mosa::Cell context = machine.heap().deref(machine.heap().arg(error, 1));
  return functor_named(machine, context, "context", 2) && dot_procedure(machine, machine.heap().arg(context, 0));
}

void check_list_goal() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("callable([a]).").kind == mosa::StopKind::Success, "list is callable");
  mosa_test::check(dot_missing(*machine, machine->query("'.'(a, []).")), "dot goal is '.'/2");
  mosa_test::check(dot_missing(*machine, machine->query("[a].")), "list goal is '.'/2");
  mosa_test::check(dot_missing(*machine, machine->query("call([a]).")), "call list is '.'/2");
  mosa_test::check(dot_missing(*machine, machine->query("(X = [a], call(X)).")), "bound list is '.'/2");
  auto listed = mosa::Machine::create();
  mosa_test::check(listed->load_program("[a].").ok, "list clause");
  mosa_test::check(listed->query("[a].").kind == mosa::StopKind::Success, "defined list runs");
  mosa_test::check(listed->query("'.'(a, []).").kind == mosa::StopKind::Success, "list clause via dot");
  auto dotted = mosa::Machine::create();
  mosa_test::check(dotted->load_program("'.'(b, []).").ok, "dot clause");
  mosa_test::check(dotted->query("[b].").kind == mosa::StopKind::Success, "defined dot runs");
  mosa_test::check(dotted->query("'.'(b, []).").kind == mosa::StopKind::Success, "defined dot syntax");
}

}  // namespace

int main() {
  check_one_fact();
  check_choice_and_nrev();
  check_cut();
  check_catch();
  check_types();
  check_order();
  check_terms();
  check_answer_and_consult();
  check_discontiguous();
  check_arith();
  check_control();
  check_index();
  check_list_goal();
  return mosa_test::failures();
}
