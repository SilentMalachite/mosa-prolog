// 規格 8.11.1 open/3,4。8.11.6 close/1,2。8.11.7 flush_output/0,1。open_string/2 は ADR 0005。
// open(+source_sink, +io_mode, -stream)
// open(+source_sink, +io_mode, -stream, +stream_options)
//   instantiation_error, uninstantiation_error, type_error(atom), type_error(list),
//   domain_error(source_sink), domain_error(io_mode), domain_error(stream_option),
//   existence_error(source_sink), permission_error(open, source_sink)
// open_string(+source, -stream)
//   source はアトムかコードのリスト。文字のリストは受けない。
//   instantiation_error, type_error(atom), type_error(list), type_error(integer),
//   uninstantiation_error, representation_error(character_code)
// close(+stream_or_alias) / close(+stream_or_alias, +close_options)
//   選択肢は force(true/false)。無いときは force(false)。
//   書き戻しに失敗し force が false なら system_error。ストリームは開いたまま。
// flush_output / flush_output(+stream_or_alias)
//   入力ストリームは permission_error(output, stream, S)。
// テスト: tests/pl/streams.pl, tests/pl/close.pl, tests/pl/japanese.pl, tests/cpp/stream_test.cpp

#include "../machine/internal.hpp"

#include <array>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace mosa {
namespace {

struct OpenChoice {
  Stream::Mode mode = Stream::Mode::Read;
  Stream::Data data = Stream::Data::Text;
  bool reposition = true;
  Stream::EofAction eof = Stream::EofAction::EofCode;
  std::string alias;
  bool has_alias = false;
};

struct OptionScan {
  bool raised = false;
  WalkedList walked;
};

enum class FileLoad { Ok, Missing, Denied };

struct FileLoadResult {
  FileLoad status = FileLoad::Missing;
  std::string bytes;
};

bool raise_inst(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
  return true;
}

bool raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
  return true;
}

bool raise_domain(Impl& machine, const Goal& goal, std::string_view domain, Cell culprit) {
  raise_formal(machine, formal_domain(machine, domain, culprit), goal.name, goal.arity);
  return true;
}

std::string_view atom_text(const Impl& machine, Cell cell) {
  return machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3));
}

bool item_unbound(const Impl& machine, const WalkedList& walked) {
  for (const Cell item : walked.items) {
    if (machine.heap.unbound(item)) return true;
  }
  return false;
}

// パスは UTF-8。OS のワイド文字への変換は filesystem に任せ、分岐を書かない。
std::filesystem::path path_from_utf8(std::string_view text) {
  std::u8string bytes;
  bytes.reserve(text.size());
  for (unsigned char byte : text) bytes.push_back(static_cast<char8_t>(byte));
  return std::filesystem::path{bytes};
}

FileLoadResult load_bytes(const std::filesystem::path& path) {
  FileLoadResult result;
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    std::error_code code;
    const bool there = std::filesystem::exists(path, code);
    result.status = there && !code ? FileLoad::Denied : FileLoad::Missing;
    return result;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  result.status = FileLoad::Ok;
  result.bytes = buffer.str();
  return result;
}

bool create_empty(const std::filesystem::path& path) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  return static_cast<bool>(output);
}

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

// 追記は末尾から書く。開いた時点の位置を、バイト列を一度歩いて残す。
void note_end(Stream& stream) {
  stream.cursor = stream.bytes.size();
  if (stream.data == Stream::Data::Binary) {
    stream.char_no = stream.bytes.size();
    return;
  }
  std::size_t index = 0;
  while (index < stream.bytes.size()) {
    const std::size_t width = scalar_width(stream.bytes, index);
    const bool newline = width == 1 && stream.bytes[index] == '\n';
    index += width;
    stream.char_no += 1;
    if (newline) {
      stream.line += 1;
      stream.line_pos = 1;
    } else {
      stream.line_pos += 1;
    }
  }
}

Stream standard_stream(Stream::Mode mode, std::string alias) {
  Stream stream;
  stream.kind = Stream::Kind::Standard;
  stream.mode = mode;
  stream.reposition = false;
  stream.alias = std::move(alias);
  return stream;
}

bool output_bound(Impl& machine, const Goal& goal, Cell stream) {
  if (machine.heap.unbound(stream)) return false;
  // Cor.2 の 8.11.5.3 f。モードが - の引数は、具体値なら型誤りにしない。
  raise_formal(machine, formal_uninstantiation(machine, stream), goal.name, goal.arity);
  return true;
}

