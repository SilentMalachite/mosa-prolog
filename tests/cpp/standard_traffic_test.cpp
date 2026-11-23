// レビュー 2026-11-20 の H-26。user_output と user_error へ書いた字と、user_input から読み終えた字を Stream::bytes に
// 残さない。前は残したので、通った量だけメモリが伸び、ファイルの読みと分け合うヒープ上限のバイト予算
// （stream_bytes_used）を食って、後で開いた小さなファイルの最初の read が止まった。標準の流れは位置を持たない
// （reposition(false)）ので、残した字を見る道は無い。std::cin、std::cout、std::cerr の行き先を差し替えて流す。
// 予算は Machine::create(16384) の 128 KiB で、流す量はその 2 倍から 8 倍にする。
#include "harness.hpp"
#include "mosa/path_text.hpp"

#include "machine/internal.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::size_t k_cells = 16384;

// 標準の 3 つの流れの行き先を、作ってから壊すまで差し替える。std::cerr を差し替える間は harness の FAIL も
// 受けてしまうので、検査は壊した後でする。
class Standard {
 public:
  explicit Standard(std::string input)
      : feed_(std::move(input)),
        in_(std::cin.rdbuf(feed_.rdbuf())),
        out_(std::cout.rdbuf(out_text_.rdbuf())),
        err_(std::cerr.rdbuf(err_text_.rdbuf())) {}
  Standard(const Standard&) = delete;
  Standard& operator=(const Standard&) = delete;
  ~Standard() {
    std::cin.rdbuf(in_);
    std::cin.clear();
    std::cout.rdbuf(out_);
    std::cerr.rdbuf(err_);
  }
  [[nodiscard]] std::string out() const { return out_text_.str(); }
  [[nodiscard]] std::string err() const { return err_text_.str(); }

 private:
  std::stringstream feed_;
  std::ostringstream out_text_;
  std::ostringstream err_text_;
  std::streambuf* in_;
  std::streambuf* out_;
  std::streambuf* err_;
};

struct Run {
  bool ok = false;
  bool limited = false;
  // user_input、user_output、user_error の bytes の長さの合計。
  std::size_t kept = 0;
  std::string out;
  std::string err;
};

Run run(std::string input, std::string_view program, std::string_view goal) {
  Run result;
  auto machine = mosa::Machine::create(k_cells);
  {
    Standard standard(std::move(input));
    try {
      if (program.empty() || machine->load_program(program).ok) {
        result.ok = machine->query(goal).kind == mosa::StopKind::Success;
      }
    } catch (const mosa::ResourceLimit&) {
      result.limited = true;
    }
    result.out = standard.out();
    result.err = standard.err();
  }
  const mosa::Impl& impl = mosa::engine_impl(*machine);
  for (std::size_t index = 0; index < 3; ++index) result.kept += impl.streams[index].bytes.size();
  return result;
}

// パスは Prolog の引用の原子にして埋める。' と \ を含むパスでも読める（レビュー R-L23、ほかのテストの prolog_path と
// 同じ）。Windows の generic_string は ANSI のコードページなので、UTF-8 で書く。
std::string prolog_path(const std::filesystem::path& path) {
  std::string out = "'";
  for (char unit : mosa::utf8_from_path(path)) {
    if (unit == '\'') out += "''";
    else if (unit == '\\') out += "\\\\";
    else out.push_back(unit);
  }
  out.push_back('\'');
  return out;
}

std::string tiny_read(const std::filesystem::path& path) {
  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << "a.\n";
  }
  return "open(" + prolog_path(path) + ", read, S), read(S, T), close(S), T == a";
}

// forall/2 と between/3 はライブラリにあるので、回しは手で書く。
constexpr std::string_view k_loud =
    "loud(0) :- !.\nloud(N) :- write(user_output, xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx), "
    "nl(user_output), M is N - 1, loud(M).\n";

