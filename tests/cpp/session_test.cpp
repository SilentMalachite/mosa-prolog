#include "answer_lines.hpp"
#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/messages.hpp"
#include "mosa/repl.hpp"
#include "mosa/suite.hpp"
#include "mosa/writer.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
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
  mosa_test::check(batch.err.find("ヒープが上限（1 MiB）に達した") != std::string::npos, "resource warning");
  mosa_test::check(batch.out.find("?- ") == std::string::npos, "resource batch no prompt");
  auto talk = mosa::run_session(args, "\n", true, 8);
  mosa_test::check(talk.code == 0, "resource tty stays up");
  mosa_test::check(talk.err.find("ヒープが上限（1 MiB）に達した") != std::string::npos, "resource tty warning");
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

// レビュー M8。REPL は読んだゴールを書き直して読み直す。全角記号モードで '、' を引用しないと , に読み直され、
// 答えも引用しないと読み戻せない。
void check_fullwidth_quoted_answer() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto result = mosa::run_session(
      args, "set_prolog_flag(japanese_punctuation, true).\nX = '、'.\nY = f('（'), Z = '：'(a).\n", true);
  mosa_test::check(result.out.find("X = '、'.") != std::string::npos, "fullwidth quoted touten answer");
  mosa_test::check(result.out.find("Y = f('（'), Z = '：'(a).") != std::string::npos,
                   "fullwidth quoted paren and colon answer");
}

// レビュー M9。REPL は読んだゴールを書いて読み直す。作った _G1 が利用者の _G1 と重なると 2 つの変数が 1 つになり、
// f(_G1, _) = f(a, b) が偽になっていた。
void check_given_made_up_name() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto result = mosa::run_session(args, "f(_G1, _) = f(a, b).\nX = f(_G1, _).\n", true);
  mosa_test::check(result.out.find("false") == std::string::npos, "given _G1 keeps the anonymous variable apart");
  mosa_test::check(result.out.find("_G1 = a") != std::string::npos, "given _G1 is bound");
  mosa_test::check(result.out.find("X = f(_G1,_G2).") != std::string::npos, "answer names the anonymous variable apart");
}

// レビュー M22。答えの束縛を 1 つずつ書くと、作る名前が束縛ごとに _G1 から始まり、違う変数が同じ名前になった。
// 同じ変数は束縛をまたいでも同じ名前のまま、利用者の _G2 は後の束縛でも飛ばす。REPL は読んだゴールを書き直して
// 読むので（M9）、_ は名前の付いた変数になる。名前の無い _ は -g で確かめる。
void check_answer_numbering() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto result = mosa::run_session(args,
                                  "length(L, 2), length(M, 1).\n"
                                  "copy_term(f(A), X), copy_term(g(B), Y).\n"
                                  "length(L, 2), length(K, 1), M = L.\n"
                                  "X = f(_G2), length(L, 1), length(M, 1).\n"
                                  "(N = 1 ; N = 2), length(L, N), length(M, 1).\n;\n",
                                  true);
  const auto has = [&result](std::string_view line) { return answered(result.out, line); };
  mosa_test::check(has("L = [_G1,_G2], M = [_G3]."), "lists of fresh variables apart: " + result.out);
  mosa_test::check(has("A = A, X = f(_G1), B = B, Y = g(_G2)."), "copies apart");
  mosa_test::check(has("L = [_G1,_G2], K = [_G3], M = [_G1,_G2]."), "one variable keeps its name across bindings");
  mosa_test::check(has("X = f(_G2), L = [_G1], M = [_G3]."), "given _G2 skipped while the numbering carries on");
  // 別解はそれぞれ別の答えなので、番号は答えごとに 1 から振る。
  mosa_test::check(has("N = 1, L = [_G1], M = [_G2]"), "first alternative");
  mosa_test::check(has("N = 2, L = [_G1,_G2], M = [_G3]."), "second alternative numbers from 1 again");
  const std::string arg_g = "-g";
  const std::string goal = "X = f(_, _), Y = g(_).";
  std::vector<std::string_view> batch{arg0, arg_g, goal};
  auto anonymous = mosa::run_session(batch, "", false);
  mosa_test::check(answered(anonymous.out, "X = f(_G1,_G2), Y = g(_G3)."), "anonymous variables apart: " + anonymous.out);
}

