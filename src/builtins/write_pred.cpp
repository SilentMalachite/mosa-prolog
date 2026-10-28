// 規格 8.14.2 write_term/3,2。8.14.3 write/2,1。8.14.4 writeq/2,1。8.14.5 write_canonical/2,1。
// print/1,2 は portray/1 に節があり成功するときその書き出しを使う。無い、または失敗なら write と同じ。
// 複合項の引数は再び print する。portray/1 は組込にしない。write は portray を見ない。
// with_output_to/2 は ADR 0005。最初の成功だけ取り、現在出力は必ず戻す。
// atom/1 と codes/1 のために作ったメモリは、成功・失敗・例外・停止のあと閉じる。渡された流れは閉じない。
// write_term(+stream_or_alias, @term, +write_options_list)
// write_term(@term, +write_options_list) は現在の出力。
//   write と print は numbervars(true)。writeq は quoted(true) と numbervars(true)。
//   write_canonical は quoted(true) と ignore_ops(true)。write_term の既定は三つとも偽。
//   同じ論理オプションの値が食い違うときは domain_error。variable_names は一回。
//   エラー順は、流れの変数、選択肢の具体化、流れのドメイン、リスト型、
//   write_option、存在、出力許可、それから書き出す。
//   instantiation_error, domain_error(stream_or_alias), domain_error(write_option),
//   type_error(list), type_error(callable), type_error(atom),
//   existence_error(stream),
//   permission_error(output, stream), permission_error(output, binary_stream)
// テスト: tests/pl/write.pl, tests/pl/japanese.pl, tests/cpp/stream_test.cpp

#include "../machine/internal.hpp"

#include "mosa/char_class.hpp"
#include "mosa/writer.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {
namespace {

enum class Kind { Term, Write, Quoted, Canonical, Print };
enum class OptShape { Ok, Inst, Type };
enum class SinkKind { Atom, Codes, Stream };
enum class Ran { Ok, Fail, Threw, Halted };

struct Spec {
  bool has_stream = false;
  bool has_options = false;
  std::size_t term_at = 0;
  std::size_t options_at = 0;
};

struct Chosen {
  bool quoted = false;
  bool ignore_ops = false;
  bool numbervars = false;
  bool saw_quoted = false;
  bool saw_ignore = false;
  bool saw_number = false;
  bool saw_names = false;
  std::vector<VarName> names;
};

struct Sink {
  SinkKind kind = SinkKind::Stream;
  std::size_t index = 0;
  Cell slot{};
};

struct Snap {
  std::vector<Entry> stack;
  bool failed = false;
  bool exception = false;
  bool eval_abort = false;
  Cell ball{};
  std::size_t heap_mark = 0;
  std::size_t trail_mark = 0;
  std::size_t extras_mark = 0;
};

struct Scalar {
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

bool raise_domain(Impl& machine, const Goal& goal, std::string_view domain, Cell culprit) {
  raise_formal(machine, formal_domain(machine, domain, culprit), goal.name, goal.arity);
  return true;
}

bool raise_exists(Impl& machine, const Goal& goal, Cell culprit) {
  raise_formal(machine, formal_existence(machine, "stream", culprit), goal.name, goal.arity);
  return true;
}

bool raise_permit(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_permission(machine, "output", type, culprit), goal.name, goal.arity);
  return true;
}

std::string_view atom_text(const Impl& machine, Cell cell) {
  return machine.atoms.name(static_cast<std::uint32_t>(cell.raw() >> 3));
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
    if (!find_alias(machine, atom_text(machine, cell), index)) {
      return raise_domain(machine, goal, "stream_or_alias", cell);
    }
    return false;
  }
  const bool shaped = cell.tag() == Tag::Str && machine.heap.functor_arity(cell) == 1 &&
                      machine.atoms.name(machine.heap.functor_atom(cell)) == "$stream";
  if (!shaped) return raise_domain(machine, goal, "stream_or_alias", cell);
  const Cell arg = machine.heap.deref(machine.heap.arg(cell, 0));
  if (arg.tag() != Tag::Int && arg.tag() != Tag::IntBox) return raise_domain(machine, goal, "stream_or_alias", cell);
  const auto number = machine.heap.integer_value(arg);
  if (number < 0 || static_cast<std::uint64_t>(number) >= machine.streams.size()) return raise_exists(machine, goal, cell);
  index = static_cast<std::size_t>(number);
  return false;
}

