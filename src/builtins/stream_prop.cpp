// 規格 8.11.1 current_input/1。8.11.2 current_output/1。8.11.3 set_input/1。
// 8.11.4 set_output/1。8.11.8 stream_property/2、at_end_of_stream/0,1。
// 8.11.9 set_stream_position/2。
// current_input(?stream_or_alias) / current_output(?stream_or_alias)
//   変数は '$stream'(N)。別名は照合だけに使い、項の形は変えない。
//   domain_error(stream_or_alias), existence_error(stream)
// set_input(+stream_or_alias) / set_output(+stream_or_alias)
//   検査が済むまで current_in と current_out は変えない。
//   set_input は read だけ。set_output は write と append。
//   instantiation_error, domain_error(stream_or_alias), existence_error(stream),
//   permission_error(input または output, stream)
// at_end_of_stream / at_end_of_stream(+stream_or_alias)
//   8.11.8.5 は end_of_stream が at か past のとき成功する。
//   /0 の閉じた現在入力は existence_error(stream, '$stream'(N))。
//   /1 は instantiation_error, domain_error(stream_or_alias), existence_error(stream)
// set_stream_position(+stream_or_alias, +stream_position)
//   検査の順は具体化、流れの項、開いていること、位置の項、reposition、三つ組が流れの上にあること。
//   カーソルを動かすのは、その後。past_end は下ろす。
//   instantiation_error, domain_error(stream_or_alias), existence_error(stream),
//   domain_error(stream_position), permission_error(reposition, stream)
// stream_property(?stream_or_alias, ?stream_property)
//   domain_error(stream_or_alias), domain_error(stream_property), existence_error(stream)
// 性質の順は file_name, mode, alias, position, end_of_stream, eof_action,
// reposition, type, encoding。戻せない流れに position は出さない。
// テスト: tests/pl/stream_prop.pl, tests/cpp/stream_test.cpp

#include "../machine/internal.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {
namespace {

enum Prop { FileName, Mode, Alias, Position, End, Eof, Reposition, Type, Encoding, Count };

bool raise_domain(Impl& machine, const Goal& goal, std::string_view domain, Cell culprit) {
  raise_formal(machine, formal_domain(machine, domain, culprit), goal.name, goal.arity);
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
  if (cell.tag() == Tag::Atom) {
    if (!find_alias(machine, atom_text(machine, cell), index)) return raise_domain(machine, goal, "stream_or_alias", cell);
    return false;
  }
  const bool shaped = cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 1 &&
                      machine.atoms.name(machine.heap.functor_atom(cell)) == "$stream";
  if (!shaped) return raise_domain(machine, goal, "stream_or_alias", cell);
  const Cell arg = machine.heap.deref(machine.heap.arg(cell, 0));
  if (!is_integer(arg)) return raise_domain(machine, goal, "stream_or_alias", cell);
  const auto number = machine.heap.integer_value(arg);
  if (number < 0 || static_cast<std::uint64_t>(number) >= machine.streams.size()) {
    raise_formal(machine, formal_existence(machine, "stream", cell), goal.name, goal.arity);
    return true;
  }
  index = static_cast<std::size_t>(number);
  return false;
}

Cell stream_term(Impl& machine, std::size_t index) {
  return machine.heap.structure(machine.atoms.intern("$stream"),
                                std::array<Cell, 1>{machine.heap.integer(static_cast<std::int64_t>(index))});
}

Cell one_arg(Impl& machine, std::string_view name, Cell arg) {
  return machine.heap.structure(machine.atoms.intern(name), std::array<Cell, 1>{arg});
}

Cell atom_arg(Impl& machine, std::string_view name, std::string_view text) {
  return one_arg(machine, name, machine.heap.atom(machine.atoms.intern(text)));
}

std::string_view mode_name(Stream::Mode mode) {
  if (mode == Stream::Mode::Read) return "read";
  if (mode == Stream::Mode::Write) return "write";
  return "append";
}

std::string_view end_name(const Stream& stream) {
  if (stream.past_end) return "past";
  // 標準入力の長さは、まだ読んでいない先があるのでバッファでは決まらない。
  if (stream.mode != Stream::Mode::Read || stream.kind == Stream::Kind::Standard) return "not";
  if (stream.cursor >= stream.bytes.size()) return "at";
  return "not";
}

std::string_view eof_name(Stream::EofAction action) {
  if (action == Stream::EofAction::Error) return "error";
  if (action == Stream::EofAction::Reset) return "reset";
  return "eof_code";
}

bool has_prop(const Stream& stream, int prop) {
  if (prop == FileName) return stream.kind == Stream::Kind::File && !stream.path.empty();
  if (prop == Alias) return !stream.alias.empty();
  if (prop == Position) return stream.reposition;
  return true;
}

Cell prop_cell(Impl& machine, const Stream& stream, int prop) {
  switch (prop) {
    case FileName: return atom_arg(machine, "file_name", stream.path);
    case Mode: return atom_arg(machine, "mode", mode_name(stream.mode));
    case Alias: return atom_arg(machine, "alias", stream.alias);
    case End: return atom_arg(machine, "end_of_stream", end_name(stream));
    case Eof: return atom_arg(machine, "eof_action", eof_name(stream.eof_action));
    case Reposition: return atom_arg(machine, "reposition", stream.reposition ? "true" : "false");
    case Type: return atom_arg(machine, "type", stream.data == Stream::Data::Text ? "text" : "binary");
    case Encoding: return atom_arg(machine, "encoding", "utf8");
    default: break;
  }
  const std::array<Cell, 3> args{machine.heap.integer(static_cast<std::int64_t>(stream.char_no)),
                                 machine.heap.integer(static_cast<std::int64_t>(stream.line)),
                                 machine.heap.integer(static_cast<std::int64_t>(stream.line_pos))};
  const Cell position = machine.heap.structure(machine.atoms.intern("stream_position"), args);
  return one_arg(machine, "position", position);
}

int prop_kind(std::string_view name) {
  if (name == "file_name") return FileName;
  if (name == "mode") return Mode;
  if (name == "alias") return Alias;
  if (name == "position") return Position;
  if (name == "end_of_stream") return End;
  if (name == "eof_action") return Eof;
  if (name == "reposition") return Reposition;
  if (name == "type") return Type;
  if (name == "encoding") return Encoding;
  return -1;
}

bool atom_in(const Impl& machine, Cell cell, std::string_view a, std::string_view b = {}, std::string_view c = {}) {
  if (machine.heap.unbound(cell)) return true;
  if (cell.tag() != Tag::Atom) return false;
  const auto text = atom_text(machine, cell);
  return text == a || (!b.empty() && text == b) || (!c.empty() && text == c);
}

bool position_shape(const Impl& machine, Cell cell) {
  if (machine.heap.unbound(cell)) return true;
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 3) return false;
  if (machine.atoms.name(machine.heap.functor_atom(cell)) != "stream_position") return false;
  for (std::size_t index = 0; index < 3; ++index) {
    const Cell part = machine.heap.deref(machine.heap.arg(cell, index));
    if (!machine.heap.unbound(part) && !is_integer(part)) return false;
  }
  return true;
}

