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

  [[nodiscard]] std::uint32_t intern(std::string_view text);
  [[nodiscard]] std::uint32_t intern_char(char32_t code);
  [[nodiscard]] bool valid(std::uint32_t id) const;
  [[nodiscard]] bool ascii(std::uint32_t id) const;
  [[nodiscard]] std::string_view name(std::uint32_t id) const;

 private:
  // deque は要素を動かさない。name() の view は、その後の intern でも有効なままである。
  std::deque<std::string> names_;
  std::deque<char> ascii_;
  std::unordered_map<std::string, std::uint32_t> ids_;
};

}