bool args_unbound(Impl& machine, const Goal& goal, Cell source, Cell mode, Cell options) {
  if (machine.heap.unbound(source) || machine.heap.unbound(mode)) return raise_inst(machine, goal);
  if (goal.arity == 4 && machine.heap.unbound(options)) return raise_inst(machine, goal);
  return false;
}

OptionScan scan_options(Impl& machine, const Goal& goal, Cell options) {
  OptionScan scan;
  if (goal.arity == 3) {
    scan.walked.shape = ListShape::Proper;
    return scan;
  }
  scan.walked = walk_list(machine, options);
  const bool open = scan.walked.shape == ListShape::Variable || scan.walked.shape == ListShape::Partial ||
                    item_unbound(machine, scan.walked);
  if (open) {
    raise_inst(machine, goal);
    scan.raised = true;
  }
  return scan;
}

bool reject_mode_type(Impl& machine, const Goal& goal, Cell mode) {
  if (mode.tag() == Tag::Atom) return false;
  return raise_type(machine, goal, "atom", mode);
}

bool reject_list_type(Impl& machine, const Goal& goal, Cell options, const WalkedList& walked) {
  if (goal.arity == 3 || walked.shape == ListShape::Proper) return false;
  return raise_type(machine, goal, "list", options);
}

bool known_mode(const Impl& machine, Cell mode, Stream::Mode& out) {
  const auto text = atom_text(machine, mode);
  if (text == "read") out = Stream::Mode::Read;
  else if (text == "write") out = Stream::Mode::Write;
  else if (text == "append") out = Stream::Mode::Append;
  else return false;
  return true;
}

bool set_atom_option(OpenChoice& choice, std::string_view name, std::string_view text) {
  if (name == "type" && text == "text") choice.data = Stream::Data::Text;
  else if (name == "type" && text == "binary") choice.data = Stream::Data::Binary;
  else if (name == "reposition" && text == "true") choice.reposition = true;
  else if (name == "reposition" && text == "false") choice.reposition = false;
  else if (name == "eof_action" && text == "error") choice.eof = Stream::EofAction::Error;
  else if (name == "eof_action" && text == "eof_code") choice.eof = Stream::EofAction::EofCode;
  else if (name == "eof_action" && text == "reset") choice.eof = Stream::EofAction::Reset;
  else if (name == "encoding" && text == "utf8") return true;
  else return false;
  return true;
}

bool take_option(Impl& machine, const Goal& goal, Cell option, OpenChoice& choice) {
  option = machine.heap.deref(option);
  if (option.tag() != Tag::Str || machine.heap.functor_arity(option) != 1) {
    return raise_domain(machine, goal, "stream_option", option);
  }
  const auto name = machine.atoms.name(machine.heap.functor_atom(option));
  const bool known = name == "type" || name == "reposition" || name == "alias" || name == "eof_action" ||
                     name == "encoding";
  if (!known) return raise_domain(machine, goal, "stream_option", option);
  const Cell arg = machine.heap.deref(machine.heap.arg(option, 0));
  if (machine.heap.unbound(arg)) return raise_inst(machine, goal);
  if (arg.tag() != Tag::Atom) return raise_type(machine, goal, "atom", arg);
  if (name == "alias") {
    if (choice.has_alias) return raise_domain(machine, goal, "stream_option", option);
    choice.has_alias = true;
    choice.alias = std::string{atom_text(machine, arg)};
    return false;
  }
  if (!set_atom_option(choice, name, atom_text(machine, arg))) {
    return raise_domain(machine, goal, "stream_option", option);
  }
  return false;
}

bool fill_choice(Impl& machine, const Goal& goal, const WalkedList& walked, OpenChoice& choice) {
  if (goal.arity == 3) return false;
  for (const Cell item : walked.items) {
    if (take_option(machine, goal, item, choice)) return true;
  }
  return false;
}

void bind_new(Impl& machine, const Goal& goal, Cell slot, Stream stream) {
  const auto index = machine.streams.size();
  machine.streams.push_back(std::move(stream));
  const std::array<Cell, 1> args{machine.heap.integer(static_cast<std::int64_t>(index))};
  const Cell term = machine.heap.structure(machine.atoms.intern("$stream"), args);
  if (!do_unify(machine, slot, term)) machine.failed = true;
  else machine.ip = goal.cont;
}

