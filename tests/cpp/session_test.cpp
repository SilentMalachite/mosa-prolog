#include "harness.hpp"
#include "mosa/repl.hpp"
#include "mosa/suite.hpp"

#include <fstream>
#include <string>
#include <string_view>
#include <vector>

void check_batch_without_tty() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/session_p.pl";
  std::ofstream file(path, std::ios::binary);
  file << "p(1).\np(2).\n";
  file.close();
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "p(X).";
  std::vector<std::string_view> args{arg0, path, arg_g, goal};
  auto result = mosa::run_session(args, "", false);
  mosa_test::check(result.code == 0, "batch exit");
  mosa_test::check(result.out.find("X = 1.") != std::string::npos, "first solution");
  mosa_test::check(result.out.find("X = 2.") == std::string::npos, "does not print the rest");
}

void check_tty_next_then_prompt() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/session_p.pl";
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "p(X).";
  std::vector<std::string_view> args{arg0, path, arg_g, goal};
  auto result = mosa::run_session(args, ";\n", true);
  mosa_test::check(result.out.find("X = 2.") != std::string::npos, "tty second");
  mosa_test::check(result.out.find("?- ") != std::string::npos, "back to prompt");
  const std::string halt = "-t";
  const std::string halt_value = "halt";
  std::vector<std::string_view> halted{arg0, path, arg_g, goal, halt, halt_value};
  auto once = mosa::run_session(halted, ";\n", true);
  mosa_test::check(once.out.find("X = 1.") != std::string::npos, "halt prints first");
  mosa_test::check(once.out.find("?- ") == std::string::npos, "halt skips prompt");
}

void check_errors() {
  const std::string broken = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/session_broken.pl";
  std::ofstream file(broken, std::ios::binary);
  file << "p(1";
  file.close();
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0, broken};
  auto result = mosa::run_session(args, "", true);
  mosa_test::check(result.code == 1, "broken exit");
  mosa_test::check(result.err.find("項が閉じていません") != std::string::npos, "incomplete phrase");
  mosa_test::check(result.out.find("?- ") == std::string::npos, "no prompt on startup error");
  std::vector<std::string_view> missing{arg0, std::string_view{"/no/such/mosa.pl"}};
  auto absent = mosa::run_session(missing, "", true);
  mosa_test::check(absent.code == 1, "missing exit");
  mosa_test::check(absent.err.find("ファイルを開けません") != std::string::npos, "missing phrase");
}

void check_runner() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/pl/runner.pl";
  mosa_test::check(mosa::run_suite(path) == 0, "runner");
}

int main() {
  check_batch_without_tty();
  check_tty_next_then_prompt();
  check_errors();
  check_runner();
  return mosa_test::failures();
}
