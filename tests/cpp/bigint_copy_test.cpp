// フェーズ10 のタスク 4（ADR 0025）。Big を写しと IR の道に通し、値が戻ることを確かめる。copy_term/2、nb_setval/2、
// bagof/3 と setof/3 の像（sol::CopyKind::Big）、assertz/1 の節（IrTag::Big）とその呼び出し、clause/2、retract/1、
// 本体の Big、呼び出せる項の判定、第 1 引数の索引（KeyKind::Big）、assert の IR の勘定、Logtalk の検査が下書きの機械へ
// 節とゴールを写す道（lgt_copy.cpp）、節の指紋、native_eligible。
// Big は bigint_support.hpp の query_with_bigs と term_with_bigs で作る。

#include "bigint_support.hpp"
#include "harness.hpp"

#include "machine/internal.hpp"
#include "machine/lgt_priv.hpp"
#include "machine/load_internal.hpp"
#include "mosa/bigint.hpp"
#include "mosa/heap.hpp"
#include "mosa/machine.hpp"
#include "mosa/writer.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

using mosa::BigInt;
using mosa_test::check;

BigInt power_of_two(std::uint64_t bits) { return mosa::shift_left(BigInt(1), bits); }

std::unique_ptr<mosa::Machine> make_machine() {
  auto machine = mosa::Machine::create();
  check(machine->load_program(":- use_module(library(lists)).").ok, "load library(lists)");
  return machine;
}

mosa::Stop ask(mosa::Machine& machine, std::string_view goal, const std::vector<BigInt>& values = {}) {
  return query_with_bigs(machine, goal, values);
}

mosa::StopKind run(mosa::Machine& machine, std::string_view goal, const std::vector<BigInt>& values = {}) {
  return ask(machine, goal, values).kind;
}

// 名前の変数の値。無ければ Cell{}（タグは Ref）を返すので、呼び手はタグを見てから値を読む。
mosa::Cell bound(mosa::Machine& machine, const mosa::Stop& stop, std::string_view name) {
  for (const mosa::Binding& binding : stop.bindings) {
    if (binding.name == name) return machine.heap().deref(binding.variable);
  }
  return mosa::Cell{};
}

bool holds_big(mosa::Machine& machine, mosa::Cell cell, const BigInt& value) {
  return cell.tag() == mosa::Tag::Big && machine.heap().big_value(cell) == value;
}

bool holds_int(mosa::Machine& machine, mosa::Cell cell, std::int64_t value) {
  return (cell.tag() == mosa::Tag::Int || cell.tag() == mosa::Tag::IntBox) && machine.heap().integer_value(cell) == value;
}

bool holds_atom(mosa::Machine& machine, mosa::Cell cell, std::string_view name) {
  return cell.tag() == mosa::Tag::Atom && machine.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == name;
}

std::vector<mosa::Cell> list_items(const mosa::Heap& heap, mosa::Cell list) {
  std::vector<mosa::Cell> items;
  list = heap.deref(list);
  while (list.tag() == mosa::Tag::Lis) {
    items.push_back(heap.deref(heap.list_head(list)));
    list = heap.deref(heap.list_tail(list));
  }
  return items;
}

mosa::Cell arg_of(const mosa::Heap& heap, mosa::Cell cell, std::uint16_t index) {
  if (cell.tag() != mosa::Tag::Str || index >= heap.functor_arity(cell)) return mosa::Cell{};
  return heap.deref(heap.arg(cell, index));
}

void check_copy_term() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  const mosa::Stop stop = ask(*machine, "copy_term(f('$big0', X, ['$big1']), C), C = f(A, Y, [B])", {big, -big});
  check(stop.kind == mosa::StopKind::Success, "copy_term: succeeds");
  check(holds_big(*machine, bound(*machine, stop, "A"), big), "copy_term: B comes back");
  check(holds_big(*machine, bound(*machine, stop, "B"), -big), "copy_term: -B comes back");
  check(machine->heap().unbound(bound(*machine, stop, "Y")), "copy_term: the variable stays a variable");
}

void check_global() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  check(run(*machine, "nb_setval(k, f('$big0', ['$big1']))", {big, -big}) == mosa::StopKind::Success, "nb_setval: f(B, [-B])");
  const mosa::Stop stop = ask(*machine, "nb_getval(k, f(A, [B]))");
  check(stop.kind == mosa::StopKind::Success, "nb_getval: succeeds");
  check(holds_big(*machine, bound(*machine, stop, "A"), big) && holds_big(*machine, bound(*machine, stop, "B"), -big),
        "nb_getval: f(B, [-B]) comes back");
}

