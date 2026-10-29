#pragma once

// mosatest --bench（計画 Task 6）。インタプリタと mosac emit の実行ファイルで bench(N) を流し、壁時計の中央値と比を出す。
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace mosa {

struct BenchArgs {
  std::string file;             // bench(N) と initialization(main, main) を持つ Prolog のファイル
  std::string work;             // 目録とビルドとログの置き場。空なら mosatest の隣（ビルド木）の mosatest-bench。
                                // 共有の /tmp の決まった名前は、ほかの利用者が先に作って中身を差し替えられるので使わない
  std::string cmake = "cmake";  // 翻訳側を作る CMake。既定は PATH から引く
  std::int64_t count = 20000;   // N。インタプリタが 1〜4 秒になる値（ADR 0009 の下見と同じ 20000）
  std::string report;           // 空でなければ結果をこの JSON（docs/bench.json の形）に書き足す（計画 Task 13）
  bool ok = false;
};

// mosatest --bench の後ろ。<ファイル> [--count <N>] [--work <ディレクトリ>] [--cmake <パス>] [--report <ファイル>]
// を受ける。
[[nodiscard]] BenchArgs parse_bench_args(std::span<const std::string_view> args);
// 両側を流して結果を標準出力に出す。失敗の診断は標準エラー。終了コードは 0 か 1。
[[nodiscard]] int run_bench(const BenchArgs& args);

}
