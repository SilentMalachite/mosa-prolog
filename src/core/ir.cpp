#include "mosa/ir.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace mosa {
namespace {

// この深さまでは C++ の再帰で写し、壊す。より深い所だけを手で積むので、スタックは項の深さによらず一定で
// 済む（ADR 0011）。
constexpr int kShallow = 64;

// 深い所の複製で、写し残した親子の組。親の args を先に揃えてから積むので、積んだ番地は後で動かない。
using CopyPending = std::vector<std::pair<const IrTerm*, IrTerm*>>;

void copy_level(const IrTerm& from, IrTerm& to, CopyPending& pending) {
  to.args.reserve(from.args.size());
  for (const IrTerm& child : from.args) {
    // args を除いた値だけを写し、子の args は積んで後で写す。
    IrTerm& made = to.args.emplace_back();
    made.tag = child.tag;
    made.slot = child.slot;
    made.atom = child.atom;
    made.arity = child.arity;
    made.integer = child.integer;
    made.floating = child.floating;
    if (!child.args.empty()) pending.emplace_back(&child, &made);
  }
}

void copy_deep(const IrTerm& from, IrTerm& to) {
  CopyPending pending;
  copy_level(from, to, pending);
  while (!pending.empty()) {
    const auto [parent, made] = pending.back();
    pending.pop_back();
    copy_level(*parent, *made, pending);
  }
}

// 深い所の args を手で積んで壊す。子の args を外に出して積むので、子の ~IrTerm は空の args だけを見る。
void release_deep(std::vector<IrTerm>&& root) {
  std::vector<std::vector<IrTerm>> pending;
  pending.push_back(std::move(root));
  while (!pending.empty()) {
    std::vector<IrTerm> batch = std::move(pending.back());
    pending.pop_back();
    for (IrTerm& child : batch) {
      if (!child.args.empty()) pending.push_back(std::move(child.args));
    }
  }
}

}  // namespace

void IrTerm::copy_args(const IrTerm& other, Depth depth) {
  if (depth.value >= kShallow) return copy_deep(other, *this);
  args.reserve(other.args.size());
  for (const IrTerm& child : other.args) args.emplace_back(child, Depth{depth.value + 1});
}

// 後ろの子から、子の木を壊してから子を外す。既定の破棄と同じ順で解放する。
void IrTerm::release(int depth) noexcept {
  if (depth >= kShallow) return release_deep(std::move(args));
  while (!args.empty()) {
    IrTerm& last = args.back();
    if (!last.args.empty()) last.release(depth + 1);
    args.pop_back();
  }
}

IrTerm& IrTerm::operator=(const IrTerm& other) {
  if (this == &other) return *this;
  // other が自分の子でもよいよう、先に写してから入れ替える。古い args は copy と一緒に壊れる。
  IrTerm copy(other);
  tag = copy.tag;
  slot = copy.slot;
  atom = copy.atom;
  arity = copy.arity;
  integer = copy.integer;
  floating = copy.floating;
  args.swap(copy.args);
  return *this;
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

}
