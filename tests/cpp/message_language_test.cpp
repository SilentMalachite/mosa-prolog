#include "harness.hpp"
#include "mosa/messages.hpp"

#include <optional>

// ADR 0007 追補 1。LANG が空でなければそれが決め、無いか空なら表示言語が決める。
int main() {
  using mosa::MessageLanguage;
  using mosa::message_language_from;
  const auto ja = MessageLanguage::Ja;
  const auto en = MessageLanguage::En;
  mosa_test::check(message_language_from("ja_JP.UTF-8", false) == ja, "ja LANG wins over non-ja display");
  mosa_test::check(message_language_from("en_US.UTF-8", true) == en, "en LANG wins over ja display");
  mosa_test::check(message_language_from("C", true) == en, "C LANG wins over ja display");
  mosa_test::check(message_language_from(std::nullopt, true) == ja, "absent LANG follows ja display");
  mosa_test::check(message_language_from("", true) == ja, "empty LANG follows ja display");
  mosa_test::check(message_language_from(std::nullopt, false) == en, "absent LANG, other display");
  mosa_test::check(message_language_from("", false) == en, "empty LANG, other display");
  return mosa_test::failures();
}
