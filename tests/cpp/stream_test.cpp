#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

std::string binding_named(mosa::Machine& machine, const mosa::Stop& stop, std::string_view name) {
  for (const mosa::Binding& binding : stop.bindings) {
    if (binding.name != name) continue;
    return mosa::write_term(machine.heap(), machine.operators(), machine.heap().deref(binding.variable),
                            mosa::WriteOptions{}, {});
  }
  return {};
}

std::string file_bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string prolog_atom(std::string_view text) {
  std::string out = "'";
  for (char unit : text) {
    if (unit == '\'') out += "''";
    else out.push_back(unit);
  }
  out.push_back('\'');
  return out;
}

void write_bytes(const std::filesystem::path& path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << bytes;
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
  const std::string atom = prolog_atom(path.string());
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
  auto stop = machine->query("open(" + prolog_atom(path.string()) + ", read, S).");
  mosa_test::check(stop.kind == mosa::StopKind::Success, "japanese path");
  mosa_test::check(binding_named(*machine, stop, "S") == "'$stream'(3)", "japanese index");
}

std::string formal_text(mosa::Machine& machine, const mosa::Stop& stop) {
  const auto ball = machine.heap().deref(stop.exception);
  return mosa::write_term(machine.heap(), machine.operators(), machine.heap().arg(ball, 0), mosa::WriteOptions{}, {});
}

void check_alias_blocks_real_file(const std::filesystem::path& path) {
  write_bytes(path, "a");
  auto machine = mosa::Machine::create();
  auto stop = machine->query("open(" + prolog_atom(path.string()) + ", read, S, [alias(user_output)]).");
  mosa_test::check(stop.kind == mosa::StopKind::Exception, "alias is taken");
  mosa_test::check(file_bytes(path) == "a", "alias error keeps bytes");
}

void check_eof_actions(const std::filesystem::path& reset_path, const std::filesystem::path& error_path,
                       const std::filesystem::path& binary_path, const std::filesystem::path& bad_path) {
  write_bytes(reset_path, "ab");
  auto resetting = mosa::Machine::create();
  const std::string reset_atom = prolog_atom(reset_path.string());
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
  const std::string error_atom = prolog_atom(error_path.string());
  mosa_test::check(erroring->query("open(" + error_atom + ", read, S, [eof_action(error)]).").kind ==
                       mosa::StopKind::Success,
                   "error open");
  auto at_end = erroring->query("get_char('$stream'(3), C).");
  mosa_test::check(binding_named(*erroring, at_end, "C") == "end_of_file", "error first eof");
  auto past = erroring->query("get_char('$stream'(3), C).");
  mosa_test::check(past.kind == mosa::StopKind::Exception, "error past throws");
  mosa_test::check(formal_text(*erroring, past) == "permission_error(input, past_end_of_stream, '$stream'(3))",
                   "error past formal");

  write_bytes(binary_path, "a");
  auto binary = mosa::Machine::create();
  const std::string binary_atom = prolog_atom(binary_path.string());
  mosa_test::check(binary->query("open(" + binary_atom + ", read, S, [type(binary)]).").kind == mosa::StopKind::Success,
                   "binary open");
  auto denied = binary->query("get_char('$stream'(3), C).");
  mosa_test::check(denied.kind == mosa::StopKind::Exception, "binary get throws");
  mosa_test::check(formal_text(*binary, denied) == "permission_error(input, binary_stream, '$stream'(3))",
                   "binary formal");

  write_bytes(bad_path, "\xFF");
  auto broken = mosa::Machine::create();
  const std::string bad_atom = prolog_atom(bad_path.string());
  mosa_test::check(broken->query("open(" + bad_atom + ", read, S).").kind == mosa::StopKind::Success, "bad open");
  auto utf = broken->query("get_char('$stream'(3), C).");
  mosa_test::check(utf.kind == mosa::StopKind::Exception, "bad utf throws");
  mosa_test::check(formal_text(*broken, utf) == "representation_error(character)", "bad utf formal");
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
  const std::string atom = prolog_atom(path.string());
  auto opened = file_machine->query("open(" + atom + ", write, S), put_char(S, a).");
  std::cout.rdbuf(old_quiet);
  mosa_test::check(opened.kind == mosa::StopKind::Success, "put file");
  mosa_test::check(quiet.str().empty(), "put file stays off stdout");
  mosa_test::check(file_bytes(path).empty(), "put file waits for flush");
}

}  // namespace

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
  check_put_char(dir / "phase3-put.txt");
  return mosa_test::failures();
}
