// レビュー L15。対話の答えの後の `;` の促しは 1 行で読み、行の最初の空白でない字で決める（`;` なら次の答え、他は止める）。
// read/1 が終止符を確かめるために読んで返した字（foo. の後の改行）は、先に捨てる。その先は読まない。
// 1 バイトずつ読むと、read/1 の残した改行や `;` の後の Enter が次の促しの答えになっていた。
// 標準入力から読む場面は std::cin の入力元を差し替え、パイプと同じく端末でない入力として読ませる（stdin_read_test と同じ）。
#include "harness.hpp"
#include "mosa/machine.hpp"
#include "mosa/repl.hpp"

#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// 端末の対話。REPL の行は input から読み、ゴールの read/1 は std::cin から読む。
mosa::SessionResult talk(const std::vector<std::string>& words, std::string_view input) {
  std::vector<std::string_view> args{"mosa"};
  for (const auto& word : words) args.emplace_back(word);
  return mosa::run_session(args, input, true);
}

// 端末の対話で、REPL の行もゴールの読みも同じ標準入力（std::cin）から読む。mosa を端末で動かしたときと同じ。
mosa::SessionResult talk_stdin(const std::vector<std::string>& words, std::string_view input) {
  std::stringstream feed;
  feed << input;
  auto* old = std::cin.rdbuf(feed.rdbuf());
  const mosa::SessionResult result = talk(words, "");
  std::cin.rdbuf(old);
  std::cin.clear();
  return result;
}

// parts を順に渡し、間で 1 度ずつ入力の終わりを返す。端末で行の途中を Ctrl+D で送り、もう 1 度 Ctrl+D を押してから
// 続きを打つのと同じ（終わりを見た read/1 は standard_input_clear で読み続ける）。
class PausedInput final : public std::streambuf {
 public:
  explicit PausedInput(std::vector<std::string> parts) : parts_(std::move(parts)) {}

 protected:
  int_type underflow() override {
    if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
    if (next_ >= parts_.size()) return traits_type::eof();
    if (next_ > 0 && !paused_) {
      paused_ = true;
      return traits_type::eof();
    }
    paused_ = false;
    std::string& part = parts_[next_++];
    setg(part.data(), part.data(), part.data() + part.size());
    return traits_type::to_int_type(*gptr());
  }

 private:
  std::vector<std::string> parts_;
  std::size_t next_ = 0;
  bool paused_ = false;
};

mosa::SessionResult talk_paused(std::vector<std::string> parts) {
  PausedInput feed(std::move(parts));
  auto* old = std::cin.rdbuf(&feed);
  const mosa::SessionResult result = talk({}, "");
  std::cin.rdbuf(old);
  std::cin.clear();
  return result;
}

bool has(const mosa::SessionResult& result, std::string_view text) {
  return result.out.find(text) != std::string::npos;
}

void check_has(const mosa::SessionResult& result, std::string_view text, const std::string& label) {
  mosa_test::check(has(result, text), label + ": wants [" + std::string{text} + "] in [" + result.out + "]");
}

void check_lacks(const mosa::SessionResult& result, std::string_view text, const std::string& label) {
  mosa_test::check(!has(result, text), label + ": [" + std::string{text} + "] not wanted in [" + result.out + "]");
}

// `;` と Enter は 1 回の答え。1 バイトずつ読むと Enter が次の促しを止め、3 つめの答えに届かない。
void check_semicolon_enter() {
  const auto three = talk({}, "(X = 1 ; X = 2 ; X = 3).\n;\n;\n");
  check_has(three, "X = 3.", "third answer");
  check_lacks(three, "X = 2.", "third answer");
  // 行の最初の空白でない字で決める。前の空白は飛ばし、後ろの字は読み捨てる。
  const auto spaced = talk({}, "(X = 1 ; X = 2 ; X = 3).\n  ;\n; more\n");
  check_has(spaced, "X = 3.", "spaced answers");
  // 空の行（Enter だけ）と、; でない字は止める。次の行は次の問い合わせ。
  const auto stop = talk({}, "(X = 1 ; X = 2).\n\nY = 3.\n");
  check_has(stop, "X = 1.", "Enter stops");
  check_lacks(stop, "X = 2", "Enter stops");
  check_has(stop, "Y = 3.", "Enter stops, next query");
  const auto other = talk({}, "(X = 1 ; X = 2).\nx\nY = 3.\n");
  check_lacks(other, "X = 2", "other key stops");
  check_has(other, "Y = 3.", "other key stops, next query");
}

