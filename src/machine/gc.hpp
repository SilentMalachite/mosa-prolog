#pragma once

// ADR 0010 の mark-and-compact。GC は run_loop の 2 歩の間（gc_safe_point）でだけ走るので、組込や生成関数の
// C++ の局所変数が握るセルは動かない。入れ子の run_loop を回す C++ だけが RootScope で値を登録する。
//
// ルートの一覧（Serena の find_referencing_symbols で Impl・Entry・Extra・Trail・Binding のフィールドを洗った結果）。
//   Impl::ip（枠）、Impl::bindings[].variable（セル）、Impl::stack の各 Entry、Impl::gc_roots。
//   Impl::ball は exception が真のときだけルートにする。偽のときは collect_garbage が Cell{} に戻す。入れ子の
//     run_loop（with_output_to・findall）が捕まった例外の後に残す古い番地は、切り詰めの後で別の語（枠など）を
//     指しうる。それを項として辿ると、枠の語に違う種類が付いて枠を辿り損ねる。安全点では exception は常に偽で、
//     ball を読むのは exception が真のとき（run_loop・errors.cpp）だけなので、消しても意味は変わらない。
//   Entry（GcVisitor::entry）: env と pattern はセル、redo_goal と cont は枠、heap_mark・trail_mark・extras_mark は
//     水位。cut・live_catches・native_cut・Extra::trim_to は選択点スタックの高さか添字で、ヒープではないので触らない。
//     candidates は節の添字か生成関数の状態（native_control.cpp の push_branch は語 1 の下位 32 ビットの状態）で、
//     ヒープの添字ではない。
//   Extra（extras）はルートではない。制御枠の語 1 から届いたものだけが生き、その env（セル）と cont（枠）を辿る。
//   Trail::slots は弱い参照。印の付いた語を指す項目だけを残す。
//   Trail::mark は計画では weak_address だが、生きた語の番地ではなく水位（heap.data() + heap_mark）なので、
//     heap_mark と同じく rank で写す。死んだ語を 0（nullptr）にすると unify の境界の意味が変わる。
//   ルートでないもの: Stop（呼び手へ返した写し。次の next() の後に古い Stop のセルを読まない）、op_undos・
//     streams・predicates・main_goal（ヒープの値を持たない）、native_next（関数の番地）。
//
// RootScope を置く入れ子（計画 Task 5）。入れ子の後で使う const Goal& は可変の写しを登録してから使う。
//   solutions.cpp の run_solutions（goal・templ・witnesses・hidden）と collect（Saved・VarIds）。VarIds の base は
//     水位なので heap_mark、stable・local・homes・alias は弱い参照、ext_raw は答えに戻すので強い参照。
//   load_directive.cpp の run_parked（退避したスタック・束縛・水位）。extras とトレイルは退避せず水位だけ持つ。
//   load.cpp の load_source（heap_base・inits）と apply_use（arg）、consult.cpp の run_consult（goal）。
//   write_pred.cpp の run_goal（Snap）と run_with（goal・sink.slot）。
//   lgt_copy.cpp の copied_succeeds は何も持たない（コメント参照）。
// GcInhibit が残るのは write_pred.cpp の apply_portray だけ。writer の C++ の再帰が書きかけの項を握るため。

#include "mosa/cell.hpp"
#include "mosa/machine.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace mosa {

struct Entry;

struct GcStats {
  std::uint64_t count = 0;
  std::uint64_t freed_words = 0;
  std::chrono::steady_clock::duration time{};
};

// 入れ子の run_loop を回す C++ が握る値を、GC が印付けと書き換えの 2 回に分けて訪ねる。
class GcVisitor {
 public:
  virtual void cell(Cell& slot) = 0;
  virtual void frame(std::uint64_t& index) = 0;       // kNoFrame は飛ばす
  virtual void heap_mark(std::size_t& mark) = 0;
  virtual void trail_mark(std::size_t& mark) = 0;
  virtual void extras_mark(std::size_t& mark) = 0;
  virtual void weak_address(std::uint64_t& raw) = 0;  // 印付けでは辿らない。生きていれば新番地、死んでいれば 0
  // ヒープの添字。印付けでは辿らない。生きていれば新しい添字、死んでいれば SIZE_MAX（0 は生きた添字でありうる）
  virtual void weak_index(std::size_t& index) = 0;
  void entry(Entry& entry);                           // Entry のセル・枠・水位をすべて訪ねる

 protected:
  GcVisitor() = default;
  GcVisitor(const GcVisitor&) = default;
  GcVisitor& operator=(const GcVisitor&) = default;
  ~GcVisitor() = default;
};

using RootVisit = std::function<void(GcVisitor&)>;

class RootScope {  // 構築で Impl::gc_roots に積み、破棄で外す
 public:
  RootScope(Machine::Impl& machine, RootVisit visit);
  ~RootScope();
  RootScope(const RootScope&) = delete;
  RootScope& operator=(const RootScope&) = delete;

 private:
  Machine::Impl& machine_;
};

class GcInhibit {  // 構築で Impl::gc_inhibit を 1 増やし、破棄で戻す
 public:
  explicit GcInhibit(Machine::Impl& machine);
  ~GcInhibit();
  GcInhibit(const GcInhibit&) = delete;
  GcInhibit& operator=(const GcInhibit&) = delete;

 private:
  Machine::Impl& machine_;
};

void collect_garbage(Machine::Impl& machine);  // 無条件に 1 回。gc_inhibit > 0 なら何もしない
void gc_safe_point(Machine::Impl& machine);    // run_loop が step の前に呼ぶ。比較 1 回で抜けるのが普通
// Impl の構築で呼ぶ。柔らかい容量と閾値を上限から決め、MOSA_GC_STRESS を読む。
void init_gc(Machine::Impl& machine);

// ここから下は gc*.cpp の間だけで使う。
// 語の種類は 1 語 2 ビット。印付けが決め、詰め直しがこれに従って中身を書き換える。
enum class WordKind : std::uint64_t { Cell = 0, Raw = 1, FrameIndex = 2, ExtrasIndex = 3 };

struct GcMarks {
  std::size_t used = 0;                     // 印付けの時点の heap.size()
  std::vector<std::uint64_t> live;          // 1 語 1 ビット。used の位置まで引けるよう 1 区画多い
  std::vector<std::uint64_t> kinds;         // 1 語 2 ビット（WordKind）
  std::vector<std::uint64_t> extras_live;   // extras の 1 項目 1 ビット
};

// gc_mark.cpp。ルートから届く語に印と種類を付ける。
void mark_live(Machine::Impl& machine, GcMarks& marks);
// gc_compact.cpp。印の付いた語を順序を保って下へ詰め、ルート・extras・トレイルを書き換える。
void compact_live(Machine::Impl& machine, const GcMarks& marks);
// gc.cpp。印付けと書き換えが同じ順でルートを訪ねる。
void visit_roots(Machine::Impl& machine, GcVisitor& visitor);

}  // namespace mosa
