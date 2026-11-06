#pragma once

// 節と、述語の節の列（ADR 0015 とその改訂 2、レビュー M21）。列を書き換えるのは clause_store.cpp だけである。

#include "mosa/ir.hpp"

#include "clause_index.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace mosa {

// ゴールの木は写さずに共有する。部分木は根と同じ持ち主を持つ別名のポインタで指す。節を消しても、動いている
// 本体はこれが生かしておく（ADR 0011 の改訂 3）。
using GoalRef = std::shared_ptr<const IrTerm>;

// Clause::died の「まだ消していない」。どの世代もこれより小さいので、生きた節はどの走査にも消えて見えない。
inline constexpr std::uint64_t kAlive = UINT64_MAX;

struct Clause {
  // ADR 0015。述語の中で変わらず、繰り返さない番号。clauses の中では狭義に増える。振るのは clause_store.cpp だけ。
  std::size_t id = 0;
  IrTerm head;
  GoalRef body;
  std::uint32_t vars = 0;
  KeyKind key = KeyKind::Any;
  std::uint32_t key_atom = 0;
  std::uint16_t key_arity = 0;
  std::int64_t key_int = 0;
  double key_float = 0;
  // ADR 0015。足したときと消したときのデータベースの世代。走査は自分の世代に生きていた節だけを見る（7.5.4）。
  // 消した節は、まだ届く走査がある間だけ残る。新しい呼び出しは died を持つ節を飛ばす。
  std::uint64_t born = 0;
  std::uint64_t died = kAlive;
  [[nodiscard]] bool dead() const { return died != kAlive; }
  // ADR 0015 の改訂（レビュー M20）。死んだ節だけが使う飛び越しの幅。番号が [id, id + skip_all) の節は全部死んで
  // おり、[id, id + skip_key) の同じ鍵の節（鍵の無い節どうしも一つの鍵と数える）も全部死んでいる。節は生き返らず、
  // 足す番号はこの範囲の外なので、足しても消しても掃除しても正しいまま。歩みが跳んだ道を縮めて書き直すので mutable。
  // 32 ビットに収まらない幅は最大値に留める（短い範囲も正しい）。
  mutable std::uint32_t skip_all = 1;
  mutable std::uint32_t skip_key = 1;
  // スロットごとの原文の変数名（名前の無い変数は空）。読み込んだ節だけが持ち、mosac のコメントだけが読む。
  // 意味は持たないので指紋には数えない。
  std::vector<std::string> names;
};

// 列が節を動かすときに写さず移すには、移動が noexcept でなければならない（std::vector は move_if_noexcept で移す）。
static_assert(std::is_nothrow_move_constructible_v<Clause>);
static_assert(std::is_nothrow_move_assignable_v<Clause>);

// 世代 generation に始めた走査がこの節を見るか。足すより前の走査と、消した後の走査には見えない。
[[nodiscard]] inline bool visible(const Clause& clause, std::uint64_t generation) {
  return clause.born <= generation && generation < clause.died;
}

class ClauseList;

// ClauseList の位置を持つ反復子。節は vector か deque のどちらかにあるので、位置で引き直す。列を指すので、列が
// 動けば（述語表が伸びて Predicate を移せば）使えない。deque の反復子と違う点で、反復子は述語を足す所をまたいで持たない。
template <bool Const>
class ClauseCursor {
 public:
  using List = std::conditional_t<Const, const ClauseList, ClauseList>;
  using iterator_category = std::random_access_iterator_tag;
  using iterator_concept = std::random_access_iterator_tag;
  using value_type = Clause;
  using difference_type = std::ptrdiff_t;
  using pointer = std::conditional_t<Const, const Clause*, Clause*>;
  using reference = std::conditional_t<Const, const Clause&, Clause&>;

  ClauseCursor() = default;
  ClauseCursor(List* list, std::size_t at) : list_(list), at_(at) {}
  // 書ける反復子は読むだけの反復子として渡せる（erase は const_iterator を取る）。変換演算子にしないのは、
  // 読むだけの反復子の中で自分への変換になり、GCC が既定で警告する（-Wclass-conversion）から。
  template <bool Mutable>
    requires(Const && !Mutable)
  ClauseCursor(const ClauseCursor<Mutable>& other) : list_(other.list_), at_(other.at_) {}

  [[nodiscard]] reference operator*() const;
  [[nodiscard]] pointer operator->() const { return &**this; }
  [[nodiscard]] reference operator[](difference_type offset) const { return *(*this + offset); }
  [[nodiscard]] std::size_t position() const { return at_; }

  ClauseCursor& operator++() { return *this += 1; }
  ClauseCursor operator++(int) {
    ClauseCursor old = *this;
    *this += 1;
    return old;
  }
  ClauseCursor& operator--() { return *this -= 1; }
  ClauseCursor operator--(int) {
    ClauseCursor old = *this;
    *this -= 1;
    return old;
  }
  ClauseCursor& operator+=(difference_type offset) {
    at_ = static_cast<std::size_t>(static_cast<difference_type>(at_) + offset);
    return *this;
  }
  ClauseCursor& operator-=(difference_type offset) { return *this += -offset; }

