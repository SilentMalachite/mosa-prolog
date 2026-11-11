// 規格 8.14.1 read_term/3, read_term/2, read/2, read/1。
// read_term(+stream_or_alias, ?term, +read_options_list)
// read_term(?term, +read_options_list) は現在の入力。
// read/2, read/1 はオプションが空の read_term。
//   終端の印は end_of_file。終止符の直後の空白は流れに残す。
//   構文エラーもその項の終止符まで進める。確認の空白は次の項に残す。壊れた UTF-8 は replace なら U+FFFD に置き換え、
//   error なら置き換えて読んだのと同じ所まで進めてから representation_error(character) を出す（iso-notes 第4節の決定 A）。
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
//   representation_error(character), syntax_error(Key)
// 構文エラーの Key と Extra syntax(stream(S, Line, LinePos, CharNo), Token) は ADR 0007。テストは Formal を比べる。
// テスト: tests/pl/read.pl, tests/pl/japanese.pl, tests/cpp/stream_test.cpp, tests/cpp/stdin_read_test.cpp,
//   tests/cpp/stdin_scale_test.cpp

#include "../machine/internal.hpp"

#include "mosa/console.hpp"

#include "mosa/lexer.hpp"
#include "mosa/reader.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <optional>
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
  const int byte = standard_input_get();
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
  // F5..FF は先頭になれない。前は F8 90 80 80 を正しい字と取り、置き換えを拒んで進まなかった（L6 の Codex レビュー 2 回目）。
  const std::size_t need = lead >= 0xF5U ? 0 : lead >= 0xF0U ? 4 : lead >= 0xE0U ? 3 : lead >= 0xC2U ? 2 : 0;
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

// 壊れた並びは get_char と同じく 1 字に数えて進む。構文の誤りの読み飛ばしは、置き換えていない壊れた並びを含みうる。
// そこで止めると項の途中に残り、次の read が注釈の中を項として読んだ（L6 の Codex レビュー）。
void advance_slice(Stream& stream, std::string_view slice) {
  Spot spot{stream.line, stream.line_pos, stream.char_no};
  std::size_t index = 0;
  while (index < slice.size()) {
    char32_t code = U'\uFFFD';
    std::size_t width = 0;
    if (!decode_scalar(slice, index, code, width)) width = ill_formed_width(slice, index);
    index += width;
    step_spot(spot, code);
  }
  stream.cursor += index;
  stream.line = spot.line;
  stream.line_pos = spot.line_pos;
  stream.char_no = spot.char_no;
  stream.past_end = false;
}

// 字句解析は切り出しの先頭の BOM を飛ばし、列に数えない（Lexer::skip_bom）。行と列から位置を戻すときも同じに飛ばす。
// 数えると 1 行目の位置が 1 字ずれ、replace が正しい字を U+FFFD に置き換え続けて止まらなかった（L6 の敵対的レビュー A4）。
std::size_t bom_width(std::string_view slice) { return slice.starts_with("\xEF\xBB\xBF") ? 3 : 0; }

