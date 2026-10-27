#pragma once

#include <filesystem>

namespace mosa {

// argv[0] は相対パスや別名になる。ライブラリの位置は実行ファイルの実体から決める。
[[nodiscard]] std::filesystem::path executable_path();

}  // namespace mosa
