#include "internal.hpp"

#include "mosa/path_text.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
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

namespace {

const EmbeddedSource* embedded_of(const Impl& machine, const std::filesystem::path& file) {
  if (machine.embedded.empty()) return nullptr;
  const auto key = utf8_from_path(canonical_key(file));
  for (const EmbeddedSource& source : machine.embedded) {
    if (source.key == key) return &source;
  }
  return nullptr;
}

}  // namespace

// 実行ファイルは埋め込んだ原文を先に引く（計画 Task 5）。埋め込みが無ければ今までどおりファイルを読む。
std::optional<std::string> read_source(const Impl& machine, const std::filesystem::path& file) {
  if (const EmbeddedSource* source = embedded_of(machine, file)) return source->text;
  std::ifstream input{file, std::ios::binary};
  if (!input) return std::nullopt;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

// 翻訳のときに .pl を足して開いた名前は、起動した場所にファイルが無くても同じパスにする。
std::filesystem::path source_path(const Impl& machine, const std::filesystem::path& file) {
  if (embedded_of(machine, file) != nullptr || !file.extension().empty()) return file;
  auto with_pl = file;
  with_pl.replace_extension(".pl");
  if (embedded_of(machine, with_pl) != nullptr) return with_pl;
  return prolog_source_path(file);
}

}  // namespace mosa
