// 規格 8.14.1 read_term/3, read_term/2, read/2, read/1。
// read_term(+stream_or_alias, ?term, +read_options_list)
// read_term(?term, +read_options_list) は現在の入力。
// read/2, read/1 はオプションが空の read_term。
//   終端の印は end_of_file。終止符の直後の空白は流れに残す。
//   構文エラーもその項の終止符まで進める。確認の空白は次の項に残す。
//   オプションは variables, variable_names, singletons と、仕様の quoted。
//   その三つの引数がリストでも変数でもなければ、読む前に domain_error(read_option, その選択肢)。
//   変数と真のリスト（空リストを含む）は読んだあとで束縛する。部分リストも同じ。
//   quoted(true) も quoted(false) も項は変えない。引用は字句の一部だから。
//   エラー順は、流れの変数、選択肢の具体化、流れのドメイン、リスト型、
//   read_option、存在、入力許可、past_end、それから構文。
//   instantiation_error, domain_error(stream_or_alias), domain_error(read_option),
//   type_error(list), existence_error(stream),
//   permission_error(input, stream), permission_error(input, binary_stream),
//   permission_error(input, past_end_of_stream),
//   representation_error(character), syntax_error(読み筋)
// 構文エラーの Extra は stream(S, Line, LinePos, CharNo)。テストは Formal を比べる。
// テスト: tests/pl/read.pl, tests/pl/japanese.pl, tests/cpp/stream_test.cpp

#include "../machine/internal.hpp"

#include "mosa/reader.hpp"

#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

enum class ReadOpt { Vars, Names, Singles };
enum class OptShape { Ok, Inst, Type };

struct Chosen {
  ReadOpt kind = ReadOpt::Vars;
  Cell arg{};
};

struct Spec {
  bool has_stream = false;
  bool has_options = false;
  std::size_t term_at = 0;
  std::size_t options_at = 0;
};

struct Spot {
  std::size_t line = 1;
  std::size_t line_pos = 1;
  std::size_t char_no = 0;
};

struct Intake {
  ReadOutcome outcome;
  std::size_t consumed = 0;
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
  raise_formal(machine, formal_permission(machine, "input", type, culprit), goal.name, goal.arity);
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
  if (number < 0 || static_cast<std::uint64_t>(number) >= machine.streams.size()) {
    return raise_exists(machine, goal, cell);
  }
  index = static_cast<std::size_t>(number);
  return false;
}

Cell stream_named(Impl& machine, std::size_t index) {
  const Stream& stream = machine.streams[index];
  if (!stream.alias.empty()) return machine.heap.atom(machine.atoms.intern(stream.alias));
  return machine.heap.structure(machine.atoms.intern("$stream"),
                                std::array<Cell, 1>{machine.heap.integer(static_cast<std::int64_t>(index))});
}

bool reject_closed(Impl& machine, const Goal& goal, std::size_t index, Cell culprit) {
  if (!machine.streams[index].closed) return false;
  return raise_exists(machine, goal, culprit);
}

bool reject_input(Impl& machine, const Goal& goal, const Stream& stream, Cell culprit) {
  if (stream.mode != Stream::Mode::Read) return raise_permit(machine, goal, "stream", culprit);
  if (stream.data == Stream::Data::Binary) return raise_permit(machine, goal, "binary_stream", culprit);
  return false;
}

bool pull_byte(Stream& stream) {
  if (stream.kind != Stream::Kind::Standard || stream.mode != Stream::Mode::Read) return false;
  const int byte = std::cin.get();
  if (byte == std::char_traits<char>::eof()) return false;
  stream.bytes.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
  return true;
}

std::string_view rest(const Stream& stream) {
  if (stream.cursor > stream.bytes.size()) return {};
  return std::string_view{stream.bytes.data() + stream.cursor, stream.bytes.size() - stream.cursor};
}

