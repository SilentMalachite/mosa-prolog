// 規格 8.11.1 open/3,4。8.11.6 close/1,2。8.11.7 flush_output/0,1。open_string/2 は ADR 0005。
// open(+source_sink, +io_mode, -stream)
// open(+source_sink, +io_mode, -stream, +stream_options)
//   instantiation_error, uninstantiation_error, type_error(atom), type_error(list),
//   domain_error(source_sink), domain_error(io_mode), domain_error(stream_option),
//   existence_error(source_sink), permission_error(open, source_sink)
// open_string(+source, -stream)
//   source はアトムかコードのリスト。文字のリストは受けない。
//   instantiation_error, type_error(atom), type_error(list), type_error(integer),
//   uninstantiation_error, representation_error(character_code)（コードの Big も。フェーズ10、設計の 8）
// close(+stream_or_alias) / close(+stream_or_alias, +close_options)
//   選択肢は force(true/false)。無いときは force(false)。
//   書き戻しに失敗し force が false なら system_error。ストリームは開いたまま。
//   user_input・user_output・user_error は吐き出すだけで閉じない（iso-notes 8.11.6.1 の A）。
// flush_output / flush_output(+stream_or_alias)
//   入力ストリームは permission_error(output, stream, S)。
// 読みのファイルは開くときに全体を読まず、読み取り口から必要な分だけ足す（全体レビュー推奨2、持ち主の選択 B）。
// 追記は位置と保存の仕組みが元の中身を要するので、通常ファイルだけをヒープ上限のバイト予算の中で読む。
// ファイルへの書き込みは close と flush のほかに、halt と Machine の終わりでも書き出す（レビュー M5）。
// '$stream'(N) の N が Big なら、流れの数を越える int64 と同じく existence_error(stream)（stream_number）。
// テスト: tests/pl/streams.pl, tests/pl/close.pl, tests/pl/japanese.pl, tests/pl/bigint.pl, tests/cpp/stream_test.cpp,
//   tests/cpp/file_flush_test.cpp

#include "../machine/internal.hpp"
#include "mosa/console.hpp"
#include "mosa/path_text.hpp"

#include <array>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <optional>
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

enum class FileLoad { Ok, Missing, Denied, TooBig };

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

// ヒープ上限のバイト予算から、今のヒープと開いた流れの中身を引いた残り。新しい流れの bytes はこの中だけに
// 伸びる（全体レビュー推奨2）。数えるときはあふれないよう丸める。
std::size_t stream_byte_budget(const Impl& machine) {
  const std::size_t total = heap_byte_budget(machine.heap.limit());
  std::size_t used = heap_byte_budget(machine.heap.size());
  for (const Stream& stream : machine.streams) {
    if (stream.closed) continue;
    used = stream.bytes.size() > total - std::min(total, used) ? total : used + stream.bytes.size();
    if (used >= total) return 0;
  }
  return total - std::min(total, used);
}

