// 7.4.2 ensure_loaded/1。指示だけ。同じパスは一度だけ節にする。
// include/1 は二度入れる。開けないファイルの診断は「ファイルを開けません」。
// 一時ファイルは tests/tmp/ensure1。

#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/path_text.hpp"
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
  // 原子は UTF-8 で書く。Windows の generic_string は ANSI コードページで、日本語の名前が化ける。
  for (char unit : mosa::utf8_from_path(path)) {
    if (unit == '\'') out += "''";
    else if (unit == '\\') out += "\\\\";
    else out.push_back(unit);
  }
  out.push_back('\'');
  return out;
}

std::string ensure_of(const std::filesystem::path& path) {
  return ":- ensure_loaded(" + prolog_path(path) + ").";
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

void check_ensure_once(const std::filesystem::path& dir) {
  const auto child = dir / "once.pl";
  mosa_test::check(write_text(child, "s(1).\n"), "write once");
  auto machine = mosa::Machine::create();
  const auto text = ensure_of(child);
  mosa_test::check(machine->load_program(text + " " + text).ok, "once load");
  mosa_test::check(answers(*machine, "s(X).") == "1", "once keeps one");

  auto query_dir = mosa::Machine::create();
  mosa_test::check(query_dir->load_program("?- ensure_loaded(" + prolog_path(child) + ").").ok, "query directive");
  mosa_test::check(answers(*query_dir, "s(X).") == "1", "query directive clause");
}

void check_ensure_vs_include(const std::filesystem::path& dir) {
  const auto child = dir / "both.pl";
  mosa_test::check(write_text(child, "s(1).\n"), "write both");
  auto after = mosa::Machine::create();
  mosa_test::check(after->load_program(ensure_of(child) + " " + include_of(child)).ok, "ensure then include");
  mosa_test::check(answers(*after, "s(X).") == "1,1", "include still doubles");

  auto before = mosa::Machine::create();
  mosa_test::check(before->load_program(include_of(child) + " " + ensure_of(child)).ok, "include then ensure");
  mosa_test::check(answers(*before, "s(X).") == "1", "ensure skips included");
}

void check_ensure_across(const std::filesystem::path& dir) {
  const auto child = dir / "shared.pl";
  mosa_test::check(write_text(child, ":- multifile(p/1).\np(1).\n"), "write shared");
  auto machine = mosa::Machine::create();
  const auto text = ensure_of(child);
  mosa_test::check(machine->load_program(text).ok, "first load");
  mosa_test::check(machine->load_program(text).ok, "second load");
  mosa_test::check(answers(*machine, "p(X).") == "1", "across loads stays one");
}

void check_ensure_retry(const std::filesystem::path& dir) {
  const auto child = dir / "retry.pl";
  mosa_test::check(write_text(child, "p(1).\n"), "write retry");
  auto machine = mosa::Machine::create();
  const auto failed = machine->load_program(ensure_of(child) + " :- discontiguous(1).");
  mosa_test::check(!failed.ok, "bad directive fails");
  mosa_test::check(machine->query("current_predicate(p/1).").kind == mosa::StopKind::Failure, "retry rolls back");
  mosa_test::check(machine->load_program(ensure_of(child)).ok, "retry loads");
  mosa_test::check(answers(*machine, "p(X).") == "1", "retry one clause");
}

void check_ensure_throw_once(const std::filesystem::path& dir) {
  const auto child = dir / "ethrow.pl";
  mosa_test::check(write_text(child, "kept(1).\n:- initialization(throw(1)).\n"), "write ethrow");
  auto machine = mosa::Machine::create();
  const auto first = machine->load_program(ensure_of(child));
  mosa_test::check(!first.ok && machine->diagnostic() == "1", "init throw");
  mosa_test::check(answers(*machine, "kept(X).") == "1", "throw keeps one");
  const auto second = machine->load_program(ensure_of(child));
  mosa_test::check(second.ok, "second ensure skips throw");
  mosa_test::check(answers(*machine, "kept(X).") == "1", "skip does not double");
}

void check_ensure_self(const std::filesystem::path& dir) {
  const auto self = dir / "self.pl";
  mosa_test::check(write_text(self, "p(1).\n:- ensure_loaded('self.pl').\n"), "write self");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(self.generic_string()), "self consult");
  mosa_test::check(answers(*machine, "p(X).") == "1", "self one clause");
}

void check_ensure_mutual(const std::filesystem::path& dir) {
  const auto left = dir / "left.pl";
  const auto right = dir / "right.pl";
  mosa_test::check(write_text(left, "p(1).\n:- ensure_loaded('right.pl').\n"), "write left");
  mosa_test::check(write_text(right, "p(2).\n:- ensure_loaded('left.pl').\n"), "write right");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(left.generic_string()), "mutual consult");
  mosa_test::check(answers(*machine, "p(X).") == "1,2", "mutual order");
}

void check_consult_reloads(const std::filesystem::path& dir) {
  const auto file = dir / "reload.pl";
  mosa_test::check(write_text(file, "p(1).\n"), "write reload 1");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(file.generic_string()), "consult first");
  mosa_test::check(write_text(file, "p(2).\n"), "write reload 2");
  mosa_test::check(machine->consult(file.generic_string()), "consult second");
  mosa_test::check(answers(*machine, "p(X).") == "2", "consult replaces");
}

