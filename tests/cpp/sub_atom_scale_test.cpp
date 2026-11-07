// レビュー M32。sub_atom/5 の 1 つの答えの時間が原子の長さに比例する範囲（4 倍の長さで 8 倍 + 30 ms）に収まること。
// 前は (Before, Length) の組をすべて先頭から数え直して試したので、Before、Length、After を与えても長さの 3 乗かかり、
// 800 字の原子の末尾の 1 字に 8.1 秒かかった。答えをすべて数え上げるときは、答えの数が長さに比例するので
// 長さの 2 乗の範囲に収まること。4 倍の長さで 2 乗は 16 倍、3 乗は 64 倍なので、その間の 32 倍 + 30 ms を上限にする。
#include "harness.hpp"

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

// as(K, A) は 2^K 個の a の原子。tail は末尾に b を足し、half は長さの半分の a の後に b を置いた原子で、
// a だけの原子のどこにも現れない（素朴に照らすと、どの位置でも半分の長さまで合ってから外れる）。
constexpr std::string_view k_program =
    "as(0, a) :- !.\n"
    "as(K, A) :- J is K - 1, as(J, H), atom_concat(H, H, A).\n"
    "tail(K, A) :- as(K, H), atom_concat(H, b, A).\n"
    "half(K, P) :- J is K - 1, as(J, H), atom_concat(H, b, P).\n"
    "last(K, A, B) :- as(K, A), atom_length(A, N), B is N - 1.\n"
    "fixed_all(K) :- last(K, A, B), sub_atom(A, B, 1, 0, a).\n"
    "length_after(K) :- as(K, A), sub_atom(A, _, 1, 0, _).\n"
    "before_after(K) :- last(K, A, B), sub_atom(A, B, _, 0, _).\n"
    "before(K) :- last(K, A, B), findall(L, sub_atom(A, B, L, _, _), [0, 1]).\n"
    "fixed_length(K) :- as(K, A), atom_length(A, N), M is N // 2, findall(B, sub_atom(A, B, M, _, _), Bs), "
    "length(Bs, C), C =:= N - M + 1.\n"
    "after(K) :- as(K, A), once(sub_atom(A, _, _, 1, S)), atom_length(A, N), atom_length(S, L), L =:= N - 1.\n"
    "sub_end(K) :- tail(K, A), findall(B, sub_atom(A, B, _, _, b), [_]).\n"
    "sub_end_after(K) :- tail(K, A), sub_atom(A, _, _, 0, b).\n"
    "sub_absent(K) :- as(K, A), half(K, P), \\+ sub_atom(A, _, _, _, P).\n"
    "alias_sub(K) :- as(K, A), J is K - 1, as(J, P), sub_atom(A, X, _, X, P), X =:= 2 ** (K - 2).\n"
    "all_matches(K) :- as(K, A), atom_length(A, N), findall(B, sub_atom(A, B, _, _, a), Bs), length(Bs, N).\n";

constexpr int k_runs = 2;
constexpr double k_slack_ms = 30.0;

// 1 回目は割り当ての始まりを含みうる。最小を取り、偶然の遅れで落ちないようにする。
double time_goal(mosa::Machine& machine, const std::string& goal) {
  double best = std::numeric_limits<double>::infinity();
  for (int run = 0; run < k_runs; ++run) {
    const auto started = std::chrono::steady_clock::now();
    const auto stop = machine.query(goal);
    const std::chrono::duration<double, std::milli> took = std::chrono::steady_clock::now() - started;
    mosa_test::check(stop.kind == mosa::StopKind::Success, goal + " succeeds");
    best = std::min(best, took.count());
  }
  return best;
}

std::string call_of(std::string_view name, int power) {
  return std::string{name} + "(" + std::to_string(power) + ").";
}

// 2^power 字と、その 4 倍の 2^(power + 2) 字で比べる。
void check_ratio(std::string_view name, int power, double limit) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_program).ok, std::string{name} + ": load");
  const double small = time_goal(*machine, call_of(name, power));
  const double large = time_goal(*machine, call_of(name, power + 2));
  std::ostringstream text;
  text << std::fixed << std::setprecision(1) << name << ": t(2^" << power + 2 << ") = " << large << " ms, t(2^"
       << power << ") = " << small << " ms, ratio " << (small > 0 ? large / small : 0.0) << " (limit " << limit
       << " x + " << k_slack_ms << " ms)";
  // 通っても余裕が見えるよう、測った値を出す。ctest -V で読める。
  std::cout << text.str() << '\n';
  mosa_test::check(large <= limit * small + k_slack_ms, text.str());
}

}  // namespace

int main() {
  check_ratio("fixed_all", 17, 8);
  check_ratio("length_after", 17, 8);
  check_ratio("before_after", 17, 8);
  check_ratio("before", 17, 8);
  check_ratio("after", 17, 8);
  check_ratio("sub_end", 17, 8);
  check_ratio("sub_end_after", 17, 8);
  check_ratio("sub_absent", 17, 8);
  check_ratio("alias_sub", 17, 8);
  // 答えの数が長さに比例する。1 つの答えが長さに比例すれば全体は 2 乗。
  check_ratio("fixed_length", 11, 32);
  check_ratio("all_matches", 11, 32);
  return mosa_test::failures();
}
