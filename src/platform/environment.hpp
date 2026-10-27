#pragma once

#include <string>

namespace mosa {

// テストが MOSA_LIB を向け替える。空文字は変数を消す。未設定の値は空文字。
[[nodiscard]] std::string environment_value(const char* name);
[[nodiscard]] bool set_environment(const char* name, const char* value);
void unset_environment(const char* name);

}  // namespace mosa
