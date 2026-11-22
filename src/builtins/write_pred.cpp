// 規格 8.14.2 write_term/3,2。8.14.3 write/2,1。8.14.4 writeq/2,1。8.14.5 write_canonical/2,1。
// print/1,2 は portray/1 に節があり成功するときその書き出しを使う。無い、または失敗なら write と同じ。
// 複合項の引数は再び print する。portray/1 は組込にしない。write は portray を見ない。
// portray/1 の中の halt では、それまでの字面（portray が現在出力へ書いた分を含む）を書いてから止まる（レビュー L14）。
// 例外では、その print の分を何も書かない。
// with_output_to/2 は write_capture.cpp にある（portray/1 の道と run_goal、OutputScope を分け合う）。portray/1 の道と
// run_goal の本体は write_portray.cpp（800 行の上限で分けた）。
// write_term(+stream_or_alias, @term, +write_options_list)
// write_term(@term, +write_options_list) は現在の出力。
//   write と print は numbervars(true)。writeq は quoted(true) と numbervars(true)。
//   write_canonical は quoted(true) と ignore_ops(true)。write_term の既定は三つとも偽。
//   同じ選択肢を繰り返したら後の方が勝つ（iso-notes 8.14 Write options の決定 B）。どれも検査はする。
//   エラー順は、流れの変数、選択肢の具体化、流れのドメイン、リスト型、
//   write_option、存在、出力許可、それから書き出す。
//   max_depth(N) は 13211-1 に無い（ADR 0028、iso-notes のデバッガの節）。N は 0 以上の整数で、0 は切らない。
//   instantiation_error, domain_error(stream_or_alias), domain_error(write_option),
//   type_error(list),
//   existence_error(stream),
//   permission_error(output, stream), permission_error(output, binary_stream)
// テスト: tests/pl/write.pl, tests/pl/print.pl, tests/pl/japanese.pl, tests/cpp/stream_test.cpp,
//   tests/cpp/portray_halt_test.cpp, tests/pl/bigint.pl, tests/pl/print_module.pl

#include "write_internal.hpp"

#include "../machine/internal.hpp"

#include "mosa/char_class.hpp"
#include "mosa/writer.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
using namespace write_internal;
namespace {

enum class Kind { Term, Write, Quoted, Canonical, Print };
enum class OptShape { Ok, Inst, Type };

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
  int max_depth = 0;
  std::vector<VarName> names;
};

struct Scalar {
  char32_t code = 0;
  std::size_t width = 0;
};

}  // namespace

namespace write_internal {

bool raise_inst(Impl& machine, const Goal& goal) {
  raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
  return true;
}

bool raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit) {
  raise_formal(machine, formal_type(machine, type, culprit), goal.name, goal.arity);
  return true;
}

}  // namespace write_internal

namespace {

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

}  // namespace

namespace write_internal {

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
  if (!is_integer_tag(arg.tag())) return raise_domain(machine, goal, "stream_or_alias", cell);
  const auto number = stream_number(machine, arg);
  if (!number) return raise_exists(machine, goal, cell);
  index = *number;
  return false;
}

}  // namespace write_internal

namespace {

Cell stream_named(Impl& machine, std::size_t index) {
  const Stream& stream = machine.streams[index];
  if (stream.alias) return machine.heap.atom(machine.atoms.intern(*stream.alias));
  return machine.heap.structure(machine.atoms.intern("$stream"),
                                std::vector<Cell>{machine.heap.integer(static_cast<std::int64_t>(index))});
}

}  // namespace

namespace write_internal {

bool reject_closed(Impl& machine, const Goal& goal, std::size_t index, Cell culprit) {
  if (!machine.streams[index].closed) return false;
  return raise_exists(machine, goal, culprit);
}

bool reject_output(Impl& machine, const Goal& goal, const Stream& stream, Cell culprit) {
  if (stream.mode == Stream::Mode::Read) return raise_permit(machine, goal, "stream", culprit);
  if (stream.data == Stream::Data::Binary) return raise_permit(machine, goal, "binary_stream", culprit);
  return false;
}

}  // namespace write_internal

namespace {

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
  // 過剰な列とサロゲートと範囲外は Unicode スカラー値ではない（chars.cpp の decode とそろえる）。
  const char32_t min = width == 2 ? 0x80U : width == 3 ? 0x800U : 0x10000U;
  if (code < min || code > 0x10FFFFU || (code >= 0xD800U && code <= 0xDFFFU)) return false;
  out = Scalar{code, width};
  return true;
}

void place_one(Stream& stream, std::string_view chunk) {
  if (!keeps_written(stream)) return;
  if (stream.cursor > stream.bytes.size()) stream.cursor = stream.bytes.size();
  note_edit(stream);
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

}  // namespace

namespace write_internal {

bool compound_named(const Impl& machine, Cell cell, std::string_view& name, Cell& arg) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 1) return false;
  name = machine.atoms.name(machine.heap.functor_atom(cell));
  arg = machine.heap.arg(cell, 0);
  return true;
}

}  // namespace write_internal

