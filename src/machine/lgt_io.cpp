// ^^ は利用者述語にしない。未実装は名前とアリティで不合格にする。
// 一時ファイルは tests/tmp/suite だけ。tests.lgt が終わったら消す。

#include "lgt_io.hpp"

#include "lgt_priv.hpp"

#include "internal.hpp"

#include "mosa/reader.hpp"
#include "mosa/writer.hpp"

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

bool is_nil(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  return cell.tag() == Tag::Atom && atom_text(heap, cell) == "[]";
}

bool is_int_cell(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

std::size_t scalar_width(std::string_view bytes, std::size_t index) {
  if (index >= bytes.size()) return 0;
  const auto lead = static_cast<unsigned char>(bytes[index]);
  std::size_t width = 1;
  if (lead <= 0x7FU) width = 1;
  else if (lead >= 0xC2U && lead <= 0xDFU) width = 2;
  else if (lead >= 0xE0U && lead <= 0xEFU) width = 3;
  else if (lead >= 0xF0U && lead <= 0xF4U) width = 4;
  else return 1;
  if (index + width > bytes.size()) return 1;
  for (std::size_t step = 1; step < width; ++step) {
    if ((static_cast<unsigned char>(bytes[index + step]) & 0xC0U) != 0x80U) return 1;
  }
  return width;
}

void note_write_end(Stream& stream) {
  stream.cursor = stream.bytes.size();
  if (stream.data == Stream::Data::Binary) {
    stream.char_no = stream.bytes.size();
    return;
  }
  std::size_t index = 0;
  while (index < stream.bytes.size()) {
    const std::size_t width = scalar_width(stream.bytes, index);
    const bool newline = width == 1 && stream.bytes[index] == '\n';
    index += width == 0 ? 1 : width;
    stream.char_no += 1;
    if (newline) {
      stream.line += 1;
      stream.line_pos = 1;
    } else {
      stream.line_pos += 1;
    }
  }
}

bool text_contents(const Heap& heap, Cell cell, std::string& out) {
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Atom) {
    out = std::string{atom_text(heap, cell)};
    return true;
  }
  std::string joined;
  int guard = 0;
  while (!is_nil(heap, cell)) {
    if (cell.tag() != Tag::Lis || ++guard > 100000) return false;
    const Cell item = heap.deref(heap.list_head(cell));
    if (item.tag() != Tag::Atom) return false;
    joined.append(atom_text(heap, item));
    cell = heap.deref(heap.list_tail(cell));
  }
  out = std::move(joined);
  return true;
}

bool byte_contents(const Heap& heap, Cell cell, std::string& out) {
  std::string bytes;
  int guard = 0;
  cell = heap.deref(cell);
  while (!is_nil(heap, cell)) {
    if (cell.tag() != Tag::Lis || ++guard > 100000) return false;
    const Cell item = heap.deref(heap.list_head(cell));
    if (!is_int_cell(item)) return false;
    const auto value = heap.integer_value(item);
    if (value < 0 || value > 255) return false;
    bytes.push_back(static_cast<char>(static_cast<unsigned char>(value)));
    cell = heap.deref(heap.list_tail(cell));
  }
  out = std::move(bytes);
  return true;
}

Cell byte_list(Heap& heap, std::string_view bytes) {
  Cell tail = heap.atom(heap.atoms().intern("[]"));
  for (std::size_t index = bytes.size(); index > 0; --index) {
    const auto unit = static_cast<unsigned char>(bytes[index - 1]);
    tail = heap.list(heap.integer(static_cast<std::int64_t>(unit)), tail);
  }
  return tail;
}

struct IoOpt {
  bool reposition = true;
  Stream::Data data = Stream::Data::Text;
  Stream::EofAction eof = Stream::EofAction::EofCode;
};

bool utf8_name(std::string_view text) { return text == "utf8" || text == "UTF-8" || text == "utf-8"; }

