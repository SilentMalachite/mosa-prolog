// 流れの組込を見る: 開く、別名、書く、性質、位置、閉じる、標準の流れ。流れから読む組込は stream_read_test.cpp に
// あり、同じ実行ファイルに入る（800 行の上限で分けた）。
#include "harness.hpp"
#include "stream_support.hpp"
#include "mosa/machine.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

using stream_support::binding_named;
using stream_support::formal_text;
using stream_support::path_atom;
using stream_support::write_bytes;

std::string file_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

void check_memory_index() {
  auto machine = mosa::Machine::create();
  auto stop = machine->query("open_string('ab', S), open_string([99], T).");
  mosa_test::check(stop.kind == mosa::StopKind::Success, "two memory streams");
  mosa_test::check(binding_named(*machine, stop, "S") == "'$stream'(3)", "first index");
  mosa_test::check(binding_named(*machine, stop, "T") == "'$stream'(4)", "second index");
}

void check_file_modes(const std::filesystem::path& path) {
  write_bytes(path, "abc");
  const std::string atom = path_atom(path);
  auto reading = mosa::Machine::create();
  auto read = reading->query("open(" + atom + ", read, S).");
  mosa_test::check(read.kind == mosa::StopKind::Success, "read opens");
  mosa_test::check(binding_named(*reading, read, "S") == "'$stream'(3)", "read index");
  mosa_test::check(file_bytes(path) == "abc", "read keeps bytes");

  auto writing = mosa::Machine::create();
  auto wrote = writing->query("open(" + atom + ", write, S, [type(binary), encoding(utf8)]).");
  mosa_test::check(wrote.kind == mosa::StopKind::Success, "write opens");
  mosa_test::check(file_bytes(path).empty(), "write truncates");

  write_bytes(path, "abc");
  auto appending = mosa::Machine::create();
  auto appended = appending->query("open(" + atom + ", append, S, [eof_action(reset), reposition(false)]).");
  mosa_test::check(appended.kind == mosa::StopKind::Success, "append opens");
  mosa_test::check(file_bytes(path) == "abc", "append keeps bytes");
}

void check_japanese_path(const std::filesystem::path& path) {
  write_bytes(path, "\xE3\x81\x82");
  auto machine = mosa::Machine::create();
  auto stop = machine->query("open(" + path_atom(path) + ", read, S).");
  mosa_test::check(stop.kind == mosa::StopKind::Success, "japanese path");
  mosa_test::check(binding_named(*machine, stop, "S") == "'$stream'(3)", "japanese index");
}

void check_alias_blocks_real_file(const std::filesystem::path& path) {
  write_bytes(path, "a");
  auto machine = mosa::Machine::create();
  auto stop = machine->query("open(" + path_atom(path) + ", read, S, [alias(user_output)]).");
  mosa_test::check(stop.kind == mosa::StopKind::Exception, "alias is taken");
  mosa_test::check(file_bytes(path) == "a", "alias error keeps bytes");
}

