// レビュー M6（ADR 0010 の改訂 2）。findall/bagof/setof が集めた答えはヒープの外に持つが、上限には数える。
// 上限は 16 MiB（2^21 語）。f(X,X,X) の答えは C++ で約 40 語と数えるので、集める途中なら約 5 万個で上限に着く。
// 組み直したリストはヒープで 1 個 6 語なので、旧コードは 1,000,000 個を集め終えてから、リストを作る途中で止まった。
#include "harness.hpp"
#include "machine/internal.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace {

constexpr std::size_t k_limit = std::size_t{16} << 17;

constexpr std::string_view k_program =
    ":- dynamic(seen/1).\n"
    // between/3 は組み込みに無い（SPEC の脇の列）。同じ順に答える。
    "btw(L, H, L) :- L =< H.\n"
    "btw(L, H, X) :- L < H, L1 is L + 1, btw(L1, H, X).\n"
    "mk(0, []) :- !.\n"
    "mk(N, [N|T]) :- N1 is N - 1, mk(N1, T).\n"
    // 決定的にゴミを作る。答え 1 つの中で GC が回らないと、ヒープは上限まで伸びる。
    "garbage(0) :- !.\n"
    "garbage(N) :- mk(1000, _), N1 is N - 1, garbage(N1).\n"
    "burn(0) :- !.\n"
    "burn(N) :- length(_, 1000), N1 is N - 1, burn(N1).\n"
    // 答えごとに使わない長さ 1000 のリストを残して、長さ 100 のリストを答える。
    "gen(N, L) :- N > 0, length(_, 1000), ( length(L, 100) ; N1 is N - 1, gen(N1, L) ).\n"
    // 1 万個ごとに印を残す。止まった後に、どこまで集めたかを読む。
    "note(X) :- ( X mod 10000 =:= 0 -> assertz(seen(X)) ; true ).\n"
    "furthest(M) :- findall(S, seen(S), L), ( L == [] -> M = 0 ; max_list_(L, M) ).\n"
    "max_list_([X], X) :- !.\n"
    "max_list_([X|T], M) :- max_list_(T, M0), ( X > M0 -> M = X ; M = M0 ).\n";

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

// 止まった後に、印の最大と答えの数を比べる。上限より前に止まれば、1,000,000 個は集まらない。
long furthest(mosa::Machine& machine) {
  const auto stop = machine.query("furthest(M), M >= 0.");
  if (stop.kind != mosa::StopKind::Success || stop.bindings.empty()) return -1;
  const std::string text = mosa::write_term(machine.heap(), machine.operators(), stop.bindings[0].variable, {}, {});
  return std::stol(text);
}

void check_stops_early(std::string_view label, std::string_view goal) {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, std::string{label} + ": load");
  const Outcome outcome = run(*machine, goal);
  mosa_test::check(outcome.limited, std::string{label} + ": the held answers reach the limit");
  // 印は 1 万個ごと。約 5 万個で止まるので、数え方が 2 倍ずれればこの幅を外れる。
  const long reached = furthest(*machine);
  mosa_test::check(reached >= 0 && reached < 1000000, std::string{label} + ": stops before collecting every answer");
  mosa_test::check(reached >= 20000 && reached <= 70000, std::string{label} + ": stops near the counted size");
  mosa_test::check(machine->heap().held() == 0, std::string{label} + ": the stop gives the held words back");
}

// 普通に終わった bagof も、組を分けた bagof も、持った語を返す。
void check_bagof_groups_released() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "groups: load");
  const Outcome outcome = run(*machine, "findall(K-L, bagof(f(X), (btw(1, 9000, X), K is X mod 3), L), G), length(G, 3).");
  mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success, "groups: three groups");
  mosa_test::check(machine->heap().held() == 0, "groups: nothing is held afterwards");
}

// 組の鍵は保存するときの写しで容量が要素数まで縮む。捨てる写し元の容量で数えると、長い鍵の組が多い bagof を
// 多く数えすぎて早く止まる（Codex レビュー 3 回目の P2）。1 MiB で、130 要素のリストを鍵にした 16 組が収まる。
void check_long_keys() {
  auto machine = mosa::Machine::create(std::size_t{1} << 17);
  mosa_test::check(machine->load_program(k_program).ok, "keys: load");
  const Outcome outcome =
      run(*machine, "findall(K-L, bagof(X, (btw(1, 16, X), mk(129, T), K = [X|T]), L), G), length(G, 16).");
  mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success, "keys: the stored key is counted");
  mosa_test::check(machine->heap().held() == 0, "keys: nothing is held afterwards");
}

// commit 済みの範囲の中で確保しても、持った語を数える（レビュー A1）。答えを 5 万個（約 1.9M 語）持ったまま、
// 1 歩で 100 万語ほどの length/2 を作る。和は上限を超えるので止まる。
void check_inside_committed() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "committed: load");
  const Outcome outcome =
      run(*machine, "findall(f(X,X,X), (btw(1, 50000, X), (X =:= 50000 -> \\+ \\+ length(_, 340000) ; true)), L).");
  mosa_test::check(outcome.limited, "committed: an allocation inside the committed range counts the held words");
  mosa_test::check(machine->heap().held() == 0, "committed: the stop gives the held words back");
}

