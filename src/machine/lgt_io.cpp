// ^^ は利用者述語にしない。未実装は名前とアリティで不合格にする。
// 一時ファイルは tests/tmp/suite だけ。tests.lgt が終わったら消す。

#include "lgt_io.hpp"

#include "lgt_priv.hpp"

#include "internal.hpp"

#include "mosa/reader.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace mosa {
namespace {

std::string_view atom_text(const Heap& heap, Cell cell) {
  return heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3));
}

bool functor_is(const Heap& heap, Cell cell, std::string_view name, std::uint16_t arity) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Str && heap.functor_arity(cell) == arity &&
         heap.atoms().name(heap.functor_atom(cell)) == name;
}

std::filesystem::path path_from_utf8(std::string_view text) {
  std::u8string bytes;
  bytes.reserve(text.size());
  for (unsigned char byte : text) bytes.push_back(static_cast<char8_t>(byte));
  return std::filesystem::path{bytes};
}

std::string utf8_of(const std::filesystem::path& path) {
  const auto text = path.generic_u8string();
  std::string bytes;
  bytes.reserve(text.size());
  for (char8_t unit : text) bytes.push_back(static_cast<char>(unit));
  return bytes;
}

bool write_bytes(const std::filesystem::path& path, std::string_view bytes) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return false;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!bytes.empty()) output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(output);
}

std::optional<std::string> read_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::nullopt;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::filesystem::path tests_root(const std::filesystem::path& file) {
  auto cursor = file.parent_path();
  while (!cursor.empty()) {
    if (cursor.filename() == "tests") return cursor;
    const auto parent = cursor.parent_path();
    if (parent == cursor) break;
    cursor = parent;
  }
  return {};
}

void remove_empty_up(const std::filesystem::path& start, const std::filesystem::path& stop) {
  std::error_code error;
  auto dir = start;
  while (!dir.empty()) {
    if (!std::filesystem::exists(dir, error) || error) return;
    if (!std::filesystem::is_empty(dir, error) || error) return;
    std::filesystem::remove(dir, error);
    if (error || dir == stop) return;
    const auto parent = dir.parent_path();
    if (parent == dir) return;
    dir = parent;
  }
}

std::string file_text(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return {};
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

struct PrologLoad {
  std::uint64_t variable = 0;
};

void scan_goal(const Heap& heap, Cell cell, std::vector<PrologLoad>& loads,
               std::vector<std::pair<std::uint64_t, std::string>>& names) {
  cell = heap.deref(cell);
  if (functor_is(heap, cell, ",", 2)) {
    scan_goal(heap, heap.arg(cell, 0), loads, names);
    scan_goal(heap, heap.arg(cell, 1), loads, names);
    return;
  }
  if (functor_is(heap, cell, "atom_concat", 3)) {
    const Cell file = heap.deref(heap.arg(cell, 1));
    if (file.tag() != Tag::Atom) return;
    names.emplace_back(heap.deref(heap.arg(cell, 2)).raw(), std::string{atom_text(heap, file)});
    return;
  }
  if (!functor_is(heap, cell, "$lgt_load_prolog_file", 1)) return;
  PrologLoad load;
  load.variable = heap.deref(heap.arg(cell, 0)).raw();
  loads.push_back(std::move(load));
}

void take_initialization(const Heap& heap, Cell term, std::vector<std::string>& files) {
  if (!functor_is(heap, term, ":-", 1)) return;
  const Cell inner = heap.deref(heap.arg(term, 0));
  if (!functor_is(heap, inner, "initialization", 1)) return;
  std::vector<PrologLoad> loads;
  std::vector<std::pair<std::uint64_t, std::string>> names;
  scan_goal(heap, heap.arg(inner, 0), loads, names);
  for (const PrologLoad& load : loads) {
    for (const auto& [variable, file] : names) {
      if (variable == load.variable) files.push_back(file);
    }
  }
}

std::vector<std::string> prolog_files(std::string_view source) {
  std::vector<std::string> files;
  AtomTable atoms;
  Heap heap(atoms);
  OperatorTable operators = OperatorTable::iso_core();
  // tests::run は図形 ::。この読み手だけに足す。ゴール自体は実行しない。
  if (operators.define(600, Specifier::Xfy, "::") != OpResult::Ok) return files;
  Reader reader(source, heap, operators);
  while (true) {
    const ReadOutcome outcome = reader.next();
    if (outcome.failure || !outcome.term) break;
    take_initialization(heap, outcome.term->term, files);
  }
  return files;
}

std::string indicator(const Heap& heap, Cell goal) {
  goal = heap.deref(goal);
  if (goal.tag() == Tag::Atom) return std::string{atom_text(heap, goal)} + "/0";
  if (goal.tag() != Tag::Str) return "^^/1";
  return std::string{heap.atoms().name(heap.functor_atom(goal))} + "/" +
         std::to_string(heap.functor_arity(goal));
}

std::string var_name(const Heap& heap, Cell cell, const std::vector<VarName>& names) {
  cell = heap.deref(cell);
  if (!heap.unbound(cell)) return {};
  for (const VarName& var : names) {
    if (var.cell.raw() == cell.raw()) return var.name;
  }
  return {};
}

void attach_output(Impl& machine, const std::filesystem::path& path, std::string bytes) {
  Stream made;
  made.kind = Stream::Kind::File;
  made.data = Stream::Data::Text;
  made.mode = Stream::Mode::Write;
  made.reposition = true;
  made.path = utf8_of(path);
  made.cursor = bytes.size();
  made.bytes = std::move(bytes);
  machine.streams.push_back(std::move(made));
  machine.current_out = machine.streams.size() - 1;
}

LgtSuper set_text(Machine& machine, const LgtIo& io, Cell goal) {
  LgtSuper result;
  result.kind = LgtSuperKind::No;
  if (io.scratch.empty()) return result;
  const Heap& heap = machine.heap();
  const Cell contents = heap.deref(heap.arg(goal, 0));
  if (contents.tag() != Tag::Atom) return result;
  const auto text = atom_text(heap, contents);
  // lgtunit の /1 は test_output.text。置き場所だけを tests/tmp/suite にする。
  const auto path = io.scratch / "test_output.text";
  if (!write_bytes(path, text)) return result;
  attach_output(engine_impl(machine), path, std::string{text});
  result.kind = LgtSuperKind::Ok;
  return result;
}

LgtSuper assert_text(Machine& machine, Cell goal, const std::vector<VarName>& names) {
  LgtSuper result;
  result.kind = LgtSuperKind::No;
  Impl& impl = engine_impl(machine);
  if (impl.current_out >= impl.streams.size()) return result;
  Stream& stream = impl.streams[impl.current_out];
  if (stream.kind != Stream::Kind::File || stream.path.empty() || stream.closed) return result;
  // ファイルストリームは flush までディスクに出さない。表明はファイルを読む。
  const auto path = path_from_utf8(stream.path);
  if (!write_bytes(path, stream.bytes)) return result;
  const auto contents = read_bytes(path);
  if (!contents) return result;
  stream.closed = true;
  impl.current_out = 1;
  Heap& heap = impl.heap;
  const std::string name = var_name(heap, heap.arg(goal, 1), names);
  if (name.empty()) return result;
  const std::array<Cell, 2> args{heap.deref(heap.arg(goal, 0)), heap.atom(heap.atoms().intern(*contents))};
  result.binding = heap.structure(heap.atoms().intern("=="), args);
  result.binding_name = name;
  result.kind = LgtSuperKind::Ok;
  return result;
}

}  // namespace

