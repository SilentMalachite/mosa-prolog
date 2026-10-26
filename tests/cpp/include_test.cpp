// 7.4.2 include/1。指示だけ。節は同じテキストの途中に入る。
// 開けないファイルの診断は「ファイルを開けません」。一時ファイルは tests/tmp/include1。

#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

std::string binding_text(mosa::Machine& machine, const mosa::Stop& stop) {
  return mosa::write_term(machine.heap(), machine.operators(),
                          machine.heap().deref(stop.bindings[0].variable), mosa::WriteOptions{}, {});
}

bool write_text(const std::filesystem::path& path, std::string_view text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << text;
  return static_cast<bool>(output);
}

std::string prolog_path(const std::filesystem::path& path) {
  std::string out = "'";
  for (char unit : path.generic_string()) {
    if (unit == '\'') out += "''";
    else if (unit == '\\') out += "\\\\";
    else out.push_back(unit);
  }
  out.push_back('\'');
  return out;
}

std::string include_of(const std::filesystem::path& path) { return ":- include(" + prolog_path(path) + ")."; }

std::string answers(mosa::Machine& machine, std::string_view goal) {
  std::string out;
  auto stop = machine.query(goal);
  while (stop.kind == mosa::StopKind::Success) {
    if (stop.bindings.empty()) return {};
    if (!out.empty()) out += ",";
    out += binding_text(machine, stop);
    if (!stop.more) return out;
    stop = machine.next();
  }
  return {};
}

void check_include_order(const std::filesystem::path& dir) {
  const auto child = dir / "child.pl";
  mosa_test::check(write_text(child, "p(2).\n"), "write child");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(1). " + include_of(child) + " p(3).").ok, "include load");
  mosa_test::check(answers(*machine, "p(X).") == "1,2,3", "include order");
  auto query_dir = mosa::Machine::create();
  mosa_test::check(query_dir->load_program("?- include(" + prolog_path(child) + ").").ok, "query directive");
  mosa_test::check(answers(*query_dir, "p(X).") == "2", "query directive clause");
}

void check_include_gap(const std::filesystem::path& dir) {
  const auto gap = dir / "gap.pl";
  mosa_test::check(write_text(gap, "q(1).\n"), "write gap");
  auto broken = mosa::Machine::create();
  const auto status = broken->load_program("p(1). " + include_of(gap) + " p(2).");
  mosa_test::check(!status.ok, "gap fails");
  mosa_test::check(broken->diagnostic().find("述語 p/1 の節が離れている") != std::string_view::npos, "gap phrase");
  mosa_test::check(broken->query("current_predicate(p/1).").kind == mosa::StopKind::Failure, "gap rolls back");
  mosa_test::check(broken->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "gap drops included");

  const auto apart = dir / "apart.pl";
  mosa_test::check(write_text(apart, ":- discontiguous(p/1).\nq(1).\n"), "write apart");
  auto joined = mosa::Machine::create();
  mosa_test::check(joined->load_program("p(1). " + include_of(apart) + " p(2).").ok, "apart load");
  mosa_test::check(answers(*joined, "p(X).") == "1,2", "apart order");
  mosa_test::check(joined->query("q(1).").kind == mosa::StopKind::Success, "apart neighbor");
}

void check_include_nested(const std::filesystem::path& dir) {
  const auto leaf = dir / "leaf.pl";
  const auto mid = dir / "mid.pl";
  mosa_test::check(write_text(leaf, "p(3).\n"), "write leaf");
  mosa_test::check(write_text(mid, "p(2).\n:- include('leaf.pl').\n"), "write mid");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(1). " + include_of(mid) + " p(4).").ok, "nested load");
  mosa_test::check(answers(*machine, "p(X).") == "1,2,3,4", "nested order");

  const auto one = dir / "one.pl";
  mosa_test::check(write_text(one, "s(1).\n"), "write one");
  auto twice = mosa::Machine::create();
  const auto text = include_of(one);
  mosa_test::check(twice->load_program(text + " " + text).ok, "twice load");
  mosa_test::check(answers(*twice, "s(X).") == "1,1", "twice doubles");
}

void check_include_relative(const std::filesystem::path& dir) {
  mosa_test::check(write_text(dir / "child.pl", "p(2).\n"), "write relative child");
  const auto rel = dir / "rel.pl";
  mosa_test::check(write_text(rel, "p(1).\n:- include('child.pl').\np(3).\n"), "write rel");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(rel.generic_string()), "relative consult");
  mosa_test::check(answers(*machine, "p(X).") == "1,2,3", "relative order");
}

void check_include_init(const std::filesystem::path& dir) {
  const auto init = dir / "init.pl";
  mosa_test::check(write_text(init, ":- initialization(grab).\n"), "write init");
  auto machine = mosa::Machine::create();
  const bool ok = machine
                      ->load_program(":- dynamic(s/1).\n"
                                     ":- initialization(assertz(s(1))).\n" +
                                     include_of(init) +
                                     "\n:- initialization(assertz(s(3))).\n"
                                     "grab :- assertz(s(9)).\n")
                      .ok;
  mosa_test::check(ok, "init include load");
  mosa_test::check(answers(*machine, "s(X).") == "1,9,3", "init include order");
}

