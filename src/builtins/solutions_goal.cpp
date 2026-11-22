// 8.10.1 findall/3、findall/4、8.10.2 bagof/3、8.10.3 setof/3 の、ゴールを動かす前の検査と証人。
//   which_solution は名前と引数の数から述語を決める。goal_shape_ok と goal_body_ok は instantiation_error、
//   type_error(callable, Goal)、ゴールの制御構造が巡回すれば resource_error(cyclic_term)。strip_caret は先頭の ^ の
//   連なりを外し、巡回すれば resource_error(cyclic_term)。bad_list は第3引数の type_error(list, List)、bad_tail は
//   findall/4 の第4引数の type_error(list, Tail)。free_witnesses は bagof/3 と setof/3 の証人（7.1.1.4 の自由変数）。
//   誤りの順は solutions.cpp の頭の注釈のとおりで、呼ぶ順は solutions.cpp の run_solutions が決める。
// solutions.cpp から分けた（800 行の上限のため）。移しただけで、振る舞いは変えない。
// テスト: tests/pl/solutions.pl

#include "../machine/internal.hpp"
#include "solutions_internal.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace mosa::sol {
namespace {

bool listed(const std::vector<Cell>& vars, Cell cell) {
  for (const Cell& var : vars) {
    if (var.raw() == cell.raw()) return true;
  }
  return false;
}

// 1 つの項を見る。変数なら vars に足し、まだ見ていない複合項なら子を右から pending に積む。右から積むので、
// 取り出す順は前の再帰と同じ左からの前順になり、vars の並びも変わらない。
void free_one(const Impl& machine, Cell term, std::vector<Cell>& vars, std::unordered_set<std::uint64_t>& seen,
              std::vector<Cell>& pending) {
  if (term.tag() == Tag::Ref) {
    const Cell value = machine.heap.deref(term);
    if (machine.heap.unbound(value)) {
      if (!listed(vars, value)) vars.push_back(value);
      return;
    }
    term = value;
  } else {
    term = machine.heap.deref(term);
  }
  if (machine.heap.unbound(term) || (term.tag() != Tag::Lis && term.tag() != Tag::Str)) return;
  if (!seen.insert(term.raw()).second) return;
  if (term.tag() == Tag::Lis) {
    pending.push_back(machine.heap.list_tail(term));
    pending.push_back(machine.heap.list_head(term));
    return;
  }
  for (std::uint16_t index = machine.heap.functor_arity(term); index > 0; --index) {
    pending.push_back(machine.heap.arg(term, static_cast<std::uint16_t>(index - 1)));
  }
}

// 深い項でも C++ のスタックを使わないよう、まだ見ていない項を手で積む（ADR 0011）。
void collect_free(const Impl& machine, Cell term, std::vector<Cell>& vars, std::unordered_set<std::uint64_t>& seen) {
  std::vector<Cell> pending{term};
  while (!pending.empty()) {
    const Cell next = pending.back();
    pending.pop_back();
    free_one(machine, next, vars, seen, pending);
  }
}

bool is_caret(const Impl& machine, Cell cell) {
  cell = machine.heap.deref(cell);
  if (cell.tag() != Tag::Str || machine.heap.functor_arity(cell) != 2) return false;
  return machine.atoms.name(machine.heap.functor_atom(cell)) == "^";
}

std::vector<Cell> witness_vars(const std::vector<Cell>& goal_vars, const std::vector<Cell>& templ,
                               const std::vector<Cell>& hidden) {
  std::vector<Cell> out;
  for (const Cell& var : goal_vars) {
    if (listed(templ, var) || listed(hidden, var)) continue;
    out.push_back(var);
  }
  return out;
}

}  // namespace

Which which_solution(std::string_view name, std::uint16_t arity) {
  // bagof/4 と setof/4 は差分リストではない。開いた末尾は findall だけ。
  if (name == "findall" && (arity == 3 || arity == 4)) return Which::Findall;
  if (arity != 3) return Which::None;
  if (name == "bagof") return Which::Bagof;
  if (name == "setof") return Which::Setof;
  return Which::None;
}

// 先頭に並んだ ^/2 だけを外し、goal を残りにする。中のゴールに埋まった ^ は量化ではない。
// 連なりが巡回すれば誤りの Formal（iso-notes 7.3.3）。次の層は今の層だけで決まるので、M: と同じ見張りで足りる。
// 前はヒープに数えない物だけを触って回り続け、--heap-max でも止まらなかった（レビュー、2026-11-17）。
std::optional<Cell> strip_caret(Impl& machine, Cell& goal, std::vector<Cell>& hidden) {
  goal = machine.heap.deref(goal);
  LayerWatch watch(goal);
  while (is_caret(machine, goal)) {
    std::unordered_set<std::uint64_t> seen;
    collect_free(machine, machine.heap.arg(goal, 0), hidden, seen);
    goal = machine.heap.deref(machine.heap.arg(goal, 1));
    if (watch.returned_to(goal)) return formal_resource(machine, "cyclic_term");
  }
  return std::nullopt;
}

std::vector<Cell> free_witnesses(Impl& machine, Cell goal, Cell templ, const std::vector<Cell>& hidden) {
  std::vector<Cell> goal_vars;
  std::vector<Cell> templ_vars;
  std::unordered_set<std::uint64_t> seen_goal;
  std::unordered_set<std::uint64_t> seen_templ;
  collect_free(machine, goal, goal_vars, seen_goal);
  collect_free(machine, templ, templ_vars, seen_templ);
  return witness_vars(goal_vars, templ_vars, hidden);
}

bool goal_shape_ok(Impl& machine, const Goal& goal, Cell cell) {
  cell = machine.heap.deref(cell);
  if (machine.heap.unbound(cell)) {
    raise_formal(machine, formal_instantiation(machine), goal.name, goal.arity);
    return false;
  }
  if (cell.tag() == Tag::Atom || cell.tag() == Tag::Str || cell.tag() == Tag::Lis) return true;
  raise_formal(machine, formal_type(machine, "callable", cell), goal.name, goal.arity);
  return false;
}

bool goal_body_ok(Impl& machine, const Goal& goal, Cell cell, Compiled& compiled) {
  if (!goal_shape_ok(machine, goal, cell)) return false;
  compiled = compile_goal(machine.heap, cell);
  if (!body_callable(*compiled.term)) {
    raise_formal(machine, formal_type(machine, "callable", cell), goal.name, goal.arity);
    return false;
  }
  return !raise_cyclic_goal(machine, compiled, goal.name, goal.arity);
}

// 部分リストは 8.10 の type_error(list) に含まれない。
bool bad_list(Impl& machine, const Goal& goal) {
  const Cell cell = machine.heap.deref(goal.arg(machine, 2));
  const WalkedList walked = walk_list(machine, cell);
  if (walked.shape != ListShape::Bad) return false;
  raise_formal(machine, formal_type(machine, "list", cell), goal.name, goal.arity);
  return true;
}

// 変数の末尾は差分リストの本体である。束縛してから拒むと、収集の副作用が残る。
bool bad_tail(Impl& machine, const Goal& goal) {
  if (goal.arity != 4) return false;
  const Cell cell = machine.heap.deref(goal.arg(machine, 3));
  if (walk_list(machine, cell).shape != ListShape::Bad) return false;
  raise_formal(machine, formal_type(machine, "list", cell), goal.name, goal.arity);
  return true;
}

}  // namespace mosa::sol
