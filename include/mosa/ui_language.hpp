#pragma once

// OS の表示言語（ADR 0007 追補 1）。実装は src/platform/ui_language.cpp。
namespace mosa {

// Windows は GetUserDefaultUILanguage の主言語が日本語か。ほかの OS は常に false（LANG だけで決める）。
[[nodiscard]] bool system_ui_language_is_japanese();

}  // namespace mosa
