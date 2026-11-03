// レビュー M17（ADR 0010 の改訂 3）。選択点のスタック、extras、節と call/1 の env、実行時に訳したゴールは
// ヒープの外の C++ の物だが、上限には数える。上限は 16 MiB（2^21 語）。
// 旧コードは alt と巡回する X = (X ; true) を止めず（ヒープは伸びないので RSS だけが伸び続ける）、deep と down は
// ヒープが上限に着くまで、数えない物を上限の数倍持ったまま進んだ。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t k_limit = std::size_t{16} << 17;

constexpr std::string_view k_program =
    ":- dynamic(seen/1).\n"
    // 選択点だけが伸び、ヒープは伸びない。
    "alt :- ( alt ; true ).\n"
    // 1 歩ごとに選択点を 1 つ残す。1,000 歩ごとに印を残し、止まった後にどこまで来たかを読む。
    "deep(N) :- note(N), N1 is N + 1, ( deep(N1) ; true ).\n"
    // 末尾でない再帰。歩みごとに継続の Extra と節の env が残る。
    "down(N) :- note(N), N1 is N + 1, down(N1), true.\n"
    "note(N) :- ( N mod 1000 =:= 0 -> assertz(seen(N)) ; true ).\n"
    "furthest(M) :- findall(S, seen(S), L), max_(L, 0, M).\n"
    "max_([], M, M).\n"
    "max_([X|T], M0, M) :- ( X > M0 -> max_(T, X, M) ; max_(T, M0, M) ).\n"
    // between/3 は組み込みに無い（SPEC の脇の列）。
    "btw(L, H, L) :- L =< H.\n"
    "btw(L, H, X) :- L < H, L1 is L + 1, btw(L1, H, X).\n"
    // 失敗で回るループ。節の env はバックトラックで放すので、数えた語は溜まらない。
    "spin(N) :- btw(1, N, X), work(X), fail.\n"
    "spin(_).\n"
    "work(X) :- Y is X * 2, Z is Y + 1, call(Z > 0).\n"
    "len([], 0).\n"
    "len([_|T], N) :- len(T, N0), N is N0 + 1.\n"
    // 1 つの鍵に 20,000 節。f(a, _) の選択点は 1 つで残りの候補の列（約 20,000 語）を持つ（敵対的レビュー A1）。
    ":- dynamic(f/2).\n"
    "fill :- btw(1, 20000, X), assertz(f(a, X)), fail.\n"
    "fill.\n"
    "ring(N) :- N < 2000, assertz(seen(N)), f(a, _), N1 is N + 1, ring(N1).\n"
    "ring(2000).\n"
    // 選択点を M 個積んでから切り、ヒープに K 要素の列を作る。切った分の容量は GC が返す（敵対的レビュー A3）。
    "mk(0) :- !.\n"
    "mk(N) :- N1 is N - 1, ( mk(N1) ; true ).\n"
    "ph(M, K) :- mk(M), !, length(L, K), garbage_collect, L = [_|_].\n"
    // N 段の (true ; (true ; ...))。call/1 が訳した木は 1 段 112 バイトで、最初の true ですぐ成功する（Codex P2）。
    "nest(0, true) :- !.\n"
    "nest(N, (true ; T)) :- N1 is N - 1, nest(N1, T).\n";

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

long furthest(mosa::Machine& machine) {
  const auto stop = machine.query("furthest(M).");
  if (stop.kind != mosa::StopKind::Success || stop.bindings.empty()) return -1;
  const std::string text = mosa::write_term(machine.heap(), machine.operators(), stop.bindings[0].variable, {}, {});
  return std::stol(text);
}

std::unique_ptr<mosa::Machine> loaded(std::string_view label) {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, std::string{label} + ": load");
  return machine;
}

// 止まった後は数えた状態が空のベクタの分まで戻り、同じ機械で次の問い合わせが動く。
void check_after_stop(mosa::Machine& machine, std::string_view label) {
  mosa_test::check(run(machine, "garbage_collect.").kind == mosa::StopKind::Success,
                   std::string{label} + ": the machine runs again after the stop");
  mosa_test::check(machine.heap().state() < k_limit / 8, std::string{label} + ": the stop gives the state words back");
}

