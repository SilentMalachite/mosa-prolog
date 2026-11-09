#pragma once

// mosac dist（SPEC 第6節、ADR 0023）。build した実行ファイルを、Mosa の LICENSE と NOTICE、README.txt と一緒に
// <out_dir>/<語幹>-<OS>-<arch>.tar.gz（Windows は .zip）に固める。書庫の形は archive.cpp。
// テスト: tests/mosac/dist_test.cmake（CTest の mosac_dist_*）、tests/cpp/archive_test.cpp。

#include "mosa/toolchain.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace mosa::codegen {

// README.txt に書く動かし方（Windows は .\<語幹>.exe、ほかは ./<語幹>）。名前にシェルが区切る字や引用符があれば、
// 名前全体が 1 語になるよう引用する（Windows は PowerShell の & '...'）。
[[nodiscard]] std::string dist_start_command(std::string_view stem, HostOs os);
// 実行ファイルの名前が、隣に置く文書（LICENSE、NOTICE、README.txt）と ASCII の大文字小文字を除いて同じなら、その文書の名前。
[[nodiscard]] std::optional<std::string_view> dist_name_clash(std::string_view exe_name);
// 語幹から作る実行ファイルの名前が文書と重なれば、診断を出して false。展開すると片方が他方を上書きするので、build の前に止める。
[[nodiscard]] bool dist_name_ok(std::string_view stem);
// README.txt の全文。カタログの鍵 mosac_dist_readme の英語、空の行、日本語。受け取る人の言語は分からないので両方を置く。
[[nodiscard]] std::string dist_readme(std::string_view stem);
// 書けたら書庫のパスを標準出力に書いて 0。書けなければ診断を出して 1。
[[nodiscard]] int write_dist(const std::filesystem::path& exe, std::string_view stem, const std::filesystem::path& out_dir);

}  // namespace mosa::codegen