bool alias_open(const Impl& machine, std::string_view alias) {
  for (const Stream& stream : machine.streams) {
    if (!stream.closed && stream.alias == alias) return true;
  }
  return false;
}

bool take_alias(Impl& machine, const Goal& goal, const OpenChoice& choice) {
  if (!choice.has_alias || !alias_open(machine, choice.alias)) return false;
  const std::array<Cell, 1> args{machine.heap.atom(machine.atoms.intern(choice.alias))};
  const Cell culprit = machine.heap.structure(machine.atoms.intern("alias"), args);
  raise_formal(machine, formal_permission(machine, "open", "source_sink", culprit), goal.name, goal.arity);
  return true;
}

bool refuse_file(Impl& machine, const Goal& goal, Cell source) {
  raise_formal(machine, formal_permission(machine, "open", "source_sink", source), goal.name, goal.arity);
  return true;
}

bool fill_read(Impl& machine, const Goal& goal, Cell source, const std::filesystem::path& path, Stream& made) {
  const FileLoadResult loaded = load_bytes(path);
  if (loaded.status == FileLoad::Missing) {
    raise_formal(machine, formal_existence(machine, "source_sink", source), goal.name, goal.arity);
    return true;
  }
  if (loaded.status == FileLoad::Denied) return refuse_file(machine, goal, source);
  made.bytes = loaded.bytes;
  return false;
}

bool fill_write(Impl& machine, const Goal& goal, Cell source, const std::filesystem::path& path, Stream& made) {
  if (made.mode == Stream::Mode::Write) {
    if (!create_empty(path)) return refuse_file(machine, goal, source);
    return false;
  }
  const FileLoadResult loaded = load_bytes(path);
  if (loaded.status == FileLoad::Denied) return refuse_file(machine, goal, source);
  if (loaded.status == FileLoad::Ok) made.bytes = loaded.bytes;
  else if (!create_empty(path)) return refuse_file(machine, goal, source);
  note_end(made);
  return false;
}

void finish_open(Impl& machine, const Goal& goal, Cell source, Cell slot, const OpenChoice& choice) {
  Stream made;
  made.kind = Stream::Kind::File;
  made.data = choice.data;
  made.mode = choice.mode;
  made.eof_action = choice.eof;
  made.reposition = choice.reposition;
  made.path = std::string{atom_text(machine, source)};
  if (choice.has_alias) {
    // 閉じた流れが同じ別名を持ったままだと、新しい流れに届かない。
    for (Stream& old : machine.streams) {
      if (old.closed && old.alias == choice.alias) old.alias.clear();
    }
    made.alias = choice.alias;
  }
  const auto path = path_from_utf8(made.path);
  if (choice.mode == Stream::Mode::Read) {
    if (fill_read(machine, goal, source, path, made) || take_alias(machine, goal, choice)) return;
  } else if (take_alias(machine, goal, choice) || fill_write(machine, goal, source, path, made)) {
    return;
  }
  bind_new(machine, goal, slot, std::move(made));
}

void run_open(Impl& machine, const Goal& goal) {
  const Cell source = machine.heap.deref(goal.arg(machine, 0));
  const Cell mode = machine.heap.deref(goal.arg(machine, 1));
  const Cell stream = machine.heap.deref(goal.arg(machine, 2));
  const Cell options = goal.arity == 4 ? machine.heap.deref(goal.arg(machine, 3)) : Cell{};
  if (args_unbound(machine, goal, source, mode, options)) return;
  const OptionScan scan = scan_options(machine, goal, options);
  if (scan.raised || reject_mode_type(machine, goal, mode)) return;
  if (reject_list_type(machine, goal, options, scan.walked) || output_bound(machine, goal, stream)) return;
  if (source.tag() != Tag::Atom) {
    raise_domain(machine, goal, "source_sink", source);
    return;
  }
  OpenChoice choice;
  if (!known_mode(machine, mode, choice.mode)) {
    raise_domain(machine, goal, "io_mode", mode);
    return;
  }
  if (fill_choice(machine, goal, scan.walked, choice)) return;
  finish_open(machine, goal, source, goal.arg(machine, 2), choice);
}

