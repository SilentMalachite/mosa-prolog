// レビュー M30。assert は、深さ 2 以上で 2 度目に出会った部分項を節に入るときに 1 度だけ組む項にするので、共有の深い
// 項（深さ 40 の f(T, T)）を足しても 8 MiB のヒープで動く。前は節の IR を木に広げ、2^40 個の節点を組もうとして
// ヒープの上限を超えても止まらなかった。共有しない形（共有する制御構造と、循環に届く部分項）はまだ木に広げるが、
// 組む IR がヒープの上限を超えるところで上限に止まる（前は深さ 20 の骨組みで上限の外に 117 MB ほど組んで成功した）。
#include "harness.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/suite.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace {

// 語で数える。8 MiB。
constexpr std::size_t k_limit = std::size_t{1} << 20;

constexpr std::string_view k_program =
    "dag(0, a) :- !.\n"
    "dag(N, f(T, T)) :- M is N - 1, dag(M, T).\n"
    "shape(a, 0).\n"
    "shape(f(A, B), N) :- N > 0, A == B, M is N - 1, shape(A, M).\n"
    "skel(0, G, G) :- !.\n"
    "skel(N, G, (T, T)) :- M is N - 1, skel(M, G, T).\n"
    "over(0, C, C) :- !.\n"
    "over(N, C, f(T, T)) :- M is N - 1, over(M, C, T).\n"
    "gs(0, []) :- !.\n"
    "gs(N, [g(N)|T]) :- M is N - 1, gs(M, T).\n"
    "same(0, _, []) :- !.\n"
    "same(N, X, [X|T]) :- M is N - 1, same(M, X, T).\n"
    "wide :- gs(1000, Gs), T =.. [f|Gs], same(1000, T, Ts), H =.. [w|Ts], assertz(H),\n"
    "    functor(W, w, 1000), call(W), arg(1, W, A), arg(1000, W, B), A == B, arg(500, A, g(501)).\n"
    "upto(N, N, [N]) :- !.\n"
    "upto(I, N, [I|T]) :- J is I + 1, upto(J, N, T).\n"
    "idle :- upto(1, 30000, X),\n"
    "    assertz((idle(N) :- (N > 0 -> M is N - 1, idle(M), (N > 0 -> true ; X == X) ; true))), idle(20).\n"
    "xs(0, []) :- !.\n"
    "xs(N, [x|T]) :- M is N - 1, xs(M, T).\n"
    "miss(0) :- !.\n"
    "miss(N) :- \\+ retract(rb(a, c)), retractall(rb(a, c)), \\+ clause(rb(a, c), _), M is N - 1, miss(M).\n"
    "timed(T) :- statistics(cputime, T0), miss(1000), statistics(cputime, T1), T is T1 - T0.\n"
    "pairs(0, _, []) :- !.\n"
    "pairs(N, P, [f(T, T)|R]) :- T = g(N, P), M is N - 1, pairs(M, P, R).\n"
    "cold :- xs(64, P), pairs(10000, P, L),\n"
    "    assertz((cold(N) :- (N > 0 -> M is N - 1, cold(M), (N > 0 -> true ; L == L) ; true))),\n"
    "    cold(60).\n"
    "kept :- xs(30000, L),\n"
    "    assertz((kept(N) :- (N > 0 -> length(L, 30000), M is N - 1, kept(M), (N > 0 -> true ; L == L) ; true))),\n"
    "    kept(20).\n"
    "head_first :- assertz(rb(a, b)), timed(Small), retract(rb(a, b)),\n"
    "    xs(30000, L), assertz((rb(a, b) :- L == L)), timed(Shared), retract((rb(a, b) :- _)),\n"
    "    xs(30000, L2), assertz((rb(a, b) :- L == L2)), timed(Copy), retract((rb(a, b) :- _)),\n"
    "    Shared < 3 * Small + 0.2, Copy < 3 * Small + 0.2.\n"
    "uchain(0, _, true) :- !.\n"
    "uchain(N, T, (nonvar(U), G)) :- U = g(T), M is N - 1, uchain(M, U, G).\n"
    "fresh(0, a) :- !.\n"
    "fresh(N, g(T)) :- M is N - 1, fresh(M, T).\n"
    "fchain(0, _, true) :- !.\n"
    "fchain(N, K, (nonvar(U), G)) :- fresh(K, U), J is K + 1, M is N - 1, fchain(M, J, G).\n"
    "calls(0, _) :- !.\n"
    "calls(N, P) :- call(P), M is N - 1, calls(M, P).\n"
    "chain_ratio :- uchain(300, a, G1), assertz((sc :- G1)), fchain(300, 1, G2), assertz((fc :- G2)),\n"
    "    statistics(cputime, T0), calls(200, sc), statistics(cputime, T1), calls(200, fc), statistics(cputime, T2),\n"
    "    Shared is T1 - T0, Copy is T2 - T1, Shared < 2 * Copy + 0.05.\n";

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

std::unique_ptr<mosa::Machine> loaded() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "load");
  return machine;
}

void check_succeeds(std::string_view label, std::string_view goal) {
  auto machine = loaded();
  const Outcome outcome = run(*machine, goal);
  mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success, std::string{label} + ": succeeds");
}