// user_output へ 1 MB 書いた後でも、小さなファイルの最初の read が予算の内で読める（レビューの再現）。
void check_written(const std::string& tiny) {
  const Run run_out = run("", k_loud, "loud(20000), " + tiny + ".");
  mosa_test::check(run_out.ok && !run_out.limited, "h26: read after 1 MB to user_output");
  mosa_test::check(run_out.out.size() == 20000 * 50, "h26: user_output still reaches the OS");
  mosa_test::check(run_out.kept == 0, "h26: user_output keeps no bytes, kept " + std::to_string(run_out.kept));
}

// user_error へは tab、put_char、nl、write の道で書く。どの道も bytes に置かない。
void check_error_writers(const std::string& tiny) {
  const Run run_err = run("", "",
                          "tab(user_error, 300000), put_char(user_error, x), nl(user_error), "
                          "write(user_error, abc), " + tiny + ".");
  mosa_test::check(run_err.ok && !run_err.limited, "h26: read after 300 KB to user_error");
  mosa_test::check(run_err.err.size() == 300000 + 2 + 3, "h26: user_error still reaches the OS");
  mosa_test::check(run_err.kept == 0, "h26: user_error keeps no bytes, kept " + std::to_string(run_err.kept));
}

// 守り。現在出力への書きの道も置かない。format の ~N は bytes ではなく行の中の位置（line_pos）で行の頭を見るので、
// 字を置かなくても変わらない。
void check_current_output() {
  const Run small = run("", ":- use_module(library(format)).\n",
                        "write(abc), print(def), writeq('G'), write_canonical(f(1)), put_char(x), tab(2), nl, "
                        "format(\"~Nab~N~w~n\", [cd]).");
  mosa_test::check(small.ok && small.out == "abcdef'G'f(1)x  \nab\ncd\n", "h26: current output text: " + small.out);
  mosa_test::check(small.kept == 0, "h26: current output keeps no bytes, kept " + std::to_string(small.kept));
}

// user_input から 300 KB を get_char と peek_char で読み切っても、予算の内に収まる。
void check_read_chars(const std::string& tiny) {
  const Run chars = run(std::string(300000, 'x'),
                        "drain :- peek_char(P), get_char(C), P == C, (C == end_of_file -> true ; drain).\n",
                        "drain, " + tiny + ".");
  mosa_test::check(chars.ok && !chars.limited, "h26: get_char through 300 KB of user_input");
  mosa_test::check(chars.kept == 0, "h26: user_input keeps no read chars, kept " + std::to_string(chars.kept));
}

// read/1 で 270 KB の項を読み切っても予算の内に収まる。残るのは最後の read の読んだ字だけ。
void check_read_terms(const std::string& tiny) {
  std::string input;
  while (input.size() < 270000) input += "t('" + std::string(60, 'x') + "').\n";
  const Run terms =
      run(std::move(input), "terms :- read(T), (T == end_of_file -> true ; terms).\n", "terms, " + tiny + ".");
  mosa_test::check(terms.ok && !terms.limited, "h26: read through 270 KB of user_input");
  mosa_test::check(terms.kept < 64, "h26: user_input keeps only the last read, kept " + std::to_string(terms.kept));
}

// 守り。壊れた UTF-8 を U+FFFD に置き換えて読み直す道（read_pred.cpp の deliver）は、1 回の read の中で位置を
// 持ち越すので、読み終えた字を捨てるのは read の頭だけにする。
void check_bad_utf8() {
  const Run refused = run("'a\xFF'.\nb.\n", "",
                          "catch(read(_), error(representation_error(character), _), true), read(T), T == b.");
  mosa_test::check(refused.ok, "h26: representation_error on user_input then the next term");
  const Run replaced =
      run("'a\xFF'.\nb.\n", "", "set_prolog_flag(invalid_utf8, replace), read(T), T == 'a\\xFFFD\\', read(U), U == b.");
  mosa_test::check(replaced.ok, "h26: U+FFFD on user_input then the next term");
}

}  // namespace

int main() {
  const std::filesystem::path dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp";
  std::filesystem::create_directories(dir);
  const std::string tiny = tiny_read(dir / "h26-tiny.pl");
  check_written(tiny);
  check_error_writers(tiny);
  check_current_output();
  check_read_chars(tiny);
  check_read_terms(tiny);
  check_bad_utf8();
  return mosa_test::failures();
}
