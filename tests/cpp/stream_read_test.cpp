// stream_test.cpp の続き（800 行の上限で分けた）。流れから読む組込を見る: 終わりの後の eof_action、get_code/2 と
// get_char/2、get_byte/2 と peek_byte/2、read_term/3、壊れた UTF-8、開いた後に読み足す読みの流れ。main は
// stream_test.cpp にあり、同じ実行ファイルに入る。
#include "harness.hpp"
#include "stream_support.hpp"
#include "mosa/machine.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

using stream_support::binding_named;
using stream_support::formal_text;
using stream_support::path_atom;
using stream_support::write_bytes;

void check_eof_actions(const std::filesystem::path& reset_path, const std::filesystem::path& error_path,
                       const std::filesystem::path& binary_path, const std::filesystem::path& bad_path) {
  write_bytes(reset_path, "ab");
  auto resetting = mosa::Machine::create();
  const std::string reset_atom = path_atom(reset_path);
  mosa_test::check(resetting->query("open(" + reset_atom + ", read, S, [eof_action(reset)]).").kind ==
                       mosa::StopKind::Success,
                   "reset open");
  auto first = resetting->query("get_char('$stream'(3), C).");
  mosa_test::check(binding_named(*resetting, first, "C") == "a", "reset first");
  auto second = resetting->query("get_char('$stream'(3), C).");
  mosa_test::check(binding_named(*resetting, second, "C") == "b", "reset second");
  auto ended = resetting->query("get_char('$stream'(3), C).");
  mosa_test::check(binding_named(*resetting, ended, "C") == "end_of_file", "reset eof");
  auto again = resetting->query("get_char('$stream'(3), C).");
  mosa_test::check(binding_named(*resetting, again, "C") == "a", "reset rereads");

  write_bytes(error_path, "");
  auto erroring = mosa::Machine::create();
  const std::string error_atom = path_atom(error_path);
  mosa_test::check(erroring->query("open(" + error_atom + ", read, S, [eof_action(error)]).").kind ==
                       mosa::StopKind::Success,
                   "error open");
  auto at_end = erroring->query("get_char('$stream'(3), C).");
  mosa_test::check(binding_named(*erroring, at_end, "C") == "end_of_file", "error first eof");
  auto past = erroring->query("get_char('$stream'(3), C).");
  mosa_test::check(past.kind == mosa::StopKind::Exception, "error past throws");
  mosa_test::check(formal_text(*erroring, past) == "permission_error(input,past_end_of_stream,'$stream'(3))",
                   "error past formal");

  write_bytes(binary_path, "a");
  auto binary = mosa::Machine::create();
  const std::string binary_atom = path_atom(binary_path);
  mosa_test::check(binary->query("open(" + binary_atom + ", read, S, [type(binary)]).").kind == mosa::StopKind::Success,
                   "binary open");
  auto denied = binary->query("get_char('$stream'(3), C).");
  mosa_test::check(denied.kind == mosa::StopKind::Exception, "binary get throws");
  mosa_test::check(formal_text(*binary, denied) == "permission_error(input,binary_stream,'$stream'(3))",
                   "binary formal");

  write_bytes(bad_path, "\xFF");
  auto broken = mosa::Machine::create();
  const std::string bad_atom = path_atom(bad_path);
  mosa_test::check(broken->query("open(" + bad_atom + ", read, S).").kind == mosa::StopKind::Success, "bad open");
  auto utf = broken->query("get_char('$stream'(3), C).");
  mosa_test::check(utf.kind == mosa::StopKind::Exception, "bad utf throws");
  mosa_test::check(formal_text(*broken, utf) == "representation_error(character)", "bad utf formal");
}