  friend ClauseCursor operator+(ClauseCursor cursor, difference_type offset) { return cursor += offset; }
  friend ClauseCursor operator+(difference_type offset, ClauseCursor cursor) { return cursor += offset; }
  friend ClauseCursor operator-(ClauseCursor cursor, difference_type offset) { return cursor -= offset; }
  friend difference_type operator-(const ClauseCursor& left, const ClauseCursor& right) {
    return static_cast<difference_type>(left.at_) - static_cast<difference_type>(right.at_);
  }
  friend bool operator==(const ClauseCursor& left, const ClauseCursor& right) { return left.at_ == right.at_; }
  friend std::strong_ordering operator<=>(const ClauseCursor& left, const ClauseCursor& right) {
    return left.at_ <=> right.at_;
  }

 private:
  template <bool>
  friend class ClauseCursor;

  List* list_ = nullptr;
  std::size_t at_ = 0;
};

// 節の列。kVectorLimit 節までは長さぴったりの vector に、それを超えたら deque に置く。std::deque は最初の節で固定の
// 塊（libc++ で 4 KB、25 節）を取るので、deque だけだと 1 節の述語でも塊が 1 つ要った（レビュー M21）。vector だけ
// だと、大きな述語が伸びるたびに古い配列と新しい配列が同時に要り、最大のメモリが節の 2.5〜3 倍になる。小さな vector
// の先頭へ足すのと真ん中を消すのは kVectorLimit 節を動かすまでなので、両端の追加と削除は均して O(1) のまま。
// 上限を 16 にするのは、32 では 17〜32 節の述語で asserta と先頭の retract の繰り返しが 19% 遅くなり（M21 の敵対的
// レビュー B1）、libc++ の塊は 25 節なので、17〜25 節の述語のメモリは deque でもほぼ変わらないから。
// 足し消しは節の場所を動かしうるので、節への参照は列を変えるまでしか持たない（走査は番号で持つ）。
class ClauseList {
 public:
  using iterator = ClauseCursor<false>;
  using const_iterator = ClauseCursor<true>;
  // これを超えたら deque へ移し、半分まで減ったら vector へ戻す。差を開けて、境目で足し消しを繰り返しても
  // 移し替えは kVectorLimit / 2 回の変更に 1 度で済む。
  static constexpr std::size_t kVectorLimit = 16;

  ClauseList() = default;
  // 写しは節だけを持つ。失敗した読み込みの写し（Snapshot）が余りの容量まで持たないようにする。
  ClauseList(const ClauseList& other);
  ClauseList& operator=(const ClauseList& other);
  // 移した元は空の列になる。
  ClauseList(ClauseList&& other) noexcept;
  ClauseList& operator=(ClauseList&& other) noexcept;
  ~ClauseList() = default;

  [[nodiscard]] std::size_t size() const { return chunked_ ? chunked_->size() : slots_.size(); }
  [[nodiscard]] bool empty() const { return size() == 0; }
  [[nodiscard]] Clause& operator[](std::size_t at) { return chunked_ ? (*chunked_)[at] : slots_[at]; }
  [[nodiscard]] const Clause& operator[](std::size_t at) const { return chunked_ ? (*chunked_)[at] : slots_[at]; }
  [[nodiscard]] Clause& front() { return (*this)[0]; }
  [[nodiscard]] const Clause& front() const { return (*this)[0]; }
  [[nodiscard]] Clause& back() { return (*this)[size() - 1]; }
  [[nodiscard]] const Clause& back() const { return (*this)[size() - 1]; }
  [[nodiscard]] iterator begin() { return {this, 0}; }
  [[nodiscard]] iterator end() { return {this, size()}; }
  [[nodiscard]] const_iterator begin() const { return {this, 0}; }
  [[nodiscard]] const_iterator end() const { return {this, size()}; }
  // メモリの確かめ（clause_list_test）が読む。deque に置いているか、vector の容量（deque のときは 0）。
  [[nodiscard]] bool chunked() const { return chunked_ != nullptr; }
  [[nodiscard]] std::size_t capacity() const { return slots_.capacity(); }

  void push_back(Clause clause);
  void push_front(Clause clause);
  // [first, last) を外す。
  void erase(const_iterator first, const_iterator last);
  void erase(const_iterator at) { erase(at, at + 1); }

 private:
  void make_room();
  void settle();

  std::vector<Clause> slots_;
  // 節が kVectorLimit を超えた列だけが持つ。deque は空でも塊を取る実装がある（libstdc++）ので、要るまで作らない。
  std::unique_ptr<std::deque<Clause>> chunked_;
};

template <bool Const>
auto ClauseCursor<Const>::operator*() const -> reference {
  return (*list_)[at_];
}

}  // namespace mosa
