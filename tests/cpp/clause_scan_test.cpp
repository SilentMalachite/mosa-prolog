// レビュー M3（ADR 0015、Ruling 7）。第1引数が未束縛の呼び出し・retract・clause/2 の走査は、次に合う節があるときだけ
// 選択点を残す。答えごとの more（REPL の true ; と true.）が今と同じで、最後の答えの後に next を呼べないこと。
// 開いた走査のために残した死んだ節は、走査が閉じた後の最初の変更が端から消すか、まとめて掃除すること。
#include "harness.hpp"

#include "machine/internal.hpp"
#include "mosa/machine.hpp"
#include "mosa/path_text.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view k_three = ":- dynamic(p/1).\n:- dynamic(seen/1).\np(1).\np(2).\np(3).\n";
constexpr std::string_view k_five = ":- dynamic(p/1).\np(1).\np(2).\np(3).\np(4).\np(5).\n";
constexpr std::string_view k_static = "p(1).\np(2).\np(3).\np(4).\np(5).\n";
constexpr std::string_view k_unmatched = ":- dynamic(q/2).\nq(1, a).\nq(2, a).\nq(3, b).\n";
// レビュー M20。9 節あるので鍵の走査は索引の組を歩く。鍵 a の節の間に鍵 b の節が入る。
constexpr std::string_view k_keyed =
    ":- dynamic(r/2).\n:- dynamic(seen/1).\nr(a, 1).\nr(b, 0).\nr(a, 2).\nr(b, 0).\nr(b, 0).\nr(a, 3).\nr(b, 0).\n"
    "r(b, 0).\nr(b, 0).\n";
constexpr std::string_view k_keyed_unmatched =
    ":- dynamic(s/3).\ns(k, 1, a).\ns(j, 0, a).\ns(k, 2, a).\ns(j, 0, a).\ns(j, 0, a).\ns(k, 3, b).\ns(j, 0, a).\n"
    "s(j, 0, a).\ns(j, 0, a).\n";

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

std::size_t stored(mosa::Machine& machine, std::string_view name = "p", std::uint16_t arity = 1) {
  auto& impl = mosa::engine_impl(machine);
  const mosa::Predicate* predicate =
      mosa::find_pred(impl, mosa::PredKey{impl.user_module, impl.atoms.intern(name), arity});
  return predicate == nullptr ? 0 : predicate->clauses.size();
}

// scan は X を答える走査（p(X)・retract(p(X))・clause(p(X), true)）。during は答えごとに走るゴールで、答えた X を
// seen/1 に足してから、残った seen と答えごとの more を確かめる。
void check_scan(const std::string& scan, const std::string& during, const std::vector<bool>& want_more,
                const std::string& want_seen, std::string_view program = k_three) {
  auto machine = mosa::Machine::create();
  const std::string label = scan + ", " + during;
  mosa_test::check(machine->load_program(program).ok, label + ": load");
  const auto more = answers(*machine, scan + ", " + during + ", assertz(seen(X)).");
  mosa_test::check(more == want_more, label + ": more per answer");
  mosa_test::check(holds(*machine, "findall(X, seen(X), L), L == " + want_seen + "."), label + ": answers");
}

// pred は scan の述語の名前と、X より前の引数（"p(" か "r(a, "）。
void check_determinism(const std::string& scan, bool removes, std::string_view program = k_three,
                       const std::string& pred = "p(") {
  // (a) 最後の答えは選択点を残さない。
  check_scan(scan, "true", {true, true, false}, "[1, 2, 3]", program);
  // (b) 先読みした節を答え 1 の中で消す。呼び出しと clause/2 は呼んだ時点の節を見るので 2 も答える（7.5.4）。
  // retract は消えた節を二度は消さないので 2 を飛ばす（lu_double_retract と同じ）。どれも最後の答えは選択点を
  // 残さない。
  const std::string drop = "(X == 1 -> retract(" + pred + "2)) ; true)";
  if (removes) check_scan(scan, drop, {true, false}, "[1, 3]", program);
  else check_scan(scan, drop, {true, true, false}, "[1, 2, 3]", program);
  // (c) 走査の中で足した節は見ず、最後の答えに選択点を残さない。
  check_scan(scan, "(X == 1 -> assertz(" + pred + "4)) ; true)", {true, true, false}, "[1, 2, 3]", program);
}

// (d) 最後の節は頭が合わない。retract と clause/2 は先に単一化して確かめるので、2 つ目の答えに選択点を残さない。
// 呼び出しは第1引数の鍵だけで次の節を決めるので、今と同じく残し、やり直して失敗する。
void check_unmatched(const std::string& scan, const std::vector<bool>& want_more,
                     std::string_view program = k_unmatched) {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(program).ok, scan + ": load");
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

