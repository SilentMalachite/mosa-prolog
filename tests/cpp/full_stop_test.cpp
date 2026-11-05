// レビュー L17（iso-notes 6.4.2 の決定 A）。書いた項に終止符を足す REPL、トップレベルの答え、initialization(G, main)、
// mosatest。書いた字面を char_conversion なしで読み直すことも見る。
// 字句解析器の側は lexer_test の test_graphic_full_stop、読みと mosatest は tests/pl/read.pl と japanese.pl の l17_*。
#include "answer_lines.hpp"
#include "harness.hpp"
#include "mosa/repl.hpp"
#include "mosa/suite.hpp"

#include <array>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Pasted {
  std::string_view setup;
  std::string_view goal;
  std::string_view answer;
};

// レビュー L17。6.4.2 では図形の字に続く . は同じトークンに入るので、書いた項に足す終止符は、最後が図形の字なら
// 空白の後に置く。前は対話が読み直すゴール X= #. に終止符が無く、答え X = #. も貼り直すと読めなかった。
void check_full_stop_after_graphic() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  const std::array<Pasted, 4> cases{{
      {"", "X = '#'.", "X = # ."},
      {"", "X = '+.'.", "X = +. ."},
      {"", "X = - '#'.", "X = - # ."},
      {"", "X = '#', Y = a.", "X = #, Y = a."},
  }};
  for (const Pasted& item : cases) {
    const std::string setup = item.setup.empty() ? std::string{} : std::string{item.setup} + "\n";
    auto first = mosa::run_session(args, setup + std::string{item.goal} + "\n", true);
    mosa_test::check(answered(first.out, item.answer), std::string{item.goal} + " -> " + first.out);
    auto again = mosa::run_session(args, setup + std::string{item.answer} + "\n", true);
    mosa_test::check(answered(again.out, item.answer), std::string{item.answer} + " pasted back -> " + again.out);
  }
  // ; の促しを空の行で止めた答えも同じ終止符。
  auto talk = mosa::run_session(args, "(X = '#' ; X = b).\n\n", true);
  mosa_test::check(answered(talk.out, "X = # ."), "prompt stopped after # -> " + talk.out);
}

std::string write_tmp(const std::string& name, std::string_view text) {
  const std::string path = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/" + name;
  std::ofstream file(path, std::ios::binary);
  file << text;
  return path;
}

// initialization(G, main) は G を書いて読み直す。
void check_init_main() {
  const std::string arg0 = "mosa";
  const std::string path = write_tmp("full_stop_main.pl", ":- initialization(X = '#', main).\n");
  std::vector<std::string_view> args{arg0, path};
  const auto ran = mosa::run_session(args, "", false);
  mosa_test::check(ran.code == 0 && ran.err.empty(), "init main ending in # -> " + ran.err);
}

// L17 の Codex レビュー。読み直しに char_conversion をかけていたので、空白を a に写すと " ." は # a. と読まれ、'#'. が
// false になった。Mosa が書いた字面は変換なしで読み直す。ここのゴールは、その変換の下で利用者が打てる字面である。
// 答えは見せるだけなので、ここでは見ない。
void check_converted() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  auto space = mosa::run_session(
      args, "assertz('#').\nchar_conversion(' ', a), set_prolog_flag(char_conversion, on).\n'#'.\n", true);
  mosa_test::check(space.out.find("false") == std::string::npos && space.err.empty(),
                   "goal # under char_conversion of the space -> " + space.out + space.err);
  // 空白、改行、タブがどれも写されるなら、利用者は '#'. を垂直タブで終える。
  auto three = mosa::run_session(args,
                                 "assertz('#').\nchar_conversion(' ', a), char_conversion('\\n', a), "
                                 "char_conversion('\\t', a), set_prolog_flag(char_conversion, on).\n'#'.\v\n",
                                 true);
  mosa_test::check(three.out.find("false") == std::string::npos && three.err.empty(),
                   "goal # under char_conversion of three layout characters -> " + three.out + three.err);
  // main は読み込みが終わってから読む。後ろの指令が変換を有効にしても、ゴールは書いたときのまま。
  const std::string main_path = write_tmp(
      "full_stop_main_converted.pl",
      ":- initialization('#', main).\n'#'.\n:- char_conversion(' ', a), set_prolog_flag(char_conversion, on).\n");
  std::vector<std::string_view> main_args{arg0, main_path};
  const auto ran = mosa::run_session(main_args, "", false);
  mosa_test::check(ran.code == 0 && ran.err.empty(), "init main after a later char_conversion -> " + ran.err);
  // mosatest も、前のテストが有効にしたままの変換をゴールの読み直しにかけない。
  const std::string suite_path =
      write_tmp("full_stop_suite_converted.pl",
                "'#'.\ntest(conv_on, (char_conversion(' ', a), set_prolog_flag(char_conversion, on)), true).\n"
                "test(graphic, '#', true).\n");
  std::ostringstream captured;
  auto* saved = std::cerr.rdbuf(captured.rdbuf());
  const int code = mosa::run_suite(suite_path);
  std::cerr.rdbuf(saved);
  mosa_test::check(code == 0, "mosatest goal # after a test turned char_conversion on -> " + captured.str());
}