void check_consult_then_ensure(const std::filesystem::path& dir) {
  const auto file = dir / "inited.pl";
  mosa_test::check(write_text(file, ":- initialization(assertz(s(1))).\n"), "write inited");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(file.generic_string()), "consult inited");
  mosa_test::check(machine->load_program(ensure_of(file)).ok, "ensure after consult");
  mosa_test::check(answers(*machine, "s(X).") == "1", "init not repeated");
}

void check_ensure_init_order(const std::filesystem::path& dir) {
  const auto init = dir / "grab.pl";
  mosa_test::check(write_text(init, ":- initialization(grab).\n"), "write grab");
  auto machine = mosa::Machine::create();
  const bool ok = machine
                      ->load_program(":- dynamic(s/1).\n"
                                     ":- initialization(assertz(s(1))).\n" +
                                     ensure_of(init) + "\n" + ensure_of(init) +
                                     "\n:- initialization(assertz(s(3))).\n"
                                     "grab :- assertz(s(9)).\n")
                      .ok;
  mosa_test::check(ok, "init ensure load");
  mosa_test::check(answers(*machine, "s(X).") == "1,9,3", "init once in order");
}

void check_ensure_order(const std::filesystem::path& dir) {
  const auto child = dir / "mid.pl";
  mosa_test::check(write_text(child, "p(2).\n"), "write mid");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(1). " + ensure_of(child) + " p(3).").ok, "order load");
  mosa_test::check(answers(*machine, "p(X).") == "1,2,3", "splice order");

  const auto gap = dir / "gap.pl";
  mosa_test::check(write_text(gap, "q(1).\n"), "write gap");
  auto broken = mosa::Machine::create();
  const auto status = broken->load_program("p(1). " + ensure_of(gap) + " p(2).");
  mosa_test::check(!status.ok, "gap fails");
  mosa_test::check(broken->diagnostic().find("述語 p/1 の節が離れている") != std::string_view::npos, "gap phrase");
  mosa_test::check(broken->query("current_predicate(p/1).").kind == mosa::StopKind::Failure, "gap rolls back");
  mosa_test::check(broken->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "gap drops ensured");
}

void check_ensure_apart(const std::filesystem::path& dir) {
  const auto apart = dir / "apart.pl";
  mosa_test::check(write_text(apart, ":- discontiguous(p/1).\nq(1).\n"), "write apart");
  auto joined = mosa::Machine::create();
  mosa_test::check(joined->load_program("p(1). " + ensure_of(apart) + " p(2).").ok, "apart load");
  mosa_test::check(answers(*joined, "p(X).") == "1,2", "apart order");
  mosa_test::check(joined->query("q(1).").kind == mosa::StopKind::Success, "apart neighbor");
}

void check_ensure_relative(const std::filesystem::path& dir) {
  const auto child = dir / "child.pl";
  mosa_test::check(write_text(child, "p(2).\n"), "write child");
  const auto rel = dir / "rel.pl";
  const auto text = "p(1).\n:- ensure_loaded('child.pl').\n:- ensure_loaded(" + prolog_path(child) + ").\np(3).\n";
  mosa_test::check(write_text(rel, text), "write rel");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(rel.generic_string()), "relative consult");
  mosa_test::check(answers(*machine, "p(X).") == "1,2,3", "relative and absolute once");
}

void check_ensure_args(const std::filesystem::path& dir) {
  auto bad = mosa::Machine::create();
  const auto number = bad->load_program("q(1). :- ensure_loaded(1).");
  mosa_test::check(!number.ok && bad->diagnostic() == "type_error(atom,1)", "number formal");
  mosa_test::check(bad->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "number rolls back");

  auto open = mosa::Machine::create();
  const auto var = open->load_program(":- ensure_loaded(X).");
  mosa_test::check(!var.ok && open->diagnostic() == "instantiation_error", "var formal");

  auto listed = mosa::Machine::create();
  const auto list = listed->load_program(":- ensure_loaded([a]).");
  mosa_test::check(!list.ok && listed->diagnostic() == "type_error(atom,[a])", "list formal");

  auto two = mosa::Machine::create();
  const auto arity = two->load_program(":- ensure_loaded(true, now).");
  mosa_test::check(!arity.ok && two->diagnostic().find("この指示はフェーズ2では使えない") != std::string_view::npos,
                   "arity two");

  auto empty = mosa::Machine::create();
  const auto nil = empty->load_program(":- ensure_loaded([]).");
  mosa_test::check(!nil.ok && empty->diagnostic() == "[]: ファイルを開けません", "empty atom is a file");

  const auto missing = dir / "missing.pl";
  auto gone = mosa::Machine::create();
  const auto status = gone->load_program("q(1). " + ensure_of(missing));
  const auto shown = std::filesystem::path{missing.generic_string()}.string();
  mosa_test::check(!status.ok && gone->diagnostic() == shown + ": ファイルを開けません", "missing phrase");
  mosa_test::check(gone->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "missing rolls back");
}

