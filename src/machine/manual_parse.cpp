// リリース 0.1.0 Task 4。マニュアルの 1 ファイルから例を読む（mosatest --manual）。規則は tests/manual/README.md。
// 囲いは必ず直前の行に印を持つ。印の無い囲いを誤りにするのは、検査から黙って漏れる例を作らないため。
// 行は 1 行ずつ前から読む。印は次の行が開きの囲いのときだけ効き、間に何かあればその印を誤りにする。
// \r は行末で落とす。Windows で checkout したファイル（CRLF）でも同じ例と同じ入力になる。\n の付かない \r も行末。
// 囲いの外では述語の表も読む（Task 5）。<!-- predicates: LIB --> の行から次の 0 桁目の ATX の見出しまでが区切りで、
// その中に書けるのは表だけ、ほかの行はすべて誤りにする（Task 5 の修正 3 と 4。Parser::section_line）。
// 1 行の形（表の行、欄、コードスパン、生の HTML）は manual_markdown.cpp が読む（修正 4 の追加で分けた）。

#include "mosa/manual.hpp"

#include "manual_markdown.hpp"
#include "mosa/messages.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mosa {
namespace {

using markdown::angle_outside_code;
using markdown::blank;
using markdown::code_point;
using markdown::comment_ends_on;
using markdown::control_byte;
using markdown::delimiter_row;
using markdown::heading;
using markdown::Lines;
using markdown::list_marker;
using markdown::shown;
using markdown::split_lines;
using markdown::split_marked_lines;
using markdown::starts_with_folded;
using markdown::table_cells;
using markdown::table_entry;
using markdown::TagScan;
using markdown::trim;

enum class MarkerKind { Check, Skip, File };

struct Marker {
  MarkerKind kind = MarkerKind::Skip;
  std::size_t line = 0;  // 印の最初の行（1 から）
  std::string id;
  std::string name;
  std::optional<std::string> input;
};

struct MarkerHead {
  MarkerKind kind = MarkerKind::Skip;
  std::string_view rest;
  bool closed = false;  // 同じ行に --> がある
};

struct Fence {
  char mark = '`';
  std::size_t length = 0;
  std::size_t indent = 0;
  std::string_view language;
};

constexpr std::string_view kCommandPrefix = "$ ";
constexpr std::string_view kQueryPrefix = "?- ";

std::vector<std::string_view> split_words(std::string_view text) {
  std::vector<std::string_view> words;
  std::size_t index = 0;
  while (index < text.size()) {
    while (index < text.size() && blank(text[index])) ++index;
    const std::size_t start = index;
    while (index < text.size() && !blank(text[index])) ++index;
    if (index > start) words.push_back(text.substr(start, index - start));
  }
  return words;
}

// CommonMark の囲い（``` か ~~~ を 3 つ以上）。字下げした囲いも見つけ、Parser::run が誤りにする（Task 5 の修正 5）。
std::optional<Fence> opening_fence(std::string_view line) {
  Fence fence;
  while (fence.indent < line.size() && line[fence.indent] == ' ') ++fence.indent;
  if (fence.indent == line.size()) return std::nullopt;
  fence.mark = line[fence.indent];
  if (fence.mark != '`' && fence.mark != '~') return std::nullopt;
  while (fence.indent + fence.length < line.size() && line[fence.indent + fence.length] == fence.mark) ++fence.length;
  if (fence.length < 3) return std::nullopt;
  const std::string_view info = trim(line.substr(fence.indent + fence.length));
  if (fence.mark == '`' && info.find('`') != std::string_view::npos) return std::nullopt;
  fence.language = info.substr(0, info.find_first_of(" \t"));
  return fence;
}

bool closes(const Fence& fence, std::string_view line) {
  const std::string_view text = trim(line);
  std::size_t length = 0;
  while (length < text.size() && text[length] == fence.mark) ++length;
  return length >= fence.length && length == text.size();
}

// 行頭のうち、検査器が囲いとして読まないもの（> の引用、タブ、同じ行の箇条書きの印、その間の空白）の長さ。
// 空白だけなら 0（opening_fence が読み、字下げした囲いとして誤りにする）。この後ろの囲いを黙って飛ばすと検査から
// 漏れるので、誤りにする。
std::size_t unreadable_prefix(std::string_view line) {
  std::size_t index = 0;
  bool unreadable = false;
  while (index < line.size()) {
    if (line[index] == ' ') {
      ++index;
      continue;
    }
    const std::size_t step = line[index] == '\t' || line[index] == '>' ? 1 : list_marker(line.substr(index));
    if (step == 0) break;
    unreadable = true;
    index += step;
  }
  return unreadable ? index : 0;
}

struct Comment {
  std::string_view text;  // <!-- の後ろ（同じ行の --> の前まで）。前後の空白は除いてある
  bool closed = false;    // 同じ行に --> がある
};

std::optional<Comment> comment(std::string_view line) {
  std::string_view text = trim(line);
  if (!text.starts_with("<!--")) return std::nullopt;
  text = trim(text.substr(4));
  Comment note;
  note.closed = text.ends_with("-->");
  if (note.closed) text = trim(text.substr(0, text.size() - 3));
  note.text = text;
  return note;
}

// 印の行なら種類と残りを返す。印でない注釈は nullopt（<!-- predicates: … --> は Parser::table_line が読む）。
std::optional<MarkerHead> marker_head(std::string_view line) {
  const std::optional<Comment> note = comment(line);
  if (!note) return std::nullopt;
  constexpr std::array<std::pair<std::string_view, MarkerKind>, 3> kinds{{
      {"check:", MarkerKind::Check}, {"skip:", MarkerKind::Skip}, {"file:", MarkerKind::File}}};
  for (const auto& [prefix, kind] : kinds) {
    if (!note->text.starts_with(prefix)) continue;
    return MarkerHead{kind, trim(note->text.substr(prefix.size())), note->closed};
  }
  return std::nullopt;
}

constexpr std::string_view kTablePrefix = "predicates:";

// 行のどこかに印の注釈（<!-- check:、skip:、file:、predicate で始まるもの）がある。複数行の注釈を閉じる行の印を
// 見つけるため（修正 5）。--> の後ろの印も、GitHub ではその行ごと HTML のブロックの中で、印として読めない。
bool holds_marker(std::string_view line) {
  for (std::size_t open = line.find("<!--"); open != std::string_view::npos; open = line.find("<!--", open + 4)) {
    const std::string_view rest = line.substr(open);
    const std::optional<Comment> note = comment(rest);
    if (marker_head(rest) || (note && starts_with_folded(note->text, "predicate"))) return true;
  }
  return false;
}

// 区切りの中の表の位置。Header は見出しの行の後ろ、Rejected は誤りにした行の後ろ（すぐ下の | --- | で本体に入り、
// 誤りを重ねない）、Body は | --- | の後ろ。
enum class Rows { None, Header, Rejected, Body };

// file: の NAME と ./NAME は章のディレクトリの中の名前だけにする。作業場所の外へ書かせない。
bool plain_file_name(std::string_view name) {
  return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\:") == std::string_view::npos;
}

bool allowed_program(std::string_view word) {
  if (word == "mosa" || word == "mosac" || word == "mosatest") return true;
  return word.starts_with("./") && plain_file_name(word.substr(2));
}

struct Words {
  std::vector<std::string> words;
  std::string_view problem;  // 空でなければ誤りの鍵（lib/messages_*.pl）
  char culprit = '\0';       // manual_console_special のときの字
};

// POSIX のシェルが特別に扱う字。引用の外ではこれ、"…" の中では \ $ ` を誤りにして、シェルが動かすのと同じ argv
// だけを受ける（読者は同じ行をシェルに打つ）。'…' の中はシェルでもすべて字のまま。
constexpr std::string_view kShellSpecial = "\\$`|&;<>()*?[#~";
constexpr std::string_view kDoubleQuoteSpecial = "\\$`";

// シェルの語の分け方のうち、空白と '…' "…" だけ。引用は隣の字とつながる（-g"x" は -gx）。
Words split_command(std::string_view text) {
  Words result;
  std::string word;
  bool in_word = false;
  char quote = '\0';
  for (const char letter : text) {
    std::string_view special = kShellSpecial;
    if (quote != '\0') special = quote == '"' ? kDoubleQuoteSpecial : std::string_view{};
    if (special.find(letter) != std::string_view::npos) {
      result.problem = "manual_console_special";
      result.culprit = letter;
      return result;
    }
    if (quote != '\0') {
      if (letter == quote) quote = '\0';
      else word += letter;
    } else if (letter == '\'' || letter == '"') {
      quote = letter;
      in_word = true;
    } else if (blank(letter)) {
      if (in_word) result.words.push_back(std::move(word));
      word.clear();
      in_word = false;
    } else {
      word += letter;
      in_word = true;
    }
  }
  if (quote != '\0') result.problem = "manual_console_quote";
  else if (in_word) result.words.push_back(std::move(word));
  return result;
}

class Parser {
 public:
  Parser(std::string_view text, std::string_view file) : file_(file) {
    Lines lines = split_marked_lines(text);
    lines_ = std::move(lines.text);
    lone_cr_ = std::move(lines.lone_cr);
  }
  ParsedManual run();