bool codes_of(Impl& machine, const Goal& goal, const WalkedList& walked, std::string& bytes) {
  for (const Cell item : walked.items) {
    if (machine.heap.unbound(item)) return raise_inst(machine, goal);
    if (item.tag() != Tag::Int && item.tag() != Tag::IntBox) return raise_type(machine, goal, "integer", item);
    const auto code = machine.heap.integer_value(item);
    if (!scalar_code(code)) {
      raise_formal(machine, formal_representation(machine, "character_code"), goal.name, goal.arity);
      return true;
    }
    append_utf8_code(bytes, static_cast<char32_t>(code));
  }
  return false;
}

void run_open_string(Impl& machine, const Goal& goal) {
  const Cell source = machine.heap.deref(goal.arg(machine, 0));
  const Cell stream = machine.heap.deref(goal.arg(machine, 1));
  if (machine.heap.unbound(source)) {
    raise_inst(machine, goal);
    return;
  }
  if (output_bound(machine, goal, stream)) return;
  std::string bytes;
  if (source.tag() == Tag::Atom && atom_text(machine, source) != "[]") {
    bytes = std::string{atom_text(machine, source)};
  } else {
    const WalkedList walked = walk_list(machine, source);
    if (walked.shape == ListShape::Partial || item_unbound(machine, walked)) {
      raise_inst(machine, goal);
      return;
    }
    if (walked.shape != ListShape::Proper) {
      const bool list = source.tag() == Tag::Lis || (source.tag() == Tag::Atom && atom_text(machine, source) == "[]");
      raise_type(machine, goal, list ? "list" : "atom", source);
      return;
    }
    if (codes_of(machine, goal, walked, bytes)) return;
  }
  Stream made;
  made.kind = Stream::Kind::Memory;
  made.data = Stream::Data::Text;
  made.mode = Stream::Mode::Read;
  made.reposition = true;
  made.bytes = std::move(bytes);
  bind_new(machine, goal, goal.arg(machine, 1), std::move(made));
}

bool find_alias(const Impl& machine, std::string_view name, std::size_t& index) {
  for (std::size_t at = 0; at < machine.streams.size(); ++at) {
    if (machine.streams[at].alias == name) {
      index = at;
      return true;
    }
  }
  return false;
}

bool resolve_stream(Impl& machine, const Goal& goal, Cell term, std::size_t& index) {
  const Cell cell = machine.heap.deref(term);
  if (machine.heap.unbound(cell)) return raise_inst(machine, goal);
  if (cell.tag() == Tag::Atom) {
    if (!find_alias(machine, atom_text(machine, cell), index)) return raise_domain(machine, goal, "stream_or_alias", cell);
    return false;
  }
  const bool shaped = cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 1 &&
                      machine.atoms.name(machine.heap.functor_atom(cell)) == "$stream";
  if (!shaped) return raise_domain(machine, goal, "stream_or_alias", cell);
  const Cell arg = machine.heap.deref(machine.heap.arg(cell, 0));
  if (arg.tag() != Tag::Int && arg.tag() != Tag::IntBox) return raise_domain(machine, goal, "stream_or_alias", cell);
  const auto number = machine.heap.integer_value(arg);
  if (number < 0 || static_cast<std::uint64_t>(number) >= machine.streams.size()) {
    raise_formal(machine, formal_existence(machine, "stream", cell), goal.name, goal.arity);
    return true;
  }
  index = static_cast<std::size_t>(number);
  return false;
}

Cell named_stream(Impl& machine, const Goal& goal, std::size_t index) {
  if (goal.arity >= 1) return goal.arg(machine, 0);
  const Stream& stream = machine.streams[index];
  if (!stream.alias.empty()) return machine.heap.atom(machine.atoms.intern(stream.alias));
  return machine.heap.structure(machine.atoms.intern("$stream"),
                                std::array<Cell, 1>{machine.heap.integer(static_cast<std::int64_t>(index))});
}

bool save_output(std::size_t index, Stream& stream) {
  if (stream.mode == Stream::Mode::Read) return true;
  if (stream.kind == Stream::Kind::Standard) {
    if (index == 1) std::cout.flush();
    else if (index == 2) std::cerr.flush();
    return true;
  }
  if (stream.kind != Stream::Kind::File) return true;
  std::ofstream output(path_from_utf8(stream.path), std::ios::binary | std::ios::trunc);
  if (!output) return false;
  if (!stream.bytes.empty()) {
    output.write(stream.bytes.data(), static_cast<std::streamsize>(stream.bytes.size()));
  }
  return static_cast<bool>(output);
}

