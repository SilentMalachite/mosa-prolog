#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/messages.hpp"
#include "mosa/repl.hpp"
#include "mosa/suite.hpp"
#include "mosa/writer.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
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

void check_consult_flag_rolls_back() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/flag_rollback.pl";
  std::ofstream file(path, std::ios::binary);
  file << "q(1).\n:- set_prolog_flag(unknown, fail).\n:- nope.\n";
  file.close();
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->consult(path), "bad consult fails");
  auto flag = machine->query("current_prolog_flag(unknown, X).");
  mosa_test::check(flag.kind == mosa::StopKind::Success && flag.bindings.size() == 1, "unknown restored");
  const auto text = mosa::write_term(machine->heap(), machine->operators(),
                                     machine->heap().deref(flag.bindings[0].variable), mosa::WriteOptions{}, {});
  mosa_test::check(text == "error", "unknown stays error");
  auto clause = machine->query("q(X).");
  mosa_test::check(clause.kind == mosa::StopKind::Exception, "rolled back clause");
}

void check_consult_invalid_utf8_rolls_back() {
  const std::string bad = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/invalid_utf8_rollback.pl";
  std::ofstream failed_file(bad, std::ios::binary);
  failed_file << "q(1).\n:- set_prolog_flag(invalid_utf8, replace).\n:- nope.\n";
  failed_file.close();
  auto failed = mosa::Machine::create();
  mosa_test::check(!failed->consult(bad), "utf8 consult fails");
  auto flag = failed->query("current_prolog_flag(invalid_utf8, X).");
  std::string text = "unbound";
  if (flag.kind == mosa::StopKind::Success && flag.bindings.size() == 1) {
    text = mosa::write_term(failed->heap(), failed->operators(), failed->heap().deref(flag.bindings[0].variable),
                            mosa::WriteOptions{}, {});
  }
  mosa_test::check(text == "error", std::string("invalid_utf8 after failed load: ") + text);

  const std::string good = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/invalid_utf8_kept.pl";
  std::ofstream kept_file(good, std::ios::binary);
  kept_file << ":- set_prolog_flag(invalid_utf8, replace).\nq(1).\n";
  kept_file.close();
  auto kept = mosa::Machine::create();
  mosa_test::check(kept->consult(good), "utf8 consult succeeds");
  auto stayed = kept->query("current_prolog_flag(invalid_utf8, replace).");
  mosa_test::check(stayed.kind == mosa::StopKind::Success, "utf8 replace kept");
}

void check_dynamic_builtin_rejected() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/dynamic_true.pl";
  std::ofstream file(path, std::ios::binary);
  file << ":- dynamic(true/0).\n";
  file.close();
  auto machine = mosa::Machine::create();
  mosa_test::check(!machine->consult(path), "dynamic true fails");
  mosa_test::check(machine->diagnostic().find("permission_error(modify,static_procedure,true/0)") !=
                       std::string::npos,
                   "dynamic true phrase");
  auto ok = machine->query("true.");
  mosa_test::check(ok.kind == mosa::StopKind::Success, "builtin true still succeeds");
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

// SPEC 第4節。対話で旗を真にすると、次に打つ問い合わせから全角記号で読める。
void check_fullwidth_query() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto result = mosa::run_session(args, "set_prolog_flag(japanese_punctuation, true).\nX = f（a，b）。\n", true);
  mosa_test::check(result.out.find("X = f(a,b).") != std::string::npos, "fullwidth query in the repl");
}

void check_runner() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/pl/runner.pl";
  mosa_test::check(mosa::run_suite(path) == 0, "runner");
}

// ディレクトリを渡したら、落ちたファイルの後ろも流して全部の失敗を出す。先で止めると後ろの失敗が隠れる。
void check_runner_reports_every_file() {
  const std::string dir = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/runner_every";
  std::filesystem::create_directories(dir);
  for (const char* name : {"a.pl", "b.pl"}) {
    std::ofstream file(dir + "/" + name, std::ios::binary);
    file << "test(" << name[0] << "_fails, fail, true).\n";
  }
  std::ostringstream captured;
  auto* saved = std::cerr.rdbuf(captured.rdbuf());
  const int code = mosa::run_suite(dir);
  std::cerr.rdbuf(saved);
  mosa_test::check(code == 1, "runner every file exit");
  mosa_test::check(captured.str().find("a.pl: fail") != std::string::npos, "runner reports a.pl");
  mosa_test::check(captured.str().find("b.pl: fail") != std::string::npos, "runner reports b.pl");
}

