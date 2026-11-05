// writer.cpp から分けた字面の判断（AGENTS.md の 800 行の上限）。項を見ず、UTF-8 の字面だけを見る。

#include "spelling.hpp"

#include "mosa/char_class.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mosa::spelling {
namespace {

struct Decoded {
  char32_t code = 0;
  std::size_t bytes = 0;
  bool valid = false;
};

int continuation(std::string_view text, std::size_t index) {
  if (index >= text.size()) return -1;
  const auto byte = static_cast<unsigned char>(text[index]);
  if ((byte & 0xC0U) != 0x80U) return -1;
  return static_cast<int>(byte & 0x3FU);
}

Decoded decode_at(std::string_view text, std::size_t index) {
  if (index >= text.size()) return {};
  const auto lead = static_cast<unsigned char>(text[index]);
  if (lead <= 0x7FU) return Decoded{lead, 1, true};
  const int first = continuation(text, index + 1);
  if (first < 0) return {};
  if (lead <= 0xDFU && lead >= 0xC2U) {
    return Decoded{static_cast<char32_t>(((lead & 0x1FU) << 6) | first), 2, true};
  }
  const int second = continuation(text, index + 2);
  if (second < 0) return {};
  if (lead >= 0xE0U && lead <= 0xEFU) {
    const auto code = static_cast<char32_t>(((lead & 0x0FU) << 12) | (first << 6) | second);
    return Decoded{code, 3, true};
  }
  const int third = continuation(text, index + 3);
  if (third < 0 || lead < 0xF0U || lead > 0xF4U) return {};
  const auto code = static_cast<char32_t>(((lead & 0x07U) << 18) | (first << 12) | (second << 6) | third);
  return Decoded{code, 4, true};
}

bool every_code(std::string_view text, bool (*ok)(char32_t)) {
  if (text.empty()) return false;
  for (std::size_t index = 0; index < text.size();) {
    const Decoded decoded = decode_at(text, index);
    if (!decoded.valid || !ok(decoded.code)) return false;
    index += decoded.bytes;
  }
  return true;
}

bool is_graphic_token(std::string_view text) {
  return every_code(text, [](char32_t code) { return classify(code) == CharClass::Graphic; });
}

bool is_small_name(std::string_view text) {
  const auto first = first_cp(text);
  if (!first || classify(*first) != CharClass::Small) return false;
  const Decoded decoded = decode_at(text, 0);
  return every_code(text.substr(decoded.bytes), continues_name) || text.size() == decoded.bytes;
}

// 全角記号モードの字句解析器は引用の外の （），、： を半角に読み、図形の字句の先頭にあって後ろに空白か % か終わりが
// 来る 。 を終止符にする（SPEC 第4節、レビュー L17）。原子の中の 。 の後ろは図形の字なので、見るのは最後の 。 だけ。
// 引用が要るのは 。 1 字の原子だけだが、+。 のように 。 で終わる原子も M8 のとおり引用する。引用しても読み戻せ、
// 字面は L17 の前と変わらない（iso-notes 6.4.2）。［］「」 は図形でなく、もう引用される。
bool fullwidth_changes(std::string_view name) {
  for (std::size_t index = 0; index < name.size();) {
    const Decoded decoded = decode_at(name, index);
    if (!decoded.valid) return false;
    if (halfwidth_punctuation(decoded.code)) return true;
    index += decoded.bytes;
  }
  return name.ends_with("。");
}

// 十六進は制御文字だけ。引用の外で字句にならない文字でも、字形があれば生のまま。
bool unprintable(char32_t code) {
  return code < 0x20 || code == 0x7F || (code >= 0x80 && code < 0xA0);
}

// スイートは \x2\ と \x11\ の字面を比べる。桁を揃えると不一致になる。
std::string hex_escape(char32_t code) {
  constexpr std::string_view digits = "0123456789abcdef";
  std::string body;
  auto value = static_cast<std::uint32_t>(code);
  do {
    body.push_back(digits[value & 0xFU]);
    value >>= 4;
  } while (value != 0);
  std::string out = "\\x";
  for (auto it = body.rbegin(); it != body.rend(); ++it) out.push_back(*it);
  out.push_back('\\');
  return out;
}

}  // namespace

std::optional<char32_t> first_cp(std::string_view text) {
  const Decoded decoded = decode_at(text, 0);
  if (!decoded.valid) return std::nullopt;
  return decoded.code;
}

bool continues_name(char32_t code) {
  const CharClass kind = classify(code);
  return kind == CharClass::Small || kind == CharClass::Capital || kind == CharClass::Underline ||
         kind == CharClass::Digit || kind == CharClass::Mark;
}

std::optional<char32_t> final_cp(std::string_view text) {
  if (text.empty()) return std::nullopt;
  std::size_t start = text.size() - 1;
  while (start > 0 && text.size() - start < 4 && (static_cast<unsigned char>(text[start]) & 0xC0U) == 0x80U) {
    --start;
  }
  const Decoded decoded = decode_at(text, start);
  if (!decoded.valid || start + decoded.bytes != text.size()) return std::nullopt;
  return decoded.code;
}

// ! と ; 以外の単独文字は、引用しないと括弧や区切りとして読まれる。
// 図形の先頭の /* は読み戻すとブロック注釈になる。途中の /* と */ は図形のまま読める。
// 単独の . は終止符なので、図形でも引用しないとアトムとして読めない。
// 7.10.5 h は引用しないと原子として読めないときだけ引用する。全角記号は旗が真の読み手でだけ別の字句になる。
bool bare_atom(std::string_view name, bool fullwidth) {
  if (name == "[]" || name == "{}") return true;
  if (is_small_name(name)) return true;
  if (is_graphic_token(name)) return name != "." && !name.starts_with("/*") && !(fullwidth && fullwidth_changes(name));
  return name == "!" || name == ";";
}

std::string quote_atom(std::string_view text) {
  std::string out = "'";
  for (std::size_t index = 0; index < text.size();) {
    const Decoded decoded = decode_at(text, index);
    if (!decoded.valid) break;
    if (decoded.code == U'\\') out += "\\\\";
    else if (decoded.code == U'\'') out += "\\'";
    else if (decoded.code == U'\n') out += "\\n";
    else if (decoded.code == U'\t') out += "\\t";
    else if (unprintable(decoded.code)) out += hex_escape(decoded.code);
    else out.append(text.substr(index, decoded.bytes));
    index += decoded.bytes;
  }
  out += '\'';
  return out;
}

std::string format_float(double value) {
  if (value == 0.0 && std::signbit(value)) return "-0.0";
  std::array<char, 128> buffer{};
  const std::to_chars_result result =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general);
  std::string text = result.ec == std::errc{} ? std::string(buffer.data(), result.ptr) : "0.0";
  for (char& ch : text) {
    if (ch == 'E') ch = 'e';
  }
  if (text.find('.') == std::string::npos) {
    const auto exponent = text.find('e');
    if (exponent == std::string::npos) text += ".0";
    else text.insert(exponent, ".0");
  }
  return text;
}

}  // namespace mosa::spelling
