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

// 実行時のゴールのスロットがここを超えたら、残りの子のゴールを元の項のまま、動くときに訳す（Build::defers）。M28 では
// 16 ビットのスロットを溢れさせないためだった。32 ビットにした後も、動かない残りまで一度に訳すと IR が項の何倍にも
// なってヒープの上限で止まるので残す（M31 の Codex 敵対的レビュー）。値は M28 のまま。
constexpr std::size_t kDeferAt = 63488;

struct Build {
  const Heap* heap = nullptr;
  std::vector<std::uint64_t> refs;
  // 今たどっている複合項。節のデータの循環を匿名に切る。
  std::unordered_set<std::uint64_t> open;
  std::vector<Cell> env;
  // 今訳している制御構造（レビュー M13、M7）。浅い部分は goal_at の道、深い部分は goal_deep の枠。
  // 実行時のゴールは、巡回の戻り先を元の項を指す Var の葉にし、call/1 として動くときに訳す。巡回は無限に
  // 展開した項として動く。節は env を持たないので、戻り先ごとに新しい名前のない変数のゴールにする。
  std::vector<std::uint64_t> goal_path;
  std::unordered_set<std::uint64_t> deep_open;
  // 実行時のゴール。束縛された複合項の引数は、写さずに元の項を env から指す（レビュー M28）。木として写すと、
  // 共有の多い項は写しも組み立ても共有を広げた大きさになる。スロットは 32 ビットなので溢れない（ADR 0021）。
  bool keep = false;
  // ゴールの骨組みの節点ごとの、後で訳せないかの答え（Build::blocks）。同じ部分木を 2 度歩かない。
  std::unordered_map<std::uint64_t, std::uint8_t> skeleton;
  // 組む IR の節点の数の上限。実行時のゴールは compile_goal がヒープの上限で決める。節は決めない（レビュー M30）。
  std::size_t node_budget = SIZE_MAX;
  std::size_t nodes = 0;
  // 節の戻り先の制御構造と、そこに振った変数のスロット。name_slots はこのスロットに名前を付けない。
  std::unordered_map<std::uint64_t, std::uint32_t> backs;

  // refs の添字。実行時のゴールは複合項の引数ごとにスロットを振るので、長い連言を線形に探すと 2 乗になる
  // （6 万個で 8 倍遅い）。短いうちは線形のほうが速いので、kLinearRefs 個を超えてから作る。
  std::unordered_map<std::uint64_t, std::uint32_t> ref_index;
  static constexpr std::size_t kLinearRefs = 32;

  [[nodiscard]] std::uint32_t slot_of(Cell cell) {
    const auto raw = cell.raw();
    if (ref_index.empty()) {
      for (std::size_t index = 0; index < refs.size(); ++index) {
        if (refs[index] == raw) return static_cast<std::uint32_t>(index);
      }
    } else if (const auto found = ref_index.find(raw); found != ref_index.end()) {
      return found->second;
    }
    if (keep) env.push_back(cell);
    return push_ref(raw);
  }

  // 同じ raw が 2 度あれば、線形に探すときと同じく前のほうを返す。
  std::uint32_t push_ref(std::uint64_t raw) {
    const std::uint32_t slot = slot_number(refs.size());
    refs.push_back(raw);
    if (!ref_index.empty()) {
      ref_index.emplace(raw, slot);
    } else if (refs.size() > kLinearRefs) {
      for (std::size_t index = 0; index < refs.size(); ++index) {
        ref_index.emplace(refs[index], static_cast<std::uint32_t>(index));
      }
    }
    return slot;
  }

  // できたゴールの木は状態の語に数える（ADR 0010 の改訂 3）が、数えるのは組み終えた後なので、組む間に溢れさせない。
  // 共有する制御構造を木に広げるゴールは、ここで上限に止まる（M28 の Codex 敵対的レビュー 5 回目）。数えるのは
  // 安全点の tree_bytes と同じく args に確保する IrTerm で、データの枠と葉の根は確保する前に数える（10 回目。
  // 訪ねた節点を数えると、まだ歩かない子の分を先に確保して上限を超える）。
  void spend(std::size_t count) {
    nodes += count;
    if (nodes > node_budget) throw ResourceLimit("heap_limit");
  }

