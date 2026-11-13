#include "internal.hpp"

#include "mosa/path_text.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
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

// 実行ファイルは埋め込んだ原文を先に引く（計画 Task 5）。埋め込みが無ければファイルを読む。
// consult の原文は通常ファイルだけ（全体レビュー推奨2、持ち主の選択 B）。装置や名前付きパイプは終わりが
// 無いことがあり、全体読みがメモリを食い続けた。大きさはヒープ上限のバイト予算で止める。原文そのものは
// 節になればヒープへ入るので、予算を超える原文はどのみち上限に当たる。
std::optional<std::string> read_source(const Impl& machine, const std::filesystem::path& file) {
  if (const EmbeddedSource* source = embedded_of(machine, file)) return source->text;
  // ifstream はディレクトリも開けることがあり、consult が空のファイルとして黙って成功した（レビュー L5）。
  std::error_code kind;
  if (!std::filesystem::is_regular_file(file, kind) || kind) return std::nullopt;
  const std::size_t budget = heap_byte_budget(machine.heap.limit());
  std::error_code size_code;
  const auto size = std::filesystem::file_size(file, size_code);
  if (size_code) return std::nullopt;
  if (size > budget) throw ResourceLimit("heap_limit");
  std::ifstream input{file, std::ios::binary};
  if (!input) return std::nullopt;
  std::string text;
  text.reserve(static_cast<std::size_t>(size));
  std::array<char, 65536> chunk{};
  while (input) {
    input.read(chunk.data(), static_cast<std::streamsize>(chunk.size()));
    const auto got = static_cast<std::size_t>(input.gcount());
    if (got == 0) break;
    // 読む間に伸びた分も予算で止める。
    if (got > budget - std::min(budget, text.size())) throw ResourceLimit("heap_limit");
    text.append(chunk.data(), got);
  }
  return text;
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