bool decode_scalar(std::string_view bytes, std::size_t index, char32_t& code, std::size_t& width) {
  if (index >= bytes.size()) return false;
  const auto lead = static_cast<unsigned char>(bytes[index]);
  if (lead <= 0x7FU) {
    code = lead;
    width = 1;
    return true;
  }
  const std::size_t need = lead >= 0xF0U ? 4 : lead >= 0xE0U ? 3 : lead >= 0xC2U ? 2 : 0;
  if (need == 0 || index + need > bytes.size()) return false;
  code = lead & (need == 2 ? 0x1FU : need == 3 ? 0x0FU : 0x07U);
  for (std::size_t unit = 1; unit < need; ++unit) {
    const auto next = static_cast<unsigned char>(bytes[index + unit]);
    if ((next & 0xC0U) != 0x80U) return false;
    code = (code << 6) | (next & 0x3FU);
  }
  const char32_t min = need == 2 ? 0x80U : need == 3 ? 0x800U : 0x10000U;
  if (code < min || code > 0x10FFFFU || (code >= 0xD800U && code <= 0xDFFFU)) return false;
  width = need;
  return true;
}

void step_spot(Spot& spot, char32_t code) {
  spot.char_no += 1;
  if (code == U'\n') {
    spot.line += 1;
    spot.line_pos = 1;
    return;
  }
  spot.line_pos += 1;
}

void advance_slice(Stream& stream, std::string_view slice) {
  Spot spot{stream.line, stream.line_pos, stream.char_no};
  std::size_t index = 0;
  while (index < slice.size()) {
    char32_t code = 0;
    std::size_t width = 0;
    if (!decode_scalar(slice, index, code, width)) break;
    index += width;
    step_spot(spot, code);
  }
  stream.cursor += index;
  stream.line = spot.line;
  stream.line_pos = spot.line_pos;
  stream.char_no = spot.char_no;
  stream.past_end = false;
}

Spot at_token(const Stream& stream, std::string_view slice, std::size_t line, std::size_t column) {
  Spot rel{1, 1, 0};
  Spot abs{stream.line, stream.line_pos, stream.char_no};
  std::size_t index = 0;
  while (rel.line < line || (rel.line == line && rel.line_pos < column)) {
    char32_t code = 0;
    std::size_t width = 0;
    if (!decode_scalar(slice, index, code, width)) break;
    index += width;
    step_spot(rel, code);
    step_spot(abs, code);
  }
  return abs;
}

std::string reason_text(SyntaxReason reason) {
  switch (reason) {
    case SyntaxReason::InvalidUtf8: return "不正な UTF-8";
    case SyntaxReason::IllegalCharacter: return "使えない文字";
    case SyntaxReason::UnterminatedQuote: return "引用が閉じていない";
    case SyntaxReason::UnterminatedComment: return "コメントが閉じていない";
    case SyntaxReason::BadEscape: return "不正なエスケープ";
    case SyntaxReason::IntegerOverflow: return "整数が範囲を超えている";
    case SyntaxReason::UnexpectedToken: return "予期しないトークン";
    case SyntaxReason::MaxArity: return "引数が多すぎる";
    case SyntaxReason::IncompleteTerm: return "項が閉じていません";
  }
  return "予期しないトークン";
}

std::string detail_text(const SyntaxFailure& failure, const Spot& spot) {
  const std::string token = failure.token.empty() ? "終端" : failure.token;
  return "行 " + std::to_string(spot.line) + " 列 " + std::to_string(spot.line_pos) + ": " +
         reason_text(failure.reason) + " " + token;
}

Cell stream_place(Impl& machine, Cell culprit, const Spot& spot) {
  const std::array<Cell, 4> args{culprit, machine.heap.integer(static_cast<std::int64_t>(spot.line)),
                                 machine.heap.integer(static_cast<std::int64_t>(spot.line_pos)),
                                 machine.heap.integer(static_cast<std::int64_t>(spot.char_no))};
  return machine.heap.structure(machine.atoms.intern("stream"), args);
}

std::size_t utf8_fault_at(std::string_view slice, std::size_t line, std::size_t column) {
  Spot rel{1, 1, 0};
  std::size_t index = 0;
  while (rel.line < line || (rel.line == line && rel.line_pos < column)) {
    char32_t code = 0;
    std::size_t width = 0;
    if (!decode_scalar(slice, index, code, width)) break;
    index += width;
    step_spot(rel, code);
  }
  return index;
}

