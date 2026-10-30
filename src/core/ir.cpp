#include "mosa/ir.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace mosa {
namespace {

// args を除いた値だけを写す。子は呼び手が手で積んで写す。
IrTerm shallow(const IrTerm& from) {
  IrTerm to;
  to.tag = from.tag;
  to.slot = from.slot;
  to.atom = from.atom;
  to.arity = from.arity;
  to.integer = from.integer;
  to.floating = from.floating;
  return to;
}

}  // namespace

IrTerm::IrTerm(const IrTerm& other)
    : tag(other.tag), slot(other.slot), atom(other.atom), arity(other.arity), integer(other.integer),
      floating(other.floating) {
  if (other.args.empty()) return;
  // 前順に写す（emit_state.hpp の walk_term と同じ形）。親の args を先に揃えてから子の組を積むので、
  // 積んだ番地は後で動かない。
  std::vector<std::pair<const IrTerm*, IrTerm*>> pending{{&other, this}};
  while (!pending.empty()) {
    const auto [from, to] = pending.back();
    pending.pop_back();
    to->args.reserve(from->args.size());
    for (const IrTerm& child : from->args) to->args.push_back(shallow(child));
    for (std::size_t index = 0; index < from->args.size(); ++index) {
      if (!from->args[index].args.empty()) pending.emplace_back(&from->args[index], &to->args[index]);
    }
  }
}

IrTerm& IrTerm::operator=(const IrTerm& other) {
  if (this == &other) return *this;
  // other が自分の子でもよいよう、先に写してから入れ替える。
  IrTerm copy(other);
  return *this = std::move(copy);
}

IrTerm& IrTerm::operator=(IrTerm&& other) noexcept {
  if (this == &other) return *this;
  // 既定の代入は args を先に壊すので、other が自分の子だと壊れた先を読む。先に取り出してから入れ替える。
  IrTerm taken(std::move(other));
  tag = taken.tag;
  slot = taken.slot;
  atom = taken.atom;
  arity = taken.arity;
  integer = taken.integer;
  floating = taken.floating;
  args.swap(taken.args);
  return *this;
}

IrTerm::~IrTerm() {
  if (args.empty()) return;
  // 子の args を外に出して積み、葉だけになったものから壊す。ムーブの後の vector は空なので再び積まない。
  std::vector<IrTerm> pending = std::move(args);
  while (!pending.empty()) {
    IrTerm last = std::move(pending.back());
    pending.pop_back();
    for (IrTerm& child : last.args) {
      if (!child.args.empty()) pending.push_back(std::move(child));
    }
  }
}

}