void check_runner() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/pl/runner.pl";
  mosa_test::check(mosa::run_suite(path) == 0, "runner");
}

// レビュー M26（ADR 0019）。値が同じ未束縛の変数である名前は、鎖 N1 = N2, N2 = N3 で示す。前は前の束縛に出た変数の
// 束縛を書かなかったので、名前が消え、1 つの変数が _G1 と Q の 2 つの字面になった。
void check_answer_aliases() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto result = mosa::run_session(args,
                                  "X = Y, Y = Z.\n"
                                  "Y = Z, X = Y.\n"
                                  "findall(X-X, true, [P-Q]), Y = g(Q).\n"
                                  "findall(f(X)-X, true, [Y-P]).\n"
                                  "X = f(Y), Y = Z.\n"
                                  "copy_term(A-A, P-Q).\n"
                                  "findall(X-X-f(X), true, [P-Q-Y]).\n"
                                  "findall(X-X-X, true, [P-Q-R]), Y = f(P).\n"
                                  "X = Y, length(L, 1), L = [Y].\n"
                                  "f(_, X) = f(a, b).\n"
                                  "functor(T, f, 2), arg(1, T, _).\n"
                                  "X = _, Y = X.\n"
                                  "findall(f(X)-X, true, [Y-_]).\n"
                                  "functor(T, f, 2), arg(1, T, A).\n"
                                  "(X = Y ; X = a), Y = Z.\n;\n",
                                  true);
  const auto has = [&result](std::string_view line) { return answered(result.out, line); };
  mosa_test::check(has("X = Y, Y = Z."), "chain of three names: " + result.out);
  mosa_test::check(has("Y = Z, Z = X."), "chain in binding order");
  mosa_test::check(has("X = X, P = Q, Y = g(Q)."), "fresh variable with two names");
  mosa_test::check(has("X = X, Y = f(_G1), P = _G1."), "one name shown after its variable");
  mosa_test::check(has("X = f(Y), Y = Z."), "alias after a value that shows it");
  mosa_test::check(has("A = A, P = Q."), "copied shared variable");
  mosa_test::check(has("X = X, P = Q, Y = f(Q)."), "fresh variable written as the last name");
  mosa_test::check(has("X = X, P = Q, Q = R, Y = f(P)."), "chain of a fresh variable");
  mosa_test::check(has("X = Y, L = [Y]."), "a plain alias stays");
  // REPL は読んだゴールを書いて読み直す。_ を _ のまま書くので、名前の無い変数は答えに出ない（レビュー L18）。
  mosa_test::check(has("X = b."), "anonymous variable not shown");
  mosa_test::check(has("T = f(_G1,_G2)."), "anonymous argument not shown");
  mosa_test::check(has("X = Y."), "alias through an anonymous variable");
  mosa_test::check(has("X = X, Y = f(_G1)."), "anonymous copy not shown");
  // 名前が 1 つで問い合わせの変数そのものでない変数は、前の値に出ても示す。前は A が消えた。
  mosa_test::check(has("T = f(_G1,_G2), A = _G1."), "one name of a fresh variable kept");
  mosa_test::check(has("X = Y, Y = Z"), "chain in the first alternative");
  mosa_test::check(has("X = a, Y = Z."), "chain gone in the second alternative");
}

// M26 の Codex レビュー。char_conversion が _ を別の字に変えるとき、REPL がゴールの無名変数を _ と書くと、読み直しで
// 2 つが同じ名前になり、f(x, x) = f(a, b) が偽になった。
void check_anonymous_under_char_conversion() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto result = mosa::run_session(args,
                                  "char_conversion(x, '_'), char_conversion('_', 'A'), "
                                  "set_prolog_flag(char_conversion, on).\n"
                                  "f(x, x) = f(a, b).\n",
                                  true);
  mosa_test::check(!answered(result.out, "false."), "converted anonymous variables stay apart: " + result.out);
}

// レビュー M27。答えの値は = の右の被演算子として書く。優先度 1200 で書くと X = (a;b) が X = a;b になり、貼り直すと
// 別のゴール（X = a, Y = Y）か誤りになった。答えは同じ表のまま貼り直すと同じ答えになること（レビュー M8）。setup は
// 両方の回で先に通すゴールで、op/3 で = を変える。
struct Pasted {
  std::string_view setup;
  std::string_view goal;
  std::string_view answer;
};

