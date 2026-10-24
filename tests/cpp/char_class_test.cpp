#include "harness.hpp"
#include "mosa/char_class.hpp"

int main() {
  mosa_test::check(mosa::unicode_version() == "18.0.0", "unicode 18.0.0");
  mosa_test::check(mosa::classify(U'a') == mosa::CharClass::Small, "a");
  mosa_test::check(mosa::classify(U'Z') == mosa::CharClass::Capital, "Z");
  mosa_test::check(mosa::classify(U'_') == mosa::CharClass::Underline, "_");
  mosa_test::check(mosa::classify(U'7') == mosa::CharClass::Digit, "7");
  mosa_test::check(mosa::classify(U'+') == mosa::CharClass::Graphic, "+");
  mosa_test::check(mosa::classify(U'!') == mosa::CharClass::Solo, "!");
  mosa_test::check(mosa::classify(U';') == mosa::CharClass::Solo, ";");
  mosa_test::check(mosa::classify(U'`') == mosa::CharClass::Illegal, "grave");
  mosa_test::check(mosa::classify(U'\u3042') == mosa::CharClass::Small, "親の類 あ");
  mosa_test::check(mosa::classify(U'\u89aa') == mosa::CharClass::Small, "親");
  mosa_test::check(mosa::classify(U'\u00c4') == mosa::CharClass::Capital, "A-umlaut");
  // \u は 4 桁なので、計画の U'\u20bb7' は U+20BB7 にならない。同じ検査を \U で行う。
  // mosa_test::check(mosa::classify(U'\u20bb7') == mosa::CharClass::Small, "𠮷");
  mosa_test::check(mosa::classify(U'\U00020bb7') == mosa::CharClass::Small, "𠮷");
  mosa_test::check(mosa::classify(U'\u3000') == mosa::CharClass::Layout, "ideographic space");
  mosa_test::check(mosa::classify(U'\u0085') == mosa::CharClass::Layout, "next line");
  mosa_test::check(mosa::classify(U'\ufeff') == mosa::CharClass::Illegal, "feff");
  mosa_test::check(mosa::classify(U'\uff10') != mosa::CharClass::Digit, "fullwidth digit");
  mosa_test::check(mosa::classify(U'\u0301') == mosa::CharClass::Mark, "combining acute");
  mosa_test::check(mosa::classify(U'\uff08') == mosa::CharClass::Graphic, "fullwidth paren");
  mosa_test::check(mosa::east_asian_wide(U'\u4e00'), "CJK wide");
  mosa_test::check(!mosa::east_asian_wide(U'A'), "A narrow");
  return mosa_test::failures();
}