Spot at_token(const Stream& stream, std::string_view slice, std::size_t line, std::size_t column) {
  Spot rel{1, 1, 0};
  Spot abs{stream.line, stream.line_pos, stream.char_no};
  const std::size_t bom = bom_width(slice);
  if (bom > 0) step_spot(abs, U'\uFEFF');
  std::size_t index = bom;
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

// ADR 0007 の syntax(stream(S, Line, LinePos, CharNo), Token)。入力の終わりでは Token が ''。
Cell syntax_place(Impl& machine, Cell culprit, const Spot& spot, std::string_view token) {
  const std::array<Cell, 4> args{culprit, machine.heap.integer(static_cast<std::int64_t>(spot.line)),
                                 machine.heap.integer(static_cast<std::int64_t>(spot.line_pos)),
                                 machine.heap.integer(static_cast<std::int64_t>(spot.char_no))};
  const std::array<Cell, 2> both{machine.heap.structure(machine.atoms.intern("stream"), args),
                                 machine.heap.atom(machine.atoms.intern(token))};
  return machine.heap.structure(machine.atoms.intern("syntax"), both);
}

// U+FFFD に置き換えた壊れた並び。at は stream.bytes の中の位置、bytes は元のバイト。
struct Splice {
  std::size_t at = 0;
  std::string bytes;
};

// 次に置き換える壊れた並びの、読む字の中の位置。字句の誤りはその位置。構文の誤りは、読み飛ばした所の中の最初の
// 壊れた並び。読み飛ばしは注釈の中の壊れた並びを注釈の終わりと取り、注釈の中の終止符で止まって、次の read が
// 注釈の中を項として読んだ（L6 の Codex レビュー）。置き換えて読み直せば、注釈を最後まで飛ばす。
std::optional<std::size_t> next_fault(const Stream& stream, const Intake& intake) {
  const SyntaxFailure& failure = *intake.outcome.failure;
  if (failure.reason == SyntaxReason::InvalidUtf8) return failure.byte;
  const std::string_view slice = rest(stream);
  const std::size_t limit = std::min(intake.consumed, slice.size());
  std::size_t index = 0;
  while (index < limit) {
    char32_t code = 0;
    std::size_t width = 0;
    if (!decode_scalar(slice, index, code, width)) return index;
    index += width;
  }
  return std::nullopt;
}

// 壊れた並びの最大の部分を U+FFFD の UTF-8 1 つにし、その位置から読み直す（get_char と同じ幅、レビュー L6）。
// そこが正しい字なら置き換えない。置き換えると同じ所で失敗し続け、読み直しが止まらない。
bool splice_fffd(Stream& stream, std::size_t index, std::vector<Splice>& splices) {
  const std::string_view slice = rest(stream);
  char32_t code = 0;
  std::size_t valid = 0;
  if (index >= slice.size() || decode_scalar(slice, index, code, valid)) return false;
  const std::size_t width = ill_formed_width(slice, index);
  splices.push_back(Splice{stream.cursor + index, std::string{slice.substr(index, width)}});
  stream.bytes.replace(stream.cursor + index, width, "\xEF\xBF\xBD");
  return true;
}

// error のときの置き換えを元のバイトに戻す。後ろから戻すので、前の置き換えの位置はずれない。U+FFFD も壊れた並びも
// 1 文字に数えるので、行と列はそのまま。戻さないと、位置を戻して読み直したとき誤りでなく U+FFFD が返る。
void restore_splices(Stream& stream, const std::vector<Splice>& splices) {
  for (auto splice = splices.rbegin(); splice != splices.rend(); ++splice) {
    stream.bytes.replace(splice->at, 3, splice->bytes);
    if (splice->at < stream.cursor) stream.cursor = stream.cursor + splice->bytes.size() - 3;
  }
}

// 構文の誤りは、その項の終止符まで進めてから出す。
void raise_syntax(Impl& machine, const Goal& goal, Stream& stream, const SyntaxFailure& failure,
                  std::size_t consumed, Cell culprit) {
  if (failure.reason == SyntaxReason::InvalidUtf8) {
    // 置き換える位置が見つからないときだけここに来る。進めずに出す。
    raise_formal(machine, formal_representation(machine, "character"), goal.name, goal.arity);
    return;
  }
  const std::string_view slice = rest(stream);
  const Spot spot = at_token(stream, slice, failure.line, failure.column);
  const std::size_t take = consumed > slice.size() ? slice.size() : consumed;
  advance_slice(stream, slice.substr(0, take));
  const Cell key = machine.heap.atom(machine.atoms.intern(syntax_key(failure.reason)));
  raise_formal_extra(machine, formal_syntax(machine, key), goal.name, goal.arity,
                     syntax_place(machine, culprit, spot, failure.token));
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

// 項（または誤り）の後ろに標準入力から足した字（from から先）を、読み取り口へ返す。終止符を確かめるためだけに読んだ字で、
// 返さないと user_input に残り、REPL が次の行として読めない（レビュー M12）。peek_char が前に取り込んだ字は残す。
void give_back(Stream& stream, std::size_t from) {
  if (stream.kind != Stream::Kind::Standard) return;
  while (stream.bytes.size() > from) {
    standard_input_unget(stream.bytes.back());
    stream.bytes.pop_back();
  }
}

// 標準入力から、足した字が終止符の後ろの字になりうるところ（Lexer::may_end_before_last）か終わりまで足す。
// 足すたびに頭から読み直すと、時間が項の長さの 2 乗で伸びた（19 KB のリストで 8 秒、stdin_scale_test）。
// 読み手が終止符で止まるのは、その後ろの字が届いたときだけなので、間の字で読み直しても結果は変わらない。
// 終止符でない所で止まる誤りでも、長い入力を読めば同じ所で止まり、余りは give_back が返す。
// 戻り値は 1 字でも足したか。終わりに当たれば ended を立て、もう読まない。
bool pull_to_mark(const Impl& machine, Stream& stream, bool& ended) {
  bool pulled = false;
  while (pull_byte(stream)) {
    pulled = true;
    if (Lexer::may_end_before_last(rest(stream), &machine.char_conv, machine.japanese_punct)) return true;
  }
  ended = true;
  return pulled;
}

// 演算子は呼び手の文脈モジュールの見え方で読む（ADR 0006）。
Intake intake_term(Impl& machine, Stream& stream, std::uint32_t module) {
  const std::size_t had = stream.bytes.size();
  bool ended = false;
  while (true) {
    const std::string_view slice = rest(stream);
    Reader reader(slice, machine.heap, op_view(machine, module), current_double_quotes(machine),
                  &machine.char_conv);
    reader.set_fullwidth(machine.japanese_punct);
    ReadOutcome outcome = reader.next();
    const std::size_t consumed = reader.consumed();
    // 標準入力は、読んだ先に字が残るか本当の終端まで足す。項が閉じても終止符が切れ目の最後の字なら、
    // 次の字を見るまで終止符と決まらない（6.4.8、1. の次が 5 なら 1.5）。次の字が無ければ終端の前の終止符である。
    // 誤りでも返す。成功と失敗とで REPL に渡る字を変えない（Grok レビュー）。
    if (consumed < slice.size()) {
      give_back(stream, std::max(had, stream.cursor + consumed));
      return Intake{std::move(outcome), consumed};
    }
    // 終わりに当たった後は読み直した結果を返す。もう一度読むと、端末の Ctrl-D 1 回で 2 回終わりを見る。
    if (ended || !pull_to_mark(machine, stream, ended)) {
      // 項が読めた後の終わりは終止符の区切りとしてだけ使う。印を残すと、端末で foo. の後の Ctrl-D が
      // REPL の次の行の読みまで終わらせる。パイプは下ろしても次の読みでまた終わりを見る。
      if (outcome.term && stream.kind == Stream::Kind::Standard) standard_input_clear();
      return Intake{std::move(outcome), consumed};
    }
  }
}

// error でも replace と同じく U+FFFD に置き換えて読み、項の終わり（構文の誤りなら、その読み飛ばしの後）まで進めてから、
// 元のバイトに戻して representation_error を出す。流れの位置は replace のときと同じになる（iso-notes 第4節の決定 A、
// レビュー L6）。壊れた並びの直後で止めると、注釈や引用の中の字を次の read が項として読んだ（L6 の敵対的レビュー A1）。
// 読み直す前に、失敗した読みが作った項をヒープから捨てる。捨てないと壊れた並びの数だけ前半の項が積もり、
// catch できる誤りの代わりに --heap-max で止まった（L6 の Codex レビュー）。読む間に GC は動かない。
void deliver(Impl& machine, const Goal& goal, Stream& stream, std::size_t heap_mark, Intake intake, Cell term_cell,
             const std::vector<Chosen>& chosen, Cell culprit) {
  std::vector<Splice> splices;
  while (intake.outcome.failure) {
    const std::optional<std::size_t> fault = next_fault(stream, intake);
    if (!fault || !splice_fffd(stream, *fault, splices)) break;
    if (machine.heap.size() >= heap_mark) machine.heap.truncate(heap_mark);
    intake = intake_term(machine, stream, goal.module);
  }
  const bool refused = !machine.utf8_replace && !splices.empty();
  if (intake.outcome.failure && !refused) {
    raise_syntax(machine, goal, stream, *intake.outcome.failure, intake.consumed, culprit);
    return;
  }
  const std::string_view slice = rest(stream);
  const std::size_t take = intake.consumed > slice.size() ? slice.size() : intake.consumed;
  advance_slice(stream, slice.substr(0, take));
  if (refused) {
    restore_splices(stream, splices);
    raise_formal(machine, formal_representation(machine, "character"), goal.name, goal.arity);
    return;
  }
  if (!intake.outcome.term) {
    stream.past_end = true;
    bind_read(machine, goal, term_cell, chosen, eof_term(machine));
    return;
  }
  bind_read(machine, goal, term_cell, chosen, *intake.outcome.term);
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
  const std::size_t heap_mark = machine.heap.size();
  deliver(machine, goal, stream, heap_mark, intake_term(machine, stream, goal.module), term_cell, chosen, culprit);
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