// 追記が元の中身を読むときだけの全体読み。通常ファイルでないもの（装置や名前付きパイプ）は終わりが無い
// ことがあるので、ディレクトリと同じく permission_error 側へ倒す。予算を超える大きさは TooBig。
FileLoadResult load_bytes(const std::filesystem::path& path, std::size_t byte_limit) {
  FileLoadResult result;
  // macOS と Linux の ifstream はディレクトリも開け、読むと空になる。ディレクトリは開けないソースとして
  // permission_error にする（8.11.5.3 f、レビュー L5）。書き込みの create_empty は前からそうなる。
  std::error_code kind;
  if (std::filesystem::is_directory(path, kind) && !kind) {
    result.status = FileLoad::Denied;
    return result;
  }
  std::error_code there_code;
  const bool there = std::filesystem::exists(path, there_code) && !there_code;
  if (there) {
    std::error_code regular_code;
    if (!std::filesystem::is_regular_file(path, regular_code) || regular_code) {
      result.status = FileLoad::Denied;
      return result;
    }
    std::error_code size_code;
    const auto size = std::filesystem::file_size(path, size_code);
    if (!size_code && size > byte_limit) {
      result.status = FileLoad::TooBig;
      return result;
    }
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    result.status = there ? FileLoad::Denied : FileLoad::Missing;
    return result;
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  result.bytes = buffer.str();
  if (result.bytes.size() > byte_limit) {
    result.status = FileLoad::TooBig;
    result.bytes.clear();
    return result;
  }
  result.status = FileLoad::Ok;
  return result;
}

bool create_empty(const std::filesystem::path& path) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  return static_cast<bool>(output);
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
    const std::size_t width = text_char_width(stream.bytes, index);
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
  // iso-notes 8.11 の決定 B。標準入力は終わりの後も読み直せる。
  if (mode == Stream::Mode::Read) stream.eof_action = Stream::EofAction::Reset;
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
  // 8.11.5.3 i と Cor.3 の 5.5.12。具体化した不正な選択肢は、引数の型ではなく項全体が領域外。
  // encoding は規格外なので、引数の型誤りを残す。
  if (arg.tag() != Tag::Atom && name != "encoding") return raise_domain(machine, goal, "stream_option", option);
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

// 読みは読み取り口を開くだけで、中身は読む述語が pull_stream で必要な分だけ足す。開けない理由の出し方は
// 全体を読んでいたときと同じ（ディレクトリは permission_error、無ければ existence_error）。
bool fill_read(Impl& machine, const Goal& goal, Cell source, const std::filesystem::path& path, Stream& made) {
  std::error_code kind;
  if (std::filesystem::is_directory(path, kind) && !kind) return refuse_file(machine, goal, source);
  auto input = std::make_unique<std::ifstream>(path, std::ios::binary);
  if (!*input) {
    std::error_code code;
    const bool there = std::filesystem::exists(path, code) && !code;
    if (there) return refuse_file(machine, goal, source);
    raise_formal(machine, formal_existence(machine, "source_sink", source), goal.name, goal.arity);
    return true;
  }
  made.source = std::move(input);
  made.byte_limit = stream_byte_budget(machine);
  return false;
}

bool fill_write(Impl& machine, const Goal& goal, Cell source, const std::filesystem::path& path, Stream& made) {
  if (made.mode == Stream::Mode::Write) {
    if (!create_empty(path)) return refuse_file(machine, goal, source);
    return false;
  }
  const FileLoadResult loaded = load_bytes(path, stream_byte_budget(machine));
  if (loaded.status == FileLoad::TooBig) throw ResourceLimit("heap_limit");
  if (loaded.status == FileLoad::Denied) return refuse_file(machine, goal, source);
  // 読んだ中身はファイルと同じなので、保存はその続きから書く。
  if (loaded.status == FileLoad::Ok) {
    made.bytes = loaded.bytes;
    made.on_disk = made.bytes.size();
  }
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
  if (choice.has_alias) made.alias = choice.alias;
  const auto path = path_from_utf8(made.path);
  if (choice.mode == Stream::Mode::Read) {
    if (fill_read(machine, goal, source, path, made) || take_alias(machine, goal, choice)) return;
  } else if (take_alias(machine, goal, choice) || fill_write(machine, goal, source, path, made)) {
    return;
  }
  // 閉じた流れが同じ別名を持ったままだと、新しい流れに届かない。消すのは開けると決まってから。
  // 失敗した open が消すと、その別名の誤りが existence_error から domain_error に変わる。
  if (choice.has_alias) {
    for (Stream& old : machine.streams) {
      if (old.closed && old.alias == choice.alias) old.alias.clear();
    }
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
    if (!is_integer_tag(item.tag())) return raise_type(machine, goal, "integer", item);
    if (!code_point_cell(machine, item)) {
      raise_formal(machine, formal_representation(machine, "character_code"), goal.name, goal.arity);
      return true;
    }
    append_utf8_code(bytes, static_cast<char32_t>(machine.heap.integer_value(item)));
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
  if (!is_integer_tag(arg.tag())) return raise_domain(machine, goal, "stream_or_alias", cell);
  const auto number = stream_number(machine, arg);
  if (!number) {
    raise_formal(machine, formal_existence(machine, "stream", cell), goal.name, goal.arity);
    return true;
  }
  index = *number;
  return false;
}

Cell named_stream(Impl& machine, const Goal& goal, std::size_t index) {
  if (goal.arity >= 1) return goal.arg(machine, 0);
  const Stream& stream = machine.streams[index];
  if (!stream.alias.empty()) return machine.heap.atom(machine.atoms.intern(stream.alias));
  return machine.heap.structure(machine.atoms.intern("$stream"),
                                std::array<Cell, 1>{machine.heap.integer(static_cast<std::int64_t>(index))});
}

bool write_whole(const std::filesystem::path& path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return false;
  if (!bytes.empty()) output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  output.close();
  return static_cast<bool>(output);
}

// ファイルが on_disk より短ければ、外で切られたか消されたので全体を書く。続きを書いた後、縮んだ分の余りを切る。
bool write_rest(const std::filesystem::path& path, std::string_view bytes, std::size_t on_disk) {
  std::error_code code;
  const auto size = std::filesystem::file_size(path, code);
  if (code || size < on_disk) return write_whole(path, bytes);
  std::fstream output(path, std::ios::binary | std::ios::in | std::ios::out);
  if (!output) return false;
  output.seekp(static_cast<std::streamoff>(on_disk));
  const std::string_view rest = bytes.substr(on_disk);
  if (!rest.empty()) output.write(rest.data(), static_cast<std::streamsize>(rest.size()));
  output.close();
  if (!output) return false;
  if (size > bytes.size()) std::filesystem::resize_file(path, bytes.size(), code);
  return !code;
}

bool save_output(std::size_t index, Stream& stream) {
  if (stream.mode == Stream::Mode::Read) return true;
  if (stream.kind == Stream::Kind::Standard) {
    if (index == 1) std::cout.flush();
    else if (index == 2) std::cerr.flush();
    return true;
  }
  if (stream.kind != Stream::Kind::File) return true;
  const auto path = path_from_utf8(stream.path);
  const bool saved =
      stream.on_disk == 0 ? write_whole(path, stream.bytes) : write_rest(path, stream.bytes, stream.on_disk);
  if (!saved) return false;
  stream.on_disk = stream.bytes.size();
  stream.unsaved = false;
  return true;
}

bool same_file(const Stream& left, const Stream& right) {
  if (left.path == right.path) return true;
  std::error_code code;
  return std::filesystem::equivalent(path_from_utf8(left.path), path_from_utf8(right.path), code) && !code;
}

// close と flush_output で保存したファイルは、それより前に書いて放置した別の流れに終わりで戻されない
// （Grok のレビューの High）。その流れがまた書けば unsaved が立ち、全体を書き直す。
// 読む流れは何も書かないので、同じファイルへ書いて放置した流れの中身を消してはいけない。
void settle_same_file(Impl& machine, std::size_t index) {
  const Stream& saved = machine.streams[index];
  if (saved.kind != Stream::Kind::File || saved.mode == Stream::Mode::Read) return;
  for (std::size_t other = 0; other < machine.streams.size(); ++other) {
    Stream& stream = machine.streams[other];
    if (other == index || stream.closed || !stream.unsaved || stream.kind != Stream::Kind::File) continue;
    if (!same_file(saved, stream)) continue;
    stream.unsaved = false;
    stream.on_disk = 0;
  }
}

void mark_closed(Impl& machine, std::size_t index) {
  machine.streams[index].closed = true;
  machine.streams[index].source.reset();
  // スロットは再利用しないので、中身を持ち続けると開け閉めのたびにメモリが増える。閉じた後は誤りしか返さない。
  std::string{}.swap(machine.streams[index].bytes);
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
  settle_same_file(machine, index);
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
  } else {
    settle_same_file(machine, index);
  }
  // 標準ストリームは吐き出すだけで閉じない（iso-notes 8.11.6.1 の A）。閉じると、戻り先の user_output まで
  // 書けなくなり、そのセッションの出力が二度と出ない（レビュー L13）。
  if (machine.streams[index].kind != Stream::Kind::Standard) mark_closed(machine, index);
  machine.ip = goal.cont;
}

}  // namespace

