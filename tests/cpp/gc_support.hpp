#pragma once

// gc_test.cpp と gc_policy_test.cpp が共に使う上限、プログラム、問い合わせの道具。2 つは 1 つの gc_test の実行ファイルに
// 入るので、無名の名前空間に写しを 2 つ持つと、片方が使わない写しが -Werror で落ちる。

#include "mosa/heap.hpp"
#include "mosa/machine.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace gc_support {

inline constexpr std::size_t k_small = std::size_t{1} << 16;

inline constexpr std::string_view k_loop = "loop(0) :- !. loop(N) :- N1 is N - 1, loop(N1).";

// 例外を通さずに問い合わせる。ResourceLimit は失敗として数える（GC が無ければここへ来る）。
inline mosa::Stop ask(mosa::Machine& machine, std::string_view goal, bool& limited) {
  limited = false;
  try {
    return machine.query(goal);
  } catch (const mosa::ResourceLimit&) {
    limited = true;
    return mosa::Stop{};
  }
}

inline std::string consulted_path(std::string_view name, std::string_view text) {
  const auto dir = std::filesystem::path{MOSA_SOURCE_DIR} / "tests" / "tmp" / "gc_consult";
  std::filesystem::create_directories(dir);
  const auto file = dir / name;
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  out << text;
  return file.generic_string();
}

}  // namespace gc_support
