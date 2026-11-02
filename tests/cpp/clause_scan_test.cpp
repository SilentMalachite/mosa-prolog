// レビュー M3（ADR 0015、Ruling 7）。第1引数が未束縛の呼び出し・retract・clause/2 の走査は、次に合う節があるときだけ
// 選択点を残す。答えごとの more（REPL の true ; と true.）が今と同じで、最後の答えの後に next を呼べないこと。
// 開いた走査のために残した死んだ節は、走査が閉じた後の最初の変更が端から消すか、まとめて掃除すること。
#include "harness.hpp"

#include "machine/internal.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view k_three = ":- dynamic(p/1).\n:- dynamic(seen/1).\np(1).\np(2).\np(3).\n";
constexpr std::string_view k_five = ":- dynamic(p/1).\np(1).\np(2).\np(3).\np(4).\np(5).\n";
constexpr std::string_view k_unmatched = ":- dynamic(q/2).\nq(1, a).\nq(2, a).\nq(3, b).\n";

// 答えを尽くし、答えごとの more を返す。more が偽の答えの後も next を呼び、もう答えが無いことを確かめる。
std::vector<bool> answers(mosa::Machine& machine, const std::string& goal) {
  std::vector<bool> more;
  auto stop = machine.query(goal);
  while (stop.kind == mosa::StopKind::Success) {
    more.push_back(stop.more);
    const bool last = !stop.more;
    stop = machine.next();
    mosa_test::check(!last || stop.kind == mosa::StopKind::Failure, goal + ": no answer after a final one");
    if (last) break;
  }
  return more;
}

bool holds(mosa::Machine& machine, const std::string& goal) {
  return machine.query(goal).kind == mosa::StopKind::Success;
}

std::size_t stored(mosa::Machine& machine) {
  auto& impl = mosa::engine_impl(machine);
  const mosa::Predicate* predicate = mosa::find_pred(impl, mosa::PredKey{impl.user_module, impl.atoms.intern("p"), 1});
  return predicate == nullptr ? 0 : predicate->clauses.size();
}

// scan は X を答える走査（p(X)・retract(p(X))・clause(p(X), true)）。during は答えごとに走るゴールで、答えた X を
// seen/1 に足してから、残った seen と答えごとの more を確かめる。
void check_scan(const std::string& scan, const std::string& during, const std::vector<bool>& want_more,
                const std::string& want_seen) {
  auto machine = mosa::Machine::create();
  const std::string label = scan + ", " + during;
  mosa_test::check(machine->load_program(k_three).ok, label + ": load");
  const auto more = answers(*machine, scan + ", " + during + ", assertz(seen(X)).");
  mosa_test::check(more == want_more, label + ": more per answer");
  mosa_test::check(holds(*machine, "findall(X, seen(X), L), L == " + want_seen + "."), label + ": answers");
}

void check_determinism(const std::string& scan, bool removes) {
  // (a) 最後の答えは選択点を残さない。
  check_scan(scan, "true", {true, true, false}, "[1, 2, 3]");
  // (b) 先読みした節を答え 1 の中で消す。呼び出しと clause/2 は呼んだ時点の節を見るので 2 も答える（7.5.4）。
  // retract は消えた節を二度は消さないので 2 を飛ばす（lu_double_retract と同じ）。どれも最後の答えは選択点を
  // 残さない。
  if (removes) check_scan(scan, "(X == 1 -> retract(p(2)) ; true)", {true, false}, "[1, 3]");
  else check_scan(scan, "(X == 1 -> retract(p(2)) ; true)", {true, true, false}, "[1, 2, 3]");
  // (c) 走査の中で足した節は見ず、最後の答えに選択点を残さない。
  check_scan(scan, "(X == 1 -> assertz(p(4)) ; true)", {true, true, false}, "[1, 2, 3]");
}

// (d) 最後の節は頭が合わない。retract と clause/2 は先に単一化して確かめるので、2 つ目の答えに選択点を残さない。
// 呼び出しは第1引数の鍵だけで次の節を決めるので、今と同じく残し、やり直して失敗する。
void check_unmatched(const std::string& scan, const std::vector<bool>& want_more) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_unmatched).ok, scan + ": load");
  mosa_test::check(answers(*machine, scan + ".") == want_more, scan + ": more per answer");
}

// 開いた走査の先の端の節を消す。走査が閉じた後の最初の変更（assertz）が端から消す。
void check_end_erase(const std::string& scan) {
  auto machine = mosa::Machine::create();
  const std::string label = "end erase after " + scan;
  mosa_test::check(machine->load_program(k_three).ok, label + ": load");
  const auto stop = machine->query(scan + ", X == 1, retract(p(3)).");
  mosa_test::check(stop.kind == mosa::StopKind::Success && stop.more, label + ": scan stays open");
  mosa_test::check(stored(*machine) == 3, label + ": the open scan keeps the dead clause");
  mosa_test::check(holds(*machine, "assertz(p(4))."), label + ": assertz");
  mosa_test::check(stored(*machine) == 3, label + ": the dead clause is gone after the scan closed");
  mosa_test::check(holds(*machine, "findall(X, p(X), L), L == [1, 2, 4]."), label + ": clauses left");
}

// 開いた走査の先の真ん中の 3 節を消す。5 節の半分を超えたので、走査が閉じた後の最初の変更がまとめて掃除する。
void check_sweep(const std::string& scan) {
  auto machine = mosa::Machine::create();
  const std::string label = "sweep after " + scan;
  mosa_test::check(machine->load_program(k_five).ok, label + ": load");
  const auto stop = machine->query(scan + ", X == 1, retract(p(2)), retract(p(3)), retract(p(4)).");
  mosa_test::check(stop.kind == mosa::StopKind::Success && stop.more, label + ": scan stays open");
  mosa_test::check(stored(*machine) == 5, label + ": the open scan keeps the dead clauses");
  mosa_test::check(holds(*machine, "assertz(p(6))."), label + ": assertz");
  mosa_test::check(stored(*machine) == 3, label + ": the dead clauses are swept after the scan closed");
  mosa_test::check(holds(*machine, "findall(X, p(X), L), L == [1, 5, 6]."), label + ": clauses left");
}

}  // namespace

int main() {
  check_determinism("p(X)", false);
  check_determinism("retract(p(X))", true);
  check_determinism("clause(p(X), true)", false);
  check_unmatched("q(X, a)", {true, true});
  check_unmatched("retract(q(X, a))", {true, false});
  check_unmatched("clause(q(X, a), true)", {true, false});
  check_end_erase("p(X)");
  check_end_erase("clause(p(X), true)");
  check_sweep("p(X)");
  check_sweep("clause(p(X), true)");
  return mosa_test::failures();
}