namespace {

int flag_bit(const Impl& machine, Cell arg, bool& value) {
  arg = machine.heap.deref(arg);
  if (machine.heap.unbound(arg)) return -1;
  if (arg.tag() != Tag::Atom) return 0;
  const auto text = atom_text(machine, arg);
  if (text != "true" && text != "false") return 0;
  value = text == "true";
  return 1;
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

// 2 度目の variable_names/1 は 1 度目を置き換える。置き換える前に、その中身を全部確かめる。
bool take_names(Impl& machine, const Goal& goal, Cell option, Cell arg, Chosen& chosen) {
  const WalkedList walked = walk_list(machine, arg);
  if (walked.shape == ListShape::Variable || walked.shape == ListShape::Partial) return raise_inst(machine, goal);
  if (walked.shape != ListShape::Proper) return raise_domain(machine, goal, "write_option", option);
  std::vector<VarName> names;
  for (Cell item : walked.items) {
    if (take_pair(machine, goal, item, names)) return true;
  }
  chosen.names = std::move(names);
  return false;
}

bool apply_flag(Impl& machine, const Goal& goal, Cell item, std::string_view name, Cell arg, Chosen& chosen) {
  bool value = false;
  const int bit = flag_bit(machine, arg, value);
  if (bit < 0) return raise_inst(machine, goal);
  if (bit == 0) return raise_domain(machine, goal, "write_option", item);
  if (name == "quoted") chosen.quoted = value;
  else if (name == "ignore_ops") chosen.ignore_ops = value;
  else chosen.numbervars = value;
  return false;
}

// max_depth(N): N は 0 以上の整数。変数は instantiation_error、それ以外は domain_error（suite は type_error も許す）。
// 正の Big は int64 に入らないだけで不正ではないので、integer_value を呼ばず上限に切り詰める。
bool take_depth(Impl& machine, const Goal& goal, Cell item, Cell arg, Chosen& chosen) {
  arg = machine.heap.deref(arg);
  if (machine.heap.unbound(arg)) return raise_inst(machine, goal);
  if (!is_integer_tag(arg.tag()) || integer_negative(machine, arg)) {
    return raise_domain(machine, goal, "write_option", item);
  }
  if (arg.tag() == Tag::Big) {
    chosen.max_depth = 0x7fffffff;
    return false;
  }
  const auto value = machine.heap.integer_value(arg);
  chosen.max_depth = value > 0x7fffffff ? 0x7fffffff : static_cast<int>(value);
  return false;
}

bool apply_option(Impl& machine, const Goal& goal, Cell item, Chosen& chosen) {
  std::string_view name;
  Cell arg{};
  if (!compound_named(machine, item, name, arg)) return raise_domain(machine, goal, "write_option", item);
  if (name == "variable_names") return take_names(machine, goal, item, arg, chosen);
  if (name == "max_depth") return take_depth(machine, goal, item, arg, chosen);
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

// 演算子は呼び手の文脈モジュールの見え方で書く（ADR 0006）。
Cell stream_handle(Impl& machine, std::size_t index) {
  return machine.heap.structure(machine.atoms.intern("$stream"),
                                std::array<Cell, 1>{machine.heap.integer(static_cast<std::int64_t>(index))});
}

bool emit_written(Impl& machine, const Goal& goal, std::size_t index, Cell term, const WriteOptions& options,
                  const std::vector<VarName>& names, Kind kind) {
  const std::uint32_t module = goal.module;
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
    if (hook.scratch_closed) {
      raise_exists(machine, goal, stream_handle(machine, hook.scratch));
      return false;
    }
    if (hook.stop && !hook.halted) return false;
    // portray が行き先の流れを閉じたら、書かずに誤りにする。前は閉じた流れへ黙って書き、何も残らなかった（M-31）。
    if (machine.streams[index].closed) {
      raise_exists(machine, goal, stream_handle(machine, index));
      return false;
    }
    // halt で止まったときは、portray の分までの字面を書いてから止まる。halt_now が残りのゴールを止める。
    emit_text(machine, index, text);
    return !hook.stop;
  }
  emit_text(machine, index, write_term(machine.heap, view, term, options, names));
  return true;
}

void run_out(Impl& machine, const Goal& call, Kind kind) {
  // print の portray は入れ子で GC を走らせ、枠を動かす。書いた後は登録した写しの cont を使う。
  Goal goal = call;
  const RootScope roots(machine, [&goal](GcVisitor& visitor) {
    visitor.frame(goal.at);
    visitor.frame(goal.cont);
  });
  const Spec spec = spec_of(goal, kind == Kind::Term);
  std::size_t index = 0;
  Chosen chosen;
  if (prepare_out(machine, goal, spec, kind, index, chosen)) return;
  WriteOptions options;
  options.quoted = chosen.quoted;
  options.ignore_ops = chosen.ignore_ops;
  options.numbervars = chosen.numbervars;
  options.max_depth = chosen.max_depth;
  options.fullwidth = machine.japanese_punct;
  if (!emit_written(machine, goal, index, goal.arg(machine, spec.term_at), options, chosen.names, kind)) return;
  machine.ip = goal.cont;
}
}  // namespace

bool run_write(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
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

// 流れの項を作らずに、組込が流れの番号へ書く（デバッガの知らせ）。位置と字数も進める。
namespace write_internal {
void emit_to_stream(Impl& machine, std::size_t index, std::string_view text) { emit_text(machine, index, text); }
}  // namespace write_internal

}  // namespace mosa
