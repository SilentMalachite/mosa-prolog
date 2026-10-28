#include "mosa/path_text.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace mosa {
namespace {

// ディレクトリはソースではない。開けない名前へ .pl を足す前に、普通のファイルだけを受ける。
bool is_source_file(const std::filesystem::path& path) {
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) && !error;
}

}  // namespace

// 相対パスは、今読んでいるファイルのディレクトリから開く。文字だけの load は、渡されたパスのまま。
std::filesystem::path resolve_include(std::string_view directory, std::string_view atom) {
  const std::filesystem::path file = path_from_utf8(atom);
  if (file.is_absolute() || directory.empty()) return file;
  return path_from_utf8(directory) / file;
}

std::filesystem::path canonical_key(const std::filesystem::path& file) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(file, error);
  if (!error) return canonical;
  const auto absolute = std::filesystem::absolute(file, error);
  if (!error) return absolute.lexically_normal();
  return file.lexically_normal();
}

// Logtalk の ensure_loaded(file_1) は file_1.pl を指す。名前のまま開けるときはそれを使う。
// 既に拡張子がある名前は file.pl.pl にしない。開けない診断は、渡されたパスのまま残す。
std::filesystem::path prolog_source_path(const std::filesystem::path& file) {
  if (is_source_file(file) || !file.extension().empty()) return file;
  auto with_pl = file;
  with_pl.replace_extension(".pl");
  if (is_source_file(with_pl)) return with_pl;
  return file;
}

}  // namespace mosa
