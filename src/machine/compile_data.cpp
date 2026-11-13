// 項をデータとして IR に写す Build の半分（compile.cpp から 800 行を超えないよう分けた）。読み込んだ節は木に写し
// （copy_data）、assert の節は 2 度目に出会った部分項を共有の項にし（share_data、ADR 0022）、実行時のゴールは束縛済みの
// 複合項を元の項のまま指す（place、レビュー M28）。
// テスト: tests/pl/database.pl の m30_*、tests/cpp/assert_shared_test.cpp、tests/cpp/goal_shared_test.cpp、
// tests/cpp/bigint_copy_test.cpp（Big の葉）

#include "compile_build.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace mosa {
namespace {

// 木に広げてこれより小さい部分項は共有せず、前と同じく出現ごとに写す。共有の項は読むたびに memo を引くので、小さい
// 項では写すより遅い（M30 の Codex 敵対的レビュー 4 回目）。共有の項を指す節点も、指す先の大きさで数える。指す数だけで
// 数えると、f(T, T) のような小さい節点を写し直し、その上でまた木に広がる。
constexpr std::uint32_t kShareWeight = 64;

IrTerm var_leaf(std::uint32_t slot) {
  IrTerm term;
  term.tag = IrTag::Var;
  term.slot = slot;
  return term;
}

// 組み立て途中の複合項。子の IrTerm を term.args に集め終えたら閉じる（emit_head.cpp の PlaceFrame と同じ形）。
struct DataFrame {
  Cell cell;
  IrTerm term;
  std::uint16_t count = 0;
};

// share_data の枠。root は写しの根、pinned は共有の項にしない所（Build::share_ref）。
struct ShareFrame {
  DataFrame data;
  bool root = false;
  bool pinned = false;
  bool apart = false;
  ir_build::Seen* node = nullptr;
  std::uint32_t weight = 1;
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

}  // namespace

namespace ir_build {

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
  // 語の子も args に確保する IrTerm なので、確保する前に数える（Build::spend）。数えないと、ヒープに収まる Big が
  // 上限の何倍もの IR になる。
  if (cell.tag() == Tag::Big) {
    const auto magnitude = heap->big_magnitude(cell);
    spend(magnitude.size());
    return big_ir(heap->big_negative(cell), magnitude);
  }
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return term;
  if (!open.contains(cell.raw())) return std::nullopt;
  // 節は env を持たないので、巡回は匿名に切る（巡回する節は 7.6.2 の外）。実行時のゴールはここに来ない（place）。
  term.tag = IrTag::Anon;
  return term;
}

// 実行時のゴールの束縛済みの複合項と Big の箱は、元の項を指す Var の葉にする。項は書き換わらず、中の変数も元のまま
// なので写した項と同じに動く。巡回する項も元の項のまま渡すので巡回が残り、Big も語を IR に写さずに済む（ADR 0025）。
IrTerm Build::place(Cell cell) {
  cell = heap->deref(cell);
  if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str && cell.tag() != Tag::Big) return *data_leaf(cell);
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
  if (!keep) return share ? share_data(root) : copy_data(root);
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

// ADR 0022。循環に届かず写し終えた部分項にまた出会ったら、共有の項のスロットを指す。初めて共有にするときは、最初の
// 写しをその場から共有の項へ移し、そこも Var にする。pinned（写しの根と頭の第 1 引数）は写す。ゴールの根を Var に
// すると call/1 になり、頭の第 1 引数は鍵になる（set_key）から。前の出会いが pinned だけなら、ここを最初の写しにする。
std::optional<IrTerm> Build::share_ref(Seen& node, bool pinned) {
  if (!node.done || node.apart || pinned || node.weight < kShareWeight) return std::nullopt;
  if (node.slot == kNoShare) {
    if (node.first == nullptr) return std::nullopt;
    // 変数のスロットではないので env に置かない。訳し終えてから base + i に付け直す（renumber_shared）。
    node.slot = kProvisionalShared - static_cast<std::uint32_t>(shared.size());
    shared.push_back(SharedTerm{node.slot, std::move(*node.first)});
    *node.first = var_leaf(node.slot);
    node.first = nullptr;
  }
  return var_leaf(node.slot);
}

// copy_data と同じ順に写す。循環に届く部分項は、写しごとに自分の匿名で循環を切る（iso-notes 7.3.3 の決定 A）ので
// 共有しない。1 つにすると、別の出現の匿名が同じ変数になる。NaN を含む部分項も、変数を含む部分項も共有しない。単一化は
// 同じ番地の複合項を中を見ずに一致とするので、1 つにすると、NaN は自分とも単一化しない（iso-notes 7.2.2）出現どうしが
// 一致してしまう。変数は assert の後で NaN になりうる（M30 の Codex 敵対的レビュー 2 回目と Codex レビュー 3 回目）。
IrTerm Build::share_data(Cell root) {
  std::vector<ShareFrame> stack;
  const auto enter = [&](Cell cell, bool pinned) -> std::optional<IrTerm> {
    cell = heap->deref(cell);
    if (cell.tag() != Tag::Lis && cell.tag() != Tag::Str) return data_leaf(cell);
    Seen& node = seen[cell.raw()];
    if (node.open) {
      IrTerm anon;
      anon.tag = IrTag::Anon;
      return anon;
    }
    if (auto ref = share_ref(node, pinned)) return ref;
    spend(child_count(*heap, cell));
    node.open = true;
    stack.push_back(ShareFrame{open_frame(*heap, cell), stack.empty(), pinned, false, &node, 1});
    return std::nullopt;
  };
  if (auto leaf = enter(root, true)) return std::move(*leaf);
  while (true) {
    ShareFrame& top = stack.back();
    if (top.data.term.args.size() < top.data.count) {
      const std::size_t index = top.data.term.args.size();
      const Cell next = heap->deref(child_of(*heap, top.data.cell, index));
      const bool variable = heap->unbound(next);
      // enter が枠を積むと top は無効になるので、積まなかったときだけ末尾の枠へ書く。
      if (auto leaf = enter(next, head_copy && top.root && index == 0)) {
        ShareFrame& parent = stack.back();
        parent.apart = parent.apart || variable || leaf->tag == IrTag::Anon ||
                       (leaf->tag == IrTag::Float && std::isnan(leaf->floating));
        // 複合項から Var の葉ができたなら、それは共有の項を指す。Big の葉は語の子の分も節点に数える。
        const bool refers = leaf->tag == IrTag::Var && (next.tag() == Tag::Lis || next.tag() == Tag::Str);
        const std::size_t weight = refers ? seen[next.raw()].weight : 1 + leaf->args.size();
        parent.weight = static_cast<std::uint32_t>(std::min<std::size_t>(kShareWeight, parent.weight + weight));
        parent.data.term.args.push_back(std::move(*leaf));
      }
      continue;
    }
    Seen& node = *top.node;
    node.open = false;
    node.done = true;
    node.apart = top.apart;
    node.weight = top.weight;
    const bool pinned = top.pinned;
    IrTerm done = std::move(top.data.term);
    stack.pop_back();
    if (stack.empty()) return done;
    ShareFrame& parent = stack.back();
    parent.apart = parent.apart || node.apart;
    parent.weight = std::min(kShareWeight, parent.weight + node.weight);
    parent.data.term.args.push_back(std::move(done));
    if (!pinned && !node.apart && node.slot == kNoShare && node.first == nullptr) {
      node.first = &parent.data.term.args.back();
    }
  }
}

}  // namespace ir_build
}  // namespace mosa