void check_get_code(const std::filesystem::path& binary_path, const std::filesystem::path& bad_path,
                    const std::filesystem::path& eof_path) {
  write_bytes(binary_path, "a");
  auto binary = mosa::Machine::create();
  const std::string binary_atom = path_atom(binary_path);
  mosa_test::check(binary->query("open(" + binary_atom + ", read, S, [type(binary)]).").kind ==
                       mosa::StopKind::Success,
                   "code binary open");
  auto denied = binary->query("get_code('$stream'(3), C).");
  mosa_test::check(denied.kind == mosa::StopKind::Exception, "code binary throws");
  mosa_test::check(formal_text(*binary, denied) == "permission_error(input,binary_stream,'$stream'(3))",
                   "code binary formal");
  mosa_test::check(binary->query("get_byte('$stream'(3), 97).").kind == mosa::StopKind::Success,
                   "code binary keeps the byte");

  // get_code の誤りは壊れたバイトを進める（iso-notes 第4節の決定 A、レビュー L6）。peek_code が次の壊れたバイトで
  // 止まることを見るため 2 バイトにする。前は 1 バイトで、get_code の後も同じバイトに立っていた。
  write_bytes(bad_path, "\xFF\xFF");
  auto broken = mosa::Machine::create();
  const std::string bad_atom = path_atom(bad_path);
  mosa_test::check(broken->query("open(" + bad_atom + ", read, S).").kind == mosa::StopKind::Success, "code bad open");
  auto utf = broken->query("get_code('$stream'(3), C).");
  mosa_test::check(utf.kind == mosa::StopKind::Exception, "code bad utf throws");
  mosa_test::check(formal_text(*broken, utf) == "representation_error(character)", "code bad utf formal");
  auto peeked = broken->query("peek_code('$stream'(3), C).");
  mosa_test::check(formal_text(*broken, peeked) == "representation_error(character)", "peek code stays on bad utf");
  auto peeked_again = broken->query("peek_code('$stream'(3), C).");
  mosa_test::check(formal_text(*broken, peeked_again) == "representation_error(character)", "peek code does not advance");

  write_bytes(eof_path, "");
  auto ending = mosa::Machine::create();
  const std::string eof_atom = path_atom(eof_path);
  mosa_test::check(ending->query("open(" + eof_atom + ", read, S, [eof_action(error)]).").kind ==
                       mosa::StopKind::Success,
                   "code eof open");
  auto once = ending->query("get_code('$stream'(3), C).");
  mosa_test::check(binding_named(*ending, once, "C") == "-1", "code first eof");
  auto twice = ending->query("get_code('$stream'(3), C).");
  mosa_test::check(formal_text(*ending, twice) == "permission_error(input,past_end_of_stream,'$stream'(3))",
                   "code past formal");
}

void check_get_bytes(const std::filesystem::path& path, const std::filesystem::path& empty_path) {
  write_bytes(path, "ab");
  const std::string atom = path_atom(path);
  auto machine = mosa::Machine::create();
  auto read = machine->query("open(" + atom + ", read, S, [type(binary)]), get_byte(S, A), get_byte(S, B).");
  mosa_test::check(read.kind == mosa::StopKind::Success, "get bytes");
  mosa_test::check(binding_named(*machine, read, "A") == "97", "byte a");
  mosa_test::check(binding_named(*machine, read, "B") == "98", "byte b");

  auto consumed = mosa::Machine::create();
  auto skip = consumed->query("open(" + atom + ", read, S, [type(binary)]), (get_byte(S, 98) ; true), get_byte(S, C).");
  mosa_test::check(skip.kind == mosa::StopKind::Success, "byte fail consumes");
  mosa_test::check(binding_named(*consumed, skip, "C") == "98", "byte after fail");

  auto typed = mosa::Machine::create();
  auto bad = typed->query("open(" + atom + ", read, S, [type(binary)]), get_byte(S, 256).");
  mosa_test::check(bad.kind == mosa::StopKind::Exception, "in_byte throws");
  mosa_test::check(formal_text(*typed, bad) == "type_error(in_byte,256)", "in_byte formal");

  auto text = mosa::Machine::create();
  auto denied = text->query("open(" + atom + ", read, S), get_byte(S, B).");
  mosa_test::check(denied.kind == mosa::StopKind::Exception, "text get_byte throws");
  auto still = text->query("get_char('$stream'(3), C).");
  mosa_test::check(binding_named(*text, still, "C") == "a", "text byte leaves char");

  write_bytes(empty_path, "");
  const std::string empty_atom = path_atom(empty_path);
  auto ending = mosa::Machine::create();
  mosa_test::check(ending->query("open(" + empty_atom + ", read, S, [type(binary), eof_action(error)]).").kind ==
                       mosa::StopKind::Success,
                   "byte eof open");
  auto once = ending->query("get_byte('$stream'(3), B).");
  mosa_test::check(binding_named(*ending, once, "B") == "-1", "byte eof");
  auto twice = ending->query("get_byte('$stream'(3), B).");
  mosa_test::check(twice.kind == mosa::StopKind::Exception, "byte past throws");
  mosa_test::check(formal_text(*ending, twice) == "permission_error(input,past_end_of_stream,'$stream'(3))",
                   "byte past formal");
}