void check_ensure_operator(const std::filesystem::path& dir) {
  const auto op = dir / "op.pl";
  mosa_test::check(write_text(op, ":- op(500, xfx, は).\n"), "write op");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(ensure_of(op) + "\na は b.\n").ok, "op ensure");
  mosa_test::check(machine->query("は(a, b).").kind == mosa::StopKind::Success, "op applies");
}

void check_ensure_broken(const std::filesystem::path& dir) {
  const auto broken = dir / "broken.pl";
  mosa_test::check(write_text(broken, ":- op(500, xfx, は).\na は\n"), "write broken");
  auto failed = mosa::Machine::create();
  const auto status = failed->load_program("q(1). " + ensure_of(broken));
  mosa_test::check(!status.ok, "broken ensure");
  mosa_test::check(failed->diagnostic().find("項が閉じていません") != std::string::npos, "broken reason");
  mosa_test::check(failed->diagnostic().find(broken.generic_string()) != std::string::npos, "broken path");
  mosa_test::check(failed->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "broken rolls back");
  mosa_test::check(!failed->load_program("a は b.").ok, "op rolled back");
}

void check_ensure_japanese(const std::filesystem::path& dir) {
  const auto ja = dir / mosa::path_from_utf8("節.pl");
  // 連は親のうしろに置く。間に入ると、宣言の無い親は離れた節になる。
  mosa_test::check(write_text(ja, "親(2).\n連(1).\n"), "write ja");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("親(1). " + ensure_of(ja)).ok, "ja ensure");
  mosa_test::check(answers(*machine, "親(X).") == "1,2", "ja order");
  mosa_test::check(machine->query("連(1).").kind == mosa::StopKind::Success, "ja clause");
}

// 拡張子の無い名前だけ file.pl を足す。普通のファイルと、既にある拡張子は変えない。
// どちらも無いときの診断は、渡された名前のままにする。
void check_ensure_suffix(const std::filesystem::path& dir) {
  const auto box = dir / "suffix";
  std::error_code error;
  std::filesystem::remove_all(box, error);
  std::filesystem::create_directories(box);
  const auto bare = box / "plain";
  const auto only_pl = box / "extless.pl";
  const auto typed = box / "kept.txt";
  const auto missing = box / "absent";
  mosa_test::check(write_text(bare, "b(1).\n"), "write bare");
  mosa_test::check(write_text(box / "plain.pl", "b(9).\n"), "write bare trap");
  mosa_test::check(write_text(only_pl, "a(1).\n"), "write extless");
  mosa_test::check(write_text(typed, "c(1).\n"), "write typed");
  mosa_test::check(write_text(box / "kept.txt.pl", "c(9).\n"), "write typed trap");
  mosa_test::check(write_text(box / "kept.pl", "c(8).\n"), "write replaced trap");

  auto extless = mosa::Machine::create();
  mosa_test::check(extless->load_program(ensure_of(box / "extless")).ok, "extless load");
  mosa_test::check(answers(*extless, "a(X).") == "1", "extless reads name.pl");

  auto plain = mosa::Machine::create();
  mosa_test::check(plain->load_program(ensure_of(bare)).ok, "bare load");
  mosa_test::check(answers(*plain, "b(X).") == "1", "bare file wins");

  auto kept = mosa::Machine::create();
  mosa_test::check(kept->load_program(ensure_of(typed)).ok, "typed load");
  mosa_test::check(answers(*kept, "c(X).") == "1", "extension stays");

  auto gone = mosa::Machine::create();
  const auto status = gone->load_program(ensure_of(missing));
  const auto shown = std::filesystem::path{missing.generic_string()}.string();
  mosa_test::check(!status.ok && gone->diagnostic() == shown + ": ファイルを開けません", "missing keeps name");

  std::filesystem::remove_all(box, error);
}

void check_ensure_goal() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("ensure_loaded(missing).").kind == mosa::StopKind::Exception, "not a goal");
}

void check_ensure_fact() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(ensure_loaded('nofile.pl')).").ok, "fact is not a directive");
  mosa_test::check(machine->query("p(ensure_loaded('nofile.pl')).").kind == mosa::StopKind::Success,
                   "fact clause");
}

}  // namespace

int main() {
  const std::filesystem::path dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "ensure1";
  std::filesystem::create_directories(dir);
  mosa_test::check(std::filesystem::is_directory(dir), "ensure dir");
  check_ensure_once(dir);
  check_ensure_vs_include(dir);
  check_ensure_across(dir);
  check_ensure_retry(dir);
  check_ensure_throw_once(dir);
  check_ensure_self(dir);
  check_ensure_mutual(dir);
  check_consult_reloads(dir);
  check_consult_then_ensure(dir);
  check_ensure_init_order(dir);
  check_ensure_order(dir);
  check_ensure_apart(dir);
  check_ensure_relative(dir);
  check_ensure_args(dir);
  check_ensure_operator(dir);
  check_ensure_broken(dir);
  check_ensure_japanese(dir);
  check_ensure_suffix(dir);
  check_ensure_goal();
  check_ensure_fact();
  return mosa_test::failures();
}
