// mosac dist（SPEC 第6節、ADR 0023）。実行ファイルは libmosa と翻訳済みのライブラリを静的に結んでおり、どちらも
// Apache-2.0 なので、再頒布するバイナリに LICENSE と NOTICE を添える（ライセンスの第 4 節）。文面は mosac を作った木の
// もの（dist_texts.inc）。書庫は隣の一時ファイルに書いてから名前を変え、書くのに失敗しても前の書庫を残す。
// テスト: tests/mosac/dist_test.cmake、tests/cpp/archive_test.cpp。

#include "dist.hpp"

#include "archive.hpp"
#include "build.hpp"
#include "dist_texts.inc"

#include "mosa/messages.hpp"
#include "mosa/path_text.hpp"
#include "mosa/toolchain.hpp"
#include "mosa/version.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace mosa::codegen {
namespace {

std::string version_text() {
  return std::to_string(kVersionMajor) + "." + std::to_string(kVersionMinor) + "." + std::to_string(kVersionPatch);
}

std::int64_t now_seconds() {
  const auto since = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::seconds>(since).count();
}

// 実行ファイルの隣に置く文書の名前。dist_entries はこの順に置く。
constexpr std::array<std::string_view, 3> k_doc_names{"LICENSE", "NOTICE", "README.txt"};

std::vector<ArchiveEntry> dist_entries(std::string exe_bytes, std::string_view stem) {
  const std::string folder = std::string{stem} + "-" + host_os_arch() + "/";
  return {
      {folder + std::string{stem} + exe_suffix(), std::move(exe_bytes), true},
      {folder + std::string{k_doc_names[0]}, std::string{k_mosa_license}, false},
      {folder + std::string{k_doc_names[1]}, std::string{k_mosa_notice}, false},
      {folder + std::string{k_doc_names[2]}, dist_readme(stem), false},
  };
}

bool plain_ascii(char unit) {
  const bool alnum = (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z') || (unit >= '0' && unit <= '9');
  return alnum || unit == '_' || unit == '-' || unit == '.';
}

char ascii_lower(char unit) { return unit >= 'A' && unit <= 'Z' ? static_cast<char>(unit - 'A' + 'a') : unit; }

// POSIX のシェルは ASCII の字でだけ語を区切るので、ASCII でない字は引用しない。引用の中の ' は '\'' と書く。
std::string posix_command(std::string_view stem) {
  const std::string command = "./" + std::string{stem};
  const auto plain = [](char unit) { return static_cast<unsigned char>(unit) >= 0x80U || plain_ascii(unit); };
  if (std::all_of(stem.begin(), stem.end(), plain)) return command;
  std::string quoted = "'";
  for (const char unit : command) quoted += unit == '\'' ? std::string{"'\\''"} : std::string(1, unit);
  return quoted + "'";
}

// ' の仲間なら、その UTF-8 のバイト数。PowerShell は U+2018 から U+201B（E2 80 98 から E2 80 9B）も ' として読む。
std::size_t single_quote_length(std::string_view rest) {
  if (rest.front() == '\'') return 1;
  const auto byte = [&](std::size_t at) { return static_cast<unsigned char>(rest[at]); };
  return rest.size() >= 3 && byte(0) == 0xE2U && byte(1) == 0x80U && byte(2) >= 0x98U && byte(2) <= 0x9BU ? 3 : 0;
}

// PowerShell は ASCII でない空白（U+3000 など）でも語を区切るので、字の表を持たず、ASCII でない字を含む名前はいつも
// 引用する。引用した文字列は & で呼ぶ（引用しただけでは文字列を出すだけ）。中の ' の仲間は 2 つ重ねる。
std::string powershell_command(std::string_view stem) {
  const std::string command = ".\\" + std::string{stem} + ".exe";
  if (std::all_of(stem.begin(), stem.end(), plain_ascii)) return command;
  std::string quoted = "& '";
  for (std::size_t at = 0; at < command.size();) {
    const std::string_view rest = std::string_view{command}.substr(at);
    const std::size_t quote = single_quote_length(rest);
    const std::size_t length = quote == 0 ? 1 : quote;
    quoted.append(rest.substr(0, length));
    if (quote != 0) quoted.append(rest.substr(0, length));
    at += length;
  }
  return quoted + "'";
}

// 出力の場所。既にあるファイルなら書けない誤りとして止める（その中に作れないので）。
bool prepare_out_dir(const std::filesystem::path& out_dir) {
  std::error_code error;
  const auto status = std::filesystem::status(out_dir, error);
  if (std::filesystem::exists(status) && !std::filesystem::is_directory(status)) {
    report_write_error(out_dir, std::make_error_code(std::errc::not_a_directory));
    return false;
  }
  std::filesystem::create_directories(out_dir, error);
  if (error) {
    report_write_error(out_dir, error);
    return false;
  }
  return true;
}

bool write_replacing(const std::filesystem::path& archive, std::string_view bytes) {
  std::filesystem::path temp = archive;
  temp += ".tmp";
  std::error_code ignored;
  // 前に残った一時ファイルを先に消す。それがリンクなら、開くとリンクの先を切り詰めてしまう（remove はリンクそのものを消す）。
  std::filesystem::remove(temp, ignored);
  {
    std::ofstream out{temp, std::ios::binary | std::ios::trunc};
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    if (!out) {
      std::filesystem::remove(temp, ignored);
      report_write_error(archive, std::make_error_code(std::errc::io_error));
      return false;
    }
  }
  std::error_code error;
  std::filesystem::rename(temp, archive, error);
  if (error) {
    std::filesystem::remove(temp, ignored);
    report_write_error(archive, error);
    return false;
  }
  return true;
}

}  // namespace

std::string dist_start_command(std::string_view stem, HostOs os) {
  return os == HostOs::windows ? powershell_command(stem) : posix_command(stem);
}

std::optional<std::string_view> dist_name_clash(std::string_view exe_name) {
  // macOS と Windows の既定のファイルシステムは大文字小文字を区別しないので、license も LICENSE と重なる。
  const auto same = [](char left, char right) { return ascii_lower(left) == ascii_lower(right); };
  for (const std::string_view doc : k_doc_names) {
    if (std::equal(exe_name.begin(), exe_name.end(), doc.begin(), doc.end(), same)) return doc;
  }
  return std::nullopt;
}

bool dist_name_ok(std::string_view stem) {
  const std::string exe_name = std::string{stem} + exe_suffix();
  const std::optional<std::string_view> clash = dist_name_clash(exe_name);
  if (!clash) return true;
  std::cerr << mosac_message("mosac_dist_name_clash", {exe_name, std::string{*clash}}) << '\n';
  return false;
}

std::string dist_readme(std::string_view stem) {
  const std::vector<std::string> args{std::string{stem}, version_text(), dist_start_command(stem, host_os())};
  return render_message_fresh(MessageLanguage::En, "mosac_dist_readme", args) + "\n\n" +
         render_message_fresh(MessageLanguage::Ja, "mosac_dist_readme", args) + "\n";
}

int write_dist(const std::filesystem::path& exe, std::string_view stem, const std::filesystem::path& out_dir) {
  std::ifstream in{exe, std::ios::binary};
  if (!in.is_open()) {
    std::cerr << mosac_message("mosac_cannot_read", {utf8_from_path(exe)}) << '\n';
    return 1;
  }
  std::string exe_bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  if (!prepare_out_dir(out_dir)) return 1;
  const std::vector<ArchiveEntry> entries = dist_entries(std::move(exe_bytes), stem);
  const bool zip = host_os() == HostOs::windows;
  const std::string bytes = zip ? zip_archive(entries, now_seconds()) : tar_gz_archive(entries, now_seconds());
  const std::filesystem::path archive =
      out_dir / path_from_utf8(std::string{stem} + "-" + host_os_arch() + (zip ? ".zip" : ".tar.gz"));
  if (!write_replacing(archive, bytes)) return 1;
  std::cout << utf8_from_path(archive) << '\n';
  return 0;
}

}  // namespace mosa::codegen
