// mark-and-compact の GC（ADR 0010、計画 Task 4）。ヒープ・extras・トレイルが頭打ちになり、生きている項が
// 値も形も変わらずに残ること。特に断らない限り上限は 2^16 語で、GC が無ければ loop は heap_limit で止まる。
#include "harness.hpp"

#include "machine/gc.hpp"
#include "machine/internal.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
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

// 印付けの語のスタックの深さは、長いリストや右に入れ子の Str の鎖でも長さによらない（ヒープの上限の外で
// メモリを食わない）。百万要素を作り、答えの束縛をルートにして印付けだけを回す。
std::size_t mark_stack_peak(std::string_view goal) {
  auto machine = mosa::Machine::create();
  const std::string program = std::string{k_make_list} + " chain(0, z) :- !. chain(N, f(N, T)) :- N1 is N - 1, chain(N1, T).";
  mosa_test::check(machine->load_program(program).ok, "mark stack: load");
  bool limited = false;
  const auto stop = ask(*machine, goal, limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "mark stack: " + std::string{goal});
  mosa::GcMarks marks;
  mosa::mark_live(mosa::engine_impl(*machine), marks);
  return marks.peak_cells;
}

void check_mark_stack_flat() {
  const auto list = mark_stack_peak("mk(1000000, L).");
  mosa_test::check(list < 64, "mark stack: list peak stays small (" + std::to_string(list) + ")");
  const auto chain = mark_stack_peak("chain(1000000, S).");
  mosa_test::check(chain < 64, "mark stack: chain peak stays small (" + std::to_string(chain) + ")");
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

// Float と IntBox（2^60 を超える整数の箱）が詰め直しで下へ動いても値を保つ。length/2 の死んだリストの上で
// is/2 が新しい箱を作るので、GC はそれらをリストの分だけ下へ写す。
void check_boxed_relocation() {
  auto machine = mosa::Machine::create(k_small);
  auto& impl = mosa::engine_impl(*machine);
  const auto stop =
      machine->query("length(_, 2000), F is 1.5e300 * 1.0, B is 4611686018427387903 + 1, N is -B - 1.");
  mosa_test::check(stop.kind == mosa::StopKind::Success && stop.bindings.size() == 3, "boxed: query succeeds");
  if (stop.bindings.size() != 3) return;
  std::array<std::uint64_t, 3> before{};
  std::array<std::string, 3> texts;
  for (std::size_t index = 0; index < 3; ++index) {
    before[index] = machine->heap().deref(impl.bindings[index].variable).raw();
    texts[index] = binding_text(*machine, stop, index);
  }
  mosa_test::check(machine->heap().deref(impl.bindings[0].variable).tag() == mosa::Tag::Float, "boxed: F is a Float");
  mosa_test::check(machine->heap().deref(impl.bindings[1].variable).tag() == mosa::Tag::IntBox, "boxed: B is an IntBox");
  mosa_test::check(machine->heap().deref(impl.bindings[2].variable).tag() == mosa::Tag::IntBox, "boxed: N is an IntBox");
  mosa::collect_garbage(impl);
  mosa::Stop after;
  after.bindings = impl.bindings;
  for (std::size_t index = 0; index < 3; ++index) {
    const auto label = "boxed " + std::string{impl.bindings[index].name};
    mosa_test::check(machine->heap().deref(impl.bindings[index].variable).raw() < before[index], label + ": slid down");
    mosa_test::check(binding_text(*machine, after, index) == texts[index], label + ": reads " + texts[index]);
  }
  mosa_test::check(texts[0] == "1.5e300" || texts[0] == "1.5e+300", "boxed: F text " + texts[0]);
  mosa_test::check(texts[1] == "4611686018427387904" && texts[2] == "-4611686018427387905", "boxed: integer texts");
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

// portray/1 の中でも GC が走る（ADR 0011）。writer は積んだ仕事と表を RootScope で GC に見せ、動いた番地で
// 表を作り直す。深さ 10,000 の項を print し、出力がヒープを使わない portray のときと同じであることを見る。
void check_portray_gc() {
  constexpr std::string_view k_nest = "nest(0, X, X) :- !. nest(N, X, Y) :- M is N - 1, nest(M, f(X, N), Y).";
  // 引数の portray は毎回リストを作って捨て、失敗して writer に戻る。葉の portray は garbage_collect/0 も呼ぶ。
  auto machine = mosa::Machine::create(std::size_t{1} << 20);
  mosa_test::check(machine->load_program(std::string(k_make_list) + " " + std::string(k_nest) +
                                         " portray(leaf) :- !, garbage_collect, write(hidden)."
                                         " portray(_) :- mk(50, _), fail.")
                       .ok,
                   "portray_gc: load");
  auto plain = mosa::Machine::create(std::size_t{1} << 20);
  mosa_test::check(plain->load_program(std::string(k_nest) + " portray(leaf) :- !, write(hidden).").ok,
                   "portray_gc: load plain");
  const std::string goal = "nest(10000, leaf, T), with_output_to(atom(A), print(T)).";
  auto& impl = mosa::engine_impl(*machine);
  const auto before = impl.gc_stats.count;
  bool limited = false;
  bool plain_limited = false;
  const auto stop = ask(*machine, goal, limited);
  const auto want = ask(*plain, goal, plain_limited);
  mosa_test::check(!limited && !plain_limited && stop.kind == mosa::StopKind::Success && want.kind == mosa::StopKind::Success,
                   "portray_gc: both print");
  const std::string got = binding_text(*machine, stop, 1);
  mosa_test::check(got.size() > 60000 && got == binding_text(*plain, want, 1), "portray_gc: same text as plain");
  mosa_test::check(got.find("hidden") != std::string::npos, "portray_gc: leaf portrayed");
  mosa_test::check(impl.gc_stats.count > before, "portray_gc: GC ran inside the hook");
}

// ADR 0011: 200,000 要素のリストを持つ節を、上限に当たるまで繰り返し呼ぶ。手で積む実体化の途中で heap_limit で
// 止まり、次の問い合わせは普通に動く。上限は節を読み込める大きさ（2^20 語）で、3 回分の実体化は入らない。
void check_materialize_limit() {
  std::string program = "data([";
  for (int index = 1; index <= 200000; ++index) program += (index == 1 ? "" : ",") + std::to_string(index);
  program += "]). ok(yes).";
  auto machine = mosa::Machine::create(std::size_t{1} << 20);
  mosa_test::check(machine->load_program(program).ok, "materialize limit: load");
  bool limited = false;
  (void)ask(*machine, "data(A), data(B), data(C), data(D).", limited);
  mosa_test::check(limited, "materialize limit: heap_limit");
  const auto next = ask(*machine, "ok(X), data(L), length(L, N).", limited);
  mosa_test::check(!limited && next.kind == mosa::StopKind::Success && binding_text(*machine, next, 0) == "yes" &&
                       binding_text(*machine, next, 2) == "200000",
                   "materialize limit: next query runs");
}

// 上限の近くで空きを生まない GC が続けば止める（ADR 0010 の 2026-10-30 改訂、所有者の決定 B）。
// junk/1 は 1 歩ごとに捨てる項を作り、grow/2 は生きたリストを 1 要素ずつ伸ばす。
constexpr std::string_view k_thrash =
    "junk(0) :- !. junk(K) :- _ = f(K, K, K, K), K1 is K - 1, junk(K1).\n"
    "grow(N, L) :- junk(5), N1 is N + 1, grow(N1, [N|L]).";

// 例外の文面も返す。ResourceLimit の文面は止め方の鍵（heap_limit）である。
std::string ask_stop_text(mosa::Machine& machine, std::string_view goal) {
  try {
    (void)machine.query(goal);
  } catch (const mosa::ResourceLimit& stop) {
    return stop.what();
  }
  return {};
}

// length/2 の 2 万 1 千要素（6 万 3 千語）で上限 2^16 語の 96% を 1 歩で生かし、捨てる項を作りながら伸ばす。
// 空きは 2536 語で上限の 1/16（4096 語）より狭いので、閾値は L と上限の中ほどにあり、閾値で始まる GC は
// 1 歩の確保を足しても 2048 語（上限の 1/32）を空けられない。length の直後の GC（空きは 0）から 3 回で止まる。
// 改訂の前は、空きが 1 歩の確保を下回って確保が上限に当たるまで、何百回も GC が続いた。
void check_thrash_stops() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_thrash).ok, "thrash: load");
  auto& impl = mosa::engine_impl(*machine);
  const auto before = impl.gc_stats.count;
  const std::string text = ask_stop_text(*machine, "length(L, 21000), grow(0, L).");
  mosa_test::check(text == "heap_limit", "thrash: stops with heap_limit (got '" + text + "')");
  const auto count = impl.gc_stats.count - before;
  // ストレスでは回数を比べず、どちらの止まり方かも問わない。ストレスの GC が閾値より先に捨てる項を回収するので、
  // 閾値に届くかは N による（N = 1 なら閾値に届かず、生きたリストが伸びて確保が上限に当たる）。どちらも同じ
  // ResourceLimit("heap_limit") で、止まったときのヒープの大きさでも見分けられない（確保は上限を超える前に断る）。
  if (impl.gc_stress == 0) mosa_test::check(count <= 3, "thrash: at most 3 GCs (got " + std::to_string(count) + ")");
}

