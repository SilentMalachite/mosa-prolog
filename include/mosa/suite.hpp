#pragma once

#include <string_view>

namespace mosa {

[[nodiscard]] int run_suite(std::string_view path);

// test/3 のディレクトリ実行とは別入口。報告書を書いて終了コードだけ返す。
[[nodiscard]] int run_iso_suite(std::string_view suite_dir, std::string_view report_path);

}
