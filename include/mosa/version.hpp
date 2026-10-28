#pragma once

#include <string>

namespace mosa {
// 旗 version、version_data と version_line が同じ番号を使う。CMakeLists.txt の project VERSION と揃える。
inline constexpr int kVersionMajor = 0;
inline constexpr int kVersionMinor = 0;
inline constexpr int kVersionPatch = 1;

[[nodiscard]] std::string version_line();
}
