// ^^ の表明のうち、書いた中身と読み残しを見るもの。lgt_io.cpp から分けた。

#include "lgt_io_priv.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace mosa::lgtio {
namespace {

Cell byte_list(Heap& heap, std::string_view bytes) {
  Cell tail = heap.atom(heap.atoms().intern("[]"));
  for (std::size_t index = bytes.size(); index > 0; --index) {
    const auto unit = static_cast<unsigned char>(bytes[index - 1]);
    tail = heap.list(heap.integer(static_cast<std::int64_t>(unit)), tail);
  }
  return tail;
}

Cell eq_cell(Heap& heap, Cell left, Cell right) {
  const std::array<Cell, 2> args{left, right};
  return heap.structure(heap.atoms().intern("=="), args);
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

}  // namespace

LgtSuper observe_contents(Machine& machine, const LgtIo& io, Cell term, const std::vector<VarName>& names) {
  const Heap& heap = machine.heap();
  int arity = 0;
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
  return {};
}

}  // namespace mosa::lgtio
