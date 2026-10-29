#pragma once

// 試験の実行関数が「ファイルをどう読むか」を受ける形（計画 Task 11、持ち主の決定 1 の B）。インタプリタは組み立てた
// 原文を今の読み込み器で読み、翻訳はその単位の install_unit で読む。空の関数は実行器の既定（今までの読み方）。

#include "mosa/machine.hpp"

#include <filesystem>
#include <functional>

namespace mosa {

using TestLoader = std::function<bool(Machine&, const std::filesystem::path&)>;

}  // namespace mosa