// 持った語で広げた容量は、返した後に残さない（レビュー A2）。同じゴミを作る問い合わせの後の閾値が、収集を
// 走らせなかった機械と同じになる。
void check_capacity_after_release() {
  auto fresh = mosa::Machine::create(k_limit);
  auto used = mosa::Machine::create(k_limit);
  mosa_test::check(fresh->load_program(k_program).ok && used->load_program(k_program).ok, "capacity: load");
  const Outcome held = run(*used, "findall(f(X,X,X), (btw(1, 40000, X), (X mod 4000 =:= 0 -> garbage(300) ; true)), L).");
  mosa_test::check(!held.limited && held.kind == mosa::StopKind::Success, "capacity: findall with garbage fits");
  const Outcome a = run(*fresh, "garbage(2000).");
  const Outcome b = run(*used, "garbage(2000).");
  mosa_test::check(!a.limited && !b.limited, "capacity: garbage fits");
  mosa_test::check(mosa::engine_impl(*used).gc_threshold == mosa::engine_impl(*fresh).gc_threshold,
                   "capacity: the threshold no longer counts the released words");
}

// 前の GC が数えた生きている語は、バックトラックでヒープが縮んでも残る。それに持った語を足すと見積もりが上限を
// 超え、回収できるゴミがあっても GC の前に止まった（Codex レビュー P2）。見積もりは今のヒープの大きさで抑える。
void check_stale_live() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "stale: load");
  const Outcome outcome = run(*machine,
                              "findall(f(X,X,X), ((length(_, 400000), fail) ; btw(1, 30000, X), "
                              "(X =:= 30000 -> burn(400) ; true)), L), length(L, 30000).");
  mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success,
                   "stale: live words freed by backtracking are not counted again");
  // 縮んだ見積もりは、次の GC まで、ゴミでヒープがまた伸びても戻さない（Codex レビュー 2 回目の P2）。
  auto again = mosa::Machine::create(k_limit);
  mosa_test::check(again->load_program(k_program).ok, "stale regrow: load");
  const Outcome regrow = run(*again,
                             "findall(T, ((btw(1, 28000, X), T = f(X,X,X)) ; (length(A, 240000), garbage_collect, "
                             "A = [_|_], fail) ; gen(250, T)), L), length(L, 28250).");
  mosa_test::check(!regrow.limited && regrow.kind == mosa::StopKind::Success,
                   "stale regrow: garbage after the shrink is not counted as live");
}

// 止まった後も、上限の大半を使う収集がまた通る。持った語を返していなければ、ここで止まる。
void check_released_after_stop() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "release: load");
  (void)run(*machine, "findall(f(X,X,X), (btw(1, 1000000, X), note(X)), L).");
  const Outcome again = run(*machine, "findall(f(X,X,X), btw(1, 25000, X), L), length(L, 25000).");
  mosa_test::check(!again.limited && again.kind == mosa::StopKind::Success, "release: a later findall fits again");
}

// 例外で抜けた収集も持った語を返す。50 回くり返しても積み上がらない。
void check_released_by_throw() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "throw: load");
  const Outcome outcome = run(*machine,
                              "( btw(1, 50, _), catch(findall(f(X,X,X), (btw(1, 25000, X), "
                              "(X =:= 25000 -> throw(e) ; true)), _), e, true), fail ; true ).");
  mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success, "throw: repeated aborted findalls fit");
  mosa_test::check(machine->heap().held() == 0, "throw: nothing is held afterwards");
}

// 入れ子の収集。内側が終われば、その答えはヒープの項になり、持った語は返る。
void check_nested() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "nested: load");
  const Outcome outcome = run(*machine,
                              "findall(N, (btw(1, 40, I), findall(f(X,X,X), btw(1, 20000, X), L), "
                              "length(L, N), I > 0), Ns), length(Ns, 40).");
  mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success, "nested: inner answers are given back");
  mosa_test::check(machine->heap().held() == 0, "nested: nothing is held afterwards");
}

// 上限の 6 割ほどを持ったまま、答えの中でゴミを作る。GC が持った語を数えて早めに回れば、ヒープは上限に着かない。
// 数えなければ、最初の GC は柔らかい容量（初めは 2^20 語）で回るので、その前にヒープと持った語の和が上限を超える。
void check_gc_counts_held() {
  auto machine = mosa::Machine::create(k_limit);
  mosa_test::check(machine->load_program(k_program).ok, "gc: load");
  const Outcome outcome = run(*machine,
                              "findall(f(X,X,X), (btw(1, 30000, X), (X =:= 30000 -> garbage(400) ; true)), L), "
                              "length(L, 30000).");
  mosa_test::check(!outcome.limited && outcome.kind == mosa::StopKind::Success, "gc: garbage beside held answers is collected");
}

}  // namespace

int main() {
  check_stops_early("findall", "findall(f(X,X,X), (btw(1, 1000000, X), note(X)), L).");
  check_stops_early("bagof", "bagof(f(X,X,X), (btw(1, 1000000, X), note(X)), L).");
  check_stops_early("setof", "setof(f(X,X,X), (btw(1, 1000000, X), note(X)), L).");
  check_bagof_groups_released();
  check_long_keys();
  check_inside_committed();
  check_capacity_after_release();
  check_stale_live();
  check_released_after_stop();
  check_released_by_throw();
  check_nested();
  check_gc_counts_held();
  return mosa_test::failures();
}
