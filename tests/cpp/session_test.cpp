#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/repl.hpp"
#include "mosa/suite.hpp"
#include "mosa/writer.hpp"

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

void check_consult_keeps_query() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/consult_live.pl";
  std::ofstream file(path, std::ios::binary);
  file << "p(1).\n";
  file.close();
  const std::string query = "consult('" + path + "'), true, p(X).";
  auto machine = mosa::Machine::create();
  auto stop = machine->query(query);
  mosa_test::check(stop.kind == mosa::StopKind::Success && stop.bindings.size() == 1, "consult query lives");
  const auto text = mosa::write_term(machine->heap(), machine->operators(),
                                     machine->heap().deref(stop.bindings[0].variable), mosa::WriteOptions{}, {});
  mosa_test::check(text == "1", "consult value 1");
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  std::vector<std::string_view> args{arg0, arg_g, query};
  auto result = mosa::run_session(args, "", false);
  mosa_test::check(result.code == 0, "consult session exit");
  mosa_test::check(result.out.find("X = 1.") != std::string::npos, "consult session answer");
}

void check_resource_limit_session() {
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "catch(p(a), _, true).";
  std::vector<std::string_view> args{arg0, arg_g, goal};
  auto batch = mosa::run_session(args, "", false, 8);
  mosa_test::check(batch.code == 2, "resource batch exit");
  mosa_test::check(batch.err.find("ヒープの上限を超えました") != std::string::npos, "resource warning");
  mosa_test::check(batch.out.find("?- ") == std::string::npos, "resource batch no prompt");
  auto talk = mosa::run_session(args, "\n", true, 8);
  mosa_test::check(talk.code == 0, "resource tty stays up");
  mosa_test::check(talk.err.find("ヒープの上限を超えました") != std::string::npos, "resource tty warning");
  mosa_test::check(talk.out.find("?- ") != std::string::npos, "resource tty prompt");
}

void check_consult_failure_reports() {
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string missing_goal = "consult('/no/such/mosa-live.pl').";
  std::vector<std::string_view> missing{arg0, arg_g, missing_goal};
  auto absent = mosa::run_session(missing, "", false);
  mosa_test::check(absent.code == 1, "missing consult exit");
  mosa_test::check(absent.out.find("false.") != std::string::npos, "missing consult fails");
  mosa_test::check(absent.err.find("/no/such/mosa-live.pl: ファイルを開けません") != std::string::npos,
                   "missing consult phrase");
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/consult_busy.pl";
  std::ofstream file(path, std::ios::binary);
  file << "p(1).\n";
  file.close();
  const std::string busy_goal = "((true ; fail), consult('" + path + "')).";
  std::vector<std::string_view> args{arg0, path, arg_g, busy_goal};
  auto busy = mosa::run_session(args, "", false);
  mosa_test::check(busy.code == 1, "busy consult exit");
  mosa_test::check(busy.out.find("false.") != std::string::npos, "busy consult fails");
  mosa_test::check(busy.err.find(path + ": 実行中の述語は置き換えられない") != std::string::npos,
                   "busy consult phrase");
}

void check_consult_twice() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/consult_twice.pl";
  std::ofstream file(path, std::ios::binary);
  file << "p(1).\np(2).\n";
  file.close();
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->consult(path), "consult once");
  mosa_test::check(machine->consult(path), "consult twice");
  auto first = machine->query("p(X).");
  mosa_test::check(first.kind == mosa::StopKind::Success && first.more && first.bindings.size() == 1,
                   "twice first");
  const auto one = mosa::write_term(machine->heap(), machine->operators(),
                                    machine->heap().deref(first.bindings[0].variable), mosa::WriteOptions{}, {});
  mosa_test::check(one == "1", "twice value 1");
  auto second = machine->next();
  mosa_test::check(second.kind == mosa::StopKind::Success && !second.more && second.bindings.size() == 1,
                   "twice second");
  const auto two = mosa::write_term(machine->heap(), machine->operators(),
                                    machine->heap().deref(second.bindings[0].variable), mosa::WriteOptions{}, {});
  mosa_test::check(two == "2", "twice value 2");
  mosa_test::check(machine->next().kind == mosa::StopKind::Failure, "twice stops");
}

void check_runner() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/pl/runner.pl";
  mosa_test::check(mosa::run_suite(path) == 0, "runner");
}

int main() {
  check_batch_without_tty();
  check_tty_next_then_prompt();
  check_errors();
  check_consult_keeps_query();
  check_consult_failure_reports();
  check_consult_twice();
  check_resource_limit_session();
  check_runner();
  return mosa_test::failures();
}
