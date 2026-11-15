// 条件付きコンパイル :- if/1、elif/1、else/0、endif/0（13211-1 に無い。SPEC 第3節、ADR 0027、iso-notes
// "Conditional compilation (not in 13211-1)"）。read_terms が項ごとに CondFrames::step を呼び、条件は run_condition が
// 読み込み中のモジュールで 1 回解く。指令の形は :- の 4 つだけで、ほかの項数（:- if(C, T, E) など）は一般の指令のまま。
// 誤り: if の無い elif・else・endif は cond_unmatched、else の後ろの elif と 2 つ目の else は cond_after_else、閉じない
// if は cond_unclosed（どれも読み込みの失敗で、読み飛ばし中の入れ子の節でも失敗させる。計画の「設計との違い」1）。
// 条件の例外はその Formal、halt/0,1 は読み込みをその場で止める。
// テスト: tests/pl/cond_compile.pl、tests/pl/japanese.pl、tests/cpp/cond_compile_test.cpp
#include "load_internal.hpp"

#include "mosa/writer.hpp"

#include <string_view>

namespace mosa {

namespace {

enum class CondKind { None, If, Elif, Else, Endif };

// term が条件の指令ならその種類。if と elif は条件を goal に入れる。
CondKind cond_kind(const Impl& machine, Cell term, Cell& goal) {
  term = machine.heap.deref(term);
  if (term.tag() != Tag::Str || machine.heap.functor_arity(term) != 1) return CondKind::None;
  if (machine.atoms.name(machine.heap.functor_atom(term)) != ":-") return CondKind::None;
  const Cell body = machine.heap.deref(machine.heap.arg(term, 0));
  if (body.tag() == Tag::Atom) {
    const std::string_view name = machine.atoms.name(static_cast<std::uint32_t>(body.raw() >> 3));
    if (name == "else") return CondKind::Else;
    return name == "endif" ? CondKind::Endif : CondKind::None;
  }
  if (body.tag() != Tag::Str || machine.heap.functor_arity(body) != 1) return CondKind::None;
  const std::string_view name = machine.atoms.name(machine.heap.functor_atom(body));
  if (name != "if" && name != "elif") return CondKind::None;
  goal = machine.heap.arg(body, 0);
  return name == "if" ? CondKind::If : CondKind::Elif;
}

// 対の誤り。指令を書いた字面（:-endif など）を添える。
int fail_cond(Impl& machine, std::string_view key, Cell term) {
  fail_load(machine, message_text(machine, key, {write_term(machine.heap, machine.operators, term, {}, {})}));
  return -1;
}

}  // namespace

int run_condition(Impl& machine, Cell goal) {
  const StopKind kind = run_parked(machine, in_load_module(machine, goal));
  // 例外の Formal は run_aside が診断に書いた。SWI-Prolog は偽として読み続けるが、Mosa は一般の指令と同じく読み込みを
  // 失敗させる（iso-notes "Conditional compilation (not in 13211-1)"）。
  if (kind == StopKind::Exception) return -1;
  // halt/0,1 は run_directive_goal と同じく読み込みをその場で止め、halt_now を残す。読んだ節は戻さない。
  if (kind == StopKind::Halt) {
    machine.load_kept = true;
    return -1;
  }
  // 成功も失敗も読み込みを続ける。条件の中の consult が初期化の例外で残した印を持ち越すと、この読み込みの後の
  // 構文の誤りで、取り消すはずの節が残る（run_directive_goal の成功と同じ理由）。
  machine.load_kept = false;
  return kind == StopKind::Success ? 1 : 0;
}

CondFrames::CondFrames(Impl& machine) : machine_(machine) {}

bool CondFrames::skipping() const { return !frames_.empty() && !frames_.back().reading; }

int CondFrames::step(Cell term) {
  Cell goal{};
  switch (cond_kind(machine_, term, goal)) {
    case CondKind::If: return open(goal);
    case CondKind::Elif: return next_branch(term, goal);
    case CondKind::Else: return otherwise(term);
    case CondKind::Endif: return finish(term);
    case CondKind::None: break;
  }
  return skipping() ? 1 : 0;
}

// 読み飛ばし中に開いた if は条件を呼ばず、endif と対にするためだけに積む。中の else と elif の誤りも見る。
int CondFrames::open(Cell goal) {
  if (skipping()) {
    frames_.push_back(Frame{true, false, false, false});
    return 1;
  }
  const int holds = run_condition(machine_, goal);
  if (holds < 0) return -1;
  frames_.push_back(Frame{false, holds == 1, false, holds == 1});
  return 1;
}

// 対の誤りは条件を呼ぶ前に見る。外が読み飛ばし中か前の分岐を取っていれば、条件を呼ばずに endif まで読み飛ばす。
int CondFrames::next_branch(Cell term, Cell goal) {
  if (frames_.empty()) return fail_cond(machine_, "cond_unmatched", term);
  if (frames_.back().seen_else) return fail_cond(machine_, "cond_after_else", term);
  if (frames_.back().outer_skipping || frames_.back().taken) {
    frames_.back().reading = false;
    return 1;
  }
  const int holds = run_condition(machine_, goal);
  if (holds < 0) return -1;
  frames_.back().taken = holds == 1;
  frames_.back().reading = holds == 1;
  return 1;
}

int CondFrames::otherwise(Cell term) {
  if (frames_.empty()) return fail_cond(machine_, "cond_unmatched", term);
  Frame& frame = frames_.back();
  if (frame.seen_else) return fail_cond(machine_, "cond_after_else", term);
  frame.seen_else = true;
  frame.reading = !frame.outer_skipping && !frame.taken;
  frame.taken = true;
  return 1;
}

int CondFrames::finish(Cell term) {
  if (frames_.empty()) return fail_cond(machine_, "cond_unmatched", term);
  frames_.pop_back();
  return 1;
}

// 閉じない if の診断は、テキストを読み終えた所でだけ書く。例外や halt で抜けた読み込みの診断を上書きしない。
bool CondFrames::close() {
  if (frames_.empty()) return true;
  fail_load(machine_, message_text(machine_, "cond_unclosed"));
  return false;
}

}  // namespace mosa
