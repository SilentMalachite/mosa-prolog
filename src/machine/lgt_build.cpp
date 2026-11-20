// テストの本体の項を木にする。ランナーが判定する目標（assertion、variant、subsumes、=~=、forall、^^）を節点にし、
// それ以外は機械の query の字面にする。lgt_exec.cpp から分けた（800 行を超える前に、レビュー 2026-11-20 の計画）。

#include "lgt_prog.hpp"

#include "mosa/writer.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

using NodeKind = LgtNodeKind;
using Node = LgtNode;
using Prog = LgtProg;

int add_node(Prog& prog, Node node) {
  prog.nodes.push_back(std::move(node));
  return static_cast<int>(prog.nodes.size() - 1);
}

// lgtunit 自身の述語のうち、ランナーが判定するもの（assertion/1、assertion/2、variant/2）。
bool lgtunit_check_term(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (name == "assertion") return arity == 1 || arity == 2;
  return name == "variant" && arity == 2;
}

enum class GoalMix { Plain, Update, Runner };

bool runner_functor(std::string_view name, std::uint16_t arity) {
  if (name == "^^") return true;
  if (name == "assertion" && (arity == 1 || arity == 2)) return true;
  if (name == "forall" && arity == 2) return true;
  if (arity != 2) return false;
  return name == "variant" || name == "subsumes" || name == "=~=";
}

bool update_functor(std::string_view name, std::uint16_t arity) {
  // op/3 は assert と同じく表を変える副作用。同じ query の中で、後のゴールが変えた表を見るように動かす。
  if (name == "op" && arity == 3) return true;
  if (arity != 1) return false;
  return name == "asserta" || name == "assertz" || name == "retract" || name == "retractall" || name == "abolish";
}

bool control_goal(std::string_view name, std::uint16_t arity) {
  if (arity == 2 && (name == "," || name == ";" || name == "->" || name == "*->")) return true;
  return arity == 1 && (name == "\\+" || name == "once" || name == "call" || name == "{}");
}

GoalMix mix_of(const Heap& heap, Cell cell, std::vector<std::uint64_t>& seen) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str) return GoalMix::Plain;
  for (const auto raw : seen) {
    if (raw == cell.raw()) return GoalMix::Plain;
  }
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (runner_functor(name, arity)) return GoalMix::Runner;
  if (update_functor(name, arity)) return GoalMix::Update;
  if (!control_goal(name, arity)) return GoalMix::Plain;
  seen.push_back(cell.raw());
  GoalMix found = GoalMix::Plain;
  for (std::uint16_t index = 0; index < arity; ++index) {
    const GoalMix child = mix_of(heap, heap.arg(cell, index), seen);
    if (child == GoalMix::Runner) {
      found = GoalMix::Runner;
      break;
    }
    if (child == GoalMix::Update) found = GoalMix::Update;
  }
  seen.pop_back();
  return found;
}

// 7.5.4 は、その呼出しが始まったときの節列だけを候補にする。
// 更新を別の機械へ分けると、開いている走査は追加も削除も見ない。
// 平たい連言を分けると、後の束縛を字面で読み戻して前の項の変数が別セルになる。
// ランナー目標の無い選言も機械に任せる。前は Disj の節点になり、左が成功すると右の枝へ戻らず、forall/2 は
// Plain でない生成側を断った（レビュー M-49）。呼び手は , と ; の 2 引数の項だけを渡す。
bool one_activation(const Heap& heap, Cell cell) {
  std::vector<std::uint64_t> seen;
  return mix_of(heap, cell, seen) != GoalMix::Runner;
}

// ランナー目標が無い -> は -1。機械の if-then に残し、条件のカットを移さない。
int build_runner_if(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell,
                    const std::vector<VarName>& names);
// 左がランナーの -> でない ; は -1。else を同じ節点に置き、成功した条件が切れるようにする。
int build_runner_soft(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell,
                      const std::vector<VarName>& names);
// ランナー目標を含む連言。隣り合うランナー目標の無い項は 1 つの query にまとめる。
int build_conj(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell, const std::vector<VarName>& names);

