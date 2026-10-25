#pragma once

// 語の番地をセルに書く。コンストラクタで一度だけ確保し、再配置しない。

#include "mosa/atom.hpp"
#include "mosa/cell.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <vector>

namespace mosa {

class ResourceLimit : public std::exception {
 public:
  explicit ResourceLimit(const char* message) noexcept : message_(message) {}
  [[nodiscard]] const char* what() const noexcept override { return message_; }

 private:
  const char* message_;
};

class Heap {
 public:
  explicit Heap(AtomTable& atoms, std::size_t limit = std::size_t{1} << 20);
  Heap(const Heap&) = delete;
  Heap& operator=(const Heap&) = delete;

  [[nodiscard]] AtomTable& atoms() const { return atoms_; }
  [[nodiscard]] std::size_t limit() const { return limit_; }
  [[nodiscard]] std::size_t size() const { return used_; }
  [[nodiscard]] std::size_t capacity() const { return words_.size(); }
  [[nodiscard]] const std::uint64_t* data() const { return words_.data(); }
  void truncate(std::size_t size);

  // ゴール枠は項のタグを付けない。返る添字から count 語を呼んだ側が埋める。
  [[nodiscard]] std::size_t allocate_words(std::size_t count);
  [[nodiscard]] std::uint64_t word_at(std::size_t index) const;
  void set_word(std::size_t index, std::uint64_t value);

  [[nodiscard]] Cell fresh_ref();
  [[nodiscard]] bool unbound(Cell cell) const;
  [[nodiscard]] Cell deref(Cell cell) const;
  void bind(Cell variable, Cell value);

  [[nodiscard]] Cell atom(std::uint32_t id) const;
  [[nodiscard]] Cell integer(std::int64_t value);
  [[nodiscard]] std::int64_t integer_value(Cell cell) const;
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
  [[nodiscard]] std::uint64_t* allocate(std::size_t count);
  [[nodiscard]] const std::uint64_t* pointer(Cell cell) const;

  AtomTable& atoms_;
  std::size_t limit_;
  std::size_t used_ = 0;
  // 全容量を構築時に置く。size() は使った語数であり、vector の再配置は起こさない。
  std::vector<std::uint64_t> words_;
};

}