bool take_opt(const Heap& heap, Cell item, IoOpt& opt) {
  item = heap.deref(item);
  if (item.tag() != Tag::Str || heap.functor_arity(item) != 1) return false;
  const auto name = heap.atoms().name(heap.functor_atom(item));
  const Cell arg = heap.deref(heap.arg(item, 0));
  if (arg.tag() != Tag::Atom) return false;
  const auto text = atom_text(heap, arg);
  if (name == "reposition" && (text == "true" || text == "false")) {
    opt.reposition = text == "true";
    return true;
  }
  if (name == "eof_action" && text == "error") {
    opt.eof = Stream::EofAction::Error;
    return true;
  }
  if (name == "eof_action" && text == "eof_code") {
    opt.eof = Stream::EofAction::EofCode;
    return true;
  }
  if (name == "eof_action" && text == "reset") {
    opt.eof = Stream::EofAction::Reset;
    return true;
  }
  if (name == "type" && text == "text") {
    opt.data = Stream::Data::Text;
    return true;
  }
  if (name == "type" && text == "binary") {
    opt.data = Stream::Data::Binary;
    return true;
  }
  if (name == "alias") return true;
  return name == "encoding" && utf8_name(text);
}

bool read_options(const Heap& heap, Cell list, IoOpt& opt) {
  list = heap.deref(list);
  int guard = 0;
  while (!is_nil(heap, list)) {
    if (list.tag() != Tag::Lis || ++guard > 32) return false;
    if (!take_opt(heap, heap.list_head(list), opt)) return false;
    list = heap.deref(heap.list_tail(list));
  }
  return true;
}

std::filesystem::path absolute_path(const std::filesystem::path& path) {
  std::error_code error;
  const auto full = std::filesystem::absolute(path, error);
  if (error) return {};
  return full.lexically_normal();
}

bool inside_root(const std::filesystem::path& root, const std::filesystem::path& full) {
  const auto base = absolute_path(root);
  if (base.empty() || full.empty()) return false;
  const auto relative = full.lexically_relative(base);
  if (relative.empty() || relative == ".") return false;
  for (const auto& part : relative) {
    if (part == "..") return false;
  }
  return true;
}

bool inside_suite(const LgtIo& io, const std::filesystem::path& full) { return inside_root(io.suite, full); }

std::filesystem::path sibling_source(const LgtIo& io, std::string_view name) {
  if (io.tests_dir.empty() || name.empty()) return {};
  const auto raw = path_from_utf8(name);
  // 絶対パスや tests.lgt の外は、同梱ディレクトリの正規ファイルではない。
  if (raw.empty() || raw.is_absolute()) return {};
  const auto full = absolute_path(io.tests_dir / raw);
  if (!inside_root(io.tests_dir, full)) return {};
  std::error_code error;
  if (!std::filesystem::is_regular_file(full, error) || error) return {};
  return full;
}

bool copy_absent_sibling(const LgtIo& io, std::string_view name, const std::filesystem::path& dest) {
  std::error_code error;
  // 既にあるスクラッチはテストが書いたものなので、同梱の中身で戻さない。
  if (std::filesystem::exists(dest, error) || error) return !error;
  const auto source = sibling_source(io, name);
  if (source.empty()) return true;
  std::filesystem::copy_file(source, dest, std::filesystem::copy_options::none, error);
  return !error;
}

std::filesystem::path locate_file(const LgtIo& io, std::string_view name) {
  if (io.scratch.empty()) return {};
  auto path = path_from_utf8(name);
  if (!path.is_absolute()) path = io.scratch / path;
  path = absolute_path(path);
  if (!inside_suite(io, path)) return {};
  return path;
}

void close_index(Impl& machine, std::size_t index) {
  Stream& stream = machine.streams[index];
  stream.closed = true;
  if (machine.current_in == index) machine.current_in = 0;
  if (machine.current_out == index) machine.current_out = 1;
}

void close_path(Impl& machine, std::string_view path) {
  for (std::size_t index = 0; index < machine.streams.size(); ++index) {
    Stream& stream = machine.streams[index];
    if (stream.closed || stream.path != path) continue;
    close_index(machine, index);
  }
}

void close_alias(Impl& machine, std::string_view alias) {
  if (alias.empty()) return;
  for (std::size_t index = 0; index < machine.streams.size(); ++index) {
    Stream& stream = machine.streams[index];
    if (stream.closed || stream.alias != alias) continue;
    close_index(machine, index);
  }
}

void release_closed_alias(Impl& machine, std::string_view alias) {
  if (alias.empty()) return;
  // 閉じた流れが同じ別名を持ったままだと、新しい流れに届かない。
  for (Stream& old : machine.streams) {
    if (old.closed && old.alias == alias) old.alias.clear();
  }
}

