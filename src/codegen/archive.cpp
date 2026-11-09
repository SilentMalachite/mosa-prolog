// mosac dist の書庫（ADR 0023）。tar は POSIX の ustar に、名前が ASCII でないか 100 バイトの欄に入らないときだけ
// PAX の path の記録を前に置く。日本語の名前を ustar の欄に入れると、展開する側の文字コードで読み違えるため。
// gzip と zip は圧縮せず、CRC-32 と長さだけを付ける。どちらも展開する道具は 2 つの形の決まりだけを見る。
// テスト: tests/cpp/archive_test.cpp、tests/mosac/dist_test.cmake。

#include "archive.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>

namespace mosa::codegen {
namespace {

constexpr std::array<std::uint32_t, 256> make_crc_table() {
  std::array<std::uint32_t, 256> table{};
  for (std::uint32_t index = 0; index < table.size(); ++index) {
    std::uint32_t value = index;
    for (int bit = 0; bit < 8; ++bit) value = (value & 1U) != 0 ? 0xEDB88320U ^ (value >> 1U) : value >> 1U;
    table[index] = value;
  }
  return table;
}

constexpr std::array<std::uint32_t, 256> k_crc_table = make_crc_table();

constexpr std::size_t k_block = 512;
// tar の書庫は 20 ブロックの記録の倍数にする。短い末尾を誤りとみなす古い tar があるため。
constexpr std::size_t k_record = 20 * k_block;
constexpr std::size_t k_stored_max = 65535;

void put_le16(std::string& out, std::uint32_t value) {
  out += static_cast<char>(value & 0xFFU);
  out += static_cast<char>((value >> 8U) & 0xFFU);
}

void put_le32(std::string& out, std::uint32_t value) {
  put_le16(out, value & 0xFFFFU);
  put_le16(out, value >> 16U);
}

// width 桁の 8 進数と NUL で欄を埋める。
void put_octal(std::string& header, std::size_t at, std::size_t width, std::uint64_t value) {
  for (std::size_t index = width; index-- > 0;) {
    header[at + index] = static_cast<char>('0' + (value & 7U));
    value >>= 3U;
  }
  header[at + width] = '\0';
}

void put_text(std::string& header, std::size_t at, std::size_t width, std::string_view text) {
  header.replace(at, std::min(width, text.size()), text.substr(0, width));
}

bool fits_ustar(std::string_view name) {
  return name.size() <= 100 && std::all_of(name.begin(), name.end(), [](char unit) {
           return static_cast<unsigned char>(unit) < 0x80U;
         });
}

struct TarHeader {
  std::string_view name;
  std::uint32_t mode = 0644;
  std::uint64_t size = 0;
  std::int64_t mtime = 0;
  char type = '0';
};

std::string tar_header(const TarHeader& fields) {
  std::string header(k_block, '\0');
  put_text(header, 0, 100, fields.name);
  put_octal(header, 100, 7, fields.mode);
  put_octal(header, 108, 7, 0);
  put_octal(header, 116, 7, 0);
  put_octal(header, 124, 11, fields.size);
  put_octal(header, 136, 11, static_cast<std::uint64_t>(std::max<std::int64_t>(fields.mtime, 0)));
  header[156] = fields.type;
  put_text(header, 257, 8, std::string_view{"ustar\0" "00", 8});
  put_octal(header, 329, 7, 0);
  put_octal(header, 337, 7, 0);
  // 検査和は、欄そのものを空白 8 つとして全バイトを足す。6 桁、NUL、空白の順に書く（POSIX）。
  std::uint32_t sum = 0;
  header.replace(148, 8, 8, ' ');
  for (const char unit : header) sum += static_cast<unsigned char>(unit);
  put_octal(header, 148, 6, sum);
  header[155] = ' ';
  return header;
}

void pad_to(std::string& out, std::size_t unit) {
  out.append((unit - out.size() % unit) % unit, '\0');
}

// "<n> path=<name>\n" の n は、n 自身の桁を含む記録全体のバイト数。
std::string pax_path_record(std::string_view name) {
  const std::string body = " path=" + std::string{name} + "\n";
  std::size_t digits = std::to_string(body.size()).size();
  while (std::to_string(body.size() + digits).size() != digits) ++digits;
  return std::to_string(body.size() + digits) + body;
}

// ustar の名前の欄に置く代わりの名前。PAX を読まない道具のために、ASCII でない字を _ にして 100 バイトで切る。
std::string ascii_fallback(std::string_view name) {
  std::string out;
  for (const char unit : name) out += static_cast<unsigned char>(unit) < 0x80U ? unit : '_';
  return out.substr(0, 100);
}

void append_tar_entry(std::string& tar, const ArchiveEntry& entry, std::int64_t mtime) {
  if (!fits_ustar(entry.name)) {
    const std::string record = pax_path_record(entry.name);
    tar += tar_header({"PaxHeader", 0644, record.size(), mtime, 'x'});
    tar += record;
    pad_to(tar, k_block);
  }
  const std::string name = ascii_fallback(entry.name);
  tar += tar_header({name, entry.executable ? 0755U : 0644U, entry.data.size(), mtime, '0'});
  tar += entry.data;
  pad_to(tar, k_block);
}

// deflate の無圧縮のブロック（BTYPE 00）。各ブロックは 1 バイトの頭、LEN、NLEN、中身。空でも最後のブロックを 1 つ置く。
void append_stored_blocks(std::string& out, std::string_view data) {
  std::size_t at = 0;
  do {
    const std::size_t len = std::min(k_stored_max, data.size() - at);
    const bool last = at + len == data.size();
    out += static_cast<char>(last ? 1 : 0);
    put_le16(out, static_cast<std::uint32_t>(len));
    put_le16(out, static_cast<std::uint32_t>(~len & 0xFFFFU));
    out.append(data.substr(at, len));
    at += len;
  } while (at < data.size());
}

// zip の日付と時刻（MS-DOS の形、UTC として書く）。1980 年より前は書けないので 1980-01-01 00:00:00 にそろえる。
struct DosTime {
  std::uint32_t date = 0;
  std::uint32_t time = 0;
};

DosTime dos_time(std::int64_t mtime) {
  constexpr std::int64_t k_1980 = 315'532'800;
  constexpr std::int64_t k_2107_end = 4'354'819'199;
  const std::int64_t clamped = std::clamp(mtime, k_1980, k_2107_end);
  const std::int64_t day = clamped / 86'400;
  const std::int64_t second = clamped % 86'400;
  const std::chrono::year_month_day ymd{std::chrono::sys_days{std::chrono::days{day}}};
  const auto year = static_cast<std::uint32_t>(static_cast<int>(ymd.year()) - 1980);
  const auto month = static_cast<std::uint32_t>(static_cast<unsigned>(ymd.month()));
  const auto mday = static_cast<std::uint32_t>(static_cast<unsigned>(ymd.day()));
  const auto hour = static_cast<std::uint32_t>(second / 3600);
  const auto minute = static_cast<std::uint32_t>(second % 3600 / 60);
  const auto half = static_cast<std::uint32_t>(second % 60 / 2);
  return {(year << 9U) | (month << 5U) | mday, (hour << 11U) | (minute << 5U) | half};
}

// zip の局所の見出しと中央の見出しに共通の、版から名前の長さまで（版の前の 4 バイトと後ろの欄は呼ぶ側が書く）。
void put_zip_common(std::string& out, const ArchiveEntry& entry, DosTime when) {
  constexpr std::uint32_t k_utf8_name = 0x0800;
  put_le16(out, 20);
  put_le16(out, k_utf8_name);
  put_le16(out, 0);
  put_le16(out, when.time);
  put_le16(out, when.date);
  put_le32(out, crc32(entry.data));
  put_le32(out, static_cast<std::uint32_t>(entry.data.size()));
  put_le32(out, static_cast<std::uint32_t>(entry.data.size()));
  put_le16(out, static_cast<std::uint32_t>(entry.name.size()));
  put_le16(out, 0);
}

}  // namespace

std::uint32_t crc32(std::string_view bytes) {
  std::uint32_t value = 0xFFFFFFFFU;
  for (const char unit : bytes) value = k_crc_table[(value ^ static_cast<unsigned char>(unit)) & 0xFFU] ^ (value >> 8U);
  return value ^ 0xFFFFFFFFU;
}

std::string tar_gz_archive(std::span<const ArchiveEntry> entries, std::int64_t mtime) {
  std::string tar;
  for (const ArchiveEntry& entry : entries) append_tar_entry(tar, entry, mtime);
  tar.append(2 * k_block, '\0');
  pad_to(tar, k_record);
  std::string gz{"\x1f\x8b\x08\x00", 4};
  put_le32(gz, static_cast<std::uint32_t>(std::clamp<std::int64_t>(mtime, 0, 0xFFFFFFFF)));
  gz += std::string{"\x00\xff", 2};
  append_stored_blocks(gz, tar);
  put_le32(gz, crc32(tar));
  put_le32(gz, static_cast<std::uint32_t>(tar.size() & 0xFFFFFFFFU));
  return gz;
}

std::string zip_archive(std::span<const ArchiveEntry> entries, std::int64_t mtime) {
  const DosTime when = dos_time(mtime);
  std::string zip;
  std::string directory;
  for (const ArchiveEntry& entry : entries) {
    const auto offset = static_cast<std::uint32_t>(zip.size());
    put_le32(zip, 0x04034b50U);
    put_zip_common(zip, entry, when);
    zip += entry.name;
    zip += entry.data;
    // 作った側を Unix（3）にすると、展開する側が外部属性の上 16 ビットをモードとして読む。
    put_le32(directory, 0x02014b50U);
    put_le16(directory, 0x031EU);
    put_zip_common(directory, entry, when);
    put_le16(directory, 0);
    put_le16(directory, 0);
    put_le16(directory, 0);
    put_le32(directory, (entry.executable ? 0100755U : 0100644U) << 16U);
    put_le32(directory, offset);
    directory += entry.name;
  }
  const auto directory_offset = static_cast<std::uint32_t>(zip.size());
  zip += directory;
  put_le32(zip, 0x06054b50U);
  put_le16(zip, 0);
  put_le16(zip, 0);
  put_le16(zip, static_cast<std::uint32_t>(entries.size()));
  put_le16(zip, static_cast<std::uint32_t>(entries.size()));
  put_le32(zip, static_cast<std::uint32_t>(directory.size()));
  put_le32(zip, directory_offset);
  put_le16(zip, 0);
  return zip;
}

}  // namespace mosa::codegen