void mark_closed(Impl& machine, std::size_t index) {
  machine.streams[index].closed = true;
  // 閉じた流れが現在入出力なら、標準の別名に戻す。
  if (machine.current_in == index) machine.current_in = 0;
  if (machine.current_out == index) machine.current_out = 1;
}

bool reject_closed(Impl& machine, const Goal& goal, std::size_t index, Cell culprit) {
  if (!machine.streams[index].closed) return false;
  raise_formal(machine, formal_existence(machine, "stream", culprit), goal.name, goal.arity);
  return true;
}

bool close_force(Impl& machine, const Goal& goal, Cell option, bool& force) {
  option = machine.heap.deref(option);
  if (option.tag() != Tag::Str || machine.heap.functor_arity(option) != 1 ||
      machine.atoms.name(machine.heap.functor_atom(option)) != "force") {
    return raise_domain(machine, goal, "close_option", option);
  }
  const Cell arg = machine.heap.deref(machine.heap.arg(option, 0));
  if (machine.heap.unbound(arg)) return raise_inst(machine, goal);
  if (arg.tag() != Tag::Atom) return raise_domain(machine, goal, "close_option", option);
  const auto text = atom_text(machine, arg);
  if (text == "true") force = true;
  else if (text == "false") force = false;
  else return raise_domain(machine, goal, "close_option", option);
  return false;
}

bool reject_close_options(Impl& machine, const Goal& goal, bool& force) {
  if (goal.arity == 1) return false;
  const Cell options = machine.heap.deref(goal.arg(machine, 1));
  if (machine.heap.unbound(options)) return raise_inst(machine, goal);
  const WalkedList walked = walk_list(machine, options);
  if (walked.shape == ListShape::Variable || walked.shape == ListShape::Partial || item_unbound(machine, walked)) {
    return raise_inst(machine, goal);
  }
  if (walked.shape != ListShape::Proper) return raise_type(machine, goal, "list", options);
  for (const Cell item : walked.items) {
    if (close_force(machine, goal, item, force)) return true;
  }
  return false;
}

void run_flush(Impl& machine, const Goal& goal) {
  std::size_t index = machine.current_out;
  if (goal.arity == 1 && resolve_stream(machine, goal, goal.arg(machine, 0), index)) return;
  const Cell culprit = named_stream(machine, goal, index);
  if (reject_closed(machine, goal, index, culprit)) return;
  Stream& stream = machine.streams[index];
  if (stream.mode == Stream::Mode::Read) {
    raise_formal(machine, formal_permission(machine, "output", "stream", culprit), goal.name, goal.arity);
    return;
  }
  if (!save_output(index, stream)) {
    raise_formal(machine, formal_system(machine), goal.name, goal.arity);
    return;
  }
  machine.ip = goal.cont;
}

void run_close(Impl& machine, const Goal& goal) {
  bool force = false;
  if (goal.arity == 2 && machine.heap.unbound(machine.heap.deref(goal.arg(machine, 0)))) {
    raise_inst(machine, goal);
    return;
  }
  if (reject_close_options(machine, goal, force)) return;
  std::size_t index = 0;
  if (resolve_stream(machine, goal, goal.arg(machine, 0), index)) return;
  const Cell culprit = goal.arg(machine, 0);
  if (reject_closed(machine, goal, index, culprit)) return;
  if (!save_output(index, machine.streams[index])) {
    if (!force) {
      raise_formal(machine, formal_system(machine), goal.name, goal.arity);
      return;
    }
  }
  mark_closed(machine, index);
  machine.ip = goal.cont;
}

}  // namespace

void init_streams(Impl& machine) {
  machine.streams.push_back(standard_stream(Stream::Mode::Read, "user_input"));
  machine.streams.push_back(standard_stream(Stream::Mode::Write, "user_output"));
  machine.streams.push_back(standard_stream(Stream::Mode::Write, "user_error"));
  machine.current_in = 0;
  machine.current_out = 1;
}

bool run_streams(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "open" && (goal.arity == 3 || goal.arity == 4)) {
    run_open(machine, goal);
    return true;
  }
  if (name == "open_string" && goal.arity == 2) {
    run_open_string(machine, goal);
    return true;
  }
  if (name == "close" && (goal.arity == 1 || goal.arity == 2)) {
    run_close(machine, goal);
    return true;
  }
  if (name == "flush_output" && goal.arity <= 1) {
    run_flush(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