// 生きているデータが上限の半分ほど（3 万語）なら、1 回の GC で上限の 1/4 ほどが空くので止めない。
void check_productive_near_limit() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_loop).ok, "productive: load");
  auto& impl = mosa::engine_impl(*machine);
  bool limited = false;
  const auto stop = ask(*machine, "length(L, 10000), loop(1000000), L = [_|_].", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "productive: finishes without a stop");
  mosa_test::check(impl.gc_stats.count >= 1, "productive: GC ran");
}

// garbage_collect/0 の GC は数えない。6 万 3 千語を生かしたまま何度呼んでも、空きを生まないことで止めない。
void check_explicit_collect_not_counted() {
  auto machine = mosa::Machine::create(k_small);
  bool limited = false;
  const auto stop = ask(*machine,
                        "length(L, 21000), garbage_collect, garbage_collect, garbage_collect, garbage_collect, "
                        "garbage_collect, L = [_|_].",
                        limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "explicit collect: not counted");
}

// 生きているデータが上限の 9 割ほど（1 万 9500 要素、5 万 8500 語、89%）でも、空きは上限の 1/16 より広く、
// 閾値で始まる GC は上限の 1/32 より多く空けるので止めない。
void check_productive_at_ninety() {
  auto machine = mosa::Machine::create(k_small);
  mosa_test::check(machine->load_program(k_loop).ok, "ninety: load");
  bool limited = false;
  const auto stop = ask(*machine, "length(L, 19500), loop(1000000), L = [_|_].", limited);
  mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, "ninety: finishes without a stop");
}