void check_peek_byte(const std::filesystem::path& path, const std::filesystem::path& empty_path) {
  write_bytes(path, "ab");
  const std::string atom = path_atom(path);
  auto machine = mosa::Machine::create();
  auto peeked = machine->query(
      "open(" + atom + ", read, S, [type(binary)]), peek_byte(S, A), peek_byte(S, B), get_byte(S, C).");
  mosa_test::check(peeked.kind == mosa::StopKind::Success, "peek byte");
  mosa_test::check(binding_named(*machine, peeked, "A") == "97", "peek first");
  mosa_test::check(binding_named(*machine, peeked, "B") == "97", "peek again");
  mosa_test::check(binding_named(*machine, peeked, "C") == "97", "get after peek");

  auto kept = mosa::Machine::create();
  auto skip = kept->query("open(" + atom + ", read, S, [type(binary)]), (peek_byte(S, 99) ; true), get_byte(S, C).");
  mosa_test::check(skip.kind == mosa::StopKind::Success, "peek fail keeps");
  mosa_test::check(binding_named(*kept, skip, "C") == "97", "byte after peek fail");

  auto typed = mosa::Machine::create();
  auto bad = typed->query("open(" + atom + ", read, S, [type(binary)]), peek_byte(S, 256).");
  mosa_test::check(formal_text(*typed, bad) == "type_error(in_byte,256)", "peek in_byte formal");
  mosa_test::check(typed->query("get_byte('$stream'(3), 97).").kind == mosa::StopKind::Success, "peek type keeps");

  auto current = mosa::Machine::create();
  auto once = current->query("open(" + atom +
                              ", read, S, [type(binary)]), set_input(S), peek_byte(B), set_input(user_input).");
  mosa_test::check(once.kind == mosa::StopKind::Success, "peek current");
  mosa_test::check(binding_named(*current, once, "B") == "97", "peek current byte");

  write_bytes(empty_path, "");
  const std::string empty_atom = path_atom(empty_path);
  auto ending = mosa::Machine::create();
  auto at = ending->query("open(" + empty_atom +
                           ", read, S, [type(binary)]), peek_byte(S, -1), stream_property(S, end_of_stream(at)).");
  mosa_test::check(at.kind == mosa::StopKind::Success, "peek eof stays at");
  auto past = ending->query("get_byte('$stream'(3), -1), stream_property('$stream'(3), end_of_stream(past)).");
  mosa_test::check(past.kind == mosa::StopKind::Success, "get eof goes past");

  write_bytes(path, "");
  auto erroring = mosa::Machine::create();
  const std::string error_atom = path_atom(path);
  mosa_test::check(erroring->query("open(" + error_atom + ", read, S, [type(binary), eof_action(error)]).").kind ==
                       mosa::StopKind::Success,
                   "peek error open");
  mosa_test::check(erroring->query("peek_byte('$stream'(3), -1), peek_byte('$stream'(3), -1).").kind ==
                       mosa::StopKind::Success,
                   "peek eof does not go past");
  mosa_test::check(erroring->query("get_byte('$stream'(3), -1).").kind == mosa::StopKind::Success, "get after peek eof");
  auto blocked = erroring->query("peek_byte('$stream'(3), B).");
  mosa_test::check(formal_text(*erroring, blocked) == "permission_error(input,past_end_of_stream,'$stream'(3))",
                   "peek after past");
}

