#pragma once

#include <cstdint>

namespace mosa {

// ADR 0006。ゴールフレームの 1 語目に文脈モジュールを詰め、フレームの語数を変えない。
inline constexpr unsigned kGoalWordModuleShift = 16;
inline constexpr std::uint64_t kGoalWordArityMask = 0xFFFFU;
inline constexpr std::uint64_t kGoalWordModuleMask = 0xFFFFFFFFU;

[[nodiscard]] constexpr std::uint64_t pack_goal_word(std::uint16_t arity, std::uint32_t module) {
  return (static_cast<std::uint64_t>(module) << kGoalWordModuleShift) | arity;
}

[[nodiscard]] constexpr std::uint16_t goal_word_arity(std::uint64_t word) {
  return static_cast<std::uint16_t>(word & kGoalWordArityMask);
}

[[nodiscard]] constexpr std::uint32_t goal_word_module(std::uint64_t word) {
  return static_cast<std::uint32_t>((word >> kGoalWordModuleShift) & kGoalWordModuleMask);
}

}
