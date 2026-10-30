// mark-and-compact の GC（ADR 0010、計画 Task 4）。ヒープ・extras・トレイルが頭打ちになり、生きている項が
// 値も形も変わらずに残ること。特に断らない限り上限は 2^16 語で、GC が無ければ loop は heap_limit で止まる。
#include "harness.hpp"

#include "machine/gc.hpp"
#include "machine/internal.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::size_t k_small = std::size_t{1} << 16;

constexpr std::string_view k_loop = "loop(0) :- !. loop(N) :- N1 is N - 1, loop(N1).";
constexpr std::string_view k_loop_three = "loop(0) :- !. loop(N) :- N > 0, N1 is N - 1, loop(N1).";
constexpr std::string_view k_tak =
    "tak(X, Y, Z, A) :- X =< Y, !, Z = A.\n"
    "tak(X, Y, Z, A) :- X1 is X - 1, Y1 is Y - 1, Z1 is Z - 1,\n"
    "  tak(X1, Y, Z, A1), tak(Y1, Z, X, A2), tak(Z1, X, Y, A3), tak(A1, A2, A3, A).";
constexpr std::string_view k_make_list = "mk(0, []) :- !. mk(N, [N|T]) :- N1 is N - 1, mk(N1, T).";
constexpr std::string_view k_member = "member(X, [X|_]). member(X, [_|T]) :- member(X, T).";

// 例外を通さずに問い合わせる。ResourceLimit は失敗として数える（GC が無ければここへ来る）。
mosa::Stop ask(mosa::Machine& machine, std::string_view goal, bool& limited) {
  limited = false;
  try {
    return machine.query(goal);
  } catch (const mosa::ResourceLimit&) {
    limited = true;
    return mosa::Stop{};
  }
}

std::string binding_text(mosa::Machine& machine, const mosa::Stop& stop, std::size_t index) {
  if (index >= stop.bindings.size()) return {};
  return mosa::write_term(machine.heap(), machine.operators(), stop.bindings[index].variable, {}, {});
}

void check_loop(std::string_view label, std::string_view program) {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(program).ok, std::string{label} + ": load");
  auto& impl = mosa::engine_impl(*machine);
  const auto capacity = impl.gc_capacity;
  bool limited = false;
  const auto stop = ask(*machine, "loop(1000000).", limited);
  mosa_test::check(!limited, std::string{label} + ": loop does not hit heap_limit");
  mosa_test::check(stop.kind == mosa::StopKind::Success, std::string{label} + ": loop succeeds");
  mosa_test::check(impl.gc_stats.count >= 1, std::string{label} + ": GC ran");
  mosa_test::check(impl.gc_capacity == capacity, std::string{label} + ": soft capacity unchanged");
  mosa_test::check(impl.extras.size() < 1000, std::string{label} + ": extras plateau");
  mosa_test::check(impl.trail.slots.size() < 1000, std::string{label} + ": trail plateau");
}

void check_tak() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_tak).ok, "tak: load");
  bool limited = false;
  const auto stop = ask(*machine, "tak(18, 12, 6, A).", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "tak(18,12,6,A) succeeds");
  mosa_test::check(binding_text(*machine, stop, 0) == "7", "tak(18,12,6,A) gives 7");
}

void check_cyclic() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_loop).ok, "cyclic: load");
  bool limited = false;
  const auto stop = ask(*machine, "X = f(X), loop(200000), X = f(Y), Y == X.", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "cyclic term survives GC");
}

void check_long_live_list() {
  auto machine = mosa::Machine::create();
  std::string program{k_make_list};
  program += ' ';
  program += k_loop;
  mosa_test::check(machine->load_program(program).ok, "long list: load");
  bool limited = false;
  const auto stop = ask(*machine, "mk(2000000, L), loop(1000000), length(L, N), N == 2000000.", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "2,000,000 element list survives GC");
  mosa_test::check(mosa::engine_impl(*machine).gc_stats.count >= 1, "long list: GC ran");
}

// 1 歩で C の 1.4 倍を超える生きたデータを作っても、GC は 1 回で済む（C を混まなくなるまで一度に倍にする）。
// length/2 は 150 万要素（300 万語）を 1 歩で作る。C を 1 回に 1 度しか倍にしないと、2M・4M・8M と 3 回続けて回る。
void check_one_gc_after_large_step() {
  auto machine = mosa::Machine::create();
  mosa_test::check(machine->load_program(k_loop).ok, "large step: load");
  auto& impl = mosa::engine_impl(*machine);
  const auto before = impl.gc_stats.count;
  bool limited = false;
  const auto stop = ask(*machine, "length(L, 1500000), loop(1000), L = [_|_].", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "large step: query succeeds");
  // ストレスでは安全点ごとに GC が回るので、回数は比べない。
  if (impl.gc_stress == 0) {
    mosa_test::check(impl.gc_stats.count - before == 1,
                     "large step: exactly one GC (got " + std::to_string(impl.gc_stats.count - before) + ")");
  }
}

