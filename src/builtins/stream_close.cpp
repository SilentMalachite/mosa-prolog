// 規格 8.11.6 close/1,2。8.11.7 flush_output/0,1。
// close(+stream_or_alias) / close(+stream_or_alias, +close_options)
//   instantiation_error, type_error(list), domain_error(close_option), domain_error(stream_or_alias),
//   existence_error(stream), system_error
//   選択肢は force(true/false)。無いときは force(false)。
//   書き戻しに失敗し force が false なら system_error。ストリームは開いたまま。
//   user_input・user_output・user_error は吐き出すだけで閉じない（iso-notes 8.11.6.1 の A）。
// flush_output / flush_output(+stream_or_alias)
//   instantiation_error, domain_error(stream_or_alias), existence_error(stream), system_error
//   入力ストリームは permission_error(output, stream, S)。
// ファイルへの書き込みは close と flush のほかに、halt と Machine の終わりでも書き出す（レビュー M5）。
// テスト: tests/pl/close.pl, tests/cpp/stream_test.cpp, tests/cpp/file_flush_test.cpp,
//   tests/cpp/output_stop_test.cpp

#include "stream_internal.hpp"
#include "mosa/path_text.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace mosa {
namespace streams {
namespace {

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
  if (stream.alias) return machine.heap.atom(machine.atoms.intern(*stream.alias));
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
// 前に保存した流れも on_disk を 0 にする。ファイルはもう相手の中身なので、続きだけを書くと相手の文の上に自分の
// 続きを足してしまう（レビュー 2026-11-20 の H-22）。続きだけを書くのは、外で変えられたファイルに限る。
// 読む流れは何も書かないので、同じファイルへ書いて放置した流れの中身を消してはいけない。
void settle_same_file(Impl& machine, std::size_t index) {
  const Stream& saved = machine.streams[index];
  if (saved.kind != Stream::Kind::File || saved.mode == Stream::Mode::Read) return;
  for (std::size_t other = 0; other < machine.streams.size(); ++other) {
    Stream& stream = machine.streams[other];
    if (other == index || stream.closed || stream.kind != Stream::Kind::File) continue;
    // 戻す物の無い流れは、ファイルを比べずに飛ばす（same_file はファイルシステムに尋ねる）。
    if (stream.mode == Stream::Mode::Read || (!stream.unsaved && stream.on_disk == 0)) continue;
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

}  // namespace

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

}  // namespace streams

void flush_open_files(Impl& machine) {
  for (std::size_t index = 0; index < machine.streams.size(); ++index) {
    Stream& stream = machine.streams[index];
    // 閉じた流れの中身は close で書いた後の古い写しなので、外で変えられた中身を戻してしまう。
    if (stream.closed || stream.kind != Stream::Kind::File || !stream.unsaved) continue;
    // 書き出せなくても終わり方は変えない。close と違い、誤りを返す相手がいない。
    (void)streams::save_output(index, stream);
  }
}

}  // namespace mosa