Cell stream_named(Impl& machine, std::size_t index) {
  const Stream& stream = machine.streams[index];
  if (!stream.alias.empty()) return machine.heap.atom(machine.atoms.intern(stream.alias));
  return machine.heap.structure(machine.atoms.intern("$stream"),
                                std::vector<Cell>{machine.heap.integer(static_cast<std::int64_t>(index))});
}

bool reject_closed(Impl& machine, const Goal& goal, std::size_t index, Cell culprit) {
  if (!machine.streams[index].closed) return false;
  return raise_exists(machine, goal, culprit);
}

bool reject_output(Impl& machine, const Goal& goal, const Stream& stream, Cell culprit) {
  if (stream.mode == Stream::Mode::Read) return raise_permit(machine, goal, "stream", culprit);
  if (stream.data == Stream::Data::Binary) return raise_permit(machine, goal, "binary_stream", culprit);
  return false;
}

bool scalar_at(std::string_view text, std::size_t index, Scalar& out) {
  if (index >= text.size()) return false;
  const auto lead = static_cast<unsigned char>(text[index]);
  if (lead <= 0x7FU) {
    out = Scalar{lead, 1};
    return true;
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
    return false;
  }
  if (index + width > text.size()) return false;
  for (std::size_t step = 1; step < width; ++step) {
    const auto unit = static_cast<unsigned char>(text[index + step]);
    if ((unit & 0xC0U) != 0x80U) return false;
    code = (code << 6) | (unit & 0x3FU);
  }
  out = Scalar{code, width};
  return true;
}

void place_one(Stream& stream, std::string_view chunk) {
  if (stream.cursor > stream.bytes.size()) stream.cursor = stream.bytes.size();
  if (stream.cursor == stream.bytes.size()) {
    stream.bytes.append(chunk);
    return;
  }
  Scalar old;
  const std::size_t width = scalar_at(stream.bytes, stream.cursor, old) ? old.width : 1;
  const std::size_t room = stream.bytes.size() - stream.cursor;
  stream.bytes.replace(stream.cursor, width > room ? room : width, chunk);
}

void advance_out(Stream& stream, char32_t code, std::size_t width) {
  stream.cursor += width;
  stream.char_no += 1;
  stream.past_end = false;
  if (code == U'\n') {
    stream.line += 1;
    stream.line_pos = 1;
    return;
  }
  stream.line_pos += 1;
}

void emit_text(Impl& machine, std::size_t index, std::string_view text) {
  Stream& stream = machine.streams[index];
  for (std::size_t at = 0; at < text.size();) {
    Scalar scalar;
    if (!scalar_at(text, at, scalar)) return;
    place_one(stream, text.substr(at, scalar.width));
    advance_out(stream, scalar.code, scalar.width);
    at += scalar.width;
  }
  // 項は一回で出す。パイプが全バッファでも、標準ストリームはここで見える。
  if (stream.kind != Stream::Kind::Standard || (index != 1 && index != 2)) return;
  std::ostream& out = index == 2 ? std::cerr : std::cout;
  out.write(text.data(), static_cast<std::streamsize>(text.size()));
  out.flush();
}

Spec spec_of(const Goal& goal, bool term_pred) {
  Spec spec;
  if (term_pred && goal.arity == 3) {
    spec.has_stream = true;
    spec.has_options = true;
    spec.term_at = 1;
    spec.options_at = 2;
    return spec;
  }
  if (term_pred) {
    spec.has_options = true;
    spec.options_at = 1;
    return spec;
  }
  if (goal.arity == 2) {
    spec.has_stream = true;
    spec.term_at = 1;
  }
  return spec;
}