void check_ball() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_loop).ok, "ball: load");
  bool limited = false;
  const auto stop = ask(*machine,
                        "catch((T = g([1,2,3], \"abc\", 1.5e300, 4611686018427387904), loop(200000), throw(T)), "
                        "B, true), B == g([1,2,3], \"abc\", 1.5e300, 4611686018427387904).",
                        limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "ball keeps floats and boxed integers");
}

void check_redo() {
  auto machine = mosa::Machine::create(k_small);
  std::string program{k_member};
  program += ' ';
  program += k_loop;
  mosa_test::check(machine->load_program(program).ok, "redo: load");
  bool limited = false;
  auto stop = ask(*machine, "member(X, [a,b,c]), loop(200000).", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success && binding_text(*machine, stop, 0) == "a",
                   "redo: first answer a");
  for (const std::string_view expected : {"b", "c"}) {
    try {
      stop = machine->next();
    } catch (const mosa::ResourceLimit&) {
      stop = mosa::Stop{};
    }
    mosa_test::check(stop.kind == mosa::StopKind::Success && binding_text(*machine, stop, 0) == expected,
                     "redo: next answer " + std::string{expected});
  }
  try {
    stop = machine->next();
  } catch (const mosa::ResourceLimit&) {
    stop = mosa::Stop{mosa::StopKind::Exception, 0, {}, {}, false};
  }
  mosa_test::check(stop.kind == mosa::StopKind::Failure, "redo: then no more");
}

void check_tiny_limit() {
  try {
    auto tiny = mosa::Machine::create(8);
    auto boom = tiny->query("catch(p(a), _, true).");
    mosa_test::check(boom.kind != mosa::StopKind::Success && boom.kind != mosa::StopKind::Exception,
                     "tiny limit stops without a prolog ball");
  } catch (const mosa::ResourceLimit&) {
    mosa_test::check(true, "tiny limit stops without a prolog ball");
  }
}

void check_direct_collect() {
  auto machine = mosa::Machine::create(k_small);
  auto& impl = mosa::engine_impl(*machine);
  const auto stop = machine->query("X = f(Y, [1|Z], 2.5).");
  mosa_test::check(stop.kind == mosa::StopKind::Success, "direct: query succeeds");
  const auto before_size = machine->heap().size();
  const auto before_text = binding_text(*machine, stop, 0);
  mosa::collect_garbage(impl);
  mosa_test::check(machine->heap().size() <= before_size, "direct: heap does not grow");
  // 呼ぶ前の Stop の束縛は古い番地なので、GC が書き換えた機械の束縛から読む。
  mosa::Stop after;
  after.bindings = impl.bindings;
  mosa_test::check(binding_text(*machine, after, 0) == before_text, "direct: binding reads the same");
  mosa_test::check(before_text.starts_with("f(") && before_text.find("2.5") != std::string::npos,
                   "direct: binding is the structure");
}

// 入れ子の run_loop（計画 Task 5）。findall・bagof・読み込みの指示・with_output_to の中でも GC が回ること。
constexpr std::string_view k_between =
    "between(L, H, L) :- L =< H. between(L, H, X) :- L < H, L1 is L + 1, between(L1, H, X).";

std::string with_loop(std::initializer_list<std::string_view> parts) {
  std::string program{k_loop};
  for (const std::string_view part : parts) {
    program += ' ';
    program += part;
  }
  return program;
}

void check_findall() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(with_loop({k_between})).ok, "findall: load");
  bool limited = false;
  const auto stop = ask(*machine, "findall(N, (between(1, 3, N), loop(200000)), L), L == [1,2,3].", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "findall: L == [1,2,3]");
  mosa_test::check(mosa::engine_impl(*machine).gc_stats.count >= 1, "findall: GC ran");
}

// 変数を出現順に v(0), v(1), ... へ束縛する。番地で決まる _G の名前を比べないため。
constexpr std::string_view k_name_vars =
    "nv(T, N0, N) :- var(T), !, T = v(N0), N is N0 + 1. nv(T, N0, N) :- T =.. [_|As], nvl(As, N0, N).\n"
    "nvl([], N, N). nvl([A|As], N0, N) :- nv(A, N0, N1), nvl(As, N1, N).";

