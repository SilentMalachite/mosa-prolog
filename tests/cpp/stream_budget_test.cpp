// 流れの読みのバイト予算（Claude のレビューの M1、L2、L3、L10）。予算は開いた時点の写しではなく
// 読むたびに全体から数え、止まった塊は抜けず、終わりを見た後で足された分も peek の後なら読める。
// stream_test.cpp が 800 行に近づいたので分けた。
#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <filesystem>
#include <fstream>
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

std::string prolog_atom(std::string_view text) {
  std::string out = "'";
  for (char unit : text) {
    if (unit == '\'') out += "''";
    else out.push_back(unit);
  }
  out.push_back('\'');
  return out;
}

// string() は Windows で \ を含む。引用原子の \a はエスケープなので、UTF-8 の / にする。
std::string path_atom(const std::filesystem::path& path) {
  const std::u8string text = path.generic_u8string();
  std::string bytes;
  bytes.reserve(text.size());
  for (char8_t unit : text) bytes.push_back(static_cast<char>(unit));
  return prolog_atom(bytes);
}

void write_bytes(const std::filesystem::path& path, std::string_view bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << bytes;
}

void check_flow_budget(const std::filesystem::path& path, const std::filesystem::path& eof_path) {
  std::string data;
  for (int index = 0; index < 512 * 1024; ++index) data.push_back(static_cast<char>('a' + index % 4));
  write_bytes(path, data);
  const std::string atom = path_atom(path);
  auto machine = mosa::Machine::create(16384);
  mosa_test::check(
      machine->load_program("seq(S, I) :- get_byte(S, B), (B == -1 -> true ; B =:= 97 + I mod 4, I1 is I + 1, "
                            "seq(S, I1)).\n")
          .ok,
      "flow budget helper loads");
  bool limited = false;
  try {
    (void)machine->query("open(" + atom + ", read, S, [type(binary)]), seq(S, 0).");
  } catch (const mosa::ResourceLimit&) {
    limited = true;
  }
  mosa_test::check(limited, "flow read stops at the byte budget");
  auto pos = machine->query("stream_property('$stream'(3), position(stream_position(C, _, _))).");
  mosa_test::check(pos.kind == mosa::StopKind::Success, "flow position after the stop");
  const std::string at = binding_named(*machine, pos, "C");
  bool limited_again = false;
  try {
    auto again = machine->query("seq('$stream'(3), " + at + ").");
    mosa_test::check(again.kind != mosa::StopKind::Failure, "flow bytes continue without a gap");
  } catch (const mosa::ResourceLimit&) {
    limited_again = true;
  }
  mosa_test::check(limited_again, "flow read stops again at the budget, keeping the chunk");

  write_bytes(eof_path, "ab");
  const std::string eof_atom = path_atom(eof_path);
  auto eof_machine = mosa::Machine::create();
  auto peeked = eof_machine->query("open(" + eof_atom + ", read, S, [type(binary)]), get_byte(S, 97), "
                                   "get_byte(S, 98), peek_byte(S, E).");
  mosa_test::check(peeked.kind == mosa::StopKind::Success, "eof peek");
  mosa_test::check(binding_named(*eof_machine, peeked, "E") == "-1", "eof peek sees the end");
  {
    std::ofstream out(eof_path, std::ios::binary | std::ios::app);
    out << "c";
  }
  auto appended = eof_machine->query("get_byte('$stream'(3), C).");
  mosa_test::check(appended.kind == mosa::StopKind::Success, "read after eof peek");
  mosa_test::check(binding_named(*eof_machine, appended, "C") == "99", "bytes appended after eof are read");
}

// 予算は流れごとでなく全体で数える。空のうちに 2 本開いても、片方が予算まで読めば
// もう片方は最初から止まる（前は開いた時点の写しで、合わせて予算の 2 倍まで保持できた）。
void check_two_streams_share(const std::filesystem::path& path) {
  write_bytes(path, std::string(512 * 1024, 'a'));
  const std::string atom = path_atom(path);
  auto machine = mosa::Machine::create(16384);
  mosa_test::check(
      machine->load_program("drain(S) :- get_byte(S, B), (B == -1 -> true ; drain(S)).\n").ok, "drain loads");
  auto opened = machine->query("open(" + atom + ", read, A, [type(binary)]), open(" + atom + ", read, B, [type(binary)]).");
  mosa_test::check(opened.kind == mosa::StopKind::Success, "two streams open");
  bool first_limited = false;
  try {
    (void)machine->query("drain('$stream'(3)).");
  } catch (const mosa::ResourceLimit&) {
    first_limited = true;
  }
  mosa_test::check(first_limited, "first stream stops at the budget");
  bool second_limited = false;
  try {
    (void)machine->query("get_byte('$stream'(4), _).");
  } catch (const mosa::ResourceLimit&) {
    second_limited = true;
  }
  mosa_test::check(second_limited, "second stream shares the same budget");
}

