// ^^ の表明のうち、書いた中身と読み残しを見るもの。lgt_io.cpp から分けた。

#include "lgt_io_priv.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

// lgtunit は null デバイスを開いて set_output する。ここではどこにも書き出さないメモリの流れで代える。
LgtSuper suppress_obs(Machine& machine) {
  Impl& impl = engine_impl(machine);
  Stream made;
  made.kind = Stream::Kind::Memory;
  made.mode = Stream::Mode::Write;
  impl.streams.push_back(std::move(made));
  impl.current_out = impl.streams.size() - 1;
  return bound_ok();
}

Cell char_list(Heap& heap, std::string_view bytes) {
  std::vector<Cell> chars;
  std::size_t index = 0;
  while (index < bytes.size()) {
    const std::size_t width = scalar_width(bytes, index);
    chars.push_back(heap.atom(heap.atoms().intern(std::string{bytes.substr(index, width)})));
    index += width;
  }
  Cell tail = heap.atom(heap.atoms().intern("[]"));
  for (auto item = chars.rbegin(); item != chars.rend(); ++item) tail = heap.list(*item, tail);
  return tail;
}

// lgtunit と同じく、流れを閉じてから中身を文字のリストで返す。閉じれば現在の出力は user_output へ戻る。
LgtSuper contents_obs(Machine& machine, const LgtIo& io, Cell goal, int arity, const std::vector<VarName>& names) {
  Impl& impl = engine_impl(machine);
  const auto index = stream_at(impl, goal, arity == 1 ? 1 : 2, false);
  if (!index || *index >= impl.streams.size()) return bound_no();
  const Stream& stream = impl.streams[*index];
  if (stream.closed || stream.kind != Stream::Kind::File) return bound_no();
  if (arity == 3) {
    IoOpt opt;
    if (!read_options(machine.heap(), machine.heap().arg(goal, 2), opt)) return bound_no();
  }
  const std::string bytes = whole_bytes(stream);
  const auto path = stream.path.empty() ? locate_file(io, "test_output.text") : path_from_utf8(stream.path);
  if (!path.empty()) drop_file(impl, path);
  Heap& heap = machine.heap();
  const Cell slot = heap.arg(goal, arity == 1 ? 0 : 1);
  // text_output_contents(_) は中身を捨てるだけ。名前の無い変数には束縛を返さない。
  if (heap.unbound(heap.deref(slot)) && var_name(heap, slot, names).empty()) return bound_ok();
  return bind_slot(machine, slot, char_list(heap, bytes), names);
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
  if (named_goal(heap, term, "suppress_text_output", arity) && arity == 0) return suppress_obs(machine);
  if (named_goal(heap, term, "text_output_contents", arity) && arity >= 1 && arity <= 3) {
    return contents_obs(machine, io, term, arity, names);
  }
  return {};
}

}  // namespace mosa::lgtio
