#pragma once

// 選択点のスタックの枠（Entry と Pick）、制御枠の中身（Extra と Ctrl）、節とゴールの変数の値（EnvCells）、
// multifile の宣言（SharedPred）。internal.hpp の 800 行の上限のために、その頭から分けた。Impl を知らないので、
// internal.hpp はこれを Impl の定義より先に読む。

#include "mosa/heap.hpp"
#include "mosa/ir.hpp"
#include "mosa/runtime.hpp"

#include "clause_index.hpp"
#include "clause_list.hpp"
#include "gc.hpp"
#include "state_alloc.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace mosa {

inline constexpr std::uint64_t kNoFrame = UINT64_MAX;
inline constexpr std::size_t kFrameHead = 4;
// 生成コードは rt::kFrameHead でゴール枠の引数を読む。2 つは同じ枠の同じ頭である。
static_assert(rt::kFrameHead == kFrameHead);

// 節や call/1 のゴールの変数の値。継続と選択点が共有し、GC は gc_pass で 1 回の回収に 1 度だけ訪ねる。
// 書き換えるのは GC だけである。作った機械の Heap::state に自分の語を数え、放すときに引く（ADR 0010 の改訂 3）。
// Heap は Impl の 2 番目の要素なので、Impl の中の env はどれも Heap より先に壊れる。
// shared は節の共有の項（ADR 0022）。materialize がそのスロットを最初に読むときに組む。
struct EnvCells {
  EnvCells(Heap& heap, std::vector<Cell> values, std::shared_ptr<const SharedTable> table = nullptr);
  ~EnvCells();
  EnvCells(const EnvCells&) = delete;
  EnvCells& operator=(const EnvCells&) = delete;

  std::vector<Cell> cells;
  std::shared_ptr<const SharedTable> shared;
  std::uint64_t gc_pass = 0;
  Heap* owner;
};
using EnvRef = std::shared_ptr<EnvCells>;

// 節の格納の木（節の本体）。上限には数えない。
[[nodiscard]] GoalRef make_goal(IrTerm term);
// 実行時に訳したゴールの木。木の語を heap の Heap::state に数える（ADR 0010 の改訂 3）。
[[nodiscard]] GoalRef make_run_goal(Heap& heap, IrTerm term);
// goal の args[index] を、goal と同じ持ち主で指す。
[[nodiscard]] GoalRef sub_goal(const GoalRef& goal, std::size_t index);
[[nodiscard]] EnvRef make_env(Heap& heap, std::vector<Cell> cells, std::shared_ptr<const SharedTable> shared = nullptr);

// Entry::cursor の「もう試す節が無い」。番号は 2^62 の前後から振るので、この値は節の番号にならない。
inline constexpr std::size_t kNoClause = SIZE_MAX;

// multifile と discontiguous の宣言。述語が無くても残す。宣言だけで節の無い手続きになる（iso-notes 7.8.3 の決定 B）。
// shared は multifile。複数のテキストに節を足してよい印でもある。
struct SharedPred {
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  bool shared = true;
};

enum class Pick {
  Clauses,
  ElseGoal,
  Negation,
  Once,
  Repeat,
  Catch,
  Cleanup,
  AtomGen,
  Retract,
  Barrier,
  // 節の本体は実行しない。Clauses に載せるとやり直しが節を呼ぶ。
  ClauseScan,
  PredGen,
  // ストリームの性質は節ではない。やり直しを節の番号に載せない。
  StreamGen,
  // 生成関数の節の候補。candidates は節の番号ではなく状態なので、pred_* を入れず節の走査に数えさせない。
  Native,
  // 動いている findall・bagof・setof（ADR 0013）。ここへ戻ればゴールの解は尽きたので、答えを束ねる。
  Collect,
  // setup_call_cleanup/3 の後始末（ADR 0016）。pattern が Cleanup、module がその文脈。解ではない。
  CallCleanup,
  // デバッガの箱の番兵（ADR 0028）。redo_goal が元のゴール枠、next が箱の番号、armed が「まだ抜けていない」。水位は
  // 写しの枠と出口の枠を積む前。soft_id は 0 のままにし、armed と cond_ok を *-> の else として読ませない。解ではない。
  DebugBox,
  // 中に選択点を残して抜けた箱の再試行の印（ADR 0028）。next が箱の番号、cut が番兵の添字。解ではない。
  DebugRedo
};

// 動いている収集の状態（ADR 0013）。Pick::Collect の Entry が持つ。中身は solutions.cpp だけが知る。
// Entry の写し（退避したスタック）も同じものを指すので、GC は gc_pass で 1 回の訪問に 1 度だけ辿る。
struct Collector {
  std::uint64_t gc_pass = 0;
  Collector() = default;
  Collector(const Collector&) = delete;
  Collector& operator=(const Collector&) = delete;
  virtual ~Collector() = default;
  virtual void visit(GcVisitor& visitor) = 0;
};
using CollectRef = std::shared_ptr<Collector>;