// 字のコードごとに char_conversion(C, a) を並べる。REPL は lists を読み込まないので member/2 は使えない。
std::string convert_to_a(std::initializer_list<int> codes) {
  std::string goal;
  for (const int code : codes) {
    const std::string name = "C" + std::to_string(code);
    goal += "char_code(" + name + ", " + std::to_string(code) + "), char_conversion(" + name + ", a), ";
  }
  return goal;
}

// SPEC 第4節で配置文字になりうる字（ASCII の 6 つ、U+0085、Zs、Zl、Zp）をどれも a に写す。
std::string every_layout_to_a() {
  return convert_to_a({32, 10, 9, 11, 12, 13, 0x85, 0x3000, 0xA0, 0x1680, 0x2000, 0x2001, 0x2002, 0x2003, 0x2004,
                       0x2005, 0x2006, 0x2007, 0x2008, 0x2009, 0x200A, 0x2028, 0x2029, 0x202F, 0x205F});
}

// L17 の Codex レビュー 3〜6 回目。ASCII の 6 つの配置文字がどれも写されるなら、利用者は '#'. を写されない Unicode の
// 配置文字（U+3000）か、表の中で配置文字へ写される字（z）で終える。
void check_converted_everything() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  const std::string ascii = "assertz('#').\n" + convert_to_a({32, 10, 9, 11, 12, 13}) +
                            "set_prolog_flag(char_conversion, on).\n'#'.\u3000";
  auto wide = mosa::run_session(args, ascii, true);
  mosa_test::check(wide.out.find("false") == std::string::npos && wide.err.empty(),
                   "goal # with every ascii layout character converted -> " + wide.out + wide.err);
  const std::string all =
      "assertz('#').\n" + every_layout_to_a() + "char_conversion(z, ' '), set_prolog_flag(char_conversion, on).\n'#'.z";
  auto mapped = mosa::run_session(args, all, true);
  mosa_test::check(mapped.out.find("false") == std::string::npos && mapped.err.empty(),
                   "goal # with every layout character converted -> " + mapped.out + mapped.err);
  // 配置文字を 1 つも読めない変換でも、利用者は '#'.% と書ける（Codex レビュー 4、5 回目）。
  const std::string no_layout = all.substr(0, all.find("char_conversion(z, ' ')"));
  auto quoted = mosa::run_session(
      args, no_layout + "char_conversion('(', a), set_prolog_flag(char_conversion, on).\n'#'.%", true);
  mosa_test::check(quoted.out.find("false") == std::string::npos && quoted.err.empty(),
                   "goal # with no layout character and ( converted -> " + quoted.out + quoted.err);
  // 後置の ## の前が引用した原子か単独の 0 なら、利用者は /**/ で分ける。1 の後はそのまま引用する（Codex レビュー 6 回目）。
  const std::string postfix = "op(200, xf, ##).\nassertz(('a b' ##)), assertz((0 ##)), assertz((1 ##)).\n" +
                              no_layout + "char_conversion('(', a), set_prolog_flag(char_conversion, on).\n";
  for (const std::string_view goal : {"'a b'/**/'##'.%", "0/**/'##'.%", "1'##'.%"}) {
    auto after = mosa::run_session(args, postfix + std::string{goal}, true);
    mosa_test::check(after.out.find("false") == std::string::npos && after.err.empty(),
                     std::string{goal} + " with no layout character left -> " + after.out + after.err);
  }
  // ' も写されるなら、利用者は括弧で囲む。
  auto bracketed = mosa::run_session(
      args, "op(200, xf, ##).\nassertz((x ##)).\n" + no_layout +
                "char_conversion('\\'', a), set_prolog_flag(char_conversion, on).\n(x##).%",
      true);
  mosa_test::check(bracketed.out.find("false") == std::string::npos && bracketed.err.empty(),
                   "goal with no layout character and ' converted -> " + bracketed.out + bracketed.err);
}

