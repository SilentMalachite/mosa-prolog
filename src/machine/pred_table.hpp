#pragma once

// 述語と述語表（ADR 0018、レビュー M25）。表を足し消しするのは PredicateTable だけである。

#include "mosa/runtime.hpp"

#include "../module/module.hpp"
#include "clause_index.hpp"
#include "clause_list.hpp"
#include "key_index.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

namespace mosa {

// 番号は真ん中から振り始める。asserta は下へ、assertz は上へ伸ばすので、どちらへ 2^62 回足してもあふれない。
// 2^62 を置くには 64 ビットの size_t が要る（CI の 3 つの OS はどれも 64 ビット）。
static_assert(sizeof(std::size_t) >= 8);
inline constexpr std::size_t kFirstClauseId = std::size_t{1} << 62;

struct Predicate {
  // 標準ライブラリによっては索引の unordered_map の移動が noexcept でない（MSVC）。暗黙の移動のままだと、述語表の
  // vector が伸びるたびに move_if_noexcept が全部の述語を節ごと写す。移動は noexcept と宣言する（C++20、P1286R2）。
  // 写しの演算は暗黙のときと同じく残す。
  Predicate() = default;
  Predicate(const Predicate&) = default;
  Predicate& operator=(const Predicate&) = default;
  Predicate(Predicate&&) noexcept = default;
  Predicate& operator=(Predicate&&) noexcept = default;
  ~Predicate() = default;

  // 既定値の 0 は user ではない。述語を作る所で必ず入れる。
  std::uint32_t module = 0;
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
  // 表に足した順の番号。add_predicate だけが振り、使い回さない。表は末尾に足して途中を消すだけなので、この
  // 番号の昇順に並ぶ。current_predicate/1 の候補は添字でなくこれで持つ。添字は消した述語より後ろがずれ、入れ子の
  // 実行が戻したスタックや脇に置いたスタックの候補はずらせない（M18 の敵対的レビュー A1）。
  std::size_t serial = 0;
  // 書くのは clause_store.cpp の関数だけ。番号と live をそこで保つ。
  ClauseList clauses;
  // [first_id, next_id) が振った番号の範囲。節を消しても縮めないので、消した節の番号を別の節が使わない。
  std::size_t first_id = kFirstClauseId;
  std::size_t next_id = kFirstClauseId;
  // dead() でない節の数。clause_store.cpp と database.cpp の mark_dead の died の書き込みだけが変える。
  std::size_t live = 0;
  // 列に残っている死んだ節の数（ADR 0015）。前の掃除が開いた走査のために残した節と、その後に消したが端に無いか
  // 走査が届くので外さなかった節。後で端から外れても減らさない。節の数の半分を超えたら次の変更が一度に掃除する。
  // 消す節ごとに真ん中を詰めると、1 回が節の数だけかかる。
  std::size_t dead_since = 0;
  // 前の掃除が残した死んだ節の数と、それに届いた一番下の枠（Codex レビュー P2）。その枠が外れれば上の枠も外れて
  // いるので、残した節も数えて掃除を決める。枠があるうちは、その後に消した節だけで決める（毎回の掃除を避ける）。
  std::size_t held_dead = 0;
  ScanRecord held_by;
  // この述語の節を最後に消した世代（ADR 0015 の改訂）。これ以上の世代の走査はどの死んだ節も見ないので、死んだ節の
  // 飛び越しの幅で跳んでよい。それより古い走査は、跳ぶ範囲に自分に見える節があり得るので 1 つずつ歩く。
  std::uint64_t last_death = 0;
  // 第1引数の索引。const の述語を引く呼び出しが初めて要るときに作るので mutable。節を書く所が一緒に直す。
  mutable FirstArgIndex index;
  bool seen = false;
  bool dynamic = false;
  // 読み込みの開始で落とす。この読み込みが dynamic を見たら真。見ていなければ置き換え後は静的。
  bool dynamic_declared = false;
  // meta_predicate/1 の宣言。空なら限定する引数は無い。meta_declared は dynamic_declared と同じく読み込みごと。
  std::vector<MetaArg> meta;
  bool meta_declared = false;
  // ADR 0008。開いた走査があるときの abolish。走査のやり直しだけが引き、新しい引き当ては無いものとして扱う。
  bool abolished = false;
  // 今の定義を始めた時の Impl::pred_serial と、始めたのがゴールか（ADR 0017）。定義は枠を作るときと、abolish 済み
  // の枠を使い直すときと、開いた走査の下の abolish で始まる。失敗した読み込みは、同じ定義が残る述語だけを写しへ
  // 戻し、読み込みの間にテキストが始めた定義を取り消す。ゴールが始めた定義は、既にある述語への変更と同じく残す。
  // bool を上の bool の隣に置き、詰め物を増やさない。text_clause_at は、今の定義へテキストが最後に節を足した
  // Impl::db_generation（定義が始まると 0）。写しより後なら写しを丸ごと戻し、前なら宣言だけを戻す（ADR 0017 の改訂）。
  bool defined_by_goal = false;
  std::size_t defined_at = 0;
  std::uint64_t text_clause_at = 0;
  // ADR 0009。指紋が合った静的述語の生成関数。節か meta を書けば detach_native が外し、付け直さない。
  rt::NativeProc native = nullptr;
};

// 述語表の伸長が節を写す形に戻れば、移動が noexcept でない標準ライブラリの CI がここで落ちる。
static_assert(std::is_nothrow_move_constructible_v<Predicate>);
static_assert(std::is_nothrow_move_assignable_v<Predicate>);

[[nodiscard]] inline PredKey key_of(const Predicate& predicate) {
  return PredKey{predicate.module, predicate.name, predicate.arity};
}

// 述語表。述語は Predicate::serial の昇順に並び（M18、pred_by_serial の二分探索）、鍵（module、name、arity）から
// 位置を引く索引を持つ（ADR 0018）。呼び出し、やり直し、assert、retract、読み込みの節はどれも鍵で述語を引くので、
// 表をなめると述語の数に比例した（レビュー M25）。索引は位置を持つので、表が伸びて述語を移しても直さなくてよい。
// 途中を消すと後ろの位置がずれるので、消すのはこの表だけにして、ずれた分を直す。表に入れた述語の鍵は変えない。
// 反復子と参照は vector のもので、足すか消すまでしか持たない。
class PredicateTable {
 public:
  using iterator = std::vector<Predicate>::iterator;
  using const_iterator = std::vector<Predicate>::const_iterator;