// 端末の -g が続くとき、前のゴールの最後の `;` の後の Enter が次のゴールの最初の答えを止めない（L10 の敵対的レビュー）。
void check_goals_in_a_row() {
  const auto goals = talk({"-g", "(X = 1 ; X = 2)", "-g", "(Y = a ; Y = b)"}, ";\n;\n");
  check_has(goals, "X = 2.", "goals in a row");
  check_has(goals, "Y = b.", "goals in a row");
  check_lacks(goals, "Y = a.", "goals in a row");
}

// レビューの再現。read/1 は foo. の後の改行を残す。促しはその行の残りを捨て、次の行を答えにする。
void check_after_read() {
  const auto read = talk_stdin({}, "read(X), (Y = 1 ; Y = 2).\nfoo.\n;\n");
  check_has(read, "X = foo, Y = 2.", "after read/1");
  check_lacks(read, "Y = 1.", "after read/1");
  // 捨てるのは read/1 が返した空白だけで、その先の行は答えになる（まだ打たれていない字を待って捨てない）。
  const auto rest = talk_stdin({}, "read(X), (Y = 1 ; Y = 2).\nfoo. ;\n");
  check_has(rest, "X = foo, Y = 2.", "rest of the read line");
  // ゴールが返された改行も読み終えていれば、次の行がそのまま答えになる（いつも 1 行捨てるのではない）。
  const auto whole = talk_stdin({}, "read(X), get_char(_), (Y = 1 ; Y = 2).\nfoo.\n;\n");
  check_has(whole, "X = foo, Y = 2.", "line read to its end");
  // 端末で foo. を Ctrl+D で送り、もう 1 度 Ctrl+D で終わりを見せてから ; を打つ。返された字は無いので、; の行を
  // 捨てずに答えにする（L15 の Codex レビュー、敵対的レビュー A1）。
  const auto paused = talk_paused({"read(X), (Y = 1 ; Y = 2).\nfoo.", ";\n"});
  check_has(paused, "X = foo, Y = 2.", "reply after an end of input");
  // 答えの行が無ければ（入力の終わり）止める。
  const auto ended = talk_stdin({}, "read(X), (Y = 1 ; Y = 2).\nfoo.");
  check_has(ended, "X = foo, Y = 1.", "end of input after read/1");
  // 端末の -g も同じ促しを使う。
  const auto goal = talk_stdin({"-g", "read(X), (Y = 1 ; Y = 2)"}, "foo.\n;\n");
  check_has(goal, "X = foo, Y = 2.", "-g after read/1");
}

// mosac build --interactive の実行ファイルは run_prepared_session で同じ促しを使う。
void check_prepared_session() {
  std::stringstream feed;
  feed << "read(X), (Y = 1 ; Y = 2).\nfoo.\n;\n";
  auto* old = std::cin.rdbuf(feed.rdbuf());
  std::ostringstream out;
  {
    auto machine = mosa::Machine::create();
    (void)mosa::run_prepared_session(*machine, &out, nullptr);
  }
  std::cin.rdbuf(old);
  std::cin.clear();
  mosa_test::check(out.str().find("X = foo, Y = 2.") != std::string::npos,
                   "prepared session after read/1: [" + out.str() + "]");
}

}  // namespace

int main() {
  check_semicolon_enter();
  check_goals_in_a_row();
  check_after_read();
  check_prepared_session();
  return mosa_test::failures();
}
