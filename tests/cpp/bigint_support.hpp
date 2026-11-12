#pragma once

// Big を持つゴールを走らせるテストの入口（フェーズ10 のタスク 3、ADR 0025）。タスク 5 まで字面から Big を作れないので、
// 読んだゴールの写し（ExportedTerm）のアトム '$big0'、'$big1'、… を values の同じ番号の値の Big の節点に替えて走らせる。
// 字面の変数は答えの束縛にする。束縛は走り出す前に根に入るので、MOSA_GC_STRESS=1 でも動いた番地に付いていく。

#include "machine/internal.hpp"
#include "mosa/bigint.hpp"
#include "mosa/machine.hpp"
#include "mosa/reader.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bigint_support {

// '$big' に数字だけが続くアトムなら、その番号。
inline std::optional<std::size_t> big_slot(std::string_view name) {
  constexpr std::string_view prefix = "$big";
  if (!name.starts_with(prefix) || name.size() == prefix.size()) return std::nullopt;
  std::size_t index = 0;
  for (const char digit : name.substr(prefix.size())) {
    if (digit < '0' || digit > '9') return std::nullopt;
    index = index * 10 + static_cast<std::size_t>(digit - '0');
  }
  return index;
}

// int64 に収まる値も Big の節点のまま置く。取り込みが正規形（Int か IntBox）に直すことも、この入口で確かめられる。
inline void put_big(mosa::ExportedTerm& term, std::size_t at, const mosa::BigInt& value) {
  term.nodes[at].kind = mosa::ExportedTerm::Kind::Big;
  term.nodes[at].atom = 0;
  term.nodes[at].integer = value.negative() ? 1 : 0;
  for (const std::uint64_t word : value.words()) {
    mosa::ExportedTerm::Node child;
    child.kind = mosa::ExportedTerm::Kind::Int;
    child.integer = std::bit_cast<std::int64_t>(word);
    term.nodes.push_back(std::move(child));
    term.nodes[at].kids.push_back(static_cast<int>(term.nodes.size() - 1));
  }
}

}  // namespace bigint_support

// 読めない字面と values の外の番号はテストの誤りなので、黙って続けずに止める。
inline mosa::Stop query_with_bigs(mosa::Machine& machine, std::string_view goal_text,
                                  std::span<const mosa::BigInt> values) {
  mosa::Impl& impl = mosa::engine_impl(machine);
  // reader は終わりの . を要る。. で終わる字面に足しても、読むのは最初の 1 項だけなので同じになる。
  const std::string text = std::string{goal_text} + " .";
  mosa::Reader reader(text, impl.heap, impl.operators, mosa::current_double_quotes(impl));
  const mosa::ReadOutcome outcome = reader.next();
  if (!outcome.term) std::abort();
  // '$bigq'(Goal, V1, ..., Vn) を写すと、根の子 1..n が名前の付いた変数の節点になる。根は Goal に移す。
  std::vector<mosa::Cell> args{outcome.term->term};
  for (const mosa::VarName& name : outcome.term->names) args.push_back(name.cell);
  mosa::ExportedTerm goal = mosa::export_term(impl, impl.heap.structure(impl.atoms.intern("$bigq"), args));
  const std::vector<int> kids = goal.nodes[static_cast<std::size_t>(goal.root)].kids;
  std::vector<mosa::ExportedName> names;
  for (std::size_t index = 0; index < outcome.term->names.size(); ++index) {
    names.push_back(mosa::ExportedName{outcome.term->names[index].name, kids[index + 1]});
  }
  goal.root = kids[0];
  const std::size_t count = goal.nodes.size();
  for (std::size_t at = 0; at < count; ++at) {
    if (goal.nodes[at].kind != mosa::ExportedTerm::Kind::Atom) continue;
    const std::optional<std::size_t> slot = bigint_support::big_slot(impl.atoms.name(goal.nodes[at].atom));
    if (!slot) continue;
    if (*slot >= values.size()) std::abort();
    bigint_support::put_big(goal, at, values[*slot]);
  }
  return mosa::query_exported(machine, goal, names);
}