// 16 MiB で deep は約 46,000 歩（1 歩約 45 語。選択点 32 語と継続の Extra 12 語が主）、down は約 72,000 歩
// （1 歩約 29 語）で止まる。旧コードは 987,000 歩と 330,000 歩まで進んだ。幅は数え方が 2 倍ずれれば外れるように取る。
void check_stops_near(std::string_view label, std::string_view goal, long low, long high) {
  auto machine = loaded(label);
  const Outcome outcome = run(*machine, goal);
  mosa_test::check(outcome.limited, std::string{label} + ": stops at the limit");
  const long reached = furthest(*machine);
  std::cout << label << ": reached " << reached << '\n';
  mosa_test::check(reached >= low && reached <= high, std::string{label} + ": stops near the counted size");
  check_after_stop(*machine, label);
}

void check_runaway(std::string_view label, std::string_view goal) {
  auto machine = loaded(label);
  const Outcome outcome = run(*machine, goal);
  mosa_test::check(outcome.limited, std::string{label} + ": the choice points reach the limit");
  check_after_stop(*machine, label);
}

// 容量が 64 要素以下のベクタは容量を返さないので、触った要素の数の最大が残る。いつ GC が走ったかで、
// 数十から数百語ずれる（GC ストレスの下で 36 語と 324 語）。env を放すときに引き忘れれば、spin(100000) は
// 10 万語を超えて残す。
constexpr std::size_t k_small_vectors = 4096;

// 数えても、失敗で回るループと深くない再帰は前と同じに終わる。env を放すときに引かなければ、spin が止まる。
void check_still_fits() {
  auto machine = loaded("fits");
  const Outcome spin = run(*machine, "spin(1000000).");
  mosa_test::check(!spin.limited && spin.kind == mosa::StopKind::Success, "fits: a failure-driven loop of 1,000,000");
  const Outcome len = run(*machine, "length(L, 20000), len(L, N), N == 20000.");
  mosa_test::check(!len.limited && len.kind == mosa::StopKind::Success, "fits: a recursion 20,000 deep");
  mosa_test::check(run(*machine, "garbage_collect.").kind == mosa::StopKind::Success, "fits: collect");
  const std::size_t settled = machine->heap().state();
  mosa_test::check(run(*machine, "spin(100000), length(L, 5000), len(L, _).").kind == mosa::StopKind::Success,
                   "fits: again");
  mosa_test::check(run(*machine, "garbage_collect.").kind == mosa::StopKind::Success, "fits: collect again");
  const std::size_t now = machine->heap().state();
  mosa_test::check(now <= settled + k_small_vectors && settled <= now + k_small_vectors,
                   "fits: every env and goal gives its words back");
}

// 候補の列は選択点の外の確保なので、Entry の大きさだけを数えると、ring は 2,000 歩を 320 MB で走り切る。
void check_candidates() {
  auto machine = loaded("bucket");
  const Outcome outcome = run(*machine, "fill, ring(0).");
  mosa_test::check(outcome.limited, "bucket: the candidate lists reach the limit");
  const long reached = furthest(*machine);
  std::cout << "bucket: reached " << reached << '\n';
  mosa_test::check(reached >= 0 && reached <= 400, "bucket: stops near the counted size");
  check_after_stop(*machine, "bucket");
}

// 入れ子の実行（with_output_to/2）の中でも、切った選択点の容量を返す。返さなければ、トップレベルで収まる
// 列が入れ子の中でだけ上限に着く。
void check_nested_give_back() {
  auto top = loaded("give back");
  const Outcome plain = run(*top, "ph(30000, 400000).");
  mosa_test::check(!plain.limited && plain.kind == mosa::StopKind::Success, "give back: fits at the top level");
  auto nested = loaded("give back nested");
  const Outcome inner = run(*nested, "with_output_to(atom(_), ph(30000, 400000)).");
  mosa_test::check(!inner.limited && inner.kind == mosa::StopKind::Success, "give back: fits inside with_output_to");
}

// 最後のゴールがすぐ成功しても、成功を返す前に上限を確かめる。確かめなければ、上限の 2 倍の木を選択点が
// 持ったまま答えを返す。
void check_success_counts() {
  auto machine = loaded("success");
  const Outcome outcome = run(*machine, "nest(300000, G), call(G).");
  mosa_test::check(outcome.limited, "success: the translated goal reaches the limit before the answer");
  check_after_stop(*machine, "success");
}

}  // namespace

int main() {
  check_still_fits();
  check_stops_near("deep", "deep(0).", 20000, 50000);
  check_stops_near("down", "down(0).", 40000, 100000);
  check_runaway("alt", "alt.");
  check_runaway("cyclic or", "X = (X ; true), call(X).");
  check_runaway("cyclic and", "X = (X, true), call(X).");
  check_candidates();
  check_nested_give_back();
  check_success_counts();
  return mosa_test::failures();
}
