// 7.4.2 [File]。指示だけ。リストの各アトムを consult と同じ手続きで読む。
// 節はテキストに継がない。開けないファイルの診断は「ファイルを開けません」。
// 一時ファイルは tests/tmp/files1。

#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

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

std::string file_list(const std::vector<std::filesystem::path>& paths) {
  std::string out = ":- [";
  for (std::size_t index = 0; index < paths.size(); ++index) {
    if (index != 0) out += ", ";
    out += prolog_path(paths[index]);
  }
  out += "].";
  return out;
}

std::string ensure_of(const std::filesystem::path& path) {
  return ":- ensure_loaded(" + prolog_path(path) + ").";
}

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

void check_file_consult(const std::filesystem::path& dir) {
  const auto first = dir / "first.pl";
  const auto second = dir / "second.pl";
  mosa_test::check(write_text(first, "q(1).\np(1).\n"), "write first");
  mosa_test::check(write_text(second, "p(2).\n"), "write second");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(file_list({first, second})).ok, "list load");
  mosa_test::check(answers(*machine, "p(X).") == "2", "later file replaces");
  mosa_test::check(answers(*machine, "q(X).") == "1", "earlier predicate stays");

  auto query_dir = mosa::Machine::create();
  mosa_test::check(query_dir->load_program("?- [" + prolog_path(second) + "].").ok, "query directive");
  mosa_test::check(answers(*query_dir, "p(X).") == "2", "query directive clause");
}

void check_file_not_spliced(const std::filesystem::path& dir) {
  const auto gap = dir / "gap.pl";
  const auto replaced = dir / "replaced.pl";
  mosa_test::check(write_text(gap, "q(1).\n"), "write gap");
  mosa_test::check(write_text(replaced, "p(9).\n"), "write replaced");
  auto joined = mosa::Machine::create();
  mosa_test::check(joined->load_program("p(1). " + file_list({gap}) + " p(2).").ok, "gap load");
  mosa_test::check(answers(*joined, "p(X).") == "1,2", "consult is not a gap");
  mosa_test::check(joined->query("q(1).").kind == mosa::StopKind::Success, "consulted neighbor");

  auto middle = mosa::Machine::create();
  mosa_test::check(middle->load_program("p(1). " + file_list({replaced}) + " p(2).").ok, "middle load");
  mosa_test::check(answers(*middle, "p(X).") == "9,2", "consult replaces then parent appends");

  auto only = mosa::Machine::create();
  mosa_test::check(only->load_program("p(1). " + file_list({replaced})).ok, "replace load");
  mosa_test::check(answers(*only, "p(X).") == "9", "consult replaces earlier clause");

  auto after = mosa::Machine::create();
  mosa_test::check(after->load_program(file_list({replaced}) + " p(2).").ok, "after load");
  mosa_test::check(answers(*after, "p(X).") == "2", "parent clause replaces consulted");
}

void check_file_reloads(const std::filesystem::path& dir) {
  const auto child = dir / "bump.pl";
  mosa_test::check(write_text(child, ":- initialization(assertz(s(1))).\n"), "write bump");
  auto twice = mosa::Machine::create();
  const auto text = file_list({child});
  mosa_test::check(twice->load_program(text + " " + text).ok, "twice load");
  mosa_test::check(answers(*twice, "s(X).") == "1,1", "consult runs twice");

  auto ensured = mosa::Machine::create();
  mosa_test::check(ensured->load_program(text + " " + ensure_of(child)).ok, "ensure after list");
  mosa_test::check(answers(*ensured, "s(X).") == "1", "ensure does not repeat");

  const auto body = dir / "body.pl";
  mosa_test::check(write_text(body, "p(1).\n"), "write body 1");
  auto reload = mosa::Machine::create();
  mosa_test::check(reload->load_program(file_list({body})).ok, "reload first");
  mosa_test::check(write_text(body, "p(2).\n"), "write body 2");
  mosa_test::check(reload->load_program(file_list({body})).ok, "reload second");
  mosa_test::check(answers(*reload, "p(X).") == "2", "second list reads again");
}

void check_file_throw(const std::filesystem::path& dir) {
  const auto child = dir / "thrown.pl";
  mosa_test::check(write_text(child, "kept(1).\n:- initialization(throw(1)).\n"), "write thrown");
  auto machine = mosa::Machine::create();
  const auto status = machine->load_program(file_list({child}));
  const auto shown = std::filesystem::path{child.generic_string()}.string();
  mosa_test::check(!status.ok && machine->diagnostic() == shown + ": 1", "init throw");
  mosa_test::check(answers(*machine, "kept(X).") == "1", "throw keeps clause");
}