void check_findall() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  const mosa::Stop stop = ask(*machine, "findall(X, member(X, ['$big0', a, '$big1']), L)", {big, -big});
  const std::vector<mosa::Cell> items = list_items(machine->heap(), bound(*machine, stop, "L"));
  check(items.size() == 3 && holds_big(*machine, items[0], big) && holds_atom(*machine, items[1], "a") &&
            holds_big(*machine, items[2], -big),
        "findall: [B, a, -B]");
}

// bagof/3 と setof/3 は答えを像（sol::Image）に写す。証人 K と型 X の両方に Big を置く。bagof/3 の組は最初に出た
// 証人の順（8.10.2.1）なので B の組が先。
void check_bagof() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  const std::vector<BigInt> values{big, -big, big + BigInt(1), -(big + BigInt(1))};
  constexpr std::string_view pairs = "['$big0'-'$big2', '$big1'-b, '$big0'-'$big3']";
  const mosa::Stop first = ask(*machine, "bagof(X, member(K-X, " + std::string{pairs} + "), Xs)", values);
  check(first.kind == mosa::StopKind::Success, "bagof: succeeds");
  check(holds_big(*machine, bound(*machine, first, "K"), big), "bagof: the first witness is B");
  const std::vector<mosa::Cell> large = list_items(machine->heap(), bound(*machine, first, "Xs"));
  check(large.size() == 2 && holds_big(*machine, large[0], big + BigInt(1)) &&
            holds_big(*machine, large[1], -(big + BigInt(1))),
        "bagof: B gives [B+1, -(B+1)]");
  const mosa::Stop all = ask(*machine, "findall(K-Xs, bagof(X, member(K-X, " + std::string{pairs} + "), Xs), R)", values);
  const mosa::Heap& heap = machine->heap();
  const std::vector<mosa::Cell> groups = list_items(heap, bound(*machine, all, "R"));
  check(groups.size() == 2, "bagof: two groups");
  if (groups.size() != 2) return;
  check(holds_big(*machine, arg_of(heap, groups[1], 0), -big), "bagof: the second witness is -B");
  const std::vector<mosa::Cell> small = list_items(heap, arg_of(heap, groups[1], 1));
  check(small.size() == 1 && holds_atom(*machine, small[0], "b"), "bagof: -B gives [b]");
  const mosa::Stop sorted = ask(*machine, "setof(X, member(X, ['$big0', 7, '$big1', '$big0']), L)", {big, -big});
  const std::vector<mosa::Cell> items = list_items(heap, bound(*machine, sorted, "L"));
  check(items.size() == 3 && holds_big(*machine, items[0], -big) && holds_int(*machine, items[1], 7) &&
            holds_big(*machine, items[2], big),
        "setof: [-B, 7, B]");
}

// q/1 は 8 節より少ないので、鍵は節の列を歩く道で照らす。
void check_assert() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  check(run(*machine, "assertz(q('$big0')), assertz(q('$big1')), assertz(q(7))", {big, -big}) == mosa::StopKind::Success,
        "assert: q(B), q(-B), q(7)");
  const mosa::Stop stop = ask(*machine, "findall(Y, q(Y), L)");
  const std::vector<mosa::Cell> items = list_items(machine->heap(), bound(*machine, stop, "L"));
  check(items.size() == 3 && holds_big(*machine, items[0], big) && holds_big(*machine, items[1], -big) &&
            holds_int(*machine, items[2], 7),
        "assert: q(Y) gives B, -B, 7");
  check(run(*machine, "q('$big0')", {big}) == mosa::StopKind::Success, "assert: q(B) holds");
  check(run(*machine, "q('$big0')", {big + BigInt(1)}) == mosa::StopKind::Failure, "assert: q(B+1) fails");
  const mosa::Stop shown = ask(*machine, "findall(Y, clause(q(Y), true), L)");
  const std::vector<mosa::Cell> clauses = list_items(machine->heap(), bound(*machine, shown, "L"));
  check(clauses.size() == 3 && holds_big(*machine, clauses[0], big) && holds_big(*machine, clauses[1], -big),
        "clause: q(Y) gives B, -B, 7");
  check(run(*machine, "retract(q('$big0'))", {big + BigInt(1)}) == mosa::StopKind::Failure, "retract: q(B+1) fails");
  check(run(*machine, "retract(q('$big0'))", {big}) == mosa::StopKind::Success, "retract: q(B)");
  const mosa::Stop left = ask(*machine, "findall(Y, q(Y), L)");
  const std::vector<mosa::Cell> rest = list_items(machine->heap(), bound(*machine, left, "L"));
  check(rest.size() == 2 && holds_big(*machine, rest[0], -big) && holds_int(*machine, rest[1], 7),
        "retract: -B and 7 are left");
}

