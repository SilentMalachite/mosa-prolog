#include "harness.hpp"
#include "mosa/char_class.hpp"
#include "mosa/lexer.hpp"

#include <string_view>
#include <vector>

namespace {

std::vector<mosa::Token> scan_all(std::string_view source, bool fullwidth = false) {
  mosa::Lexer lexer(source);
  lexer.set_fullwidth(fullwidth);
  std::vector<mosa::Token> tokens;
  for (;;) {
    const mosa::Scan scan = lexer.next();
    if (scan.failure) {
      mosa_test::check(false, "scan failed");
      return {};
    }
    tokens.push_back(scan.token);
    if (scan.token.kind == mosa::TokenKind::End ||
        scan.token.kind == mosa::TokenKind::EndFullStop) {
      return tokens;
    }
  }
}

void test_names_and_columns() {
  auto atoms = scan_all("foo.");
  mosa_test::check(atoms.size() == 2 && atoms[0].kind == mosa::TokenKind::Name &&
                       atoms[0].text == "foo" && atoms[1].kind == mosa::TokenKind::EndFullStop,
                   "atom and full stop");
  auto glued = scan_all("0X10.");
  mosa_test::check(glued.size() == 3 && glued[0].kind == mosa::TokenKind::Integer &&
                       glued[0].integer == 0 && glued[1].kind == mosa::TokenKind::Variable &&
                       glued[1].text == "X10",
                   "0X10 is not hex");
  auto cols = scan_all("あ.");
  mosa_test::check(cols.size() == 2 && cols[1].kind == mosa::TokenKind::EndFullStop &&
                       cols[1].line == 1 && cols[1].column == 2,
                   "column counts あ as one");
  mosa::Lexer crlf("a\r\nb");
  auto first = crlf.next();
  auto second = crlf.next();
  mosa_test::check(first.token.text == "a" && second.token.line == 2 && second.token.column == 1,
                   "crlf is one break");
  auto mark = mosa::Lexer("\u0301");
  auto bad_mark = mark.next();
  mosa_test::check(bad_mark.failure &&
                       bad_mark.failure->reason == mosa::SyntaxReason::IllegalCharacter,
                   "combining mark cannot start");
  auto composed = scan_all("e\u0301.");
  mosa_test::check(composed.size() == 2 && composed[0].kind == mosa::TokenKind::Name &&
                       composed[0].text == "e\u0301",
                   "mark continues a name");
  auto full = mosa::Lexer("（a）.");
  auto open = full.next();
  mosa_test::check(open.token.kind == mosa::TokenKind::Name && open.token.text == "（",
                   "fullwidth paren is a graphic name");
  auto bang = scan_all("!!.");
  mosa_test::check(bang.size() == 3 && bang[0].kind == mosa::TokenKind::Solo && bang[0].text == "!" &&
                       bang[1].kind == mosa::TokenKind::Solo && bang[1].text == "!" &&
                       bang[2].kind == mosa::TokenKind::EndFullStop,
                   "bang is not a graphic token");
  auto univ = scan_all("=.. .");
  mosa_test::check(univ.size() == 2 && univ[0].kind == mosa::TokenKind::Name && univ[0].text == "=..",
                   "univ is one graphic token");
  auto semi = scan_all(";;.");
  mosa_test::check(semi.size() == 3 && semi[0].kind == mosa::TokenKind::Solo && semi[0].text == ";" &&
                       semi[1].kind == mosa::TokenKind::Solo && semi[1].text == ";" &&
                       semi[2].kind == mosa::TokenKind::EndFullStop,
                   "semicolon is not a graphic token");
}

void test_invalid_utf8() {
  bool threw = false;
  try {
    mosa::Lexer bad("あ\xff");
    const auto name = bad.next();
    mosa_test::check(!name.failure && name.token.kind == mosa::TokenKind::Name &&
                         name.token.text == "あ",
                     "hiragana before bad byte");
    // 0xFF を読むのはこの next である。後続に残る失敗は見ない。
    const auto utf = bad.next();
    mosa_test::check(utf.failure && utf.failure->reason == mosa::SyntaxReason::InvalidUtf8 &&
                         utf.failure->line == 1 && utf.failure->column == 2 &&
                         utf.failure->token.empty(),
                     "invalid utf-8 column");
  } catch (...) {
    threw = true;
  }
  mosa_test::check(!threw, "invalid utf-8 does not throw");
}

void test_numbers() {
  auto hex = scan_all("0xFF.");
  mosa_test::check(hex[0].kind == mosa::TokenKind::Integer && hex[0].integer == 255, "0xFF");
  auto under = scan_all("1_000.");
  mosa_test::check(under[0].integer == 1000, "underscore digits");
  auto tail = mosa::Lexer("1_.");
  auto tail_scan = tail.next();
  mosa_test::check(tail_scan.failure &&
                       tail_scan.failure->reason == mosa::SyntaxReason::UnexpectedToken &&
                       tail_scan.failure->token == "1_",
                   "trailing underscore");
  auto doubled_under = mosa::Lexer("1__0.");
  auto doubled_scan = doubled_under.next();
  mosa_test::check(doubled_scan.failure &&
                       doubled_scan.failure->reason == mosa::SyntaxReason::UnexpectedToken,
                   "doubled underscore");
  auto var_under = scan_all("_1.");
  mosa_test::check(var_under[0].kind == mosa::TokenKind::Variable && var_under[0].text == "_1",
                   "underscore variable");
  auto code = scan_all("0'あ.");
  mosa_test::check(code[0].kind == mosa::TokenKind::Integer && code[0].integer == 0x3042, "0'");
  auto code2 = scan_all("0'𠮷.");
  mosa_test::check(code2[0].integer == 0x20BB7, "0' non-bmp");
  auto stop = scan_all("1.");
  mosa_test::check(stop.size() == 2 && stop[0].kind == mosa::TokenKind::Integer &&
                       stop[0].integer == 1 && stop[1].kind == mosa::TokenKind::EndFullStop,
                   "1. is integer and full stop");
  auto real = scan_all("1.5e-2.");
  mosa_test::check(real[0].kind == mosa::TokenKind::Float && real[0].floating == 1.5e-2, "float");
  auto big = mosa::Lexer("9223372036854775808.");
  auto big_scan = big.next();
  mosa_test::check(big_scan.failure &&
                       big_scan.failure->reason == mosa::SyntaxReason::IntegerOverflow,
                   "int64 overflow");
}

void test_quotes_and_comments() {
  auto nl = scan_all("'\\n'.");
  mosa_test::check(nl[0].text == "\n", "symbolic newline");
  auto hexesc = scan_all("'\\x41\\'.");
  mosa_test::check(hexesc[0].text == "A", "hex escape");
  auto uesc = scan_all("'\\u3042'.");
  mosa_test::check(uesc[0].text == "あ", "u escape");
  auto cont = scan_all("'ab\\\nc'.");
  mosa_test::check(cont[0].text == "abc", "line continuation");
  auto oct = scan_all("'\\141\\'.");
  mosa_test::check(oct[0].text == "a", "octal escape");
  auto doubled = scan_all("'a''b'.");
  mosa_test::check(doubled[0].text == "a'b", "doubled quote");
  auto grave = mosa::Lexer("`");
  auto grave_scan = grave.next();
  mosa_test::check(grave_scan.failure &&
                       grave_scan.failure->reason == mosa::SyntaxReason::IllegalCharacter,
                   "grave");
  auto comment = scan_all("/* /* */ foo.");
  mosa_test::check(comment[0].kind == mosa::TokenKind::Name && comment[0].text == "foo",
                   "block comment does not nest");
  auto line = scan_all("% foo\nbar.");
  mosa_test::check(line[0].text == "bar", "line comment");
  auto bom = scan_all("\uFEFFfoo.");
  mosa_test::check(bom[0].text == "foo", "leading BOM skipped");
}

void test_bad_underscores() {
  auto beside_dot = mosa::Lexer("1._2.");
  auto beside_dot_scan = beside_dot.next();
  mosa_test::check(beside_dot_scan.failure &&
                       beside_dot_scan.failure->reason == mosa::SyntaxReason::UnexpectedToken,
                   "underscore beside decimal point");
  auto beside_exp = mosa::Lexer("1.2e_3.");
  auto beside_exp_scan = beside_exp.next();
  mosa_test::check(beside_exp_scan.failure &&
                       beside_exp_scan.failure->reason == mosa::SyntaxReason::UnexpectedToken,
                   "underscore beside exponent");
  auto beside_sign = mosa::Lexer("1.2e+_3.");
  auto beside_sign_scan = beside_sign.next();
  mosa_test::check(beside_sign_scan.failure &&
                       beside_sign_scan.failure->reason == mosa::SyntaxReason::UnexpectedToken,
                   "underscore beside exponent sign");
}

void test_radix_and_comment_end() {
  auto binary = scan_all("0b10.");
  mosa_test::check(binary[0].kind == mosa::TokenKind::Integer && binary[0].integer == 2, "0b10");
  auto octal = scan_all("0o10.");
  mosa_test::check(octal[0].kind == mosa::TokenKind::Integer && octal[0].integer == 8, "0o10");
  auto upper_bin = scan_all("0B10.");
  mosa_test::check(upper_bin.size() == 3 && upper_bin[0].kind == mosa::TokenKind::Integer &&
                       upper_bin[0].integer == 0 && upper_bin[1].kind == mosa::TokenKind::Variable &&
                       upper_bin[1].text == "B10",
                   "0B10 is not binary");
  auto arrow = scan_all("->.");
  mosa_test::check(arrow.size() == 2 && arrow[0].kind == mosa::TokenKind::Name &&
                       arrow[0].text == "->",
                   "graphic arrow");
  auto end_comment = scan_all("a.% c\n");
  mosa_test::check(end_comment.size() == 2 && end_comment[1].kind == mosa::TokenKind::EndFullStop,
                   "end before %");
  mosa::Lexer after_end("a.% c\n");
  (void)after_end.next();
  (void)after_end.next();
  const mosa::Scan rest_scan = after_end.next();
  mosa_test::check(!rest_scan.failure && rest_scan.token.kind == mosa::TokenKind::End,
                   "comment after end is skipped");
  auto univ_comment = scan_all("X =..% c\n Y.");
  mosa_test::check(univ_comment.size() == 4 && univ_comment[1].text == "=..",
                   "=.. before % stays one name");
  auto fw_end_comment = scan_all("w（x）。% c", true);
  mosa_test::check(fw_end_comment.size() == 5 &&
                       fw_end_comment[4].kind == mosa::TokenKind::EndFullStop,
                   "fullwidth end before %");
  auto open_comment = mosa::Lexer("/* unterminated");
  auto comment_scan = open_comment.next();
  mosa_test::check(comment_scan.failure &&
                       comment_scan.failure->reason == mosa::SyntaxReason::UnterminatedComment &&
                       comment_scan.failure->token == "/*",
                   "unterminated comment");
}

void test_fullwidth() {
  mosa_test::check(mosa::halfwidth_punctuation(U'（') == U'(' && mosa::halfwidth_punctuation(U'）') == U')' &&
                       mosa::halfwidth_punctuation(U'［') == U'[' && mosa::halfwidth_punctuation(U'］') == U']' &&
                       mosa::halfwidth_punctuation(U'，') == U',' && mosa::halfwidth_punctuation(U'、') == U',' &&
                       mosa::halfwidth_punctuation(U'：') == U':',
                   "seven fullwidth marks map to ascii");
  mosa_test::check(!mosa::halfwidth_punctuation(U'。') && !mosa::halfwidth_punctuation(U'「') &&
                       !mosa::halfwidth_punctuation(U'」') && !mosa::halfwidth_punctuation(U'a'),
                   "kuten and kagi need context");
  auto call = scan_all("親（a）。", true);
  mosa_test::check(call.size() == 5 && call[1].kind == mosa::TokenKind::Solo && call[1].text == "(" &&
                       !call[1].after_layout && call[3].text == ")" &&
                       call[4].kind == mosa::TokenKind::EndFullStop && call[4].column == 5,
                   "fullwidth call reads as ascii");
  mosa::Lexer stop("a。 b", nullptr);
  stop.set_fullwidth(true);
  (void)stop.next();
  const auto kuten = stop.next();
  mosa_test::check(kuten.token.kind == mosa::TokenKind::EndFullStop && stop.offset() == 4,
                   "kuten consumes its three bytes");
  auto glued = scan_all("1。5。", true);
  mosa_test::check(glued.size() == 4 && glued[0].kind == mosa::TokenKind::Integer && glued[0].integer == 1 &&
                       glued[1].kind == mosa::TokenKind::Name && glued[1].text == "。" &&
                       glued[2].kind == mosa::TokenKind::Integer,
                   "kuten before a digit is a symbol, not a decimal point");
  auto kagi = scan_all("「a」」b」。", true);
  mosa_test::check(kagi.size() == 2 && kagi[0].kind == mosa::TokenKind::Name && kagi[0].text == "a」b",
                   "kagi quotes an atom and doubles its closer");
  mosa::Lexer open("「ab", nullptr);
  open.set_fullwidth(true);
  const auto unterminated = open.next();
  mosa_test::check(unterminated.failure &&
                       unterminated.failure->reason == mosa::SyntaxReason::UnterminatedQuote &&
                       unterminated.failure->token == "「",
                   "unterminated kagi names its opener");
  mosa::Lexer off("「a」.", nullptr);
  const auto off_scan = off.next();
  mosa_test::check(off_scan.failure && off_scan.failure->reason == mosa::SyntaxReason::IllegalCharacter,
                   "kagi stays illegal when the flag is off");
}

}  // namespace

int main() {
  test_names_and_columns();
  test_invalid_utf8();
  test_numbers();
  test_bad_underscores();
  test_quotes_and_comments();
  test_radix_and_comment_end();
  test_fullwidth();
  return mosa_test::failures();
}
