#include "internal.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace mosa {
namespace {

struct Build {
  const Heap* heap = nullptr;
  std::vector<std::uint64_t> refs;
  // 今たどっている複合項。循環を匿名に切る。with_output_to が束縛済みの項を再び訳す。
  std::unordered_set<std::uint64_t> open;
  std::vector<Cell> env;
  // 実行時のゴールで巡回の戻り先になった複合項。写さずに元の項を指す。
  std::vector<std::uint64_t> loops;
  bool keep = false;

  [[nodiscard]] std::uint16_t slot_of(Cell cell) {
    const auto raw = cell.raw();
    for (std::size_t index = 0; index < refs.size(); ++index) {
      if (refs[index] == raw) return static_cast<std::uint16_t>(index);
    }
    refs.push_back(raw);
    if (keep) env.push_back(cell);
    return static_cast<std::uint16_t>(refs.size() - 1);
  }

  [[nodiscard]] bool is_name(Cell cell, std::string_view name, std::uint16_t arity) const {
    cell = heap->deref(cell);
    if (arity == 0) return cell.tag() == Tag::Atom && heap->atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == name;
    if (cell.tag() != Tag::Str || heap->functor_arity(cell) != arity) return false;
    return heap->atoms().name(heap->functor_atom(cell)) == name;
  }