// assert の写しは、木に広げて 64 節点以上の部分項を 2 度目から共有の項にする（ADR 0022）。100 語の Big の f(B) は語の子を
// 数えて 102 節点なので共有になり、組み直しても値が戻る。
void check_shared() {
  auto machine = make_machine();
  const BigInt wide = power_of_two(64 * 100 - 1);
  check(run(*machine, "T = f('$big0'), assertz(sh(x, T, T))", {wide}) == mosa::StopKind::Success, "shared: assert sh/3");
  mosa::Impl& impl = mosa::engine_impl(*machine);
  const mosa::Predicate* predicate =
      mosa::find_pred(impl, mosa::PredKey{impl.user_module, impl.atoms.intern("sh"), 3});
  check(predicate != nullptr && predicate->clauses.size() == 1 && predicate->clauses[0].shared != nullptr,
        "shared: f(B) of 100 words is a shared term");
  const mosa::Stop stop = ask(*machine, "sh(x, A, B)");
  const mosa::Heap& heap = machine->heap();
  check(holds_big(*machine, arg_of(heap, bound(*machine, stop, "A"), 0), wide) &&
            holds_big(*machine, arg_of(heap, bound(*machine, stop, "B"), 0), wide),
        "shared: sh(x, A, B) gives f(B) twice");
}

// 本体のゴールの引数と投げる項の Big。本体は呼ぶたびに IR から組む。
void check_body() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  check(run(*machine, "assertz((u(X) :- X = f('$big0'))), assertz((t :- throw(e('$big1'))))", {big, -big}) ==
            mosa::StopKind::Success,
        "body: assert u/1 and t/0");
  const mosa::Heap& heap = machine->heap();
  const mosa::Stop called = ask(*machine, "u(Y)");
  check(holds_big(*machine, arg_of(heap, bound(*machine, called, "Y"), 0), big), "body: u(Y) gives f(B)");
  const mosa::Stop caught = ask(*machine, "catch(t, e(Z), true)");
  check(holds_big(*machine, bound(*machine, caught, "Z"), -big), "body: t throws e(-B)");
  const mosa::Stop shown = ask(*machine, "clause(u(_), Body)");
  check(holds_big(*machine, arg_of(heap, arg_of(heap, bound(*machine, shown, "Body"), 1), 0), big),
        "body: clause/2 shows _ = f(B)");
  check(run(*machine, "G = q('$big0'), assertz(G), call(G)", {big}) == mosa::StopKind::Success,
        "body: call/1 of q(B) after assertz");
}

// 数と同じく Big はゴールにならない（7.6.2）。type_error(callable, _) の Formal だけを比べる。
void check_callable() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  const auto raises = [&](std::string_view goal, std::string_view culprit) {
    const std::string text = "catch((" + std::string{goal} + "), error(E, _), true), E == type_error(callable, " +
                             std::string{culprit} + ")";
    return run(*machine, text, {big}) == mosa::StopKind::Success;
  };
  check(raises("assertz((s :- '$big0'))", "'$big0'"), "callable: assertz((s :- B))");
  check(raises("assertz((s :- (true, '$big0')))", "(true, '$big0')"), "callable: assertz((s :- (true, B)))");
  check(raises("assertz('$big0')", "'$big0'"), "callable: assertz(B)");
  check(raises("call('$big0')", "'$big0'"), "callable: call(B)");
  // 本体の判定が動かす前に断らなければ、fail が先に動いて call/1 は黙って失敗する。
  check(raises("call((fail, '$big0'))", "(fail, '$big0')"), "callable: call((fail, B))");
  check(run(*machine, "catch(s, error(existence_error(procedure, _), _), true)") == mosa::StopKind::Success,
        "callable: no s/0 was added");
  // 読み込みの節の判定（refused_clause）。字面から Big を読めるのはタスク 5 からなので、節を C++ で訳して渡す。
  mosa::Impl& impl = mosa::engine_impl(*machine);
  const std::vector<BigInt> values{big};
  const mosa::Cell term = term_with_bigs(*machine, "(s :- '$big0')", values);
  mosa::Clause clause;
  check(mosa::compile_clause(impl.heap, term, clause), "callable: s :- B compiles");
  const std::optional<mosa::Cell> formal = mosa::refused_clause(impl, term, clause);
  const std::string expected = "type_error(callable," + mosa::to_text(big) + ")";
  check(formal && mosa::write_term(impl.heap, machine->operators(), *formal, {}, {}) == expected,
        "callable: a loaded s :- B is refused with " + expected);
}