// 解をすべて next() で集め、束縛 T の字面を解ごとに並べる。
std::string all_answers(mosa::Machine& machine, std::string_view goal, bool& limited) {
  std::string text;
  auto stop = ask(machine, goal, limited);
  while (!limited && stop.kind == mosa::StopKind::Success) {
    for (std::size_t index = 0; index < stop.bindings.size(); ++index) {
      if (stop.bindings[index].name == "T") text += binding_text(machine, stop, index) + ";";
    }
    if (!stop.more) break;
    try {
      stop = machine.next();
    } catch (const mosa::ResourceLimit&) {
      limited = true;
    }
  }
  return text;
}

// GC の無い解（loop を外した同じゴール）と同じ並びになること。自由変数の扱いも比べる。
void check_bagof_case(std::string_view label, std::string_view goal, std::string_view shown) {
  const std::string program = with_loop({k_member, k_name_vars});
  const std::string tail = std::string{", copy_term("} + std::string{shown} + ", T), nv(T, 0, _).";
  const std::string with = "bagof(" + std::string{goal} + ", loop(100000)), L)" + tail;
  const std::string without = "bagof(" + std::string{goal} + ", true), L)" + tail;
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(program).ok, std::string{label} + ": load");
  auto plain = mosa::Machine::create(k_small);
  mosa_test::check(plain->load_program(program).ok, std::string{label} + ": load plain");
  bool limited = false;
  const std::string expected = all_answers(*plain, without, limited);
  const std::string actual = all_answers(*machine, with, limited);
  mosa_test::check(!limited && !expected.empty(), std::string{label} + ": does not hit heap_limit");
  mosa_test::check(actual == expected, std::string{label} + ": " + actual + " == " + expected);
  mosa_test::check(mosa::engine_impl(*machine).gc_stats.count >= 1, std::string{label} + ": GC ran");
}

void check_bagof() {
  check_bagof_case("bagof", "X-Y, (member(X-Y, [a-1,b-2,a-3])", "L");
  check_bagof_case("bagof free", "Y, (member(X-Y, [a-1,b-2,a-3])", "X-L");
  check_bagof_case("bagof alias", "Y, (member(X-Y, [a-1,b-2,Z-3])", "X-Z-L");
  check_bagof_case("bagof caret", "Y, X^(member(X-Y, [b-2,a-1,Z-3])", "Z-L");
  // ゴールが作った変数は解ごとに別の変数（VarIds::base を水位として写すこと）。
  check_bagof_case("bagof goal vars", "Y, (member(X, [a,b,a]), copy_term(g(_, _), Y)", "X-L");
}

// 証人がゴールの作った新しい変数を含む項 f(_) に結び付く解は、解ごとに名前を付け直して 1 つの組になる（8.10.2.4 の bagof の
// 自由変数の具体化は変種で比べる）。gv/2 の枠は base の位置にあり、loop の間に死ぬ。VarIds::base を語の生死で
// 写すと（SIZE_MAX）、新しい変数が呼ぶ前からあった変数として数えられ、組が 2 つに割れる。
void check_bagof_fresh_witness() {
  const std::string program = with_loop(
      {k_member, k_name_vars,
       "gv(X, Y) :- loop(100000), member(Y, [1,2]), copy_term(f(_), X).",
       "gp(X, Y) :- member(Y, [1,2]), copy_term(f(_), X)."});
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(program).ok, "bagof fresh witness: load");
  auto plain = mosa::Machine::create(k_small);
  mosa_test::check(plain->load_program(program).ok, "bagof fresh witness: load plain");
  bool limited = false;
  const std::string expected = all_answers(*plain, "bagof(Y, gp(X, Y), L), copy_term(X-L, T), nv(T, 0, _).", limited);
  const std::string actual = all_answers(*machine, "bagof(Y, gv(X, Y), L), copy_term(X-L, T), nv(T, 0, _).", limited);
  mosa_test::check(!limited && expected == "f(v(0))-[1,2];", "bagof fresh witness: no-GC answer is one group: " + expected);
  mosa_test::check(actual == expected, "bagof fresh witness: " + actual + " == " + expected);
  mosa_test::check(mosa::engine_impl(*machine).gc_stats.count >= 1, "bagof fresh witness: GC ran");
}

void check_directive_case(std::string_view label, std::string_view directive) {
  auto machine = mosa::Machine::create(k_small);
  bool loaded = false;
  try {
    loaded = machine->load_program(with_loop({directive, "p(ok)."})).ok;
  } catch (const mosa::ResourceLimit&) {
    loaded = false;
  }
  mosa_test::check(loaded, std::string{label} + ": load succeeds");
  bool limited = false;
  const auto stop = ask(*machine, "p(X).", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success && binding_text(*machine, stop, 0) == "ok",
                   std::string{label} + ": later clause p(ok) is usable");
  mosa_test::check(mosa::engine_impl(*machine).gc_stats.count >= 1, std::string{label} + ": GC ran");
}