  [[nodiscard]] std::size_t size() const { return predicates_.size(); }
  [[nodiscard]] bool empty() const { return predicates_.empty(); }
  [[nodiscard]] iterator begin() { return predicates_.begin(); }
  [[nodiscard]] iterator end() { return predicates_.end(); }
  [[nodiscard]] const_iterator begin() const { return predicates_.begin(); }
  [[nodiscard]] const_iterator end() const { return predicates_.end(); }
  // メモリの確かめ（pred_table_test）が読む。索引の枠の数。
  [[nodiscard]] std::size_t index_slots() const { return index_.slots(); }

  // 呼び出しのたびに通るので、ここに置いて find_pred の中で展開させる。
  [[nodiscard]] Predicate* find(PredKey key) {
    const std::size_t slot = index_.find(predicates_, key);
    return slot == Index::kNoSlot ? nullptr : &predicates_[index_.at(slot)];
  }
  [[nodiscard]] const Predicate* find(PredKey key) const {
    const std::size_t slot = index_.find(predicates_, key);
    return slot == Index::kNoSlot ? nullptr : &predicates_[index_.at(slot)];
  }
  // made の鍵はまだ表に無いこと（呼び手は先に find で確かめる）。末尾に足すので serial の昇順を保つ。
  Predicate& add(Predicate made);
  // key の述語を外す。無ければ何もしない。
  void erase(PredKey key);
  // drop が真を返す述語を外し、索引を作り直す。表の長さだけかかる。確保はしない。
  template <class Drop>
  void erase_if(Drop drop) {
    std::erase_if(predicates_, [&drop](const Predicate& predicate) { return drop(predicate); });
    index_.rebuild(predicates_);
  }

 private:
  using Index = KeyIndex<Predicate>;

  std::vector<Predicate> predicates_;
  Index index_;
};

}  // namespace mosa
