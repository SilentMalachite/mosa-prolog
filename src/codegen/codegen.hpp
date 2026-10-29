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
  std::size_t predicates = 0;  // 翻訳の候補だった述語（モジュールの静的な述語）。差分テストの翻訳の比率の分母（Task 11）
};

// module はモジュール名のアトム番号。unit_name は NativeUnit::name に入れる。
// interpreted_only の述語は生成関数を出さず、節を interpreted に数える（翻訳のときに読めなかったファイルの述語）。
// trace が真なら、生成関数が節に入るたびにその節の文を標準エラーに書く（mosac build --debug、計画 Task 9）。
// space は C++ の名前空間 mosa_gen::<space>。空なら m<mangle(モジュール)>。mosac emit --units（Task 11）は同じモジュール
// user の単位をファイルごとに出すので、u<N>_m<モジュール> を渡して unit() をぶつけない。
[[nodiscard]] EmitResult emit_module(const Machine& machine, std::uint32_t module, std::string_view unit_name,
                                     std::span<const PredKey> interpreted_only = {}, bool trace = false,
                                     std::string_view space = {});

// 計画 Task 5。mosac emit が out/ に書く目録。name は out/ からの相対の名前である。
struct ProjectFile {
  std::string name;
  std::string text;
};

struct ProjectOptions {
  // mosac の在るディレクトリ。生成する CMakeLists.txt の MOSA_RUNTIME_DIR の既定の元で、同梱ライブラリの原文
  // lib/<名前>.pl を比べる場所でもある（計画 Task 10）
  std::string runtime_dir;
  bool check_calls = true;  // 未定義の述語の呼び出しの警告で translate_program を止めるか。translate_units は警告を出さず見ない
  bool trace = false;       // mosac build --debug。生成コードの kTrace を真にする
  bool interactive = false;  // mosac build --interactive。main.cpp が rt::finish_interactive で終わる（REPL を起こす。計画 Task 14）
  bool library = false;     // mosac emit --library。lib/<名前>.pl の単位 <名前>.cpp だけを出す（計画 Task 10）
};

// ok が偽なら files は空で、diagnostic に Prolog の行で書いた診断を改行で区切って入れる。
// libraries は結ぶ翻訳済みの同梱ライブラリの名前（pl/libmosa_pl_<名前>）。main.cpp と CMakeLists.txt が同じ並びで見る。
// source_only は原文だけを同梱したライブラリ（mosac の隣の lib/ と違う版）。build --verbose が出すだけで、結ぶ物は無い。
struct Project {
  bool ok = false;
  std::string diagnostic;
  std::vector<ProjectFile> files;
  std::vector<std::string> libraries;
  std::vector<std::string> source_only;
};

// path はコマンド行に書いたままのパスで、根のファイルの診断の頭になる。宣言だけを実行して読み（計画 Task 5）、
// 根のファイルのモジュールと、読んだファイルのモジュールごとに単位を 1 つ出す。library(_) のモジュールは
// 同梱ライブラリとして main.cpp が bundle_library に渡す（計画 Task 10）。
[[nodiscard]] Project translate_program(std::string_view path, const ProjectOptions& options);

// mosac emit --units <ディレクトリ>（計画 Task 11）。試験のファイルごとに差分の原文（src/machine/diff.hpp）を組み立てて
// 翻訳し、単位 u<N>_m<モジュール>.cpp と、rt::diff_main を呼ぶ main.cpp、目標 mosa_diff の CMakeLists.txt を出す。
// 組み立てられないファイルは単位の無い欄にして標準エラーに知らせる（実行ファイルが違いとして数える）。
[[nodiscard]] Project translate_units(std::string_view dir, const ProjectOptions& options);

// emit の CMake の目標（実行ファイル）の名前。根のファイルの語幹が CMake の名前に使える文字だけならそれ、ほかは app。
// mosatest --bench はこれで作った実行ファイルを探す。
[[nodiscard]] std::string target_name(std::string_view root);

// 英数字はそのまま、_ は _5f、ほかの UTF-8 のバイトは _ と 2 桁の 16 進（小文字）。
[[nodiscard]] std::string mangle(std::string_view name);

}  // namespace mosa::codegen
