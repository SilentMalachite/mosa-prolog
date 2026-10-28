// mosac が出すはずの形を手で書いた生成関数（ADR 0009）。include は runtime.hpp だけにする。
// 頭部は節の変数のうち未束縛のまま残すものだけをスロットの順に作り、単一化は節の側を左に置く
// （enter_clause が materialize した頭部を do_unify の左に渡すのと同じ向き）。

#include "native_units.hpp"

#include <array>
#include <cstdint>

namespace native_units {

using mosa::Cell;
using mosa::Machine;
using mosa::Tag;
namespace rt = mosa::rt;

Hits& hits() {
  static Hits counts;
  return counts;
}

namespace {

// 単位 1 の番号。アトムと呼び先は単位の中だけで振る。
constexpr std::uint32_t kNil = 0;
constexpr std::uint32_t kFirst = 1;
constexpr std::uint32_t kThird = 2;
constexpr std::uint32_t kA = 3;
constexpr std::uint32_t kApp = 0;
constexpr std::uint32_t kNrev = 1;
constexpr std::uint32_t kAtomLength = 2;
constexpr std::uint32_t kCutcEntry = 3;

constinit rt::UnitTag kListUnit{};
constexpr std::array<std::string_view, 4> kListAtoms{"[]", "first", "third", "a"};
constexpr std::array<rt::NativeCallee, 3> kListCalls{
    rt::NativeCallee{"user", "app", 3}, rt::NativeCallee{"user", "nrev", 2},
    rt::NativeCallee{"user", "atom_length", 2}};

Cell nil(const rt::Regs& r) { return rt::atom_cell(r.atoms[kNil]); }

// app([], L, L)。第1引数は呼び手が [] に合わせた。L の 2 回目は 1 回目の値（arg 1）が左。
void app_fact(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {
  if (!rt::unify(m, rt::arg(r, frame, 1), rt::arg(r, frame, 2))) return rt::fail(m);
  rt::proceed(m, rt::frame_cont(r, frame));
}

// app([H|T], L, [H|R]) :- app(T, L, R)。first は第1引数の [H|T] に当たるリスト。
void app_rule(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame, Cell first) {
  const Cell h = rt::head(first);
  const Cell third = rt::deref(rt::arg(r, frame, 2));
  Cell rest;
  if (rt::unbound(third)) {
    auto* block = rt::alloc(m, 3);
    rest = rt::put_ref(block);
    rt::bind(m, third, rt::put_list(block + 1, h, rest));
  } else if (third.tag() == Tag::Lis) {
    // H の 2 回目。節の側（第1引数から得た H）を左に置く。
    if (!rt::unify(m, h, rt::head(third))) return rt::fail(m);
    rest = rt::tail(third);
  } else {
    return rt::fail(m);
  }
  const std::array<Cell, 3> args{rt::tail(first), rt::arg(r, frame, 1), rest};
  rt::call_goal(m, r, kApp, args, rt::frame_cont(r, frame), r.cut);
}

// 第1引数が未束縛のときだけ両方の節が候補になる。select_clauses と同じ規則。
void app_unbound(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame, Cell first) {
  static constexpr std::array<std::uint32_t, 1> kRest{rt::state_of(2, 0)};
  rt::push_alternative(m, r, frame, &app_3, kRest);
  rt::bind(m, first, nil(r));
  app_fact(m, r, frame);
}

// 第2節のやり直し。第1引数は選択点の前の未束縛に戻っている。H と T をこの順に作る。
void app_redo_rule(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {
  const Cell first = rt::deref(rt::arg(r, frame, 0));
  auto* block = rt::alloc(m, 4);
  const Cell h = rt::put_ref(block);
  const Cell t = rt::put_ref(block + 1);
  const Cell list = rt::put_list(block + 2, h, t);
  rt::bind(m, first, list);
  app_rule(m, r, frame, list);
}

void nrev_fact(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {
  const Cell second = rt::deref(rt::arg(r, frame, 1));
  if (rt::unbound(second)) rt::bind(m, second, nil(r));
  else if (second.raw() != nil(r).raw()) return rt::fail(m);
  rt::proceed(m, rt::frame_cont(r, frame));
}

// nrev([H|T], R) :- nrev(T, S), app(S, [H], R)。未束縛で残るのは S だけ。枠の変数は H、R、S の順。
void nrev_rule(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame, Cell first) {
  auto* block = rt::alloc(m, 1);
  const Cell s = rt::put_ref(block);
  const std::array<Cell, 3> vars{rt::head(first), rt::arg(r, frame, 1), s};
  const auto next = rt::resume_frame(m, &nrev_2, rt::state_of(2, 1), r.cut, rt::frame_cont(r, frame), vars);
  const std::array<Cell, 2> args{rt::tail(first), s};
  rt::call_goal(m, r, kNrev, args, next, r.cut);
}

// 再開の状態ではカットの境界は枠の語 2 で、regs.cut ではない。
void nrev_resume(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {
  auto* block = rt::alloc(m, 2);
  const Cell one = rt::put_list(block, rt::var(r, frame, 0), nil(r));
  const std::array<Cell, 3> args{rt::var(r, frame, 2), one, rt::var(r, frame, 1)};
  rt::call_goal(m, r, kApp, args, rt::frame_cont(r, frame), rt::frame_cut(r, frame));
}

void nrev_unbound(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame, Cell first) {
  static constexpr std::array<std::uint32_t, 1> kRest{rt::state_of(2, 0)};
  rt::push_alternative(m, r, frame, &nrev_2, kRest);
  rt::bind(m, first, nil(r));
  nrev_fact(m, r, frame);
}

void nrev_redo_rule(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {
  const Cell first = rt::deref(rt::arg(r, frame, 0));
  auto* block = rt::alloc(m, 4);
  const Cell h = rt::put_ref(block);
  const Cell t = rt::put_ref(block + 1);
  const Cell list = rt::put_list(block + 2, h, t);
  rt::bind(m, first, list);
  nrev_rule(m, r, frame, list);
}

// chk(X) :- app(_, X, [a]), atom_length(X, _), app(_, _, X)。第2ゴールは継続枠が生きたまま例外を投げうる。
void chk_call(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {
  const Cell x = rt::arg(r, frame, 0);
  auto* block = rt::alloc(m, 3);
  const Cell anon = rt::put_ref(block);
  const Cell one = rt::put_list(block + 1, rt::atom_cell(r.atoms[kA]), nil(r));
  const std::array<Cell, 1> vars{x};
  const auto next = rt::resume_frame(m, &chk_1, rt::state_of(1, 1), r.cut, rt::frame_cont(r, frame), vars);
  const std::array<Cell, 3> args{anon, x, one};
  rt::call_goal(m, r, kApp, args, next, r.cut);
}

void chk_after_first(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {
  auto* block = rt::alloc(m, 1);
  const Cell anon = rt::put_ref(block);
  const std::array<Cell, 1> vars{rt::var(r, frame, 0)};
  const auto cut = rt::frame_cut(r, frame);
  const auto next = rt::resume_frame(m, &chk_1, rt::state_of(1, 2), cut, rt::frame_cont(r, frame), vars);
  const std::array<Cell, 2> args{rt::var(r, frame, 0), anon};
  rt::call_goal(m, r, kAtomLength, args, next, cut);
}

void chk_after_second(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {
  auto* block = rt::alloc(m, 2);
  const std::array<Cell, 3> args{rt::put_ref(block), rt::put_ref(block + 1), rt::var(r, frame, 0)};
  rt::call_goal(m, r, kApp, args, rt::frame_cont(r, frame), rt::frame_cut(r, frame));
}

void cutc_fact(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame, std::uint32_t atom) {
  const Cell x = rt::deref(rt::arg(r, frame, 0));
  const Cell value = rt::atom_cell(r.atoms[atom]);
  if (rt::unbound(x)) rt::bind(m, x, value);
  else if (x.raw() != value.raw()) return rt::fail(m);
  rt::proceed(m, rt::frame_cont(r, frame));
}

// cutc(first). cutc(X) :- !, X = second. cutc(third)。第2節はカットごと IR のまま動かす。
void cutc_call(Machine::Impl& m, const rt::Regs& r, std::uint64_t frame) {
  static constexpr std::array<std::uint32_t, 2> kAll{rt::state_of(2, 0), rt::state_of(3, 0)};
  static constexpr std::array<std::uint32_t, 1> kSecond{rt::state_of(2, 0)};
  static constexpr std::array<std::uint32_t, 1> kThirdOnly{rt::state_of(3, 0)};
  const Cell x = rt::deref(rt::arg(r, frame, 0));
  if (rt::unbound(x)) {
    rt::push_alternative(m, r, frame, &cutc_1, kAll);
    return cutc_fact(m, r, frame, kFirst);
  }
  if (x.raw() == rt::atom_cell(r.atoms[kFirst]).raw()) {
    rt::push_alternative(m, r, frame, &cutc_1, kSecond);
    return cutc_fact(m, r, frame, kFirst);
  }
  if (x.raw() == rt::atom_cell(r.atoms[kThird]).raw()) rt::push_alternative(m, r, frame, &cutc_1, kThirdOnly);
  rt::interpret_clause(m, r, kCutcEntry, frame, 2);
}

}  // namespace

void app_3(Machine::Impl& m, std::uint64_t frame, std::uint32_t state) {
  hits().app += 1;
  const rt::Regs r = rt::regs(m, kListUnit);
  switch (state) {
    case rt::kCall: {
      const Cell first = rt::deref(rt::arg(r, frame, 0));
      if (first.tag() == Tag::Lis) return app_rule(m, r, frame, first);
      if (first.raw() == nil(r).raw()) return app_fact(m, r, frame);
      if (rt::unbound(first)) return app_unbound(m, r, frame, first);
      return rt::fail(m);
    }
    case rt::state_of(2, 0): return app_redo_rule(m, r, frame);
    default: return rt::fail(m);
  }
}

void nrev_2(Machine::Impl& m, std::uint64_t frame, std::uint32_t state) {
  hits().nrev += 1;
  const rt::Regs r = rt::regs(m, kListUnit);
  switch (state) {
    case rt::kCall: {
      const Cell first = rt::deref(rt::arg(r, frame, 0));
      if (first.tag() == Tag::Lis) return nrev_rule(m, r, frame, first);
      if (first.raw() == nil(r).raw()) return nrev_fact(m, r, frame);
      if (rt::unbound(first)) return nrev_unbound(m, r, frame, first);
      return rt::fail(m);
    }
    case rt::state_of(2, 0): return nrev_redo_rule(m, r, frame);
    case rt::state_of(2, 1): return nrev_resume(m, r, frame);
    default: return rt::fail(m);
  }
}

void chk_1(Machine::Impl& m, std::uint64_t frame, std::uint32_t state) {
  hits().chk += 1;
  const rt::Regs r = rt::regs(m, kListUnit);
  switch (state) {
    case rt::kCall: return chk_call(m, r, frame);
    case rt::state_of(1, 1): return chk_after_first(m, r, frame);
    case rt::state_of(1, 2): return chk_after_second(m, r, frame);
    default: return rt::fail(m);
  }
}

void cutc_1(Machine::Impl& m, std::uint64_t frame, std::uint32_t state) {
  hits().cutc += 1;
  const rt::Regs r = rt::regs(m, kListUnit);
  switch (state) {
    case rt::kCall: return cutc_call(m, r, frame);
    case rt::state_of(2, 0): return rt::interpret_clause(m, r, kCutcEntry, frame, 2);
    case rt::state_of(3, 0): return cutc_fact(m, r, frame, kThird);
    default: return rt::fail(m);
  }
}

const rt::UnitTag& list_tag() { return kListUnit; }
std::span<const std::string_view> list_atoms() { return kListAtoms; }
std::span<const rt::NativeCallee> list_calls() { return kListCalls; }

namespace {

// 単位 2 の番号。rel/1 は翻訳のときの見え方 (a ===> b) - c で書いてある。
constexpr std::uint32_t kOpA = 0;
constexpr std::uint32_t kOpB = 1;
constexpr std::uint32_t kOpC = 2;
constexpr std::uint32_t kArrow = 3;
constexpr std::uint32_t kMinus = 4;
constexpr std::uint32_t kF = 5;

constinit rt::UnitTag kOpUnit{};
constexpr std::array<std::string_view, 6> kOpAtoms{"a", "b", "c", "===>", "-", "f"};

Cell op_atom(const rt::Regs& r, std::uint32_t index) { return rt::atom_cell(r.atoms[index]); }

// f(a, 2.5, -3) と束縛済みの第1引数を引数ごとに合わせる。
bool same_match(Machine::Impl& m, const rt::Regs& r, Cell x) {
  if (rt::functor_word(x) != mosa::pack_functor(r.atoms[kF], 3)) return false;
  const Cell first = rt::deref(rt::struct_arg(x, 0));
  if (rt::unbound(first)) rt::bind(m, first, op_atom(r, kOpA));
  else if (first.raw() != op_atom(r, kOpA).raw()) return false;
  if (!rt::unify(m, rt::put_float(rt::alloc(m, 1), 2.5), rt::struct_arg(x, 1))) return false;
  return rt::unify(m, rt::small_int(-3), rt::struct_arg(x, 2));
}

}  // namespace

void rel_1(Machine::Impl& m, std::uint64_t frame, std::uint32_t state) {
  hits().rel += 1;
  const rt::Regs r = rt::regs(m, kOpUnit);
  if (state != rt::kCall) return rt::fail(m);
  auto* block = rt::alloc(m, 6);
  const std::array<Cell, 2> inner_args{op_atom(r, kOpA), op_atom(r, kOpB)};
  const Cell inner = rt::put_struct(block, r.atoms[kArrow], inner_args);
  const std::array<Cell, 2> outer_args{inner, op_atom(r, kOpC)};
  const Cell outer = rt::put_struct(block + 3, r.atoms[kMinus], outer_args);
  if (!rt::unify(m, outer, rt::arg(r, frame, 0))) return rt::fail(m);
  rt::proceed(m, rt::frame_cont(r, frame));
}

void same_1(Machine::Impl& m, std::uint64_t frame, std::uint32_t state) {
  hits().same += 1;
  const rt::Regs r = rt::regs(m, kOpUnit);
  if (state != rt::kCall) return rt::fail(m);
  const Cell x = rt::deref(rt::arg(r, frame, 0));
  if (rt::unbound(x)) {
    auto* block = rt::alloc(m, 5);
    const Cell real = rt::put_float(block, 2.5);
    const std::array<Cell, 3> args{op_atom(r, kOpA), real, rt::small_int(-3)};
    rt::bind(m, x, rt::put_struct(block + 1, r.atoms[kF], args));
  } else if (x.tag() != Tag::Str || !same_match(m, r, x)) {
    return rt::fail(m);
  }
  rt::proceed(m, rt::frame_cont(r, frame));
}

const rt::UnitTag& op_tag() { return kOpUnit; }
std::span<const std::string_view> op_atoms() { return kOpAtoms; }

}