OptShape scan_options(const Impl& machine, Cell options, std::vector<Cell>& items) {
  const WalkedList walked = walk_list(machine, options);
  if (walked.shape == ListShape::Variable || walked.shape == ListShape::Partial) return OptShape::Inst;
  if (walked.shape != ListShape::Proper) return OptShape::Type;
  for (Cell item : walked.items) {
    if (machine.heap.unbound(machine.heap.deref(item))) return OptShape::Inst;
  }
  items = walked.items;
  return OptShape::Ok;
}

bool compound_named(const Impl& machine, Cell cell, std::string_view& name, Cell& arg) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 1) return false;
  name = machine.atoms.name(machine.heap.functor_atom(cell));
  arg = machine.heap.arg(cell, 0);
  return true;
}

int flag_bit(const Impl& machine, Cell arg, bool& value) {
  arg = machine.heap.deref(arg);
  if (machine.heap.unbound(arg)) return -1;
  if (arg.tag() != Tag::Atom) return 0;
  const auto text = atom_text(machine, arg);
  if (text != "true" && text != "false") return 0;
  value = text == "true";
  return 1;
}

bool assign_bool(Impl& machine, const Goal& goal, Cell item, bool& flag, bool& seen, bool value) {
  if (seen && flag != value) return raise_domain(machine, goal, "write_option", item);
  flag = value;
  seen = true;
  return false;
}

bool variable_spelling(std::string_view text) {
  const CodeText codes = decode_codes(text);
  if (codes.codes.empty()) return false;
  const CharClass first = classify(codes.codes[0]);
  if (first != CharClass::Capital && first != CharClass::Underline) return false;
  for (std::size_t index = 1; index < codes.codes.size(); ++index) {
    const CharClass kind = classify(codes.codes[index]);
    const bool name = kind == CharClass::Small || kind == CharClass::Capital || kind == CharClass::Underline ||
                      kind == CharClass::Digit || kind == CharClass::Mark;
    if (!name) return false;
  }
  return true;
}

bool take_pair(Impl& machine, const Goal& goal, Cell item, std::vector<VarName>& names) {
  item = machine.heap.deref(item);
  if (machine.heap.unbound(item)) return raise_inst(machine, goal);
  const bool pair = item.tag() == Tag::Str && machine.heap.functor_arity(item) == 2 &&
                    machine.atoms.name(machine.heap.functor_atom(item)) == "=";
  if (!pair) return raise_domain(machine, goal, "write_option", item);
  const Cell name = machine.heap.deref(machine.heap.arg(item, 0));
  const Cell var = machine.heap.deref(machine.heap.arg(item, 1));
  if (machine.heap.unbound(name)) return raise_inst(machine, goal);
  if (name.tag() != Tag::Atom || !variable_spelling(atom_text(machine, name))) {
    return raise_domain(machine, goal, "write_option", item);
  }
  // 束縛済みは Cor.3 で無視する。未束縛だけ名前を付ける。
  if (!machine.heap.unbound(var)) return false;
  names.push_back(VarName{std::string{atom_text(machine, name)}, var});
  return false;
}

bool take_names(Impl& machine, const Goal& goal, Cell option, Cell arg, Chosen& chosen) {
  if (chosen.saw_names) return raise_domain(machine, goal, "write_option", option);
  const WalkedList walked = walk_list(machine, arg);
  if (walked.shape == ListShape::Variable || walked.shape == ListShape::Partial) return raise_inst(machine, goal);
  if (walked.shape != ListShape::Proper) return raise_domain(machine, goal, "write_option", option);
  for (Cell item : walked.items) {
    if (take_pair(machine, goal, item, chosen.names)) return true;
  }
  chosen.saw_names = true;
  return false;
}