void check_answer_operands() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  const std::array<Pasted, 13> cases{{
      {"", "X = (a ; b), Y = 1.", "X = (a;b), Y = 1."},
      {"", "X = (a , b).", "X = (a,b)."},
      {"", "X = (a :- b).", "X = (a:-b)."},
      {"", "X = (a = b).", "X = (a=b)."},
      {"", "X = (=).", "X = (=)."},
      {"", "X = (-), Y = (:-).", "X = (-), Y = (:-)."},
      {"", "X = (\\+ a), Y = (a < b).", "X = (\\+a), Y = (a<b)."},
      {"", "X = (A :- B).", "X = (A:-B)."},
      // 演算子の原子に束縛された名前は名前で書くので、括弧に入れない（M27 の敵対的レビュー A3）。
      {"", "Y = (-), X = (a :- Y), Z = - Y.", "Y = (-), X = (a:-Y), Z = -Y."},
      // 699 までの項と、引数とリストの要素は前と同じく括弧に入れない。
      {"", "X = 1+2, Y = - a, Z = [-, (a :- b)], W = f(=).", "X = 1+2, Y = -a, Z = [-,(a:-b)], W = f(=)."},
      // = を変えたら、今の表の = の右の被演算子として書く。
      {"op(700, xfy, =).", "X = (a = b).", "X = a=b."},
      {"op(700, yfx, =).", "X = (a = b).", "X = (a=b)."},
      {"op(200, xfx, =).", "X = (1 + 2).", "X = (1+2)."},
  }};
  for (const Pasted& item : cases) {
    const std::string setup = item.setup.empty() ? std::string{} : std::string{item.setup} + "\n";
    auto first = mosa::run_session(args, setup + std::string{item.goal} + "\n", true);
    mosa_test::check(answered(first.out, item.answer), std::string{item.goal} + " -> " + first.out);
    auto again = mosa::run_session(args, setup + std::string{item.answer} + "\n", true);
    mosa_test::check(answered(again.out, item.answer), std::string{item.answer} + " pasted back -> " + again.out);
  }
  // = が演算子でなければ答えは読み戻せないので、貼り直さない。ISO の 699 で書くこと: 1+2 は括弧に入らず
  // （0 なら入る）、a<b は入る（999 なら入らない）。ゴールは op/3 の前に読むので = が使える（M27 の敵対的レビュー A2）。
  auto plain = mosa::run_session(args, "op(0, xfx, =), X = 1+2, Y = (a :- b), Z = (a < b).\n", true);
  mosa_test::check(answered(plain.out, "X = 1+2, Y = (a:-b), Z = (a<b)."), "no = operator writes at 699: " + plain.out);
}

// レビュー M22。mosatest の solutions は期待と答えの束縛を 1 つの番号付けで書いて比べる。答えを束縛ごとに書くと、
// M22 の誤った字面（h1）も、別の 2 つの変数を 1 つの共有した変数とする期待（h3）も通った。h2 は逆向きの取り違え。
// 期待は全体を 1 度に書いて読み直すので、_ は _G1、_G2 と番号の付いた名前になる。h1〜h3 は落ちたゴールを見分ける印。
void check_runner_answer_numbering() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/runner_m22.pl";
  {
    std::ofstream file(path, std::ios::binary);
    file << "test(m22_old_text, (length(L, 2), length(M, 1), h1 = h1), solutions([(L = [_G1,_G2], M = [_G1])])).\n"
            "test(m22_shared_as_apart, copy_term(h2(Q)-g(Q), X-Y), solutions([(X = h2(_), Y = g(_))])).\n"
            "test(m22_apart_as_shared, (copy_term(h3(_), X), copy_term(g(_), Y)), "
            "solutions([(X = h3(_G1), Y = g(_G1))])).\n";
  }
  std::ostringstream captured;
  auto* saved = std::cerr.rdbuf(captured.rdbuf());
  const int code = mosa::run_suite(path);
  std::cerr.rdbuf(saved);
  const std::string text = captured.str();
  mosa_test::check(code == 1, "wrong answer texts fail");
  mosa_test::check(text.find("h1") != std::string::npos, "M22 text no longer matches: " + text);
  mosa_test::check(text.find("h2") != std::string::npos, "shared variable is not two variables");
  mosa_test::check(text.find("h3") != std::string::npos, "two variables are not one shared variable");
}

