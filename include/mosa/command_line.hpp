#pragma once

// コマンドラインの引数（SPEC 第4節、計画 Task 15b）。定義は src/platform/command_line.cpp だけに置く。
#include <string>
#include <string_view>
#include <vector>

namespace mosa {

// 引数を UTF-8 の並びで返す。Windows の argv は ANSI コードページなので、UTF-16 の元から作り直す。
// 他の OS では argv をそのまま写す。
[[nodiscard]] std::vector<std::string> command_line_utf8(int argc, char** argv);

// Windows のコマンドラインの UTF-16 を、C 実行時ライブラリ（2008 以降）が argv を作るのと同じ規則で分ける。
// 先頭のプログラムの名前は引用符だけを見る。どの OS でも試せるように、Windows の呼び出しを持たない（レビュー L-38 の 5）。
[[nodiscard]] std::vector<std::u16string> split_command_line(std::u16string_view line);

}
