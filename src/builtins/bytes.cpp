// 規格 8.13.1 get_byte/1,2。8.13.3 put_byte/1,2。
// get_byte(+stream_or_alias, ?in_byte) / get_byte(?in_byte)
// put_byte(+stream_or_alias, +byte) / put_byte(+byte)
//   instantiation_error, domain_error(stream_or_alias), existence_error(stream),
//   type_error(in_byte), type_error(byte),
//   permission_error(input, stream), permission_error(input, text_stream),
//   permission_error(input, past_end_of_stream),
//   permission_error(output, stream), permission_error(output, text_stream)
// 終端の印は -1。テキストのストリームでは読まない。
// テスト: tests/pl/bytes.pl, tests/cpp/stream_test.cpp

#include "../machine/internal.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>

namespace mosa {
namespace {

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

bool is_integer(Cell cell) { return cell.tag() == Tag::Int || cell.tag() == Tag::IntBox; }

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
  if (!is_integer(arg)) return raise_domain(machine, goal, cell);
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
  return machine.heap.structure(machine.atoms.intern("$stream"),
                                std::array<Cell, 1>{machine.heap.integer(static_cast<std::int64_t>(index))});
}

bool reject_closed(Impl& machine, const Goal& goal, std::size_t index, Cell culprit) {
  if (!machine.streams[index].closed) return false;
  return raise_exists(machine, goal, culprit);
}

bool reject_byte_input(Impl& machine, const Goal& goal, const Stream& stream, Cell culprit) {
  if (stream.mode != Stream::Mode::Read) return raise_permit(machine, goal, "input", "stream", culprit);
  if (stream.data != Stream::Data::Binary) return raise_permit(machine, goal, "input", "text_stream", culprit);
  return false;
}

bool reject_byte_output(Impl& machine, const Goal& goal, const Stream& stream, Cell culprit) {
  if (stream.mode == Stream::Mode::Read) return raise_permit(machine, goal, "output", "stream", culprit);
  if (stream.data != Stream::Data::Binary) return raise_permit(machine, goal, "output", "text_stream", culprit);
  return false;
}

bool reject_in_byte(Impl& machine, const Goal& goal, Cell cell) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return false;
  if (!is_integer(cell)) return raise_type(machine, goal, "in_byte", cell);
  const auto value = machine.heap.integer_value(cell);
  if (value < -1 || value > 255) return raise_type(machine, goal, "in_byte", cell);
  return false;
}

bool reject_put_byte(Impl& machine, const Goal& goal, Cell cell, unsigned char& byte) {
  cell = machine.heap.deref(cell);
  if (!is_integer(cell)) return raise_type(machine, goal, "byte", cell);
  const auto value = machine.heap.integer_value(cell);
  if (value < 0 || value > 255) return raise_type(machine, goal, "byte", cell);
  byte = static_cast<unsigned char>(value);
  return false;
}

void rewind_stream(Stream& stream) {
  stream.cursor = 0;
  stream.char_no = 0;
  stream.line = 1;
  stream.line_pos = 1;
  stream.past_end = false;
}

bool pull_byte(Stream& stream) {
  if (stream.cursor < stream.bytes.size()) return true;
  if (stream.kind != Stream::Kind::Standard || stream.mode != Stream::Mode::Read) return false;
  const int unit = std::cin.get();
  if (unit == std::char_traits<char>::eof()) return false;
  stream.bytes.push_back(static_cast<char>(static_cast<unsigned char>(unit)));
  return true;
}

// バイナリの行番号は動かさない。文字数はバイトの個数。
void advance_byte(Stream& stream) {
  stream.cursor += 1;
  stream.char_no += 1;
  stream.past_end = false;
}

bool take_byte(Impl& machine, const Goal& goal, Stream& stream, Cell culprit, int& value) {
  if (stream.past_end && stream.eof_action == Stream::EofAction::Error) {
    return raise_permit(machine, goal, "input", "past_end_of_stream", culprit);
  }
  if (stream.past_end && stream.eof_action == Stream::EofAction::Reset) rewind_stream(stream);
  if (stream.past_end) {
    value = -1;
    return false;
  }
  if (!pull_byte(stream)) {
    value = -1;
    stream.past_end = true;
    return false;
  }
  value = static_cast<unsigned char>(stream.bytes[stream.cursor]);
  // 照合が失敗しても、読んだバイトは戻さない。
  advance_byte(stream);
  return false;
}

void finish_get(Impl& machine, const Goal& goal, Cell slot, int value) {
  if (!do_unify(machine, slot, machine.heap.integer(value))) machine.failed = true;
  else machine.ip = goal.cont;
}

void emit_os(std::size_t index, unsigned char byte) {
  if (index != 1 && index != 2) return;
  std::ostream& out = index == 2 ? std::cerr : std::cout;
  const char unit = static_cast<char>(byte);
  out.write(&unit, 1);
  out.flush();
}

void place_byte(Stream& stream, unsigned char byte) {
  if (stream.cursor > stream.bytes.size()) stream.cursor = stream.bytes.size();
  if (stream.cursor == stream.bytes.size()) stream.bytes.push_back(static_cast<char>(byte));
  else stream.bytes[stream.cursor] = static_cast<char>(byte);
  advance_byte(stream);
}

bool select_stream(Impl& machine, const Goal& goal, bool input, std::size_t& index) {
  if (goal.arity == 1) {
    index = input ? machine.current_in : machine.current_out;
    return false;
  }
  return resolve_stream(machine, goal, goal.arg(machine, 0), index);
}

void run_get_byte(Impl& machine, const Goal& goal) {
  std::size_t index = 0;
  if (select_stream(machine, goal, true, index)) return;
  const Cell culprit = stream_culprit(machine, goal, index);
  if (reject_closed(machine, goal, index, culprit) ||
      reject_byte_input(machine, goal, machine.streams[index], culprit)) {
    return;
  }
  const Cell slot = goal.arg(machine, goal.arity == 1 ? 0 : 1);
  if (reject_in_byte(machine, goal, slot)) return;
  int value = 0;
  if (take_byte(machine, goal, machine.streams[index], culprit, value)) return;
  finish_get(machine, goal, slot, value);
}

void run_put_byte(Impl& machine, const Goal& goal) {
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
  if (reject_closed(machine, goal, index, culprit) ||
      reject_byte_output(machine, goal, machine.streams[index], culprit)) {
    return;
  }
  unsigned char byte = 0;
  if (reject_put_byte(machine, goal, slot, byte)) return;
  Stream& stream = machine.streams[index];
  if (stream.kind == Stream::Kind::Standard) emit_os(index, byte);
  place_byte(stream, byte);
  machine.ip = goal.cont;
}

}  // namespace

bool run_byte_io(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "get_byte" && (goal.arity == 1 || goal.arity == 2)) {
    run_get_byte(machine, goal);
    return true;
  }
  if (name == "put_byte" && (goal.arity == 1 || goal.arity == 2)) {
    run_put_byte(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
