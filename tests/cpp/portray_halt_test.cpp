// レビュー L14。portray/1 の中で halt したら、print/1 がそれまでに書いた字面（項の前の部分と、portray が現在出力へ
// 書いたもの）は、write/1 の後の halt と同じく行き先の流れに届く。行き先は user_output、set_output/1 のファイル、
// print/2 の別名、入れ子の print/1 の portray。例外で抜けたときは今までどおり何も書かない（catch で続きが書かれる）。

#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/path_text.hpp"
#include "mosa/repl.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string tmp_path(std::string_view name) {
  return std::string(MOSA_SOURCE_DIR) + "/tests/tmp/" + std::string(name);
}

std::optional<std::string> slurp(const std::string& path) {
  std::ifstream input(mosa::path_from_utf8(path), std::ios::binary);
  if (!input) return std::nullopt;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string shown(const std::string& path) {
  const auto got = slurp(path);
  return got ? "[" + *got + "]" : "(missing)";
}

void put_file(const std::string& path, std::string_view bytes) {
  std::ofstream output(mosa::path_from_utf8(path), std::ios::binary | std::ios::trunc);
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void fresh(const std::string& path) {
  std::error_code code;
  std::filesystem::remove(mosa::path_from_utf8(path), code);
}

// user_output は std::cout へ書き、run_session の out（答えの行）には入らない。その間だけ受ける。
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

struct Run {
  int code = 0;
  std::string out;
};

// program を読み、goal を -g で動かす。
Run batch(std::string_view program_text, const std::string& goal) {
  const auto program = tmp_path("l14_portray.pl");
  put_file(program, program_text);
  const std::vector<std::string> words{"mosa", program, "-g", goal};
  const std::vector<std::string_view> args(words.begin(), words.end());
  const CoutCapture capture;
  const mosa::SessionResult result = mosa::run_session(args, "", false);
  std::cout.flush();
  return Run{result.code, capture.text()};
}

constexpr std::string_view k_halts = "portray(boom) :- write(p), halt(6).\n";

void check_user_output() {
  // レビューの再現そのもの（portray は -g の中で足す）。
  const Run repro = batch("", "assertz((portray(boom) :- write(p), halt(6))), print(f(boom)).");
  mosa_test::check(repro.code == 6, "repro exit " + std::to_string(repro.code));
  mosa_test::check(repro.out == "f(p", "repro writes f(p, got [" + repro.out + "]");
  // 項の残り（閉じ括弧）と、後のゴールは書かない。
  const Run list = batch(k_halts, "print([a, boom, c]), write(after).");
  mosa_test::check(list.code == 6 && list.out == "[a,p", "list writes [a,p, got [" + list.out + "]");
  // 項そのものの portray で止まれば、portray が書いた分だけになる。
  const Run whole = batch(k_halts, "print(boom).");
  mosa_test::check(whole.code == 6 && whole.out == "p", "whole term writes p, got [" + whole.out + "]");
  // 何も書かずに止まる portray でも、項の頭は届く。
  const Run quiet = batch("portray(boom) :- halt(6).\n", "print(f(boom)).");
  mosa_test::check(quiet.code == 6 && quiet.out == "f(", "quiet portray writes f(, got [" + quiet.out + "]");
  // 前の portray が成功して字面を差し替えた後に、後の portray が止まる。
  const Run later = batch("portray(one) :- write(two).\n" + std::string(k_halts), "print([x, one, boom]).");
  mosa_test::check(later.code == 6 && later.out == "[x,two,p", "later portray writes [x,two,p, got [" +
                                                                     later.out + "]");
  // 指令の中の print でも同じ。
  const Run directive = batch(std::string(k_halts) + ":- print(f(boom)).\n", "true.");
  mosa_test::check(directive.code == 6 && directive.out == "f(p", "directive writes f(p, got [" +
                                                                       directive.out + "]");
}

// 止まったときの portray の字面も、成功したときと同じく writer が前の字句との間の空白を決めて置く（- と + の
// 間の空白が無いと、2 つの記号は -+ という 1 つの原子に読まれる）。
void check_glue() {
  const Run done = batch("portray(boom) :- write(+).\n", "print(- boom), halt(6).");
  const Run halted = batch("portray(boom) :- write(+), halt(6).\n", "print(- boom).");
  mosa_test::check(done.code == 6 && halted.code == 6, "glue exits");
  mosa_test::check(halted.out == "- +" && halted.out == done.out,
                   "glue: halted [" + halted.out + "] matches the finished [" + done.out + "]");
}

void check_files() {
  const auto out = tmp_path("l14_set_output.txt");
  fresh(out);
  const Run set = batch(k_halts, "open('" + out + "', write, S), set_output(S), print(f(boom)), write(after).");
  mosa_test::check(set.code == 6, "set_output exit " + std::to_string(set.code));
  mosa_test::check(slurp(out) == "f(p", "set_output file holds f(p, got " + shown(out));
  mosa_test::check(set.out.empty(), "set_output writes nothing to user_output, got [" + set.out + "]");
  const auto alias = tmp_path("l14_alias.txt");
  fresh(alias);
  const Run two = batch(k_halts, "open('" + alias + "', write, _, [alias(l14)]), print(l14, g(boom)).");
  mosa_test::check(two.code == 6, "print/2 exit " + std::to_string(two.code));
  // portray の現在出力は print/2 の字面へ挟む捕獲用のメモリなので、p も別名のファイルへ行く。
  mosa_test::check(slurp(alias) == "g(p", "print/2 file holds g(p, got " + shown(alias));
  mosa_test::check(two.out.empty(), "print/2 writes nothing to user_output, got [" + two.out + "]");
}

// 入れ子の print の portray で止まると、内の print の字面が外の portray の書いたものになり、外の print へ届く。
void check_nested() {
  const Run nested = batch("portray(boom) :- !, print(g(bam)).\nportray(bam) :- write(q), halt(7).\n",
                           "print(f(boom)).");
  mosa_test::check(nested.code == 7, "nested exit " + std::to_string(nested.code));
  mosa_test::check(nested.out == "f(g(q", "nested writes f(g(q, got [" + nested.out + "]");
}

// 例外は catch で受けて続けられる。書きかけを残すと続く出力と混ざるので、今までどおり何も書かない。
void check_throw_unchanged() {
  const Run thrown = batch("portray(boom) :- write(p), throw(x).\n",
                           "catch(print(f(boom)), x, write(caught)), halt(6).");
  mosa_test::check(thrown.code == 6, "throw exit " + std::to_string(thrown.code));
  mosa_test::check(thrown.out == "caught", "throw writes nothing of print, got [" + thrown.out + "]");
}

// 埋め込みの query でも halt の前の字面は user_output へ届き、機械は使い続けられる。
void check_embedded() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(std::string(k_halts)).ok, "embedded: load portray");
  std::string text;
  {
    const CoutCapture capture;
    const auto stop = machine->query("print(f(boom)).");
    std::cout.flush();
    text = capture.text();
    mosa_test::check(stop.kind == mosa::StopKind::Halt, "embedded: print stops with halt");
  }
  mosa_test::check(text == "f(p", "embedded writes f(p, got [" + text + "]");
  const CoutCapture capture;
  mosa_test::check(machine->query("write(again).").kind == mosa::StopKind::Success, "embedded: next query runs");
  std::cout.flush();
  mosa_test::check(capture.text() == "again", "embedded: next write is visible, got [" + capture.text() + "]");
}

}  // namespace

int main() {
  check_user_output();
  check_glue();
  check_files();
  check_nested();
  check_throw_unchanged();
  check_embedded();
  return mosa_test::failures();
}
