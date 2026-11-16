// フェーズ13 Task 3。条件付きコンパイル :- if/elif/else/endif の診断（13211-1 に無い。SPEC 第3節、ADR 0027）。
// tests/pl/cond_compile.pl の行は「読み込みが失敗する」ことしか見られないので、対の誤りの字面、条件の誤りの Formal、
// halt の終了コードをここで Machine::load_program の結果から見る（計画の「設計との違い」7）。期待の字面は
// message_text でカタログから作り、流す機械の言語に頼らない。書き出すファイルは tests/tmp/ の下に置く。
#include "harness.hpp"

#include "machine/internal.hpp"
#include "mosa/machine.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace {

bool contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }

// 読み込みが失敗し、診断に key の文面（引数 arg）が入ること。日英の両方のカタログで見る。
void check_message(std::string_view source, std::string_view key, std::string_view arg, std::string_view label) {
  for (const auto language : {mosa::MessageLanguage::En, mosa::MessageLanguage::Ja}) {
    const std::string tag = std::string(label) + (language == mosa::MessageLanguage::Ja ? " (ja)" : " (en)");
    auto machine = mosa::Machine::create();
    auto& impl = mosa::engine_impl(*machine);
    impl.message_lang = language;
    const mosa::LoadStatus status = machine->load_program(source);
    const std::string expected = arg.empty() ? mosa::message_text(impl, key) : mosa::message_text(impl, key, {arg});
    // カタログに鍵が無いと、両側が同じ代わりの字面（鍵と引数）になって比べても通ってしまう。
    mosa_test::check(!expected.starts_with(key), tag + ": the catalog has " + std::string(key));
    mosa_test::check(!status.ok, tag + ": the load fails");
    mosa_test::check(contains(status.message, expected), tag + ": message [" + status.message + "]");
  }
}

void check_cond_messages() {
  check_message(":- endif.\n", "cond_unmatched", ":-endif", "stray endif");
  check_message(":- if(true).\n:- else.\n:- elif(true).\n:- endif.\n", "cond_after_else", ":-elif(true)",
                "elif after else");
  check_message(":- if(true).\n:- else.\n:- else.\n:- endif.\n", "cond_after_else", ":-else", "second else");
  check_message(":- if(true).\n", "cond_unclosed", "", "unclosed if");
}

// 条件の例外は、一般の指令と同じく Formal を診断にして読み込みを失敗させる。
void check_condition_error(std::string_view source, std::string_view formal, std::string_view label) {
  auto machine = mosa::Machine::create();
  const mosa::LoadStatus status = machine->load_program(source);
  mosa_test::check(!status.ok, std::string(label) + ": the load fails");
  mosa_test::check(contains(status.message, formal), std::string(label) + ": message [" + status.message + "]");
}

void check_cond_condition_errors() {
  check_condition_error(":- if(_).\n:- endif.\n", "instantiation_error", "variable condition");
  check_condition_error(":- if(1).\n:- endif.\n", "type_error(callable,1)", "number condition");
  check_condition_error(":- if(throw(cond_ball)).\n:- endif.\n", "cond_ball", "throwing condition");
}

// 条件の中の halt は、一般の指令と同じく読み込みをその場で止め、終了コードを残す（Review Focus 4）。
void check_cond_halt() {
  auto machine = mosa::Machine::create();
  const mosa::LoadStatus status = machine->load_program(":- if(halt(3)).\n:- endif.\ncond_after_halt.\n");
  const auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(!status.ok, "halt: the load stops");
  mosa_test::check(impl.halt_now, "halt: halt_now is set");
  mosa_test::check(impl.halt_code == 3, "halt: the exit code is 3");
  // 問い合わせの入口は halt_now を消すので、halt の旗を見た後で問う。
  mosa_test::check(machine->query("current_predicate(cond_after_halt/0).").kind == mosa::StopKind::Failure,
                   "halt: the clause after the halt is not loaded");
}

// 読み飛ばす部分の構文の誤りは、診断を出さずに読み飛ばす（問い 3 の A）。
void check_cond_silent_skip() {
  auto machine = mosa::Machine::create();
  const mosa::LoadStatus status = machine->load_program(":- if(fail).\ncond_x(1r3).\n:- endif.\ncond_y.\n");
  mosa_test::check(status.ok, "silent skip: the load succeeds");
  mosa_test::check(status.message.empty(), "silent skip: no message [" + status.message + "]");
}

// :- if(C, T, E) は条件の指令でなく、今と同じく組み込みの if/3 を呼ぶ一般の指令である。
void check_cond_if3() {
  auto machine = mosa::Machine::create();
  const mosa::LoadStatus status = machine->load_program(":- if(true, assertz(cond_if3_ran), true).\n");
  mosa_test::check(status.ok, "if/3: the load succeeds [" + status.message + "]");
  mosa_test::check(machine->query("cond_if3_ran.").kind == mosa::StopKind::Success, "if/3: the goal ran");
  mosa_test::check(machine->next().kind == mosa::StopKind::Failure, "if/3: exactly one solution");
}

// if と endif の対はテキストごとに閉じる。include したテキストの閉じない if は、読んだ側の endif で閉じない。
void check_cond_include_balance() {
  const std::string dir = std::string(MOSA_SOURCE_DIR) + "/tests/tmp";
  std::filesystem::create_directories(dir);
  const std::string part = dir + "/p13_open_part.pl";
  {
    std::ofstream file(part, std::ios::binary);
    file << ":- if(true).\n";
  }
  const std::string source = ":- include('" + part + "').\n:- endif.\n";
  check_message(source, "cond_unclosed", "", "include balance");
}

}  // namespace

int main() {
  check_cond_messages();
  check_cond_condition_errors();
  check_cond_halt();
  check_cond_silent_skip();
  check_cond_if3();
  check_cond_include_balance();
  return mosa_test::failures();
}
