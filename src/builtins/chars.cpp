// 規格 8.12.1 get_char/1,2、get_code/1,2。8.12.2 peek_char/1,2、peek_code/1,2。
// 8.12.3 put_char/1,2、put_code/1,2、nl/0,1。
// get_char(+stream_or_alias, ?in_character) / get_char(?in_character)
// peek_char(+stream_or_alias, ?in_character) / peek_char(?in_character)
// get_code(+stream_or_alias, ?in_character_code) / get_code(?in_character_code)
// peek_code(+stream_or_alias, ?in_character_code) / peek_code(?in_character_code)
//   終端は -1。peek は位置を変えない。流れを先に調べ、コードの型は消費の前。
// put_char(+stream_or_alias, +character) / put_char(+character)
// put_code(+stream_or_alias, +character_code) / put_code(+character_code)
// nl(@stream_or_alias) / nl
//   改行は U+000A 一つ。コードの具体化は流れより先、型は書き込む前。
//   instantiation_error, domain_error(stream_or_alias), existence_error(stream),
//   type_error(in_character), type_error(integer), type_error(character),
//   permission_error(input, stream), permission_error(input, binary_stream),
//   permission_error(input, past_end_of_stream),
//   permission_error(output, stream), permission_error(output, binary_stream),
//   representation_error(character), representation_error(in_character_code),
//   representation_error(character_code)
// invalid_utf8 が replace のときだけ、壊れた先頭 1 バイトを U+FFFD にする。既定の error は進めない。
// テスト: tests/pl/chars.pl, tests/pl/japanese.pl, tests/cpp/stream_test.cpp

#include "../machine/internal.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>

namespace mosa {
namespace {

enum class Step { Char, Need, Eof, Bad };

struct Got {
  Step step = Step::Bad;
  char32_t code = 0;
  std::size_t width = 0;
};

bool raise_inst(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
  return true;
}

bool raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
  return true;
}

bool raise_domain(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_domain(machine, "stream_or_alias", culprit), goal.name, goal.arity);
  return true;
}

bool raise_exists(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_existence(machine, "stream", culprit), goal.name, goal.arity);
  return true;
}

bool raise_permit(Impl& machine, const Goal& goal, std::string_view action, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_permission(machine, action, type, culprit), goal.name, goal.arity);
  return true;
}

std::string_view atom_text(const Impl& machine, Cell cell) {
  return machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3));
}

bool is_cont(unsigned char byte) { return (byte & 0xC0U) == 0x80U; }

Got classify(std::string_view bytes, std::size_t cursor) {
  Got got;
  if (cursor >= bytes.size()) {
    got.step = Step::Need;
    got.width = 1;
    return got;
  }
  const auto lead = static_cast<unsigned char>(bytes[cursor]);
  if (lead <= 0x7FU) {
    got.step = Step::Char;
    got.code = lead;
    got.width = 1;
    return got;
  }
  std::size_t width = 0;
  char32_t code = 0;
  if (lead >= 0xC2U && lead <= 0xDFU) {
    width = 2;
    code = lead & 0x1FU;
  } else if (lead >= 0xE0U && lead <= 0xEFU) {
    width = 3;
    code = lead & 0x0FU;
  } else if (lead >= 0xF0U && lead <= 0xF4U) {
    width = 4;
    code = lead & 0x07U;
  } else {
    return got;
  }
  if (cursor + width > bytes.size()) {
    got.step = Step::Need;
    got.width = width;
    return got;
  }
  for (std::size_t index = 1; index < width; ++index) {
    const auto unit = static_cast<unsigned char>(bytes[cursor + index]);
    if (!is_cont(unit)) return got;
    code = (code << 6) | (unit & 0x3FU);
  }
  const char32_t min = width == 2 ? 0x80U : width == 3 ? 0x800U : 0x10000U;
  if (code < min || code > 0x10FFFFU || (code >= 0xD800U && code <= 0xDFFFU)) return got;
  got.step = Step::Char;
  got.code = code;
  got.width = width;
  return got;
}

