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

// main の戻り値にする終了コード。数をそのまま返すと POSIX では下位 8 ビットしか残らず、
// 256 件の倍数の失敗が 0 になって ctest が通過と数える（レビュー H-33）ので、0 か 1 に畳む。
inline int failures() { return failure_count() == 0 ? 0 : 1; }

}