void check_directive() {
  check_directive_case("directive", ":- loop(300000).");
  check_directive_case("initialization", ":- initialization(loop(300000)).");
}

void check_with_output_to() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_loop).ok, "with_output_to: load");
  bool limited = false;
  const auto stop = ask(*machine, "with_output_to(atom(A), (loop(200000), write(done))), A == done.", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "with_output_to: A == done");
  mosa_test::check(mosa::engine_impl(*machine).gc_stats.count >= 1, "with_output_to: GC ran");
}

// 実行中の consult（計画 Task 5 の見直し）。外側に選択点・選択点より古いセルのトレイル・if-then-else の extras が
// ある状態で consult の指示が GC を起こす。退避した水位と goal.cont が写されていなければ、後の解が崩れる。
std::string consulted_path(std::string_view name, std::string_view text) {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "gc_consult";
  std::filesystem::create_directories(dir);
  const auto file = dir / name;
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  out << text;
  return file.generic_string();
}

std::string consult_goal(std::string_view shape, const std::string& file) {
  const std::string inner = "X = w(_), member(A, [x,y,z]), X = w(Q), var(Q), Q = A, (A == y -> consult('" + file +
                            "') ; true)";
  if (shape == "findall") return "findall(A, (" + inner + "), L), L == [x,y,z], q(done).";
  return "(" + inner + ", assertz(seen(A)), fail ; true), findall(S, seen(S), L), L == [x,y,z], q(done).";
}

void check_consult_case(std::string_view shape) {
  const std::string label = "consult " + std::string{shape};
  const std::string program = with_loop({k_member, ":- dynamic(seen/1)."});
  const std::string heavy = consulted_path("heavy.pl", ":- loop(300000).\nq(done).\n");
  const std::string light = consulted_path("light.pl", "q(done).\n");
  // GC の無い答え。同じゴールが指示の loop なしで成功することを先に確かめる。
  auto plain = mosa::Machine::create(k_small);
  mosa_test::check(plain->load_program(program).ok, label + ": load plain");
  bool limited = false;
  const auto expected = ask(*plain, consult_goal(shape, light), limited);
  mosa_test::check(!limited && expected.kind == mosa::StopKind::Success, label + ": succeeds without GC");
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(program).ok, label + ": load");
  auto& impl = mosa::engine_impl(*machine);
  const auto before = impl.gc_stats.count;
  const auto stop = ask(*machine, consult_goal(shape, heavy), limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success,
                   label + ": later answers see undone bindings and give [x,y,z]");
  mosa_test::check(impl.gc_stats.count > before, label + ": GC ran inside the consult");
}

void check_consult() {
  check_consult_case("fail");
  check_consult_case("findall");
}

// 退避した extras と trail の水位も GC が写すこと。外側に死んだ extras（済んだ if-then-else）と死んだセルを指す
// トレイル（t/0 の V）を作ってから consult する。指示が古いセル Y を束縛すると、正しく写した水位なら読み込みの後で
// 戻され、束縛しない指示と同じ大きさに揃う。写さない水位は大きすぎ、指示のトレイルと extras が残る。
std::pair<std::size_t, std::size_t> sizes_after_consult(std::string_view name, std::string_view directive) {
  const std::string program =
      with_loop({k_member, "t :- V = v(_), member(_, [1,2]), V = v(1), !."});
  const std::string file = consulted_path(name, std::string{directive} + "\nq(done).\n");
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(program).ok, std::string{name} + ": load");
  bool limited = false;
  const auto stop = ask(*machine, "member(A, [x,y,z]), t, (true -> true ; true), A == y, consult('" + file +
                                      "'), q(done).", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, std::string{name} + ": query succeeds");
  const auto& impl = mosa::engine_impl(*machine);
  mosa_test::check(impl.gc_stats.count >= 1, std::string{name} + ": GC ran");
  return {impl.trail.slots.size(), impl.extras.size()};
}

void check_consult_marks() {
  const auto quiet = sizes_after_consult("quiet.pl", ":- loop(300000).");
  const auto binding = sizes_after_consult("binding.pl", ":- X = f(Y), Y = 1, loop(300000), X == f(1), (true -> true ; true).");
  mosa_test::check(binding.first == quiet.first, "consult marks: directive trail is undone (" +
                                                     std::to_string(binding.first) + " vs " + std::to_string(quiet.first) + ")");
  mosa_test::check(binding.second == quiet.second, "consult marks: directive extras are dropped (" +
                                                       std::to_string(binding.second) + " vs " + std::to_string(quiet.second) + ")");
}

