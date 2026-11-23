#pragma once

// 番号 0 は欠番。同じ UTF-8 バイト列は同じ番号にし、正規化はしない。

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mosa {

class AtomTable {
 public:
  AtomTable();
  // ids_ の鍵は names_ の要素を指す view なので、写しは自分の names_ から ids_ を作り直す。移しは入れ替えで、deque と
  // unordered_map の入れ替えは要素を動かさないので、view は移した先の要素を指したまま。移しの元は番号 0 を欠番にした
  // 表になる。前の既定の移しは 3 つを 1 つずつ移したので、途中の確保が投げると元の ids_ が消えた要素を指し、移した後の
  // 元と自分への移しは番号 0 の欠番を失った（レビュー R-M02）。移しの構築は空の表を作るので投げうるが、元は変えない。
  AtomTable(const AtomTable& other);
  AtomTable& operator=(const AtomTable& other);
  AtomTable(AtomTable&& other);
  AtomTable& operator=(AtomTable&& other) noexcept;
  ~AtomTable() = default;
  void swap(AtomTable& other) noexcept;

  [[nodiscard]] std::uint32_t intern(std::string_view text);
  [[nodiscard]] std::uint32_t intern_char(char32_t code);
  [[nodiscard]] bool valid(std::uint32_t id) const;
  [[nodiscard]] bool ascii(std::uint32_t id) const;
  [[nodiscard]] std::string_view name(std::uint32_t id) const;

 private:
  // deque は要素を動かさない。name() の view は、その後の intern でも有効なままである。ids_ の鍵も names_ の要素を
  // 指す view にし、名前を 1 度だけ持つ。前は鍵にも写しを持ち、引くたびに std::string を作った（レビュー N-01）。
  std::deque<std::string> names_;
  std::deque<char> ascii_;
  std::unordered_map<std::string_view, std::uint32_t> ids_;
};

}
