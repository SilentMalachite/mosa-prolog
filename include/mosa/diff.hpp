#pragma once

// mosatest --diff（計画 Task 11）。試験のディレクトリを mosac emit --units で翻訳し、CMake で実行ファイル mosa_diff を
// 作って動かす。比べるのは実行ファイルの中（rt::diff_main）で、インタプリタと翻訳が同じ原文と同じ問い合わせで出した
// 答え、エラー項、標準出力を 1 件ずつ見る。
#include <span>
#include <string>
#include <string_view>

namespace mosa {

struct DiffArgs {
  std::string dir;                // tests/pl か tests/conformance、またはそれと同じ形のディレクトリ
  std::string report;             // docs/agreement.json。空なら報告を書かない
  std::string work;               // 目録とビルドとログの置き場。空なら mosatest の隣（ビルド木）の mosatest-diff
  std::string cmake = "cmake";    // 翻訳側を作る CMake。既定は PATH から引く
  std::string config = "Release"; // 翻訳側の構成。libmosa と合わせる必要がある（MSVC の /MT と /MTd）ので CTest が渡す
  bool ok = false;
};

// mosatest --diff の後ろ。<ディレクトリ> [--report <ファイル>] [--work <ディレクトリ>] [--cmake <パス>] [--config <名前>]。
// --report を省くと、リポジトリ直下から tests/pl か tests/conformance を渡したときだけ docs/agreement.json に書く。
[[nodiscard]] DiffArgs parse_diff_args(std::span<const std::string_view> args);
// 翻訳して流す。結果は実行ファイルが標準出力に出す。終了コードは違いが無ければ 0、ほかは 1。
[[nodiscard]] int run_diff(const DiffArgs& args);

}