void check_read_term(const std::filesystem::path& eof_path, const std::filesystem::path& reset_path,
                     const std::filesystem::path& binary_path, const std::filesystem::path& bad_path) {
  write_bytes(eof_path, "");
  auto ending = mosa::Machine::create();
  const std::string eof_atom = path_atom(eof_path);
  mosa_test::check(ending->query("open(" + eof_atom + ", read, S, [eof_action(error)]).").kind ==
                       mosa::StopKind::Success,
                   "read eof open");
  auto once = ending->query("read('$stream'(3), T).");
  mosa_test::check(binding_named(*ending, once, "T") == "end_of_file", "read first eof");
  auto twice = ending->query("read('$stream'(3), T).");
  mosa_test::check(formal_text(*ending, twice) == "permission_error(input,past_end_of_stream,'$stream'(3))",
                   "read past formal");

  write_bytes(reset_path, "a.");
  auto resetting = mosa::Machine::create();
  const std::string reset_atom = path_atom(reset_path);
  auto again = resetting->query("open(" + reset_atom + ", read, S, [eof_action(reset)]), read(S, A), read(S, B), read(S, C).");
  mosa_test::check(again.kind == mosa::StopKind::Success, "read reset");
  mosa_test::check(binding_named(*resetting, again, "A") == "a", "read reset term");
  mosa_test::check(binding_named(*resetting, again, "B") == "end_of_file", "read reset eof");
  mosa_test::check(binding_named(*resetting, again, "C") == "a", "read reset rereads");

  write_bytes(binary_path, "a");
  auto binary = mosa::Machine::create();
  const std::string binary_atom = path_atom(binary_path);
  mosa_test::check(binary->query("open(" + binary_atom + ", read, S, [type(binary)]).").kind ==
                       mosa::StopKind::Success,
                   "read binary open");
  auto denied = binary->query("read('$stream'(3), T).");
  mosa_test::check(formal_text(*binary, denied) == "permission_error(input,binary_stream,'$stream'(3))",
                   "read binary formal");
  mosa_test::check(binary->query("get_byte('$stream'(3), 97).").kind == mosa::StopKind::Success, "read binary keeps");

  write_bytes(bad_path, "\xFF");
  auto broken = mosa::Machine::create();
  const std::string bad_atom = path_atom(bad_path);
  mosa_test::check(broken->query("open(" + bad_atom + ", read, S).").kind == mosa::StopKind::Success, "read utf open");
  auto utf = broken->query("read('$stream'(3), T).");
  mosa_test::check(formal_text(*broken, utf) == "representation_error(character)", "read utf formal");
  // read/1 は壊れたバイトまでを読んだものとして進める（iso-notes 第4節の決定 A、レビュー L6）。前は 2 度目も同じ誤りで、
  // 誤りを捕えて読み続けるループが止まらなかった。
  auto passed = broken->query("read('$stream'(3), T), T == end_of_file.");
  mosa_test::check(passed.kind == mosa::StopKind::Success, "read utf advances");
}