std::optional<std::size_t> open_alias(const Impl& machine, std::string_view alias) {
  for (std::size_t index = 0; index < machine.streams.size(); ++index) {
    const Stream& stream = machine.streams[index];
    if (!stream.closed && stream.alias == alias) return index;
  }
  return std::nullopt;
}

LgtSuper bound_ok() {
  LgtSuper result;
  result.kind = LgtSuperKind::Ok;
  return result;
}

LgtSuper bound_no() {
  LgtSuper result;
  result.kind = LgtSuperKind::No;
  return result;
}

LgtSuper bind_slot(Machine& machine, Cell slot, Cell value, const std::vector<VarName>& names) {
  const std::string name = var_name(machine.heap(), slot, names);
  if (name.empty()) return bound_no();
  LgtSuper result = bound_ok();
  result.binding_name = name;
  result.binding = value;
  return result;
}

Cell eq_cell(Heap& heap, Cell left, Cell right) {
  const std::array<Cell, 2> args{left, right};
  return heap.structure(heap.atoms().intern("=="), args);
}

bool load_place(const Heap& heap, Cell goal, int arity, int& alias, int& contents, int& options, bool& current,
                IoOpt& opt, Stream::Data data) {
  alias = -1;
  contents = 0;
  options = -1;
  current = arity == 1;
  opt = IoOpt{};
  opt.data = data;
  if (arity >= 2) {
    alias = 0;
    contents = 1;
  }
  if (arity == 3) options = 2;
  if (options >= 0 && !read_options(heap, heap.arg(goal, static_cast<std::uint16_t>(options)), opt)) return false;
  return true;
}

bool open_obs(Machine& machine, const LgtIo& io, Cell goal, int arity, std::string_view file, Stream::Mode mode,
              Stream::Data data, bool input_bytes) {
  if (io.scratch.empty()) return false;
  const Heap& heap = machine.heap();
  int alias = -1;
  int contents = 0;
  int options = -1;
  bool current = false;
  IoOpt opt;
  if (!load_place(heap, goal, arity, alias, contents, options, current, opt, data)) return false;
  std::string bytes;
  const Cell body = heap.arg(goal, static_cast<std::uint16_t>(contents));
  if (input_bytes) {
    if (!byte_contents(heap, body, bytes)) return false;
  } else if (!text_contents(heap, body, bytes)) {
    return false;
  }
  std::string alias_name;
  if (alias >= 0) {
    const Cell alias_cell = heap.deref(heap.arg(goal, static_cast<std::uint16_t>(alias)));
    if (alias_cell.tag() != Tag::Atom) return false;
    alias_name = std::string{atom_text(heap, alias_cell)};
  }
  const auto path = locate_file(io, file);
  if (path.empty() || !write_bytes(path, bytes)) return false;
  Impl& impl = engine_impl(machine);
  const std::string key = utf8_of(path);
  close_alias(impl, alias_name);
  close_path(impl, key);
  release_closed_alias(impl, alias_name);
  Stream made;
  made.kind = Stream::Kind::File;
  made.data = opt.data;
  made.mode = mode;
  made.eof_action = opt.eof;
  made.reposition = opt.reposition;
  made.path = key;
  made.alias = std::move(alias_name);
  made.bytes = std::move(bytes);
  if (mode != Stream::Mode::Read) note_write_end(made);
  impl.streams.push_back(std::move(made));
  const auto index = impl.streams.size() - 1;
  if (current && mode == Stream::Mode::Read) impl.current_in = index;
  if (current && mode != Stream::Mode::Read) impl.current_out = index;
  return true;
}

std::optional<std::size_t> stream_at(Impl& machine, Cell goal, int arity, bool input) {
  if (arity == 1) return input ? machine.current_in : machine.current_out;
  const Heap& heap = machine.heap;
  const Cell alias = heap.deref(heap.arg(goal, 0));
  if (alias.tag() != Tag::Atom) return std::nullopt;
  return open_alias(machine, atom_text(heap, alias));
}

std::string whole_bytes(const Stream& stream) { return stream.bytes; }

std::string rest_bytes(const Stream& stream) {
  if (stream.cursor >= stream.bytes.size()) return {};
  return stream.bytes.substr(stream.cursor);
}

