#pragma once

// 子プロセスの起動と待ち合わせ（計画 Task 6、mosatest --bench）。定義は src/platform/process.cpp だけに置く。
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace mosa {

// argv[0] のプログラムを PATH から探して起こし、終わるまで待つ。引数は UTF-8。起こせなければ nullopt、
// 起きれば終了コード。log が空でなければ標準出力と標準エラーをそのファイルに上書きで書き、空なら親のものを継ぐ。
// env が空なら親の環境を継ぎ、空でなければ子の環境をその組だけにする（Toolchain::env、計画 Task 9）。
// PATH から探すのはどちらの場合も親の PATH なので、env の PATH で探させたいプログラムは絶対パスで渡す。
[[nodiscard]] std::optional<int> run_process(std::span<const std::string> argv, const std::filesystem::path& log,
                                             std::span<const std::pair<std::string, std::string>> env = {});

}