std::string assert_text(std::string_view how, std::string_view name, std::string_view key, int number) {
  return std::string{how} + "(" + std::string{name} + "(" + std::string{key} + ", " + std::to_string(number) + "))";
}

std::string numbers(mosa::Machine& machine, std::string_view goal, const std::vector<BigInt>& values) {
  const mosa::Stop stop = ask(machine, "findall(N, " + std::string{goal} + ", L)", values);
  std::string text;
  for (const mosa::Cell item : list_items(machine.heap(), bound(machine, stop, "L"))) {
    text += item.tag() == mosa::Tag::Int ? std::to_string(machine.heap().integer_value(item)) + " " : "? ";
  }
  return text;
}

// 第 1 引数が key の節だけが候補なら、その節に入ると選択点は残らない。
bool alone(mosa::Machine& machine, std::string_view goal, std::int64_t expected) {
  const mosa::Stop stop = ask(machine, goal);
  return stop.kind == mosa::StopKind::Success && holds_int(machine, bound(machine, stop, "N"), expected) && !stop.more;
}

// 鍵は値でなく big_key の指紋。B の指紋と同じビットの整数を鍵に持つ節を先頭に足す。KeyKind が Int と Big を分けて
// いれば、その整数の呼び出しの候補はその節だけで、選択点を残さない。r/2 は 11 節なので索引の組を引き、w/2 は 4 節
// なので節の列を歩く。
void check_index() {
  auto machine = make_machine();
  const BigInt big = power_of_two(100);
  const std::vector<BigInt> values{big, big + BigInt(1)};
  const std::string twin = std::to_string(std::bit_cast<std::int64_t>(mosa::big_key(false, big.words())));
  std::string program;
  for (int number = 0; number < 10; ++number) {
    program += (number == 0 ? "" : ", ") + assert_text("assertz", "r", number % 2 == 0 ? "'$big0'" : "'$big1'", number);
  }
  for (int number = 0; number < 3; ++number) {
    program += ", " + assert_text("assertz", "w", number % 2 == 0 ? "'$big0'" : "'$big1'", number);
  }
  program += ", " + assert_text("asserta", "r", twin, 10) + ", " + assert_text("asserta", "w", twin, 3);
  check(run(*machine, program, values) == mosa::StopKind::Success, "index: assert r/2 and w/2");
  check(numbers(*machine, "r('$big0', N)", {big}) == "0 2 4 6 8 ", "index: r(B, N) has exactly 5 answers");
  check(numbers(*machine, "r('$big0', N)", {big + BigInt(1)}) == "1 3 5 7 9 ", "index: r(B+1, N)");
  check(numbers(*machine, "r('$big0', N)", {-big}).empty(), "index: r(-B, N) has none");
  check(numbers(*machine, "r(X, N)", {}) == "10 0 1 2 3 4 5 6 7 8 9 ", "index: r(X, N) has all 11 in order");
  check(alone(*machine, "r(" + twin + ", N)", 10), "index: the twin integer key selects only its clause");
  check(numbers(*machine, "w('$big0', N)", {big}) == "0 2 ", "index: w(B, N) on the clause list");
  check(alone(*machine, "w(" + twin + ", N)", 3), "index: w(twin, N) on the clause list");
}

// assert の IR は節点の数をヒープの上限で決める（compile_asserted の ir_budget）。Big の語の子も節点に数えなければ、
// ヒープに収まる 100,000 語の Big が上限の何倍もの IR になっても止まらない。
void check_budget() {
  constexpr std::size_t limit = std::size_t{1} << 18;
  auto machine = mosa::Machine::create(limit);
  const BigInt huge = power_of_two(64 * 100000 - 1);
  bool limited = false;
  try {
    static_cast<void>(ask(*machine, "assertz(h('$big0'))", {huge}));
  } catch (const mosa::ResourceLimit&) {
    limited = true;
  }
  check(limited, "budget: asserting a 100,000-word Big stops at the heap limit");
  check(run(*machine, "assertz(h('$big0')), h(X), X == '$big0'", {power_of_two(100)}) == mosa::StopKind::Success,
        "budget: a small Big asserts after the stop");
}