LgtIo lgt_io_context(std::string_view tests_path, std::string_view relative_dir) {
  LgtIo io;
  if (tests_path.empty()) return io;
  const std::filesystem::path file{std::string{tests_path}};
  io.tests_dir = file.parent_path();
  const auto root = tests_root(file);
  // 同梱ツリーの外。tests.lgt からの相対ディレクトリを、リポジトリの tests の下に置く。
  io.suite = (root.empty() ? std::filesystem::path{"tests"} : root) / "tmp" / "suite";
  if (!relative_dir.empty()) io.scratch = io.suite / std::string{relative_dir};
  return io;
}

void lgt_io_cleanup(const LgtIo& io) {
  std::error_code error;
  if (!io.scratch.empty()) std::filesystem::remove_all(io.scratch, error);
  if (io.suite.empty()) return;
  const auto start = io.scratch.empty() ? io.suite : io.scratch.parent_path();
  remove_empty_up(start, io.suite);
}

void lgt_load_tester(Machine& machine, const LgtIo& io) {
  if (io.tests_dir.empty()) return;
  const auto tester = io.tests_dir / "tester.lgt";
  std::error_code error;
  if (!std::filesystem::is_regular_file(tester, error) || error) return;
  // Logtalk としては実行しない。logtalk_load はファイル名を作らないので無視する。
  for (const std::string& name : prolog_files(file_text(tester))) {
    // 読めなくてもファイル全体は落とさない。呼ぶテストが存在エラーになる。
    if (!machine.consult((io.tests_dir / name).string())) continue;
  }
}

LgtSuper lgt_run_super(Machine& machine, const LgtIo& io, std::string_view goal) {
  LgtSuper result;
  const LgtParsed parsed = lgt_parse_term(machine, goal);
  if (!parsed.ok) {
    result.note = "^^/1";
    return result;
  }
  const Cell term = machine.heap().deref(parsed.term);
  if (functor_is(machine.heap(), term, "set_text_output", 1)) return set_text(machine, io, term);
  if (functor_is(machine.heap(), term, "text_output_assertion", 2)) {
    return assert_text(machine, term, parsed.names);
  }
  result.note = indicator(machine.heap(), term);
  return result;
}

}  // namespace mosa
