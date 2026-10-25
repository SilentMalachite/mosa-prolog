#include "mosa/suite.hpp"

#include "mosa/machine.hpp"
#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/unify.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace mosa {
namespace {

struct Exam {
  std::string goal;
  std::string expect;
};

bool is_test_fact(const Heap& heap, Cell term) {
  term = heap.deref(term);
  return term.tag() == Tag::Str && heap.functor_arity(term) == 3 &&
         heap.atoms().name(heap.functor_atom(term)) == "test";
}

std::string written(const Heap& heap, const OperatorTable& ops, Cell term, const std::vector<VarName>& names) {
  return write_term(heap, ops, term, {}, names);
}

bool load_file_text(Machine& machine, std::string source, std::vector<Exam>& exams, std::string& error) {
  std::string program;
  Reader reader(source, machine.heap(), machine.operators());
  while (true) {
    const ReadOutcome outcome = reader.next();
    if (outcome.failure) {
      error = "項が閉じていません";
      if (outcome.failure->reason != SyntaxReason::IncompleteTerm) error = "予期しないトークン";
      return false;
    }
    if (!outcome.term) break;
    const Cell term = machine.heap().deref(outcome.term->term);
    if (is_test_fact(machine.heap(), term)) {
      Exam exam;
      exam.goal = written(machine.heap(), machine.operators(), machine.heap().arg(term, 1), outcome.term->names);
      exam.expect = written(machine.heap(), machine.operators(), machine.heap().arg(term, 2), outcome.term->names);
      exams.push_back(std::move(exam));
      continue;
    }
    const std::string text = written(machine.heap(), machine.operators(), term, outcome.term->names) + ".\n";
    program += text;
    if (term.tag() == Tag::Str && machine.heap().functor_arity(term) == 1 &&
        machine.atoms().name(machine.heap().functor_atom(term)) == ":-") {
      // 後続の項を読む前に演算子を表へ入れる。節の蓄積は最後の load に任せる。
      if (!machine.load_program(text).ok) {
        error = std::string{machine.diagnostic()};
        return false;
      }
    }
  }
  if (!program.empty() && !machine.load_program(program).ok) {
    error = std::string{machine.diagnostic()};
    return false;
  }
  return true;
}

bool atom_named(const Heap& heap, Cell cell, std::string_view name) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Atom && heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == name;
}

bool unify_formal(Machine& machine, Cell actual, Cell expected) {
  auto saved = machine.heap().size();
  // 公開の query を介さず、既存の単一化は機械の外では印付きで呼ぶ。
  Trail trail;
  const bool ok = unify(machine.heap(), trail, trail.slots.size(), machine.heap().size(), actual, expected);
  if (!ok) machine.heap().truncate(saved);
  return ok;
}

std::vector<Cell> list_items(const Heap& heap, Cell list) {
  std::vector<Cell> items;
  while (true) {
    list = heap.deref(list);
    if (list.tag() != Tag::Lis) break;
    items.push_back(heap.list_head(list));
    list = heap.list_tail(list);
  }
  return items;
}

void binding_pairs(const Heap& heap, Cell term, std::vector<std::pair<std::string, std::string>>& pairs,
                   const std::vector<VarName>& names, const OperatorTable& ops) {
  term = heap.deref(term);
  if (term.tag() == Tag::Str && heap.functor_arity(term) == 2 &&
      heap.atoms().name(heap.functor_atom(term)) == ",") {
    binding_pairs(heap, heap.arg(term, 0), pairs, names, ops);
    binding_pairs(heap, heap.arg(term, 1), pairs, names, ops);
    return;
  }
  if (term.tag() == Tag::Str && heap.functor_arity(term) == 2 &&
      heap.atoms().name(heap.functor_atom(term)) == "=") {
    std::string name;
    const Cell left = heap.arg(term, 0);
    for (const VarName& var : names) {
      if (var.cell.raw() == left.raw() || heap.deref(var.cell).raw() == heap.deref(left).raw()) name = var.name;
    }
    if (name.empty() && left.tag() == Tag::Ref) name = "V";
    pairs.emplace_back(name, write_term(heap, ops, heap.arg(term, 1), {}, names));
  }
}