void check_put_code(const std::filesystem::path& path) {
  std::ostringstream caught;
  auto* old = std::cout.rdbuf(caught.rdbuf());
  auto machine = mosa::Machine::create();
  auto stop = machine->query("put_code(97), nl, put_code(10).");
  std::cout.rdbuf(old);
  mosa_test::check(stop.kind == mosa::StopKind::Success, "put_code stdout");
  mosa_test::check(caught.str() == "a\n\n", "put_code stdout bytes");

  write_bytes(path, "xyz");
  const std::string atom = path_atom(path);
  std::ostringstream quiet;
  auto* old_quiet = std::cout.rdbuf(quiet.rdbuf());
  auto file_machine = mosa::Machine::create();
  auto wrote = file_machine->query("open(" + atom + ", write, S), put_code(S, 97), char_code('親', C), put_code(S, C), nl(S).");
  std::cout.rdbuf(old_quiet);
  mosa_test::check(wrote.kind == mosa::StopKind::Success, "put_code file");
  mosa_test::check(quiet.str().empty(), "put_code file stays off stdout");
  mosa_test::check(file_bytes(path).empty(), "put_code waits for flush");
  mosa_test::check(file_machine->query("flush_output('$stream'(3)).").kind == mosa::StopKind::Success, "put_code flush");
  mosa_test::check(file_bytes(path) == "a\xE8\xA6\xAA\n", "put_code flushed bytes");

  auto denied = file_machine->query("put_code('$stream'(3), -1).");
  mosa_test::check(denied.kind == mosa::StopKind::Exception, "put_code range");
  mosa_test::check(formal_text(*file_machine, denied) == "representation_error(character_code)", "put_code range formal");
  mosa_test::check(file_machine->query("flush_output('$stream'(3)).").kind == mosa::StopKind::Success, "range flush");
  mosa_test::check(file_bytes(path) == "a\xE8\xA6\xAA\n", "range does not write");

  std::ostringstream still;
  auto* old_still = std::cout.rdbuf(still.rdbuf());
  auto current = mosa::Machine::create();
  auto switched = current->query("open(" + atom +
                                  ", write, S), set_output(S), put_code(98), nl, set_output(user_output), flush_output(S).");
  std::cout.rdbuf(old_still);
  mosa_test::check(switched.kind == mosa::StopKind::Success, "nl via current");
  mosa_test::check(still.str().empty(), "nl stays off stdout");
  mosa_test::check(file_bytes(path) == "b\n", "current nl bytes");

  auto binary = mosa::Machine::create();
  auto blocked = binary->query("open(" + atom + ", write, S, [type(binary)]), put_code(S, 97).");
  mosa_test::check(blocked.kind == mosa::StopKind::Exception, "put_code binary");
  mosa_test::check(formal_text(*binary, blocked) == "permission_error(output,binary_stream,'$stream'(3))",
                   "put_code binary formal");
  mosa_test::check(binary->query("put_byte('$stream'(3), 65), flush_output('$stream'(3)).").kind ==
                       mosa::StopKind::Success,
                   "binary put after put_code");
  mosa_test::check(file_bytes(path) == "A", "put_code wrote no byte");
}

void check_put_char(const std::filesystem::path& path) {
  std::ostringstream caught;
  auto* old = std::cout.rdbuf(caught.rdbuf());
  auto machine = mosa::Machine::create();
  auto stop = machine->query("put_char(a), put_char('あ').");
  std::cout.rdbuf(old);
  mosa_test::check(stop.kind == mosa::StopKind::Success, "put stdout");
  mosa_test::check(caught.str() == "aあ", "put stdout bytes");

  std::ostringstream err;
  auto* old_err = std::cerr.rdbuf(err.rdbuf());
  auto error_machine = mosa::Machine::create();
  auto error_stop = error_machine->query("put_char(user_error, b).");
  std::cerr.rdbuf(old_err);
  mosa_test::check(error_stop.kind == mosa::StopKind::Success, "put stderr");
  mosa_test::check(err.str() == "b", "put stderr bytes");

  write_bytes(path, "xyz");
  std::ostringstream quiet;
  auto* old_quiet = std::cout.rdbuf(quiet.rdbuf());
  auto file_machine = mosa::Machine::create();
  const std::string atom = path_atom(path);
  auto opened = file_machine->query("open(" + atom + ", write, S), put_char(S, a).");
  std::cout.rdbuf(old_quiet);
  mosa_test::check(opened.kind == mosa::StopKind::Success, "put file");
  mosa_test::check(quiet.str().empty(), "put file stays off stdout");
  mosa_test::check(file_bytes(path).empty(), "put file waits for flush");
}

void check_put_bytes(const std::filesystem::path& path, const std::filesystem::path& reset_path) {
  write_bytes(path, "xyz");
  const std::string atom = path_atom(path);
  std::ostringstream quiet;
  auto* old = std::cout.rdbuf(quiet.rdbuf());
  auto machine = mosa::Machine::create();
  auto wrote = machine->query("open(" + atom + ", write, S, [type(binary)]), put_byte(S, 65).");
  std::cout.rdbuf(old);
  mosa_test::check(wrote.kind == mosa::StopKind::Success, "put byte");
  mosa_test::check(quiet.str().empty(), "put byte stays off stdout");
  mosa_test::check(file_bytes(path).empty(), "put byte waits for flush");
  auto bad = machine->query("put_byte('$stream'(3), 256).");
  mosa_test::check(bad.kind == mosa::StopKind::Exception, "byte range throws");
  mosa_test::check(formal_text(*machine, bad) == "type_error(byte,256)", "byte range formal");

  write_bytes(reset_path, "ab");
  const std::string reset_atom = path_atom(reset_path);
  auto resetting = mosa::Machine::create();
  auto goal = "open(" + reset_atom + ", read, S, [type(binary), eof_action(reset)]), get_byte(S, A), get_byte(S, B), get_byte(S, E), get_byte(S, C).";
  auto again = resetting->query(goal);
  mosa_test::check(again.kind == mosa::StopKind::Success, "byte reset");
  mosa_test::check(binding_named(*resetting, again, "A") == "97", "byte reset a");
  mosa_test::check(binding_named(*resetting, again, "E") == "-1", "byte reset eof");
  mosa_test::check(binding_named(*resetting, again, "C") == "97", "byte reset rereads");
}

