// library(Name) の探索（SPEC 第5節、計画の Task 6）。$MOSA_LIB、次に実行ファイルのディレクトリの lib/。
// テスト: tests/cpp/library_path_test.cpp
#include "mosa/library_path.hpp"
#include "mosa/path_text.hpp"

#include "mosa/exe_path.hpp"

#include <string>
#include <system_error>

namespace mosa {

namespace {

std::optional<std::filesystem::path> source_in(const std::filesystem::path& dir, std::string_view name) {
  std::filesystem::path file = dir / path_from_utf8(name);
  // library(lists) は lists.pl を指す。拡張子を書いた名前はそのまま使う。
  if (file.extension() != ".pl") file += ".pl";
  std::error_code error;
  if (std::filesystem::is_regular_file(file, error) && !error) return file;
  return std::nullopt;
}

}  // namespace

std::optional<std::filesystem::path> find_library_in(std::string_view name,
                                                     const std::optional<std::filesystem::path>& env_dir,
                                                     const std::optional<std::filesystem::path>& exe_dir) {
  if (env_dir) {
    if (auto found = source_in(*env_dir, name)) return found;
  }
  if (exe_dir) return source_in(*exe_dir / "lib", name);
  return std::nullopt;
}

std::optional<std::filesystem::path> find_library(std::string_view name) {
  const auto exe = executable_path();
  const auto exe_dir = exe ? std::optional<std::filesystem::path>{exe->parent_path()} : std::nullopt;
  return find_library_in(name, environment_path("MOSA_LIB"), exe_dir);
}

}