bool value_ok(const Impl& machine, int prop, Cell arg) {
  arg = machine.heap.deref(arg);
  switch (prop) {
    case FileName:
    case Alias: return machine.heap.unbound(arg) || arg.tag() == Tag::Atom;
    case Mode: return atom_in(machine, arg, "read", "write", "append");
    case End: return atom_in(machine, arg, "not", "at", "past");
    case Eof: return atom_in(machine, arg, "error", "eof_code", "reset");
    case Reposition: return atom_in(machine, arg, "true", "false");
    case Type: return atom_in(machine, arg, "text", "binary");
    case Encoding: return atom_in(machine, arg, "utf8");
    case Position: return position_shape(machine, arg);
    default: return false;
  }
}

bool reject_property(Impl& machine, const Goal& goal, Cell prop, int& kind) {
  if (machine.heap.unbound(prop)) {
    kind = -1;
    return false;
  }
  if (prop.tag() != Tag::Str || machine.heap.functor_arity(prop) != 1) {
    return raise_domain(machine, goal, "stream_property", prop);
  }
  kind = prop_kind(machine.atoms.name(machine.heap.functor_atom(prop)));
  if (kind < 0 || !value_ok(machine, kind, machine.heap.arg(prop, 0))) {
    return raise_domain(machine, goal, "stream_property", prop);
  }
  return false;
}

std::size_t pack_hit(std::size_t stream, int prop) {
  return stream * static_cast<std::size_t>(Count) + static_cast<std::size_t>(prop);
}

bool bind_hit(Impl& machine, const Goal& goal, std::size_t packed) {
  const auto stream = packed / static_cast<std::size_t>(Count);
  const auto prop = static_cast<int>(packed % static_cast<std::size_t>(Count));
  const Cell spec = machine.heap.deref(goal.arg(machine, 0));
  // 別名は照合済み。変数のときだけ '$stream'(N) を束縛する。
  if (machine.heap.unbound(spec) && !do_unify(machine, goal.arg(machine, 0), stream_term(machine, stream))) {
    return false;
  }
  return do_unify(machine, goal.arg(machine, 1), prop_cell(machine, machine.streams[stream], prop));
}