void check_flush_writes(const std::filesystem::path& text_path, const std::filesystem::path& append_path,
                        const std::filesystem::path& byte_path) {
  const std::string text = path_atom(text_path);
  auto machine = mosa::Machine::create();
  auto flushed = machine->query("open(" + text + ", write, S), put_char(S, a), put_char(S, '\\n'), flush_output(S).");
  mosa_test::check(flushed.kind == mosa::StopKind::Success, "flush text");
  mosa_test::check(file_bytes(text_path) == "a\n", "flush text bytes");

  write_bytes(append_path, "ab");
  const std::string append = path_atom(append_path);
  auto appending = mosa::Machine::create();
  auto added = appending->query("open(" + append + ", append, S), put_char(S, c), flush_output(S).");
  mosa_test::check(added.kind == mosa::StopKind::Success, "flush append");
  mosa_test::check(file_bytes(append_path) == "abc", "flush append bytes");

  const std::string binary = path_atom(byte_path);
  auto closing = mosa::Machine::create();
  auto closed = closing->query("open(" + binary + ", write, S, [type(binary)]), put_byte(S, 65), close(S).");
  mosa_test::check(closed.kind == mosa::StopKind::Success, "close writes");
  mosa_test::check(file_bytes(byte_path) == "A", "close bytes");
  auto again = closing->query("close('$stream'(3)).");
  mosa_test::check(again.kind == mosa::StopKind::Exception, "close twice");
  mosa_test::check(formal_text(*closing, again) == "existence_error(stream,'$stream'(3))", "close twice formal");
}

void check_alias_reopen(const std::filesystem::path& path) {
  const std::string atom = path_atom(path);
  auto machine = mosa::Machine::create();
  auto first = machine->query("open(" + atom + ", write, S, [alias(mine)]), put_char(S, a), close(mine).");
  mosa_test::check(first.kind == mosa::StopKind::Success, "close alias");
  mosa_test::check(file_bytes(path) == "a", "alias wrote a");
  auto second = machine->query("close(mine).");
  mosa_test::check(second.kind == mosa::StopKind::Exception, "alias stays closed");
  mosa_test::check(formal_text(*machine, second) == "existence_error(stream,mine)", "alias formal");
  auto third = machine->query("open(" + atom + ", write, S, [alias(mine)]), put_char(S, b), close(S).");
  mosa_test::check(third.kind == mosa::StopKind::Success, "alias reused");
  mosa_test::check(file_bytes(path) == "b", "alias wrote b");
}

void check_stream_property(const std::filesystem::path& path) {
  write_bytes(path, "ab");
  const std::string atom = path_atom(path);
  auto named = mosa::Machine::create();
  auto found = named->query("open(" + atom + ", read, S), stream_property(S, file_name(" + atom + ")).");
  mosa_test::check(found.kind == mosa::StopKind::Success, "file_name");
  auto placed = named->query("get_char('$stream'(3), _), stream_property('$stream'(3), position(stream_position(1, 1, 2))).");
  mosa_test::check(placed.kind == mosa::StopKind::Success, "file position");

  auto binary = mosa::Machine::create();
  auto byte = binary->query("open(" + atom + ", read, S, [type(binary)]), get_byte(S, _), stream_property(S, position(stream_position(1, 1, 1))).");
  mosa_test::check(byte.kind == mosa::StopKind::Success, "binary position");

  auto fixed = mosa::Machine::create();
  auto still = fixed->query("open(" + atom + ", read, S, [reposition(false)]), stream_property(S, position(_)).");
  mosa_test::check(still.kind == mosa::StopKind::Failure, "no position");
  auto flag = fixed->query("stream_property('$stream'(3), reposition(false)).");
  mosa_test::check(flag.kind == mosa::StopKind::Success, "reposition false");
}

