// レビュー M12。標準入力の read/1 は、項が閉じるまで 1 バイトずつ足して読み直す。足した途中で末尾がドットでも、
// 次の字を見るまでは終止符と決めない（6.4.8: end char の後ろは layout char か %）。1.5 の小数点で切らない。
// std::cin の入力元を差し替え、パイプと同じく端末でない入力として user_input に読ませる。
#include "harness.hpp"
#include "mosa/console.hpp"
#include "mosa/machine.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <streambuf>
#include <utility>
#include <vector>

namespace {

// input を標準入力に置き、新しいマシンで goal を一度だけ問う。
bool holds(std::string_view input, std::string_view goal) {
  std::stringstream feed;
  feed << input;
  auto* old = std::cin.rdbuf(feed.rdbuf());
  bool ok = false;
  {
    auto machine = mosa::Machine::create();
    ok = machine->query(std::string{goal}).kind == mosa::StopKind::Success;
  }
  std::cin.rdbuf(old);
  std::cin.clear();
  return ok;
}

void check(std::string_view input, std::string_view goal, std::string_view label) {
  mosa_test::check(holds(input, goal), std::string{label});
}

void check_floats() {
  check("X = 1.5.\n", "read(T), T = (V = F), var(V), F == 1.5, read(E), E == end_of_file.", "review repro");
  check("1.5.\n", "read(F), F == 1.5.", "bare float");
  check("foo(2.25). bar(3.125).\n", "read(A), A == foo(2.25), read(B), B == bar(3.125).", "two floats");
  check("3.14159.\n", "read(F), F == 3.14159.", "many fraction digits");
  check("1.5e10.\n", "read(F), F == 1.5e10.", "exponent");
  check("2.5E-3.\n", "read(F), F == 2.5e-3.", "negative exponent");
  check("[1.5,2.5|T].\n", "read(L), L = [A, B|T], var(T), A == 1.5, B == 2.5.", "floats in a list");
  check("- 0.5.\n", "read(F), F == -0.5.", "negative float");
  check("X = 1.5.", "read(T), T = (_ = F), F == 1.5.", "float then end of input");
}

void check_ends() {
  check("1.\n", "read(I), I == 1.", "integer then newline");
  check("X = 1.", "read(T), T = (_ = I), I == 1.", "dot at the very end of input");
  check("a.", "read(A), A == a, read(E), E == end_of_file.", "atom at the very end of input");
  check("a.%c\nb.\n", "read(A), A == a, read(B), B == b.", "percent after the end");
  check("a.\tb.\n", "read(A), A == a, read(B), B == b.", "tab after the end");
  check("a.\nx", "read(A), A == a, get_char(N), N == '\\n', get_char(C), C == x.", "layout after the end is left");
  check("a. b.\n", "read(A), A == a, peek_char(S), S == ' ', read(B), B == b.", "space after the end is left");
  check("'a.b'.\n", "read(A), A == 'a.b'.", "dot inside quotes");
  check("X =.. Y.\n", "read(T), T = (A =.. B), var(A), var(B).", "univ");
  check("f(x) .\n", "read(T), T == f(x).", "layout before the end");
  check("a..\n", "catch(read(_), error(syntax_error(_), _), true), read(E), E == end_of_file.", "two dots");
}

// 終止符を確かめるために先読みした字は標準入力の読み取り口へ返す。REPL は次の行をそこから読むので、read/1 の後の
// 改行は修正前と同じく REPL の空行になり、次の問い合わせの get_char に残らない（敵対的レビュー A1）。
// rest は問い合わせのあと読み取り口から読める字。
std::string left_after(std::string_view input, std::string_view goal, bool& ok) {
  std::stringstream feed;
  feed << input;
  auto* old = std::cin.rdbuf(feed.rdbuf());
  std::string rest;
  {
    auto machine = mosa::Machine::create();
    ok = machine->query(std::string{goal}).kind == mosa::StopKind::Success;
    for (int ch = mosa::standard_input_get(); ch != EOF; ch = mosa::standard_input_get()) {
      rest.push_back(static_cast<char>(ch));
    }
  }
  std::cin.rdbuf(old);
  std::cin.clear();
  return rest;
}

void check_left(std::string_view input, std::string_view goal, std::string_view expected, std::string_view label) {
  bool ok = false;
  const std::string rest = left_after(input, goal, ok);
  mosa_test::check(ok, std::string{label} + " query");
  mosa_test::check(rest == expected, std::string{label} + " left [" + rest + "]");
}

void check_lookahead_returned() {
  check_left("foo.\nbar\n", "read(X), X == foo.", "\nbar\n", "newline goes back");
  check_left("X = 1.5.\nz", "read(T), T = (_ = F), F == 1.5.", "\nz", "newline after a float goes back");
  check_left("a.%c\nz", "read(A), A == a.", "%c\nz", "percent goes back");
  check_left("foo.\nbar\n", "read(X), X == foo, get_char(C), C == '\\n'.", "bar\n", "get_char in the same query");
  check_left("a.\nb.\n", "read(A), A == a, read(B), B == b.", "\n", "two reads");
  check_left("foo.", "read(X), X == foo.", "", "nothing after the end");
  check_left("xy", "peek_char(C), C == x.", "y", "peeked char stays in the stream");
  check_left("xfoo.\nz", "peek_char(C), C == x, get_char(_), read(T), T == foo.", "\nz", "peek then read");
}

// 構文誤りでも、先読みした字は返す。成功と失敗で REPL に渡る字が変わらない（Grok レビュー Low）。
void check_failure_returned() {
  const std::string_view caught = "catch(read(_), error(syntax_error(_), _), true).";
  check_left("foo bar.\nz", caught, "\nz", "newline after an error goes back");
  check_left("foo bar.%c\nz", caught, "%c\nz", "percent after an error goes back");
  check_left("f(.\nz", caught, "\nz", "newline after an unclosed term goes back");
  check("foo bar.\nb.\n", "catch(read(_), error(syntax_error(_), _), true), read(B), B == b.", "read after an error");
}

// 端末で foo. と打って Ctrl-D を押すと、終止符を確かめる読みが入力の終わりに当たる。項はもう読めたので、その終わりは
// 区切りとしてだけ使い、印を下ろす。残すと REPL の次の行の読みがすぐ終わりを見て REPL を閉じる（Grok レビュー Medium）。
// rdbuf を差し替えると終わりの印も下りるので、端末と同じく一度 EOF を返した後も字を出す streambuf で読ませる。
// chunks の空文字列が Ctrl-D の 1 回に当たる。
class TerminalBuf : public std::streambuf {
 public:
  explicit TerminalBuf(std::vector<std::string> chunks) : chunks_(std::move(chunks)) {}