void drop_file(Impl& machine, const std::filesystem::path& path) {
  close_path(machine, utf8_of(path));
  std::error_code error;
  std::filesystem::remove(path, error);
}

LgtSuper assert_bytes(Machine& machine, Cell expected, Cell slot, std::string bytes, bool as_text,
                      const std::vector<VarName>& names) {
  Heap& heap = machine.heap();
  const Cell right = as_text ? heap.atom(heap.atoms().intern(bytes)) : byte_list(heap, bytes);
  return bind_slot(machine, slot, eq_cell(heap, heap.deref(expected), right), names);
}

LgtSuper take_assertion(Machine& machine, const LgtIo& io, Cell goal, int arity, std::string_view file, bool input,
                        bool as_text, const std::vector<VarName>& names) {
  Impl& impl = engine_impl(machine);
  const int stream_arity = arity == 2 ? 1 : arity - 1;
  const auto index = stream_at(impl, goal, stream_arity, input);
  if (!index || *index >= impl.streams.size()) return bound_no();
  Stream& stream = impl.streams[*index];
  if (stream.closed) return bound_no();
  if (stream.kind != Stream::Kind::File) return bound_no();
  const std::string bytes = input ? rest_bytes(stream) : whole_bytes(stream);
  const int expected_arg = arity == 2 ? 0 : 1;
  const int slot_arg = arity - 1;
  if (arity == 4) {
    IoOpt opt;
    if (!read_options(machine.heap(), machine.heap().arg(goal, 2), opt)) return bound_no();
  }
  const Cell expected = machine.heap().arg(goal, static_cast<std::uint16_t>(expected_arg));
  const Cell slot = machine.heap().arg(goal, static_cast<std::uint16_t>(slot_arg));
  const auto path = stream.path.empty() ? locate_file(io, file) : path_from_utf8(stream.path);
  if (!path.empty()) drop_file(impl, path);
  return assert_bytes(machine, expected, slot, bytes, as_text, names);
}

LgtSuper check_obs(Machine& machine, const LgtIo& io, Cell goal, int arity, std::string_view file, bool input,
                   bool as_text) {
  Impl& impl = engine_impl(machine);
  const auto index = stream_at(impl, goal, arity, input);
  if (!index || *index >= impl.streams.size()) return bound_no();
  Stream& stream = impl.streams[*index];
  if (stream.closed || stream.kind != Stream::Kind::File) return bound_no();
  const std::string bytes = input ? rest_bytes(stream) : whole_bytes(stream);
  std::string expect;
  const Cell expected = machine.heap().arg(goal, static_cast<std::uint16_t>(arity - 1));
  const bool read = as_text ? text_contents(machine.heap(), expected, expect)
                            : byte_contents(machine.heap(), expected, expect);
  const auto path = stream.path.empty() ? locate_file(io, file) : path_from_utf8(stream.path);
  if (!path.empty()) drop_file(impl, path);
  if (!read || expect != bytes) return bound_no();
  return bound_ok();
}

LgtSuper clean_named(Machine& machine, const LgtIo& io, std::string_view name) {
  const auto path = locate_file(io, name);
  if (path.empty()) return bound_no();
  drop_file(engine_impl(machine), path);
  return bound_ok();
}

LgtSuper clean_dir(Machine& machine, const LgtIo& io, Cell goal) {
  const Cell name = machine.heap().deref(machine.heap().arg(goal, 0));
  if (name.tag() != Tag::Atom) return bound_no();
  const auto path = locate_file(io, atom_text(machine.heap(), name));
  if (path.empty()) return bound_no();
  close_path(engine_impl(machine), utf8_of(path));
  std::error_code error;
  std::filesystem::remove_all(path, error);
  return bound_ok();
}

LgtSuper file_path_obs(Machine& machine, const LgtIo& io, Cell goal, const std::vector<VarName>& names) {
  const Heap& heap = machine.heap();
  const Cell file = heap.deref(heap.arg(goal, 0));
  if (file.tag() != Tag::Atom) return bound_no();
  const auto name = atom_text(heap, file);
  const auto path = locate_file(io, name);
  if (path.empty()) return bound_no();
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error || !copy_absent_sibling(io, name, path)) return bound_no();
  return bind_slot(machine, heap.arg(goal, 1), heap.atom(heap.atoms().intern(utf8_of(path))), names);
}

