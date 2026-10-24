#pragma once

#include <iostream>
#include <string_view>

namespace mosa_test {

// 翻訳単位が分かれても回数を 1 つにするため、inline 関数の関数ローカル static に置く。
inline int& failure_count() {
  static int count = 0;
  return count;
}

inline void check(bool condition, std::string_view label) {
  if (!condition) {
    failure_count() += 1;
    std::cerr << "FAIL: " << label << '\n';
  }
}

inline int failures() { return failure_count(); }

}
