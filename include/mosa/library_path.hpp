#pragma once

// library(Name) の探索（SPEC 第5節）。$MOSA_LIB、次に実行ファイルのディレクトリの lib/ を見る。
#include <filesystem>
#include <optional>
#include <string_view>

namespace mosa {

[[nodiscard]] std::optional<std::filesystem::path> find_library(std::string_view name);
// 探す場所を引数で受ける版。find_library は環境変数と実行ファイルの場所を渡してこれを呼ぶ。
[[nodiscard]] std::optional<std::filesystem::path> find_library_in(
    std::string_view name, const std::optional<std::filesystem::path>& env_dir,
    const std::optional<std::filesystem::path>& exe_dir);

}
