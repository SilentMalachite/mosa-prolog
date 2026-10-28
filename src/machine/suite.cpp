#include "mosa/suite.hpp"
#include "mosa/path_text.hpp"

#include "internal.hpp"
#include "mosa/machine.hpp"
#include "mosa/reader.hpp"
#include "mosa/syntax_error.hpp"
#include "mosa/unify.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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

bool load_file_text(Machine& machine, std::string source, std::string_view directory, std::string_view origin,
                    std::vector<Exam>& exams, std::string& error) {
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
      // 断片はファイルそのものではない。相対の include だけ、このファイルの隣を見る。
      if (!load_source(engine_impl(machine), text, directory).ok) {
        error = std::string{machine.diagnostic()};
        return false;
      }
    }
  }
  if (!program.empty() && !load_source(engine_impl(machine), program, directory, origin).ok) {
    error = std::string{machine.diagnostic()};
    return false;
  }
  return true;
}

bool atom_named(const Heap& heap, Cell cell, std::string_view name) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Atom && heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == name;
}

bool numeric_cell(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox || cell.tag() == Tag::Float;
}

Cell negate_number(Machine& machine, Cell cell) {
  cell = machine.heap().deref(cell);
  if (cell.tag() == Tag::Float) return machine.heap().floating(-machine.heap().float_value(cell));
  const auto value = machine.heap().integer_value(cell);
  if (value == std::numeric_limits<std::int64_t>::min()) return cell;
  return machine.heap().integer(-value);
}

// 期待の `-1` は前置演算子で読まれる。エラーの項は整数なので、数の符号だけ畳む。
Cell fold_minus(Machine& machine, Cell cell) {
  Heap& heap = machine.heap();
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Lis) {
    return heap.list(fold_minus(machine, heap.list_head(cell)), fold_minus(machine, heap.list_tail(cell)));
  }
  if (cell.tag() != Tag::Str) return cell;
  const auto arity = heap.functor_arity(cell);
  std::vector<Cell> args;
  args.reserve(arity);
  for (std::uint16_t index = 0; index < arity; ++index) args.push_back(fold_minus(machine, heap.arg(cell, index)));
  if (arity == 1 && machine.atoms().name(heap.functor_atom(cell)) == "-" && numeric_cell(heap, args[0])) {
    return negate_number(machine, args[0]);
  }
  return heap.structure(heap.functor_atom(cell), args);
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

// next はヒープを選択点まで戻す。期待項は query のあとで同じヒープに読むので、組は先に文字列にする。
bool freeze_answers(const Machine& machine, const std::vector<Cell>& items, const std::vector<VarName>& names,
                    std::vector<std::vector<std::pair<std::string, std::string>>>& rows) {
  rows.clear();
  rows.reserve(items.size());
  for (const Cell item : items) {
    std::vector<std::pair<std::string, std::string>> pairs;
    binding_pairs(machine.heap(), item, pairs, names, machine.operators());
    // 組が無い答えは、切れた項を成功と見なさない。
    if (pairs.empty()) return false;
    rows.push_back(std::move(pairs));
  }
  return true;
}

bool saved_matches(const Machine& machine, const Stop& stop,
                   const std::vector<std::pair<std::string, std::string>>& pairs) {
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
    return unify_formal(machine, actual, fold_minus(machine, machine.heap().arg(expect, 0)));
  }
  if (expect.tag() == Tag::Str && machine.atoms().name(machine.heap().functor_atom(expect)) == "solutions") {
    const auto wanted = list_items(machine.heap(), machine.heap().arg(expect, 0));
    std::vector<std::vector<std::pair<std::string, std::string>>> rows;
    if (!freeze_answers(machine, wanted, outcome.term->names, rows)) return false;
    Stop current = stop;
    for (std::size_t index = 0; index < rows.size(); ++index) {
      if (index > 0) current = machine.next();
      if (current.kind != StopKind::Success) return false;
      if (!saved_matches(machine, current, rows[index])) return false;
    }
    return true;
  }
  error = exam.expect;
  return false;
}

int run_one(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::cerr << utf8_from_path(path) << ": ファイルを開けません\n";
    return 1;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  auto machine = Machine::create();
  std::vector<Exam> exams;
  std::string error;
  if (!load_file_text(*machine, buffer.str(), utf8_from_path(path.parent_path()), utf8_from_path(path), exams, error)) {
    std::cerr << utf8_from_path(path) << ": " << error << '\n';
    return 1;
  }
  int failures = 0;
  for (const Exam& exam : exams) {
    if (!run_exam(*machine, exam, error)) {
      std::cerr << utf8_from_path(path) << ": " << exam.goal << '\n';
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