// portray/1 の中は GC しない（apply_portray の GcInhibit、TODO(iso) 7.10.5）。loop(3000) はヒープを閾値（2^15 語）
// より上へ伸ばし、上限（2^16 語）には届かない。print が最後のゴールなので、外側の安全点も通らない。
void check_portray_inhibited() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(with_loop({"portray(foo) :- loop(3000), write(bar)."})).ok,
                   "portray: load");
  auto& impl = mosa::engine_impl(*machine);
  const auto before = impl.gc_stats.count;
  bool limited = false;
  const auto stop = ask(*machine, "with_output_to(atom(A), print(foo)).", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success && binding_text(*machine, stop, 0) == "bar",
                   "portray: print(foo) writes bar");
  mosa_test::check(machine->heap().size() >= impl.gc_threshold, "portray: heap passed the threshold");
  // ストレスでは print の前の安全点でも GC が回るので、回数は比べない。
  if (impl.gc_stress == 0) mosa_test::check(impl.gc_stats.count == before, "portray: no GC during the hook");
}

// 標準エラーの流れを一時的に差し替え、例外で抜けても元へ戻す。
class CerrRedirect {
 public:
  explicit CerrRedirect(std::ostringstream& sink) : saved_(std::cerr.rdbuf(sink.rdbuf())) {}
  ~CerrRedirect() { std::cerr.rdbuf(saved_); }
  CerrRedirect(const CerrRedirect&) = delete;
  CerrRedirect& operator=(const CerrRedirect&) = delete;

 private:
  std::streambuf* saved_;
};

// 標準エラーを奪って goal を問い合わせ、出た文字列を返す。成功しなければ ok を偽にする。
std::string ask_capturing_stderr(mosa::Machine& machine, std::string_view goal, bool& ok) {
  bool limited = false;
  std::ostringstream captured;
  mosa::Stop stop;
  {
    const CerrRedirect redirect(captured);
    stop = ask(machine, goal, limited);
  }
  ok = !limited && stop.kind == mosa::StopKind::Success;
  return captured.str();
}

// 旗 trace_gc が true なら、GC のたびに標準エラーへ GC を含む 1 行が出る。偽に戻せば止まる。
void check_trace_line() {
  auto machine = mosa::Machine::create(k_small);
  const auto& impl = mosa::engine_impl(*machine);
  bool ok = false;
  const std::string quiet = ask_capturing_stderr(*machine, "garbage_collect.", ok);
  mosa_test::check(ok && quiet.empty(), "trace: silent by default");
  const std::string logged =
      ask_capturing_stderr(*machine, "set_prolog_flag(trace_gc, true), garbage_collect.", ok);
  mosa_test::check(ok, "trace: set true and collect succeeds");
  mosa_test::check(logged.find("GC") != std::string::npos, "trace: a GC line on stderr");
  // ストレスでは安全点ごとに GC が回るので、行の数は固定しない。
  if (impl.gc_stress == 0) {
    mosa_test::check(std::count(logged.begin(), logged.end(), '\n') == 1, "trace: exactly one line");
  }
  (void)ask_capturing_stderr(*machine, "current_prolog_flag(trace_gc, true).", ok);
  mosa_test::check(ok, "trace: flag reads back true");
  // 旗を戻す問い合わせの前の安全点ではまだ真なので、その出力は捨てる。次の問い合わせは旗が偽から始まるので、
  // ストレスでも行は出ない。
  (void)ask_capturing_stderr(*machine, "set_prolog_flag(trace_gc, false).", ok);
  mosa_test::check(ok, "trace: set false succeeds");
  const std::string stopped = ask_capturing_stderr(*machine, "garbage_collect.", ok);
  mosa_test::check(ok, "trace: collect after switching off succeeds");
  mosa_test::check(stopped.find("GC") == std::string::npos, "trace: no GC line after switching off");
}

}  // namespace

int main() {
  check_loop("two goals", k_loop);
  check_loop("three goals", k_loop_three);
  check_tak();
  check_cyclic();
  check_long_live_list();
  check_one_gc_after_large_step();
  check_ball();
  check_redo();
  check_tiny_limit();
  check_direct_collect();
  check_findall();
  check_bagof();
  check_bagof_fresh_witness();
  check_directive();
  check_with_output_to();
  check_consult();
  check_consult_marks();
  check_portray_inhibited();
  check_trace_line();
  return mosa_test::failures();
}
