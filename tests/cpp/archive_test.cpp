// ADR 0023。mosac dist の書庫を、展開する道具に頼らずバイトで確かめる。gzip は無圧縮の deflate ブロックを読み戻して tar を
// 取り出し、tar は見出しの欄と検査和、zip は小端の欄を読む。展開して動かす確かめは tests/mosac/dist_test.cmake。
#include "harness.hpp"

#include "codegen/archive.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

using mosa::codegen::ArchiveEntry;

std::uint32_t byte_at(std::string_view bytes, std::size_t at) {
  return static_cast<unsigned char>(bytes[at]);
}

std::uint32_t le16(std::string_view bytes, std::size_t at) {
  return byte_at(bytes, at) | (byte_at(bytes, at + 1) << 8U);
}

std::uint32_t le32(std::string_view bytes, std::size_t at) {
  return le16(bytes, at) | (le16(bytes, at + 2) << 16U);
}

// gzip の無圧縮のブロックを順に読み、中身をつなげて返す。ブロックの形が違えば check を落として空を返す。
std::string inflate_stored(std::string_view gz, const std::string& label, std::size_t* blocks = nullptr) {
  std::string out;
  std::size_t at = 10;
  for (;;) {
    if (at + 5 > gz.size()) {
      mosa_test::check(false, label + ": stored block runs past the end");
      return {};
    }
    const std::uint32_t head = byte_at(gz, at);
    const std::uint32_t len = le16(gz, at + 1);
    const std::uint32_t nlen = le16(gz, at + 3);
    mosa_test::check((head & 0x06U) == 0, label + ": block type is stored");
    mosa_test::check(len <= 65535 && nlen == (~len & 0xFFFFU), label + ": LEN and NLEN");
    out.append(gz.substr(at + 5, len));
    at += 5 + len;
    if (blocks != nullptr) ++*blocks;
    if ((head & 0x01U) != 0) break;
  }
  mosa_test::check(at + 8 == gz.size(), label + ": 8-byte trailer after the last block");
  return out;
}