// Logtalk の検査は、利用者の節とゴールを下書きの機械へセルのまま写して走らせる（lgt_run_copied_goal）。
void check_scratch() {
  auto main = make_machine();
  const BigInt big = power_of_two(100);
  check(run(*main, "assertz(q('$big0')), assertz((v(X) :- X = f('$big1')))", {big, -big}) == mosa::StopKind::Success,
        "scratch: assert q(B) and v/1");
  const auto copied = [&](std::string_view goal, const std::vector<BigInt>& values) {
    auto scratch = mosa::Machine::create();
    return mosa::lgt_run_copied_goal(*main, *scratch, term_with_bigs(*main, goal, values));
  };
  check(copied("(q(X), X == '$big0', v(Y), Y == f('$big1'))", {big, -big}), "scratch: q(B) and v(f(-B)) are copied");
  check(!copied("q('$big0')", {big + BigInt(1)}), "scratch: q(B+1) fails in the copy");
}

// 節 1 つずつの静的な述語。読み込みと同じ compile_clause で訳す。assert の述語は動的なので、native_eligible が先に断る。
mosa::Predicate static_predicate(mosa::Machine& machine, const std::vector<std::string_view>& texts,
                                 const std::vector<BigInt>& values) {
  mosa::Impl& impl = mosa::engine_impl(machine);
  mosa::Predicate predicate;
  predicate.module = impl.user_module;
  predicate.name = impl.atoms.intern("p");
  predicate.arity = 1;
  for (const std::string_view text : texts) {
    const mosa::Cell term = term_with_bigs(machine, text, values);
    mosa::Clause clause;
    if (!mosa::compile_clause(impl.heap, term, clause)) std::abort();
    mosa::append_clause(predicate, std::move(clause), 0);
  }
  return predicate;
}

std::uint64_t fingerprint(std::string_view text, const BigInt& value) {
  auto machine = mosa::Machine::create();
  const mosa::Predicate predicate = static_predicate(*machine, {text}, {value});
  return mosa::clause_fingerprint(machine->atoms(), predicate);
}

void check_fingerprint() {
  const BigInt big = power_of_two(100);
  check(fingerprint("p('$big0')", big) == fingerprint("p('$big0')", big), "fingerprint: B and B agree");
  check(fingerprint("p('$big0')", big) != fingerprint("p('$big0')", big + BigInt(1)), "fingerprint: B and B+1 differ");
  check(fingerprint("p('$big0')", big) != fingerprint("p('$big0')", -big), "fingerprint: B and -B differ");
  check(fingerprint("p(f(['$big0']))", big) != fingerprint("p(f(['$big0']))", big + BigInt(1)),
        "fingerprint: nested B and B+1 differ");
}

// 生成コードはまだ Big の定数を書けない（タスク 10）。頭と本体の入れ子の引数の Big も見つける。
void check_eligible() {
  auto machine = mosa::Machine::create();
  const mosa::Impl& impl = mosa::engine_impl(*machine);
  const std::vector<BigInt> values{power_of_two(100)};
  const auto eligible = [&](const std::vector<std::string_view>& texts) {
    return mosa::native_eligible(impl, static_predicate(*machine, texts, values));
  };
  check(eligible({"p(f([7]))", "(p(X) :- X = g(h(7)))"}), "eligible: int constants");
  check(!eligible({"p('$big0')"}), "eligible: a Big first argument");
  check(!eligible({"p(f(['$big0']))"}), "eligible: a Big nested in the head");
  check(!eligible({"p(7)", "(p(X) :- X = g(h('$big0')))"}), "eligible: a Big nested in the second clause's body");
}

}  // namespace

int main() {
  // 先の 4 つは IR を通らない。タスク 4 の前は bagof/3 と setof/3 の像が Big を浮動小数点数に読み、その後の assert が
  // data_leaf で止まる。
  check_copy_term();
  check_global();
  check_findall();
  check_bagof();
  check_assert();
  check_shared();
  check_body();
  check_callable();
  check_index();
  check_budget();
  check_scratch();
  check_fingerprint();
  check_eligible();
  return mosa_test::failures();
}
