#pragma once

// mosatest --bench --report（計画 Task 13）。測った結果を docs/bench.json の形で書く。CI はその値と今回の値を比べる。
#include <cstdint>
#include <filesystem>
#include <string>

namespace mosa {

struct BenchResult {
  std::string file;  // --bench に渡したままの字面。表の鍵
  std::int64_t count = 0;
  std::int64_t interpreter_ms = 0;
  std::int64_t translated_ms = 0;
  std::string ratio;  // 小数 1 桁の字面（bench.cpp の ratio_text）
};

// json の中の同じ file の行だけを result に置き換え（無ければ足し）、file の順に並べて書き直す。書けなければ false。
[[nodiscard]] bool write_bench_report(const std::filesystem::path& json, const BenchResult& result);

}  // namespace mosa