bool apply_flag(Impl& machine, const Goal& goal, Cell item, std::string_view name, Cell arg, Chosen& chosen) {
  bool value = false;
  const int bit = flag_bit(machine, arg, value);
  if (bit < 0) return raise_inst(machine, goal);
  if (bit == 0) return raise_domain(machine, goal, "write_option", item);
  if (name == "quoted") return assign_bool(machine, goal, item, chosen.quoted, chosen.saw_quoted, value);
  if (name == "ignore_ops") return assign_bool(machine, goal, item, chosen.ignore_ops, chosen.saw_ignore, value);
  return assign_bool(machine, goal, item, chosen.numbervars, chosen.saw_number, value);
}

bool apply_option(Impl& machine, const Goal& goal, Cell item, Chosen& chosen) {
  std::string_view name;
  Cell arg{};
  if (!compound_named(machine, item, name, arg)) return raise_domain(machine, goal, "write_option", item);
  if (name == "variable_names") return take_names(machine, goal, item, arg, chosen);
  if (name == "quoted" || name == "ignore_ops" || name == "numbervars") {
    return apply_flag(machine, goal, item, name, arg, chosen);
  }
  return raise_domain(machine, goal, "write_option", item);
}

void preset(Kind kind, Chosen& chosen) {
  if (kind == Kind::Write || kind == Kind::Print) chosen.numbervars = true;
  if (kind == Kind::Quoted) {
    chosen.quoted = true;
    chosen.numbervars = true;
  }
  if (kind == Kind::Canonical) {
    chosen.quoted = true;
    chosen.ignore_ops = true;
  }
}

bool prepare_out(Impl& machine, const Goal& goal, const Spec& spec, Kind kind, std::size_t& index,
                 Chosen& chosen) {
  if (spec.has_stream && machine.heap.unbound(machine.heap.deref(goal.arg(machine, 0)))) return raise_inst(machine, goal);
  std::vector<Cell> items;
  OptShape shape = OptShape::Ok;
  if (spec.has_options) shape = scan_options(machine, goal.arg(machine, spec.options_at), items);
  if (shape == OptShape::Inst) return raise_inst(machine, goal);
  index = machine.current_out;
  if (spec.has_stream && resolve_stream(machine, goal, goal.arg(machine, 0), index)) return true;
  if (shape == OptShape::Type) return raise_type(machine, goal, "list", goal.arg(machine, spec.options_at));
  if (spec.has_options) {
    for (Cell item : items) {
      if (apply_option(machine, goal, item, chosen)) return true;
    }
  } else {
    preset(kind, chosen);
  }
  const Cell culprit = spec.has_stream ? goal.arg(machine, 0) : stream_named(machine, index);
  return reject_closed(machine, goal, index, culprit) || reject_output(machine, goal, machine.streams[index], culprit);
}

struct StreamSpot {
  std::size_t bytes = 0;
  std::size_t cursor = 0;
  std::size_t char_no = 0;
  std::size_t line = 1;
  std::size_t line_pos = 1;
  bool past_end = false;
};

struct PortrayHook {
  Impl* machine = nullptr;
  std::uint32_t name = 0;
  std::size_t scratch = 0;
  bool have_scratch = false;
  bool stop = false;

  PortrayHook() = default;
  PortrayHook(const PortrayHook&) = delete;
  PortrayHook& operator=(const PortrayHook&) = delete;

  ~PortrayHook() {
    if (have_scratch && machine != nullptr) machine->streams[scratch].closed = true;
  }
};

StreamSpot spot_of(const Stream& stream) {
  return StreamSpot{stream.bytes.size(), stream.cursor, stream.char_no, stream.line, stream.line_pos,
                    stream.past_end};
}

void rewind_spot(Stream& stream, const StreamSpot& spot) {
  stream.bytes.resize(spot.bytes);
  stream.cursor = spot.cursor;
  stream.char_no = spot.char_no;
  stream.line = spot.line;
  stream.line_pos = spot.line_pos;
  stream.past_end = spot.past_end;
}

