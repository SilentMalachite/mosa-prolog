// レビュー 2026-11-20 の H-02（ADR 0010 の改訂 4）。単一化と標準順序の比較は、入った複合項の組をヒープの外の
// C++ の集合に覚える。a だけの巡回リストを互いに素な周期 p と p+1 で 2 つ作ると、組は p·(p+1) になり、前は
// --heap-max 16 でも p=2000 で RSS が 238 MB まで伸びて成功した。今は使っているヒープの語の数を超えた組を上限に
// 数えるので、=、==、compare/3、@<、sort/2、subsumes_term/2、unify_with_occurs_check/2 と、setof/3 の並べ替えは
// ヒープの上限で止まる。守りは、組がヒープの語の数に届かない歩み（小さな周期、同じ周期、基底のリストの L = M と
// L == M）が前と同じ答えになること。リストは 240,000 要素で、16 MiB で前から通る一番大きなもの（30 万と 33 万の間）
// から離し、組を全部数える案なら落ちる大きさ（約 185,000 を超えたら落ちる）にする。止めた後にもう一度流し、止めた
// 収集が持っていた語を返したことも見る。
// 上限は 16 MiB（2^21 語）。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t k_limit = std::size_t{16} << 17;

constexpr std::string_view k_program =
    "fill([]).\n"
    "fill([a|T]) :- fill(T).\n"
    "app([], L, L).\n"
    "app([H|T], L, [H|R]) :- app(T, L, R).\n"
    // 周期 P の a だけの巡回リスト。
    "cyc(P, X) :- length(L, P), fill(L), app(L, X, X).\n"
    "pair(P, Q, X, Y) :- cyc(P, X), cyc(Q, Y).\n"
    // 長さ N の a だけの基底のリストと、その写し。
    "two(N, L, M) :- length(L, N), fill(L), copy_term(L, M).\n";

struct Outcome {
  bool limited = false;
  mosa::StopKind kind = mosa::StopKind::Failure;
};

Outcome run(mosa::Machine& machine, std::string_view goal) {
  Outcome outcome;
  try {
    outcome.kind = machine.query(goal).kind;
  } catch (const mosa::ResourceLimit&) {
    outcome.limited = true;
  }
  return outcome;
}

// 直す前は、どれも上限を越えて組を覚えたまま答えを出した（=、==、compare/3 は成功し、@< は偽で失敗した）。
void check_stops(mosa::Machine& machine, std::string_view goal) {
  const Outcome outcome = run(machine, goal);
  mosa_test::check(outcome.limited, std::string{goal} + ": stops at the heap limit");
  // 止まった後も、同じ機械で次の問い合わせが動く。
  mosa_test::check(run(machine, "X = f(Y), Y = 1, X == f(1).").kind == mosa::StopKind::Success,
                   std::string{goal} + ": next query runs");
}

void check_same(mosa::Machine& machine, std::string_view goal, mosa::StopKind want) {
  const Outcome outcome = run(machine, goal);
  mosa_test::check(!outcome.limited && outcome.kind == want, std::string{goal} + ": same answer as before");
}

}  // namespace

int main() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "load");
  // 守りを先に流す。
  check_same(*machine, "pair(200, 201, X, Y), X = Y.", mosa::StopKind::Success);
  check_same(*machine, "pair(200, 201, X, Y), compare(=, X, Y).", mosa::StopKind::Success);
  check_same(*machine, "pair(2000, 2000, X, Y), X = Y, X == Y.", mosa::StopKind::Success);
  check_same(*machine, "two(240000, L, M), L = M.", mosa::StopKind::Success);
  check_same(*machine, "two(240000, L, M), L == M.", mosa::StopKind::Success);
  check_stops(*machine, "pair(2000, 2001, X, Y), X = Y.");
  check_stops(*machine, "pair(2000, 2001, X, Y), X == Y.");
  check_stops(*machine, "pair(2000, 2001, X, Y), compare(_, X, Y).");
  check_stops(*machine, "pair(2000, 2001, X, Y), X @< Y.");
  check_stops(*machine, "pair(2000, 2001, X, Y), sort([X, Y], _).");
  check_stops(*machine, "pair(2000, 2001, X, Y), subsumes_term(X, Y).");
  check_stops(*machine, "pair(2000, 2001, X, Y), unify_with_occurs_check(X, Y).");
  // 集めた答えを持ったまま、並べ替えの比較で止まる。
  check_stops(*machine, "pair(2000, 2001, X, Y), setof(Z, (Z = X ; Z = Y), _).");
  check_same(*machine, "two(240000, L, M), L = M.", mosa::StopKind::Success);
  return mosa_test::failures();
}