  [[nodiscard]] bool is_name(Cell cell, std::string_view name, std::uint16_t arity) const {
    cell = heap->deref(cell);
    if (arity == 0) return cell.tag() == Tag::Atom && heap->atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == name;
    if (cell.tag() != Tag::Str || heap->functor_arity(cell) != arity) return false;
    return heap->atoms().name(heap->functor_atom(cell)) == name;
  }

  [[nodiscard]] IrTerm data(Cell cell, bool shape = false);
  [[nodiscard]] IrTerm copy_data(Cell cell);
  [[nodiscard]] IrTerm place(Cell cell);
  [[nodiscard]] IrTerm place_root(Cell cell);
  [[nodiscard]] std::optional<IrTerm> data_leaf(Cell cell);
  [[nodiscard]] bool goal_loops(Cell cell) const;
  [[nodiscard]] IrTerm goal_back(Cell cell);
  [[nodiscard]] IrTerm goal_leaf(Cell cell);
  [[nodiscard]] IrTerm goal_deep(Cell cell);
  [[nodiscard]] IrTerm goal_at(Cell cell, int depth);
  [[nodiscard]] IrTerm goal(Cell cell);
  [[nodiscard]] IrTerm late_goal(Cell cell);
  [[nodiscard]] bool defers(Cell cell);
  [[nodiscard]] bool blocks(Cell root);
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

// 複合項の子の数。open_frame が args に確保する数でもある。
std::size_t child_count(const Heap& heap, Cell cell) { return cell.tag() == Tag::Lis ? 2 : heap.functor_arity(cell); }

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

// 複合項でない項と、節のデータの巡回の戻り先。たどる複合項は nullopt で、呼び手が枠を積む。
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
  // 節は env を持たないので、巡回は匿名に切る（巡回する節は 7.6.2 の外）。実行時のゴールはここに来ない（place）。
  term.tag = IrTag::Anon;
  return term;
}

// 実行時のゴールの束縛済みの複合項は、元の項を指す Var の葉にする。項は書き換わらず、中の変数も元のままなので、
// 写した項と同じに動く。巡回する項も元の項のまま渡すので、巡回が残る。
IrTerm Build::place(Cell cell) {
  cell = heap->deref(cell);
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return *data_leaf(cell);
  IrTerm term;
  term.tag = IrTag::Var;
  term.slot = slot_of(cell);
  return term;
}

// ゴールの葉の根は写し、引数から先を指す。根を Var にすると call/1 として動き、同じ項を訳し直して止まらない
// （レビュー M13）。
IrTerm Build::place_root(Cell root) {
  root = heap->deref(root);
  if (root.tag() != Tag::Lis && root.tag() != Tag::Str) return *data_leaf(root);
  spend(child_count(*heap, root));
  DataFrame frame = open_frame(*heap, root);
  for (std::size_t index = 0; index < frame.count; ++index) frame.term.args.push_back(place(child_of(*heap, root, index)));
  return std::move(frame.term);
}

// shape はゴールの葉の根。節は env を持たないので、データをすべて写す。
IrTerm Build::data(Cell root, bool shape) {
  if (!keep) return copy_data(root);
  return shape ? place_root(root) : place(root);
}

