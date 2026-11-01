// ヒープの上限（C++ の ResourceLimit）で with_output_to/2 や portray/1 の途中から抜けても、現在出力が呼ぶ前の流れへ
// 戻り、捕獲用のメモリの流れが開いたまま残らないこと（レビュー H6）。止めるゴールは 16 MiB の上限で length/2 が溢れる。
#include "harness.hpp"

#include "machine/internal.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/repl.hpp"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t k_limit = std::size_t{16} << 17;
const std::string k_tmp = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/";

bool ok(mosa::Machine& machine, std::string_view goal) {
  return machine.query(goal).kind == mosa::StopKind::Success;
}

// 上限で止まったことを確かめる。止まらずに終わったら、その時点で検査の前提が崩れている。
bool stops(mosa::Machine& machine, std::string_view goal) {
  try {
    (void)machine.query(goal);
  } catch (const mosa::ResourceLimit&) {
    return true;
  }
  return false;
}

constexpr std::string_view k_on_user_output = "current_output(S), stream_property(S, alias(user_output)).";
constexpr std::string_view k_three_open = "findall(S, stream_property(S, mode(_)), L), length(L, 3).";

std::string write_file(std::string_view name, std::string_view text) {
  const std::string path = k_tmp + std::string(name);
  std::ofstream file(path, std::ios::binary);
  file << text;
  return path;
}

std::string read_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// user_output は std::cout へ書く。答えの行（result.out）とは別なので、その間だけ受けて後ろに付ける。
class CoutCapture {
 public:
  CoutCapture() : saved_(std::cout.rdbuf(sink_.rdbuf())) {}
  CoutCapture(const CoutCapture&) = delete;
  CoutCapture& operator=(const CoutCapture&) = delete;
  ~CoutCapture() { std::cout.rdbuf(saved_); }
  std::string text() const { return sink_.str(); }

 private:
  std::ostringstream sink_;
  std::streambuf* saved_;
};

mosa::SessionResult session(const std::vector<std::string_view>& args, std::string_view input) {
  const CoutCapture capture;
  mosa::SessionResult result = mosa::run_session(args, input, true, k_limit);
  std::cout.flush();
  result.out += capture.text();
  return result;
}

void check_session_with_output_to() {
  const std::vector<std::string_view> args{"mosa", "-g", "with_output_to(atom(A),(write(hello),length(L,100000000)))"};
  const auto result = session(args, "write(visible), nl.\n");
  mosa_test::check(result.err.find("16 MiB") != std::string::npos, "session_with_output_to: the goal hit the limit");
  mosa_test::check(result.out.find("visible") != std::string::npos, "session_with_output_to: next write is visible");
  mosa_test::check(result.out.find("hello") == std::string::npos, "session_with_output_to: captured text stays hidden");
}

void check_session_portray() {
  const std::string path = write_file("output_stop_portray.pl", "portray(big) :- length(_, 100000000).\n");
  const std::vector<std::string_view> args{"mosa", path, "-g", "print(f(big))"};
  const auto result = session(args, "write(visible), nl.\n");
  mosa_test::check(result.err.find("16 MiB") != std::string::npos, "session_portray: the hook hit the limit");
  mosa_test::check(result.out.find("visible") != std::string::npos, "session_portray: next write is visible");
  mosa_test::check(result.err.find("existence_error") == std::string::npos, "session_portray: no closed stream");
}

void check_api_plain_and_repeated() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(ok(*machine, k_three_open), "api_plain: three standard streams before");
  for (int round = 0; round < 3; ++round) {
    mosa_test::check(stops(*machine, "with_output_to(atom(_), (write(x), length(_, 100000000)))."),
                     "api_plain: stops");
    mosa_test::check(ok(*machine, k_on_user_output), "api_plain: back on user_output");
    mosa_test::check(ok(*machine, k_three_open), "api_plain: capture stream closed after the stop");
  }
  mosa_test::check(ok(*machine, "with_output_to(atom(A), write(ok)), A == ok."), "api_plain: capture works again");
}

void check_api_nested() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(stops(*machine, "with_output_to(atom(_), (write(a), with_output_to(codes(_), "
                                   "(write(b), length(_, 100000000)))))."),
                   "api_nested: stops");
  mosa_test::check(ok(*machine, k_on_user_output), "api_nested: back on user_output");
  mosa_test::check(ok(*machine, k_three_open), "api_nested: both capture streams closed");
}

void check_api_portray_inside_capture() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program("portray(big) :- write(p), length(_, 100000000).").ok,
                   "api_portray: load portray");
  mosa_test::check(stops(*machine, "with_output_to(atom(_), print(f(big)))."), "api_portray: stops in capture");
  mosa_test::check(ok(*machine, k_on_user_output), "api_portray: back on user_output after capture");
  mosa_test::check(ok(*machine, k_three_open), "api_portray: capture and scratch closed");
  mosa_test::check(stops(*machine, "print(f(big))."), "api_portray: stops at top");
  mosa_test::check(ok(*machine, k_on_user_output), "api_portray: back on user_output after print");
  mosa_test::check(ok(*machine, "with_output_to(atom(A), write(ok)), A == ok."), "api_portray: write works");
}

