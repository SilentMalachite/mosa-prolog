#pragma once

// 呼び出し側は Formal をこのヘルパだけで作る。アトム名の直書きはヘルパの中に閉じる。

#include "mosa/cell.hpp"

#include <cstdint>
#include <string_view>

namespace mosa {

class Machine;

[[nodiscard]] Cell make_instantiation_error(Machine& machine);
[[nodiscard]] Cell make_type_error(Machine& machine, std::string_view type, Cell culprit);
[[nodiscard]] Cell make_domain_error(Machine& machine, std::string_view domain, Cell culprit);
[[nodiscard]] Cell make_existence_error(Machine& machine, std::string_view kind, Cell culprit);
[[nodiscard]] Cell make_evaluation_error(Machine& machine, std::string_view which);
[[nodiscard]] Cell make_permission_error(Machine& machine, std::string_view action, std::string_view type,
                                         Cell culprit);
Cell throw_error(Machine& machine, Cell formal, std::uint32_t name, std::uint16_t arity);

}