bool portray_ready(const Impl& machine, std::uint32_t& name) {
  for (const Predicate& predicate : machine.predicates) {
    if (predicate.arity != 1 || predicate.clauses.empty()) continue;
    if (machine.atoms.name(predicate.name) != "portray") continue;
    name = predicate.name;
    return true;
  }
  return false;
}

IrTerm portray_call(std::uint32_t name) {
  // 引数は変数スロットのまま渡す。data に通すと単項のマイナスが数に畳まれる。
  IrTerm arg;
  arg.tag = IrTag::Var;
  arg.slot = 0;
  IrTerm goal;
  goal.tag = IrTag::Fun;
  goal.atom = name;
  goal.arity = 1;
  goal.args.push_back(arg);
  return goal;
}

std::size_t push_memory(Impl& machine);
Ran run_goal(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env, std::uint32_t module);

WriteCut apply_portray(void* raw, Cell term, std::string& out) {
  auto* hook = static_cast<PortrayHook*>(raw);
  if (hook->stop) return WriteCut::Stop;
  Impl& machine = *hook->machine;
  if (!hook->have_scratch) {
    hook->scratch = push_memory(machine);
    hook->have_scratch = true;
  }
  const StreamSpot spot = spot_of(machine.streams[hook->scratch]);
  const std::size_t saved = machine.current_out;
  // write/1 は現在出力へ書く。親の字面へ挟むため、成功分だけメモリから戻す。
  machine.current_out = hook->scratch;
  // portray/1 は user の述語を探して呼ぶ（portray_ready）。
  const Ran ran = run_goal(machine, portray_call(hook->name), std::vector<Cell>{term}, machine.user_module);
  machine.current_out = saved;
  // portray の push_back が vector を伸ばすと、呼ぶ前の参照は無効になる。
  Stream& stream = machine.streams[hook->scratch];
  if (ran == Ran::Ok) out = stream.bytes.substr(spot.bytes);
  // ゴール全体が失敗した書き出しを残すと、続く write の字面と重なる。
  rewind_spot(stream, spot);
  if (ran == Ran::Ok) return WriteCut::Swap;
  if (ran == Ran::Fail) {
    machine.failed = false;
    return WriteCut::Keep;
  }
  hook->stop = true;
  return WriteCut::Stop;
}

// 演算子は呼び手の文脈モジュールの見え方で書く（ADR 0006）。
bool emit_written(Impl& machine, std::uint32_t module, std::size_t index, Cell term, const WriteOptions& options,
                  const std::vector<VarName>& names, Kind kind) {
  // print は portray/1 で Prolog を走らせ、モジュール表が動くことがある。固有層は写してから見る。
  // machine.operators は Impl の中にあり、場所が変わらないので写さない。
  const Module* found = module == machine.user_module ? nullptr : find_module(machine, module);
  const OperatorTable local = found == nullptr ? OperatorTable{} : found->ops;
  const OperatorView view = found == nullptr ? OperatorView{machine.operators} : OperatorView{machine.operators, &local};
  std::uint32_t name = 0;
  if (kind == Kind::Print && portray_ready(machine, name)) {
    PortrayHook hook;
    hook.machine = &machine;
    hook.name = name;
    const std::string text =
        write_term(machine.heap, view, term, options, names, apply_portray, &hook);
    if (hook.stop) return false;
    emit_text(machine, index, text);
    return true;
  }
  emit_text(machine, index, write_term(machine.heap, view, term, options, names));
  return true;
}

void run_out(Impl& machine, const Goal& goal, Kind kind) {
  const Spec spec = spec_of(goal, kind == Kind::Term);
  std::size_t index = 0;
  Chosen chosen;
  if (prepare_out(machine, goal, spec, kind, index, chosen)) return;
  WriteOptions options;
  options.quoted = chosen.quoted;
  options.ignore_ops = chosen.ignore_ops;
  options.numbervars = chosen.numbervars;
  if (!emit_written(machine, goal.module, index, goal.arg(machine, spec.term_at), options, chosen.names, kind)) return;
  machine.ip = goal.cont;
}

