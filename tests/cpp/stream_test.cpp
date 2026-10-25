#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <filesystem>
#include <fstream>
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

void check_alias_blocks_real_file(const std::filesystem::path& path) {
  write_bytes(path, "a");
  auto machine = mosa::Machine::create();
  auto stop = machine->query("open(" + prolog_atom(path.string()) + ", read, S, [alias(user_output)]).");
  mosa_test::check(stop.kind == mosa::StopKind::Exception, "alias is taken");
  mosa_test::check(file_bytes(path) == "a", "alias error keeps bytes");
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
  return mosa_test::failures();
}
