// 計画 Task 13。docs/bench.json。プログラムごとに 1 行の JSON オブジェクトを programs に並べる。--bench は 1 回に
// 1 ファイルだけ測るので、既存のファイルを行で読み、同じ file の行だけを置き換える。行で読むのは自分が書いた形だけで、
// CI の比べる手順（.github/workflows/ci.yml）も同じ行を sed で読む。生成物で、手で直さない（.ja.md も置かない）。

#include "bench_report.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace mosa {
namespace {

constexpr std::string_view kEntryPrefix = "{\"file\":";

std::string quote(std::string_view text) {
  std::string out = "\"";
  for (const char unit : text) {
    if (unit == '"' || unit == '\\') out.push_back('\\');
    out.push_back(unit);
  }
  return out + "\"";
}

std::string entry_text(const BenchResult& result) {
  return std::string{kEntryPrefix} + quote(result.file) + ",\"count\":" + std::to_string(result.count) +
         ",\"interpreter_ms\":" + std::to_string(result.interpreter_ms) +
         ",\"translated_ms\":" + std::to_string(result.translated_ms) + ",\"ratio\":" + result.ratio + "}";
}

// 前後の空白と区切りの , を外した行が自分の書いたプログラムの行なら、それを返す。
std::string_view entry_of(std::string_view line) {
  while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.remove_prefix(1);
  while (!line.empty() && (line.back() == '\r' || line.back() == ',' || line.back() == ' ')) line.remove_suffix(1);
  if (!line.starts_with(kEntryPrefix) || !line.ends_with('}')) return {};
  return line;
}

std::vector<std::string> read_entries(const std::filesystem::path& json) {
  std::vector<std::string> entries;
  std::ifstream in{json, std::ios::binary};
  for (std::string line; std::getline(in, line);) {
    const std::string_view entry = entry_of(line);
    if (!entry.empty()) entries.emplace_back(entry);
  }
  return entries;
}

}  // namespace

bool write_bench_report(const std::filesystem::path& json, const BenchResult& result) {
  std::vector<std::string> entries = read_entries(json);
  // 鍵は行の頭の {"file":"…", までで比べる。ほかのファイルの名がこの名で始まっても、閉じの " と , で分かれる。
  const std::string key = std::string{kEntryPrefix} + quote(result.file) + ",";
  std::erase_if(entries, [&](const std::string& entry) { return entry.starts_with(key); });
  entries.push_back(entry_text(result));
  std::sort(entries.begin(), entries.end());
  std::ofstream out{json, std::ios::binary | std::ios::trunc};
  if (!out) return false;
  out << "{\"generated\":\"mosatest --bench --report writes this file. Do not edit by hand.\",\n"
      << " \"unit\":\"median wall-clock milliseconds of 5 runs per side; ratio = interpreter / translated\",\n"
      << " \"programs\":[\n";
  for (std::size_t index = 0; index < entries.size(); ++index) {
    out << "  " << entries[index] << (index + 1 < entries.size() ? ",\n" : "\n");
  }
  out << " ]\n}\n";
  return static_cast<bool>(out);
}

}  // namespace mosa