// Codex レビュー P2。開いた走査の中で 3 節を消してから足すと、掃除が走るが 3 節とも走査が届くので残る。残した節も
// 死んだ節として数え続けるので、走査が閉じた後に真ん中の 1 節を消すと半分を超え、次の変更がまとめて掃除する。
// 両端は生きているので端からは消えない。数えなければ、残した節は次に半分の節を消すまで残る。
void check_held_sweep(const std::string& scan) {
  auto machine = mosa::Machine::create();
  const std::string label = "held sweep after " + scan;
  mosa_test::check(machine->load_program(k_five).ok, label + ": load");
  const auto stop =
      machine->query(scan + ", X == 1, retract(p(2)), retract(p(3)), retract(p(4)), assertz(p(6)).");
  mosa_test::check(stop.kind == mosa::StopKind::Success && stop.more, label + ": scan stays open");
  mosa_test::check(stored(*machine) == 6, label + ": the sweep keeps the clauses the open scan reaches");
  mosa_test::check(holds(*machine, "retract(p(5))."), label + ": retract after the scan closed");
  mosa_test::check(holds(*machine, "assertz(p(7))."), label + ": assertz");
  mosa_test::check(stored(*machine) == 3, label + ": the kept clauses count towards the next sweep");
  mosa_test::check(holds(*machine, "findall(X, p(X), L), L == [1, 6, 7]."), label + ": clauses left");
}

// M20 の敵対的レビュー A7。鍵 a の開いた走査は鍵 b の節に届かない。走査の先の b の 6 節を消すと、次の変更の前の
// 掃除は走査が開いたままでも b の節を消し、走査が届く a の死んだ節だけを残す。鍵を見ずに残すと 10 節が残る。
void check_keyed_sweep(const std::string& scan) {
  auto machine = mosa::Machine::create();
  const std::string label = "keyed sweep after " + scan;
  mosa_test::check(machine->load_program(k_keyed).ok, label + ": load");
  const std::string during =
      "assertz(seen(X)), (X == 1 -> (retract(r(b, 0)), fail ; true), retract(r(a, 2)), assertz(r(c, 9)) ; true)";
  auto stop = machine->query(scan + ", " + during + ".");
  mosa_test::check(stop.kind == mosa::StopKind::Success && stop.more, label + ": scan stays open");
  mosa_test::check(stored(*machine, "r", 2) == 4, label + ": only the dead clause the scan reaches is kept");
  while (stop.kind == mosa::StopKind::Success && stop.more) stop = machine->next();
  mosa_test::check(holds(*machine, "findall(X, seen(X), L), L == [1, 2, 3]."), label + ": the scan sees r(a, 2)");
  mosa_test::check(holds(*machine, "findall(X-Y, r(X, Y), L), L == [a-1, a-3, c-9]."), label + ": clauses left");
}

// M19 の敵対的レビュー B2。脇に置いた走査の下で静的な述語を読み直すと、走査が届く古い節は死んだ節で残る。静的な
// 述語は変更されないので、走査が閉じた後の最初の呼び出しが消す。消さないと次に読み直すまで残り、束縛しない呼び出しが
// 毎回それを歩く。
std::string scan_file(std::string_view name, std::string_view text) {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "clause_scan";
  std::filesystem::create_directories(dir);
  const auto path = dir / mosa::path_from_utf8(name);
  std::ofstream{path, std::ios::binary} << text;
  return mosa::utf8_from_path(path);
}

void check_static_reload() {
  auto machine = mosa::Machine::create();
  const auto renewed = scan_file("renewed.pl", "p(10).\n");
  const auto init = scan_file("init.pl", ":- initialization(consult('" + renewed + "')).\n");
  mosa_test::check(machine->load_program(k_static).ok, "static reload: load");
  const std::string scan = "findall(X, (p(X), (X =:= 1 -> consult('" + init + "') ; true)), L), L == [1, 2, 3, 4, 5].";
  mosa_test::check(holds(*machine, scan), "static reload: the parked scan keeps the replaced clauses");
  mosa_test::check(stored(*machine) == 5, "static reload: the dead clauses wait for a call");
  mosa_test::check(holds(*machine, "findall(X, p(X), L), L == [10]."), "static reload: the new clause");
  mosa_test::check(stored(*machine) == 1, "static reload: the first call after the scan closed sweeps");
}

}  // namespace

int main() {
  check_determinism("p(X)", false);
  check_determinism("retract(p(X))", true);
  check_determinism("clause(p(X), true)", false);
  check_unmatched("q(X, a)", {true, true});
  check_unmatched("retract(q(X, a))", {true, false});
  check_unmatched("clause(q(X, a), true)", {true, false});
  // レビュー M20。鍵の組を列にせずに歩いても、答えごとの more は列を作っていたときと同じ。
  check_determinism("r(a, X)", false, k_keyed, "r(a, ");
  check_determinism("retract(r(a, X))", true, k_keyed, "r(a, ");
  check_determinism("clause(r(a, X), true)", false, k_keyed, "r(a, ");
  check_unmatched("s(k, X, a)", {true, true}, k_keyed_unmatched);
  check_unmatched("retract(s(k, X, a))", {true, false}, k_keyed_unmatched);
  check_unmatched("clause(s(k, X, a), true)", {true, false}, k_keyed_unmatched);
  check_end_erase("p(X)");
  check_end_erase("clause(p(X), true)");
  check_sweep("p(X)");
  check_sweep("clause(p(X), true)");
  check_held_sweep("p(X)");
  check_held_sweep("clause(p(X), true)");
  check_keyed_sweep("r(a, X)");
  check_keyed_sweep("clause(r(a, X), true)");
  check_static_reload();
  return mosa_test::failures();
}
