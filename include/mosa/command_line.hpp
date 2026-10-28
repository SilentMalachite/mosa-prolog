#pragma once

// コマンドラインの引数（SPEC 第4節、計画 Task 15b）。定義は src/platform/command_line.cpp だけに置く。
#include <string>
#include <vector>

namespace mosa {

// 引数を UTF-8 の並びで返す。Windows の argv は ANSI コードページなので、UTF-16 の元から作り直す。
// 他の OS では argv をそのまま写す。
[[nodiscard]] std::vector<std::string> command_line_utf8(int argc, char** argv);

}