// Muse のレビューの High。solutions は解をすべて並べる。並べた後にも解が残るゴール（e1）と、
// 空のリストなのに成功するゴール（e2）は落ちる。ちょうど並べたゴールと、失敗するゴールの空のリストは通る。
void check_runner_counts_every_solution() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/runner_every_solution.pl";
  {
    std::ofstream file(path, std::ios::binary);
    file << "p(1).\np(2).\np(3).\n"
            "test(extra, (p(X), e1 = e1), solutions([X = 1, X = 2])).\n"
            "test(empty_but_true, e2 = e2, solutions([])).\n"
            "test(exact, p(X), solutions([X = 1, X = 2, X = 3])).\n"
            "test(empty_fails, fail, solutions([])).\n";
  }
  std::ostringstream captured;
  auto* saved = std::cerr.rdbuf(captured.rdbuf());
  const int code = mosa::run_suite(path);
  std::cerr.rdbuf(saved);
  const std::string text = captured.str();
  mosa_test::check(code == 1, "extra solutions fail");
  mosa_test::check(text.find("e1") != std::string::npos, "a solution after the list fails: " + text);
  mosa_test::check(text.find("e2") != std::string::npos, "an empty list needs a failing goal: " + text);
  mosa_test::check(text.find("p(X)\n") == std::string::npos && text.find("fail\n") == std::string::npos,
                   "exact lists pass: " + text);
}