// L17 の Codex レビュー 7 回目。区切りの字（. ' /* ( )）そのものが写されるなら、利用者は表の中でその字へ写される字で
// 書く。引用と注釈の中は写されないので、閉じる ' と */ は元の字のままである。
void check_converted_inverse() {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  const std::string no_layout = every_layout_to_a();
  const std::string on = "set_prolog_flag(char_conversion, on).\n";
  const std::string postfix = "op(200, xf, ##).\nassertz(('a b' ##)), assertz((x ##)).\n";
  const std::string cases[][2] = {
      // / が写され、z が / に写される。注釈は z**/ と書く。
      {postfix + no_layout + "char_conversion('(', a), char_conversion('/', a), char_conversion(z, '/'), " + on,
       "'a b'z**/'##'.%"},
      // ' が写され、w が ' に写される。引用は w##' と書く。
      {postfix + no_layout + "char_conversion('(', a), char_conversion('\\'', a), char_conversion(w, '\\''), " + on,
       "xw##'.%"},
      // ( が写され、[ が ( に写される。括弧は [x##) と書く。
      {postfix + no_layout + "char_conversion('(', a), char_conversion('\\'', a), char_conversion('[', '('), " + on,
       "[x##).%"},
      // . が写され、w が . に写される。終止符は w と書く。
      {"assertz('#').\nchar_conversion(w, '.'), char_conversion('.', a), " + on, "'#' w"},
  };
  for (const auto& [setup, goal] : cases) {
    auto ran = mosa::run_session(args, setup + goal, true);
    mosa_test::check(ran.out.find("false") == std::string::npos && ran.err.empty(),
                     goal + " with its separator converted -> " + ran.out + ran.err);
  }
}

// L17 の Codex レビュー 8 回目。配置文字と ' と括弧がどれも写されると、書いた字面をどう区切っても変換の下では
// 読めない。main の '#' と、全角の括弧で打った（#）が偽になった。a を b、b を c に写すと、X = a の答えが X = c に
// なった（読んだ項 X = b を書いて、もう一度写していた）。書いた字面は変換なしで読み直す。
void check_written_unconverted() {
  const std::string arg0 = "mosa";
  const std::string blocked = every_layout_to_a() +
                              "char_conversion('\\'', a), char_conversion('(', a), char_conversion(')', a), "
                              "set_prolog_flag(char_conversion, on).";
  const std::string path = write_tmp("full_stop_main_blocked.pl", ":- initialization('#', main).\n'#'.\n:- " + blocked);
  std::vector<std::string_view> main_args{arg0, path};
  const auto ran = mosa::run_session(main_args, "", false);
  mosa_test::check(ran.code == 0 && ran.err.empty(), "init main # with layout, ' and brackets converted -> " + ran.err);
  std::vector<std::string_view> args{arg0};
  auto wide = mosa::run_session(
      args, "assertz('#').\nset_prolog_flag(japanese_punctuation, true).\n" + blocked + "\n（#）.%", true);
  mosa_test::check(wide.out.find("false") == std::string::npos && wide.err.empty(),
                   "full-width brackets with layout, ' and brackets converted -> " + wide.out + wide.err);
  auto twice = mosa::run_session(
      args, "char_conversion(a, b), char_conversion(b, c), set_prolog_flag(char_conversion, on).\nX = a.\n", true);
  mosa_test::check(answered(twice.out, "X = b."), "X = a under a to b and b to c -> " + twice.out);
}

}  // namespace

int main() {
  check_full_stop_after_graphic();
  check_init_main();
  check_converted();
  check_converted_everything();
  check_converted_inverse();
  check_written_unconverted();
  return mosa_test::failures();
}