bool splice_fffd(Stream& stream, const SyntaxFailure& failure) {
  const std::string_view slice = rest(stream);
  const std::size_t index = utf8_fault_at(slice, failure.line, failure.column);
  if (index >= slice.size()) return false;
  // 不正な 1 バイトだけを U+FFFD の UTF-8 にし、その位置から読み直す。
  stream.bytes.replace(stream.cursor + index, 1, "\xEF\xBF\xBD");
  return true;
}

bool note_syntax(Impl& machine, const Goal& goal, Stream& stream, const SyntaxFailure& failure,
                 std::size_t consumed, Cell culprit) {
  if (failure.reason == SyntaxReason::InvalidUtf8) {
    if (machine.utf8_replace && splice_fffd(stream, failure)) return true;
    raise_formal(machine, formal_representation(machine, "character"), goal.name, goal.arity);
    return false;
  }
  const std::string_view slice = rest(stream);
  const Spot spot = at_token(stream, slice, failure.line, failure.column);
  const std::size_t take = consumed > slice.size() ? slice.size() : consumed;
  advance_slice(stream, slice.substr(0, take));
  const Cell detail = machine.heap.atom(machine.atoms.intern(detail_text(failure, spot)));
  raise_formal_extra(machine, formal_syntax(machine, detail), goal.name, goal.arity,
                     stream_place(machine, culprit, spot));
  return false;
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

bool compound_one(const Impl& machine, Cell cell, std::string_view& name, Cell& arg) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 1) return false;
  name = machine.atoms.name(machine.heap.functor_atom(cell));
  arg = machine.heap.arg(cell, 0);
  return true;
}

bool reject_option(Impl& machine, const Goal& goal, Cell item, std::vector<Chosen>& chosen) {
  std::string_view name;
  Cell arg{};
  if (!compound_one(machine, item, name, arg)) return raise_domain(machine, goal, "read_option", item);
  if (name == "variables" || name == "variable_names" || name == "singletons") {
    // リストでない引数を単一化の失敗にすると、読む前の domain_error にならない。
    if (walk_list(machine, arg).shape == ListShape::Bad) return raise_domain(machine, goal, "read_option", item);
    const ReadOpt kind = name == "variables" ? ReadOpt::Vars : name == "variable_names" ? ReadOpt::Names : ReadOpt::Singles;
    chosen.push_back(Chosen{kind, arg});
    return false;
  }
  if (name != "quoted") return raise_domain(machine, goal, "read_option", item);
  const Cell flag = machine.heap.deref(arg);
  if (machine.heap.unbound(flag)) return raise_inst(machine, goal);
  if (flag.tag() == Tag::Atom) {
    const auto text = atom_text(machine, flag);
    if (text == "true" || text == "false") return false;
  }
  return raise_domain(machine, goal, "read_option", item);
}

Cell name_pair(Impl& machine, const VarName& var) {
  const std::array<Cell, 2> args{machine.heap.atom(machine.atoms.intern(var.name)), var.cell};
  return machine.heap.structure(machine.atoms.intern("="), args);
}

Cell name_list(Impl& machine, const std::vector<VarName>& vars) {
  std::vector<Cell> items;
  items.reserve(vars.size());
  for (const VarName& var : vars) items.push_back(name_pair(machine, var));
  return list_from_cells(machine, items);
}

void bind_read(Impl& machine, const Goal& goal, Cell term_cell, const std::vector<Chosen>& chosen, const ReadTerm& read) {
  if (!do_unify(machine, term_cell, read.term)) {
    machine.failed = true;
    return;
  }
  for (const Chosen& one : chosen) {
    Cell value = list_from_cells(machine, {});
    if (one.kind == ReadOpt::Vars) value = list_from_cells(machine, read.variables);
    else if (one.kind == ReadOpt::Names) value = name_list(machine, read.names);
    else value = name_list(machine, read.singletons);
    if (!do_unify(machine, one.arg, value)) {
      machine.failed = true;
      return;
    }
  }
  machine.ip = goal.cont;
}

ReadTerm eof_term(Impl& machine) {
  ReadTerm term;
  term.term = machine.heap.atom(machine.atoms.intern("end_of_file"));
  return term;
}

