// module/2 と export/1 の読み込み（SPEC 第5節、計画の Task 4）。読み込みの失敗は例外ではなく診断なので C++ で見る。
#include "harness.hpp"
#include "mosa/machine.hpp"

#include <string>

namespace {

bool succeeds(mosa::Machine& machine, std::string_view goal) {
  return machine.query(goal).kind == mosa::StopKind::Success;
}

bool mentions(const mosa::Machine& machine, std::string_view text) {
  return std::string{machine.diagnostic()}.find(text) != std::string::npos;
}

void check_module_text() {
  auto machine = mosa::Machine::create();
  const auto status = machine->load_program(":- module(m, [p/1]). p(1). q(2).");
  mosa_test::check(status.ok, "module text loads");
  mosa_test::check(succeeds(*machine, "p(1)."), "export imported into user");
  mosa_test::check(succeeds(*machine, "m:q(2)."), "private reachable with m:");
  mosa_test::check(machine->query("q(2).").kind == mosa::StopKind::Exception, "private hidden from user");
}

void check_not_first() {
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program("p(1).\n:- module(m, []).\n").ok, "module/2 after a clause fails");
  mosa_test::check(mentions(*machine, "permission_error(create,module,m)"), "permission_error(create, module, m)");
  mosa_test::check(machine->query("m:true.").kind == mosa::StopKind::Exception, "rejected module is not created");
}

void check_bad_module_name() {
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program(":- module(1, []).").ok, "module name 1 fails");
  mosa_test::check(mentions(*machine, "type_error(atom,1)"), "type_error(atom, 1)");
  auto unbound = mosa::Machine::create();
  mosa_test::check(!unbound->load_program(":- module(_, []).").ok, "unbound module name fails");
  mosa_test::check(mentions(*unbound, "instantiation_error"), "instantiation_error");
}

void check_bad_export() {
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program(":- module(m, [foo]).").ok, "export foo fails");
  mosa_test::check(mentions(*machine, "type_error(predicate_indicator,foo)"), "type_error(predicate_indicator, foo)");
}

void check_rollback() {
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->load_program(":- module(m, [p/1]). p(1). p(").ok, "syntax error fails");
  mosa_test::check(machine->query("m:true.").kind == mosa::StopKind::Exception, "failed load leaves no module");
  mosa_test::check(machine->load_program("r(1).").ok, "later text goes to user");
  mosa_test::check(succeeds(*machine, "user:r(1)."), "context back to user");
}

}  // namespace

int main() {
  check_module_text();
  check_not_first();
  check_bad_module_name();
  check_bad_export();
  check_rollback();
  return mosa_test::failures();
}