int build_node(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell, const std::vector<VarName>& names) {
  cell = heap.deref(cell);
  if (cell.tag() == Tag::Str) {
    const auto name = heap.atoms().name(heap.functor_atom(cell));
    const auto arity = heap.functor_arity(cell);
    if (name == "->" && arity == 2) {
      const int arrow = build_runner_if(prog, heap, ops, cell, names);
      if (arrow >= 0) return arrow;
    }
    if (name == ";" && arity == 2) {
      const int soft = build_runner_soft(prog, heap, ops, cell, names);
      if (soft >= 0) return soft;
    }
    if ((name == "," || name == ";") && arity == 2 && one_activation(heap, cell)) {
      Node plain;
      plain.text = write_term(heap, ops, cell, lgt_write_options(), names);
      return add_node(prog, std::move(plain));
    }
    if (name == "," && arity == 2) return build_conj(prog, heap, ops, cell, names);
    if ((name == ";" || name == "forall") && arity == 2) {
      Node node;
      node.kind = name == ";" ? NodeKind::Disj : NodeKind::Forall;
      node.left = build_node(prog, heap, ops, heap.arg(cell, 0), names);
      node.right = build_node(prog, heap, ops, heap.arg(cell, 1), names);
      return add_node(prog, std::move(node));
    }
    if (name == "assertion" && (arity == 1 || arity == 2)) {
      Node node;
      node.kind = NodeKind::Assert;
      if (arity == 2) node.note = write_term(heap, ops, heap.arg(cell, 0), lgt_write_options(), names);
      node.left = build_node(prog, heap, ops, heap.arg(cell, arity == 1 ? 0 : 1), names);
      return add_node(prog, std::move(node));
    }
    if (name == "^^") {
      // 入出力表に渡すと未知の assertion/N や variant/2 になり、前のゴールの束縛が表明に届かない。
      if (arity == 1 && lgtunit_check_term(heap, heap.arg(cell, 0))) {
        return build_node(prog, heap, ops, heap.arg(cell, 0), names);
      }
      Node node;
      node.kind = NodeKind::Super;
      if (arity == 1) node.text = write_term(heap, ops, heap.arg(cell, 0), lgt_write_options(), names);
      else node.note = "^^/" + std::to_string(arity);
      return add_node(prog, std::move(node));
    }
    if (arity == 2 && (name == "variant" || name == "subsumes" || name == "=~=")) {
      Node node;
      node.text = write_term(heap, ops, cell, lgt_write_options(), names);
      if (name == "variant") node.kind = NodeKind::Variant;
      else if (name == "=~=") node.kind = NodeKind::Approx;
      else node.kind = NodeKind::Subsumes;
      return add_node(prog, std::move(node));
    }
  }
  Node plain;
  plain.text = write_term(heap, ops, cell, lgt_write_options(), names);
  return add_node(prog, std::move(plain));
}

int build_runner_if(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell,
                    const std::vector<VarName>& names) {
  std::vector<std::uint64_t> seen;
  // 子にランナー目標が無い。条件のカットは機械の if-then に残す。
  if (mix_of(heap, cell, seen) != GoalMix::Runner) return -1;
  Node node;
  node.kind = NodeKind::If;
  node.left = build_node(prog, heap, ops, heap.arg(cell, 0), names);
  node.right = build_node(prog, heap, ops, heap.arg(cell, 1), names);
  return add_node(prog, std::move(node));
}

int build_runner_soft(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell,
                      const std::vector<VarName>& names) {
  const Cell arrow = heap.deref(heap.arg(cell, 0));
  if (arrow.tag() != Tag::Str || heap.functor_arity(arrow) != 2) return -1;
  if (heap.atoms().name(heap.functor_atom(arrow)) != "->") return -1;
  std::vector<std::uint64_t> seen;
  // 機械だけの -> は選言の左の query のままにする。カットをランナーへ移さない。
  if (mix_of(heap, arrow, seen) != GoalMix::Runner) return -1;
  Node node;
  node.kind = NodeKind::Soft;
  node.left = build_node(prog, heap, ops, heap.arg(arrow, 0), names);
  node.right = build_node(prog, heap, ops, heap.arg(arrow, 1), names);
  node.other = build_node(prog, heap, ops, heap.arg(cell, 1), names);
  return add_node(prog, std::move(node));
}

bool runner_item(const Heap& heap, Cell cell) {
  std::vector<std::uint64_t> seen;
  return mix_of(heap, cell, seen) == GoalMix::Runner;
}

// 名前のある変数は本体の names で書くので、項ごとに書いて貼っても同じ変数のまま。名前の無い変数は _ で 1 度しか出ない。
int plain_run(Prog& prog, const Heap& heap, const OperatorTable& ops, const std::vector<Cell>& items,
              std::size_t begin, std::size_t end, const std::vector<VarName>& names) {
  Node plain;
  for (std::size_t index = begin; index < end; ++index) {
    if (index > begin) plain.text += ", ";
    plain.text += "(" + write_term(heap, ops, items[index], lgt_write_options(), names) + ")";
  }
  return add_node(prog, std::move(plain));
}

// lgtunit は本体を 1 つの節として流す。前のゴールが値の中の変数に付けた束縛（{numbervars(T, 0, N)} の後の T）は、
// query を分けると字面で残した前の値へ戻らない。ランナー目標の間の項の並びだけを 1 つの query にし、ランナー目標で
// 分ける形と右へ入れ子の Conj は今のままにする。
int build_conj(Prog& prog, const Heap& heap, const OperatorTable& ops, Cell cell, const std::vector<VarName>& names) {
  std::vector<Cell> items;
  cell = heap.deref(cell);
  while (lgt_functor_is(heap, cell, ",", 2)) {
    items.push_back(heap.arg(cell, 0));
    cell = heap.deref(heap.arg(cell, 1));
  }
  items.push_back(cell);
  std::vector<int> parts;
  std::size_t index = 0;
  while (index < items.size()) {
    std::size_t end = index + 1;
    if (!runner_item(heap, items[index])) {
      while (end < items.size() && !runner_item(heap, items[end])) ++end;
    }
    // 1 項だけの並びは前と同じ節点にする。
    parts.push_back(end - index == 1 ? build_node(prog, heap, ops, items[index], names)
                                     : plain_run(prog, heap, ops, items, index, end, names));
    index = end;
  }
  int right = parts.back();
  for (std::size_t at = parts.size() - 1; at-- > 0;) {
    Node node;
    node.kind = NodeKind::Conj;
    node.left = parts[at];
    node.right = right;
    right = add_node(prog, std::move(node));
  }
  return right;
}

}  // namespace

int lgt_build_node(LgtProg& prog, const Heap& heap, const OperatorTable& ops, Cell cell,
                   const std::vector<VarName>& names) {
  return build_node(prog, heap, ops, cell, names);
}

}  // namespace mosa