// 抜けた Catch のスタック位置と、その Entry の effect_id。入れ子の実行の後では同じ位置に別の Catch が
// ありうるので、位置だけでは有効に戻す相手を決めない。
struct CatchRecord {
  std::size_t index = 0;
  std::uint64_t id = 0;
};

struct Entry {
  Pick pick = Pick::Clauses;
  std::size_t heap_mark = 0;
  std::size_t trail_mark = 0;
  std::uint64_t redo_goal = kNoFrame;
  // PredGen は述語の番号（Predicate::serial）、StreamGen と Native は状態。節の走査は candidates を使わない（ADR 0015
  // の改訂）。上限に数える（ADR 0010 の改訂 3）。作るときは id_list で、この機械の Heap に数える列にする。
  IdList candidates;
  // candidates の次の添字。節の走査では、最後に返した節の鍵の組の添字（ClauseWalk::hint、無ければ kNoHint）で、
  // やり直しが組を二分探索せずに続ける。番号を照らしてから使うので、組が変わっていても答えは変わらない。
  std::size_t next = 0;
  // Clauses・Retract・ClauseScan の走査を始めたときの Impl::db_generation。消した節がまだこの走査に届くかを決める。
  std::uint64_t generation = 0;
  // Clauses・Retract・ClauseScan は候補の列を作らず、次に試す節の番号（尽きたら kNoClause）と選ぶ鍵だけを持つ。
  // 列を作ると、最初の答えで切っても呼ぶたびに候補の数だけかかる（レビュー M3 と M20）。番号は消しても asserta
  // しても付け直さない（ADR 0015）。Retract と ClauseScan のカーソルは、次に合う節（先に試して合った節）を指す。
  // sub_atom の AtomGen は、次に試す候補の Before を next に、Length をここに置く（レビュー M32）。デバッガの番兵
  // Pick::DebugBox は、開いたときに決めた箱の深さをここに置く（debug_box.cpp の open_depth）。
  std::size_t cursor = kNoClause;
  IndexKey key;
  std::uint32_t pred_module = 0;
  std::uint32_t pred_name = 0;
  std::uint16_t pred_arity = 0;
  // else_goal と recovery を動かす文脈モジュール。
  std::uint32_t module = 0;
  std::size_t cut = 0;
  GoalRef else_goal;
  // 選択点の種類によっては、組込が状態のセルを置く（operators.cpp）。null は空と同じ。
  EnvRef env;
  std::uint64_t cont = kNoFrame;
  // 0 は *-> の else ではない印。64 ビットなので、決定的に外れる *-> を何回まわしてもあふれない（ADR 0012）。
  std::uint64_t soft_id = 0;
  bool cond_ok = false;
  bool armed = false;
  std::size_t extras_mark = 0;
  // 0 は束縛の水位だけで、選択点ではない。catch_log と scan_log はこの番号で同じ枠かを確かめる。
  std::uint64_t effect_id = 0;
  // 積んだときの Impl::catch_log の長さ。やり直すとき、これより後に無効にした Catch を有効に戻す。
  std::size_t catch_mark = 0;
  Cell pattern{};
  GoalRef recovery;
  // 節ではない組込のやり直し。1 は atom_concat、2 は sub_atom、3 は旗、4 は current_op、
  // 5 は current_char_conversion、6 は length。
  std::uint8_t gen_kind = 0;
  // Pick::Native のやり直しで呼ぶ生成関数。
  rt::NativeProc native = nullptr;
  // Pick::Collect の収集の状態。ほかの選択点では null。
  CollectRef collect;
};

// Native は extras に載せない。語 1 の第 63 ビットが立った制御枠で、ヒープに関数と節の変数を持つ（ADR 0009）。
// DebugExit はデバッガの箱の出口（ADR 0028）。trim_to が番兵の添字、soft_id が箱の番号、cont が元の継続。
enum class Ctrl { Interpret, Commit, SoftCommit, NegDone, OnceDone, CatchExit, Native, Collect, CleanupExit, DebugExit };

struct Extra {
  Ctrl ctrl = Ctrl::Interpret;
  GoalRef goal;
  GoalRef then;
  EnvRef env;
  std::uint64_t cont = kNoFrame;
  std::size_t cut = 0;
  // Commit と CatchExit では切り詰める高さ。SoftCommit では else の Entry の位置で、条件が動く間は変わらない
  // （条件のカットは else より上だけを切る）。スタックをなめて soft_id を探さずに済む（ADR 0011 の改訂 3）。
  std::size_t trim_to = 0;
  std::uint64_t soft_id = 0;
  std::uint32_t module = 0;
};

}  // namespace mosa