bool pull_stream(Stream& stream) {
  if (stream.mode != Stream::Mode::Read) return false;
  if (stream.kind == Stream::Kind::Standard) {
    const int byte = standard_input_get();
    if (byte == std::char_traits<char>::eof()) return false;
    stream.bytes.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
    return true;
  }
  if (stream.kind != Stream::Kind::File || !stream.source) return false;
  // 塊は持っている分に合わせて倍に伸ばす。大きな項を読み直す read_term が、塊のたびに頭から読んでも
  // 全体の仕事が長さの 2 倍ほどで済むため。短く読めたときは終わりと決めず、印を下ろして次で確かめる。
  // 外で足された分もその次に見える。
  const std::size_t want = std::min<std::size_t>(std::max<std::size_t>(65536, stream.bytes.size()), 8 * 1024 * 1024);
  std::string chunk(want, '\0');
  if (!stream.source->good()) {
    // 短く読めて終わりの印が立った後は、読んだ分だけの位置へ置き直さないと、外で足された分を読み直さない
    // 実装がある。置けない相手（パイプ）は印だけ下ろしてそのまま読む。
    stream.source->clear();
    stream.source->seekg(static_cast<std::streamoff>(stream.bytes.size()));
    if (!stream.source->good()) stream.source->clear();
  }
  stream.source->read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
  const auto got = static_cast<std::size_t>(stream.source->gcount());
  if (got == 0) {
    stream.source.reset();
    return false;
  }
  if (got > stream.byte_limit - std::min(stream.byte_limit, stream.bytes.size())) {
    throw ResourceLimit("heap_limit");
  }
  stream.bytes.append(chunk.data(), got);
  return true;
}