void check_set_output(const std::filesystem::path& path) {
  write_bytes(path, "xyz");
  const std::string atom = path_atom(path);
  std::ostringstream quiet;
  auto* old = std::cout.rdbuf(quiet.rdbuf());
  auto machine = mosa::Machine::create();
  auto wrote = machine->query(
      "open(" + atom + ", write, S), set_output(S), put_char(a), set_output(user_output), flush_output(S).");
  std::cout.rdbuf(old);
  mosa_test::check(wrote.kind == mosa::StopKind::Success, "set_output writes");
  mosa_test::check(quiet.str().empty(), "set_output stays off stdout");
  mosa_test::check(file_bytes(path) == "a", "set_output flushed");

  write_bytes(path, "ab");
  std::ostringstream still;
  auto* old_still = std::cout.rdbuf(still.rdbuf());
  auto appending = mosa::Machine::create();
  auto added = appending->query(
      "open(" + atom + ", append, S), set_output(S), put_char(c), set_output(user_output), flush_output(S).");
  std::cout.rdbuf(old_still);
  mosa_test::check(added.kind == mosa::StopKind::Success, "set_output append");
  mosa_test::check(still.str().empty(), "append stays off stdout");
  mosa_test::check(file_bytes(path) == "abc", "append via current");
}

void check_at_end(const std::filesystem::path& path) {
  write_bytes(path, "");
  const std::string atom = path_atom(path);
  auto empty = mosa::Machine::create();
  auto at = empty->query("open(" + atom + ", read, S, [type(binary)]), at_end_of_stream(S).");
  mosa_test::check(at.kind == mosa::StopKind::Success, "binary empty is at");

  write_bytes(path, "a");
  auto mid = mosa::Machine::create();
  auto not_yet = mid->query("open(" + atom + ", read, S, [type(binary)]), at_end_of_stream(S).");
  mosa_test::check(not_yet.kind == mosa::StopKind::Failure, "binary has a byte");
  auto after = mid->query("get_byte('$stream'(3), _), at_end_of_stream('$stream'(3)).");
  mosa_test::check(after.kind == mosa::StopKind::Success, "binary after the byte");

  // user_input は close しても閉じない（iso-notes 8.11.6.1 の A）。閉じた流れはファイルで作る。
  auto closed_in = mosa::Machine::create();
  auto closed = closed_in->query("open(" + atom + ", read, S), close(S), at_end_of_stream(S).");
  mosa_test::check(closed.kind == mosa::StopKind::Exception, "closed input");
  mosa_test::check(formal_text(*closed_in, closed) == "existence_error(stream,'$stream'(3))",
                   "at_end closed formal");
}

void check_set_position(const std::filesystem::path& path) {
  write_bytes(path, "ab");
  const std::string atom = path_atom(path);
  auto binary = mosa::Machine::create();
  auto back = binary->query("open(" + atom +
                             ", read, S, [type(binary)]), get_byte(S, 97), "
                             "set_stream_position(S, stream_position(0, 1, 1)), get_byte(S, C).");
  mosa_test::check(back.kind == mosa::StopKind::Success, "binary seek");
  mosa_test::check(binding_named(*binary, back, "C") == "97", "binary byte again");

  auto fixed = mosa::Machine::create();
  auto denied = fixed->query("open(" + atom +
                              ", read, S, [reposition(false)]), get_char(S, a), "
                              "set_stream_position(S, stream_position(0, 1, 1)).");
  mosa_test::check(denied.kind == mosa::StopKind::Exception, "reposition false");
  mosa_test::check(formal_text(*fixed, denied) == "permission_error(reposition,stream,'$stream'(3))",
                   "reposition formal");
  auto next = fixed->query("get_char('$stream'(3), C).");
  mosa_test::check(next.kind == mosa::StopKind::Success, "seek did not move");
  mosa_test::check(binding_named(*fixed, next, "C") == "b", "next char stays");

  auto writing = mosa::Machine::create();
  auto wrote = writing->query("open(" + atom +
                               ", write, S), put_char(S, a), put_char(S, b), "
                               "set_stream_position(S, stream_position(0, 1, 1)), put_char(S, c), flush_output(S).");
  mosa_test::check(wrote.kind == mosa::StopKind::Success, "write seek");
  mosa_test::check(file_bytes(path) == "cb", "overwrite at start");
}