LgtSuper create_obs(Machine& machine, const LgtIo& io, Cell goal, bool binary) {
  const Heap& heap = machine.heap();
  const Cell file = heap.deref(heap.arg(goal, 0));
  if (file.tag() != Tag::Atom) return bound_no();
  std::string bytes;
  const Cell body = heap.arg(goal, 1);
  const bool read = binary ? byte_contents(heap, body, bytes) : text_contents(heap, body, bytes);
  if (!read) return bound_no();
  const auto path = locate_file(io, atom_text(heap, file));
  if (path.empty() || !write_bytes(path, bytes)) return bound_no();
  return bound_ok();
}

Cell stream_cell(Heap& heap, std::size_t index) {
  const std::array<Cell, 1> args{heap.integer(static_cast<std::int64_t>(index))};
  return heap.structure(heap.atoms().intern("$stream"), args);
}

LgtSuper closed_obs(Machine& machine, const LgtIo& io, Cell goal, Stream::Mode mode,
                    const std::vector<VarName>& names) {
  IoOpt opt;
  if (!read_options(machine.heap(), machine.heap().arg(goal, 1), opt)) return bound_no();
  const auto path = locate_file(io, "temporary_file");
  if (path.empty() || !write_bytes(path, {})) return bound_no();
  Impl& impl = engine_impl(machine);
  close_path(impl, utf8_of(path));
  Stream made;
  made.kind = Stream::Kind::File;
  made.data = opt.data;
  made.mode = mode;
  made.eof_action = opt.eof;
  made.reposition = opt.reposition;
  made.path = utf8_of(path);
  made.closed = true;
  impl.streams.push_back(std::move(made));
  std::error_code error;
  std::filesystem::remove(path, error);
  return bind_slot(machine, machine.heap().arg(goal, 0), stream_cell(machine.heap(), impl.streams.size() - 1), names);
}

LgtSuper position_obs(Machine& machine, Cell goal, const std::vector<VarName>& names) {
  Heap& heap = machine.heap();
  const std::array<Cell, 3> args{heap.integer(0), heap.integer(1), heap.integer(1)};
  const Cell position = heap.structure(heap.atoms().intern("stream_position"), args);
  return bind_slot(machine, heap.arg(goal, 0), position, names);
}

bool named_goal(const Heap& heap, Cell cell, std::string_view name, int& arity) {
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Atom) {
    arity = 0;
    return atom_text(heap, cell) == name;
  }
  if (cell.tag() != Tag::Str || heap.atoms().name(heap.functor_atom(cell)) != name) return false;
  arity = heap.functor_arity(cell);
  return true;
}

LgtSuper open_named(Machine& machine, const LgtIo& io, Cell term, int arity, std::string_view file, Stream::Mode mode,
                    Stream::Data data, bool bytes) {
  return open_obs(machine, io, term, arity, file, mode, data, bytes) ? bound_ok() : bound_no();
}

