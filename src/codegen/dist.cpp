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

#include <chrono>
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

// 書庫を固めた OS で動かす命令。Windows のコマンドプロンプトは今のディレクトリの実行ファイルを ./ 無しで探す。
std::string start_command(std::string_view stem) {
  if (host_os() == HostOs::windows) return std::string{stem} + exe_suffix();
  return "./" + std::string{stem};
}

std::int64_t now_seconds() {
  const auto since = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::seconds>(since).count();
}

std::vector<ArchiveEntry> dist_entries(std::string exe_bytes, std::string_view stem) {
  const std::string folder = std::string{stem} + "-" + host_os_arch() + "/";
  return {
      {folder + std::string{stem} + exe_suffix(), std::move(exe_bytes), true},
      {folder + "LICENSE", std::string{k_mosa_license}, false},
      {folder + "NOTICE", std::string{k_mosa_notice}, false},
      {folder + "README.txt", dist_readme(stem), false},
  };
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

std::string dist_readme(std::string_view stem) {
  const std::vector<std::string> args{std::string{stem}, version_text(), start_command(stem)};
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