// 後順に組み立てる。長いリストや深い項でも C++ の呼び出しを積まないよう、途中の複合項を手で積む（ADR 0011）。
// 子は左から訳すので、変数の番号は前の再帰と同じ順に振られる。
IrTerm Build::copy_data(Cell root) {
  std::vector<DataFrame> stack;
  const auto enter = [&](Cell cell) -> std::optional<IrTerm> {
    cell = heap->deref(cell);
    if (auto leaf = data_leaf(cell)) return leaf;
    spend(child_count(*heap, cell));
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
    open.erase(top.cell.raw());
    IrTerm done = std::move(top.term);
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

// カットの効く位置の子（, ; -> *-> の両方と、M:G の G）。M:G は文脈のカットの壁を引き継ぐ（run_qualify）。
std::vector<Cell> skeleton_children(const Heap& heap, Cell cell) {
  if (is_connective(heap, cell)) return {heap.deref(heap.arg(cell, 0)), heap.deref(heap.arg(cell, 1))};
  if (cell.tag() == Tag::Str && heap.functor_arity(cell) == 2 && heap.atoms().name(heap.functor_atom(cell)) == ":") {
    return {heap.deref(heap.arg(cell, 1))};
  }
  return {};
}

// 骨組みの葉が後で訳せないか。! は call/1 の中では局所になる。未束縛の変数は今なら call(X) だが、後で訳すとその時の
// 束縛で分類が変わる。呼べない項は、call/1 が何も動かす前に出す type_error(callable, Goal)（7.8.3.3）が遅れる。
bool leaf_blocks(const Heap& heap, Cell cell) {
  if (heap.unbound(cell) || non_callable(heap, cell)) return true;
  return cell.tag() == Tag::Atom && heap.atoms().name(static_cast<std::uint32_t>(cell.raw() >> 3)) == "!";
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
  spend(1);
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

// 骨組みに後で訳せない葉があるか。後順に歩いて節点ごとに覚える。巡回する骨組みも後で訳せないとみなす。
bool Build::blocks(Cell root) {
  enum : std::uint8_t { kOpen, kFree, kBlocked };
  std::vector<std::pair<Cell, bool>> stack{{heap->deref(root), false}};
  while (!stack.empty()) {
    const auto [cell, expanded] = stack.back();
    if (!expanded) {
      if (skeleton.contains(cell.raw())) {
        stack.pop_back();
        continue;
      }
      const std::vector<Cell> children = skeleton_children(*heap, cell);
      if (children.empty()) {
        skeleton[cell.raw()] = leaf_blocks(*heap, cell) ? kBlocked : kFree;
        stack.pop_back();
        continue;
      }
      skeleton[cell.raw()] = kOpen;
      stack.back().second = true;
      for (const Cell child : children) stack.emplace_back(child, false);
      continue;
    }
    bool blocked = false;
    for (const Cell child : skeleton_children(*heap, cell)) blocked = blocked || skeleton[child.raw()] != kFree;
    skeleton[cell.raw()] = blocked ? kBlocked : kFree;
    stack.pop_back();
  }
  return skeleton[heap->deref(root).raw()] != kFree;
}

// 引数が原子だけの葉は今訳してもスロットを使わないので、後で訳すとかえって 1 つ使う。
bool slotless_leaf(const Heap& heap, Cell cell) {
  if (cell.tag() == Tag::Atom) return true;
  if (cell.tag() != Tag::Str) return false;
  for (std::uint16_t index = 0; index < heap.functor_arity(cell); ++index) {
    const Cell arg = heap.deref(heap.arg(cell, index));
    if (heap.unbound(arg) || arg.tag() == Tag::Str || arg.tag() == Tag::Lis) return false;
  }
  return true;
}

// ; の左の -> と *-> は、その IR の札で if-then-else になる（open_or）。Call に包むとただの選言になり、then が
// 失敗すると else が動く（M28 の Codex レビュー 4 回目）。中の条件と then は 1 つずつ後で訳せる。
bool tags_parent(const Heap& heap, const GoalFrame& frame, std::size_t index, Cell child) {
  child = heap.deref(child);
  if (frame.term.tag != IrTag::Or || index != 0 || !is_connective(heap, child)) return false;
  const auto name = heap.atoms().name(heap.functor_atom(child));
  return name == "->" || name == "*->";
}

// スロットが kDeferAt を超えたら、残りのゴールを元の項を指す call/1 の葉にし、動くときに新しいスロットで訳す（M28 の
// Codex 敵対的レビュー、M31 の Codex 敵対的レビュー）。動かない残りは訳さない。後で訳せない部分木は、その子ごとに決める。
bool Build::defers(Cell cell) {
  if (!keep || refs.size() < kDeferAt) return false;
  cell = heap->deref(cell);
  return !slotless_leaf(*heap, cell) && !blocks(cell);
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
  const auto [entry, fresh] = backs.try_emplace(cell.raw(), slot_number(refs.size()));
  if (fresh) push_ref(cell.raw());
  term.slot = entry->second;
  return term;
}

IrTerm Build::goal_deep(Cell root) {
  std::vector<GoalFrame> stack;
  spend(heap->functor_arity(root));
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
      } else if (!tags_parent(*heap, top, index, next) && defers(next)) {
        top.term.args.push_back(late_goal(next));
      } else if (auto frame = goal_frame(*heap, next)) {
        if (goal_loops(next)) {
          top.term.args.push_back(goal_back(next));
          continue;
        }
        deep_open.insert(next.raw());
        spend(frame->count);
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
  spend(frame->count);
  goal_path.push_back(cell.raw());
  for (std::size_t index = 0; index < frame->count; ++index) {
    const Cell next = heap->arg(cell, static_cast<std::uint16_t>(index));
    if (child_is_data(*frame, index)) frame->term.args.push_back(data(next));
    else if (runs_late(*heap, *frame, index, next)) frame->term.args.push_back(late_goal(next));
    else if (!tags_parent(*heap, *frame, index, next) && defers(next)) frame->term.args.push_back(late_goal(next));
    else frame->term.args.push_back(goal_at(next, depth + 1));
  }
  goal_path.pop_back();
  return std::move(frame->term);
}

IrTerm Build::goal(Cell cell) { return goal_at(cell, 0); }

// 子のゴールを持たないゴール。制御構造は呼び手が goal_frame で見分けて枠を作る。
// 葉は親の args の中にあるので、数えるのは葉の args だけ（M28 の Codex 敵対的レビュー 6 回目と 10 回目）。
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
    spend(1);
    IrTerm term;
    term.tag = IrTag::Throw;
    term.args.push_back(data(heap->arg(cell, 0)));
    return term;
  }
  if (is_name(cell, "halt", 0) || is_name(cell, "halt", 1)) {
    IrTerm term;
    term.tag = IrTag::Halt;
    if (heap->deref(cell).tag() == Tag::Str) {
      spend(1);
      term.args.push_back(data(heap->arg(cell, 0)));
    }
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

// 束縛された複合項の引数はどれも元の項を指すので、変数の無い引数ごとにもスロットが要る。スロットは 32 ビットなので、
// 前のように溢れたら写し直す段は要らない（ADR 0021）。
Compiled compile_goal(Heap& heap, Cell cell) {
  Build build;
  build.heap = &heap;
  build.keep = true;
  // 節点 1 つは親の args の IrTerm 1 つ。ヒープと状態の語は GC が空けうる（前に訳したゴールは、届かない extras が
  // 持ったままのことがある）ので引かない。引くと、収まるゴールが GC の時機で止まる（M28 の Codex 敵対的レビュー
  // 6 回目）。持った語は動いている収集の答えで、空かない。木だけで残りを超えれば、安全点（gc.cpp）も必ず止める。
  const std::size_t held = heap.held();
  const std::size_t left = held < heap.limit() ? heap.limit() - held : 0;
  build.node_budget = left / ((sizeof(IrTerm) + kHeapWordBytes - 1) / kHeapWordBytes);
  IrTerm term = build.goal(cell);
  Compiled compiled;
  compiled.term = make_run_goal(heap, std::move(term));
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
  clause.vars = slot_number(build.refs.size());
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
