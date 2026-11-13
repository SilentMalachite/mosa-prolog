#pragma once

#include <span>
#include <string>
#include <string_view>

namespace mosa {

[[nodiscard]] int run_suite(std::string_view path);

// test/3 のディレクトリ実行とは別入口。報告書を書いて終了コードだけ返す。
// 報告書の拡張子を .json に替えた場所へ、バッジ用の JSON も書く。前回の JSON があれば合格率の物差しにし、
// 下がった回は 1 で終えて JSON を書き換えない（全体レビュー推奨3、SPEC 第8節）。
[[nodiscard]] int run_iso_suite(std::string_view suite_dir, std::string_view report_path);

struct SuiteArgs {
  std::string dir;
  std::string report;
  bool ok = false;
};

// mosatest --suite の後ろ。<ディレクトリ> [--report <ファイル>] を受ける。
[[nodiscard]] SuiteArgs parse_suite_args(std::span<const std::string_view> args);

}