void check_write_stream(const std::filesystem::path& text_path, const std::filesystem::path& binary_path) {
  write_bytes(text_path, "z");
  auto text = mosa::Machine::create();
  const std::string text_atom = path_atom(text_path);
  auto wrote = text->query("open(" + text_atom + ", write, S), write(S, 東), flush_output(S).");
  mosa_test::check(wrote.kind == mosa::StopKind::Success, "write file");
  mosa_test::check(file_bytes(text_path) == "\xE6\x9D\xB1", "write file bytes");

  write_bytes(binary_path, "a");
  auto binary = mosa::Machine::create();
  const std::string binary_atom = path_atom(binary_path);
  mosa_test::check(binary->query("open(" + binary_atom + ", write, S, [type(binary)]).").kind == mosa::StopKind::Success,
                   "write binary open");
  auto denied = binary->query("write('$stream'(3), a).");
  mosa_test::check(formal_text(*binary, denied) == "permission_error(output,binary_stream,'$stream'(3))",
                   "write binary formal");
  mosa_test::check(binary->query("put_byte('$stream'(3), 97), flush_output('$stream'(3)).").kind ==
                       mosa::StopKind::Success,
                   "write binary keeps");
  mosa_test::check(file_bytes(binary_path) == "a", "write binary put");

  mosa_test::check(text->query("close('$stream'(3)).").kind == mosa::StopKind::Success, "write close");
  auto again = text->query("write('$stream'(3), a).");
  mosa_test::check(again.kind == mosa::StopKind::Exception, "write closed");
  mosa_test::check(formal_text(*text, again) == "existence_error(stream,'$stream'(3))", "write closed formal");
}

void check_close_standard() {
  auto machine = mosa::Machine::create();
  auto closed = machine->query("close(user_output).");
  mosa_test::check(closed.kind == mosa::StopKind::Success, "close user_output");
  // 標準ストリームは吐き出すだけで閉じない（iso-notes 8.11.6.1 の A、レビュー L13）。
  auto kept = machine->query("current_output(S), stream_property(S, alias(user_output)), flush_output(S).");
  mosa_test::check(kept.kind == mosa::StopKind::Success, "user_output stays open and current after close");
}

// consult は通常ファイルだけを、ヒープ上限のバイト予算の中で読む（全体レビュー推奨2、持ち主の選択 B）。
// 1 MiB の機械では、2 MiB の原文は中身が注釈だけでも上限で止まる。
void check_consult_source_limit(const std::filesystem::path& big_path, const std::filesystem::path& small_path) {
  write_bytes(big_path, "%" + std::string(2 * 1024 * 1024, 'x') + "\n");
  write_bytes(small_path, "lazy_ok(1).\n");
  auto machine = mosa::Machine::create(131072);
  bool limited = false;
  try {
    (void)machine->consult(big_path.generic_string());
  } catch (const mosa::ResourceLimit&) {
    limited = true;
  }
  mosa_test::check(limited, "consult stops a source past the heap byte budget");
  auto small = mosa::Machine::create(131072);
  mosa_test::check(small->consult(small_path.generic_string()), "consult loads a small source under the budget");
  mosa_test::check(small->query("lazy_ok(1).").kind == mosa::StopKind::Success, "consult small source answers");
}

void check_flush_failure(const std::filesystem::path& path) {
  write_bytes(path, "z");
  const std::string atom = path_atom(path);
  auto machine = mosa::Machine::create();
  auto opened = machine->query("open(" + atom + ", write, S), put_char(S, a).");
  mosa_test::check(opened.kind == mosa::StopKind::Success, "open before block");
  std::filesystem::remove(path);
  std::filesystem::create_directory(path);
  auto failed = machine->query("flush_output('$stream'(3)).");
  mosa_test::check(failed.kind == mosa::StopKind::Exception, "flush blocked");
  mosa_test::check(formal_text(*machine, failed) == "system_error", "flush system_error");
  auto kept = machine->query("close('$stream'(3)).");
  mosa_test::check(kept.kind == mosa::StopKind::Exception, "close force false");
  auto forced = machine->query("close('$stream'(3), [force(true)]).");
  mosa_test::check(forced.kind == mosa::StopKind::Success, "close force true");
  std::filesystem::remove_all(path);
}

}  // namespace

