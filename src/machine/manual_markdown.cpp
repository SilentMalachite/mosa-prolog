// リリース 0.1.0 Task 5 の修正 4 の追加。マニュアルの 1 行を GitHub と同じ形に読む関数（manual_markdown.hpp）。
// manual_parse.cpp が 800 行を超えるので、行の並びを読む Parser と分けた。規則は tests/manual/README.md。

#include "manual_markdown.hpp"

#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace mosa::markdown {
namespace {

// 名前は書いたままか、Prolog と同じく一重引用符の中（'' は ' 1 つ）。','/2 を Prolog の書き方で書けるように。
std::optional<std::string> indicator_name(std::string_view text) {
  if (text.size() < 2 || text.front() != '\'' || text.back() != '\'') return std::string{text};
  const std::string_view inner = text.substr(1, text.size() - 2);
  std::string name;
  for (std::size_t index = 0; index < inner.size(); ++index) {
    if (inner[index] == '\'') {
      if (index + 1 == inner.size() || inner[index + 1] != '\'') return std::nullopt;
      ++index;
    }
    name += inner[index];
  }
  return name;
}

std::size_t backtick_run(std::string_view text, std::size_t index) {
  std::size_t end = index;
  while (end < text.size() && text[end] == '`') ++end;
  return end - index;
}

// index の ` の並びから始まるコードスパンの後ろ。CommonMark と同じく、同じ長さの次の ` の並びで閉じる。閉じなければ
// その並びは字のままなので、並びの後ろを返す。
std::size_t after_code_span(std::string_view cell, std::size_t index) {
  const std::size_t open = backtick_run(cell, index);
  std::size_t next = cell.find('`', index + open);
  while (next != std::string_view::npos) {
    const std::size_t close = backtick_run(cell, next);
    if (close == open) return next + close;
    next = cell.find('`', next + close);
  }
  return index + open;
}

constexpr std::size_t kNone = std::string_view::npos;

bool ascii_letter(char letter) { return (letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z'); }

// \ と ASCII の記号の組み（CommonMark の逃がし）。逃がした字は ` の並びにも ( の数にもならない。
bool escaped(std::string_view text, std::size_t index) {
  constexpr std::string_view kPunctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
  return text[index] == '\\' && index + 1 < text.size() && kPunctuation.find(text[index + 1]) != kNone;
}

// cmark の scan_spacechars と cmark_isspace が空白とする字（行の中なので \n は来ない）。
bool link_space(char letter) {
  return letter == ' ' || letter == '\t' || letter == '\v' || letter == '\f' || letter == '\r';
}

std::size_t skip_link_spaces(std::string_view text, std::size_t index) {
  while (index < text.size() && link_space(text[index])) ++index;
  return index;
}

// インラインリンクの行き先の後ろ（cmark の manual_scan_link_url）。<…> か、括弧の釣り合った空白の無い並び。
// 行き先にならなければ kNone。
std::size_t link_url_end(std::string_view text, std::size_t index) {
  if (text[index] == '<') {
    for (++index; index < text.size(); ++index) {
      if (escaped(text, index)) ++index;
      else if (text[index] == '>') return index + 1;
      else if (text[index] == '<') return kNone;
    }
    return kNone;
  }
  std::size_t depth = 0;
  for (; index < text.size() && !link_space(text[index]); ++index) {
    if (escaped(text, index)) {
      ++index;
    } else if (text[index] == '(') {
      ++depth;
    } else if (text[index] == ')') {
      if (depth == 0) break;
      --depth;
    }
  }
  return depth == 0 ? index : kNone;
}

// 題（"…"、'…'、(…)）の後ろ。題を始める字でなければ index、この行で閉じなければ kNone。
std::size_t link_title_end(std::string_view text, std::size_t index) {
  const char open = text[index];
  if (open != '"' && open != '\'' && open != '(') return index;
  const char close = open == '(' ? ')' : open;
  for (++index; index < text.size(); ++index) {
    if (text[index] == '\\' && index + 1 < text.size()) ++index;
    else if (text[index] == close) return index + 1;
  }
  return kNone;
}

// ]( の後ろのインラインリンクが ` を飲みうるか。cmark の handle_close_bracket と同じ順に、空白、行き先、空白、題、
// 空白、) と読む。リンクの形にならなければ飲まない。行き先や題が次の行に続きうるときは飲みうるとする。
bool link_takes_backtick(std::string_view text, std::size_t paren) {
  std::size_t index = skip_link_spaces(text, paren + 1);
  if (index == text.size()) return true;
  const std::size_t url = link_url_end(text, index);
  if (url == kNone) return false;
  index = skip_link_spaces(text, url);
  if (index == text.size()) return true;
  if (index > url) {
    index = link_title_end(text, index);
    if (index == kNone) return true;
    index = skip_link_spaces(text, index);
  }
  if (index < text.size() && text[index] != ')') return false;
  return text.substr(paren, index - paren).find('`') != kNone;
}

// ][ の後ろのラベル（[…]）が ` を飲みうるか。ラベルが次の行に続きうるときも飲みうるとする。
bool label_takes_backtick(std::string_view text, std::size_t bracket) {
  for (std::size_t index = bracket + 1; index < text.size(); ++index) {
    if (text[index] == '\\' && index + 1 < text.size()) ++index;
    else if (text[index] == '[') return false;
    else if (text[index] == ']') return text.substr(bracket, index - bracket).find('`') != kNone;
  }
  return true;
}

// index の ] の後ろのリンクの行き先、題、ラベルが ` を飲みうる。飲まれた ` はコードスパンを開かないので、その後ろの
// ` の組みは行の中だけでは決まらない（修正 4 の追加。GitHub で [a](`) <details>` の <details> が映る）。
bool link_at(std::string_view text, std::size_t index) {
  if (text[index] != ']' || index + 1 >= text.size()) return false;
  if (text[index + 1] == '(') return link_takes_backtick(text, index + 1);
  return text[index + 1] == '[' && label_takes_backtick(text, index + 1);
}

// index の < がタグを始める（後ろが英字、/、?、!）。<!-- は注釈なので除く。
bool tag_at(std::string_view text, std::size_t index) {
  if (text[index] != '<' || index + 1 >= text.size() || text.substr(index).starts_with("<!--")) return false;
  const char next = text[index + 1];
  return ascii_letter(next) || next == '/' || next == '?' || next == '!';
}

// HTML の注釈の終わり（--> か --!>）の後ろ。無ければ kNone。GitHub の HTML の読み方では --!> でも閉じる。
std::size_t comment_end(std::string_view text, std::size_t from) {
  const std::size_t dash = text.find("-->", from);
  const std::size_t bang = text.find("--!>", from);
  if (dash == kNone && bang == kNone) return kNone;
  return dash < bang ? dash + 3 : bang + 4;
}

// 生の HTML として読み、注釈の外にタグがあれば true。コードスパンも \ の逃がしも無い。注釈は --> か --!> で閉じ、
// <!--> と <!---> は空の注釈。open は閉じていない注釈の中かどうかで、行をまたいで持ち越す。
bool raw_tags(std::string_view text, bool& open) {
  bool tag = false;
  std::size_t index = 0;
  while (index < text.size()) {
    if (open) {
      index = comment_end(text, index);
      if (index == kNone) return tag;
      open = false;
    } else if (text.substr(index).starts_with("<!--")) {
      const std::string_view rest = text.substr(index + 4);
      open = !rest.starts_with('>') && !rest.starts_with("->");
      index += open ? 4 : rest.starts_with('>') ? 5 : 6;
    } else {
      tag = tag_at(text, index) || tag;
      ++index;
    }
  }
  return tag;
}

// Markdown の文として読み、コードスパンとその行で閉じる <!-- … --> の外にタグがあれば true（注釈の中は raw_tags で
// 読む）。\ で逃がした < はタグにしない。loose は ` の組みが行の中だけでは決まらなくなったかどうかで、段落の次の行へ
// 持ち越す。閉じない ` の並び、その行で閉じない <!--、` を飲みうるリンクの後ろで loose にし、そこからはコードスパンと
// 注釈の中のタグも誤りにする。前の行の組みは後ろの行で変わらないので、前の行は読み直さない。
bool markdown_tags(std::string_view text, bool& loose) {
  bool tag = false;
  std::size_t index = 0;
  while (index < text.size()) {
    if (escaped(text, index)) {
      index += 2;
    } else if (!loose && text[index] == '`') {
      const std::size_t end = after_code_span(text, index);
      loose = end == index + backtick_run(text, index);
      index = end;
    } else if (!loose && text.substr(index).starts_with("<!--")) {
      const std::size_t close = text.find("-->", index + 2);
      loose = close == kNone;
      bool open = false;
      if (!loose) tag = raw_tags(text.substr(index, close + 3 - index), open) || tag;
      index = loose ? index + 4 : close + 3;
    } else {
      loose = loose || link_at(text, index);
      tag = tag_at(text, index) || tag;
      ++index;
    }
  }
  return tag;
}

// 行を段落の文として読み、| があれば欄ごとにも読む。GFM の表は欄を分けてからコードスパンを読み、欄は 1 行で閉じる。
bool markdown_line(std::string_view text, bool& loose) {
  bool tag = markdown_tags(text, loose);
  if (text.find('|') == kNone) return tag;
  for (const std::string_view cell : table_cells(text)) {
    bool cell_loose = false;
    tag = markdown_tags(cell, cell_loose) || tag;
  }
  return tag;
}

// 行頭の空白、タブ、> の引用、箇条書きの印を除いた残り。ここが <!-- で始まる行は、入れ物の中の注釈のブロックかも
// しれない。
std::string_view container_content(std::string_view line) {
  std::size_t index = 0;
  while (index < line.size()) {
    const std::size_t step = blank(line[index]) || line[index] == '>' ? 1 : list_marker(line.substr(index));
    if (step == 0) break;
    index += step;
  }
  return line.substr(index);
}

}  // namespace

// 行の終わりは \n、\r\n、\n の付かない \r（CommonMark と同じ）。\r を行の途中の字として残すと、GitHub が 2 行に
// 見せるものを 1 行として読み、後ろの行の印や表の行を見落とす（Task 5 の修正 4）。
Lines split_marked_lines(std::string_view text) {
  Lines lines;
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t end = text.find_first_of("\r\n", start);
    if (end == std::string_view::npos) end = text.size();
    const bool cr = end < text.size() && text[end] == '\r';
    const bool crlf = cr && end + 1 < text.size() && text[end + 1] == '\n';
    lines.text.push_back(text.substr(start, end - start));
    lines.lone_cr.push_back(cr && !crlf);
    start = end + (crlf ? 2 : 1);
  }
  return lines;
}

std::vector<std::string_view> split_lines(std::string_view text) { return split_marked_lines(text).text; }

bool blank(char letter) { return letter == ' ' || letter == '\t'; }

std::string_view trim(std::string_view text) {
  while (!text.empty() && blank(text.front())) text.remove_prefix(1);
  while (!text.empty() && blank(text.back())) text.remove_suffix(1);
  return text;
}

// 箇条書きの印（- * + か 1. 1)）とその後の空白 1 つの長さ。印でなければ 0。
std::size_t list_marker(std::string_view text) {
  std::size_t index = 0;
  if (!text.empty() && (text.front() == '-' || text.front() == '*' || text.front() == '+')) {
    index = 1;
  } else {
    while (index < text.size() && text[index] >= '0' && text[index] <= '9') ++index;
    if (index == 0 || index == text.size() || (text[index] != '.' && text[index] != ')')) return 0;
    ++index;
  }
  return index < text.size() && blank(text[index]) ? index + 1 : 0;
}

// 0 桁目の ATX の見出し（# が 1〜6 個、その後ろは行末か空白）。表の区切りはここで終わる。区切りの中で字下げした
// 行はどれも誤りなので、字下げした見出しを見分ける要は無い。
bool heading(std::string_view line) {
  std::size_t marks = 0;
  while (marks < line.size() && line[marks] == '#') ++marks;
  if (marks == 0 || marks > 6) return false;
  return marks == line.size() || blank(line[marks]);
}

// | で始まる表の行の最初の欄が、ちょうど 1 つのコードスパン `name/arity` ならその述語。ほかは nullopt。
// 引数の数は 10 進の数字で、0 でない頭の 0（between/03）は書き違いとして受けない。
std::optional<PredicateIndicator> table_entry(std::string_view line) {
  std::string_view text = trim(line);
  if (!text.starts_with('|')) return std::nullopt;
  text.remove_prefix(1);
  const std::string_view cell = trim(text.substr(0, text.find('|')));
  if (cell.size() < 3 || cell.front() != '`' || cell.back() != '`') return std::nullopt;
  const std::string_view code = trim(cell.substr(1, cell.size() - 2));
  const std::size_t slash = code.rfind('/');
  if (code.find('`') != std::string_view::npos || slash == std::string_view::npos || slash == 0) return std::nullopt;
  const std::string_view digits = code.substr(slash + 1);
  if (digits.empty() || digits.front() < '0' || digits.front() > '9') return std::nullopt;
  if (digits.size() > 1 && digits.front() == '0') return std::nullopt;
  int arity = 0;
  const auto [last, problem] = std::from_chars(digits.data(), digits.data() + digits.size(), arity);
  if (problem != std::errc{} || last != digits.data() + digits.size()) return std::nullopt;
  std::optional<std::string> name = indicator_name(code.substr(0, slash));
  if (!name) return std::nullopt;
  return PredicateIndicator{std::move(*name), arity};
}

// GFM の表の行の欄（cmark-gfm の row_from_string と同じ分け方）。頭の | の後ろを、直前に \ の無い | で分ける
// （\\| の | も分けない）。最後の | の後ろが空白だけなら欄にしない。見出しの行と | --- | の欄の数が違うと GFM は表に
// しないので、数は GitHub と同じでなければならない。
std::vector<std::string_view> table_cells(std::string_view row) {
  std::vector<std::string_view> cells;
  std::size_t index = row.starts_with('|') ? 1 : 0;
  while (index < row.size()) {
    const std::size_t start = index;
    while (index < row.size() && row[index] != '|') {
      index += row[index] == '\\' && index + 1 < row.size() && row[index + 1] == '|' ? 2 : 1;
    }
    const std::string_view cell = row.substr(start, index - start);
    if (index < row.size() || !trim(cell).empty()) cells.push_back(cell);
    ++index;
  }
  return cells;
}

// GFM の表の区切りの行（| --- | :--: |）。0 桁目の | で始まり、どの欄も - の並び（両端に : を書いてよい）。
bool delimiter_row(std::string_view row) {
  if (!row.starts_with('|')) return false;
  const std::vector<std::string_view> cells = table_cells(row);
  for (std::string_view cell : cells) {
    cell = trim(cell);
    if (cell.starts_with(':')) cell.remove_prefix(1);
    if (cell.ends_with(':')) cell.remove_suffix(1);
    if (cell.empty() || cell.find_first_not_of('-') != std::string_view::npos) return false;
  }
  return !cells.empty();
}

// 本体の行のコードスパンの外の <。表の中の HTML は後ろの行まで壊しうるので誤りにする（Task 5 の修正 4）。GFM は欄を
// 分けてから欄ごとにコードスパンを読むので、欄ごとに左から読む。\` と \\ は逃がした字で、` の並びにしない。\< は字の
// < として見せるが、決めた規則のとおり誤りにする。` を飲みうるリンクの後ろ（修正 4 の追加）は ` の組みが決まらない
// ので、その欄の残りの < はコードスパンの中に見えても誤り。
bool angle_outside_code(std::string_view row) {
  for (const std::string_view cell : table_cells(row)) {
    bool loose = false;
    std::size_t index = 0;
    while (index < cell.size()) {
      const char letter = cell[index];
      if (letter == '<') return true;
      const char after = index + 1 < cell.size() ? cell[index + 1] : '\0';
      loose = loose || link_at(cell, index);
      if (letter == '`' && !loose) index = after_code_span(cell, index);
      else index += letter == '\\' && (after == '`' || after == '\\') ? 2 : 1;
    }
  }
  return false;
}

// 区切りの行に残った 0x20 未満の字（タブ、\f、\v、NUL など）。\n の付かない \r で終わった行は \r を持つものと見る。
// GitHub はこれらを空白や行の終わりや別の字として読み、検査器と見え方が分かれる（修正 4）。
std::optional<unsigned char> control_byte(std::string_view line, bool lone_cr) {
  for (const char letter : line) {
    const auto byte = static_cast<unsigned char>(letter);
    if (byte < 0x20) return byte;
  }
  if (lone_cr) return static_cast<unsigned char>('\r');
  return std::nullopt;
}

std::string code_point(unsigned char byte) {
  constexpr std::string_view kHex = "0123456789ABCDEF";
  return std::string{"U+00"} + kHex[byte >> 4] + kHex[byte & 0x0F];
}

// 誤りの文面に出す行。制御文字は ? にし、NUL や ESC の並びを端末に流さない（字の名は manual_section_control が出す）。
std::string shown(std::string_view line) {
  std::string out{line};
  for (char& letter : out) {
    if (static_cast<unsigned char>(letter) < 0x20) letter = '?';
  }
  return out;
}

// 大文字と小文字を問わない ASCII の頭の一致。
bool starts_with_folded(std::string_view text, std::string_view prefix) {
  if (text.size() < prefix.size()) return false;
  for (std::size_t index = 0; index < prefix.size(); ++index) {
    const char letter = text[index] >= 'A' && text[index] <= 'Z' ? static_cast<char>(text[index] - 'A' + 'a')
                                                                   : text[index];
    if (letter != prefix[index]) return false;
  }
  return true;
}

// GFM の HTML ブロック <!-- … --> は --> を含む行で終わる。<!--> と <!---> もその行で閉じるので、--> は <!-- の
// 位置から探す。--> の後ろに字があっても同じ。
bool comment_ends_on(std::string_view line) {
  const std::size_t open = line.find("<!--");
  return line.find("-->", open == std::string_view::npos ? 0 : open) != std::string_view::npos;
}

// Sure は 0 桁目の <!-- で始まったブロックで、GitHub は必ず生の HTML として読む。Maybe は字下げ、引用、箇条書きの中の
// <!-- で始まったもので、入れ物と字下げしだいでコードや文にもなるので、両方の読み方で探す。ブロックは段落を切る。
void TagScan::line(std::size_t index, std::string_view text) {
  if (index != next_) reset();
  next_ = index + 1;
  const bool start = block_ == Block::None && container_content(text).starts_with("<!--");
  if (start) block_ = text.starts_with("<!--") ? Block::Sure : Block::Maybe;
  bool tag = block_ != Block::None && raw_tags(text, open_);
  if (block_ == Block::Sure || trim(text).empty()) loose_ = false;
  else tag = markdown_line(text, loose_) || tag;
  const bool ends = start ? comment_ends_on(text) : text.find("-->") != kNone;
  if (block_ != Block::None && ends) {
    block_ = Block::None;
    open_ = false;
  }
  if (tag) found_.push_back(index);
}

std::vector<std::size_t> TagScan::finish() { return std::move(found_); }

// 渡されなかった行（囲いの中、区切りの中）の前後は続きとして読まない。
void TagScan::reset() {
  block_ = Block::None;
  open_ = false;
  loose_ = false;
}

}  // namespace mosa::markdown