bool callable_cell(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  return cell.tag() == Tag::Atom || cell.tag() == Tag::Str || cell.tag() == Tag::Lis;
}

bool reject_goal(Impl& machine, const Goal& goal, Cell cell) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) return raise_inst(machine, goal);
  if (!callable_cell(machine, cell)) return raise_type(machine, goal, "callable", cell);
  const Compiled compiled = compile_goal(machine.heap, cell);
  if (!body_callable(compiled.term)) return raise_type(machine, goal, "callable", cell);
  return false;
}

int prepare_capture(Impl& machine, const Goal& goal, Cell cell, Sink& sink) {
  std::string_view name;
  Cell arg{};
  if (!compound_named(machine, cell, name, arg)) return 0;
  if (name != "atom" && name != "codes") return 0;
  arg = machine.heap.deref(arg);
  if (name == "atom") {
    if (!machine.heap.unbound(arg) && arg.tag() != Tag::Atom) {
      raise_type(machine, goal, "atom", arg);
      return 1;
    }
    sink.kind = SinkKind::Atom;
    sink.slot = arg;
    return 2;
  }
  if (!machine.heap.unbound(arg)) {
    const WalkedList walked = walk_list(machine, arg);
    if (walked.shape == ListShape::Partial) {
      raise_inst(machine, goal);
      return 1;
    }
    if (walked.shape != ListShape::Proper) {
      raise_type(machine, goal, "list", arg);
      return 1;
    }
  }
  sink.kind = SinkKind::Codes;
  sink.slot = arg;
  return 2;
}

bool prepare_with(Impl& machine, const Goal& goal, Sink& sink) {
  const Cell sink_cell = machine.heap.deref(goal.arg(machine, 0));
  if (machine.heap.unbound(sink_cell)) return raise_inst(machine, goal);
  if (reject_goal(machine, goal, goal.arg(machine, 1))) return true;
  const int captured = prepare_capture(machine, goal, sink_cell, sink);
  if (captured == 1) return true;
  if (captured == 2) return false;
  if (resolve_stream(machine, goal, sink_cell, sink.index)) return true;
  const Cell culprit = goal.arg(machine, 0);
  return reject_closed(machine, goal, sink.index, culprit) ||
         reject_output(machine, goal, machine.streams[sink.index], culprit);
}

Snap take_snap(const Impl& machine) {
  Snap saved;
  saved.stack = machine.stack;
  saved.failed = machine.failed;
  saved.exception = machine.exception;
  saved.eval_abort = machine.eval_abort;
  saved.ball = machine.ball;
  saved.heap_mark = machine.heap.size();
  saved.trail_mark = machine.trail.slots.size();
  saved.extras_mark = machine.extras.size();
  return saved;
}

void disarm(Impl& machine, std::size_t floor) {
  for (std::size_t index = 0; index < floor; ++index) {
    if (machine.stack[index].pick == Pick::Catch) machine.stack[index].armed = false;
  }
}

void trim_extras(Impl& machine, std::size_t mark) {
  if (machine.extras.size() > mark) machine.extras.resize(mark);
}

Ran run_goal(Impl& machine, const IrTerm& goal, const std::vector<Cell>& env, std::uint32_t module) {
  const Snap saved = take_snap(machine);
  disarm(machine, machine.stack.size());
  Entry barrier;
  barrier.pick = Pick::Barrier;
  barrier.heap_mark = saved.heap_mark;
  barrier.trail_mark = saved.trail_mark;
  barrier.extras_mark = saved.extras_mark;
  push_frame(machine, barrier);
  machine.failed = false;
  machine.exception = false;
  machine.eval_abort = false;
  launch(machine, goal, kNoFrame, machine.stack.size(), env, module);
  const Stop stop = run_loop(machine);
  machine.stack = saved.stack;
  trim_extras(machine, saved.extras_mark);
  if (stop.kind == StopKind::Exception) {
    const ExportedTerm ball = export_term(machine, machine.ball);
    machine.failed = saved.failed;
    machine.exception = false;
    machine.eval_abort = saved.eval_abort;
    propagate_throw(machine, import_term(machine, ball));
    return Ran::Threw;
  }
  if (stop.kind == StopKind::Halt) {
    machine.failed = false;
    machine.exception = false;
    machine.halt_now = true;
    return Ran::Halted;
  }
  if (stop.kind != StopKind::Success) {
    machine.failed = true;
    machine.exception = saved.exception;
    machine.eval_abort = saved.eval_abort;
    machine.ball = saved.ball;
    return Ran::Fail;
  }
  machine.failed = false;
  machine.exception = false;
  machine.eval_abort = false;
  return Ran::Ok;
}