 protected:
  int_type underflow() override {
    while (next_ < chunks_.size()) {
      std::string& chunk = chunks_[next_++];
      if (chunk.empty()) return traits_type::eof();
      setg(chunk.data(), chunk.data(), chunk.data() + chunk.size());
      return traits_type::to_int_type(chunk.front());
    }
    return traits_type::eof();
  }

 private:
  std::vector<std::string> chunks_;
  std::size_t next_ = 0;
};

std::string after_end(std::vector<std::string> chunks, std::string_view goal, bool& ok) {
  TerminalBuf feed(std::move(chunks));
  auto* old = std::cin.rdbuf(&feed);
  std::string rest;
  {
    auto machine = mosa::Machine::create();
    ok = machine->query(std::string{goal}).kind == mosa::StopKind::Success;
    for (int ch = mosa::standard_input_get(); ch != EOF; ch = mosa::standard_input_get()) {
      rest.push_back(static_cast<char>(ch));
    }
  }
  std::cin.rdbuf(old);
  std::cin.clear();
  return rest;
}

void check_end_cleared() {
  bool ok = false;
  std::string rest = after_end({"foo.", "", "bar\n"}, "read(X), X == foo.", ok);
  mosa_test::check(ok, "end after a dot query");
  mosa_test::check(rest == "bar\n", "end after a dot is cleared [" + rest + "]");
  rest = after_end({"X = 1.5.", "", "z"}, "read(T), T = (_ = F), F == 1.5.", ok);
  mosa_test::check(ok, "end after a float query");
  mosa_test::check(rest == "z", "end after a float is cleared [" + rest + "]");
  rest = after_end({"a. b.", "", "z"}, "read(A), A == a, read(B), B == b.", ok);
  mosa_test::check(ok, "end after two terms query");
  mosa_test::check(rest == "z", "end after two terms is cleared [" + rest + "]");
  rest = after_end({"", "z"}, "read(E), E == end_of_file.", ok);
  mosa_test::check(ok, "plain end query");
  mosa_test::check(rest.empty(), "plain end stays [" + rest + "]");
  rest = after_end({"foo", "", "z"}, "catch(read(_), error(syntax_error(_), _), true).", ok);
  mosa_test::check(ok, "end inside a term query");
  mosa_test::check(rest.empty(), "end inside a term stays [" + rest + "]");
}

void check_japanese() {
  check("X = '親'.\n", "read(T), T = (_ = A), A == '親'.", "japanese atom");
  check("親(1.5).\n", "read(T), T == 親(1.5).", "japanese functor with a float");
}

}  // namespace

int main() {
  check_floats();
  check_ends();
  check_japanese();
  check_lookahead_returned();
  check_failure_returned();
  check_end_cleared();
  return mosa_test::failures();
}