// 3 回に分けて上限の 88% まで生かし、その間に捨てる項を作る（再レビューの diverge.pl を半分の上限に縮めたもの）。
// ストレスの GC が捨てる項を先に回収すると、続く閾値の GC は自分ではほとんど空けない。判定を前の判定からの空き
// （ストレスの GC の分も含む）で数えるので、ストレスの有無で止まるかが分かれない。
constexpr std::string_view k_diverge =
    "length(A, 11000), junk(3000), length(B, 5500), junk(3000), length(C, 2700), junk(3000), "
    "A = [_|_], B = [_|_], C = [_|_].";

void check_stress_same_outcome() {
  for (const std::size_t every : {std::size_t{0}, std::size_t{10}, std::size_t{1000}}) {
    const std::string label = "diverge stress " + std::to_string(every);
    auto machine = mosa::Machine::create(k_small);
    mosa_test::check(machine->load_program(k_thrash).ok, label + ": load");
    auto& impl = mosa::engine_impl(*machine);
    // 環境の MOSA_GC_STRESS によらず、この機械だけ N 回の安全点ごとに GC する（init_gc の後で上書きする）。
    if (every != 0) {
      impl.gc_stress = every;
      impl.gc_ticks = 0;
      impl.gc_trigger = 0;
    }
    bool limited = false;
    const auto stop = ask(*machine, k_diverge, limited);
    mosa_test::check(!limited && stop.kind == mosa::StopKind::Success, label + ": finishes without a stop");
  }
}