 private:
  void finish();
  void error(std::size_t line, std::string_view key, std::vector<std::string> args = {});
  std::size_t read_marker(std::size_t index, const MarkerHead& head);
  std::size_t read_override(std::size_t index, Marker& marker);
  std::size_t read_block(std::size_t index, const Fence& fence);
  std::size_t skip_unreadable(std::size_t index, const Fence& fence);
  void add_example(const Marker& marker, const Fence& fence, std::size_t line, std::string body);
  std::optional<ExampleKind> check_kind(const Fence& fence, std::size_t line, std::string_view body);
  void check_console(std::size_t line, std::string_view body);
  bool comment_line(std::size_t index);
  void table_line(std::size_t index);
  void table_comment(std::size_t index, const Comment& note);
  std::size_t section_line(std::size_t index);
  void table_row(std::size_t index, std::string_view line);
  void end_rows();
  void add_entry(std::size_t index, PredicateIndicator entry);

  std::vector<std::string_view> lines_;
  std::vector<bool> lone_cr_;  // lines_ と同じ位置。その行は \n の付かない \r で終わった
  std::string file_;
  ParsedManual result_;
  std::vector<std::pair<std::size_t, std::string>> errors_;  // 行の番号と文面。finish で行の順に並べる
  TagScan tags_;  // 囲いと区切りの外の生の HTML のタグ。印のある章でだけ誤り
  std::optional<Marker> pending_;
  std::optional<std::size_t> table_;  // 開いている表の区切り（result_.tables の位置）
  Rows rows_ = Rows::None;
  std::size_t header_ = 0;   // Rows::Header の行の位置
  bool in_comment_ = false;  // 区切りの外の複数行の <!-- … --> の中。中の predicates: の印は数えない
};

ParsedManual Parser::run() {
  std::size_t index = 0;
  while (index < lines_.size()) {
    if (table_) {
      index = section_line(index);
      continue;
    }
    const std::string_view line = lines_[index];
    if (const auto fence = opening_fence(line)) {
      index = fence->indent == 0 ? read_block(index, *fence) : skip_unreadable(index, *fence);
      continue;
    }
    if (const std::size_t prefix = unreadable_prefix(line); prefix > 0) {
      if (const auto fence = opening_fence(line.substr(prefix))) {
        index = skip_unreadable(index, *fence);
        continue;
      }
    }
    if (pending_) {
      error(pending_->line, "manual_marker_alone");
      pending_.reset();
    }
    if (in_comment_ && !comment_line(index)) {
      tags_.line(index, line);
      ++index;
      continue;
    }
    if (const auto head = marker_head(line)) {
      const std::size_t next = read_marker(index, *head);
      for (; index < next; ++index) tags_.line(index, lines_[index]);
      continue;
    }
    table_line(index);
    ++index;
  }
  finish();
  return std::move(result_);
}

// 生の HTML のタグは、ファイルのどこかに区切りがあるときだけ誤り（区切りより前の行もそう）。印の無い章の HTML は
// 表を隠さない。後から足す誤りもあるので、誤りは最後に行の順に並べる。
void Parser::finish() {
  end_rows();
  if (pending_) error(pending_->line, "manual_marker_alone");
  const std::vector<std::size_t> tags = tags_.finish();
  if (!result_.tables.empty()) {
    for (const std::size_t index : tags) error(index + 1, "manual_html_tag", {shown(lines_[index])});
  }
  std::ranges::stable_sort(errors_, {}, &std::pair<std::size_t, std::string>::first);
  for (auto& [line, text] : errors_) result_.errors.push_back(std::move(text));
}

// 複数行の注釈の中の行。--> を含む行で注釈は閉じる（GFM の HTML のブロックと同じ）。その行の印は GitHub では注釈の
// 中に隠れるので誤りにし（Task 5 の修正 5、rr4 の opencomment と markerclose）、その後は印として読んで、印の無い
// 囲いの誤りを重ねない。返すのは、この行を印としてさらに読むかどうか。
bool Parser::comment_line(std::size_t index) {
  const std::string_view line = lines_[index];
  if (line.find("-->") == std::string_view::npos) return false;
  in_comment_ = false;
  if (!holds_marker(line)) return false;
  error(index + 1, "manual_marker_in_comment", {shown(trim(line))});
  return true;
}

// 区切りの外で、囲いの外と複数行の注釈の外の行だけがここに来る。見るのは印の注釈と生の HTML のタグだけ。タグは
// TagScan が GitHub の読み方で探す。
void Parser::table_line(std::size_t index) {
  const std::string_view line = lines_[index];
  tags_.line(index, line);
  const std::optional<Comment> note = comment(line);
  if (!note) return;
  in_comment_ = !comment_ends_on(line);
  table_comment(index, *note);
}

// predicate で始まる注釈は、大文字と小文字を問わず述語の表の印と見る。predicates: NAME の形でなければ誤りにし、
// 印の書き違いで表が比べられずに通ることを防ぐ。前の区切りも閉じ、誤った印の後ろの行をそこに数えない。
void Parser::table_comment(std::size_t index, const Comment& note) {
  if (!starts_with_folded(note.text, "predicate")) return;
  table_.reset();
  const bool spelled = note.text.starts_with(kTablePrefix);
  const std::vector<std::string_view> words =
      spelled ? split_words(note.text.substr(kTablePrefix.size())) : std::vector<std::string_view>{};
  if (!note.closed || words.size() != 1) {
    error(index + 1, "manual_marker_bad", {shown(trim(lines_[index]))});
    return;
  }
  result_.tables.push_back(ManualTable{std::string{words[0]}, index + 1, {}});
  table_ = result_.tables.size() - 1;
}

// 区切りの中の 1 行（Task 5 の修正 3 と 4 の決定）。決まり: 区切りに置けるのは表だけで、どの行も空の行、見出しの
// 行、そのすぐ下の | --- |、本体の行のどれか（table_row）。ほかの行も、制御文字を持つ行もすべて誤り。わけ: GFM の
// 形をまねて漏れを塞ぐやり方は 3 度続けて別の形を見落としたので、GitHub での見え方が 1 通りに決まる形だけを通す。
// 0 桁目の ATX の見出しで区切りを閉じ、predicate で始まる注釈は次の区切りの印として読む。囲いは中の行を読まずに
// 飛ばし、1 つの誤りにする。返すのは次に読む行。
std::size_t Parser::section_line(std::size_t index) {
  const std::string_view line = lines_[index];
  const std::optional<Comment> note = line.starts_with("<!--") ? comment(line) : std::nullopt;
  const bool marker = note && starts_with_folded(note->text, "predicate");
  if (heading(line) || marker) {
    tags_.line(index, line);
    end_rows();
    table_.reset();
    if (marker) {
      in_comment_ = !comment_ends_on(line);
      table_comment(index, *note);
    }
    return index + 1;
  }
  // 制御文字の行も形は読む。| --- | の後ろのタブで見出しの誤りまで重ねないため。
  if (const std::optional<unsigned char> byte = control_byte(line, lone_cr_[index])) {
    error(index + 1, "manual_section_control", {code_point(*byte)});
  }
  if (line.empty()) {
    end_rows();
    return index + 1;
  }
  if (const auto fence = opening_fence(line)) {
    end_rows();
    error(index + 1, "manual_section_line", {shown(line)});
    rows_ = Rows::Rejected;
    std::size_t next = index + 1;
    while (next < lines_.size() && !closes(*fence, lines_[next])) ++next;
    return std::min(next + 1, lines_.size());
  }
  table_row(index, line);
  return index + 1;
}

// 区切りの中の表の行。どれも 0 桁目の | で始まる。見出しの行はバッククォートも < も持たない。そのすぐ下の | --- |
// は見出しの行と欄の数が同じ。その下の本体の行は、最初の欄がちょうど 1 つのコードスパン `name/arity` で、コード
// スパンの外に < を持たない。欄の数の違う見出しは誤りにするが、下の行は本体として読み、誤りを重ねない。
void Parser::table_row(std::size_t index, std::string_view line) {
  const bool bar = line.starts_with('|');
  if (rows_ == Rows::Body) {
    std::optional<PredicateIndicator> entry = bar && !angle_outside_code(line) ? table_entry(line) : std::nullopt;
    if (entry) add_entry(index, std::move(*entry));
    else error(index + 1, "manual_table_row", {shown(line)});
    return;
  }
  if (delimiter_row(line)) {
    if (rows_ == Rows::None) error(index + 1, "manual_section_line", {shown(line)});
    if (rows_ == Rows::Header && table_cells(lines_[header_]).size() != table_cells(line).size()) {
      error(header_ + 1, "manual_table_header", {shown(lines_[header_])});
    }
    rows_ = Rows::Body;
    return;
  }
  end_rows();
  if (bar && line.find_first_of("`<") == std::string_view::npos) {
    rows_ = Rows::Header;
    header_ = index;
  } else {
    error(index + 1, "manual_section_line", {shown(line)});
    rows_ = Rows::Rejected;
  }
}

// 表が切れた。見出しの行のすぐ下に | --- | が来なかったなら、表のつもりで書いた行が表にならなかった。
void Parser::end_rows() {
  if (rows_ == Rows::Header) error(header_ + 1, "manual_table_header", {shown(lines_[header_])});
  rows_ = Rows::None;
}

// 同じファイルの同じ名前の表に 2 度書いた述語は誤り。比べるのは集合なので、黙って重ねると片方の行は何も確かめない。
void Parser::add_entry(std::size_t index, PredicateIndicator entry) {
  ManualTable& table = result_.tables[*table_];
  for (const ManualTable& other : result_.tables) {
    if (other.library != table.library || std::ranges::find(other.entries, entry) == other.entries.end()) continue;
    error(index + 1, "manual_predicates_twice", {indicator_text(entry), table.library});
    return;
  }
  table.entries.push_back(std::move(entry));
}

void Parser::error(std::size_t line, std::string_view key, std::vector<std::string> args) {
  errors_.emplace_back(line, file_ + ":" + std::to_string(line) + ": " +
                                 render_message_fresh(default_message_language(), key, args));
}

// 印を読み、次の行の囲いのために取っておく。返すのは印の次の行の位置。
std::size_t Parser::read_marker(std::size_t index, const MarkerHead& head) {
  Marker marker;
  marker.kind = head.kind;
  marker.line = index + 1;
  const std::vector<std::string_view> words = split_words(head.rest);
  const std::size_t wanted = head.kind == MarkerKind::File ? 2 : 1;
  const bool multi_line = !head.closed && head.kind == MarkerKind::Check;
  if (words.size() != wanted || (!head.closed && !multi_line)) {
    error(marker.line, "manual_marker_bad", {std::string{trim(lines_[index])}});
    in_comment_ = !comment_ends_on(lines_[index]);  // 閉じない注釈は GitHub では後ろの行を隠す（修正 5）
    return index + 1;
  }
  marker.id = std::string{words[0]};
  if (head.kind == MarkerKind::File) {
    if (!plain_file_name(words[1])) {
      error(marker.line, "manual_file_name", {std::string{words[1]}});
      return index + 1;
    }
    marker.name = std::string{words[1]};
  }
  const std::size_t next = multi_line ? read_override(index + 1, marker) : index + 1;
  if (multi_line && next > lines_.size()) return lines_.size();
  pending_ = std::move(marker);
  return next;
}

// 複数行の check: の入力を --> の行まで読む。行はそのまま（空の行も）。閉じなければ誤りにし、size() + 1 を返す。
// 途中の行の --> も誤り。GitHub はそこで印の注釈を閉じ、後ろの入力を文として映す（修正 5。DCG の規則など）。
std::size_t Parser::read_override(std::size_t index, Marker& marker) {
  std::string input;
  while (index < lines_.size() && trim(lines_[index]) != "-->") {
    if (lines_[index].find("-->") != std::string_view::npos) {
      error(index + 1, "manual_override_arrow", {shown(lines_[index])});
    }
    input += lines_[index];
    input += '\n';
    ++index;
  }
  if (index == lines_.size()) {
    error(marker.line, "manual_marker_unclosed");
    return lines_.size() + 1;
  }
  marker.input = std::move(input);
  return index + 1;
}

// 0 桁目の囲いの中を閉じの囲いまで読み、取っておいた印と合わせて例にする。返すのは閉じの次の行の位置。閉じの行も
// 0 桁目だけ（修正 5）。字下げした閉じの行は、GitHub が閉じない（空白 4 つ）こともあるので誤りにし、そこで閉じる。
std::size_t Parser::read_block(std::size_t index, const Fence& fence) {
  const std::size_t line = index + 1;
  std::optional<Marker> marker = std::move(pending_);
  pending_.reset();
  std::string body;
  std::size_t next = index + 1;
  while (next < lines_.size() && !closes(fence, lines_[next])) {
    body += lines_[next];
    body += '\n';
    ++next;
  }
  if (next == lines_.size()) {
    error(line, "manual_fence_unclosed");
    return next;
  }
  if (lines_[next].front() != fence.mark) error(next + 1, "manual_fence_unsupported");
  if (!marker) {
    error(line, "manual_unmarked");
    return next + 1;
  }
  add_example(*marker, fence, line, std::move(body));
  return next + 1;
}

// 読めない囲い（字下げ、引用、箇条書きの印、タブの後ろ）は誤りにし、閉じの囲いまで飛ばす。中の行を印や囲いとして
// 読み違えないため。直前の印も捨てる。
std::size_t Parser::skip_unreadable(std::size_t index, const Fence& fence) {
  error(index + 1, "manual_fence_unsupported");
  pending_.reset();
  std::size_t next = index + 1;
  while (next < lines_.size()) {
    const std::string_view line = lines_[next++];
    if (closes(fence, line.substr(unreadable_prefix(line)))) break;
  }
  return next;
}

void Parser::add_example(const Marker& marker, const Fence& fence, std::size_t line, std::string body) {
  std::optional<ExampleKind> kind = ExampleKind::Skip;
  if (marker.kind == MarkerKind::Check) kind = check_kind(fence, line, body);
  if (marker.kind == MarkerKind::File) {
    kind = ExampleKind::File;
    if (fence.language != "prolog") {
      error(line, "manual_file_kind");
      kind.reset();
    }
  }
  for (const ManualExample& earlier : result_.examples) {
    if (earlier.id != marker.id) continue;
    error(line, "manual_duplicate_id", {marker.id});
    kind.reset();
    break;
  }
  if (!kind) return;
  result_.examples.push_back(ManualExample{marker.id, *kind, file_, line, std::move(body), marker.input, marker.name});
}

std::optional<ExampleKind> Parser::check_kind(const Fence& fence, std::size_t line, std::string_view body) {
  if (fence.language == "console") {
    check_console(line, body);
    return ExampleKind::Console;
  }
  if (fence.language == "text" && body.starts_with(kQueryPrefix)) return ExampleKind::Repl;
  error(line, "manual_check_kind");
  return std::nullopt;
}

// 最初の行は "$ " で始まり、どのコマンドも許したプログラムで始まり、引用が閉じる。誤りはその行の番号で出す。
void Parser::check_console(std::size_t line, std::string_view body) {
  const std::vector<std::string_view> lines = split_lines(body);
  if (lines.empty() || !lines.front().starts_with(kCommandPrefix)) {
    error(line + 1, "manual_console_start");
    return;
  }
  for (std::size_t offset = 0; offset < lines.size(); ++offset) {
    if (!lines[offset].starts_with(kCommandPrefix)) continue;
    const Words words = split_command(lines[offset].substr(kCommandPrefix.size()));
    if (!words.problem.empty()) {
      std::vector<std::string> args;
      if (words.culprit != '\0') args.emplace_back(1, words.culprit);
      error(line + 1 + offset, words.problem, std::move(args));
    } else if (words.words.empty() || !allowed_program(words.words.front())) {
      error(line + 1 + offset, "manual_console_program",
            {words.words.empty() ? std::string{} : words.words.front()});
    }
  }
}

}  // namespace

