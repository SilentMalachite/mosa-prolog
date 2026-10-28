#pragma once

// mosac の翻訳器（ADR 0009、計画 Task 4）。読み込んだモジュールの静的述語の IR から、runtime.hpp だけを
// include する C++ を出す。出した関数は install_unit が指紋の合う述語に付ける。
// translated は生成関数を出した述語の数、interpreted は IR のまま動く節の数である。生成関数の中で
// interpret_clause に渡す節も、install_unit が付けない述語（native_eligible が偽）の節も数える。

#include "mosa/machine.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace mosa::codegen {

struct EmitResult {
  std::string source;
  std::string header;
  std::size_t translated = 0;
  std::size_t interpreted = 0;
};

// module はモジュール名のアトム番号。unit_name は NativeUnit::name に入れる。
[[nodiscard]] EmitResult emit_module(const Machine& machine, std::uint32_t module, std::string_view unit_name);

// 英数字はそのまま、_ は __、ほかの UTF-8 のバイトは _ と 2 桁の 16 進（小文字）。
[[nodiscard]] std::string mangle(std::string_view name);

}  // namespace mosa::codegen
