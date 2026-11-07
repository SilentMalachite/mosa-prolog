// OS の表示言語（ADR 0007 改訂 1）。Windows だけ GetUserDefaultUILanguage を呼ぶ。
// テスト: tests/cpp/message_language_test.cpp（決定の部分。この呼び出しは Windows の CI で動く）
#include "mosa/ui_language.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace mosa {

bool system_ui_language_is_japanese() {
#ifdef _WIN32
  return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_JAPANESE;
#else
  return false;
#endif
}

}  // namespace mosa