ParsedManual parse_manual(std::string_view text, std::string_view file) { return Parser{text, file}.run(); }

std::string indicator_text(const PredicateIndicator& item) { return item.name + "/" + std::to_string(item.arity); }

std::vector<ConsoleCommand> console_commands(std::string_view body) {
  std::vector<ConsoleCommand> commands;
  const std::vector<std::string_view> lines = split_lines(body);
  for (std::size_t offset = 0; offset < lines.size(); ++offset) {
    const std::string_view line = lines[offset];
    if (line.starts_with(kCommandPrefix)) {
      ConsoleCommand command;
      Words words = split_command(line.substr(kCommandPrefix.size()));
      if (words.problem.empty()) command.argv = std::move(words.words);
      command.offset = offset;
      commands.push_back(std::move(command));
    } else if (!commands.empty()) {
      commands.back().output += line;
      commands.back().output += '\n';
    }
  }
  return commands;
}

std::string default_input(std::string_view expected) {
  std::string input;
  for (const std::string_view line : split_lines(expected)) {
    if (!line.starts_with(kQueryPrefix)) continue;
    input += line.substr(kQueryPrefix.size());
    input += '\n';
  }
  return input;
}

// 版の突き合わせは id だけでなく印の種類も見る。片方の版だけが skip: にして検査を逃れることを防ぐ。
std::string id_sequence(std::span<const ManualExample> examples) {
  std::string out;
  for (const ManualExample& example : examples) {
    if (!out.empty()) out += ' ';
    if (example.kind == ExampleKind::Skip) out += "skip:";
    else if (example.kind == ExampleKind::File) out += "file:";
    else out += "check:";
    out += example.id;
  }
  return out;
}

}  // namespace mosa
