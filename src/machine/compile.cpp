#include "internal.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
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
  // 実行時のゴールで巡回の戻り先になった複合項。写さずに元の項を指す。葉ごとに足すので集合にする（レビュー M13）。
  std::unordered_set<std::uint64_t> loops;
  // 今訳している制御構造（レビュー M13、M7）。浅い部分は goal_at の道、深い部分は goal_deep の枠。
  // 実行時のゴールは、巡回の戻り先を元の項を指す Var の葉にし、call/1 として動くときに訳す。巡回は無限に
  // 展開した項として動く。節は env を持たないので、戻り先ごとに新しい名前のない変数のゴールにする。
  std::vector<std::uint64_t> goal_path;
  std::unordered_set<std::uint64_t> deep_open;
  bool keep = false;
  // 節の戻り先の制御構造と、そこに振った変数のスロット。name_slots はこのスロットに名前を付けない。
  std::unordered_map<std::uint64_t, std::uint16_t> backs;

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

  [[nodiscard]] IrTerm data(Cell cell, bool shape = false);
  [[nodiscard]] std::optional<IrTerm> data_leaf(Cell cell);
  [[nodiscard]] IrTerm close_data(Cell cell, IrTerm term, bool shape);
  [[nodiscard]] bool goal_loops(Cell cell) const;
  [[nodiscard]] IrTerm goal_back(Cell cell);
  [[nodiscard]] IrTerm goal_leaf(Cell cell);
  [[nodiscard]] IrTerm goal_deep(Cell cell);
  [[nodiscard]] IrTerm goal_at(Cell cell, int depth);
  [[nodiscard]] IrTerm goal(Cell cell);
  [[nodiscard]] IrTerm late_goal(Cell cell);
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
  // 訳している途中の制御構造は元の項を指す。葉の引数ごとに巡回するゴール全体を写すと 2 乗以上になる（レビュー M13）。
  // 節では元の項を指せないので、データとして写し、その中の巡回を下で匿名に切る。
  if (keep && goal_loops(cell)) {
    term.tag = IrTag::Var;
    term.slot = slot_of(cell);
    return term;
  }
  if (!open.contains(cell.raw())) return std::nullopt;
  // 実行時のゴールは元の項を指せば巡回が残る。匿名に切ると巡回リストが部分リストになる。
  // 節は env を持たないので匿名のまま（巡回する節は 7.6.2 の外）。
  if (keep) {
    loops.insert(cell.raw());
    term.tag = IrTag::Var;
    term.slot = slot_of(cell);
    return term;
  }
  term.tag = IrTag::Anon;
  return term;
}

// shape はゴールの葉の根。根を Var にすると call(X) として動き、同じ項を訳し直して止まらない（レビュー M13）。
IrTerm Build::close_data(Cell cell, IrTerm term, bool shape) {
  open.erase(cell.raw());
  // 巡回の入口まで写すと、元の項より一段長い項になる。入口は元の項そのものを渡す。
  if (!shape && keep && loops.contains(cell.raw())) {
    term = IrTerm{};
    term.tag = IrTag::Var;
    term.slot = slot_of(cell);
  }
  return term;
}

// 後順に組み立てる。長いリストや深い項でも C++ の呼び出しを積まないよう、途中の複合項を手で積む（ADR 0011）。
// 子は左から訳すので、変数の番号は前の再帰と同じ順に振られる。
IrTerm Build::data(Cell root, bool shape) {
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
    IrTerm done = close_data(top.cell, std::move(top.term), shape && stack.size() == 1);
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

bool is_connective(const Heap& heap, Cell cell) {
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 2) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  return name == "," || name == ";" || name == "->" || name == "*->";
}

// 数はゴールにならない（7.6.2）。変数は呼べるかどうかが束縛で決まるので、ここでは数えない。
bool non_callable(const Heap& heap, Cell cell) {
  const Tag tag = cell.tag();
  return !heap.unbound(cell) && tag != Tag::Atom && tag != Tag::Str && tag != Tag::Lis;
}

// , ; -> *-> の下のゴール位置に、未束縛の変数か呼べない項があるか（7.6.2 が変換するのはこの 4 つだけ）。
// 入れ子の catch/3 は自分で決めるので入らない。見た節点を覚え、巡回するゴールでもこの歩き手は止まる
// （ゴールの翻訳はレビュー M13）。
bool has_late_leaf(const Heap& heap, Cell root) {
  std::vector<Cell> pending{root};
  std::unordered_set<std::uint64_t> seen;
  while (!pending.empty()) {
    const Cell cell = heap.deref(pending.back());
    pending.pop_back();
    if (heap.unbound(cell) || non_callable(heap, cell)) return true;
    if (!is_connective(heap, cell) || !seen.insert(cell.raw()).second) continue;
    pending.push_back(heap.arg(cell, 1));
    pending.push_back(heap.arg(cell, 0));
  }
  return false;
}

// ADR 0014。catch/3 の Goal と Recovery は catch が動くときに call/1 で訳す（7.8.9.1）。ゴール位置の変数が
// そのとき ! なら、そのゴールに閉じたカットになる。今訳すと call(X) の壁になるので、データのまま Call にする。
// 変数 1 つだけのゴールは Var の葉が同じく call(X) として動くので、今のまま訳す。
// 呼べない部分のあるゴールも Call にする（レビュー M15）。call/1 が何も動かす前に type_error(callable, Goal) を
// 出す（7.8.3.3）。今訳すと前のゴールが先に動き、外の call/1 や assertz/1 が catch/3 ごと拒む。
bool runs_late(const Heap& heap, const GoalFrame& frame, std::size_t index, Cell child) {
  if (frame.term.tag != IrTag::Catch || index == 1) return false;
  child = heap.deref(child);
  if (non_callable(heap, child)) return true;
  return is_connective(heap, child) && has_late_leaf(heap, child);
}

