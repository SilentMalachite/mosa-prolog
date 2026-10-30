#pragma once

// 予約と確保を分けたアドレス範囲（ADR 0010）。ヒープはこの上に置き、伸ばしても確保済みの語が動かない。
// OS を呼ぶ定義は src/platform/address_range.cpp だけに置く（words() のような読むだけの口は速さのためここで inline）。
#include <cstddef>
#include <cstdint>
#include <optional>

namespace mosa {

class AddressRange {
 public:
  // bytes をページ単位に切り上げて予約する。OS が断れば nullopt。
  [[nodiscard]] static std::optional<AddressRange> reserve(std::size_t bytes);
  AddressRange(AddressRange&& other) noexcept;
  AddressRange& operator=(AddressRange&& other) noexcept;
  AddressRange(const AddressRange&) = delete;
  AddressRange& operator=(const AddressRange&) = delete;
  ~AddressRange();

  // 先頭から bytes までを使えるようにする（ページ単位に切り上げ）。すでに足りていれば何もせず true。
  // 予約を超えるか OS が断れば false。
  [[nodiscard]] bool commit(std::size_t bytes);
  // 全ての語の読み書きが通るので、呼び出しにならないよう header で定義する（bench の nrev で差が出た）。
  [[nodiscard]] std::uint64_t* words() const { return static_cast<std::uint64_t*>(base_); }
  [[nodiscard]] std::size_t reserved_bytes() const;
  [[nodiscard]] std::size_t committed_bytes() const;

 private:
  AddressRange(void* base, std::size_t reserved) : base_(base), reserved_(reserved) {}
  void release();

  void* base_ = nullptr;
  std::size_t reserved_ = 0;
  std::size_t committed_ = 0;
};

}