// ADR 0007。捕まらなかった例外の文面は、旗で選んだ言語のカタログから引く。
void check_uncaught_message(std::string_view lang, std::string_view phrase, std::string_view label) {
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "set_prolog_flag(message_language, " + std::string{lang} + "), throw(foo).";
  std::vector<std::string_view> args{arg0, arg_g, goal};
  auto result = mosa::run_session(args, "", false);
  mosa_test::check(result.code == 2, std::string{label} + " exit");
  mosa_test::check(result.err.find(phrase) != std::string::npos, label);
}

// CMake はこのテストを LANG=ja_JP.UTF-8 で流す。
void check_message_language_default() {
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "current_prolog_flag(message_language, X).";
  std::vector<std::string_view> args{arg0, arg_g, goal};
  auto result = mosa::run_session(args, "", false);
  mosa_test::check(result.out.find("X = ja.") != std::string::npos, "message_language default from LANG");
}

void check_language_from_lang() {
  using mosa::MessageLanguage;
  mosa_test::check(mosa::language_from_lang("ja_JP.UTF-8") == MessageLanguage::Ja, "lang ja_JP");
  mosa_test::check(mosa::language_from_lang("ja") == MessageLanguage::Ja, "lang ja");
  mosa_test::check(mosa::language_from_lang("en_US.UTF-8") == MessageLanguage::En, "lang en_US");
  mosa_test::check(mosa::language_from_lang("C") == MessageLanguage::En, "lang C");
  mosa_test::check(mosa::language_from_lang(std::nullopt) == MessageLanguage::En, "lang unset");
}

// SPEC 第4節。日本語の変数の書き方を REPL の help で見せる。
void check_help_japanese_variables() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto ja = mosa::run_session(args, "set_prolog_flag(message_language, ja).\nhelp.\n", true);
  mosa_test::check(ja.out.find("_太郎") != std::string::npos, "help ja shows _太郎");
  mosa_test::check(ja.out.find("下線") != std::string::npos, "help ja text");
  auto en = mosa::run_session(args, "set_prolog_flag(message_language, en).\nhelp.\n", true);
  mosa_test::check(en.out.find("underscore") != std::string::npos, "help en text");
}

// SPEC 第4節。REPL の構文エラーは行、列、問題のトークンを旗の言語で見せる。日本語のトークンも崩さない。
void check_syntax_shown() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto ja = mosa::run_session(args, "set_prolog_flag(message_language, ja).\n子 親.\n", true);
  mosa_test::check(ja.err.find("行 1 列") != std::string::npos, "syntax_ja_shown place");
  mosa_test::check(ja.err.find("親") != std::string::npos, "syntax_ja_shown token");
  auto en = mosa::run_session(args, "set_prolog_flag(message_language, en).\n子 親.\n", true);
  mosa_test::check(en.err.find("line 1 column") != std::string::npos, "syntax_en_shown place");
  mosa_test::check(en.err.find("親") != std::string::npos, "syntax_en_shown token");
}

// CMake は MOSA_LIB を鍵の無い messages_en.pl の置き場にして、この関数だけを流す。
void check_message_missing_key() {
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "set_prolog_flag(message_language, en), throw(foo).";
  std::vector<std::string_view> args{arg0, arg_g, goal};
  auto result = mosa::run_session(args, "", false);
  mosa_test::check(result.code == 2, "missing key exit");
  mosa_test::check(result.err.find("goal_raised: foo") != std::string::npos, "missing key fallback");
}

int main(int argc, char** argv) {
  if (argc > 1 && std::string_view{argv[1]} == "missing") {
    check_message_missing_key();
    return mosa_test::failures();
  }
  check_batch_without_tty();
  check_tty_next_then_prompt();
  check_errors();
  check_consult_keeps_query();
  check_consult_failure_reports();
  check_consult_flag_rolls_back();
  check_consult_invalid_utf8_rolls_back();
  check_dynamic_builtin_rejected();
  check_consult_twice();
  check_resource_limit_session();
  check_fullwidth_query();
  check_runner();
  check_runner_reports_every_file();
  check_uncaught_message("ja", "警告: ゴールは例外を送出しました: foo", "uncaught_ja");
  check_uncaught_message("en", "Warning: goal raised exception: foo", "uncaught_en");
  check_message_language_default();
  check_language_from_lang();
  check_help_japanese_variables();
  check_syntax_shown();
  return mosa_test::failures();
}
