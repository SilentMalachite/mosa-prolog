#pragma once

// 項から IR を組む Build。ゴールと節を訳す compile.cpp と、データの項を写す compile_data.cpp が共に使う。1 ファイル
// 800 行を超えないよう 2 つに分けたので、組み立ての状態をここに置く。2 つの翻訳単位で同じ型にするため、無名の名前空間
// ではなく ir_build に置く。

#include "internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace mosa::ir_build {

// ADR 0022（レビュー M30）。assert の写し（Build::share_data）が出会った複合項。open は今たどっている写しの中、apart は
// 出現ごとに写しを分ける（変数・循環を切る匿名・NaN を含む）こと。first は共有の項にしうる所で最初に写した所で、親の args の中にあるので、
// 親の IrTerm が動いても番地は変わらない（args は子の数だけ先に確保してある）。weight は木に広げたときの節点の数で、
// kShareWeight で止める。
inline constexpr std::uint32_t kNoShare = UINT32_MAX;
struct Seen {
  bool open = false;
  bool done = false;
  bool apart = false;
  std::uint32_t slot = kNoShare;
  std::uint32_t weight = 0;
  IrTerm* first = nullptr;
};

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
  // 組む IR の節点の数の上限。実行時のゴールと assert の節は、compile_goal と compile_asserted がヒープの上限で決める
  // （レビュー M28、M30）。読み込んだ節は読んだとおりの木なので決めない。
  std::size_t node_budget = SIZE_MAX;
  std::size_t nodes = 0;
  // 節の戻り先の制御構造と、そこに振った変数のスロット。name_slots はこのスロットに名前を付けない。
  std::unordered_map<std::uint64_t, std::uint32_t> backs;
  // ADR 0022。assert の節は、2 度目に出会った部分項を共有の項にする（Build::share_ref）。節を写す間ずっと持つので、
  // 頭と本体の間でも共有する。
  bool share = false;
  // 今の写しが節の頭。頭の第 1 引数は共有の項にしない。
  bool head_copy = false;
  std::unordered_map<std::uint64_t, Seen> seen;
  std::vector<SharedTerm> shared;

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

  // 以下 7 つは compile_data.cpp、残りは compile.cpp に置く。
  [[nodiscard]] IrTerm data(Cell cell, bool shape = false);
  [[nodiscard]] IrTerm copy_data(Cell cell);
  [[nodiscard]] IrTerm share_data(Cell cell);
  [[nodiscard]] std::optional<IrTerm> share_ref(Seen& node, bool pinned);
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

}  // namespace mosa::ir_build
