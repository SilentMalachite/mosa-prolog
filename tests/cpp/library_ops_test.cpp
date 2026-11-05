// ADR 0006 の改訂 1（レビュー L19 の A1、持ち主の決定 A）。library(...) で見つけたファイルは、user の表の代わりに
// ISO の核と自分のモジュールの演算子で読む。ctest の library_ops が、tests.cmake の書くライブラリを MOSA_LIB に置く。
#include "harness.hpp"
#include "mosa/repl.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct Session {
  std::string_view input;
  std::string_view out;
  std::string_view err;
};

void check_session(const Session& item) {
  const std::string arg0 = "mosa";
  std::vector<std::string_view> args{arg0};
  const std::string input = "set_prolog_flag(message_language, en).\n" + std::string{item.input};
  auto result = mosa::run_session(args, input, true);
  const bool out_ok = item.out.empty() || result.out.find(item.out) != std::string::npos;
  const bool err_ok = item.err.empty() || result.err.find(item.err) != std::string::npos;
  mosa_test::check(out_ok && err_ok, std::string{item.input} + " -> " + result.out + result.err);
}

}  // namespace

int main() {
  const std::string plain = std::string(MOSA_SOURCE_DIR) + "/tests/tmp/library_ops/plain.pl";
  const std::string consult = "op(700, xfx, ===).\nconsult('" + plain + "').\nq(X), X = (a === b).\n";
  const std::array<Session, 14> cases{{
      // user の -> を変えてからライブラリを読む。前は uselib.pl の節が読めなかった。
      {"op(700, xfx, ->).\n:- use_module(library(uselib)).\nop(1050, xfy, ->).\npick(1, Y).\n", "Y = pos.", ""},
      // :- を消しても、ライブラリの :- module(…) は読める。利用者の指令は ?- で書く。
      {"op(0, fx, (:-)).\n?- use_module(library(uselib)).\npick(-1, Y).\n", "Y = neg.", ""},
      // 敵対的レビュー A1 の再現。前は format.pl が別の節に読まれ、文面が鍵の形に落ちた。
      {"op(1100, xfy, ->).\n:- use_module(library(format)).\nop(1050, xfy, ->).\natom_length(X, Y).\n", "",
       "Warning: goal raised exception: error(instantiation_error"},
      {"op(1100, xfy, ->).\n:- use_module(library(format)).\nop(1050, xfy, ->).\n"
       "with_output_to(atom(A), format(\"~a~w\", [ok, 1])).\n",
       "A = ok1.", ""},
      // ライブラリが user に定義した演算子は、読み終えた後の user の表に残る。
      {":- use_module(library(userops)).\nX = (a === b), write(X), nl.\n", "a===b", ""},
      {":- use_module(library(rmops)).\ncurrent_op(P, T, =..).\n", "false.", ""},
      // ライブラリの固有層の演算子は、そのファイルを読むのに使える。
      {":- use_module(library(localops)).\nt(X), functor(X, N, A).\n", "N = ~~, A = 2.", ""},
      // パスで読むファイルは今までどおり user の表で読む。
      {consult, "true.", ""},
      // ライブラリの user:op/3 は、利用者の表へもその場で入る。ISO の既定と同じ値への変更と、利用者の演算子の削除も
      // 残る。前は読み終えた核を ISO の核と見比べて写したので、どちらも消えた（L19 A1 の Codex レビュー）。
      {"op(1100, xfy, ->).\nop(700, xfx, ===).\n:- use_module(library(resetops)).\ncurrent_op(P, xfy, ->).\n",
       "P = 1050.", ""},
      {"op(1100, xfy, ->).\nop(700, xfx, ===).\n:- use_module(library(resetops)).\ncurrent_op(P, T, ===).\n", "false.",
       ""},
      // 利用者の表の後置と同じ名前の中置は、パスで読むときと同じく拒否する。前は黙って捨て、読み込みは成功した。
      {"op(300, yf, ===).\n:- use_module(library(userops)).\ncurrent_op(P, T, ===).\n", "P = 300, T = yf.",
       "permission_error(create,operator,===)"},
      // 順に入れるので、後置を消してから中置を足すのは通る。
      {"op(300, yf, ===).\n:- use_module(library(swapops)).\nX = (a === b), write(X), nl.\n", "a===b", ""},
      // 断られた読み込みは、退けた利用者の表に入れた user:op/3 も戻す。ライブラリの中で consult したファイルも同じ。
      {":- use_module(library(rejectops)).\ncurrent_op(P, T, ===).\n", "false.", ""},
      {":- use_module(library(nestops)).\ncurrent_op(P, T, ===).\n", "false.", ""},
  }};
  for (const Session& item : cases) check_session(item);
  return mosa_test::failures();
}
