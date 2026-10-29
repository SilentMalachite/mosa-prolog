#pragma once

// 計画 Task 5、Task 10。project.cpp（読み込み、単位と同梱ライブラリの選び方）、project_main.cpp（main.cpp と
// CMakeLists.txt の文面）、check.cpp（静的な警告）が共有する。

#include "../machine/internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mosa::codegen {

// TranslationLog::files と同じ添字で、診断に書くファイルの名前。根はコマンド行のまま、ほかは今のディレクトリからの相対。
using DisplayNames = std::vector<std::string>;

// 起動時に install_unit で付ける単位。プログラムのモジュールごとに 1 つで、根のファイルのモジュールが先。
struct UnitPlan {
  std::uint32_t module = 0;
  std::size_t file = 0;
};

// 同梱ライブラリ（計画 Task 10）。name は library(Name) の Name（ファイルの語幹）で、実行ファイルは
// rt::bundle_library にこの名前で登録する。prebuilt なら翻訳済みの単位 pl/libmosa_pl_<name> を結んで
// mosa_gen::m<mangle(module)>::unit を渡し、偽なら原文だけを同梱して IR のまま動かす。
struct LibraryPlan {
  std::string name;
  std::size_t file = 0;
  std::uint32_t module = 0;
  bool prebuilt = false;
};

struct ProjectPlan {
  std::vector<UnitPlan> units;
  std::vector<LibraryPlan> libraries;
  // files と同じ添字。実行ファイルに埋め込む鍵で、翻訳した機械の絶対パスは残さない（project.cpp の embed_keys）。
  std::vector<std::string> keys;
};

[[nodiscard]] std::optional<std::size_t> log_file(const TranslationLog& log, std::string_view key);
[[nodiscard]] std::vector<std::string> export_warnings(Impl& machine, const TranslationLog& log,
                                                       const DisplayNames& names);
[[nodiscard]] std::vector<std::string> call_warnings(Impl& machine, const TranslationLog& log,
                                                     const DisplayNames& names);

// 定義は project.cpp。mosac emit --units（units.cpp、Task 11）が同じ選び方で単位と同梱ライブラリを決める。
[[nodiscard]] std::vector<UnitPlan> plan_units(const Impl& machine, const TranslationLog& log);
[[nodiscard]] std::vector<std::string> embed_keys(const TranslationLog& log);
[[nodiscard]] std::string library_name(std::string_view key);
[[nodiscard]] std::vector<PredKey> opaque_preds(const TranslationLog& log);
[[nodiscard]] bool load_catalogs(Machine& machine);

// 定義は project_main.cpp。
[[nodiscard]] std::string unit_stem(const Impl& machine, std::uint32_t module);
// 原文 text を constexpr std::array<std::string_view, N> kFile<index> の断片の並びにする（MSVC の字面の上限のため）。
[[nodiscard]] std::string source_array(std::size_t index, std::string_view text);
// 同梱ライブラリの単位を結ばない CMakeLists.txt。sources は空白で区切った原文の名前（mosac emit --units、Task 11）。
[[nodiscard]] std::string cmake_plain_text(std::string_view target, std::string_view sources, std::string_view runtime_dir);
[[nodiscard]] std::string main_text(const Impl& machine, const TranslationLog& log, const ProjectPlan& plan,
                                        bool interactive = false);
[[nodiscard]] std::string cmake_text(const Impl& machine, const ProjectPlan& plan, std::string_view target,
                                     std::string_view runtime_dir);

}  // namespace mosa::codegen