// iso-notes 8.11 の決定 B。user_input は eof_action(reset)。終わりの後も読み直せ、読んだ分を巻き戻しはしない。
void check_standard_reset() {
  std::stringstream feed;
  feed << "a. ";
  auto* old = std::cin.rdbuf(feed.rdbuf());
  auto machine = mosa::Machine::create();
  auto first = machine->query("read(user_input, X), X == a, read(user_input, Y), Y == end_of_file.");
  mosa_test::check(first.kind == mosa::StopKind::Success, "standard first read");
  auto again = machine->query("read(user_input, W), W == end_of_file.");
  mosa_test::check(again.kind == mosa::StopKind::Success, "standard reset no replay");
  feed << "b. ";
  auto more = machine->query("read(user_input, Z), Z == b.");
  mosa_test::check(more.kind == mosa::StopKind::Success, "standard reset reads on");
  std::cin.rdbuf(old);
  std::cin.clear();
}

void check_eof_actions(const std::filesystem::path& reset_path, const std::filesystem::path& error_path,
                       const std::filesystem::path& binary_path, const std::filesystem::path& bad_path);
void check_get_code(const std::filesystem::path& binary_path, const std::filesystem::path& bad_path,
                    const std::filesystem::path& eof_path);
void check_get_bytes(const std::filesystem::path& path, const std::filesystem::path& empty_path);
void check_peek_byte(const std::filesystem::path& path, const std::filesystem::path& empty_path);
void check_read_term(const std::filesystem::path& eof_path, const std::filesystem::path& reset_path,
                     const std::filesystem::path& binary_path, const std::filesystem::path& bad_path);
void check_invalid_utf8(const std::filesystem::path& path, const std::filesystem::path& peek_path,
                        const std::filesystem::path& read_path, const std::filesystem::path& error_path);
void check_lazy_read(const std::filesystem::path& path, const std::filesystem::path& big_path);

int main() {
  const std::filesystem::path root{MOSA_SOURCE_DIR};
  const auto dir = root / "tests" / "tmp";
  std::filesystem::create_directories(dir);
  check_memory_index();
  check_file_modes(dir / "phase3-open.txt");
  check_japanese_path(dir / "親.txt");
  check_alias_blocks_real_file(dir / "phase3-alias.txt");
  check_eof_actions(dir / "phase3-reset.txt", dir / "phase3-eof-error.txt", dir / "phase3-binary.txt",
                    dir / "phase3-bad-utf.txt");
  check_get_code(dir / "phase3-code-binary.txt", dir / "phase3-code-utf.txt", dir / "phase3-code-eof.txt");
  check_put_char(dir / "phase3-put.txt");
  check_put_code(dir / "phase3-put-code.txt");
  check_get_bytes(dir / "phase3-bytes.txt", dir / "phase3-bytes-empty.txt");
  check_peek_byte(dir / "phase3-peek-byte.txt", dir / "phase3-peek-empty.txt");
  check_read_term(dir / "phase3-read-eof.txt", dir / "phase3-read-reset.txt", dir / "phase3-read-binary.txt",
                  dir / "phase3-read-utf.txt");
  check_invalid_utf8(dir / "phase3-invalid-utf8.txt", dir / "phase3-invalid-peek.txt",
                     dir / "phase3-invalid-read.txt", dir / "phase3-invalid-error.txt");
  check_write_stream(dir / "phase3-write.txt", dir / "phase3-write-binary.txt");
  check_put_bytes(dir / "phase3-put-byte.txt", dir / "phase3-byte-reset.txt");
  check_flush_writes(dir / "phase3-flush.txt", dir / "phase3-flush-append.txt", dir / "phase3-close-byte.txt");
  check_alias_reopen(dir / "phase3-alias-reopen.txt");
  check_stream_property(dir / "phase3-property.txt");
  check_set_output(dir / "phase3-set-output.txt");
  check_set_position(dir / "phase3-set-position.txt");
  check_at_end(dir / "phase3-at-end.txt");
  check_lazy_read(dir / "phase3-lazy.txt", dir / "phase3-lazy-big.txt");
  check_consult_source_limit(dir / "phase3-consult-big.pl", dir / "phase3-consult-small.pl");
  check_close_standard();
  check_standard_reset();
  check_flush_failure(dir / "phase3-flush-fail");
  return mosa_test::failures();
}