void push_gen(Impl& machine, const Goal& goal, const std::vector<std::size_t>& hits, std::size_t next,
              std::size_t heap_mark, std::size_t trail_mark) {
  Entry entry;
  entry.pick = Pick::StreamGen;
  entry.candidates = hits;
  entry.next = next;
  entry.redo_goal = goal.at;
  entry.heap_mark = heap_mark;
  entry.trail_mark = trail_mark;
  push_frame(machine, std::move(entry));
}

bool prove_props(Impl& machine, const Goal& goal, const std::vector<std::size_t>& hits, std::size_t begin) {
  for (std::size_t index = begin; index < hits.size(); ++index) {
    const auto heap_mark = machine.heap.size();
    const auto trail_mark = machine.trail.slots.size();
    const auto extras_mark = machine.extras.size();
    const bool more = index + 1 < hits.size();
    if (more) push_gen(machine, goal, hits, index + 1, heap_mark, trail_mark);
    if (bind_hit(machine, goal, hits[index])) {
      machine.ip = goal.cont;
      return true;
    }
    if (more) machine.stack.pop_back();
    restore_state(machine, heap_mark, trail_mark, extras_mark);
  }
  return false;
}

std::vector<std::size_t> collect_hits(const Impl& machine, bool one, std::size_t only, int kind) {
  std::vector<std::size_t> hits;
  for (std::size_t index = 0; index < machine.streams.size(); ++index) {
    if (machine.streams[index].closed) continue;
    if (one && index != only) continue;
    for (int prop = 0; prop < Count; ++prop) {
      if (kind >= 0 && prop != kind) continue;
      if (!has_prop(machine.streams[index], prop)) continue;
      hits.push_back(pack_hit(index, prop));
    }
  }
  return hits;
}

void match_current(Impl& machine, const Goal& goal, std::size_t current) {
  const Cell spec = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(spec)) {
    if (!do_unify(machine, goal.arg(machine, 0), stream_term(machine, current))) machine.failed = true;
    else machine.ip = goal.cont;
    return;
  }
  std::size_t index = 0;
  if (resolve_stream(machine, goal, spec, index)) return;
  if (index != current && machine.streams[index].closed) {
    raise_formal(machine, formal_existence(machine, "stream", spec), goal.name, goal.arity);
    return;
  }
  if (index != current) {
    machine.failed = true;
    return;
  }
  machine.ip = goal.cont;
}

struct Triple {
  std::size_t cursor = 0;
  std::size_t char_no = 0;
  std::size_t line = 1;
  std::size_t line_pos = 1;
};

enum class PosKind { Ok, Inst, Domain };

// note_end と同じ幅。戻した位置の文字数が、開いたときの数え方とずれない。
std::size_t text_width(std::string_view bytes, std::size_t index) {
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

PosKind read_position(Impl& machine, Cell cell, Triple& triple) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 3 ||
      machine.atoms.name(machine.heap.functor_atom(cell)) != "stream_position") {
    return PosKind::Domain;
  }
  std::array<std::size_t, 3> parts{};
  for (std::size_t index = 0; index < parts.size(); ++index) {
    const Cell part = machine.heap.deref(machine.heap.arg(cell, index));
    if (machine.heap.unbound(part)) return PosKind::Inst;
    if (!is_integer(part)) return PosKind::Domain;
    const auto value = machine.heap.integer_value(part);
    if (value < 0) return PosKind::Domain;
    parts[index] = static_cast<std::size_t>(value);
  }
  if (parts[1] == 0 || parts[2] == 0) return PosKind::Domain;
  triple.char_no = parts[0];
  triple.line = parts[1];
  triple.line_pos = parts[2];
  return PosKind::Ok;
}

bool locate(const Stream& stream, const Triple& want, std::size_t& cursor) {
  Triple spot;
  while (true) {
    if (spot.char_no == want.char_no && spot.line == want.line && spot.line_pos == want.line_pos) {
      cursor = spot.cursor;
      return true;
    }
    if (spot.cursor >= stream.bytes.size()) return false;
    if (stream.data == Stream::Data::Binary) {
      spot.cursor += 1;
      spot.char_no += 1;
      continue;
    }
    const std::size_t width = text_width(stream.bytes, spot.cursor);
    const bool newline = width == 1 && stream.bytes[spot.cursor] == '\n';
    spot.cursor += width;
    spot.char_no += 1;
    if (newline) {
      spot.line += 1;
      spot.line_pos = 1;
    } else {
      spot.line_pos += 1;
    }
  }
}

}  // namespace

void run_current_input(Impl& machine, const Goal& goal) { match_current(machine, goal, machine.current_in); }

void run_current_output(Impl& machine, const Goal& goal) { match_current(machine, goal, machine.current_out); }

