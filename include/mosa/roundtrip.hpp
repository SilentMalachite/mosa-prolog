#pragma once

// syntax.pl はゴールではなく例文である。各ケースは既定の演算子表から始める。

#include <string_view>

namespace mosa {

[[nodiscard]] int run_roundtrip(std::string_view text);

}