LgtSuper observe(Machine& machine, const LgtIo& io, Cell term, const std::vector<VarName>& names) {
  const Heap& heap = machine.heap();
  int arity = 0;
  if (named_goal(heap, term, "set_text_output", arity) && arity >= 1 && arity <= 3) {
    return open_named(machine, io, term, arity, "test_output.text", Stream::Mode::Write, Stream::Data::Text, false);
  }
  if (named_goal(heap, term, "set_text_input", arity) && arity >= 1 && arity <= 3) {
    return open_named(machine, io, term, arity, "test_input.text", Stream::Mode::Read, Stream::Data::Text, false);
  }
  if (named_goal(heap, term, "set_binary_output", arity) && arity >= 1 && arity <= 3) {
    return open_named(machine, io, term, arity, "test_output.binary", Stream::Mode::Write, Stream::Data::Binary, true);
  }
  if (named_goal(heap, term, "set_binary_input", arity) && arity >= 1 && arity <= 3) {
    return open_named(machine, io, term, arity, "test_input.binary", Stream::Mode::Read, Stream::Data::Binary, true);
  }
  if (named_goal(heap, term, "text_output_assertion", arity) && arity >= 2 && arity <= 4) {
    return take_assertion(machine, io, term, arity, "test_output.text", false, true, names);
  }
  if (named_goal(heap, term, "text_input_assertion", arity) && (arity == 2 || arity == 3)) {
    return take_assertion(machine, io, term, arity, "test_input.text", true, true, names);
  }
  if (named_goal(heap, term, "binary_output_assertion", arity) && (arity == 2 || arity == 3)) {
    return take_assertion(machine, io, term, arity, "test_output.binary", false, false, names);
  }
  if (named_goal(heap, term, "binary_input_assertion", arity) && (arity == 2 || arity == 3)) {
    return take_assertion(machine, io, term, arity, "test_input.binary", true, false, names);
  }
  if (named_goal(heap, term, "check_text_input", arity) && (arity == 1 || arity == 2)) {
    return check_obs(machine, io, term, arity, "test_input.text", true, true);
  }
  if (named_goal(heap, term, "check_binary_input", arity) && (arity == 1 || arity == 2)) {
    return check_obs(machine, io, term, arity, "test_input.binary", true, false);
  }
  if (named_goal(heap, term, "file_path", arity) && arity == 2) return file_path_obs(machine, io, term, names);
  if (named_goal(heap, term, "create_text_file", arity) && arity == 2) return create_obs(machine, io, term, false);
  if (named_goal(heap, term, "create_binary_file", arity) && arity == 2) return create_obs(machine, io, term, true);
  if (named_goal(heap, term, "clean_text_input", arity) && arity == 0) return clean_named(machine, io, "test_input.text");
  if (named_goal(heap, term, "clean_text_output", arity) && arity == 0) return clean_named(machine, io, "test_output.text");
  if (named_goal(heap, term, "clean_binary_input", arity) && arity == 0) return clean_named(machine, io, "test_input.binary");
  if (named_goal(heap, term, "clean_binary_output", arity) && arity == 0) return clean_named(machine, io, "test_output.binary");
  if (named_goal(heap, term, "clean_file", arity) && arity == 1) {
    const Cell name = heap.deref(heap.arg(term, 0));
    if (name.tag() != Tag::Atom) return bound_no();
    return clean_named(machine, io, atom_text(heap, name));
  }
  if (named_goal(heap, term, "clean_directory", arity) && arity == 1) return clean_dir(machine, io, term);
  if (named_goal(heap, term, "closed_input_stream", arity) && arity == 2) {
    return closed_obs(machine, io, term, Stream::Mode::Read, names);
  }
  if (named_goal(heap, term, "closed_output_stream", arity) && arity == 2) {
    return closed_obs(machine, io, term, Stream::Mode::Write, names);
  }
  if (named_goal(heap, term, "stream_position", arity) && arity == 1) return position_obs(machine, term, names);
  return {};
}

}  // namespace

LgtIo lgt_io_context(std::string_view tests_path, std::string_view relative_dir) {
  LgtIo io;
  if (tests_path.empty()) return io;
  // 実行中はカレントをスクラッチへ移すので、相対のままだと隣の Prolog とスクラッチを見失う。
  const std::filesystem::path file = absolute_path(std::filesystem::path{std::string{tests_path}});
  if (file.empty()) return io;
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

static SuiteSuper suite_super_call(Machine& machine, const LgtIo& io, Cell goal) {
  const std::string text = write_term(machine.heap(), machine.operators(), machine.heap().deref(goal), {}, {});
  const LgtSuper ran = lgt_run_super(machine, io, text);
  if (ran.kind == LgtSuperKind::Unknown) return SuiteSuper::Skip;
  return ran.kind == LgtSuperKind::Ok ? SuiteSuper::Ok : SuiteSuper::No;
}

void lgt_arm_super(Machine& machine, const LgtIo& io) {
  Impl& impl = engine_impl(machine);
  impl.suite_super = suite_super_call;
  impl.suite_io = &io;
  impl.suite_self = &machine;
}

void lgt_disarm_super(Machine& machine) {
  Impl& impl = engine_impl(machine);
  impl.suite_super = nullptr;
  impl.suite_io = nullptr;
  impl.suite_self = nullptr;
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
  const LgtSuper ran = observe(machine, io, term, parsed.names);
  if (ran.kind != LgtSuperKind::Unknown) return ran;
  result.note = indicator(machine.heap(), term);
  return result;
}

}  // namespace mosa