  [[nodiscard]] IrTerm data(Cell cell);
  [[nodiscard]] std::optional<IrTerm> data_leaf(Cell cell);
  [[nodiscard]] IrTerm close_data(Cell cell, IrTerm term);
  [[nodiscard]] IrTerm goal_leaf(Cell cell);
  [[nodiscard]] IrTerm goal_deep(Cell cell);
  [[nodiscard]] IrTerm goal_at(Cell cell, int depth);
  [[nodiscard]] IrTerm goal(Cell cell);
};

// 組み立て途中の制御構造。子のゴール（catch/3 の 2 番目だけはデータ）を term.args に集め終えたら閉じる。
struct GoalFrame {
  Cell cell;
  IrTerm term;
  std::uint16_t count = 0;
};

// この深さまでは C++ の再帰で訳す。より深い制御構造は手で積んで訳す（ADR 0011 の改訂 2）。
constexpr int kShallow = 64;

// 組み立て途中の複合項。子の IrTerm を term.args に集め終えたら閉じる（emit_head.cpp の PlaceFrame と同じ形）。
struct DataFrame {
  Cell cell;
  IrTerm term;
  std::uint16_t count = 0;
};

// 複合項の頭だけを作る。'.'/2 はリストそのもの。構造体のままにすると = と == が食い違う。
DataFrame open_frame(const Heap& heap, Cell cell) {
  DataFrame frame{cell, IrTerm{}, 2};
  const bool dot = cell.tag() == Tag::Str && heap.functor_arity(cell) == 2 &&
                   heap.atoms().name(heap.functor_atom(cell)) == ".";
  if (cell.tag() == Tag::Lis || dot) {
    frame.term.tag = IrTag::List;
    frame.term.args.reserve(2);
    return frame;
  }
  frame.term.tag = IrTag::Fun;
  frame.term.atom = heap.functor_atom(cell);
  frame.term.arity = heap.functor_arity(cell);
  frame.term.args.reserve(frame.term.arity);
  frame.count = frame.term.arity;
  return frame;
}

Cell child_of(const Heap& heap, Cell cell, std::size_t index) {
  if (cell.tag() == Tag::Lis) return index == 0 ? heap.list_head(cell) : heap.list_tail(cell);
  return heap.arg(cell, static_cast<std::uint16_t>(index));
}

// 複合項でない項と、巡回の戻り先。たどる複合項は nullopt で、呼び手が枠を積む。
std::optional<IrTerm> Build::data_leaf(Cell cell) {
  IrTerm term;
  if (heap->unbound(cell)) {
    term.tag = IrTag::Var;
    term.slot = slot_of(cell);
    return term;
  }
  if (cell.tag() == Tag::Atom) {
    term.tag = IrTag::Atom;
    term.atom = static_cast<std::uint32_t>(cell.raw() >> 3);
    return term;
  }
  if (cell.tag() == Tag::Int || cell.tag() == Tag::IntBox) {
    term.tag = IrTag::Int;
    term.integer = heap->integer_value(cell);
    return term;
  }
  if (cell.tag() == Tag::Float) {
    term.tag = IrTag::Float;
    term.floating = heap->float_value(cell);
    return term;
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return term;
  if (!open.contains(cell.raw())) return std::nullopt;
  // 実行時のゴールは元の項を指せば巡回が残る。匿名に切ると巡回リストが部分リストになる。
  // 節は env を持たないので匿名のまま（巡回する節は 7.6.2 の外）。
  if (keep) {
    loops.push_back(cell.raw());
    term.tag = IrTag::Var;
    term.slot = slot_of(cell);
    return term;
  }
  term.tag = IrTag::Anon;
  return term;
}

IrTerm Build::close_data(Cell cell, IrTerm term) {
  open.erase(cell.raw());
  // 巡回の入口まで写すと、元の項より一段長い項になる。入口は元の項そのものを渡す。
  if (keep && std::find(loops.begin(), loops.end(), cell.raw()) != loops.end()) {
    term = IrTerm{};
    term.tag = IrTag::Var;
    term.slot = slot_of(cell);
  }
  return term;
}

// 後順に組み立てる。長いリストや深い項でも C++ の呼び出しを積まないよう、途中の複合項を手で積む（ADR 0011）。
// 子は左から訳すので、変数の番号は前の再帰と同じ順に振られる。
IrTerm Build::data(Cell root) {
  std::vector<DataFrame> stack;
  const auto enter = [&](Cell cell) -> std::optional<IrTerm> {
    cell = heap->deref(cell);
    if (auto leaf = data_leaf(cell)) return leaf;
    open.insert(cell.raw());
    stack.push_back(open_frame(*heap, cell));
    return std::nullopt;
  };
  if (auto leaf = enter(root)) return std::move(*leaf);
  while (true) {
    DataFrame& top = stack.back();
    if (top.term.args.size() < top.count) {
      const Cell next = child_of(*heap, top.cell, top.term.args.size());
      // enter が枠を積むと top は無効になるので、積まなかったときだけ書く。
      if (auto leaf = enter(next)) stack.back().term.args.push_back(std::move(*leaf));
      continue;
    }
    IrTerm done = close_data(top.cell, std::move(top.term));
    stack.pop_back();
    if (stack.empty()) return done;
    stack.back().term.args.push_back(std::move(done));
  }
}

// 子のゴールを持つ制御構造（, ; -> *-> catch/3）なら、子を除いた頭の枠を返す。
std::optional<GoalFrame> goal_frame(const Heap& heap, Cell cell) {
  if (cell.tag() != Tag::Str) return std::nullopt;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  GoalFrame frame{cell, IrTerm{}, arity};
  if (arity == 2 && name == ",") frame.term.tag = IrTag::And;
  else if (arity == 2 && name == ";") frame.term.tag = IrTag::Or;
  else if (arity == 2 && name == "->") frame.term.tag = IrTag::IfThen;
  else if (arity == 2 && name == "*->") frame.term.tag = IrTag::SoftIf;
  else if (arity == 3 && name == "catch") frame.term.tag = IrTag::Catch;
  else return std::nullopt;
  frame.term.args.reserve(arity);
  return frame;
}

// catch/3 の 2 番目は捕まえる型であり、ゴールではない。
bool child_is_data(const GoalFrame& frame, std::size_t index) {
  return frame.term.tag == IrTag::Catch && index == 1;
}

// 後順に訳す。子は左から訳すので、変数の番号は浅いときの再帰と同じ順に振られる。
IrTerm Build::goal_deep(Cell root) {
  std::vector<GoalFrame> stack;
  stack.push_back(*goal_frame(*heap, root));
  while (true) {
    GoalFrame& top = stack.back();
    const std::size_t index = top.term.args.size();
    if (index < top.count) {
      const Cell next = heap->deref(heap->arg(top.cell, static_cast<std::uint16_t>(index)));
      if (child_is_data(top, index)) {
        top.term.args.push_back(data(next));
      } else if (auto frame = goal_frame(*heap, next)) {
        // 積むと top は無効になる。
        stack.push_back(std::move(*frame));
      } else {
        top.term.args.push_back(goal_leaf(next));
      }
      continue;
    }
    IrTerm done = std::move(top.term);
    stack.pop_back();
    if (stack.empty()) return done;
    stack.back().term.args.push_back(std::move(done));
  }
}

IrTerm Build::goal_at(Cell cell, int depth) {
  cell = heap->deref(cell);
  auto frame = goal_frame(*heap, cell);
  if (!frame) return goal_leaf(cell);
  if (depth >= kShallow) return goal_deep(cell);
  for (std::size_t index = 0; index < frame->count; ++index) {
    const Cell next = heap->arg(cell, static_cast<std::uint16_t>(index));
    frame->term.args.push_back(child_is_data(*frame, index) ? data(next) : goal_at(next, depth + 1));
  }
  return std::move(frame->term);
}

IrTerm Build::goal(Cell cell) { return goal_at(cell, 0); }

// 子のゴールを持たないゴール。制御構造は呼び手が goal_frame で見分けて枠を作る。
IrTerm Build::goal_leaf(Cell cell) {
  if (is_name(cell, "true", 0)) {
    IrTerm term;
    term.tag = IrTag::True;
    return term;
  }
  if (is_name(cell, "fail", 0) || is_name(cell, "false", 0)) {
    IrTerm term;
    term.tag = IrTag::Fail;
    return term;
  }
  if (is_name(cell, "!", 0)) {
    IrTerm term;
    term.tag = IrTag::Cut;
    return term;
  }
  if (is_name(cell, "throw", 1)) {
    IrTerm term;
    term.tag = IrTag::Throw;
    term.args.push_back(data(heap->arg(cell, 0)));
    return term;
  }
  if (is_name(cell, "halt", 0) || is_name(cell, "halt", 1)) {
    IrTerm term;
    term.tag = IrTag::Halt;
    if (heap->deref(cell).tag() == Tag::Str) term.args.push_back(data(heap->arg(cell, 0)));
    return term;
  }
  return data(cell);
}

void set_key(Clause& clause) {
  if (clause.head.tag != IrTag::Fun || clause.head.args.empty()) {
    clause.key = KeyKind::None;
    return;
  }
  const IrTerm& first = clause.head.args[0];
  if (first.tag == IrTag::Var || first.tag == IrTag::Anon) clause.key = KeyKind::Var;
  else if (first.tag == IrTag::Atom) {
    clause.key = KeyKind::Atom;
    clause.key_atom = first.atom;
  } else if (first.tag == IrTag::Int) {
    clause.key = KeyKind::Int;
    clause.key_int = first.integer;
  } else if (first.tag == IrTag::Float) {
    clause.key = KeyKind::Float;
    clause.key_float = first.floating;
  } else if (first.tag == IrTag::List) clause.key = KeyKind::List;
  else if (first.tag == IrTag::Fun) {
    clause.key = KeyKind::Fun;
    clause.key_atom = first.atom;
    clause.key_arity = first.arity;
  } else {
    clause.key = KeyKind::Any;
  }
}

// 名前はスロットに写すだけで、IR には入れない。読み込みの変数名が節の意味を変えないため。
void name_slots(const Heap& heap, const Build& build, std::span<const VarName> names, Clause& clause) {
  if (names.empty()) return;
  clause.names.assign(build.refs.size(), std::string{});
  for (const VarName& name : names) {
    const auto raw = heap.deref(name.cell).raw();
    for (std::size_t index = 0; index < build.refs.size(); ++index) {
      if (build.refs[index] == raw) clause.names[index] = name.name;
    }
  }
}

}  // namespace

Compiled compile_goal(const Heap& heap, Cell cell) {
  Build build;
  build.heap = &heap;
  build.keep = true;
  Compiled compiled;
  compiled.term = build.goal(cell);
  compiled.env = std::move(build.env);
  return compiled;
}

bool dot_head(const Heap& heap, Cell head, Cell& left, Cell& right) {
  head = heap.deref(head);
  if (head.tag() == Tag::Lis) {
    left = heap.list_head(head);
    right = heap.list_tail(head);
    return true;
  }
  if (head.tag() == Tag::Str && heap.functor_arity(head) == 2 && heap.atoms().name(heap.functor_atom(head)) == ".") {
    left = heap.arg(head, 0);
    right = heap.arg(head, 1);
    return true;
  }
  return false;
}

void set_dot_head(Build& build, const Heap& heap, Clause& clause, Cell left, Cell right) {
  // 頭の '.'/2 はデータとしてのリストではなく、述語 '.'/2 である。
  clause.head.tag = IrTag::Fun;
  clause.head.atom = heap.atoms().intern(".");
  clause.head.arity = 2;
  clause.head.args.push_back(build.data(left));
  clause.head.args.push_back(build.data(right));
}

bool compile_clause(const Heap& heap, Cell cell, Clause& clause, std::span<const VarName> names) {
  Build build;
  build.heap = &heap;
  Cell head = heap.deref(cell);
  IrTerm body;
  body.tag = IrTag::True;
  if (build.is_name(head, ":-", 2)) {
    body = build.goal(heap.arg(head, 1));
    head = heap.deref(heap.arg(head, 0));
  }
  Cell left;
  Cell right;
  if (dot_head(heap, head, left, right)) {
    set_dot_head(build, heap, clause, left, right);
  } else if (heap.unbound(head) || head.tag() == Tag::Int || head.tag() == Tag::IntBox ||
             head.tag() == Tag::Float) {
    return false;
  } else {
    clause.head = build.data(head);
  }
  clause.body = body.tag == IrTag::True && body.args.empty() ? body : body;
  if (build.is_name(heap.deref(cell), ":-", 2)) clause.body = body;
  clause.vars = static_cast<std::uint16_t>(build.refs.size());
  name_slots(heap, build, names, clause);
  if (clause.head.tag == IrTag::Atom) {
    clause.head.arity = 0;
  }
  set_key(clause);
  return clause.head.tag == IrTag::Atom || clause.head.tag == IrTag::Fun;
}

bool is_directive(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  return name == ":-" || name == "?-";
}

}