bool pull_byte(Stream& stream) {
  if (stream.kind != Stream::Kind::Standard || stream.mode != Stream::Mode::Read) return false;
  const int byte = std::cin.get();
  if (byte == std::char_traits<char>::eof()) return false;
  stream.bytes.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
  return true;
}

Got next_scalar(Stream& stream) {
  while (true) {
    Got got = classify(stream.bytes, stream.cursor);
    if (got.step != Step::Need) return got;
    if (pull_byte(stream)) continue;
    got.step = stream.cursor >= stream.bytes.size() ? Step::Eof : Step::Bad;
    return got;
  }
}

void advance(Stream& stream, char32_t code, std::size_t width) {
  stream.cursor += width;
  stream.char_no += 1;
  stream.past_end = false;
  if (code == U'\n') {
    stream.line += 1;
    stream.line_pos = 1;
  } else {
    stream.line_pos += 1;
  }
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
    if (!find_alias(machine, atom_text(machine, cell), index)) return raise_domain(machine, goal, cell);
    return false;
  }
  const bool shaped = cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 1 &&
                      machine.atoms.name(machine.heap.functor_atom(cell)) == "$stream";
  if (!shaped) return raise_domain(machine, goal, cell);
  const Cell arg = machine.heap.deref(machine.heap.arg(cell, 0));
  if (arg.tag() != Tag::Int && arg.tag() != Tag::IntBox) return raise_domain(machine, goal, cell);
  const auto number = machine.heap.integer_value(arg);
  if (number < 0 || static_cast<std::uint64_t>(number) >= machine.streams.size()) {
    return raise_exists(machine, goal, cell);
  }
  index = static_cast<std::size_t>(number);
  return false;
}

Cell stream_culprit(Impl& machine, const Goal& goal, std::size_t index) {
  if (goal.arity == 2) return goal.arg(machine, 0);
  const Stream& stream = machine.streams[index];
  if (!stream.alias.empty()) return machine.heap.atom(machine.atoms.intern(stream.alias));
  const std::string text = "$stream";
  const auto name = machine.atoms.intern(text);
  return machine.heap.structure(name, std::array<Cell, 1>{machine.heap.integer(static_cast<std::int64_t>(index))});
}

bool reject_closed(Impl& machine, const Goal& goal, std::size_t index, Cell culprit) {
  if (!machine.streams[index].closed) return false;
  return raise_exists(machine, goal, culprit);
}

bool reject_input(Impl& machine, const Goal& goal, const Stream& stream, Cell culprit) {
  if (stream.mode != Stream::Mode::Read) return raise_permit(machine, goal, "input", "stream", culprit);
  if (stream.data == Stream::Data::Binary) return raise_permit(machine, goal, "input", "binary_stream", culprit);
  return false;
}

bool reject_output(Impl& machine, const Goal& goal, const Stream& stream, Cell culprit) {
  if (stream.mode == Stream::Mode::Read) return raise_permit(machine, goal, "output", "stream", culprit);
  if (stream.data == Stream::Data::Binary) return raise_permit(machine, goal, "output", "binary_stream", culprit);
  return false;
}

bool reject_in_code(Impl& machine, const Goal& goal, Cell cell) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return false;
  if (cell.tag() != Tag::Int && cell.tag() != Tag::IntBox) return raise_type(machine, goal, "integer", cell);
  const auto value = machine.heap.integer_value(cell);
  if (value != -1 && !scalar_code(value)) {
    raise_formal(machine, formal_representation(machine, "in_character_code"), goal.name, goal.arity);
    return true;
  }
  return false;
}

bool reject_in_char(Impl& machine, const Goal& goal, Cell cell) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return false;
  char32_t code = 0;
  if (cell.tag() == Tag::Atom && (atom_text(machine, cell) == "end_of_file" || one_char_atom(machine, cell, code))) {
    return false;
  }
  return raise_type(machine, goal, "in_character", cell);
}

