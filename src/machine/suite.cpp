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
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
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
  // 問い合わせはこの字面を読み直す。_ を _G1 と書くと名前付きの変数になって答えの名前の表に入り、作る名前がそれを
  // 飛ばすので（レビュー M9）、答えの中の新しい変数が期待の字面と違う番号になる。読んだ節の名前の無い変数は 1 度しか出ない。
  WriteOptions source;
  source.underscore_once = true;
  exam.goal = write_term(heap, machine.operators(), goal, source, names);
  exam.expect = written(heap, machine.operators(), expect, names);
  exam.canonical = write_term(heap, machine.operators(), goal, canonical_options(), names);
  for (const VarName& name : names) {
    if (occurs(heap, goal, name.cell)) exam.names.push_back(name.name);
  }
  if (expect.tag() == Tag::Str && heap.functor_arity(expect) == 1 &&
      heap.atoms().name(heap.functor_atom(expect)) == "solutions") {
    exam.solutions = std::max<std::size_t>(1, list_items(heap, heap.arg(expect, 0)).size());
    exam.enumerate = true;
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

// 例外の版は読めないフォルダで filesystem_error を投げ、mosatest が落ちる（Muse のレビューの低）。
// 一覧を取れなければ nullopt を返し、呼び手が失敗にする。種類の分からない項目は流せないので飛ばす。
std::optional<std::vector<std::filesystem::path>> test_files(const std::filesystem::path& dir) {
  std::vector<std::filesystem::path> files;
  std::error_code error;
  std::filesystem::directory_iterator at(dir, error);
  for (; !error && at != std::filesystem::directory_iterator{}; at.increment(error)) {
    std::error_code kind;
    if (!at->is_regular_file(kind) || kind) continue;
    const std::filesystem::path& path = at->path();
    if (path.extension() != ".pl" || path.filename() == "syntax.pl") continue;
    files.push_back(path);
  }
  if (error) return std::nullopt;
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
  return is_integer_tag(cell.tag()) || cell.tag() == Tag::Float;
}

// -(-2^63) は Big の 2^63 に、-(2^63) は IntBox の -2^63 になる。int64 では反転できないので BigInt で反転し、
// heap.big の正規形に戻す（ADR 0025）。エラーの項の数と同じ Cell になり、単一化で合う。
Cell negate_number(Machine& machine, Cell cell) {
  Heap& heap = machine.heap();
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Float) return heap.floating(-heap.float_value(cell));
  return heap.big(-heap.big_value(cell));
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

// 期待の (Name = Value, ...) から名前と値の組を拾う。値はまだ書かない（freeze_answers が 1 つの番号付けで書く）。
void binding_pairs(const Heap& heap, Cell term, std::vector<std::pair<std::string, Cell>>& pairs,
                   const std::vector<VarName>& names) {
  term = heap.deref(term);
  if (term.tag() == Tag::Str && heap.functor_arity(term) == 2 &&
      heap.atoms().name(heap.functor_atom(term)) == ",") {
    binding_pairs(heap, heap.arg(term, 0), pairs, names);
    binding_pairs(heap, heap.arg(term, 1), pairs, names);
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
    pairs.emplace_back(name, heap.arg(term, 1));
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
    std::vector<std::pair<std::string, Cell>> found;
    binding_pairs(machine.heap(), item, found, names);
    // 組が無い答えは、切れた項を成功と見なさない。
    if (found.empty()) return false;
    std::vector<Cell> values;
    values.reserve(found.size());
    for (const auto& pair : found) values.push_back(pair.second);
    // 組ごとに書くと、作る名前が組ごとに _G1 から始まり、別の変数と共有した変数を区別できない（レビュー M22）。
    // saved_matches も同じ順に 1 つの番号付けで書く。
    std::vector<std::string> texts = write_terms(machine.heap(), machine.operators(), values, {}, names);
    std::vector<std::pair<std::string, std::string>> pairs;
    pairs.reserve(found.size());
    for (std::size_t at = 0; at < found.size(); ++at) pairs.emplace_back(found[at].first, std::move(texts[at]));
    rows.push_back(std::move(pairs));
  }
  return true;
}

bool saved_matches(const Machine& machine, const Stop& stop,
                   const std::vector<std::pair<std::string, std::string>>& pairs) {
  std::vector<Cell> values;
  values.reserve(pairs.size());
  for (const auto& pair : pairs) {
    const Binding* found = nullptr;
    for (const Binding& binding : stop.bindings) {
      if (binding.name == pair.first) found = &binding;
    }
    if (found == nullptr) return false;
    values.push_back(machine.heap().deref(found->variable));
  }
  const std::vector<std::string> texts =
      write_terms(machine.heap(), machine.operators(), values, {}, stop_names(stop));
  for (std::size_t at = 0; at < pairs.size(); ++at) {
    if (texts[at] != pairs[at].second) return false;
  }
  return true;
}

bool run_exam(Machine& machine, const Exam& exam, std::string& error) {
  // ゴールは読み込んだ項を書いた字面なので、前のテストが char_conversion を有効にしたままでも変換なしで読む（レビュー L17）。
  Stop stop = query_written(machine, exam.goal + full_stop_after(exam.goal));
  const std::string expect_text = exam.expect + full_stop_after(exam.expect);
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
    // 並べた解の後に解が残っていれば一致しない。空のリストはゴールが失敗するときだけ通る。
    const Stop rest = rows.empty() ? stop : machine.next();
    return rest.kind == StopKind::Failure;
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
  std::error_code error;
  if (!std::filesystem::is_directory(file, error)) return run_one(file);
  const auto files = test_files(file);
  if (!files) {
    std::cerr << render_message_fresh(default_message_language(), "cannot_open_file", std::array{utf8_from_path(file)})
              << '\n';
    return 1;
  }
  // 落ちたファイルで止めない。後ろのファイルの失敗が隠れ、直す前に失敗を見られなくなる。
  int code = 0;
  for (const auto& item : *files) {
    if (run_one(item) != 0) code = 1;
  }
  return code;
}

}
