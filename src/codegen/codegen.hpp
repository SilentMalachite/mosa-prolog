#pragma once

// mosac の翻訳器（ADR 0009、計画 Task 4）。読み込んだモジュールの静的述語の IR から、runtime.hpp だけを
// include する C++ を出す。出した関数は install_unit が指紋の合う述語に付ける。
// translated は生成関数を出した述語の数、interpreted は IR のまま動く節の数である。生成関数の中で
// interpret_clause に渡す節も、install_unit が付けない述語（native_eligible が偽）の節も数える。

#include "mosa/machine.hpp"

#include "../module/module.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::codegen {

struct EmitResult {
  std::string source;
  std::string header;
  std::size_t translated = 0;
  std::size_t interpreted = 0;
};

// module はモジュール名のアトム番号。unit_name は NativeUnit::name に入れる。
// interpreted_only の述語は生成関数を出さず、節を interpreted に数える（翻訳のときに読めなかったファイルの述語）。
// trace が真なら、生成関数が節に入るたびにその節の文を標準エラーに書く（mosac build --debug、計画 Task 9）。
[[nodiscard]] EmitResult emit_module(const Machine& machine, std::uint32_t module, std::string_view unit_name,
                                     std::span<const PredKey> interpreted_only = {}, bool trace = false);

// 計画 Task 5。mosac emit が out/ に書く目録。name は out/ からの相対の名前である。
struct ProjectFile {
  std::string name;
  std::string text;
};

struct ProjectOptions {
  std::string runtime_dir;  // 生成する CMakeLists.txt の MOSA_RUNTIME_DIR の既定（emit した mosac のビルド木）
  bool check_calls = true;  // --units（Task 11）は偽にする。存在エラーを確かめるテストも翻訳するため
  bool trace = false;       // mosac build --debug。生成コードの kTrace を真にする
};

// ok が偽なら files は空で、diagnostic に Prolog の行で書いた診断を改行で区切って入れる。
struct Project {
  bool ok = false;
  std::string diagnostic;
  std::vector<ProjectFile> files;
};

// path はコマンド行に書いたままのパスで、根のファイルの診断の頭になる。宣言だけを実行して読み（計画 Task 5）、
// 根のファイルのモジュールと、読んだファイルのモジュールごとに単位を 1 つ出す。library(_) のモジュールは Task 10。
[[nodiscard]] Project translate_program(std::string_view path, const ProjectOptions& options);

// emit の CMake の目標（実行ファイル）の名前。根のファイルの語幹が CMake の名前に使える文字だけならそれ、ほかは app。
// mosatest --bench はこれで作った実行ファイルを探す。
[[nodiscard]] std::string target_name(std::string_view root);

// 英数字はそのまま、_ は _5f、ほかの UTF-8 のバイトは _ と 2 桁の 16 進（小文字）。
[[nodiscard]] std::string mangle(std::string_view name);

}  // namespace mosa::codegen