void check_file_init_early(const std::filesystem::path& dir) {
  const auto child = dir / "early.pl";
  mosa_test::check(write_text(child, "kept(1).\n:- initialization(needs_later).\n"), "write early");
  auto machine = mosa::Machine::create();
  const auto status = machine->load_program(":- dynamic(s/1).\n" + file_list({child}) +
                                            "\nneeds_later :- assertz(s(1)).\n"
                                            ":- initialization(assertz(s(4))).\n");
  mosa_test::check(!status.ok, "early init fails");
  mosa_test::check(machine->diagnostic().find("existence_error(procedure, needs_later/0)") != std::string::npos,
                   "early calls missing");
  mosa_test::check(machine->query("kept(1).").kind == mosa::StopKind::Success, "early keeps clause");
  mosa_test::check(machine->query("current_predicate(needs_later/1).").kind == mosa::StopKind::Failure,
                   "later clause not defined");
  mosa_test::check(machine->query("s(1).").kind == mosa::StopKind::Failure, "later goal did not run");
  mosa_test::check(machine->query("s(4).").kind == mosa::StopKind::Failure, "later init skipped");
}

void check_file_missing(const std::filesystem::path& dir) {
  const auto good = dir / "good.pl";
  const auto missing = dir / "missing.pl";
  mosa_test::check(write_text(good, "p(1).\n"), "write good");
  auto machine = mosa::Machine::create();
  const auto status = machine->load_program("q(1). " + file_list({good, missing}));
  const auto shown = std::filesystem::path{missing.generic_string()}.string();
  mosa_test::check(!status.ok && machine->diagnostic() == shown + ": ファイルを開けません", "missing phrase");
  mosa_test::check(machine->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "missing rolls back");
  mosa_test::check(machine->query("current_predicate(p/1).").kind == mosa::StopKind::Failure, "earlier file rolls back");

  const auto repl = dir / "repl.pl";
  mosa_test::check(write_text(repl, "p(2).\n"), "write repl");
  auto prior = mosa::Machine::create();
  mosa_test::check(prior->load_program("p(1).").ok, "prior clause");
  const auto again = prior->load_program(file_list({repl, missing}));
  mosa_test::check(!again.ok, "replace then missing fails");
  mosa_test::check(answers(*prior, "p(X).") == "1", "failed list restores clause");
}

void check_file_args(const std::filesystem::path& dir) {
  auto number = mosa::Machine::create();
  const auto bad = number->load_program("q(1). :- [1].");
  mosa_test::check(!bad.ok && number->diagnostic() == "type_error(atom, 1)", "number formal");
  mosa_test::check(number->query("current_predicate(q/1).").kind == mosa::StopKind::Failure, "number rolls back");

  auto open = mosa::Machine::create();
  const auto var = open->load_program(":- [X].");
  mosa_test::check(!var.ok && open->diagnostic() == "instantiation_error", "var formal");

  auto partial = mosa::Machine::create();
  const auto tail = partial->load_program(":- [a|X].");
  mosa_test::check(!tail.ok && partial->diagnostic() == "instantiation_error", "partial formal");

  auto typed = mosa::Machine::create();
  const auto mixed = typed->load_program(":- [1, X].");
  mosa_test::check(!mixed.ok && typed->diagnostic() == "instantiation_error", "var before type");

  auto improper = mosa::Machine::create();
  const auto tail_atom = improper->load_program(":- [a|1].");
  mosa_test::check(!tail_atom.ok && improper->diagnostic() == "type_error(list, [a|1])", "improper list");

  auto bare = mosa::Machine::create();
  const auto other = bare->load_program(":- X.");
  mosa_test::check(!other.ok && bare->diagnostic().find("この指示はフェーズ2では使えない") != std::string_view::npos,
                   "bare variable is not a list");

  auto empty = mosa::Machine::create();
  mosa_test::check(empty->load_program(":- []. p(1).").ok, "empty list");
  mosa_test::check(answers(*empty, "p(X).") == "1", "empty list loads nothing");

  auto nil = mosa::Machine::create();
  const auto named = nil->load_program(":- [[]].");
  mosa_test::check(!named.ok && nil->diagnostic() == "[]: ファイルを開けません", "empty atom is a file");

  const auto good = dir / "typed.pl";
  mosa_test::check(write_text(good, "p(1).\n"), "write typed");
  auto before = mosa::Machine::create();
  const auto mixed_files = before->load_program(":- [" + prolog_path(good) + ", 1].");
  mosa_test::check(!mixed_files.ok && before->diagnostic() == "type_error(atom, 1)", "type before open");
  mosa_test::check(before->query("current_predicate(p/1).").kind == mosa::StopKind::Failure, "type does not consult");
}

void check_file_relative(const std::filesystem::path& dir) {
  const auto first = dir / "rel_a.pl";
  const auto second = dir / "rel_b.pl";
  const auto rel = dir / "rel.pl";
  mosa_test::check(write_text(first, "p(1).\n"), "write rel a");
  mosa_test::check(write_text(second, "p(2).\n"), "write rel b");
  mosa_test::check(write_text(rel, ":- ['rel_a.pl', 'rel_b.pl'].\n"), "write rel");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(rel.generic_string()), "relative consult");
  mosa_test::check(answers(*machine, "p(X).") == "2", "relative order");
}