// 数と空きの基準は、トップレベルの問い合わせ・consult・load_program のたびに戻す。前の分を次へ持ち越さない。
void check_unproductive_reset() {
  auto machine = mosa::Machine::create(k_small);
  auto& impl = mosa::engine_impl(*machine);
  const std::string file = consulted_path("reset.pl", "r(1).\n");
  const auto reset = [&impl] { return impl.gc_unproductive == 0 && impl.gc_judged_freed == impl.gc_stats.freed_words; };
  impl.gc_unproductive = 2;
  impl.gc_judged_freed = impl.gc_stats.freed_words + 1;
  const auto stop = machine->query("true.");
  mosa_test::check(stop.kind == mosa::StopKind::Success && reset(), "unproductive count resets at a query");
  impl.gc_unproductive = 2;
  impl.gc_judged_freed = impl.gc_stats.freed_words + 1;
  mosa_test::check(machine->consult(file) && reset(), "unproductive count resets at consult");
  impl.gc_unproductive = 2;
  impl.gc_judged_freed = impl.gc_stats.freed_words + 1;
  mosa_test::check(machine->load_program("s(1).").ok && reset(), "unproductive count resets at load_program");
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

// trace_gc の行はカタログで直に作った文面とバイトまで同じで、言語を変えれば従う。行ごとに新しいマシンで
// カタログを読み直すと 1 行 1.5 ms ほどかかるので、2000 行が 0.5 秒に収まることで読み直していないことを見る。
void check_trace_text() {
  const std::array<std::string, 5> numbers{"7", "1024", "512", "2048", "1.25"};
  auto machine = mosa::Machine::create(k_small);
  auto& impl = mosa::engine_impl(*machine);
  for (const auto language : {mosa::MessageLanguage::En, mosa::MessageLanguage::Ja, mosa::MessageLanguage::En}) {
    impl.message_lang = language;
    const std::string label = language == mosa::MessageLanguage::Ja ? "trace text ja" : "trace text en";
    const std::string expected =
        mosa::message_text(impl, "gc_trace", {numbers[0], numbers[1], numbers[2], numbers[3], numbers[4]});
    mosa_test::check(!expected.starts_with("gc_trace:") && expected.find("1024") != std::string::npos,
                     label + ": the catalog renders the line");
    mosa_test::check(mosa::gc_trace_text(impl, numbers) == expected, label + ": same bytes as the catalog");
    const auto started = std::chrono::steady_clock::now();
    for (int index = 0; index < 2000; ++index) (void)mosa::gc_trace_text(impl, numbers);
    const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    mosa_test::check(took.count() < 500, label + ": 2000 lines in " + std::to_string(took.count()) + " ms");
  }
}

}  // namespace

int main() {
  check_loop("two goals", k_loop);
  check_loop("three goals", k_loop_three);
  check_tak();
  check_cyclic();
  check_long_live_list();
  check_one_gc_after_large_step();
  check_mark_stack_flat();
  check_ball();
  check_boxed_relocation();
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
  check_portray_gc();
  check_materialize_limit();
  check_thrash_stops();
  check_productive_near_limit();
  check_explicit_collect_not_counted();
  check_productive_at_ninety();
  check_stress_same_outcome();
  check_unproductive_reset();
  check_trace_line();
  check_trace_text();
  return mosa_test::failures();
}