// call/1 の葉。clause/2 は Call を中の項として見せる（clause_term.cpp の expose_start）。
// 実行時のゴールは元の項を env から指す。写すと、入れ子の catch が段ごとに残り全体を写して 2 乗になる。
IrTerm Build::late_goal(Cell cell) {
  IrTerm term;
  term.tag = IrTag::Call;
  term.atom = heap->atoms().intern("call");
  term.arity = 1;
  if (keep) {
    IrTerm source;
    source.tag = IrTag::Var;
    source.slot = slot_of(heap->deref(cell));
    term.args.push_back(std::move(source));
  } else {
    term.args.push_back(data(cell));
  }
  return term;
}

// 後順に訳す。子は左から訳すので、変数の番号は浅いときの再帰と同じ順に振られる。
bool Build::goal_loops(Cell cell) const {
  if (goal_path.empty() && deep_open.empty()) return false;
  return deep_open.contains(cell.raw()) || std::find(goal_path.begin(), goal_path.end(), cell.raw()) != goal_path.end();
}

// 節の戻り先は、頭や引数のデータの巡回を匿名に切るのと同じく、名前のない変数のゴールに切る（オーナー決定 A、
// iso-notes 7.3.3）。変数は戻り先の制御構造ごとに 1 つ。ゴールの位置にしか現れず誰も束縛しないので、
// 共有しても動きは同じで、戻り辺ごとに振ると共有の多い本体でスロットが 2^n 個要る（M7 の敵対的レビュー A2）。
// refs には戻り先の複合項の raw を置く。束縛されていない変数の raw とはタグが違うので slot_of は取り違えない。
IrTerm Build::goal_back(Cell cell) {
  IrTerm term;
  term.tag = IrTag::Var;
  if (keep) {
    term.slot = slot_of(cell);
    return term;
  }
  const auto [entry, fresh] = backs.try_emplace(cell.raw(), static_cast<std::uint16_t>(refs.size()));
  if (fresh) refs.push_back(cell.raw());
  term.slot = entry->second;
  return term;
}

IrTerm Build::goal_deep(Cell root) {
  std::vector<GoalFrame> stack;
  stack.push_back(*goal_frame(*heap, root));
  deep_open.insert(root.raw());
  while (true) {
    GoalFrame& top = stack.back();
    const std::size_t index = top.term.args.size();
    if (index < top.count) {
      const Cell next = heap->deref(heap->arg(top.cell, static_cast<std::uint16_t>(index)));
      if (child_is_data(top, index)) {
        top.term.args.push_back(data(next));
      } else if (runs_late(*heap, top, index, next)) {
        top.term.args.push_back(late_goal(next));
      } else if (auto frame = goal_frame(*heap, next)) {
        if (goal_loops(next)) {
          top.term.args.push_back(goal_back(next));
          continue;
        }
        deep_open.insert(next.raw());
        // 積むと top は無効になる。
        stack.push_back(std::move(*frame));
      } else {
        top.term.args.push_back(goal_leaf(next));
      }
      continue;
    }
    deep_open.erase(top.cell.raw());
    IrTerm done = std::move(top.term);
    stack.pop_back();
    if (stack.empty()) return done;
    stack.back().term.args.push_back(std::move(done));
  }
}

// 浅い道は高々 kShallow 個なので、集合ではなく線形に探す。ふつうの call/1 の費用を増やさないため。
IrTerm Build::goal_at(Cell cell, int depth) {
  cell = heap->deref(cell);
  auto frame = goal_frame(*heap, cell);
  if (!frame) return goal_leaf(cell);
  if (goal_loops(cell)) return goal_back(cell);
  if (depth >= kShallow) return goal_deep(cell);
  goal_path.push_back(cell.raw());
  for (std::size_t index = 0; index < frame->count; ++index) {
    const Cell next = heap->arg(cell, static_cast<std::uint16_t>(index));
    if (child_is_data(*frame, index)) frame->term.args.push_back(data(next));
    else if (runs_late(*heap, *frame, index, next)) frame->term.args.push_back(late_goal(next));
    else frame->term.args.push_back(goal_at(next, depth + 1));
  }
  goal_path.pop_back();
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
  return data(cell, true);
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
    // 戻り先の制御構造に束縛された名前は、その戻り先の変数の名前ではない。
    if (build.backs.contains(raw)) continue;
    for (std::size_t index = 0; index < build.refs.size(); ++index) {
      if (build.refs[index] == raw) clause.names[index] = name.name;
    }
  }
}

}  // namespace

Compiled compile_goal(Heap& heap, Cell cell) {
  Build build;
  build.heap = &heap;
  build.keep = true;
  Compiled compiled;
  compiled.term = make_run_goal(heap, build.goal(cell));
  compiled.env = make_env(heap, std::move(build.env));
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

namespace {

GoalRef fact_body() {
  static const GoalRef body = [] {
    IrTerm term;
    term.tag = IrTag::True;
    return make_goal(std::move(term));
  }();
  return body;
}

}  // namespace

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
  // 事実の本体はどの節も同じ true なので、1 つを共有して節ごとに確保しない。
  clause.body = body.tag == IrTag::True && body.args.empty() ? fact_body() : make_goal(std::move(body));
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
