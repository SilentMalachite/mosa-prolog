#pragma once

// 子プロセスの起動と待ち合わせ（計画 Task 6、mosatest --bench）。定義は src/platform/process.cpp だけに置く。
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace mosa {

// argv[0] のプログラムを PATH から探して起こし、終わるまで待つ。引数は UTF-8。起こせなければ nullopt、
// 起きれば終了コード。log が空でなければ標準出力と標準エラーをそのファイルに上書きで書き、空なら親のものを継ぐ。
[[nodiscard]] std::optional<int> run_process(std::span<const std::string> argv, const std::filesystem::path& log);

}
