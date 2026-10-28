#pragma once

// database.cpp と database_scan.cpp だけが共有する。ほかの組込からは使わない。
#include "../machine/internal.hpp"

#include <cstdint>
#include <string_view>

namespace mosa::database {

struct HeadName {
  std::uint32_t name = 0;
  std::uint16_t arity = 0;
};

[[nodiscard]] bool callable_term(const Impl& machine, Cell cell);
void raise_instantiation(Impl& machine, const Goal& goal);
void raise_type(Impl& machine, const Goal& goal, std::string_view type, Cell culprit);
void succeed(Impl& machine, const Goal& goal);
[[nodiscard]] HeadName head_indicator(Impl& machine, Cell head);
// 引数の M: を外し、行き先のモジュールを返す。偽なら誤りを投げ済み。M: が無ければ文脈モジュール。
[[nodiscard]] bool strip_target(Impl& machine, const Goal& goal, Cell& term, std::uint32_t& module, bool clause);
void run_clause(Impl& machine, const Goal& goal);
void run_current_predicate(Impl& machine, const Goal& goal);

}
