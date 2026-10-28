#pragma once

// 原子の名前（UTF-8）とファイルのパスの相互変換。std::string からパスを作ると、Windows では
// ANSI コードページで読まれて日本語のファイル名が化ける。char8_t を通すと、どの OS でも UTF-8 として扱われる。
#include <filesystem>
#include <string>
#include <string_view>

namespace mosa {

[[nodiscard]] inline std::filesystem::path path_from_utf8(std::string_view text) {
  std::u8string bytes;
  bytes.reserve(text.size());
  for (const char byte : text) bytes.push_back(static_cast<char8_t>(byte));
  return std::filesystem::path{bytes};
}

// 区切りは / にそろえる。診断と読み込み済みの鍵に使う。
[[nodiscard]] inline std::string utf8_from_path(const std::filesystem::path& path) {
  const std::u8string bytes = path.generic_u8string();
  std::string text;
  text.reserve(bytes.size());
  for (const char8_t byte : bytes) text.push_back(static_cast<char>(byte));
  return text;
}

}
