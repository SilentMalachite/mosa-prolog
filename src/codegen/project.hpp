#pragma once

// 計画 Task 5。project.cpp（読み込み、単位の選び方、main.cpp と CMakeLists.txt）と check.cpp（静的な警告）が共有する。

#include "../machine/internal.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::codegen {

// TranslationLog::files と同じ添字で、診断に書くファイルの名前。根はコマンド行のまま、ほかは今のディレクトリからの相対。
using DisplayNames = std::vector<std::string>;

[[nodiscard]] std::optional<std::size_t> log_file(const TranslationLog& log, std::string_view key);
[[nodiscard]] std::vector<std::string> export_warnings(Impl& machine, const TranslationLog& log,
                                                       const DisplayNames& names);
[[nodiscard]] std::vector<std::string> call_warnings(Impl& machine, const TranslationLog& log,
                                                     const DisplayNames& names);

}  // namespace mosa::codegen