void check_include_throw(const std::filesystem::path& dir) {
  const auto thrown = dir / "thrown.pl";
  mosa_test::check(write_text(thrown, ":- initialization(throw(1)).\n"), "write thrown");
  auto machine = mosa::Machine::create();
  const auto status = machine->load_program("kept(1).\n:- dynamic(s/1).\n" + include_of(thrown) +
                                            "\n:- initialization(assertz(s(4))).\n");
  mosa_test::check(!status.ok && machine->diagnostic() == "1", "include throw");
  mosa_test::check(machine->query("kept(1).").kind == mosa::StopKind::Success, "throw keeps clause");
  mosa_test::check(machine->query("s(4).").kind == mosa::StopKind::Failure, "later init skipped");
}

void check_include_args(const std::filesystem::path& dir) {
  auto bad = mosa::Machine::create();
  const auto number = bad->load_program("q(1). :- include(1).");
  mosa_test::check(!number.ok && bad->diagnostic() == "type_error(atom, 1)", "number formal");
  mosa_test::check(bad->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "number rolls back");

  auto open = mosa::Machine::create();
  const auto var = open->load_program(":- include(X).");
  mosa_test::check(!var.ok && open->diagnostic() == "instantiation_error", "var formal");

  auto listed = mosa::Machine::create();
  const auto list = listed->load_program(":- include([a]).");
  mosa_test::check(!list.ok && listed->diagnostic() == "type_error(atom, [a])", "list formal");

  auto two = mosa::Machine::create();
  const auto arity = two->load_program(":- include(true, now).");
  mosa_test::check(!arity.ok && two->diagnostic().find("この指示はフェーズ2では使えない") != std::string_view::npos,
                   "arity two");

  auto empty = mosa::Machine::create();
  const auto nil = empty->load_program(":- include([]).");
  mosa_test::check(!nil.ok && empty->diagnostic() == "[]: ファイルを開けません", "empty atom is a file");

  const auto missing = dir / "missing.pl";
  auto gone = mosa::Machine::create();
  const auto status = gone->load_program("q(1). " + include_of(missing));
  const auto shown = std::filesystem::path{missing.generic_string()}.string();
  mosa_test::check(!status.ok && gone->diagnostic() == shown + ": ファイルを開けません", "missing phrase");
  mosa_test::check(gone->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "missing rolls back");
}

void check_include_operator(const std::filesystem::path& dir) {
  const auto op = dir / "op.pl";
  mosa_test::check(write_text(op, ":- op(500, xfx, は).\n"), "write op");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(include_of(op) + "\na は b.\n").ok, "op include");
  mosa_test::check(machine->query("は(a, b).").kind == mosa::StopKind::Success, "op applies after include");

  const auto broken = dir / "broken.pl";
  mosa_test::check(write_text(broken, ":- op(500, xfx, は).\na は\n"), "write broken");
  auto failed = mosa::Machine::create();
  const auto status = failed->load_program("q(1). " + include_of(broken));
  mosa_test::check(!status.ok, "broken include");
  mosa_test::check(failed->diagnostic().find("項が閉じていません") != std::string::npos, "broken reason");
  mosa_test::check(failed->diagnostic().find(broken.generic_string()) != std::string::npos, "broken path");
  mosa_test::check(failed->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "broken rolls back");
  mosa_test::check(!failed->load_program("a は b.").ok, "op rolled back");
}

void check_include_cycle(const std::filesystem::path& dir) {
  const auto cycle = dir / "cycle.pl";
  mosa_test::check(write_text(cycle, "p(1).\n:- include('cycle.pl').\n"), "write cycle");
  auto machine = mosa::Machine::create();
  const auto status = machine->load_program(include_of(cycle));
  mosa_test::check(!status.ok, "cycle fails");
  mosa_test::check(machine->diagnostic().find("ファイルが循環しています") != std::string_view::npos, "cycle phrase");
  mosa_test::check(machine->query("current_predicate(p/1).").kind == mosa::StopKind::Failure, "cycle rolls back");
}

void check_include_japanese(const std::filesystem::path& dir) {
  const auto ja = dir / "節.pl";
  // 連は親のうしろに置く。間に入ると、宣言の無い親は離れた節になる。
  mosa_test::check(write_text(ja, "親(2).\n連(1).\n"), "write ja");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("親(1). " + include_of(ja)).ok, "ja include");
  mosa_test::check(answers(*machine, "親(X).") == "1,2", "ja order");
  mosa_test::check(machine->query("連(1).").kind == mosa::StopKind::Success, "ja clause");
}

void check_include_goal() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("include(missing).").kind == mosa::StopKind::Exception, "not a goal");
}

}  // namespace

int main() {
  const std::filesystem::path dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "include1";
  std::filesystem::create_directories(dir);
  mosa_test::check(std::filesystem::is_directory(dir), "include dir");
  check_include_order(dir);
  check_include_gap(dir);
  check_include_nested(dir);
  check_include_relative(dir);
  check_include_init(dir);
  check_include_throw(dir);
  check_include_args(dir);
  check_include_operator(dir);
  check_include_cycle(dir);
  check_include_japanese(dir);
  check_include_goal();
  return mosa_test::failures();
}
