#pragma once

// mosac dist の書庫（ADR 0023）。macOS と Linux の tar.gz、Windows の zip を標準の C++ だけで組む。利用者の機械に
// CMake も tar も無くてよいため。圧縮はしない（gzip は無圧縮の deflate ブロック、zip は stored）。
// テスト: tests/cpp/archive_test.cpp（バイトの形）、tests/mosac/dist_test.cmake（cmake -E tar で展開して動かす）。

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace mosa::codegen {

struct ArchiveEntry {
  std::string name;  // UTF-8、区切りは /、書庫の根からの相対
  std::string data;
  bool executable = false;  // 真ならモード 0755、偽なら 0644
};

[[nodiscard]] std::uint32_t crc32(std::string_view bytes);
// mtime は 1970-01-01 UTC からの秒。ディレクトリの項目は書かない（展開する側が親を作る）。
[[nodiscard]] std::string tar_gz_archive(std::span<const ArchiveEntry> entries, std::int64_t mtime);
[[nodiscard]] std::string zip_archive(std::span<const ArchiveEntry> entries, std::int64_t mtime);

}  // namespace mosa::codegen