// Muse のレビューの低。中を一覧できないフォルダは、filesystem_error で落ちずに 1 で終える。
// 権限を外しても読めてしまう OS や root では確かめられないので、開けるかを先に試して飛ばす。
void check_runner_unreadable_folder() {
  const std::filesystem::path dir = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/runner_unreadable";
  std::error_code error;
  std::filesystem::permissions(dir, std::filesystem::perms::owner_all, error);
  std::filesystem::create_directories(dir, error);
  std::ofstream(dir / "a.pl", std::ios::binary) << "test(a_ok, true, true).\n";
  std::filesystem::permissions(dir, std::filesystem::perms::none, error);
  std::error_code probe;
  const std::filesystem::directory_iterator opened(dir, probe);
  if (probe) {
    std::ostringstream captured;
    auto* saved = std::cerr.rdbuf(captured.rdbuf());
    const int code = mosa::run_suite(dir.string());
    std::cerr.rdbuf(saved);
    mosa_test::check(code == 1, "unreadable folder fails");
    mosa_test::check(captured.str().find("runner_unreadable") != std::string::npos,
                     "unreadable folder is named: " + captured.str());
  }
  std::filesystem::permissions(dir, std::filesystem::perms::owner_all, error);
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

// 全体レビュー推奨3。試験が 0 件のファイルと、対象ファイルが 0 件のフォルダは失敗にする。
// 前はどちらも 0 で終え、消えた試験に気づけなかった。
void check_runner_empty_fails() {
  const std::filesystem::path base = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/runner_empty";
  std::error_code error;
  std::filesystem::remove_all(base, error);
  std::filesystem::create_directories(base / "bare", error);
  std::filesystem::create_directories(base / "only_clauses", error);
  std::ofstream(base / "no_exams.pl", std::ios::binary) << "helper(1).\n";
  std::ofstream(base / "only_clauses" / "a.pl", std::ios::binary) << "helper(1).\n";
  const auto run_captured = [](const std::filesystem::path& place, std::string& text) {
    std::ostringstream captured;
    auto* saved = std::cerr.rdbuf(captured.rdbuf());
    const int code = mosa::run_suite(place.string());
    std::cerr.rdbuf(saved);
    text = captured.str();
    return code;
  };
  std::string text;
  mosa_test::check(run_captured(base / "no_exams.pl", text) == 1, "a file with no tests fails");
  mosa_test::check(text.find("no_exams.pl") != std::string::npos, "the empty file is named: " + text);
  mosa_test::check(run_captured(base / "only_clauses", text) == 1, "a folder of testless files fails");
  mosa_test::check(text.find("a.pl") != std::string::npos, "the testless file is named: " + text);
  mosa_test::check(run_captured(base / "bare", text) == 1, "a folder with no files fails");
  mosa_test::check(text.find("bare") != std::string::npos, "the bare folder is named: " + text);
  std::ofstream(base / "one.pl", std::ios::binary) << "test(one_ok, true, true).\n";
  mosa_test::check(run_captured(base / "one.pl", text) == 0, "one passing test still passes");
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

// レビュー M8。メッセージの引数は字面にしてから旗のまま読み直す。旗が真で '：' が : に化けず、
// '、' で鍵の文面に落ちないこと。カタログに無い鍵の代わりの表示も同じ旗で引用する。
void check_fullwidth_message_args() {
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal =
      "set_prolog_flag(message_language, ja), set_prolog_flag(japanese_punctuation, true), throw(f(a, '：', '、')).";
  std::vector<std::string_view> args{arg0, arg_g, goal};
  auto result = mosa::run_session(args, "", false);
  mosa_test::check(result.err.find("警告: ゴールは例外を送出しました: f(a,'：','、')") != std::string::npos,
                   "fullwidth message args quoted");
  auto machine = mosa::Machine::create();
  const mosa::Stop flag = machine->query("set_prolog_flag(japanese_punctuation, true).");
  mosa_test::check(flag.kind == mosa::StopKind::Success, "fullwidth fallback flag set");
  const std::array<mosa::Cell, 1> atoms{machine->heap().atom(machine->atoms().intern("、"))};
  mosa_test::check(mosa::render_message(*machine, "m8_no_such_key", atoms) == "m8_no_such_key: '、'",
                   "fullwidth fallback quoted");
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

// SPEC 第7節。読み込みの診断も旗の言語で出す。日本語の文面は ja のカタログが持つ。
void check_load_diagnostics_en() {
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "set_prolog_flag(message_language, en), consult('/no/such/mosa-live.pl').";
  std::vector<std::string_view> args{arg0, arg_g, goal};
  auto absent = mosa::run_session(args, "", false);
  mosa_test::check(absent.err.find("/no/such/mosa-live.pl: cannot open file") != std::string::npos,
                   "missing consult en phrase");
  auto machine = mosa::Machine::create();
  static_cast<void>(machine->query("set_prolog_flag(message_language, en)."));
  const auto gap = machine->load_program("p(1).\nq(1).\np(2).\n");
  mosa_test::check(!gap.ok && machine->diagnostic().find("clauses of p/1 are not together") != std::string_view::npos,
                   "gap en phrase");
  static_cast<void>(machine->query("set_prolog_flag(message_language, ja)."));
  const auto gap_ja = machine->load_program("r(1).\ns(1).\nr(2).\n");
  mosa_test::check(!gap_ja.ok && machine->diagnostic().find("述語 r/1 の節が離れている") != std::string_view::npos,
                   "gap ja phrase");
}

// CMake は LANG=C でこの関数だけを流す。マシンを作る前の使い方の表示も LANG の言語で出す。
void check_english_default() {
  const std::string arg0 = "mosa";
  const std::string bad = "--no-such-option";
  std::vector<std::string_view> args{arg0, bad};
  auto result = mosa::run_session(args, "", false);
  mosa_test::check(result.code == 1, "english usage exit");
  mosa_test::check(result.err.find("mosa: usage: mosa") != std::string::npos, "english usage text");
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

// -g のゴールは末尾の . を省いてよい（SWI-Prolog と同じ）。読めないゴールは false. ではなく構文エラーを出す。
mosa::SessionResult run_goal_arg(const std::string& goal) {
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string arg_t = "-t";
  const std::string halt = "halt";
  std::vector<std::string_view> args{arg0, arg_g, goal, arg_t, halt};
  return mosa::run_session(args, "", false);
}

void check_goal_without_period() {
  // writeq の出力は本物の標準出力へ行くので、答えの行で見る。
  const auto bare = run_goal_arg("X = abc");
  mosa_test::check(bare.code == 0 && bare.out.find("X = abc.") != std::string::npos, "goal without period runs");
  const auto ended = run_goal_arg("X = abc.");
  mosa_test::check(ended.code == 0 && ended.out.find("X = abc.") != std::string::npos, "goal with period still runs");
  const auto comment = run_goal_arg("X = abc % 注");
  mosa_test::check(comment.code == 0 && comment.out.find("X = abc.") != std::string::npos, "goal ending in a comment");
  const auto broken = run_goal_arg("foo(");
  mosa_test::check(broken.code == 2, "broken goal exit");
  mosa_test::check(broken.out.find("false.") == std::string::npos, "broken goal is not false");
  mosa_test::check(broken.err.find("構文エラー") != std::string::npos, "broken goal shows syntax error");
  // 対話では、読めないゴールの後もプロンプトへ進む。例外のときと同じ。
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "foo(";
  std::vector<std::string_view> args{arg0, arg_g, goal};
  const auto tty = mosa::run_session(args, "X = 1.\n", true);
  mosa_test::check(tty.err.find("構文エラー") != std::string::npos, "tty broken goal shows syntax error");
  mosa_test::check(tty.out.find("X = 1.") != std::string::npos, "tty broken goal reaches prompt");
}

// initialization(G, main) は、ファイルを読み終えたあとに G を 1 回だけ呼ぶ。終了コードは G の結果で決まる。
mosa::SessionResult run_main_program(const std::string& name, const std::string& body, bool with_goal = false) {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/" + name + ".pl";
  std::ofstream file(path, std::ios::binary);
  file << ":- initialization(main, main).\n" << body;
  file.close();
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "write(goal_ran).";
  std::vector<std::string_view> args{arg0, path};
  if (with_goal) {
    args.push_back(arg_g);
    args.push_back(goal);
  }
  return mosa::run_session(args, "", false);
}

void check_init_main() {
  const auto ok = run_main_program("session_main_ok", "main.\n", true);
  mosa_test::check(ok.code == 0, "init_main_success exit");
  mosa_test::check(ok.out.empty() && ok.err.empty(), "init_main_success prints nothing");
  mosa_test::check(ok.out.find("goal_ran") == std::string::npos, "main exit skips -g");
  const auto failed = run_main_program("session_main_fail", "main :- fail.\n");
  mosa_test::check(failed.code == 1, "init_main_fail exit");
  const auto thrown = run_main_program("session_main_throw", "main :- throw(foo).\n");
  mosa_test::check(thrown.code == 2, "init_main_throw exit");
  mosa_test::check(thrown.err.find("警告: ゴールは例外を送出しました: foo") != std::string::npos,
                   "init_main_throw message");
  const auto halted = run_main_program("session_main_halt3", "main :- halt(3).\n");
  mosa_test::check(halted.code == 3, "init_main_halt3 exit");
}

// 敵対的レビュー A3（M11、ADR 0016）。選択点を残して成功した -g のゴール、対話の答え、main は、SWI-Prolog の
// '$run_init_goal' と toplevel と同じく答えの後で切り、setup_call_cleanup/3 の後始末を動かす。その例外はゴールの例外。
// SessionResult は Echo の出力だけを持ち write/1 の出力は持たないので、後始末が投げる警告で動いたことを見る。
// 書いた順（答えの後に後始末）は mosac_run_exit_code の cleanup_main.pl が見る。
void check_answer_runs_cleanup() {
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "setup_call_cleanup(true, (X = 1 ; X = 2), throw(oops(X))).";
  std::vector<std::string_view> args{arg0, arg_g, goal};
  const auto batch = mosa::run_session(args, "", false);
  const auto answer = batch.out.find("X = 1.");
  mosa_test::check(batch.code == 2 && answer != std::string::npos && batch.err.find("oops(1)") != std::string::npos,
                   "-g answer then cleanup");
  std::vector<std::string_view> plain{arg0};
  const auto talk = mosa::run_session(plain, goal + "\n\n", true);
  mosa_test::check(talk.out.find("X = 1") != std::string::npos && talk.err.find("oops(1)") != std::string::npos,
                   "toplevel answer then cleanup");
  const auto main_raised = run_main_program(
      "session_main_cleanup", "main :- setup_call_cleanup(true, (X = 1 ; X = 2), throw(oops(X))).\n");
  mosa_test::check(main_raised.code == 2 && main_raised.err.find("oops(1)") != std::string::npos, "main answer then cleanup");
}

// 指示の halt/0,1 は読み込みを止め、その値で終わる。後の指示、initialization(_, main)、-g は動かない。
// -g が動けば答えの行 true. が out に出、main が動けば 0 で終わる。
void check_directive_halt() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/session_directive_halt.pl";
  std::ofstream file(path, std::ios::binary);
  file << ":- initialization(main, main).\nmain.\n:- halt(4).\n:- throw(after_halt).\n";
  file.close();
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "true.";
  std::vector<std::string_view> args{arg0, path, arg_g, goal};
  const auto result = mosa::run_session(args, "", false);
  mosa_test::check(result.code == 4, "directive_halt exit " + std::to_string(result.code));
  mosa_test::check(result.out.empty() && result.err.empty(), "directive_halt stops load, main and -g");
  const std::string bare = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/session_directive_halt0.pl";
  std::ofstream zero(bare, std::ios::binary);
  zero << ":- halt.\n:- throw(after_halt).\n";
  zero.close();
  std::vector<std::string_view> zero_args{arg0, bare, arg_g, goal};
  const auto stopped = mosa::run_session(zero_args, "", false);
  mosa_test::check(stopped.code == 0 && stopped.out.empty() && stopped.err.empty(), "directive_halt0 stops load");
}

// Muse のレビューの中。プロンプトに打った指示の halt/0,1 も、ファイルの指示と同じくその値で終わる。
// 続けて打った行は動かず、空の警告も出ない。
void check_prompt_directive_halt() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/session_p.pl";
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0, path};
  const auto result = mosa::run_session(args, ":- halt(4).\nwrite(after_halt), nl.\n", true);
  mosa_test::check(result.code == 4, "prompt directive halt exit " + std::to_string(result.code));
  mosa_test::check(result.out.find("after_halt") == std::string::npos, "prompt directive halt stops the prompt");
  mosa_test::check(result.err.empty(), "prompt directive halt warns nothing, got [" + result.err + "]");
  const auto bare = mosa::run_session(args, ":- halt.\nwrite(after_halt), nl.\n", true);
  mosa_test::check(bare.code == 0 && bare.out.find("after_halt") == std::string::npos && bare.err.empty(),
                   "prompt directive halt/0 stops the prompt");
}

void check_argv_after_dashes() {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/session_p.pl";
  const std::string arg0 = "mosa";
  const std::string arg_g = "-g";
  const std::string goal = "current_prolog_flag(argv, X).";
  const std::string arg_t = "-t";
  const std::string halt = "halt";
  const std::string dashes = "--";
  const std::string first = "a";
  const std::string second = "漢字";
  std::vector<std::string_view> args{arg0, path, arg_g, goal, arg_t, halt, dashes, first, second};
  const auto result = mosa::run_session(args, "", false);
  mosa_test::check(result.code == 0 && result.out.find("X = [a,漢字].") != std::string::npos,
                   "argv_after_dashes list");
  std::vector<std::string_view> bare{arg0, path, arg_g, goal, arg_t, halt};
  const auto none = mosa::run_session(bare, "", false);
  mosa_test::check(none.out.find("X = [].") != std::string::npos, "argv without dashes is empty");
}

void check_load_then_exit();
void check_repeated_goals();

int main(int argc, char** argv) {
  if (argc > 1 && std::string_view{argv[1]} == "english") {
    check_english_default();
    return mosa_test::failures();
  }
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
  check_fullwidth_quoted_answer();
  check_given_made_up_name();
  check_answer_numbering();
  check_answer_aliases();
  check_anonymous_under_char_conversion();
  check_answer_operands();
  check_runner();
  check_runner_reports_every_file();
  check_runner_empty_fails();
  check_runner_answer_numbering();
  check_runner_counts_every_solution();
  check_runner_unreadable_folder();
  check_uncaught_message("ja", "警告: ゴールは例外を送出しました: foo", "uncaught_ja");
  check_uncaught_message("en", "Warning: goal raised exception: foo", "uncaught_en");
  check_fullwidth_message_args();
  check_message_language_default();
  check_language_from_lang();
  check_help_japanese_variables();
  check_syntax_shown();
  check_load_diagnostics_en();
  check_goal_without_period();
  check_init_main();
  check_answer_runs_cleanup();
  check_directive_halt();
  check_prompt_directive_halt();
  check_argv_after_dashes();
  check_load_then_exit();
  check_repeated_goals();
  return mosa_test::failures();
}
