#include "harness.hpp"
#include "mosa/bigint.hpp"
#include "mosa/char_class.hpp"
#include "mosa/char_conv.hpp"
#include "mosa/lexer.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <string>
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
}

// フェーズ10（ADR 0025）。int64 に入らない整数の字面は溢れにせず、大きさを Big で持つ整数のトークンになる。
// 2^63 も - の有無に依らず Big で、符号を付けて IntBox の min_integer に戻すのは読む側（6.3.4.1）。
void test_big_numbers() {
  auto plain = scan_all("9223372036854775807.");
  mosa_test::check(plain.size() == 2 && !plain[0].big && plain[0].integer == std::numeric_limits<std::int64_t>::max(),
                   "max_integer is plain");
  auto zeros = scan_all("0009223372036854775807.");
  mosa_test::check(zeros.size() == 2 && !zeros[0].big, "leading zeros keep max_integer plain");
  struct Case {
    std::string_view source;
    std::string_view value;
  };
  const std::array<Case, 8> cases{{
      {"9223372036854775808.", "9223372036854775808"},
      {"0x8000000000000000.", "9223372036854775808"},
      {"0o1000000000000000000000.", "9223372036854775808"},
      {"0b10000000000000000000000000000000000000000000000000000000000000000.", "18446744073709551616"},
      {"9223372036854775809.", "9223372036854775809"},
      {"92233720368547758080.", "92233720368547758080"},
      {"0xFFFF_ffff_FFFF_ffff_FFFF.", "1208925819614629174706175"},
      {"1_000_000_000_000_000_000_000.", "1000000000000000000000"},
  }};
  for (const Case& item : cases) {
    auto tokens = scan_all(item.source);
    const bool ok = tokens.size() == 2 && tokens[0].kind == mosa::TokenKind::Integer && tokens[0].big &&
                    !tokens[0].big->negative() && mosa::to_text(*tokens[0].big) == item.value &&
                    tokens[1].kind == mosa::TokenKind::EndFullStop;
    mosa_test::check(ok, std::string("big token: ") + std::string(item.source));
  }
  // 整数部が int64 を越えても、小数部が続けば浮動小数点数である。
  auto real = scan_all("92233720368547758080.5.");
  mosa_test::check(real.size() == 2 && real[0].kind == mosa::TokenKind::Float && !real[0].big &&
                       real[0].floating == 92233720368547758080.5,
                   "big integer part of a float");
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
  // -> と終止符の間に空白を置く。続けた ->. は 1 つの名前になる（レビュー L17、test_graphic_full_stop）。
  auto arrow = scan_all("-> .");
  mosa_test::check(arrow.size() == 2 && arrow[0].kind == mosa::TokenKind::Name &&
                       arrow[0].text == "->" && arrow[1].kind == mosa::TokenKind::EndFullStop,
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

// レビュー L17（持ち主は A を選んだ、iso-notes 6.4.2）。6.4.2 が図形トークンにしないのは、空白か % か終わりの前の
// . 1 字だけである。前は図形の字の後ろの . もそこで切り、+. を + と終止符に読んだ。
bool names_then(const std::vector<mosa::Token>& tokens, std::vector<std::string_view> names, mosa::TokenKind last) {
  if (tokens.size() != names.size() + 1 || tokens.back().kind != last) return false;
  for (std::size_t index = 0; index < names.size(); ++index) {
    if (tokens[index].kind != mosa::TokenKind::Name || tokens[index].text != names[index]) return false;
  }
  return true;
}

void test_graphic_full_stop() {
  using mosa::TokenKind;
  mosa_test::check(names_then(scan_all("+.\n."), {"+."}, TokenKind::EndFullStop), "+. before a newline is one name");
  mosa_test::check(names_then(scan_all("+. ."), {"+."}, TokenKind::EndFullStop), "+. before a space is one name");
  mosa_test::check(names_then(scan_all("+.% c\n."), {"+."}, TokenKind::EndFullStop), "+. before % is one name");
  mosa_test::check(names_then(scan_all("->."), {"->."}, TokenKind::End), "->. at the end of input is one name");
  mosa_test::check(names_then(scan_all("#. a."), {"#.", "a"}, TokenKind::EndFullStop), "#. then a name");
  mosa_test::check(names_then(scan_all("+.. ."), {"+.."}, TokenKind::EndFullStop), "+.. is one name");
  mosa_test::check(names_then(scan_all("+ ."), {"+"}, TokenKind::EndFullStop), "a space keeps the full stop");
  mosa_test::check(names_then(scan_all("=.. ."), {"=.."}, TokenKind::EndFullStop), "=.. stays one name");
  mosa_test::check(names_then(scan_all(".. ."), {".."}, TokenKind::EndFullStop), ".. stays one name");
  // 先頭の . は今までどおり終止符。後ろが空白でなければ図形の字。
  mosa_test::check(names_then(scan_all("a. "), {"a"}, TokenKind::EndFullStop), "a dot after a name ends");
  mosa_test::check(names_then(scan_all(".+ ."), {".+"}, TokenKind::EndFullStop), "a dot before + starts a name");
  // 全角記号モードの 。 も同じ規則（決定の 2026-11-03）。
  mosa_test::check(names_then(scan_all("+。 。", true), {"+。"}, TokenKind::EndFullStop), "+。 is one name");
  mosa_test::check(names_then(scan_all("+。\n", true), {"+。"}, TokenKind::End), "+。 before a newline");
  mosa_test::check(names_then(scan_all("+.。", true), {"+.。"}, TokenKind::End), "+.。 is one name");
  mosa_test::check(names_then(scan_all("+ 。", true), {"+"}, TokenKind::EndFullStop), "a space keeps the kuten");
  // 旗が偽なら 。 はただの図形の字で、終止符にならない。
  mosa_test::check(names_then(scan_all("+。 ."), {"+。"}, TokenKind::EndFullStop), "+。 without the flag");
}

// 溢れの向きは指数の符号ではなく値の桁で決まる。仮数部が 400 桁あると、e- でも上位、e の無い 0.000…1 でも
// 下位に溢れる（M2 の敵対的レビュー A1/A2）。
void test_float_range_long_mantissa() {
  const std::string zeros(400, '0');
  for (const std::string& text : {"1" + zeros + ".0e-10.", "1" + zeros + ".0.", "0.0" + zeros + "1e800."}) {
    auto big = mosa::Lexer(text);
    auto big_scan = big.next();
    mosa_test::check(big_scan.failure && big_scan.failure->reason == mosa::SyntaxReason::FloatOverflow,
                     "long mantissa overflow: " + text.substr(text.size() - 8));
  }
  for (const std::string& text : {"0." + zeros + "1.", "0." + zeros + "1e5.", "0." + zeros + "1e+5.",
                                  "1" + zeros + ".0e-800."}) {
    auto tiny = scan_all(text);
    mosa_test::check(!tiny.empty() && tiny[0].kind == mosa::TokenKind::Float && tiny[0].floating == 0.0,
                     "long mantissa underflow: " + text.substr(text.size() - 8));
  }
}

// レビュー M2（持ち主は A を選んだ、iso-notes 6.4.5）。double に入らない浮動小数点数は inf にせず、整数の
// 溢れと同じく字句の誤りにする。下位の溢れは 0.0、非正規化数と最大値に丸まる綴りは今までどおり数である。
void test_float_range() {
  for (const char* text : {"1.0e400.", "1.8e308.", "1.0e+400.", "1_0.0e400.", "1.0E400."}) {
    auto big = mosa::Lexer(text);
    auto big_scan = big.next();
    mosa_test::check(big_scan.failure && big_scan.failure->reason == mosa::SyntaxReason::FloatOverflow,
                     std::string("float overflow: ") + text);
  }
  auto tiny = scan_all("1.0e-400.");
  mosa_test::check(tiny[0].kind == mosa::TokenKind::Float && tiny[0].floating == 0.0, "underflow is 0.0");
  auto subnormal = scan_all("1.0e-310.");
  mosa_test::check(subnormal[0].floating > 0.0 && subnormal[0].floating < 1.0e-300, "subnormal kept");
  auto top = scan_all("1.7976931348623158e308.");
  mosa_test::check(top[0].floating == std::numeric_limits<double>::max(), "rounds to max");
  test_float_range_long_mantissa();
}

// 標準入力から 1 バイトずつ足して読むときの読み直しの目印（read_pred.cpp の intake_term）。最後の字が終止符の後ろに
// 来うる字で、その前の字が終止符になりうるときだけ真。写像は next と同じく char_conversion と全角の旗で決める。
void test_end_marks() {
  const auto marks = [](std::string_view source, bool fullwidth = false, const mosa::CharConv* conv = nullptr) {
    return mosa::Lexer::may_end_before_last(source, conv, fullwidth);
  };
  mosa_test::check(marks("a. ") && marks("a.\n") && marks("a.\t") && marks("a.%") && marks(". "),
                   "dot then layout or percent marks an end");
  mosa_test::check(marks("a.\r") && !marks("a.\r\n"), "the end is marked at the carriage return");
  mosa_test::check(marks("a.\xE3\x80\x80"), "dot then an ideographic space marks an end");
  mosa_test::check(!marks("a.\xE3\x80"), "an unfinished follower does not mark an end");
  mosa_test::check(!marks("") && !marks(".") && !marks("a.") && !marks("1.5") && !marks("a b") && !marks("a.b"),
                   "no dot and follower, no mark");
  mosa_test::check(marks("a.\n", true), "an ascii end is marked under the flag too");
  mosa_test::check(marks("\xE8\xA6\xAA\xE3\x80\x82\n", true) && !marks("\xE8\xA6\xAA\xE3\x80\x82\n", false),
                   "kuten ends only under the flag");
  mosa::CharConv conv;
  conv.enabled = true;
  conv.map.emplace_back(U'!', U'.');
  mosa_test::check(marks("a!\n", false, &conv) && !marks("a!\n"), "a char converted to a dot marks an end");
  conv.map.clear();
  conv.map.emplace_back(U'.', U'x');
  mosa_test::check(!marks("a.\n", false, &conv), "a dot converted away does not mark an end");
}

// 標準入力の read の下見（read_pred.cpp の pull_to_mark）。読み手が止まりうる所があれば真。なければ、終わりで切れた
// 空白の始まりか、引用か注釈の中で最後に読み始めた字か escape へ from を進め、その引用か注釈を inside に置く。
using Inside = mosa::Lexer::Inside;
using Scout = mosa::Lexer::Scout;

struct Scouted {
  bool stop = false;
  Scout state;
};

Scouted scout(std::string_view source, Scout from = {}, bool fullwidth = false, const mosa::CharConv* conv = nullptr) {
  Scouted seen{false, from};
  seen.stop = mosa::Lexer::scout(source, conv, fullwidth, seen.state);
  return seen;
}

bool waits(const Scouted& seen, std::size_t from, Inside inside = Inside::None, char32_t quote = 0) {
  return !seen.stop && seen.state.from == from && seen.state.inside == inside && seen.state.quote == quote;
}

void test_scout() {
  mosa_test::check(scout("a. ").stop && scout("f(x) .\n").stop && scout("a b. ").stop, "an end stops");
  mosa_test::check(waits(scout("'a. "), 3, Inside::Quote, U'\'') && waits(scout("f('a. "), 5, Inside::Quote, U'\'') &&
                       waits(scout("\"a. "), 3, Inside::Quote, U'"'),
                   "an end inside quotes waits inside the quotes");
  mosa_test::check(waits(scout("/* a. "), 5, Inside::Block), "an end inside a block comment waits inside it");
  mosa_test::check(waits(scout("x % a. "), 6, Inside::Line), "an end inside a line comment waits inside it");
  mosa_test::check(waits(scout("f(a, "), 4) && waits(scout("X =.. "), 5), "no end, read again from the layout");
  const Scout quoted{5, Inside::Quote, U'\''};
  const std::string_view two = "f('a. b', 'c. ";
  mosa_test::check(scout("f('a. b'). ", quoted).stop && waits(scout(two, quoted), two.size() - 1, Inside::Quote, U'\''),
                   "the next scout goes on inside the quotes");
  mosa_test::check(waits(scout("'a\\'. "), 5, Inside::Quote, U'\'') && waits(scout("'a''. "), 5, Inside::Quote, U'\''),
                   "escaped and doubled quotes do not close");
  mosa_test::check(scout("'a\\'. b'. ", quoted).stop, "a later quote closes");
  const Scouted carriage = scout("'a\\\r");
  mosa_test::check(waits(carriage, 2, Inside::Quote, U'\''), "a continuation at the end is read again");
  const Scouted joined = scout("'a\\\r\nb'. ", carriage.state);
  mosa_test::check(joined.stop && joined.state.from == 2, "the read again continuation takes the line feed");
  const Scouted star = scout("/* a *");
  mosa_test::check(waits(star, 5, Inside::Block) && scout("/* a */ b. ", star.state).stop,
                   "a star at the end is read again");
  mosa_test::check(waits(scout("\xE3\x80\x8C" "a\xE3\x80\x82 ", {}, true), 7, Inside::Quote, U'\u300D'),
                   "a corner bracket waits for its close under the flag");
  mosa::CharConv conv;
  conv.enabled = true;
  conv.map.emplace_back(U'!', U'\'');
  mosa_test::check(waits(scout("!a. ", {}, false, &conv), 3, Inside::Quote, U'\''),
                   "a char converted to a quote opens quotes");
  const Scouted broken = scout("'a\nb. ");
  mosa_test::check(broken.stop && waits(Scouted{false, broken.state}, 2),
                   "a failure stops, and the next scout starts after it");
  const Scouted resumed = scout("'a\nb. ", Scout{2, Inside::Quote, U'\''});
  mosa_test::check(resumed.stop && waits(Scouted{false, resumed.state}, 2),
                   "a failure inside resumed quotes skips nothing");
}

}  // namespace

int main() {
  test_names_and_columns();
  test_invalid_utf8();
  test_numbers();
  test_big_numbers();
  test_float_range();
  test_bad_underscores();
  test_quotes_and_comments();
  test_radix_and_comment_end();
  test_fullwidth();
  test_graphic_full_stop();
  test_end_marks();
  test_scout();
  return mosa_test::failures();
}