void check_file_cycle(const std::filesystem::path& dir) {
  const auto self = dir / "self.pl";
  mosa_test::check(write_text(self, "p(1).\n:- ['self.pl'].\n"), "write self");
  auto machine = mosa::Machine::create();
  const auto status = machine->load_program(file_list({self}));
  mosa_test::check(!status.ok, "cycle fails");
  mosa_test::check(machine->diagnostic().find("ファイルが循環しています") != std::string_view::npos, "cycle phrase");
  mosa_test::check(machine->query("current_predicate(p/1).").kind == mosa::StopKind::Failure, "cycle rolls back");
}

void check_file_operator(const std::filesystem::path& dir) {
  const auto op = dir / "op.pl";
  mosa_test::check(write_text(op, ":- op(500, xfx, は).\n"), "write op");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(file_list({op}) + "\na は b.\n").ok, "op list");
  mosa_test::check(machine->query("は(a, b).").kind == mosa::StopKind::Success, "op applies after list");

  const auto missing = dir / "op_missing.pl";
  auto failed = mosa::Machine::create();
  const auto status = failed->load_program(file_list({op, missing}));
  mosa_test::check(!status.ok, "op then missing");
  mosa_test::check(!failed->load_program("a は b.").ok, "op rolled back");
}

void check_file_syntax_restores(const std::filesystem::path& dir) {
  const auto child = dir / "syntax_child.pl";
  mosa_test::check(write_text(child, "p(2).\nr(3).\n"), "write syntax child");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program("p(1).").ok, "prior clause");
  const auto status = machine->load_program(file_list({child}) + " q(.");
  mosa_test::check(!status.ok, "syntax after list");
  mosa_test::check(answers(*machine, "p(X).") == "1", "syntax restores replaced");
  mosa_test::check(machine->query("current_predicate(r/1).").kind == mosa::StopKind::Failure,
                   "syntax drops consulted");
  mosa_test::check(machine->load_program(ensure_of(child)).ok, "ensure reads again");
  mosa_test::check(answers(*machine, "p(X).") == "2", "rolled path is not prepared");
  mosa_test::check(answers(*machine, "r(X).") == "3", "consulted predicate returns");
}

void check_file_kept_does_not_stick(const std::filesystem::path& dir) {
  const auto inner = dir / "kept_inner.pl";
  const auto outer = dir / "kept_outer.pl";
  mosa_test::check(write_text(inner, "k(1).\n:- initialization(throw(x)).\n"), "write kept inner");
  mosa_test::check(write_text(outer, ":- initialization(consult(" + prolog_path(inner) + ")).\n"),
                   "write kept outer");
  auto kept = mosa::Machine::create();
  mosa_test::check(kept->load_program(file_list({outer})).ok, "nested failure is not an error");
  mosa_test::check(answers(*kept, "k(X).") == "1", "inner throw still keeps");

  auto rolled = mosa::Machine::create();
  mosa_test::check(rolled->load_program("old(1).").ok, "prior old");
  const auto status = rolled->load_program("p(1). " + file_list({outer}) + " q(.");
  mosa_test::check(!status.ok, "syntax after nested throw");
  mosa_test::check(answers(*rolled, "old(X).") == "1", "earlier load stays");
  mosa_test::check(rolled->query("current_predicate(p/1).").kind == mosa::StopKind::Failure,
                   "parent clause rolls back");
  mosa_test::check(rolled->query("current_predicate(k/1).").kind == mosa::StopKind::Failure,
                   "nested clause rolls back");
}

void check_file_japanese(const std::filesystem::path& dir) {
  const auto ja = dir / "節.pl";
  mosa_test::check(write_text(ja, "親(2).\n連(1).\n"), "write ja");
  auto machine = mosa::Machine::create();
  const auto text = file_list({ja});
  mosa_test::check(machine->load_program(text + " " + text).ok, "ja list");
  mosa_test::check(answers(*machine, "親(X).") == "2", "ja replaces");
  mosa_test::check(answers(*machine, "連(X).") == "1", "ja does not double");
}

}  // namespace

int main() {
  const std::filesystem::path dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "files1";
  std::filesystem::create_directories(dir);
  mosa_test::check(std::filesystem::is_directory(dir), "files dir");
  check_file_consult(dir);
  check_file_not_spliced(dir);
  check_file_reloads(dir);
  check_file_throw(dir);
  check_file_init_early(dir);
  check_file_missing(dir);
  check_file_args(dir);
  check_file_relative(dir);
  check_file_cycle(dir);
  check_file_operator(dir);
  check_file_syntax_restores(dir);
  check_file_kept_does_not_stick(dir);
  check_file_japanese(dir);
  return mosa_test::failures();
}
