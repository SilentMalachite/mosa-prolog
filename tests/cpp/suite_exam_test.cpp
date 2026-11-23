// tests/pl の実行器（mosatest F）は、error(Formal) の期待を、投げられた error(F, _) の F が期待の Formal の
// 具体化であるかで見る（レビュー 2026-11-20 の H-41）。前は両向きの単一化だったので、変数や一部が変数の F も
// 合い、error/2 でない投げた項は項ごと Formal と比べたので、生の type_error(integer, a) も合った。
// error/2 でない投げた項は、5 つ目の期待 ball(Ball) で、投げた項そのものを包摂で比べる。
// solutions(L) の L は閉じたリストで、要素は Name = Value を , でつないだ組でなければならない（レビュー M-53）。前は
// リストでない尾で止まり、= でない項を捨てたので、typo は空のリストに、[X = 1|bad] は [X = 1] になり、
// (X = 1, Y == 2) は X だけを比べて通った。書き損じた期待は、どの答えにも合わないものとして落とす。
// 1 件ごとに 1 ファイルを tests/tmp/suite_exam に書き、run_suite の終了コードを見る。

#include "harness.hpp"
#include "mosa/path_text.hpp"
#include "mosa/suite.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

struct Exam {
  std::string_view name;
  std::string_view line;
  bool passes;
};

int run_file(const Exam& item, std::string& shown) {
  const std::string dir = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/suite_exam";
  std::error_code error;
  std::filesystem::create_directories(mosa::path_from_utf8(dir), error);
  const std::string path = dir + "/" + std::string{item.name} + ".pl";
  {
    std::ofstream file(mosa::path_from_utf8(path), std::ios::binary | std::ios::trunc);
    file << item.line << '\n';
  }
  std::ostringstream captured;
  auto* saved = std::cerr.rdbuf(captured.rdbuf());
  const int code = mosa::run_suite(path);
  std::cerr.rdbuf(saved);
  shown = captured.str();
  return code;
}

void check_exam(const Exam& item) {
  std::string shown;
  const int code = run_file(item, shown);
  const std::string name{item.name};
  if (item.passes) mosa_test::check(code == 0, name + ": passes\n" + shown);
  else mosa_test::check(code == 1, name + ": fails");
}

}  // namespace

int main() {
  const std::array<Exam, 24> exams{{
      // 投げた Formal が変数か一部が変数なら、期待の Formal を満たしたことにならない。
      {"h41_var", "test(t, throw(error(_, _)), error(type_error(integer, a))).", false},
      {"h41_partial", "test(t, throw(error(type_error(_, _), _)), error(type_error(integer, a))).", false},
      // error/2 でない投げた項は、error(Formal) の期待に合わない。
      {"h41_raw", "test(t, throw(type_error(integer, a)), error(type_error(integer, a))).", false},
      // 期待の変数は、投げた Formal のどの部分にも合う。Context は比べない。
      {"h41_exact", "test(t, throw(error(type_error(integer, a), ctx)), error(type_error(integer, a))).", true},
      {"h41_general", "test(t, throw(error(type_error(integer, a), _)), error(type_error(_, _))).", true},
      {"h41_minus", "test(t, throw(error(domain_error(d, -1), _)), error(domain_error(d, -1))).", true},
      // 期待に同じ変数が 2 度あれば、投げた Formal の 2 か所も同じでなければならない。
      {"h41_shared", "test(t, throw(error(f(a, b), _)), error(f(X, X))).", false},
      {"h41_ball", "test(t, throw(b), ball(b)).", true},
      {"h41_ball_general", "test(t, throw(f(1)), ball(f(_))).", true},
      {"h41_ball_other", "test(t, throw(c), ball(b)).", false},
      {"h41_ball_success", "test(t, true, ball(b)).", false},
      {"h41_ball_error", "test(t, throw(error(b, _)), ball(b)).", false},
      {"h41_ball_unbound", "test(t, throw(f(_)), ball(f(1))).", false},
      // 直す前は通った書き損じ。リストでない項、リストでない尾、閉じていない尾、= でない組。
      {"m53_not_list", "test(t, fail, solutions(typo)).", false},
      {"m53_bad_tail", "test(t, X = 1, solutions([X = 1|bad])).", false},
      {"m53_open_tail", "test(t, X = 1, solutions([X = 1|_])).", false},
      {"m53_not_pair", "test(t, (X = 1, Y = 3), solutions([(X = 1, Y == 2)])).", false},
      {"m53_not_pair_held", "test(t, (X = 1, Y = 2), solutions([(X = 1, Y == 2)])).", false},
      // 前後で同じ守り。空のリストは失敗するゴールだけに合い、組と解の数は全部比べる。
      {"m53_empty", "test(t, fail, solutions([])).", true},
      {"m53_empty_success", "test(t, true, solutions([])).", false},
      {"m53_two", "test(t, (X = 1 ; X = 2), solutions([X = 1, X = 2])).", true},
      {"m53_pairs", "test(t, (X = 1, Y = 3), solutions([(X = 1, Y = 3)])).", true},
      {"m53_pairs_differ", "test(t, (X = 1, Y = 3), solutions([(X = 1, Y = 2)])).", false},
      // solutions は 1 引数だけ。前は 2 つ目の引数を見ずに合格にした（レビュー R-M15）。
      {"rm15_arity", "test(t, X = 1, solutions([X = 1], garbage)).", false},
  }};
  for (const Exam& item : exams) check_exam(item);
  return mosa_test::failures();
}