std::optional<std::size_t> stream_number(const Impl& machine, Cell number) {
  // Big は int64 の外にあり、流れの数（int64 に収まる）より大きいか負なので、どちらの側でも無い流れである。
  if (number.tag() == Tag::Big) return std::nullopt;
  const auto value = machine.heap.integer_value(number);
  if (value < 0 || static_cast<std::uint64_t>(value) >= machine.streams.size()) return std::nullopt;
  return static_cast<std::size_t>(value);
}

void flush_open_files(Impl& machine) {
  for (std::size_t index = 0; index < machine.streams.size(); ++index) {
    Stream& stream = machine.streams[index];
    // 閉じた流れの中身は close で書いた後の古い写しなので、外で変えられた中身を戻してしまう。
    if (stream.closed || stream.kind != Stream::Kind::File || !stream.unsaved) continue;
    // 書き出せなくても終わり方は変えない。close と違い、誤りを返す相手がいない。
    (void)save_output(index, stream);
  }
}

void init_streams(Impl& machine) {
  machine.streams.push_back(standard_stream(Stream::Mode::Read, "user_input"));
  // 標準の出力は他のプロセスと共有して末尾へ足していくので append（iso-notes 8.11 の決定 B）。
  machine.streams.push_back(standard_stream(Stream::Mode::Append, "user_output"));
  machine.streams.push_back(standard_stream(Stream::Mode::Append, "user_error"));
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
  if (name == "current_input" && goal.arity == 1) {
    run_current_input(machine, goal);
    return true;
  }
  if (name == "current_output" && goal.arity == 1) {
    run_current_output(machine, goal);
    return true;
  }
  if (name == "set_input" && goal.arity == 1) {
    run_set_input(machine, goal);
    return true;
  }
  if (name == "set_output" && goal.arity == 1) {
    run_set_output(machine, goal);
    return true;
  }
  if (name == "at_end_of_stream" && goal.arity <= 1) {
    run_at_end(machine, goal);
    return true;
  }
  if (name == "set_stream_position" && goal.arity == 2) {
    run_set_position(machine, goal);
    return true;
  }
  if (name == "stream_property" && goal.arity == 2) {
    run_stream_property(machine, goal);
    return true;
  }
  return false;
}

}  // namespace mosa