std::vector<VarName> stop_names(const Stop& stop) {
  std::vector<VarName> names;
  for (const Binding& binding : stop.bindings) names.push_back(VarName{binding.name, binding.variable});
  return names;
}

bool solution_matches(const Machine& machine, const Stop& stop, Cell solution, const std::vector<VarName>& names) {
  std::vector<std::pair<std::string, std::string>> pairs;
  binding_pairs(machine.heap(), solution, pairs, names, machine.operators());
  for (const auto& pair : pairs) {
    const Binding* found = nullptr;
    for (const Binding& binding : stop.bindings) {
      if (binding.name == pair.first) found = &binding;
    }
    if (found == nullptr) return false;
    const auto text =
        write_term(machine.heap(), machine.operators(), machine.heap().deref(found->variable), {}, stop_names(stop));
    if (text != pair.second) return false;
  }
  return true;
}

bool run_exam(Machine& machine, const Exam& exam, std::string& error) {
  const std::string goal_text = exam.goal + ".";
  Stop stop = machine.query(goal_text);
  const std::string expect_text = exam.expect + ".";
  Reader expect_reader(expect_text, machine.heap(), machine.operators());
  const ReadOutcome outcome = expect_reader.next();
  if (!outcome.term) {
    error = exam.goal;
    return false;
  }
  const Cell expect = machine.heap().deref(outcome.term->term);
  if (atom_named(machine.heap(), expect, "true")) return stop.kind == StopKind::Success;
  if (atom_named(machine.heap(), expect, "fail")) return stop.kind == StopKind::Failure;
  if (expect.tag() == Tag::Str && machine.atoms().name(machine.heap().functor_atom(expect)) == "error" &&
      machine.heap().functor_arity(expect) == 1) {
    if (stop.kind != StopKind::Exception) return false;
    Cell actual = machine.heap().deref(stop.exception);
    if (actual.tag() == Tag::Str && machine.heap().functor_arity(actual) == 2 &&
        machine.atoms().name(machine.heap().functor_atom(actual)) == "error") {
      actual = machine.heap().arg(actual, 0);
    }
    return unify_formal(machine, actual, machine.heap().arg(expect, 0));
  }
  if (expect.tag() == Tag::Str && machine.atoms().name(machine.heap().functor_atom(expect)) == "solutions") {
    const auto wanted = list_items(machine.heap(), machine.heap().arg(expect, 0));
    Stop current = stop;
    for (std::size_t index = 0; index < wanted.size(); ++index) {
      if (index > 0) current = machine.next();
      if (current.kind != StopKind::Success) return false;
      if (!solution_matches(machine, current, wanted[index], outcome.term->names)) return false;
    }
    return true;
  }
  error = exam.expect;
  return false;
}

int run_one(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::cerr << path.string() << ": ファイルを開けません\n";
    return 1;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  auto machine = Machine::create();
  std::vector<Exam> exams;
  std::string error;
  if (!load_file_text(*machine, buffer.str(), exams, error)) {
    std::cerr << path.string() << ": " << error << '\n';
    return 1;
  }
  int failures = 0;
  for (const Exam& exam : exams) {
    if (!run_exam(*machine, exam, error)) {
      std::cerr << path.string() << ": " << exam.goal << '\n';
      ++failures;
    }
  }
  return failures == 0 ? 0 : 1;
}

}  // namespace

int run_suite(std::string_view path) {
  const std::filesystem::path file{std::string{path}};
  if (std::filesystem::is_directory(file)) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(file)) {
      if (!entry.is_regular_file()) continue;
      if (entry.path().extension() != ".pl") continue;
      if (entry.path().filename() == "syntax.pl") continue;
      files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const auto& item : files) {
      if (run_one(item) != 0) return 1;
    }
    return 0;
  }
  return run_one(file);
}

}
