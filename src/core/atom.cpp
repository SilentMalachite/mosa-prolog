#include "mosa/atom.hpp"

#include <string>
#include <utility>

namespace mosa {
namespace {

void append_utf8(std::string& text, char32_t code) {
  const auto push = [&text](unsigned int byte) {
    text.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
  };
  if (code <= 0x7FU) {
    push(static_cast<unsigned int>(code));
    return;
  }
  if (code <= 0x7FFU) {
    push(0xC0U | static_cast<unsigned int>(code >> 6));
    push(0x80U | (static_cast<unsigned int>(code) & 0x3FU));
    return;
  }
  if (code <= 0xFFFFU) {
    push(0xE0U | static_cast<unsigned int>(code >> 12));
    push(0x80U | ((static_cast<unsigned int>(code) >> 6) & 0x3FU));
    push(0x80U | (static_cast<unsigned int>(code) & 0x3FU));
    return;
  }
  push(0xF0U | static_cast<unsigned int>(code >> 18));
  push(0x80U | ((static_cast<unsigned int>(code) >> 12) & 0x3FU));
  push(0x80U | ((static_cast<unsigned int>(code) >> 6) & 0x3FU));
  push(0x80U | (static_cast<unsigned int>(code) & 0x3FU));
}

bool all_ascii(std::string_view text) {
  for (const char ch : text) {
    if (static_cast<unsigned char>(ch) >= 0x80U) return false;
  }
  return true;
}

}  // namespace

AtomTable::AtomTable() {
  // 0 番は使わない。最初の intern が 1 になる。
  names_.emplace_back();
  ascii_.push_back(0);
}

std::uint32_t AtomTable::intern(std::string_view text) {
  const std::string owned{text};
  const auto found = ids_.find(owned);
  if (found != ids_.end()) return found->second;
  const auto id = static_cast<std::uint32_t>(names_.size());
  names_.push_back(owned);
  ascii_.push_back(all_ascii(owned) ? 1 : 0);
  ids_.emplace(owned, id);
  return id;
}

std::uint32_t AtomTable::intern_char(char32_t code) {
  std::string text;
  append_utf8(text, code);
  return intern(text);
}

bool AtomTable::valid(std::uint32_t id) const { return id > 0 && id < names_.size(); }

bool AtomTable::ascii(std::uint32_t id) const { return valid(id) && ascii_[id] != 0; }

std::string_view AtomTable::name(std::uint32_t id) const {
  if (!valid(id)) return {};
  return names_[id];
}

}
