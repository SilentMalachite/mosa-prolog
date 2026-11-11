// 管の user_input から read/1 で読む時間が項の長さに比例する範囲（4 倍の長さで 8 倍 + 30 ms）に収まること。
// 前は 1 バイト足すごとに項を頭から読み直し、時間が長さの 2 乗で伸びた（19 KB のリストで 8 秒。フェーズ 9 の
// Codex レビューを確かめて見つけた）。読み直すのは、足した字が終止符の後ろの字になりうるときと終わりだけにした。
// 形は 1 行のリスト、1 行に 1 要素のリスト、", " で区切ったリスト、深い f(…)。改行や空白ごとに読み直す直し方も落とす。
// 引用や注釈の中の ". " も終止符に見える。そこで頭から読み直すと、文の多い長い引用や注釈で 2 乗に戻った（64 KB の
// 引用で 3 秒、Muse のレビュー）。形は、長い引用のアトムと文字列、長い注釈、多くの行の注釈、引用のアトムのリスト。
// 引用符が届くたびに引用の頭から読み直す直し方は、\' や '' の多い引用で 2 乗に戻った（Muse の再レビュー）ので、その形も測る。
#include "harness.hpp"

#include "mosa/console.hpp"
#include "mosa/machine.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

namespace {

constexpr int k_runs = 2;
constexpr double k_slack_ms = 30.0;

std::string list_text(std::size_t n, std::string_view separator) {
  std::string text = "[";
  for (std::size_t i = 0; i < n; ++i) {
    if (i > 0) text += separator;
    text += std::to_string(i);
  }
  return text + "].\n";
}

std::string sentences(std::size_t n, std::string_view sentence = "ab. ") {
  std::string text;
  for (std::size_t i = 0; i < n; ++i) text += sentence;
  return text;
}

std::string deep_text(std::size_t n) {
  std::string text;
  for (std::size_t i = 0; i < n; ++i) text += "f(";
  text += "a";
  text.append(n, ')');
  return text + ".\n";
}

// input を標準入力に置き、read/1 の問い合わせだけを測る。マシンを作る時間は入れない。読み取り口に返された字も
// 読み切ってから戻す。残すと次の回の入力の頭に付く。1 回目は割り当ての始まりを含みうるので最小を取る。
double time_read(const std::string& input, std::string_view label) {
  double best = std::numeric_limits<double>::infinity();
  for (int run = 0; run < k_runs; ++run) {
    auto machine = mosa::Machine::create();
    std::stringstream feed;
    feed << input;
    auto* old = std::cin.rdbuf(feed.rdbuf());
    const auto started = std::chrono::steady_clock::now();
    const auto stop = machine->query("read(T), T \\== end_of_file.");
    const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - started;
    while (mosa::standard_input_get() != EOF) {
    }
    std::cin.rdbuf(old);
    std::cin.clear();
    mosa_test::check(stop.kind == mosa::StopKind::Success, std::string{label} + " reads a term");
    best = std::min(best, took.count());
  }
  return best;
}

template <typename Make>
void check_linear(std::string_view label, Make make, std::size_t small_n) {
  const std::string small_text = make(small_n);
  const std::string large_text = make(small_n * 4);
  const double small = time_read(small_text, label);
  const double large = time_read(large_text, label);
  std::ostringstream text;
  text << std::fixed << std::setprecision(1) << label << ": t(" << large_text.size() << " bytes) = " << large
       << " ms, t(" << small_text.size() << " bytes) = " << small << " ms, ratio " << (small > 0 ? large / small : 0.0)
       << " (limit 8 x + " << k_slack_ms << " ms)";
  // 通っても余裕が見えるよう、測った値を出す。ctest -V で読める。
  std::cout << text.str() << '\n';
  mosa_test::check(large <= 8 * small + k_slack_ms, text.str());
}

}  // namespace

int main() {
  check_linear("list on one line", [](std::size_t n) { return list_text(n, ","); }, 500);
  check_linear("list with one element a line", [](std::size_t n) { return list_text(n, ",\n"); }, 500);
  check_linear("list with spaces", [](std::size_t n) { return list_text(n, ", "); }, 500);
  check_linear("deep f(...)", deep_text, 500);
  check_linear("quoted atom of sentences", [](std::size_t n) { return "'" + sentences(n) + "'.\n"; }, 1000);
  check_linear("string of sentences", [](std::size_t n) { return "\"" + sentences(n) + "\".\n"; }, 1000);
  check_linear("quoted atom with escaped quotes", [](std::size_t n) { return "'" + sentences(n, "ab\\'. ") + "'.\n"; },
               1000);
  check_linear("quoted atom with doubled quotes", [](std::size_t n) { return "'" + sentences(n, "ab''. ") + "'.\n"; },
               1000);
  check_linear("block comment with slashes", [](std::size_t n) { return "/* " + sentences(n, "a/b. ") + "*/ x.\n"; },
               1000);
  check_linear("block comment of sentences", [](std::size_t n) { return "/* " + sentences(n) + "*/ x.\n"; }, 1000);
  check_linear("comment lines", [](std::size_t n) {
    std::string text;
    for (std::size_t i = 0; i < n; ++i) text += "% ab. cd.\n";
    return text + "x.\n";
  }, 1000);
  check_linear("list of quoted sentences", [](std::size_t n) {
    std::string text = "[";
    for (std::size_t i = 0; i < n; ++i) text += i > 0 ? ", 'ab. cd'" : "'ab. cd'";
    return text + "].\n";
  }, 500);
  return mosa_test::failures();
}