std::string tar_of(const std::vector<ArchiveEntry>& entries, const std::string& label, std::size_t* blocks = nullptr) {
  const std::string gz = mosa::codegen::tar_gz_archive(entries, 1'794'314'096);
  mosa_test::check(gz.size() > 18 && gz.substr(0, 3) == std::string{"\x1f\x8b\x08", 3}, label + ": gzip magic");
  const std::string tar = inflate_stored(gz, label, blocks);
  mosa_test::check(le32(gz, gz.size() - 8) == mosa::codegen::crc32(tar), label + ": gzip CRC-32");
  mosa_test::check(le32(gz, gz.size() - 4) == tar.size(), label + ": gzip ISIZE");
  return tar;
}

std::string field(std::string_view header, std::size_t at, std::size_t width) {
  return std::string{header.substr(at, width)};
}

// 見出しの検査和。検査和の欄（148 から 8 バイト）は空白として足す。
bool checksum_ok(std::string_view header) {
  std::uint32_t sum = 0;
  for (std::size_t index = 0; index < 512; ++index) sum += (index >= 148 && index < 156) ? 0x20U : byte_at(header, index);
  return std::stoul(field(header, 148, 6), nullptr, 8) == sum;
}

void crc32_known() {
  mosa_test::check(mosa::codegen::crc32("123456789") == 0xCBF43926U, "crc32 of 123456789");
  mosa_test::check(mosa::codegen::crc32("") == 0, "crc32 of nothing");
}

void tar_gz_frame() {
  const std::vector<ArchiveEntry> entries{{"a/hello", "hi\n", true}, {"a/LICENSE", "text", false}};
  const std::string tar = tar_of(entries, "frame");
  mosa_test::check(tar.size() % 10240 == 0, "tar is a whole number of 10240-byte records");
  const std::string_view first = std::string_view{tar}.substr(0, 512);
  mosa_test::check(field(first, 0, 8) == std::string{"a/hello\0", 8}, "tar name");
  mosa_test::check(field(first, 100, 8) == std::string{"0000755\0", 8}, "tar mode of the executable");
  mosa_test::check(field(first, 124, 12) == std::string{"00000000003\0", 12}, "tar size");
  mosa_test::check(field(first, 156, 1) == "0", "tar typeflag");
  mosa_test::check(field(first, 257, 8) == std::string{"ustar\0" "00", 8}, "tar magic and version");
  mosa_test::check(checksum_ok(first), "tar checksum");
  mosa_test::check(tar.substr(512, 512) == "hi\n" + std::string(509, '\0'), "tar data padded to 512");
  const std::string_view second = std::string_view{tar}.substr(1024, 512);
  mosa_test::check(field(second, 100, 8) == std::string{"0000644\0", 8}, "tar mode of a plain file");
  mosa_test::check(checksum_ok(second), "tar checksum of the second header");
  mosa_test::check(tar.substr(2048, 1024) == std::string(1024, '\0'), "two zero blocks at the end");
}

void tar_gz_splits_blocks() {
  std::string big(200'000, 'x');
  for (std::size_t index = 0; index < big.size(); index += 7) big[index] = static_cast<char>('a' + index % 26);
  std::size_t blocks = 0;
  const std::string tar = tar_of({{"big", big, false}}, "split", &blocks);
  mosa_test::check(blocks >= 4, "200,000 bytes take at least 4 stored blocks");
  mosa_test::check(tar.substr(512, big.size()) == big, "split blocks join to the tar");
}

// PAX の記録 "<n> path=<name>\n" の n はその記録全体のバイト数。
std::string pax_record(std::string_view name) {
  const std::string body = " path=" + std::string{name} + "\n";
  std::size_t digits = std::to_string(body.size()).size();
  while (std::to_string(body.size() + digits).size() != digits) ++digits;
  return std::to_string(body.size() + digits) + body;
}

void tar_pax_name() {
  const std::string japanese = "こんにちは-x/こんにちは";
  const std::string long_name = std::string(120, 'n');
  for (const std::string& name : {japanese, long_name}) {
    const std::string tar = tar_of({{name, "x", false}}, "pax");
    const std::string_view pax = std::string_view{tar}.substr(0, 512);
    mosa_test::check(field(pax, 156, 1) == "x", "pax header typeflag x: " + name);
    mosa_test::check(checksum_ok(pax), "pax header checksum: " + name);
    const std::string record = pax_record(name);
    mosa_test::check(std::stoul(field(pax, 124, 11), nullptr, 8) == record.size(), "pax size: " + name);
    mosa_test::check(tar.substr(512, record.size()) == record, "pax record: " + name);
    const std::string_view entry = std::string_view{tar}.substr(1024, 512);
    mosa_test::check(field(entry, 156, 1) == "0", "entry after pax: " + name);
    mosa_test::check(checksum_ok(entry), "entry checksum after pax: " + name);
  }
}

// 局所の見出しを 1 つ確かめ、次の見出しの位置を返す。
std::size_t check_local(std::string_view zip, std::size_t at, const ArchiveEntry& entry) {
  const std::string label = "zip local " + entry.name;
  mosa_test::check(le32(zip, at) == 0x04034b50U, label + ": signature");
  mosa_test::check(le16(zip, at + 4) == 20, label + ": version needed");
  mosa_test::check((le16(zip, at + 6) & 0x0800U) != 0, label + ": UTF-8 flag");
  mosa_test::check(le16(zip, at + 8) == 0, label + ": stored");
  mosa_test::check(le32(zip, at + 14) == mosa::codegen::crc32(entry.data), label + ": CRC-32");
  mosa_test::check(le32(zip, at + 18) == entry.data.size() && le32(zip, at + 22) == entry.data.size(), label + ": sizes");
  const std::uint32_t name_len = le16(zip, at + 26);
  const std::uint32_t extra_len = le16(zip, at + 28);
  mosa_test::check(zip.substr(at + 30, name_len) == entry.name, label + ": name");
  mosa_test::check(zip.substr(at + 30 + name_len + extra_len, entry.data.size()) == entry.data, label + ": data");
  return at + 30 + name_len + extra_len + entry.data.size();
}

// 中央の見出しを 1 つ確かめ、次の見出しの位置を返す。
std::size_t check_central(std::string_view zip, std::size_t at, const ArchiveEntry& entry, std::size_t local) {
  const std::string label = "zip central " + entry.name;
  mosa_test::check(le32(zip, at) == 0x02014b50U, label + ": signature");
  mosa_test::check(le16(zip, at + 4) == 0x031EU, label + ": version made by (Unix)");
  mosa_test::check((le16(zip, at + 8) & 0x0800U) != 0, label + ": UTF-8 flag");
  mosa_test::check(le32(zip, at + 16) == mosa::codegen::crc32(entry.data), label + ": CRC-32");
  const std::uint32_t mode = entry.executable ? 0100755U : 0100644U;
  mosa_test::check(le32(zip, at + 38) == (mode << 16U), label + ": Unix mode");
  mosa_test::check(le32(zip, at + 42) == local, label + ": local header offset");
  const std::uint32_t name_len = le16(zip, at + 28);
  mosa_test::check(zip.substr(at + 46, name_len) == entry.name, label + ": name");
  return at + 46 + name_len + le16(zip, at + 30) + le16(zip, at + 32);
}

void zip_entries() {
  const std::vector<ArchiveEntry> entries{{"a/hello.exe", "MZ", true}, {"a/こんにちは.txt", "text\n", false}};
  const std::string zip = mosa::codegen::zip_archive(entries, 1'794'314'096);
  std::vector<std::size_t> locals;
  std::size_t at = 0;
  for (const ArchiveEntry& entry : entries) {
    locals.push_back(at);
    at = check_local(zip, at, entry);
  }
  const std::size_t directory = at;
  for (std::size_t index = 0; index < entries.size(); ++index) at = check_central(zip, at, entries[index], locals[index]);
  mosa_test::check(le32(zip, at) == 0x06054b50U, "zip end record signature");
  mosa_test::check(le16(zip, at + 8) == 2 && le16(zip, at + 10) == 2, "zip end record counts");
  mosa_test::check(le32(zip, at + 12) == at - directory, "zip end record directory size");
  mosa_test::check(le32(zip, at + 16) == directory, "zip end record directory offset");
  mosa_test::check(at + 22 == zip.size(), "zip ends after the end record");
}

void check_dos_time(std::int64_t mtime, std::uint32_t date, std::uint32_t time, const std::string& label) {
  const std::string zip = mosa::codegen::zip_archive(std::vector<ArchiveEntry>{{"t", "", false}}, mtime);
  mosa_test::check(le16(zip, 10) == time, label + ": DOS time");
  mosa_test::check(le16(zip, 12) == date, label + ": DOS date");
}

void zip_dos_time() {
  check_dos_time(1'794'314'096, (46U << 9U) | (11U << 5U) | 10U, (12U << 11U) | (34U << 5U) | 28U, "2026-11-10 12:34:56");
}

void zip_dos_time_floor() { check_dos_time(0, (0U << 9U) | (1U << 5U) | 1U, 0, "1970 clamps to 1980-01-01"); }

}  // namespace

int main() {
  crc32_known();
  tar_gz_frame();
  tar_gz_splits_blocks();
  tar_pax_name();
  zip_entries();
  zip_dos_time();
  zip_dos_time_floor();
  return mosa_test::failures();
}
