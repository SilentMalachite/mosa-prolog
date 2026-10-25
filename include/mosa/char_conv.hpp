#pragma once

// 恒等の文字は入れない。旗が off のときは apply が元の符号点を返す。

#include <cstdint>
#include <utility>
#include <vector>

namespace mosa {

struct CharConv {
  bool enabled = false;
  std::vector<std::pair<char32_t, char32_t>> map;

  [[nodiscard]] char32_t apply(char32_t code) const {
    if (!enabled) return code;
    for (const auto& pair : map) {
      if (pair.first == code) return pair.second;
    }
    return code;
  }
};

}