bool reject_put_code(Impl& machine, const Goal& goal, Cell cell, char32_t& code) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return raise_inst(machine, goal);
  if (cell.tag() != Tag::Int && cell.tag() != Tag::IntBox) return raise_type(machine, goal, "integer", cell);
  const auto value = machine.heap.integer_value(cell);
  if (!scalar_code(value)) {
    raise_formal(machine, formal_representation(machine, "character_code"), goal.name, goal.arity);
    return true;
  }
  code = static_cast<char32_t>(value);
  return false;
}

bool reject_put_char(Impl& machine, const Goal& goal, Cell cell, char32_t& code) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return raise_inst(machine, goal);
  if (!one_char_atom(machine, cell, code)) return raise_type(machine, goal, "character", cell);
  return false;
}

bool take_char(Impl& machine, const Goal& goal, Stream& stream, Cell culprit, bool peek, bool& eof, char32_t& code) {
  if (stream.past_end && stream.eof_action == Stream::EofAction::Error) {
    return raise_permit(machine, goal, "input", "past_end_of_stream", culprit);
  }
  if (stream.past_end && stream.eof_action == Stream::EofAction::Reset) reset_at_end(stream);
  if (stream.past_end) {
    eof = true;
    return false;
  }
  const Got got = next_scalar(stream);
  if (got.step == Step::Bad) {
    // replace は壊れた先頭 1 バイトだけを U+FFFD にする。peek は位置を変えない。
    if (!machine.utf8_replace) {
      raise_formal(machine, formal_representation(machine, "character"), goal.name, goal.arity);
      return true;
    }
    eof = false;
    code = U'\uFFFD';
    if (!peek) advance(stream, code, 1);
    return false;
  }
  if (got.step == Step::Eof) {
    eof = true;
    if (!peek) stream.past_end = true;
    return false;
  }
  eof = false;
  code = got.code;
  // peek は位置を変えない。get は読んだあとで、照合が失敗しても戻さない。
  if (!peek) advance(stream, got.code, got.width);
  return false;
}

void finish_get(Impl& machine, const Goal& goal, Cell slot, bool eof, char32_t code, bool as_code) {
  Cell value = machine.heap.integer(eof ? -1 : static_cast<std::int64_t>(code));
  if (!as_code) {
    value = eof ? machine.heap.atom(machine.atoms.intern("end_of_file"))
                : machine.heap.atom(machine.atoms.intern_char(code));
  }
  if (!do_unify(machine, slot, value)) machine.failed = true;
  else machine.ip = goal.cont;
}

void emit_os(std::size_t index, std::string_view chunk) {
  if (index != 1 && index != 2) return;
  std::ostream& out = index == 2 ? std::cerr : std::cout;
  out.write(chunk.data(), static_cast<std::streamsize>(chunk.size()));
  // パイプは全バッファになる。標準ストリームは put のたびに出す。
  out.flush();
}

void place_bytes(Stream& stream, std::string_view chunk) {
  if (stream.cursor > stream.bytes.size()) stream.cursor = stream.bytes.size();
  if (stream.cursor == stream.bytes.size()) {
    stream.bytes.append(chunk);
    return;
  }
  const Got got = classify(stream.bytes, stream.cursor);
  const std::size_t room = stream.bytes.size() - stream.cursor;
  const std::size_t old = got.step == Step::Char && got.width <= room ? got.width : 1;
  stream.bytes.replace(stream.cursor, old > room ? room : old, chunk);
}

bool select_stream(Impl& machine, const Goal& goal, bool input, std::size_t& index) {
  if (goal.arity == 1) {
    index = input ? machine.current_in : machine.current_out;
    return false;
  }
  return resolve_stream(machine, goal, goal.arg(machine, 0), index);
}

