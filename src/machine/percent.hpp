#pragma once

// 合格率と一致率の百分率の字面。小数第 1 位まで、第 2 位が 5 のときは切り上げる。整数だけで計算し、スイートの報告
// （iso_suite.cpp）と差分の報告（diff_main.cpp、diff_report.cpp）が同じ丸め方をする。テストランナーの外には出さない。

#include <cstddef>
#include <string>

namespace mosa {

[[nodiscard]] inline std::string percent_text(std::size_t part, std::size_t whole) {
  if (whole == 0) return "0.0";
  const std::size_t tenths = (part * 2000 + whole) / (whole * 2);
  return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}

}  // namespace mosa