// query の冒頭の戻しを通らない入口（load_program の指示）で、portray の scope だけが戻したことを見る。
void check_api_portray_then_directive() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program("portray(big) :- write(p), length(_, 100000000).").ok,
                   "portray_directive: load portray");
  mosa_test::check(stops(*machine, "print(f(big))."), "portray_directive: stops");
  // REPL の recover_heap と同じく、上限で止まったヒープを空けてから読む。
  machine->heap().truncate(0);
  const CoutCapture capture;
  mosa_test::check(machine->load_program(":- write(portray_mark), flush_output.\n").ok,
                   "portray_directive: directive runs");
  mosa_test::check(capture.text() == "portray_mark", "portray_directive: the directive wrote to user_output");
}

// portray の中の print がまた portray を呼び、内側で止まる。scratch が 2 つ積まれ、どちらも戻して閉じる。
void check_api_nested_portray() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program("portray(a) :- write(pa), print(h(b)).\n"
                                         "portray(b) :- write(pb), length(_, 100000000).")
                       .ok,
                   "api_nested_portray: load");
  mosa_test::check(stops(*machine, "with_output_to(atom(_), print(f(a)))."), "api_nested_portray: stops");
  mosa_test::check(ok(*machine, k_on_user_output), "api_nested_portray: back on user_output");
  mosa_test::check(ok(*machine, k_three_open), "api_nested_portray: every scratch closed");
}

void check_api_set_output_inside() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(stops(*machine, "with_output_to(atom(_), (set_output(user_error), length(_, 100000000)))."),
                   "api_set_output: stops");
  mosa_test::check(ok(*machine, k_on_user_output), "api_set_output: back on user_output, not user_error");
}

// 利用者が開いた流れへの差し替えでは、止まっても流れを閉じない。
void check_api_stream_sink_survives() {
  auto machine = mosa::Machine::create(k_limit);
  const std::string path = k_tmp + "output_stop_sink.txt";
  const std::string open = "open('" + path + "', write, _, [alias(os_sink)]).";
  mosa_test::check(ok(*machine, open), "api_sink: open");
  mosa_test::check(stops(*machine, "with_output_to(os_sink, (write(before), length(_, 100000000)))."),
                   "api_sink: stops");
  mosa_test::check(ok(*machine, k_on_user_output), "api_sink: back on user_output");
  mosa_test::check(ok(*machine, "write(os_sink, after), close(os_sink)."), "api_sink: the user's stream stays open");
  mosa_test::check(read_file(path).find("after") != std::string::npos, "api_sink: the file got the later write");
}

// 呼ぶ前の現在出力がゴールの中で閉じられたら、閉じた流れではなく user_output へ戻す（close/1 の 8.11.6.1 と同じ）。
void check_closed_saved_falls_back() {
  auto machine = mosa::Machine::create(k_limit);
  const std::string path = k_tmp + "output_stop_closed.txt";
  const std::string goal = "open('" + path + "', write, S), set_output(S), with_output_to(atom(_), close(S)), "
                           "current_output(C), stream_property(C, alias(user_output)).";
  mosa_test::check(ok(*machine, goal), "closed_saved: current output falls back to user_output");
  const CoutCapture capture;
  mosa_test::check(ok(*machine, "write(closed_saved_mark), flush_output."), "closed_saved: write after still works");
  mosa_test::check(capture.text() == "closed_saved_mark", "closed_saved: the write reached user_output");
}

void check_normal_exits_unchanged() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(ok(*machine, "with_output_to(atom(A), write(x)), A == x."), "normal: success binds");
  mosa_test::check(ok(*machine, "\\+ with_output_to(atom(_), fail)."), "normal: failure");
  mosa_test::check(ok(*machine, "catch(with_output_to(atom(_), throw(e)), e, true), current_output(S), "
                                "stream_property(S, alias(user_output))."),
                   "normal: caught exception restores");
  mosa_test::check(ok(*machine, k_three_open), "normal: nothing left open");
  mosa_test::check(machine->query("with_output_to(atom(_), halt).").kind == mosa::StopKind::Halt, "normal: halt");
  mosa_test::check(ok(*machine, k_on_user_output), "normal: halt restores");
  mosa_test::check(ok(*machine, k_three_open), "normal: halt closes the capture");
}

// 現在入出力が閉じた流れを指したまま次の問い合わせに来たら、標準の流れへ戻す（差し替えの戻し忘れへの備え。開いた流れを指すのは set_output の持ち越しなので戻さない）。
void check_query_resets_closed_current() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(ok(*machine, "with_output_to(atom(_), true)."), "query_reset: make a closed slot");
  mosa::Impl& impl = mosa::engine_impl(*machine);
  const std::size_t closed = impl.streams.size() - 1;
  mosa_test::check(impl.streams[closed].closed, "query_reset: the capture slot is closed");
  impl.current_out = closed;
  impl.current_in = closed;
  mosa_test::check(ok(*machine, "current_output(S), stream_property(S, alias(user_output)), "
                                "current_input(I), stream_property(I, alias(user_input))."),
                   "query_reset: both reset");
  impl.current_out = impl.streams.size() + 5;
  mosa_test::check(ok(*machine, k_on_user_output), "query_reset: out of range resets");
}

}  // namespace

int main() {
  check_session_with_output_to();
  check_session_portray();
  check_api_plain_and_repeated();
  check_api_nested();
  check_api_portray_inside_capture();
  check_api_portray_then_directive();
  check_api_nested_portray();
  check_api_set_output_inside();
  check_api_stream_sink_survives();
  check_closed_saved_falls_back();
  check_normal_exits_unchanged();
  check_query_resets_closed_current();
  return mosa_test::failures();
}