void check_stops(std::string_view label, std::string_view goal) {
  auto machine = loaded();
  const Outcome outcome = run(*machine, goal);
  mosa_test::check(outcome.limited, std::string{label} + ": stops at the heap limit");
  const Outcome again = run(*machine, "true.");
  mosa_test::check(!again.limited && again.kind == mosa::StopKind::Success,
                   std::string{label} + ": the machine runs again after the stop");
}

bool write_text(const std::filesystem::path& path, std::string_view text) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return false;
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
  return static_cast<bool>(output);
}

std::string read_text(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

// M30 の Codex 敵対的レビュー 1 回目（P2）。tests.lgt の deterministic(Goal) は、利用者の節をすべて別の機械へ写してから
// Goal を判定する（lgt_judge.cpp の scratch_ok、lgt_copy.cpp）。写し先でも共有の項を保つ。前は写し先で木に広げ、深さ 40 の項の節で止まらなかった。
void check_suite_copy() {
  const auto root = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "assert-shared";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  const auto file = root / "suite" / "logtalk" / "prolog" / "predicates" / "assertz_1" / "tests.lgt";
  const std::string text =
      "test(m30_copied_dag, deterministic(X == a)) :-\n"
      "    {assertz((d(0, a) :- !)), assertz((d(N, f(T, T)) :- M is N - 1, d(M, T))),\n"
      "     d(40, T), assertz(m30_fact(T)), X = a}.\n";
  mosa_test::check(write_text(file, text), "write suite");
  const auto report = root / "report.md";
  const int code = mosa::run_iso_suite((root / "suite").string(), report.string());
  const std::string body = read_text(report);
  mosa_test::check(code == 0 && body.find("合格数: 1") != std::string::npos, "suite copy keeps sharing: " + body);
  std::filesystem::remove_all(root, error);
}

}  // namespace

int main() {
  check_succeeds("head", "dag(40, X), assertz(p(X)), p(Y), shape(Y, 40).");
  check_succeeds("asserta", "dag(40, X), asserta(p(X)), p(Y), shape(Y, 40).");
  check_succeeds("body", "dag(40, X), assertz((b(Y) :- Y = g(X))), b(g(Y)), shape(Y, 40).");
  check_succeeds("clause", "dag(40, X), assertz(c(X)), clause(c(Y), true), shape(Y, 40).");
  check_succeeds("retract", "dag(40, X), assertz(r(X)), retract(r(Y)), shape(Y, 40).");
  // 引数 1000 個の T を頭の引数に 1000 回。頭の第 1 引数と次の 1 つは写し、残りは T を指すので、IR は約 5000 節点。
  // 深さ 1 を写すと 100 万節点になり、8 MiB の予算（149796 節点）を超える。
  check_succeeds("wide", "wide.");
  // M30 の Codex レビュー 1 回目（P2）。共有の項は最初に要るときに組む。前は節に入るたびにすべて組んだので、動かない
  // 分岐の X == X の X（要素 30000 個のリスト、480 KB）を再帰の段ごとに組んで持ち、20 段で 8 MiB の上限に止まった。
  check_succeeds("idle branch", "idle.");
  // M30 の Codex 敵対的レビュー 3 回目（P2 2 件）。共有の項は env のスロットを取らず、呼び出しの間も持たない。前は
  // 共有の項 1 万個のスロットを呼び出しごとに作り（cold）、読んだ要素 30000 個のリストを継続の env が持ち続けた
  // （kept）ので、どちらも 8 MiB の上限で止まった。9f96a13 は両方とも収まる。
  check_succeeds("cold shared terms", "cold.");
  check_succeeds("read once", "kept.");
  // M30 の Codex レビュー 2 回目と Codex 敵対的レビュー 2 回目（P2）。retract/1・retractall/1・clause/2 は頭を照合して
  // から本体の共有の項を組む。前は頭の合わない節でも本体の要素 30000 個のリストを組み、1000 回で約 1 秒かかった
  // （本体 true の節は数 ms）。clause/2 は M30 より前から頭の前に本体を組んでいたので、共有しない本体（Copy）も見る。
  // 候補ごとにヒープを戻すので、上限ではなく時間の比で見る。
  check_succeeds("head first", "head_first.");
  // M30 の Codex 敵対的レビュー 5 回目（P2）。g(g(…)) の各段を共有の項にした鎖は、同じ大きさの共有しない写しと
  // 同じ程度の時間で組む。前は共有の項ごとに別の歩みを始めて組み立て用の入れ物と memo の要素を確保し、9f96a13 の
  // 写しの 4.4 倍かかった。いまは写しの 1.1 倍ほど（memo を引き、組んだ項を置く分）。
  check_succeeds("shared chain", "chain_ratio.");
  // 共有する制御構造は木に広げる。深さ 12 は収まり、深さ 20（2^20 個の葉）は組む間に上限で止まる。
  check_succeeds("small skeleton", "skel(12, true, G), assertz((q :- G)), q.");
  check_stops("shared skeleton", "skel(20, true, G), assertz((q :- G)).");
  // どの節点も循環に届くので共有せず、出現ごとに循環を切る匿名を作る。
  check_stops("dag over a cycle", "C = f(C), over(40, C, X), assertz(o(X)).");
  check_suite_copy();
  return mosa_test::failures();
}