// レビュー 2026-11-20 の H-23。ファイルの流れへの最初の read は、最初の項の終止符を含む塊までしか読まない。前は足した
// 分の最後の 2 字だけで終止符を探したので、塊の切れ目がたまたま終止符の後ろに来るかファイルが終わるまで足し続け、
// 予算の小さい Machine では最初の項 a も読めずに止まった。
void check_first_read_stays_small(const std::filesystem::path& path) {
  std::string data = "a.\n";
  while (data.size() < 1024 * 1024) data += "b(xxxxxxxxxxxxxxxxxxxx).\n";
  write_bytes(path, data);
  auto machine = mosa::Machine::create(16384);
  bool limited = false;
  std::string term;
  try {
    const auto stop = machine->query("open(" + path_atom(path) + ", read, S), read(S, T).");
    if (stop.kind == mosa::StopKind::Success) term = binding_named(*machine, stop, "T");
  } catch (const mosa::ResourceLimit&) {
    limited = true;
  }
  mosa_test::check(!limited && term == "a", "h23: the first read stays within the first chunk, got " + term);
}

// 守り。塊ごとに読み直しても、塊の切れ目にまたがる項は 1 つに読む。切れ目は読みの塊の大きさ（64 KiB から、持って
// いる分に合わせて倍）に合わせ、浮動小数点数、終止符らしい並びを含む引用と注釈、切れ目の直後の空白で決まる終止符、
// UTF-8 の字の途中を置く。
class ChunkText {
 public:
  // end の手前まで空白で埋める。埋め草を節にすると読んだ項のリストが大きくなり、CI の MOSA_GC_STRESS=1 では
  // GC のたびにそれをなめて遅すぎた。塊の切れ目にまたがる項は add で置く。
  void fill_to(std::size_t end) {
    while (data_.size() < end) data_ += data_.size() % 80 == 79 ? '\n' : ' ';
  }
  void add(std::string_view text, int terms) {
    data_ += text;
    count_ += terms;
  }
  [[nodiscard]] const std::string& data() const { return data_; }
  [[nodiscard]] int count() const { return count_; }

 private:
  std::string data_;
  int count_ = 0;
};

void check_reads_across_chunks(const std::filesystem::path& path) {
  ChunkText text;
  text.add("a.\n", 1);
  text.fill_to(65536 - 4);
  text.add("c(1.5).\n", 1);
  text.fill_to(131072 - 7);
  text.add("q('ab. cd').\n", 1);
  text.fill_to(262144 - 6);
  text.add("/* x. y */ g.\n", 1);
  text.fill_to(524288 - 2);
  text.add("e.\n", 1);
  text.fill_to(1048576 - 4);
  text.add("j('日本').\n", 1);
  text.add("z.", 1);
  write_bytes(path, text.data());
  auto machine = mosa::Machine::create();
  // library(lists) を読まずに済むよう、照合の補助もここで定義する。
  const auto loaded = machine->load_program(
      "all(S, L) :- read(S, T), (T == end_of_file -> L = [] ; L = [T|R], all(S, R)).\n"
      "in(X, [Y|T]) :- (X == Y -> true ; in(X, T)).\n"
      "final([X], X) :- !.\nfinal([_|T], X) :- final(T, X).\n");
  mosa_test::check(loaded.ok, "h23: reader helpers load");
  const auto stop = machine->query("open(" + path_atom(path) + ", read, S), all(S, L), close(S), length(L, N), "
                                   "in(c(1.5), L), in(q('ab. cd'), L), in(g, L), in(e, L), in(j('日本'), L), "
                                   "final(L, z).");
  mosa_test::check(stop.kind == mosa::StopKind::Success, "h23: terms across chunk edges read whole");
  if (stop.kind == mosa::StopKind::Success) {
    mosa_test::check(binding_named(*machine, stop, "N") == std::to_string(text.count()), "h23: every term read once");
  }
}

}  // namespace

int main() {
  const std::filesystem::path root{MOSA_SOURCE_DIR};
  const auto dir = root / "tests" / "tmp";
  std::filesystem::create_directories(dir);
  check_flow_budget(dir / "stream-budget-flow.txt", dir / "stream-budget-eof.txt");
  check_two_streams_share(dir / "stream-budget-two.txt");
  check_first_read_stays_small(dir / "h23-first-read.txt");
  check_reads_across_chunks(dir / "h23-chunks.txt");
  return mosa_test::failures();
}
