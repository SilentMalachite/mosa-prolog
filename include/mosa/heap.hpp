#pragma once

// 語の番地をセルに書く。コンストラクタで上限までの番地を予約し（ADR 0010）、使う分だけ commit で足す。
// 予約した範囲は動かないので、伸びても確保済みの語の番地は変わらない。

#include "mosa/address_range.hpp"
#include "mosa/atom.hpp"
#include "mosa/bigint.hpp"
#include "mosa/cell.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>

namespace mosa {

class ResourceLimit : public std::exception {
 public:
  explicit ResourceLimit(const char* message) noexcept : message_(message) {}
  [[nodiscard]] const char* what() const noexcept override { return message_; }

 private:
  const char* message_;
};

// 上限までの番地を OS が予約させなかった（起動時の Heap の構築）。止め方は ResourceLimit と同じで、文面だけを分ける。
class HeapReserveFailed : public ResourceLimit {
 public:
  HeapReserveFailed() noexcept : ResourceLimit("heap_reserve") {}
};

// どちらも語の数。4 GiB は 2^29 語。起動時に commit する量は min(kInitialHeapCommit, 上限)。
inline constexpr std::size_t kDefaultHeapMax = std::size_t{1} << 29;
inline constexpr std::size_t kInitialHeapCommit = std::size_t{1} << 20;

class Heap {
 public:
  // limit は伸ばせる上限（語）。予約が断られたら HeapReserveFailed。
  explicit Heap(AtomTable& atoms, std::size_t limit = kDefaultHeapMax);
  Heap(const Heap&) = delete;
  Heap& operator=(const Heap&) = delete;

  [[nodiscard]] AtomTable& atoms() const { return atoms_; }
  [[nodiscard]] std::size_t limit() const { return limit_; }
  [[nodiscard]] std::size_t size() const { return used_; }
  // 今 commit してある語数。上限（limit）までの途中の段階であり、allocate が足りなければ伸ばす。
  [[nodiscard]] std::size_t committed() const { return committed_words_; }
  [[nodiscard]] std::size_t capacity() const { return committed_words_; }
  // ヒープの外に持ち、上限ではヒープの語と同じに数える語（ADR 0010 の改訂 2。動いている収集の答え）。
  [[nodiscard]] std::size_t held() const { return held_; }
  void hold(std::size_t words);
  void release(std::size_t words);
  // ヒープの外の実行の状態（選択点・extras・env・実行時に訳したゴール）の語（ADR 0010 の改訂 3）。持った語と
  // 違って確保の天井には入れず、歩みごとの安全点が上限に数える。env は作るたびに足すので、天井を決め直さない。
  [[nodiscard]] std::size_t state() const { return state_; }
  void add_state(std::size_t words) { state_ += words; }
  // 足した物だけが引くので、足した分を超えない。超えれば別の物の分まで消えるので、黙って 0 にしない。
  void drop_state(std::size_t words) {
    assert(words <= state_);
    state_ -= words;
  }
  [[nodiscard]] const std::uint64_t* data() const { return range_.words(); }
  void truncate(std::size_t size);
  // GC が生きている語を詰めたあとの大きさ。truncate と同じ動きだが、呼ぶ意図を名前で分ける。
  void reset_size_after_gc(std::size_t used);

  // ゴール枠は項のタグを付けない。返る添字から count 語を呼んだ側が埋める。
  // 速い道は比較 1 回。bench の nrev で呼び出しの分まで目に見えたので header に置く。
  [[nodiscard]] std::size_t allocate_words(std::size_t count) {
    return static_cast<std::size_t>(allocate(count) - range_.words());
  }
  [[nodiscard]] std::uint64_t word_at(std::size_t index) const;
  void set_word(std::size_t index, std::uint64_t value);

  [[nodiscard]] Cell fresh_ref();
  [[nodiscard]] bool unbound(Cell cell) const;
  [[nodiscard]] Cell deref(Cell cell) const;
  void bind(Cell variable, Cell value);

  [[nodiscard]] Cell atom(std::uint32_t id) const;
  [[nodiscard]] Cell integer(std::int64_t value);
  // Int と IntBox だけを受ける。Big を渡すと std::abort() で止まる（切り詰めて別の値にしない。ADR 0025）。
  [[nodiscard]] std::int64_t integer_value(Cell cell) const;
  // 正規形で作る（ADR 0025）。int64 に収まる値は integer と同じ Int か IntBox、収まらない値だけが Big の箱になる。
  [[nodiscard]] Cell big(const BigInt& value);
  [[nodiscard]] BigInt big_value(Cell cell) const;  // Int、IntBox、Big のどれでも受ける
  // 以下 2 つは Big だけを受ける。絶対値の語は下位が先で、最上位の語は 0 でない。
  [[nodiscard]] std::span<const std::uint64_t> big_magnitude(Cell cell) const;
  [[nodiscard]] bool big_negative(Cell cell) const;
  [[nodiscard]] Cell floating(double value);
  [[nodiscard]] double float_value(Cell cell) const;
  [[nodiscard]] Cell structure(std::uint32_t functor, std::span<const Cell> args);
  [[nodiscard]] std::uint16_t functor_arity(Cell cell) const;
  [[nodiscard]] std::uint32_t functor_atom(Cell cell) const;
  [[nodiscard]] Cell arg(Cell cell, std::size_t index) const;
  [[nodiscard]] Cell list(Cell head, Cell tail);
  [[nodiscard]] Cell list_head(Cell cell) const;
  [[nodiscard]] Cell list_tail(Cell cell) const;
  [[nodiscard]] const std::uint64_t* word(Cell cell) const;

 private:
  [[nodiscard]] std::uint64_t* allocate(std::size_t count) {
    // used_ <= ceiling_ <= committed_words_ <= limit_ が常に成り立つので、引き算はあふれない。
    // ceiling_ は持った語の分だけ commit 済みより低い。commit 済みの中の確保でも持った語を数える（ADR 0010 の改訂 2）。
    if (count > ceiling_ - used_) [[unlikely]] grow_for(count);
    auto* slot = range_.words() + used_;
    used_ += count;
    return slot;
  }
  // 足りないときだけ。上限を確かめて伸ばし、上限を超えるなら ResourceLimit("heap_limit")。
  void grow_for(std::size_t count);
  // need 語までを commit する。1 MiB 単位に切り上げ、上限か OS の断りで ResourceLimit。
  void grow(std::size_t need);
  [[nodiscard]] const std::uint64_t* pointer(Cell cell) const;
  // 速い道の天井 min(commit 済み, 上限 - 持った語) を決め直す。used_ より下には置かない。
  void reset_ceiling();

  AtomTable& atoms_;
  std::size_t limit_;
  std::size_t used_ = 0;
  // 上限までの番地を構築時に予約する。size() は使った語数、committed_words_ は使える語数。
  AddressRange range_;
  std::size_t committed_words_ = 0;
  std::size_t held_ = 0;
  std::size_t state_ = 0;
  std::size_t ceiling_ = 0;
};

}
