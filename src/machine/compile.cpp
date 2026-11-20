#include "compile_build.hpp"

#include "mosa/bigint.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mosa {

using ir_build::Build;

namespace {

// 実行時のゴールのスロットがここを超えたら、残りの子のゴールを元の項のまま、動くときに訳す（Build::defers）。M28 では
// 16 ビットのスロットを溢れさせないためだった。32 ビットにした後も、動かない残りまで一度に訳すと IR が項の何倍にも
// なってヒープの上限で止まるので残す（M31 の Codex 敵対的レビュー）。値は M28 のまま。
constexpr std::size_t kDeferAt = 63488;

// 葉の中のゴールをたどる歩みの上限（Build::wraps_back）。これより長い周期の、葉だけの巡回は前のとおり動く。
constexpr std::size_t kWrapWalk = 63;

// 組み立て途中の制御構造。子のゴール（catch/3 の 2 番目だけはデータ）を term.args に集め終えたら閉じる。
struct GoalFrame {
  Cell cell;
  IrTerm term;
  std::uint16_t count = 0;
};

// この深さまでは C++ の再帰で訳す。より深い制御構造は手で積んで訳す（ADR 0011 の改訂 2）。
constexpr int kShallow = 64;

// 子のゴールを持つ制御構造（, ; -> *-> catch/3）なら、その IR の札。
std::optional<IrTag> frame_tag(const Heap& heap, Cell cell) {
  if (cell.tag() != Tag::Str) return std::nullopt;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (arity == 2 && name == ",") return IrTag::And;
  if (arity == 2 && name == ";") return IrTag::Or;
  if (arity == 2 && name == "->") return IrTag::IfThen;
  if (arity == 2 && name == "*->") return IrTag::SoftIf;
  if (arity == 3 && name == "catch") return IrTag::Catch;
  return std::nullopt;
}

// 子のゴールを持つ制御構造なら、子を除いた頭の枠を返す。
std::optional<GoalFrame> goal_frame(const Heap& heap, Cell cell) {
  const auto tag = frame_tag(heap, cell);
  if (!tag) return std::nullopt;
  GoalFrame frame{cell, IrTerm{}, heap.functor_arity(cell)};
  frame.term.tag = *tag;
  frame.term.args.reserve(frame.count);
  return frame;
}

// 動くときに中のゴールを訳す葉（\+ G、once(G)、call(G, ...)、M:G）なら、その G。巡回を見る Build::wraps_back が使う。
// call/9 からは利用者の述語なので、第 1 引数はデータである。'$not'/1 と '$once'/1 は forall/2 と setup_call_cleanup/3 が
// 積む (\+)/1 と once/1 の別名（ADR 0028）なので同じに辿る。辿らないと、外の訳が巡回を見逃し、Cond や Setup の後で
// 内側の別名がやっと誤りにする（iso-notes 7.3.3 の決定 A は何も動かす前）。
bool wraps_first(std::string_view name, std::uint16_t arity) {
  const bool call = name == "call" && arity <= kCallArityMax;
  const bool negation = name == "\\+" || name == "$not";
  const bool once = name == "once" || name == "$once";
  return (arity == 1 && (negation || once)) || call;
}

std::optional<Cell> wrapped_goal(const Heap& heap, Cell cell) {
  if (cell.tag() != Tag::Str) return std::nullopt;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  if (arity == 2 && name == ":") return heap.deref(heap.arg(cell, 1));
  if (wraps_first(name, arity)) return heap.deref(heap.arg(cell, 0));
  return std::nullopt;
}

// catch/3 の 2 番目は捕まえる型であり、ゴールではない。
bool child_is_data(IrTag parent, std::size_t index) { return parent == IrTag::Catch && index == 1; }

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

// 後で訳すかを決める歩み（Build::blocks）の節点の子。, ; -> *-> の両方、葉が動くときに訳す中のゴール（M:G の G と
// wrapped_goal の G）、catch/3 の Goal と Recovery。skeletal はカットの効く位置の子（, ; -> *-> と M:G。M:G は文脈の
// カットの壁を引き継ぐ、run_qualify）で、その子だけが葉の答えを親へ渡し、ほかの子は巡回だけを渡す（レビュー H-08）。
// if/3 の引数には入らない（iso-notes 7.3.3 の決定 B）。動いている if/3 へ戻る辺は葉で見る。子は多くて 2 つなので配列に
// 置く。節点ごとに vector を確保すると、100 万個の連言の実行が 1.3 秒から 1.8 秒に延びた。
struct GoalKids {
  std::array<Cell, 2> cells{};
  std::uint8_t count = 0;
  bool skeletal = false;
};

GoalKids goal_children(const Heap& heap, Cell cell) {
  if (cell.tag() != Tag::Str) return {};
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  const auto arity = heap.functor_arity(cell);
  const auto arg = [&](std::uint16_t index) { return heap.deref(heap.arg(cell, index)); };
  if (arity == 2 && (name == "," || name == ";" || name == "->" || name == "*->")) return {{arg(0), arg(1)}, 2, true};
  if (arity == 2 && name == ":") return {{arg(1), Cell{}}, 1, true};
  if (arity == 3 && name == "catch") return {{arg(0), arg(2)}, 2, false};
  if (wraps_first(name, arity)) return {{arg(0), Cell{}}, 1, false};
  return {};
}

// Build::blocks が節点ごとに覚える答え。kOpen は歩いている道の上。kLeaf は骨組みに後で訳せない葉がある。kCycle は巡回に
// 届く。0 なら後で訳してよい。
enum : std::uint8_t { kOpen = 1, kLeaf = 2, kCycle = 4 };

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
bool runs_late(const Heap& heap, IrTag parent, std::size_t index, Cell child) {
  if (parent != IrTag::Catch || index == 1) return false;
  child = heap.deref(child);
  if (non_callable(heap, child)) return true;
  return is_connective(heap, child) && has_late_leaf(heap, child);
}

// 引数が原子だけの葉は今訳してもスロットを使わないので、後で訳すとかえって 1 つ使う。Big の引数は複合項と同じく
// 元の箱を指すスロットを使う（Build::place）。
bool slotless_leaf(const Heap& heap, Cell cell) {
  if (cell.tag() == Tag::Atom) return true;
  if (cell.tag() != Tag::Str) return false;
  for (std::uint16_t index = 0; index < heap.functor_arity(cell); ++index) {
    const Cell arg = heap.deref(heap.arg(cell, index));
    if (heap.unbound(arg) || arg.tag() == Tag::Str || arg.tag() == Tag::Lis || arg.tag() == Tag::Big) return false;
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

}  // namespace

namespace ir_build {

// call/1 の葉。clause/2 は Call を中の項として見せる（clause_term.cpp の expose_start）。
// 実行時のゴールは元の項を env から指す。写すと、入れ子の catch が段ごとに残り全体を写して 2 乗になる。
IrTerm Build::late_goal(Cell cell) {
  note_back(heap->deref(cell));
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

// レビュー H-09（2026-11-21、持ち主の B）。catch/3 の後で訳す Goal と Recovery を、その場で訳すときと同じく今の道の上で
// 歩いてから葉にする。根だけを見ると、catch を通って戻る巡回は catch が動いて Goal を訳すときに見つかり、その catch が
// 受けた（iso-notes 7.3.3 の決定 A は何も動かす前）。
IrTerm Build::late_catch(Cell cell) {
  if (keep) probe_late(heap->deref(cell));
  return late_goal(cell);
}

// goal_at と同じ制御構造を歩き、同じく枠は goal_loops で、葉は note_back で見る。歩いている枠は deep_open に置き、
// 抜けるときに外す。入れ子の後で訳す Goal は根だけを見る。中まで歩くと、その Goal を訳す段の翻訳もまた下の段を
// すべて歩くので、入れ子の後で訳す catch が段の数の 2 乗になる（tests/stack/deep_builtins.pl の 100 万段）。歩く節点は
// node_budget までにする。その場で訳すときに組む IR の節点の数と同じだけで、超えたら見つけずに終える。
void Build::probe_late(Cell root) {
  struct Step {
    Cell cell;
    IrTag tag;
    std::uint16_t index;
  };
  const auto root_tag = frame_tag(*heap, root);
  if (!root_tag || goal_loops(root)) return;
  std::vector<Step> stack{{root, *root_tag, 0}};
  deep_open.insert(root.raw());
  for (std::size_t steps = 0; !stack.empty() && !cyclic && steps < node_budget; ++steps) {
    Step& top = stack.back();
    if (top.index == heap->functor_arity(top.cell)) {
      deep_open.erase(top.cell.raw());
      stack.pop_back();
      continue;
    }
    const std::uint16_t index = top.index++;
    const IrTag parent = top.tag;
    const Cell child = heap->deref(heap->arg(top.cell, index));
    if (child_is_data(parent, index)) continue;
    const auto tag = frame_tag(*heap, child);
    if (!tag || runs_late(*heap, parent, index, child)) {
      note_back(child);
    } else if (goal_loops(child)) {
      cyclic = true;
    } else {
      deep_open.insert(child.raw());
      // 積むと top は無効になる。
      stack.push_back({child, *tag, 0});
    }
  }
  for (const Step& step : stack) deep_open.erase(step.cell.raw());
}

// 後で訳せない部分木か。骨組みに後で訳せない葉があるか、葉の中のゴールも通って巡回に届けば塞がる（巡回はレビュー H-08）。
// 後順に歩いて節点ごとに覚え、同じ部分木を 2 度歩かない。開いたままの節点（歩いている道の上）へ戻る子は巡回である。道の
// 上の祖先へ戻る巡回も、祖先からこの部分木へ下りた辺を通って部分木へ戻るので、答えは道によらず、後の呼び出しが使ってよい。
// 骨組みと巡回は 1 度の歩みで決める。分けると後で訳す回ごとに残りを 2 度歩き、100 万個の連言が 1.6 秒から 2.8 秒になった。
bool Build::blocks(Cell root) {
  // 子を歩き終えた節点（子の無い葉も）の答え。
  const auto settle = [this](Cell cell, const GoalKids& kids) {
    if (kids.count == 0) {
      return static_cast<std::uint8_t>((leaf_blocks(*heap, cell) ? kLeaf : 0) | (runs_back(cell) ? kCycle : 0));
    }
    std::uint8_t state = 0;
    for (std::uint8_t index = 0; index < kids.count; ++index) {
      const std::uint8_t seen = skeleton[kids.cells[index].raw()];
      if (seen == kOpen || (seen & kCycle) != 0) state |= kCycle;
      if (kids.skeletal && (seen & kLeaf) != 0) state |= kLeaf;
    }
    return state;
  };
  std::vector<std::pair<Cell, bool>> stack{{heap->deref(root), false}};
  while (!stack.empty()) {
    const auto [cell, expanded] = stack.back();
    if (!expanded && skeleton.contains(cell.raw())) {
      stack.pop_back();
      continue;
    }
    const GoalKids kids = goal_children(*heap, cell);
    if (expanded || kids.count == 0) {
      skeleton[cell.raw()] = settle(cell, kids);
      stack.pop_back();
      continue;
    }
    skeleton[cell.raw()] = kOpen;
    stack.back().second = true;
    for (std::uint8_t index = 0; index < kids.count; ++index) stack.emplace_back(kids.cells[index], false);
  }
  return skeleton[heap->deref(root).raw()] != 0;
}

// スロットが kDeferAt を超えたら、残りのゴールを元の項を指す call/1 の葉にし、動くときに新しいスロットで訳す（M28 の
// Codex 敵対的レビュー、M31 の Codex 敵対的レビュー）。動かない残りは訳さない。後で訳せない部分木は、その子ごとに決める。
// 巡回に届く部分木もその場で訳す。後で訳すと新しい道から訳し始めるので、call/N や catch/3 の中を通って戻る辺が見えず、
// 何度でも動いた（レビュー H-08）。その場で訳せば、ゴールが小さいときと同じ所で巡回の誤りになる。
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

// 葉の G をたどり、今訳している制御構造に戻るか、葉どうしで回れば巡回（X = call(X)、X = user:X、周期 2 の
// X = call(Y), Y = call(X)）。G が制御構造なら止める。その中から戻る巡回は、葉が動いて G を訳す時に道の上で見つかる。
// 歩くのは kWrapWalk 歩まで。葉は動くたびに中を訳し直すので、端までたどると深さ 1,000,000 の user:…:p が層ごとに
// 残りを歩き直して 2 乗になる（machine_deep が 600 秒で時間切れ）。63 歩の Brent は周期 32 までの巡回を見つける。
bool Build::wraps_back(Cell cell) const {
  LayerWatch watch(cell);
  std::size_t steps = 0;
  for (auto next = wrapped_goal(*heap, cell); next && steps < kWrapWalk; next = wrapped_goal(*heap, *next), ++steps) {
    if (goal_loops(*next) || runs_back(*next) || watch.returned_to(*next)) return true;
  }
  return false;
}

// 動いている if/3 に戻ったか（compile_if_goal）。葉と葉の中のたどりで見る。if/3 の引数の中までは外からたどらない。
// 外からたどると、引数の兄弟の枝が歩みを使い切って戻りを見落とし、その if/3 が呼べない引数で出す
// type_error(callable, _) より先に巡回の誤りを出す（2026-11-17 の敵対的レビューの A1 と A3）。
bool Build::runs_back(Cell cell) const {
  if (!running_if || !is_name(cell, "if", 3)) return false;
  for (std::uint16_t index = 0; index < running_if->size(); ++index) {
    if (heap->deref(heap->arg(cell, index)).raw() != (*running_if)[index]) return false;
  }
  return true;
}

// 後で訳す葉と、子を持たない葉の巡回を印す。節（keep が偽）は前のとおり戻り先を変数のゴールに切る（M7）。
void Build::note_back(Cell cell) {
  if (keep && (goal_loops(cell) || runs_back(cell) || wraps_back(cell))) cyclic = true;
}

// 節の戻り先は、頭や引数のデータの巡回を匿名に切るのと同じく、名前のない変数のゴールに切る（オーナー決定 A、
// iso-notes 7.3.3）。変数は戻り先の制御構造ごとに 1 つ。ゴールの位置にしか現れず誰も束縛しないので、
// 共有しても動きは同じで、戻り辺ごとに振ると共有の多い本体でスロットが 2^n 個要る（M7 の敵対的レビュー A2）。
// refs には戻り先の複合項の raw を置く。束縛されていない変数の raw とはタグが違うので slot_of は取り違えない。
IrTerm Build::goal_back(Cell cell) {
  IrTerm term;
  term.tag = IrTag::Var;
  if (keep) {
    cyclic = true;
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
      if (child_is_data(top.term.tag, index)) {
        top.term.args.push_back(data(next));
      } else if (runs_late(*heap, top.term.tag, index, next)) {
        top.term.args.push_back(late_catch(next));
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
        note_back(next);
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
  if (!frame) {
    note_back(cell);
    return goal_leaf(cell);
  }
  if (goal_loops(cell)) return goal_back(cell);
  if (depth >= kShallow) return goal_deep(cell);
  spend(frame->count);
  goal_path.push_back(cell.raw());
  for (std::size_t index = 0; index < frame->count; ++index) {
    const Cell next = heap->arg(cell, static_cast<std::uint16_t>(index));
    if (child_is_data(frame->term.tag, index)) frame->term.args.push_back(data(next));
    else if (runs_late(*heap, frame->term.tag, index, next)) frame->term.args.push_back(late_catch(next));
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

}  // namespace ir_build

namespace {

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
  } else if (first.tag == IrTag::Big) {
    // 鍵は値でなく big_key の指紋（ADR 0025）。索引は鍵の違う節を飛ばすだけで、合うかは頭の単一化が決める。
    clause.key = KeyKind::Big;
    clause.key_int = std::bit_cast<std::int64_t>(big_key(first.integer != 0, big_words(first)));
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

// 節点 1 つは親の args の IrTerm 1 つ。ヒープと状態の語は GC が空けうる（前に訳したゴールは、届かない extras が
// 持ったままのことがある）ので引かない。引くと、収まるゴールが GC の時機で止まる（M28 の Codex 敵対的レビュー
// 6 回目）。持った語は動いている収集の答えで、空かない。木だけで残りを超えれば、安全点（gc.cpp）も必ず止める。
std::size_t ir_budget(const Heap& heap) {
  const std::size_t held = heap.held();
  const std::size_t left = held < heap.limit() ? heap.limit() - held : 0;
  return left / ((sizeof(IrTerm) + kHeapWordBytes - 1) / kHeapWordBytes);
}

}  // namespace

// 束縛された複合項の引数はどれも元の項を指すので、変数の無い引数ごとにもスロットが要る。スロットは 32 ビットなので、
// 前のように溢れたら写し直す段は要らない（ADR 0021）。
namespace {

Compiled compile_run_goal(Heap& heap, Cell cell, std::optional<std::array<std::uint64_t, 3>> running_if) {
  Build build;
  build.heap = &heap;
  build.keep = true;
  build.node_budget = ir_budget(heap);
  build.running_if = running_if;
  IrTerm term = build.goal(cell);
  Compiled compiled;
  compiled.term = make_run_goal(heap, std::move(term));
  compiled.env = make_env(heap, std::move(build.env));
  compiled.cyclic = build.cyclic;
  return compiled;
}

}  // namespace

Compiled compile_goal(Heap& heap, Cell cell) { return compile_run_goal(heap, cell, std::nullopt); }

Compiled compile_if_goal(Heap& heap, Cell cell, const std::array<Cell, 3>& args) {
  std::array<std::uint64_t, 3> raws{};
  for (std::size_t index = 0; index < args.size(); ++index) raws[index] = heap.deref(args[index]).raw();
  return compile_run_goal(heap, cell, raws);
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

bool compile_into(Build& build, const Heap& heap, Cell cell, Clause& clause, std::span<const VarName> names) {
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
  } else if (heap.unbound(head) || is_integer_tag(head.tag()) || head.tag() == Tag::Float) {
    return false;
  } else {
    build.head_copy = true;
    clause.head = build.data(head);
    build.head_copy = false;
  }
  clause.vars = slot_number(build.refs.size());
  if (build.share) renumber_shared(clause.head, body, build.shared, clause.vars);
  // 事実の本体はどの節も同じ true なので、1 つを共有して節ごとに確保しない。
  clause.body = body.tag == IrTag::True && body.args.empty() ? fact_body() : make_goal(std::move(body));
  name_slots(heap, build, names, clause);
  if (clause.head.tag == IrTag::Atom) {
    clause.head.arity = 0;
  }
  set_key(clause);
  return clause.head.tag == IrTag::Atom || clause.head.tag == IrTag::Fun;
}

}  // namespace

bool compile_clause(const Heap& heap, Cell cell, Clause& clause, std::span<const VarName> names) {
  Build build;
  build.heap = &heap;
  return compile_into(build, heap, cell, clause, names);
}

// 節はヒープの外のデータベースに残るが、組む IR が上限の語数を超えるなら、それはヒープに収まらない項を木に広げている。
bool compile_asserted(const Heap& heap, Cell cell, Clause& clause) {
  Build build;
  build.heap = &heap;
  build.share = true;
  build.node_budget = ir_budget(heap);
  if (!compile_into(build, heap, cell, clause, {})) return false;
  clause.shared = make_shared_table(std::move(build.shared), clause.vars);
  return true;
}

bool is_directive(const Heap& heap, Cell cell) {
  cell = heap.deref(cell);
  if (cell.tag() != Tag::Str || heap.functor_arity(cell) != 1) return false;
  const auto name = heap.atoms().name(heap.functor_atom(cell));
  return name == ":-" || name == "?-";
}

}