// 演算子は呼び手の文脈モジュールの見え方で読む（ADR 0006）。
Intake intake_term(Impl& machine, Stream& stream, std::uint32_t module) {
  while (true) {
    const std::string_view slice = rest(stream);
    Reader reader(slice, machine.heap, op_view(machine, module), current_double_quotes(machine),
                  &machine.char_conv);
    ReadOutcome outcome = reader.next();
    const std::size_t consumed = reader.consumed();
    // 標準入力は、項が閉じるか本当の終端まで 1 バイトずつ足す。
    if (outcome.term || consumed < slice.size() || !pull_byte(stream)) return Intake{std::move(outcome), consumed};
  }
}

void deliver(Impl& machine, const Goal& goal, Stream& stream, Intake intake, Cell term_cell,
             const std::vector<Chosen>& chosen, Cell culprit) {
  while (true) {
    if (intake.outcome.failure) {
      // 置換できたときだけ、カーソルを進めずに同じ位置から読み直す。
      if (!note_syntax(machine, goal, stream, *intake.outcome.failure, intake.consumed, culprit)) return;
      intake = intake_term(machine, stream, goal.module);
      continue;
    }
    const std::string_view slice = rest(stream);
    const std::size_t take = intake.consumed > slice.size() ? slice.size() : intake.consumed;
    advance_slice(stream, slice.substr(0, take));
    if (!intake.outcome.term) {
      stream.past_end = true;
      bind_read(machine, goal, term_cell, chosen, eof_term(machine));
      return;
    }
    bind_read(machine, goal, term_cell, chosen, *intake.outcome.term);
    return;
  }
}

bool prepare(Impl& machine, const Goal& goal, const Spec& spec, std::size_t& index, std::vector<Chosen>& chosen,
             Cell& culprit) {
  if (spec.has_stream && machine.heap.unbound(machine.heap.deref(goal.arg(machine, 0)))) return raise_inst(machine, goal);
  std::vector<Cell> items;
  OptShape shape = OptShape::Ok;
  if (spec.has_options) shape = scan_options(machine, goal.arg(machine, spec.options_at), items);
  if (shape == OptShape::Inst) return raise_inst(machine, goal);
  index = machine.current_in;
  if (spec.has_stream && resolve_stream(machine, goal, goal.arg(machine, 0), index)) return true;
  if (shape == OptShape::Type) return raise_type(machine, goal, "list", goal.arg(machine, spec.options_at));
  for (Cell item : items) {
    if (reject_option(machine, goal, item, chosen)) return true;
  }
  culprit = spec.has_stream ? goal.arg(machine, 0) : stream_named(machine, index);
  if (reject_closed(machine, goal, index, culprit) || reject_input(machine, goal, machine.streams[index], culprit)) {
    return true;
  }
  return false;
}

void run_one(Impl& machine, const Goal& goal, bool term_pred) {
  const Spec spec = spec_of(goal, term_pred);
  std::size_t index = 0;
  std::vector<Chosen> chosen;
  Cell culprit{};
  if (prepare(machine, goal, spec, index, chosen, culprit)) return;
  Stream& stream = machine.streams[index];
  if (stream.past_end && stream.eof_action == Stream::EofAction::Error) {
    raise_permit(machine, goal, "past_end_of_stream", culprit);
    return;
  }
  if (stream.past_end && stream.eof_action == Stream::EofAction::Reset) reset_at_end(stream);
  const Cell term_cell = goal.arg(machine, spec.term_at);
  if (stream.past_end) {
    bind_read(machine, goal, term_cell, chosen, eof_term(machine));
    return;
  }
  deliver(machine, goal, stream, intake_term(machine, stream, goal.module), term_cell, chosen, culprit);
}

}  // namespace

bool run_read(Impl& machine, const Goal& goal) {
  const auto name = machine.atoms.name(goal.name);
  if (name == "read_term" && (goal.arity == 2 || goal.arity == 3)) {
    run_one(machine, goal, true);
    return true;
  }
  if (name == "read" && (goal.arity == 1 || goal.arity == 2)) {
    run_one(machine, goal, false);
    return true;
  }
  return false;
}

}  // namespace mosa