void run_get(Impl& machine, const Goal& goal, bool peek, bool as_code) {
  std::size_t index = 0;
  if (select_stream(machine, goal, true, index)) return;
  const Cell culprit = stream_culprit(machine, goal, index);
  if (reject_closed(machine, goal, index, culprit) || reject_input(machine, goal, machine.streams[index], culprit)) {
    return;
  }
  const Cell slot = goal.arg(machine, goal.arity == 1 ? 0 : 1);
  if (as_code) {
    if (reject_in_code(machine, goal, slot)) return;
  } else if (reject_in_char(machine, goal, slot)) {
    return;
  }
  bool eof = false;
  char32_t code = 0;
  if (take_char(machine, goal, machine.streams[index], culprit, peek, eof, code)) return;
  finish_get(machine, goal, slot, eof, code, as_code);
}

void write_char(Impl& machine, const Goal& goal, std::size_t index, char32_t code) {
  std::string chunk;
  append_utf8_code(chunk, code);
  Stream& stream = machine.streams[index];
  if (stream.kind == Stream::Kind::Standard) emit_os(index, chunk);
  place_bytes(stream, chunk);
  advance(stream, code, chunk.size());
  machine.ip = goal.cont;
}

void run_put(Impl& machine, const Goal& goal, bool as_code) {
  if (goal.arity == 2 && machine.heap.unbound(machine.heap.deref(goal.arg(machine, 0)))) {
    raise_inst(machine, goal);
    return;
  }
  const Cell slot = goal.arg(machine, goal.arity == 1 ? 0 : 1);
  if (machine.heap.unbound(machine.heap.deref(slot))) {
    raise_inst(machine, goal);
    return;
  }
  std::size_t index = 0;
  if (select_stream(machine, goal, false, index)) return;
  const Cell culprit = stream_culprit(machine, goal, index);
  if (reject_closed(machine, goal, index, culprit) || reject_output(machine, goal, machine.streams[index], culprit)) {
    return;
  }
  char32_t code = 0;
  if (as_code) {
    if (reject_put_code(machine, goal, slot, code)) return;
  } else if (reject_put_char(machine, goal, slot, code)) {
    return;
  }
  write_char(machine, goal, index, code);
}

void run_nl(Impl& machine, const Goal& goal) {
  std::size_t index = machine.current_out;
  Cell culprit = stream_culprit(machine, goal, index);
  if (goal.arity == 1) {
    culprit = goal.arg(machine, 0);
    if (resolve_stream(machine, goal, culprit, index)) return;
  }
  if (reject_closed(machine, goal, index, culprit) || reject_output(machine, goal, machine.streams[index], culprit)) {
    return;
  }
  write_char(machine, goal, index, U'\n');
}

}  // namespace

// eof_action(reset)。ファイルと記憶の流れは先頭へ戻す。標準入力は読んだ分を巻き戻さず、
// 終わりの印を下ろして OS からの続きを待つ（iso-notes 8.11 の決定 B）。端末は Ctrl-D の後も読める。
void reset_at_end(Stream& stream) {
  stream.past_end = false;
  if (stream.kind == Stream::Kind::Standard) {
    std::cin.clear();
    return;
  }
  stream.cursor = 0;
  stream.char_no = 0;
  stream.line = 1;
  stream.line_pos = 1;
}

bool run_char_io(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "get_char" && (goal.arity == 1 || goal.arity == 2)) {
    run_get(machine, goal, false, false);
    return true;
  }
  if (name == "peek_char" && (goal.arity == 1 || goal.arity == 2)) {
    run_get(machine, goal, true, false);
    return true;
  }
  if (name == "get_code" && (goal.arity == 1 || goal.arity == 2)) {
    run_get(machine, goal, false, true);
    return true;
  }
  if (name == "peek_code" && (goal.arity == 1 || goal.arity == 2)) {
    run_get(machine, goal, true, true);
    return true;
  }
  if (name == "put_char" && (goal.arity == 1 || goal.arity == 2)) {
    run_put(machine, goal, false);
    return true;
  }
  if (name == "put_code" && (goal.arity == 1 || goal.arity == 2)) {
    run_put(machine, goal, true);
    return true;
  }
  if (name == "nl" && goal.arity <= 1) {
    run_nl(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