void run_set_input(Impl& machine, const Goal& goal) {
  const Cell spec = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(spec)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return;
  }
  std::size_t index = 0;
  if (resolve_stream(machine, goal, spec, index)) return;
  if (machine.streams[index].closed) {
    raise_formal(machine, formal_existence(machine, "stream", spec), goal.name, goal.arity);
    return;
  }
  if (machine.streams[index].mode != Stream::Mode::Read) {
    raise_formal(machine, formal_permission(machine, "input", "stream", spec), goal.name, goal.arity);
    return;
  }
  machine.current_in = index;
  machine.ip = goal.cont;
}

void run_set_output(Impl& machine, const Goal& goal) {
  const Cell spec = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(spec)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return;
  }
  std::size_t index = 0;
  if (resolve_stream(machine, goal, spec, index)) return;
  if (machine.streams[index].closed) {
    raise_formal(machine, formal_existence(machine, "stream", spec), goal.name, goal.arity);
    return;
  }
  if (machine.streams[index].mode == Stream::Mode::Read) {
    raise_formal(machine, formal_permission(machine, "output", "stream", spec), goal.name, goal.arity);
    return;
  }
  machine.current_out = index;
  machine.ip = goal.cont;
}

void run_set_position(Impl& machine, const Goal& goal) {
  const Cell spec = machine.heap.deref(goal.arg(machine, 0));
  const Cell position = machine.heap.deref(goal.arg(machine, 1));
  if (machine.heap.unbound(spec) || machine.heap.unbound(position)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return;
  }
  std::size_t index = 0;
  if (resolve_stream(machine, goal, spec, index)) return;
  if (machine.streams[index].closed) {
    raise_formal(machine, formal_existence(machine, "stream", spec), goal.name, goal.arity);
    return;
  }
  Triple triple;
  const PosKind kind = read_position(machine, position, triple);
  if (kind == PosKind::Inst) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return;
  }
  if (kind == PosKind::Domain) {
    raise_formal(machine, formal_domain(machine, "stream_position", position), goal.name, goal.arity);
    return;
  }
  if (!machine.streams[index].reposition) {
    raise_formal(machine, formal_permission(machine, "reposition", "stream", spec), goal.name, goal.arity);
    return;
  }
  std::size_t cursor = 0;
  if (!locate(machine.streams[index], triple, cursor)) {
    raise_formal(machine, formal_domain(machine, "stream_position", position), goal.name, goal.arity);
    return;
  }
  Stream& stream = machine.streams[index];
  stream.cursor = cursor;
  stream.char_no = triple.char_no;
  stream.line = triple.line;
  stream.line_pos = triple.line_pos;
  stream.past_end = false;
  machine.ip = goal.cont;
}

void run_at_end(Impl& machine, const Goal& goal) {
  std::size_t index = machine.current_in;
  Cell spec{};
  if (goal.arity == 1) {
    spec = machine.heap.deref(goal.arg(machine, 0));
    if (machine.heap.unbound(spec)) {
      raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
      return;
    }
    if (resolve_stream(machine, goal, spec, index)) return;
  }
  if (machine.streams[index].closed) {
    // 8.11.8.5 は current_input の項を stream_property に渡す。
    if (goal.arity == 0) spec = stream_term(machine, index);
    raise_formal(machine, formal_existence(machine, "stream", spec), goal.name, goal.arity);
    return;
  }
  const auto end = end_name(machine.streams[index]);
  if (end != "at" && end != "past") {
    machine.failed = true;
    return;
  }
  machine.ip = goal.cont;
}

void run_stream_property(Impl& machine, const Goal& goal) {
  const Cell spec = machine.heap.deref(goal.arg(machine, 0));
  const Cell prop = machine.heap.deref(goal.arg(machine, 1));
  bool one = false;
  std::size_t only = 0;
  if (!machine.heap.unbound(spec)) {
    if (resolve_stream(machine, goal, spec, only)) return;
    if (machine.streams[only].closed) {
      raise_formal(machine, formal_existence(machine, "stream", spec), goal.name, goal.arity);
      return;
    }
    one = true;
  }
  int kind = -1;
  if (reject_property(machine, goal, prop, kind)) return;
  const auto hits = collect_hits(machine, one, only, kind);
  if (!prove_props(machine, goal, hits, 0)) machine.failed = true;
}

bool redo_stream_prop(Impl& machine) {
  if (machine.stack.empty() || machine.stack.back().pick != Pick::StreamGen) return false;
  apply_catches(machine, machine.stack.back());
  const Entry top = machine.stack.back();
  const Goal goal = read_goal(machine, top.redo_goal);
  machine.stack.pop_back();
  return prove_props(machine, goal, top.candidates, top.next);
}

}  // namespace mosa