void check_invalid_utf8(const std::filesystem::path& path, const std::filesystem::path& peek_path,
                        const std::filesystem::path& read_path, const std::filesystem::path& error_path) {
  // error は壊れた並びを 1 文字として進めてから誤りを出す（iso-notes 第4節の決定 A、レビュー L6）。replace を確かめる
  // 2 つ目の壊れたバイトを残すため、2 バイトにする。前は 1 バイトで、error の後も位置 0 のままだった。
  write_bytes(path, "\xFF\xFF");
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("open(" + path_atom(path) + ", read, _).").kind == mosa::StopKind::Success,
                   "utf flag open");
  auto utf = machine->query("get_char('$stream'(3), C).");
  mosa_test::check(formal_text(*machine, utf) == "representation_error(character)", "utf flag default");
  auto advanced = machine->query("stream_property('$stream'(3), position(stream_position(N, _, _))).");
  mosa_test::check(binding_named(*machine, advanced, "N") == "1", "utf flag advances");
  auto replaced = machine->query("set_prolog_flag(invalid_utf8, replace).");
  // 旗が無いときは domain_error(prolog_flag) で止まり、後続の置換を誤報にしない。
  if (replaced.kind != mosa::StopKind::Success) {
    mosa_test::check(false, formal_text(*machine, replaced));
    return;
  }
  auto got = machine->query("get_char('$stream'(3), C), char_code(C, Code), "
                             "stream_property('$stream'(3), position(stream_position(N, _, _))).");
  mosa_test::check(binding_named(*machine, got, "Code") == "65533" && binding_named(*machine, got, "N") == "2",
                   "utf flag replaces");
  write_bytes(peek_path, "\xFF");
  mosa_test::check(machine->query("open(" + path_atom(peek_path) + ", read, _).").kind ==
                       mosa::StopKind::Success,
                   "utf peek open");
  auto peeked = machine->query("peek_char('$stream'(4), C), char_code(C, Code), "
                                "stream_property('$stream'(4), position(stream_position(N, _, _))).");
  mosa_test::check(binding_named(*machine, peeked, "Code") == "65533" && binding_named(*machine, peeked, "N") == "0",
                   "utf peek keeps");
  auto after = machine->query("get_char('$stream'(4), C), char_code(C, Code), "
                               "stream_property('$stream'(4), position(stream_position(N, _, _))).");
  mosa_test::check(binding_named(*machine, after, "Code") == "65533" && binding_named(*machine, after, "N") == "1",
                   "utf peek then get");
  write_bytes(read_path, "\xFF");
  mosa_test::check(machine->query("open(" + path_atom(read_path) + ", read, _).").kind ==
                       mosa::StopKind::Success,
                   "utf read open");
  auto read = machine->query("read_term('$stream'(5), _, []).");
  mosa_test::check(read.kind != mosa::StopKind::Exception ||
                       formal_text(*machine, read) != "representation_error(character)",
                   "utf read replaces");
  auto read_at = machine->query("stream_property('$stream'(5), position(stream_position(N, _, _))).");
  mosa_test::check(binding_named(*machine, read_at, "N") == "1", "utf read char count");
  write_bytes(error_path, "\xFF");
  mosa_test::check(machine->query("open(" + path_atom(error_path) + ", read, _).").kind ==
                       mosa::StopKind::Success,
                   "utf error open");
  mosa_test::check(machine->query("set_prolog_flag(invalid_utf8, error).").kind == mosa::StopKind::Success,
                   "utf flag error");
  auto again = machine->query("get_char('$stream'(6), C).");
  mosa_test::check(formal_text(*machine, again) == "representation_error(character)", "utf flag restored");
  auto passed = machine->query("stream_property('$stream'(6), position(stream_position(N, _, _))).");
  mosa_test::check(binding_named(*machine, passed, "N") == "1", "utf error advances");
  auto dropped = machine->query("set_prolog_flag(invalid_utf8, drop).");
  mosa_test::check(formal_text(*machine, dropped) == "domain_error(flag_value,invalid_utf8+drop)", "utf drop formal");
  auto current = machine->query("current_prolog_flag(invalid_utf8, error).");
  mosa_test::check(current.kind == mosa::StopKind::Success, "utf flag stays error");
}

// 読みの流れは開くときに全体を読まない（全体レビュー推奨2、持ち主の選択 B）。開いた後に外で足した分が見え、
// まだ読んでいない先への set_stream_position も、必要な分だけ読み足して届く。
void check_lazy_read(const std::filesystem::path& path, const std::filesystem::path& big_path) {
  write_bytes(path, "ab");
  const std::string atom = path_atom(path);
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->query("open(" + atom + ", read, S, [type(binary)]), get_byte(S, 97).").kind ==
                       mosa::StopKind::Success,
                   "lazy first byte");
  {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    out << "c";
  }
  auto rest = machine->query("get_byte('$stream'(3), B), get_byte('$stream'(3), C), get_byte('$stream'(3), E).");
  mosa_test::check(rest.kind == mosa::StopKind::Success, "lazy rest");
  mosa_test::check(binding_named(*machine, rest, "B") == "98", "lazy second byte");
  mosa_test::check(binding_named(*machine, rest, "C") == "99", "lazy sees the appended byte");
  mosa_test::check(binding_named(*machine, rest, "E") == "-1", "lazy then ends");

  write_bytes(big_path, std::string(200000, 'a') + "z");
  const std::string big_atom = path_atom(big_path);
  auto big = mosa::Machine::create();
  auto sought = big->query("open(" + big_atom +
                           ", read, S, [type(binary)]), set_stream_position(S, stream_position(200000, 1, 1)), "
                           "get_byte(S, Z), at_end_of_stream(S).");
  mosa_test::check(sought.kind == mosa::StopKind::Success, "lazy seek past the loaded part");
  mosa_test::check(binding_named(*big, sought, "Z") == "122", "lazy seek byte");
}
