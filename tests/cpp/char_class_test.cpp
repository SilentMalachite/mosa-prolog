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
  mosa_test::check(mosa::classify(U'\uff09') == mosa::CharClass::Graphic, "fullwidth close paren");
  mosa_test::check(mosa::classify(U'\uff3f') == mosa::CharClass::Graphic, "fullwidth underline");
  mosa_test::check(mosa::classify(U'\u3008') == mosa::CharClass::Illegal, "left angle bracket");
  mosa_test::check(mosa::classify(U'\u00ab') == mosa::CharClass::Illegal, "pi guillemet");
  mosa_test::check(mosa::classify(U'\u00bb') == mosa::CharClass::Illegal, "pf guillemet");
  mosa_test::check(mosa::east_asian_wide(U'\u4e00'), "CJK wide");
  mosa_test::check(!mosa::east_asian_wide(U'A'), "A narrow");
  // 単純大文字化（UnicodeData.txt の 12 列目）。写像の無い文字はそのまま。
  mosa_test::check(mosa::simple_upper(U'a') == U'A', "upper a");
  mosa_test::check(mosa::simple_upper(U'A') == U'A', "upper A kept");
  mosa_test::check(mosa::simple_upper(U'1') == U'1', "upper digit kept");
  mosa_test::check(mosa::simple_upper(U'\u03c3') == U'\u03a3', "upper sigma");
  mosa_test::check(mosa::simple_upper(U'\u03c2') == U'\u03a3', "upper final sigma");
  mosa_test::check(mosa::simple_upper(U'\u00ff') == U'\u0178', "upper y diaeresis");
  mosa_test::check(mosa::simple_upper(U'\u00df') == U'\u00df', "upper sharp s kept");
  mosa_test::check(mosa::simple_upper(U'\u3042') == U'\u3042', "upper kana kept");
  mosa_test::check(mosa::simple_upper(U'\uff41') == U'\uff21', "upper fullwidth a");
  mosa_test::check(mosa::simple_upper(U'\U00010428') == U'\U00010400', "upper deseret");
  mosa_test::check(mosa::simple_upper(U'\U0010FFFF') == U'\U0010FFFF', "upper last kept");
  return mosa_test::failures();
}
