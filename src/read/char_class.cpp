#include "mosa/char_class.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace mosa {
namespace {

struct CategoryRange {
  char32_t first;
  char32_t last;
  char major;
  char minor;
};

struct WideRange {
  char32_t first;
  char32_t last;
};

struct UpperRange {
  char32_t first;
  char32_t last;
  std::int32_t delta;
};

#include "../unicode_tables.inc"

// 6.5 の ASCII 分類は一般カテゴリと違う。`;` は Po だが solo、`_` は Pc だが下線、
// バッククォートは図形でも引用でもない。生成表は見ない。
constexpr std::array<CharClass, 128> make_ascii_table() {
  std::array<CharClass, 128> table{};
  for (CharClass& entry : table) {
    entry = CharClass::Illegal;
  }
  for (char32_t code : {U'\t', U'\n', U'\v', U'\f', U'\r', U' '}) {
    table[code] = CharClass::Layout;
  }
  for (char32_t code = U'a'; code <= U'z'; ++code) {
    table[code] = CharClass::Small;
  }
  for (char32_t code = U'A'; code <= U'Z'; ++code) {
    table[code] = CharClass::Capital;
  }
  table[U'_'] = CharClass::Underline;
  for (char32_t code = U'0'; code <= U'9'; ++code) {
    table[code] = CharClass::Digit;
  }
  for (char32_t code : {U'#', U'$', U'&', U'*', U'+', U'-', U'.', U'/', U':', U'<', U'=',
                        U'>', U'?', U'@', U'^', U'~', U'\\'}) {
    table[code] = CharClass::Graphic;
  }
  for (char32_t code : {U'!', U'(', U')', U',', U';', U'[', U']', U'{', U'|', U'}', U'%'}) {
    table[code] = CharClass::Solo;
  }
  table[U'\''] = CharClass::Quote;
  table[U'"'] = CharClass::Quote;
  return table;
}

constexpr auto k_ascii = make_ascii_table();

// 区間は重ならず first 順である。終端が点以上の最初の区間だけ見れば、
// 点が入る区間か、その直後の区間かが分かる。
template <typename Range, std::size_t Count>
[[nodiscard]] std::optional<Range> find_range(const std::array<Range, Count>& ranges,
                                              char32_t code_point) {
  const auto it = std::lower_bound(ranges.begin(), ranges.end(), code_point,
                                    [](const Range& range, char32_t point) {
                                      return range.last < point;
                                    });
  if (it == ranges.end() || code_point < it->first) {
    return std::nullopt;
  }
  return *it;
}

// 非 ASCII の図形は Sm So Pd Po だけ。Ps Pe Pc は計画の図形一覧に無い。
// Nd は数字にしない。
[[nodiscard]] CharClass class_from_category(char major, char minor) {
  switch ((static_cast<unsigned char>(major) << 8) | static_cast<unsigned char>(minor)) {
    case 'L' << 8 | 'l':
    case 'L' << 8 | 'o':
    case 'L' << 8 | 'm':
    case 'N' << 8 | 'l':
      return CharClass::Small;
    case 'L' << 8 | 'u':
    case 'L' << 8 | 't':
      return CharClass::Capital;
    case 'M' << 8 | 'n':
    case 'M' << 8 | 'c':
      return CharClass::Mark;
    case 'S' << 8 | 'm':
    case 'S' << 8 | 'o':
    case 'P' << 8 | 'd':
    case 'P' << 8 | 'o':
      return CharClass::Graphic;
    case 'Z' << 8 | 's':
    case 'Z' << 8 | 'l':
    case 'Z' << 8 | 'p':
      return CharClass::Layout;
    default:
      return CharClass::Illegal;
  }
}

// U+0085 は Cc だがレイアウトに数える。U+FEFF は Cf でも不正にする。
// 全角括弧と全角下線だけ Ps Pe Pc でも図形にする。ASCII の括弧・下線と混ぜない。
// 先頭の BOM を読み飛ばすのは lexer の仕事で、分類は変えない。
[[nodiscard]] CharClass classify_above_ascii(char32_t code_point) {
  if (code_point == U'\u0085') {
    return CharClass::Layout;
  }
  if (code_point == U'\uFF08' || code_point == U'\uFF09' || code_point == U'\uFF3F') {
    return CharClass::Graphic;
  }
  if (code_point == U'\uFEFF' || code_point > U'\U0010FFFF') {
    return CharClass::Illegal;
  }
  const auto range = find_range(k_category_ranges, code_point);
  if (!range) {
    return CharClass::Illegal;
  }
  return class_from_category(range->major, range->minor);
}

}  // namespace

CharClass classify(char32_t code_point) {
  if (code_point <= 0x7F) {
    return k_ascii[static_cast<std::size_t>(code_point)];
  }
  return classify_above_ascii(code_point);
}

bool east_asian_wide(char32_t code_point) {
  if (code_point > U'\U0010FFFF') {
    return false;
  }
  return find_range(k_wide_ranges, code_point).has_value();
}

char32_t simple_uppercase(char32_t code_point) {
  if (code_point > U'\U0010FFFF') return code_point;
  const auto range = find_range(k_upper_ranges, code_point);
  if (!range) return code_point;
  const auto mapped = static_cast<std::int64_t>(code_point) + range->delta;
  if (mapped < 0 || mapped > 0x10FFFF) return code_point;
  return static_cast<char32_t>(mapped);
}

std::string_view unicode_version() { return k_unicode_version; }

}  // namespace mosa