std::size_t push_memory(Impl& machine) {
  Stream made;
  made.kind = Stream::Kind::Memory;
  made.data = Stream::Data::Text;
  made.mode = Stream::Mode::Write;
  made.reposition = true;
  machine.streams.push_back(std::move(made));
  return machine.streams.size() - 1;
}

void bind_capture(Impl& machine, const Goal& goal, const Sink& sink, std::size_t index) {
  const std::string& bytes = machine.streams[index].bytes;
  Cell value = machine.heap.atom(machine.atoms.intern(bytes));
  if (sink.kind == SinkKind::Codes) {
    const CodeText codes = decode_codes(bytes);
    std::vector<Cell> items;
    items.reserve(codes.codes.size());
    for (const char32_t code : codes.codes) items.push_back(machine.heap.integer(static_cast<std::int64_t>(code)));
    value = list_from_cells(machine, items);
  }
  if (!do_unify(machine, sink.slot, value)) {
    machine.failed = true;
    return;
  }
  machine.ip = goal.cont;
}

void finish_with(Impl& machine, const Goal& goal, const Sink& sink, std::size_t index, Ran ran) {
  if (ran == Ran::Ok) {
    if (sink.kind == SinkKind::Stream) machine.ip = goal.cont;
    else bind_capture(machine, goal, sink, index);
  }
  // 捕獲用のメモリは呼んだ側の流れではない。開いたまま残すと stream_property が拾い、
  // その後も書き込める。バイトを束縛してから閉じ、スロットは再利用しない。
  if (sink.kind != SinkKind::Stream) machine.streams[index].closed = true;
}

void run_with(Impl& machine, const Goal& goal) {
  Sink sink;
  if (prepare_with(machine, goal, sink)) return;
  const std::size_t saved_out = machine.current_out;
  std::size_t index = sink.index;
  if (sink.kind != SinkKind::Stream) index = push_memory(machine);
  machine.current_out = index;
  const Compiled compiled = compile_goal(machine.heap, machine.heap.deref(goal.arg(machine, 1)));
  const Ran ran = run_goal(machine, compiled.term, compiled.env, goal.module);
  machine.current_out = saved_out;
  finish_with(machine, goal, sink, index, ran);
}

}  // namespace

bool run_write(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "with_output_to" && goal.arity == 2) {
    run_with(machine, goal);
    return true;
  }
  if (name == "write_term" && (goal.arity == 2 || goal.arity == 3)) {
    run_out(machine, goal, Kind::Term);
    return true;
  }
  if (name == "write" && (goal.arity == 1 || goal.arity == 2)) {
    run_out(machine, goal, Kind::Write);
    return true;
  }
  if (name == "print" && (goal.arity == 1 || goal.arity == 2)) {
    run_out(machine, goal, Kind::Print);
    return true;
  }
  if (name == "writeq" && (goal.arity == 1 || goal.arity == 2)) {
    run_out(machine, goal, Kind::Quoted);
    return true;
  }
  if (name == "write_canonical" && (goal.arity == 1 || goal.arity == 2)) {
    run_out(machine, goal, Kind::Canonical);
    return true;
  }
  return false;
}

}  // namespace mosa
