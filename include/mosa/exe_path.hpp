#pragma once

// OS ごとの問い合わせ。定義は src/platform/exe_path.cpp だけに置く。
#include <filesystem>
#include <optional>
#include <string_view>

namespace mosa {

// 動いている実行ファイルの絶対パス。取れない OS では nullopt。
[[nodiscard]] std::optional<std::filesystem::path> executable_path();
// 環境変数をパスとして読む。無いか空なら nullopt。
[[nodiscard]] std::optional<std::filesystem::path> environment_path(std::string_view name);

}
