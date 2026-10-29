#include "mosa/suite.hpp"
#include "mosa/path_text.hpp"

#include "internal.hpp"
#include "test_file.hpp"
#include "mosa/machine.hpp"
#include "mosa/reader.hpp"
#include "mosa/unify.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <array>
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

}  // namespace

bool is_test_fact(const Heap& heap, Cell term) {
  term = heap.deref(term);
  return term.tag() == Tag::Str && heap.functor_arity(term) == 3 &&
         heap.atoms().name(heap.functor_atom(term)) == "test";
}

namespace {

std::string written(const Heap& heap, const OperatorTable& ops, Cell term, const std::vector<VarName>& names) {
  return write_term(heap, ops, term, {}, names);
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

bool occurs(const Heap& heap, Cell term, Cell variable) {
  term = heap.deref(term);
  if (term.tag() == Tag::Ref) return term.raw() == heap.deref(variable).raw();
  if (term.tag() == Tag::Lis) {
    return occurs(heap, heap.list_head(term), variable) || occurs(heap, heap.list_tail(term), variable);
  }
  if (term.tag() != Tag::Str) return false;
  for (std::uint16_t index = 0; index < heap.functor_arity(term); ++index) {
    if (occurs(heap, heap.arg(term, index), variable)) return true;
  }
  return false;
}

// 差分の節の本体は演算子に頼らず書く。読む側の表が op/3 で変わっていても、同じ項に読み戻せる。
// 1 度だけ出る名前の無い変数は _ にする。_G1 と書くと節の中で 1 度しか出ない名前の付いた変数になる。
WriteOptions canonical_options() {
  WriteOptions options;
  options.ignore_ops = true;
  options.underscore_once = true;
  return options;
}

Exam make_exam(const Machine& machine, Cell term, const std::vector<VarName>& names) {
  const Heap& heap = machine.heap();
  const Cell goal = heap.arg(term, 1);
  const Cell expect = heap.deref(heap.arg(term, 2));
  Exam exam;
  exam.name = written(heap, machine.operators(), heap.arg(term, 0), names);
  exam.goal = written(heap, machine.operators(), goal, names);
  exam.expect = written(heap, machine.operators(), expect, names);
  exam.canonical = write_term(heap, machine.operators(), goal, canonical_options(), names);
  for (const VarName& name : names) {
    if (occurs(heap, goal, name.cell)) exam.names.push_back(name.name);
  }
  if (expect.tag() == Tag::Str && heap.functor_arity(expect) == 1 &&
      heap.atoms().name(heap.functor_atom(expect)) == "solutions") {
    exam.solutions = std::max<std::size_t>(1, list_items(heap, heap.arg(expect, 0)).size());
  }
  return exam;
}

}  // namespace

bool read_test_file(Machine& machine, std::string_view source, std::string_view directory, std::string_view origin,
                    std::vector<Exam>& exams, std::string& error) {
  Impl& impl = engine_impl(machine);
  struct Unhook {
    Impl& impl;
    ~Unhook() { impl.term_hook = nullptr; }
  } unhook{impl};
  // test/3 だけを横取りし、ほかの項は読み込み器がその場で処理する。include した先の test/3 は節のまま残す。
  impl.term_hook = [&](const ReadTerm& read, std::string_view at) {
    if (at != origin) return false;
    const Cell term = machine.heap().deref(read.term);
    if (!is_test_fact(machine.heap(), term)) return false;
    exams.push_back(make_exam(machine, term, read.names));
    return true;
  };
  if (load_source(impl, source, directory, origin).ok) return true;
  error = std::string{machine.diagnostic()};
  return false;
}

std::vector<std::filesystem::path> test_files(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (!entry.is_regular_file()) continue;
    if (entry.path().extension() != ".pl") continue;
    if (entry.path().filename() == "syntax.pl") continue;
    files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());
  return files;
}

namespace {

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
    std::cerr << render_message_fresh(default_message_language(), "cannot_open_file", std::array{utf8_from_path(path)})
              << '\n';
    return 1;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  auto machine = Machine::create();
  std::vector<Exam> exams;
  std::string error;
  const std::string source = buffer.str();
  if (!read_test_file(*machine, source, utf8_from_path(path.parent_path()), utf8_from_path(path), exams, error)) {
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
  const std::filesystem::path file = path_from_utf8(path);
  if (std::filesystem::is_directory(file)) {
    // 落ちたファイルで止めない。後ろのファイルの失敗が隠れ、直す前に失敗を見られなくなる。
    int code = 0;
    for (const auto& item : test_files(file)) {
      if (run_one(item) != 0) code = 1;
    }
    return code;
  }
  return run_one(file);
}

}
